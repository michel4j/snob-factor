/*	File of stuff for logistic binary variables  */

#include "glob.h"

static void set_var();
static int read_attr_aux();
static int read_smpl_aux();
static int read_datum();
static void printdat();
static void set_sizes();
static void set_best_pars();
static void clear_stats();
static void score_var();
static void cost_var();
static void deriv_var();
static void nonleaf_cost_var();
static void adjust();
static void show();

typedef struct Vauxst {
    int states;
    double rstatesm;  /* 1 / (states-1) */
    double lstatessq; /*  log (states^2) */
    double mff;       /* a max value for ff */
} Vaux;
/*	The Vaux structure matches that of Multistate, although not all
    used for Binary  */

typedef int Datum;

typedef struct Sauxst {
    int missing;
    double dummy;
    Datum xn;
} Saux;

typedef struct Pauxst {
    int dummy;
} Paux;

/*	Common variable for holding a data value  */

typedef struct Basicst { /* Basic parameter info about var in class.
            The first few fields are standard and must
            appear in the order shown.  */
    int id;              /* The variable number (index)  */
    int signif;
    int infac; /* shows if affected by factor  */
    /********************  Following fields vary according to need ***/
    double bap;     /* current ap val */
    double bapsprd; /*  Spread measure for ap parameters.
        This gives the expected squared round-off error in bap */
    double nap, sap, fap, fbp;
    double napsprd; /* basically, the squared sprd among the ap params
                of children  */
    double sapsprd;
    double fapsprd;
    double bpsprd;
    double samplesize; /* Size of sample on which estimates based */
} Basic;

typedef struct Statsst { /* Stuff accumulated to revise Basic  */
                         /* First fields are standard  */
    double cnt;          /* Weighted count  */
    double btcost, ntcost, stcost, ftcost;
    double bpcost, npcost, spcost, fpcost;
    double vsq; /*  weighted sum of squared scores for this var */
    int id;     /* Variable number  */
    /********************  Following fields vary according to need ***/
    double parkftcost; /* Unweighted item costs of xn */
    double cnt1, fapd1, fbpd1;
    double tvsprd;
    double oldftcost;
    double adj;
    double apd2, bpd2;
    double scst[2];
    double dbya, dbyb; /* derivs of cost wrt fap, fbp */
    double parkft;
    double bsq;
} Stats;

/*	Static variables useful for many types of variable    */
static Saux *saux;
static Paux *paux;
static Vaux *vaux;
static Basic *cvi, *dcvi;
static Stats *evi;

/*	Static variables specific to this type   */
static double dadnap;
static double dapsprd; /* Dad's napsprd */

/*--------------------------  define ------------------------------- */
/*	This routine is used to set up a VarType entry in the global "types"
array.  It is the only function whose name needs to be altered for different
types of variable, and this name must be copied into the file "dotypes.c"
when installing a new type of variable. It is also necessary to change the
"Ntypes" constant, and to decide on a type id (an integer) for the new type.
    */

void expbinary_define(typindx) int typindx;
/*	typindx is the index in types[] of this type   */
{
    CurVType = Types + typindx;
    CurVType->id = typindx;
    /* 	Set type name as string up to 59 chars  */
    CurVType->name = "ExpBinary";
    CurVType->data_size = sizeof(Datum);
    CurVType->attr_aux_size = sizeof(Vaux);
    CurVType->pop_aux_size = sizeof(Paux);
    CurVType->smpl_aux_size = sizeof(Saux);
    CurVType->read_aux_attr = &read_attr_aux;
    CurVType->read_aux_smpl = &read_smpl_aux;
    CurVType->read_datum = &read_datum;
    CurVType->print_datum = &printdat;
    CurVType->set_sizes = &set_sizes;
    CurVType->set_best_pars = &set_best_pars;
    CurVType->clear_stats = &clear_stats;
    CurVType->score_var = &score_var;
    CurVType->cost_var = &cost_var;
    CurVType->deriv_var = &deriv_var;
    CurVType->cost_var_nonleaf = &nonleaf_cost_var;
    CurVType->adjust = &adjust;
    CurVType->show = &show;
    CurVType->set_var = &set_var;

    return;
}

/*	----------------------- setvar --------------------------  */
void set_var(iv) int iv;
{
    CurAttr = CurAttrList + iv;
    CurVType = CurAttr->vtype;
    CurPopVar = CurPopVarList + iv;
    paux = (Paux *)CurPopVar->paux;
    CurVar = CurVarList + iv;
    vaux = (Vaux *)CurAttr->vaux;
    saux = (Saux *)CurVar->saux;
    cvi = (Basic *)CurClass->basics[iv];
    evi = (Stats *)CurClass->stats[iv];
    return;
}

/*	---------------------  readvaux ---------------------------   */
/*	To read any auxiliary info about a variable of this type in some
sample.
    */
int read_attr_aux(vax)
Vaux *vax;
{ return (0); }

/*	-------------------  readsaux ------------------------------  */
/*	To read auxilliary info re sample for this attribute   */
int read_smpl_aux(sax)
Saux *sax;
{
    /*	Multistate has no auxilliary info re sample  */
    return (0);
}

/*	-------------------  readdat -------------------------------  */
/*	To read a value for this variable type         */
int read_datum(char *loc, int iv)
{
    int i;
    Datum xn;

    /*	Read datum into xn, return error.  */
    i = read_int(&xn, 1);
    if (i)
        return (i);
    if (!xn)
        return (-1); /* Missing */
    xn--;
    if ((xn < 0) || (xn >= 2))
        return (2);
    memcpy(loc, &xn, sizeof(Datum));
    return (0);
}

/*	---------------------  printdat --------------------------  */
/*	To print a Datum value   */
void printdat(Datum *loc)
{
    /*	Print datum from address loc   */
    printf("%3d", (*((Datum *)loc) + 1));
    return;
}

/*	---------------------  setsizes  -----------------------   */
/*	To use info in ctx.vset to set sizes of basic and stats
blocks for variable, and place in VSetVar basicsize, statssize.
    */
void set_sizes( int iv)
{

    CurAttr = CurAttrList + iv;

    /*	Set sizes of CVinst (basic) and EVinst (stats) in VSetVar  */
    CurAttr->basic_size = sizeof(Basic);
    CurAttr->stats_size = sizeof(Stats);
    return;
}

/*	----------------------- set_best_pars -----------------------  */
void set_best_pars(int iv)
{

    set_var(iv);

    if (CurClass->type == Dad) {
        cvi->bap = cvi->nap;
        cvi->bapsprd = cvi->napsprd;
        evi->btcost = evi->ntcost;
        evi->bpcost = evi->npcost;
    } else if ((CurClass->use == Fac) && cvi->infac) {
        cvi->bap = cvi->fap;
        cvi->bapsprd = cvi->fapsprd;
        evi->btcost = evi->ftcost;
        evi->bpcost = evi->fpcost;
    } else {
        cvi->bap = cvi->sap;
        cvi->bapsprd = cvi->sapsprd;
        evi->btcost = evi->stcost;
        evi->bpcost = evi->spcost;
    }
    return;
}

/*	---------------------------  clear_stats  --------------------   */
/*	Clears stats to accumulate in cost_var, and derives useful functions
of basic params   */
void clear_stats(int iv)
{
    double round, pr0, pr1;

    set_var(iv);
    evi->cnt = 0.0;
    evi->stcost = evi->ftcost = 0.0;
    evi->vsq = 0.0;
    evi->cnt1 = evi->fapd1 = evi->fbpd1 = 0.0;
    evi->apd2 = evi->bpd2 = 0.0;
    evi->tvsprd = 0.0;
    if (CurClass->age == 0)
        return;
    /*	Some useful functions  */
    /*	Set up non-fac case costs in scst[]  */
    /*	This requires us to calculate probs and log probs of states.  */
    if (cvi->sap > 0.0) {
        pr1 = 1.0 / (1.0 + exp(-2.0 * cvi->sap));
        pr0 = 1.0 - pr1;
    } else {
        pr0 = 1.0 / (1.0 + exp(2.0 * cvi->sap));
        pr1 = 1.0 - pr0;
    }
    /*	Fisher is 4.pr0.pr1  */
    round = 2.0 * pr0 * pr1 * cvi->sapsprd;
    evi->scst[0] = round - log(pr0);
    evi->scst[1] = round - log(pr1);
    evi->bsq = cvi->fbp * cvi->fbp;

    return;
}

/*	-------------------------  score_var  ------------------------   */
/*	To eval derivs of a case wrt score, scorespread. Adds to vvd1,vvd2.
 */
void score_var(int iv)
{
    double cc, pr0, pr1, ff, ft, dbyv, hdffbydv, hdftbydv;
    set_var(iv);
    if (CurAttr->inactive)
        return;
    if (saux->missing)
        return;
    /*	Calc prob of val 1  */
    cc = cvi->fap + CurCaseFacScore * cvi->fbp;
    if (cc > 0.0) {
        pr1 = exp(-2.0 * cc);
        pr0 = pr1 / (1.0 + pr1);
        pr1 = 1.0 - pr0;
    } else {
        pr0 = exp(2.0 * cc);
        pr1 = pr0 / (1.0 + pr0);
        pr0 = 1.0 - pr1;
    }
    /*	Approximate Fisher by 1/(1+cc^2)  (wrt cc)  */
    /*	This approx is for getting vvd2.  Use true Fisher 4.p0.p1 for
        variation in ap, bp.  */
    /*	Now, dffbydc = -2 * cc * ff * ff, and
             dftbydc = 8 * p0 * p1 * (p0-p1) = 2 * ft * (p0-p1)   */
    ff = 1.0 / (1.0 + cc * cc);
    hdffbydv = -cc * cvi->fbp * ff * ff;
    ft = 4.0 * pr0 * pr1;
    hdftbydv = ft * (pr0 - pr1) * cvi->fbp;
    /*	Apply Bbeta mix to the approximate Fish  */
    hdffbydv = Bbeta * hdffbydv + (1.0 - Bbeta) * hdftbydv;
    ff = Bbeta * ff + (1.0 - Bbeta) * ft;
    /*	Now build deriv of cost wrt vv  */
    if (saux->xn == 1)
        dbyv = -2.0 * cvi->fbp * pr0;
    else
        dbyv = 2.0 * cvi->fbp * pr1;
    /*	From cost term 0.5 * vvsq * bpsprd * ft: */
    dbyv += CurCaseFacScore * cvi->bpsprd * ft;
    /*	And via dftbydv, terms 0.5*(fapsprd * vvsq*bpsprd)*ft :   */
    dbyv += (cvi->fapsprd + CurCaseFacScoreSq * cvi->bpsprd) * hdftbydv;
    CaseFacScoreD1 += dbyv;
    CaseFacScoreD2 += evi->bsq * ff;
    EstFacScoreD2 += evi->bsq * ff;
    /*	Don't yet know cvvsprd, so just accum bsq * dffbydv  */
    CaseFacScoreD3 += 2.0 * evi->bsq * hdffbydv;
    return;
}

/*	---------------------  cost_var  ---------------------------  */
/*	Accumulate item cost into CaseNoFacCost, CaseFacCost  */
void cost_var(int iv, int fac)
{
    double cost;
    double cc, ff, ft, hdffbydc, hdftbydc, pr0, pr1, small;

    set_var(iv);
    if (saux->missing)
        return;
    if (CurClass->age == 0) {
        evi->parkftcost = 0.0;
        return;
    }
    /*	Do nofac costing first  */
    cost = evi->scst[saux->xn];
    CaseNoFacCost += cost;

    /*	Only do faccost if fac  */
    if (!fac)
        goto facdone;
    cc = cvi->fap + CurCaseFacScore * cvi->fbp;
    if (cc > 0.0) {
        small = exp(-2.0 * cc);
        pr0 = small / (1.0 + small);
        pr1 = 1.0 - pr0;
        if (saux->xn) {
            cost = -log(pr1);
            evi->dbya = -2.0 * pr0;
        } else {
            cost = 2.0 * cc + log(1.0 + small);
            evi->dbya = 2.0 * pr1;
        }
    } else {
        small = exp(2.0 * cc);
        pr1 = small / (1.0 + small);
        pr0 = 1.0 - pr1;
        if (saux->xn) {
            cost = -2.0 * cc + log(1.0 + small);
            evi->dbya = -2.0 * pr0;
        } else {
            cost = -log(pr0);
            evi->dbya = 2.0 * pr1;
        }
    }
    ff = 1.0 / (1.0 + cc * cc);
    hdffbydc = -cc * ff * ff;
    evi->parkft = ft = 4.0 * pr0 * pr1;
    hdftbydc = ft * (pr0 - pr1);
    /*	Apply Bbeta mix to the approximate Fish  */
    hdffbydc = Bbeta * hdffbydc + (1.0 - Bbeta) * hdftbydc;
    ff = Bbeta * ff + (1.0 - Bbeta) * ft;
    /*	In calculating the cost, use ft for all spreads, rather than using
        ff for the v spread, but use ff in getting differentials  */
    cost += 0.5 * ((cvi->fapsprd + CurCaseFacScoreSq * cvi->bpsprd) * ft +
                   evi->bsq * cvvsprd * ft);
    evi->dbya += (cvi->fapsprd + CurCaseFacScoreSq * cvi->bpsprd) * hdftbydc +
                 evi->bsq * cvvsprd * hdffbydc;
    evi->dbyb = CurCaseFacScore * evi->dbya + cvi->fbp * cvvsprd * ff;

facdone:
    CaseFacCost += cost;
    evi->parkftcost = cost;
    return;
}

/*	------------------  deriv_var  ------------------------------  */
/*	Given item weight in cwt, calcs derivs of item cost wrt basic
params and accumulates in paramd1, paramd2  */
void deriv_var(int iv, int fac)
{

    set_var(iv);
    if (saux->missing)
        return;
    /*	Do no-fac first  */
    evi->cnt += CurCaseWeight;
    /*	For non-fac, just accum weight of 1s in cnt1  */
    if (saux->xn == 1)
        evi->cnt1 += CurCaseWeight;
    /*	Accum. weighted item cost  */
    evi->stcost += CurCaseWeight * evi->scst[saux->xn];
    evi->ftcost += CurCaseWeight * evi->parkftcost;

    /*	Now for factor form  */
    if (!fac)
        goto facdone;
    evi->vsq += CurCaseWeight * CurCaseFacScoreSq;
    evi->fapd1 += CurCaseWeight * evi->dbya;
    evi->fbpd1 += CurCaseWeight * evi->dbyb;
    /*	Accum actual 2nd derivs  */
    evi->apd2 += CurCaseWeight * evi->parkft;
    evi->bpd2 += CurCaseWeight * evi->parkft * CurCaseFacScoreSq;
facdone:
    return;
}

/*	-------------------  adjust  ---------------------------    */
/*	To adjust parameters of a multistate variable     */
void adjust(int iv, int fac)
{

    double pr0, pr1, cc;
    double adj, apd2, cnt, vara, del, tt, spcost, fpcost;
    int n;

    set_var(iv);
    cnt = evi->cnt;

    if (CurDad) { /* Not root */
        dcvi = (Basic *)CurDad->basics[iv];
        dadnap = dcvi->nap;
        dapsprd = dcvi->napsprd;
    } else { /* Root */
        dcvi = 0;
        dadnap = 0.0;
        dapsprd = 1.0;
    }

    /*	If too few data, use dad's n-paras   */
    if ((Control & AdjPr) && (cnt < MinSize)) {
        cvi->nap = cvi->sap = cvi->fap = dadnap;
        cvi->fbp = 0.0;
        cvi->napsprd = cvi->fapsprd = cvi->sapsprd = dapsprd;
        cvi->bpsprd = 1.0;
        goto hasage;
    }

    /*	If class age zero, make some preliminary estimates  */
    if (CurClass->age)
        goto hasage;
    evi->oldftcost = 0.0;
    evi->adj = 1.0;
    pr1 = (evi->cnt1 + 0.5) / (evi->cnt + 1.0);
    pr0 = 1.0 - pr1;
    cvi->fap = cvi->sap = 0.5 * log(pr1 / pr0);
    cvi->fbp = 0.0;
    /*	Set sapsprd  */
    apd2 = cnt + 1.0 / dapsprd;
    cvi->fapsprd = cvi->sapsprd = cvi->bpsprd = 1.0 / apd2;
    /*	Make a stab at item cost   */
    CurClass->cstcost -= evi->cnt1 * log(pr1) + (cnt - evi->cnt1) * log(pr0);
    CurClass->cftcost = CurClass->cstcost + 100.0 * cnt;

hasage:
    /*	Calculate spcost for non-fac params  */
    vara = 0.0;
    del = cvi->sap - dadnap;
    vara = del * del;
    vara += cvi->sapsprd; /* Additional variance from roundoff */
    /*	Now vara holds squared difference from sap to dad's nap. This
    is a variance with squared spread dapsprd, Normal form */
    spcost = 0.5 * vara / dapsprd; /* The squared deviations term */
    spcost += 0.5 * log(dapsprd);  /* log sigma */
    spcost += HALF_LOG_2PI + LATTICE;
    /*	This completes the prior density terms  */
    /*	The vol of uncertainty is sqrt (sapsprd)  */
    spcost -= 0.5 * log(cvi->sapsprd);

    if (!fac) {
        fpcost = spcost + 100.0;
        cvi->infac = 1;
        goto facdone1;
    }
    /*	Get factor pcost  */
    del = cvi->fap - dadnap;
    vara = del * del + cvi->fapsprd;
    fpcost = 0.5 * vara / dapsprd; /* The squared deviations term */
    fpcost += 0.5 * log(dapsprd);  /* log sigma */
    fpcost += (HALF_LOG_2PI + LATTICE);
    fpcost -= 0.5 * log(cvi->fapsprd);

    /*	And for fbp[]:  (N(0,1) prior)  */
    vara = cvi->fbp * cvi->fbp + cvi->bpsprd;
    fpcost += 0.5 * vara; /* The squared deviations term */
    fpcost += HALF_LOG_2PI + LATTICE - 0.5 * log(cvi->bpsprd);

facdone1:
    /*	Store param costs  */
    evi->spcost = spcost;
    evi->fpcost = fpcost;
    /*	Add to class param costs  */
    CurClass->nofac_par_cost += spcost;
    CurClass->fac_par_cost += fpcost;
    if (!(Control & AdjPr))
        goto adjdone;
    if (cnt < MinSize)
        goto adjdone;

    /*	Adjust non-fac params.  */
    n = 3;
adjloop:
    cc = cvi->sap;
    if (cc > 0.0) {
        pr1 = 1.0 / (1.0 + exp(-2.0 * cc));
        pr0 = 1.0 - pr1;
    } else {
        pr0 = 1.0 / (1.0 + exp(2.0 * cc));
        pr1 = 1.0 - pr0;
    }
    /*	Approximate Fisher by 1/(1+cc^2)  (wrt cc)  */
    apd2 = 1.0 / (1.0 + cc * cc);
    /*	For a item in state 1, apd1 = -pr0.  If state 0, apd1 = pr1. */
    tt = (cnt - evi->cnt1) * pr1 - evi->cnt1 * pr0;
    /*	Use dads's nap, dapsprd for Normal prior.   */
    tt += (cvi->sap - dadnap) / dapsprd;
    /*	Fisher deriv wrt cc is -2cc * apd2 * apd2  */
    tt -= cnt * cc * apd2 * apd2 * cvi->sapsprd;
    apd2 = cnt * apd2 + 1.0 / dapsprd;
    cvi->sap -= tt / apd2;
    cvi->sapsprd = 1.0 / apd2;
    /*	Repeat the adjustment  */
    if (--n)
        goto adjloop;

    if (!fac)
        goto facdone2;

    /*	Adjust factor parameters.  We have fapd1, fbpd1 from the data,
        but must add derivatives of pcost terms.  */
    evi->fapd1 += (cvi->fap - dadnap) / dapsprd;
    evi->fbpd1 += cvi->fbp;
    evi->apd2 += 1.0 / dapsprd;
    evi->bpd2 += 1.0;
    /*	In an attempt to speed things, fiddle adjustment multiple   */
    tt = evi->ftcost / cnt;
    if (tt < evi->oldftcost)
        adj = evi->adj * 1.1;
    else
        adj = InitialAdj;
    if (adj > MaxAdj)
        adj = MaxAdj;
    evi->adj = adj;
    evi->oldftcost = tt;
    cvi->fap -= adj * evi->fapd1 / evi->apd2;
    cvi->fbp -= adj * evi->fbpd1 / evi->bpd2;

    /*	Set fapsprd, bpsprd.   */
    cvi->fapsprd = 1.0 / evi->apd2;
    cvi->bpsprd = 1.0 / evi->bpd2;

facdone2:
    /*	If no sons, set as-dad params from non-fac params  */
    if (CurClass->num_sons < 2) {
        cvi->nap = cvi->sap;
        cvi->napsprd = cvi->sapsprd;
    }
    cvi->samplesize = evi->cnt;

adjdone:
    return;
}

/*	------------------------  show  -----------------------   */
void show(Class* ccl, int iv){

    set_class(ccl);
    set_var(iv);
    printf("V%3d  Cnt%6.1f  %s  Adj%8.2f\n", iv + 1, evi->cnt,
           (cvi->infac) ? " In" : "Out", evi->adj);

    if (CurClass->num_sons < 2)
        goto skipn;
    printf(" N: AP ");
    printf("%6.3f", cvi->nap);
    printf(" +-%7.3f\n", sqrt(cvi->napsprd));
skipn:
    printf(" S: AP ");
    printf("%6.3f", cvi->sap);
    printf("\n F: AP ");
    printf("%6.3f", cvi->fap);
    printf("\n F: BP ");
    printf("%6.3f", cvi->fbp);
    printf(" +-%7.3f\n", sqrt(cvi->bpsprd));
    return;
}

/*	----------------------  nonleaf_cost_var -----------------------------  */
void nonleaf_cost_var(int iv) {
    Basic *soncvi;
    Class *son;
    double del, co0, co1, co2, tstvn, tssn;
    double apsprd, pcost, map, tap;
    int n, ison, nson, nints;

    set_var(iv);
    if (CurAttr->inactive) {
        evi->npcost = evi->ntcost = 0.0;
        return;
    }
    nson = CurClass->num_sons;
    if (nson < 2) { /* cannot define parameters */
        evi->npcost = evi->ntcost = 0.0;
        cvi->nap = cvi->sap;
        cvi->napsprd = 1.0;
        return;
    }
    /*      We need to accumlate things over sons. We need:   */
    nints = 0;   /* Number of internal sons (M) */
    tap = 0.0;   /*  Total of sons' bap-s  */
    tstvn = 0.0; /* Total variance of sons' bap-s */
    tssn = 0.0;  /* Total internal sons' bapsprd  */

    apsprd = cvi->napsprd;
    /*	The calculation is like that in reals.c (q.v.)   */
    for (ison = CurClass->son_id; ison > 0; ison = son->sib_id) {
        son = CurPopln->classes[ison];
        soncvi = (Basic *)son->basics[iv];
        tap += soncvi->bap;
        tstvn += soncvi->bap * soncvi->bap;
        if (son->type == Dad) { /* used as parent */
            nints++;
            tssn += soncvi->bapsprd;
            tstvn += soncvi->bapsprd / son->num_sons;
        } else
            tstvn += soncvi->bapsprd;
    }
    /*      Calc coeffs for quadratic c_2 * s^2 + c_1 * s - c_0  */
    co2 = (1.0 + 0.5 / nson);
    co1 = nints + 0.5 * (nson - 3.0);
    /*      Can now compute V in the 'reals' comments.  tssn = S of the comments
     */
    /*      First we get the V around the sons' mean, then update nap and
       correct the value of V. */
    map = tap / nson;
    tstvn -= map * tap;
    /*	tstvn now gives total variance about sons' mean */

    /*      Iterate the adjustment of param, spread  */
    n = 5;
adjloop:
    /*      Update param  */
    /*      The V of comments is tstvn + nson * del * del */
    cvi->nap = (dapsprd * map) / (nson * dapsprd + apsprd);
    del = cvi->nap - map;
    tstvn += del * del * nson; /* adding variance round new nap */
    co0 = 0.5 * (tstvn + nson * del * del) + tssn;
    /*      Solve for new spread  */
    apsprd = 2.0 * co0 / (co1 + sqrt(co1 * co1 + 4.0 * co0 * co2));
    n--;
    if (n)
        goto adjloop;
    /*	Store new values  */
    cvi->napsprd = apsprd;

    /*      Calc cost  */
    del = cvi->nap - dadnap;
    pcost = del * del;
    pcost =
        0.5 * (pcost + apsprd / nson) / dapsprd + (HALF_LOG_2PI + 0.5 * log(dapsprd));
    /*      Add hlog Fisher, lattice  */
    pcost += 0.5 * log(0.5 * nson + nints) + 0.5 * log((double)nson) -
             1.5 * log(apsprd) + 2.0 * LATTICE;
    /*	Add roundoff for params  */
    pcost += 1.0;
    evi->npcost = pcost;
    return;
}
/*zzzz*/
#ifdef JJ
#endif
