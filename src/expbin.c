/*	File of stuff for logistic binary variables  */

#include "glob.h"

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
    double napsprd; /* basically, the squared sprd among the ap params of children  */
    double sapsprd;
    double fapsprd;
    double bpsprd;
    double dadnap, dapsprd;
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

static void set_var(int iv, Class *cls);
static int read_attr_aux(void *vax);
static int read_smpl_aux(void *sax);
static int set_attr_aux(void *vax, int aux);
static int set_smpl_aux(void *sax, int unit, double prec);
static int read_datum(char *loc, int iv);
static int set_datum(char *loc, int iv, void *value);
static void print_datum(char *loc);
static void set_sizes(int iv);
static void set_best_pars(int iv, Class *cls);
static void clear_stats(int iv, Class *cls);
static void score_var(int iv, Class *cls);
static void cost_var(int iv, int fac, Class *cls);
static void deriv_var(int iv, int fac, Class *cls);
static void cost_var_nonleaf(int iv, int vald, Class *cls);
static void adjust(int iv, int fac, Class *cls);
static void show(Class *cls, int iv);
static void details(Class *cls, int iv, MemBuffer *buffer);

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
    VarType *vtype;
    vtype = &Types[typindx];
    vtype->id = typindx;
    /* 	Set type name as string up to 59 chars  */
    vtype->name = "Binary";
    vtype->data_size = sizeof(Datum);
    vtype->attr_aux_size = sizeof(Vaux);
    vtype->pop_aux_size = sizeof(Paux);
    vtype->smpl_aux_size = sizeof(Saux);
    vtype->read_aux_attr = &read_attr_aux;
    vtype->read_aux_smpl = &read_smpl_aux;
    vtype->set_aux_attr = &set_attr_aux;
    vtype->set_aux_smpl = &set_smpl_aux;
    vtype->read_datum = &read_datum;
    vtype->set_datum = &set_datum;
    vtype->print_datum = &print_datum;
    vtype->set_sizes = &set_sizes;
    vtype->set_best_pars = &set_best_pars;
    vtype->clear_stats = &clear_stats;
    vtype->score_var = &score_var;
    vtype->cost_var = &cost_var;
    vtype->deriv_var = &deriv_var;
    vtype->cost_var_nonleaf = &cost_var_nonleaf;
    vtype->adjust = &adjust;
    vtype->show = &show;
    vtype->set_var = &set_var;
    vtype->details = &details;
}

/*	----------------------- setvar --------------------------  */
void set_var(int iv, Class *cls) {
    /*
        Basic *cls_var = (Basic *)cls->basics[iv];
        Stats *exp_var = (Stats *)cls->stats[iv];
    */
}

/*	---------------------  readvaux ---------------------------   */
/*	To read any auxiliary info about a variable of this type in some
sample.
    */
int read_attr_aux(void *vax) { return (0); }
int set_attr_aux(void *vax, int aux) { return (0); }

/*	-------------------  readsaux ------------------------------  */
/*	To read auxilliary info re sample for this attribute   */
int read_smpl_aux(void *sax) { return (0); } /*	Multistate has no auxilliary info re sample  */
int set_smpl_aux(void *sax, int unit, double prec) { return (0); }

/*	-------------------  readdat -------------------------------  */
/*	To read a value for this variable type         */
int read_datum(char *loc, int iv) {
    int i;
    int xn;

    /*	Read datum into xn, return error.  */
    i = read_int(&xn, 1);
    if (i) {
        return (i);
    }
    set_datum(loc, iv, &xn);
    return i;
}

int set_datum(char *loc, int iv, void *value) {
    int xn = *(int *)(value);
    if (!xn) {
        return -1 * (int)sizeof(int); /* Missing */
    }
    xn--;
    if ((xn < 0) || (xn >= 2)) {
        return -1 * (int)sizeof(int);
    }
    memcpy(loc, &xn, sizeof(int));

    return (int)sizeof(int);
}

/*	---------------------  print_datum --------------------------  */
/*	To print a Datum value   */
void print_datum(char *loc) {
    /*	Print datum from address loc   */
    printf("%9d ", (*((int *)(loc)) + 1));
    return;
}

/*	---------------------  setsizes  -----------------------   */
/*	To use info in ctx.vset to set sizes of basic and stats
blocks for variable, and place in VSetVar basicsize, statssize.
    */
void set_sizes(int iv) {

    VSetVar *vset_var = &CurCtx.vset->variables[iv];

    /*	Set sizes of ClassVar (basic) and ExplnVar (stats) in VSetVar  */
    vset_var->basic_size = sizeof(Basic);
    vset_var->stats_size = sizeof(Stats);
    return;
}

/*	----------------------- set_best_pars -----------------------  */
void set_best_pars(int iv, Class *cls) {

    set_var(iv, cls);
    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];

    if (cls->type == Dad) {
        cls_var->bap = cls_var->nap;
        cls_var->bapsprd = cls_var->napsprd;
        exp_var->btcost = exp_var->ntcost;
        exp_var->bpcost = exp_var->npcost;
    } else if ((cls->use == Fac) && cls_var->infac) {
        cls_var->bap = cls_var->fap;
        cls_var->bapsprd = cls_var->fapsprd;
        exp_var->btcost = exp_var->ftcost;
        exp_var->bpcost = exp_var->fpcost;
    } else {
        cls_var->bap = cls_var->sap;
        cls_var->bapsprd = cls_var->sapsprd;
        exp_var->btcost = exp_var->stcost;
        exp_var->bpcost = exp_var->spcost;
    }
    return;
}

/*	---------------------------  clear_stats  --------------------   */
/*	Clears stats to accumulate in cost_var, and derives useful functions
of basic params   */
void clear_stats(int iv, Class *cls) {
    double round, pr0, pr1;

    set_var(iv, cls);
    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];

    exp_var->cnt = 0.0;
    exp_var->stcost = exp_var->ftcost = 0.0;
    exp_var->vsq = 0.0;
    exp_var->cnt1 = exp_var->fapd1 = exp_var->fbpd1 = 0.0;
    exp_var->apd2 = exp_var->bpd2 = 0.0;
    exp_var->tvsprd = 0.0;
    if (cls->age == 0)
        return;
    /*	Some useful functions  */
    /*	Set up non-fac case costs in scst[]  */
    /*	This requires us to calculate probs and log probs of states.  */
    if (cls_var->sap > 0.0) {
        pr1 = 1.0 / (1.0 + exp(-2.0 * cls_var->sap));
        pr0 = 1.0 - pr1;
    } else {
        pr0 = 1.0 / (1.0 + exp(2.0 * cls_var->sap));
        pr1 = 1.0 - pr0;
    }
    /*	Fisher is 4.pr0.pr1  */
    round = 2.0 * pr0 * pr1 * cls_var->sapsprd;
    exp_var->scst[0] = round - log(pr0);
    exp_var->scst[1] = round - log(pr1);
    exp_var->bsq = cls_var->fbp * cls_var->fbp;

    return;
}

/*	-------------------------  score_var  ------------------------   */
/*	To eval derivs of a case wrt score, scorespread. Adds to vvd1,vvd2.
 */
void score_var(int iv, Class *cls) {
    double cc, pr0, pr1, ff, ft, dbyv, hdffbydv, hdftbydv;
    VSetVar *vset_var = &CurCtx.vset->variables[iv];
    SampleVar *smpl_var = &CurCtx.sample->variables[iv];
    Saux *saux = (Saux *)(smpl_var->saux);
    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];

    set_var(iv, cls);
    if (vset_var->inactive)
        return;
    if (saux->missing)
        return;
    /*	Calc prob of val 1  */
    cc = cls_var->fap + Scores.CaseFacScore * cls_var->fbp;
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
    hdffbydv = -cc * cls_var->fbp * ff * ff;
    ft = 4.0 * pr0 * pr1;
    hdftbydv = ft * (pr0 - pr1) * cls_var->fbp;
    /*	Apply Bbeta mix to the approximate Fish  */
    hdffbydv = Bbeta * hdffbydv + (1.0 - Bbeta) * hdftbydv;
    ff = Bbeta * ff + (1.0 - Bbeta) * ft;
    /*	Now build deriv of cost wrt vv  */
    if (saux->xn == 1)
        dbyv = -2.0 * cls_var->fbp * pr0;
    else
        dbyv = 2.0 * cls_var->fbp * pr1;
    /*	From cost term 0.5 * vvsq * bpsprd * ft: */
    dbyv += Scores.CaseFacScore * cls_var->bpsprd * ft;
    /*	And via dftbydv, terms 0.5*(fapsprd * vvsq*bpsprd)*ft :   */
    dbyv += (cls_var->fapsprd + Scores.CaseFacScoreSq * cls_var->bpsprd) * hdftbydv;
    Scores.CaseFacScoreD1 += dbyv;
    Scores.CaseFacScoreD2 += exp_var->bsq * ff;
    Scores.EstFacScoreD2 += exp_var->bsq * ff;
    /*	Don't yet know cvvsprd, so just accum bsq * dffbydv  */
    Scores.CaseFacScoreD3 += 2.0 * exp_var->bsq * hdffbydv;
    return;
}

/*	---------------------  cost_var  ---------------------------  */
/*	Accumulate item cost into CaseNoFacCost, CaseFacCost  */
void cost_var(int iv, int fac, Class *cls) {
    double cost;
    double cc, ff, ft, hdffbydc, hdftbydc, pr0, pr1, small;

    SampleVar *smpl_var = &CurCtx.sample->variables[iv];
    Saux *saux = (Saux *)(smpl_var->saux);
    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];

    set_var(iv, cls);
    if (saux->missing)
        return;
    if (cls->age == 0) {
        exp_var->parkftcost = 0.0;
        return;
    }
    /*	Do nofac costing first  */
    cost = exp_var->scst[saux->xn];
    Scores.CaseNoFacCost += cost;

    /*	Only do faccost if fac  */
    if (!fac)
        goto facdone;
    cc = cls_var->fap + Scores.CaseFacScore * cls_var->fbp;
    if (cc > 0.0) {
        small = exp(-2.0 * cc);
        pr0 = small / (1.0 + small);
        pr1 = 1.0 - pr0;
        if (saux->xn) {
            cost = -log(pr1);
            exp_var->dbya = -2.0 * pr0;
        } else {
            cost = 2.0 * cc + log(1.0 + small);
            exp_var->dbya = 2.0 * pr1;
        }
    } else {
        small = exp(2.0 * cc);
        pr1 = small / (1.0 + small);
        pr0 = 1.0 - pr1;
        if (saux->xn) {
            cost = -2.0 * cc + log(1.0 + small);
            exp_var->dbya = -2.0 * pr0;
        } else {
            cost = -log(pr0);
            exp_var->dbya = 2.0 * pr1;
        }
    }
    ff = 1.0 / (1.0 + cc * cc);
    hdffbydc = -cc * ff * ff;
    exp_var->parkft = ft = 4.0 * pr0 * pr1;
    hdftbydc = ft * (pr0 - pr1);
    /*	Apply Bbeta mix to the approximate Fish  */
    hdffbydc = Bbeta * hdffbydc + (1.0 - Bbeta) * hdftbydc;
    ff = Bbeta * ff + (1.0 - Bbeta) * ft;
    /*	In calculating the cost, use ft for all spreads, rather than using
        ff for the v spread, but use ff in getting differentials  */
    cost += 0.5 * ((cls_var->fapsprd + Scores.CaseFacScoreSq * cls_var->bpsprd) * ft + exp_var->bsq * Scores.cvvsprd * ft);
    exp_var->dbya += (cls_var->fapsprd + Scores.CaseFacScoreSq * cls_var->bpsprd) * hdftbydc + exp_var->bsq * Scores.cvvsprd * hdffbydc;
    exp_var->dbyb = Scores.CaseFacScore * exp_var->dbya + cls_var->fbp * Scores.cvvsprd * ff;

facdone:
    Scores.CaseFacCost += cost;
    exp_var->parkftcost = cost;
    return;
}

/*	------------------  deriv_var  ------------------------------  */
/*	Given item weight in cwt, calcs derivs of item cost wrt basic
params and accumulates in paramd1, paramd2  */
void deriv_var(int iv, int fac, Class *cls) {
    const double case_weight = cls->case_weight;

    SampleVar *smpl_var = &CurCtx.sample->variables[iv];
    Saux *saux = (Saux *)(smpl_var->saux);
    Stats *exp_var = (Stats *)cls->stats[iv];

    set_var(iv, cls);
    if (saux->missing)
        return;
    /*	Do no-fac first  */
    exp_var->cnt += case_weight;
    /*	For non-fac, just accum weight of 1s in cnt1  */
    if (saux->xn == 1)
        exp_var->cnt1 += case_weight;
    /*	Accum. weighted item cost  */
    exp_var->stcost += case_weight * exp_var->scst[saux->xn];
    exp_var->ftcost += case_weight * exp_var->parkftcost;

    /*	Now for factor form  */
    if (fac) {

        exp_var->vsq += case_weight * Scores.CaseFacScoreSq;
        exp_var->fapd1 += case_weight * exp_var->dbya;
        exp_var->fbpd1 += case_weight * exp_var->dbyb;
        /*	Accum actual 2nd derivs  */
        exp_var->apd2 += case_weight * exp_var->parkft;
        exp_var->bpd2 += case_weight * exp_var->parkft * Scores.CaseFacScoreSq;
    }
}

/*	-------------------  adjust  ---------------------------    */
/*	To adjust parameters of a multistate variable     */
void adjust(int iv, int fac, Class *cls) {

    double pr0, pr1, cc;
    double adj, apd2, cnt, vara, del, tt, spcost, fpcost;
    int n;
    Population *popln = CurCtx.popln;
    Class *dad = (cls->dad_id >= 0) ? popln->classes[cls->dad_id] : 0;
    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];
    Basic *dad_var;

    set_var(iv, cls);
    cnt = exp_var->cnt;

    if (dad) { /* Not root */
        dad_var = (Basic *)dad->basics[iv];
        cls_var->dadnap = dad_var->nap;
        cls_var->dapsprd = dad_var->napsprd;
    } else { /* Root */
        dad_var = 0;
        cls_var->dadnap = 0.0;
        cls_var->dapsprd = 1.0;
    }

    /*	If too few data, use dad's n-paras   */
    if ((Control & AdjPr) && (cnt < MinSize)) {
        cls_var->nap = cls_var->sap = cls_var->fap = cls_var->dadnap;
        cls_var->fbp = 0.0;
        cls_var->napsprd = cls_var->fapsprd = cls_var->sapsprd = cls_var->dapsprd;
        cls_var->bpsprd = 1.0;

    } else if (!cls->age) {
        /*	If class age zero, make some preliminary estimates  */
        exp_var->oldftcost = 0.0;
        exp_var->adj = 1.0;
        pr1 = (exp_var->cnt1 + 0.5) / (exp_var->cnt + 1.0);
        pr0 = 1.0 - pr1;
        cls_var->fap = cls_var->sap = 0.5 * log(pr1 / pr0);
        cls_var->fbp = 0.0;
        /*	Set sapsprd  */
        apd2 = cnt + 1.0 / cls_var->dapsprd;
        cls_var->fapsprd = cls_var->sapsprd = cls_var->bpsprd = 1.0 / apd2;
        /*	Make a stab at item cost   */
        cls->cstcost -= exp_var->cnt1 * log(pr1) + (cnt - exp_var->cnt1) * log(pr0);
        cls->cftcost = cls->cstcost + 100.0 * cnt;
    }

    /*	Calculate spcost for non-fac params  */
    vara = 0.0;
    del = cls_var->sap - cls_var->dadnap;
    vara = del * del;
    vara += cls_var->sapsprd; /* Additional variance from roundoff */
    /*	Now vara holds squared difference from sap to dad's nap. This
    is a variance with squared spread cls_var->dapsprd, Normal form */
    spcost = 0.5 * vara / cls_var->dapsprd; /* The squared deviations term */
    spcost += 0.5 * log(cls_var->dapsprd);  /* log sigma */
    spcost += HALF_LOG_2PI + LATTICE;
    /*	This completes the prior density terms  */
    /*	The vol of uncertainty is sqrt (sapsprd)  */
    spcost -= 0.5 * log(cls_var->sapsprd);

    if (!fac) {
        fpcost = spcost + 100.0;
        cls_var->infac = 1;
    } else {
        /*	Get factor pcost  */
        del = cls_var->fap - cls_var->dadnap;
        vara = del * del + cls_var->fapsprd;
        fpcost = 0.5 * vara / cls_var->dapsprd; /* The squared deviations term */
        fpcost += 0.5 * log(cls_var->dapsprd);  /* log sigma */
        fpcost += (HALF_LOG_2PI + LATTICE);
        fpcost -= 0.5 * log(cls_var->fapsprd);

        /*	And for fbp[]:  (N(0,1) prior)  */
        vara = cls_var->fbp * cls_var->fbp + cls_var->bpsprd;
        fpcost += 0.5 * vara; /* The squared deviations term */
        fpcost += HALF_LOG_2PI + LATTICE - 0.5 * log(cls_var->bpsprd);
    }

    /*	Store param costs  */
    exp_var->spcost = spcost;
    exp_var->fpcost = fpcost;
    /*	Add to class param costs  */
    cls->nofac_par_cost += spcost;
    cls->fac_par_cost += fpcost;
    if (!(Control & AdjPr))
        goto adjdone;
    if (cnt < MinSize)
        goto adjdone;

    /*	Adjust non-fac params.  */
    n = 3;
adjloop:
    cc = cls_var->sap;
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
    tt = (cnt - exp_var->cnt1) * pr1 - exp_var->cnt1 * pr0;
    /*	Use dads's nap, cls_var->dapsprd for Normal prior.   */
    tt += (cls_var->sap - cls_var->dadnap) / cls_var->dapsprd;
    /*	Fisher deriv wrt cc is -2cc * apd2 * apd2  */
    tt -= cnt * cc * apd2 * apd2 * cls_var->sapsprd;
    apd2 = cnt * apd2 + 1.0 / cls_var->dapsprd;
    cls_var->sap -= tt / apd2;
    cls_var->sapsprd = 1.0 / apd2;
    /*	Repeat the adjustment  */
    if (--n)
        goto adjloop;

    if (!fac)
        goto facdone2;

    /*	Adjust factor parameters.  We have fapd1, fbpd1 from the data,
        but must add derivatives of pcost terms.  */
    exp_var->fapd1 += (cls_var->fap - cls_var->dadnap) / cls_var->dapsprd;
    exp_var->fbpd1 += cls_var->fbp;
    exp_var->apd2 += 1.0 / cls_var->dapsprd;
    exp_var->bpd2 += 1.0;
    /*	In an attempt to speed things, fiddle adjustment multiple   */
    tt = exp_var->ftcost / cnt;
    if (tt < exp_var->oldftcost)
        adj = exp_var->adj * 1.1;
    else
        adj = InitialAdj;
    if (adj > MaxAdj)
        adj = MaxAdj;
    exp_var->adj = adj;
    exp_var->oldftcost = tt;
    cls_var->fap -= adj * exp_var->fapd1 / exp_var->apd2;
    cls_var->fbp -= adj * exp_var->fbpd1 / exp_var->bpd2;

    /*	Set fapsprd, bpsprd.   */
    cls_var->fapsprd = 1.0 / exp_var->apd2;
    cls_var->bpsprd = 1.0 / exp_var->bpd2;

facdone2:
    /*	If no sons, set as-dad params from non-fac params  */
    if (cls->num_sons < 2) {
        cls_var->nap = cls_var->sap;
        cls_var->napsprd = cls_var->sapsprd;
    }
    cls_var->samplesize = exp_var->cnt;

adjdone:
    return;
}

/*	------------------------  show  -----------------------   */
void show(Class *cls, int iv) {

    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];

    set_var(iv, cls);
    printf("V%3d  Cnt%6.1f  %s  Adj%8.2f\n", iv + 1, exp_var->cnt, (cls_var->infac) ? " In" : "Out", exp_var->adj);

    if (cls->num_sons > 1) {
        printf(" N: AP ");
        printf("%6.3f", cls_var->nap);
        printf(" +-%7.3f\n", sqrt(cls_var->napsprd));
    }
    printf(" S: AP ");
    printf("%6.3f", cls_var->sap);
    printf("\n F: AP ");
    printf("%6.3f", cls_var->fap);
    printf("\n F: BP ");
    printf("%6.3f", cls_var->fbp);
    printf(" +-%7.3f\n", sqrt(cls_var->bpsprd));
}

/*	------------------------  details  -----------------------   */
void details(Class *cls, int iv, MemBuffer *buffer) {

    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];
    VSetVar *vset_var = &CurCtx.vset->variables[iv];
    set_var(iv, cls);
    print_buffer(buffer, "{\"index\": %d, \"name\": \"%s\", \"weight\": %0.1f, \"factor\": %s, ", iv + 1, vset_var->name, exp_var->cnt,
                 (cls_var->infac) ? "true" : "false");
    print_buffer(buffer, "\"type\": %d, ", vset_var->type);
    if (cls->use == Fac) {
        print_buffer(buffer, "\"frequency\": %0.3f, ", cls_var->fap);
        print_buffer(buffer, "\"influence\": %0.3f", cls_var->fbp);
    } else {
        print_buffer(buffer, "\"frequency\": %0.3f", cls_var->sap);
    }
    print_buffer(buffer, "}");
}
/*	----------------------  cost_var_nonleaf -----------------------------  */
void cost_var_nonleaf(int iv, int vald, Class *cls) {
    Basic *son_var;
    Class *son;
    double del, co0, co1, co2, tstvn, tssn;
    double apsprd, pcost, map, tap;
    int n, ison, nson, nints;

    Population *popln = CurCtx.popln;
    VSetVar *vset_var = &CurCtx.vset->variables[iv];
    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];

    set_var(iv, cls);
    if (vset_var->inactive) {
        exp_var->npcost = exp_var->ntcost = 0.0;
        return;
    }
    nson = cls->num_sons;
    if (nson < 2) { /* cannot define parameters */
        exp_var->npcost = exp_var->ntcost = 0.0;
        cls_var->nap = cls_var->sap;
        cls_var->napsprd = 1.0;
        return;
    }
    /*      We need to accumlate things over sons. We need:   */
    nints = 0;   /* Number of internal sons (M) */
    tap = 0.0;   /*  Total of sons' bap-s  */
    tstvn = 0.0; /* Total variance of sons' bap-s */
    tssn = 0.0;  /* Total internal sons' bapsprd  */

    apsprd = cls_var->napsprd;
    /*	The calculation is like that in reals.c (q.v.)   */
    for (ison = cls->son_id; ison > 0; ison = son->sib_id) {
        son = popln->classes[ison];
        son_var = (Basic *)son->basics[iv];
        tap += son_var->bap;
        tstvn += son_var->bap * son_var->bap;
        if (son->type == Dad) { /* used as parent */
            nints++;
            tssn += son_var->bapsprd;
            tstvn += son_var->bapsprd / son->num_sons;
        } else
            tstvn += son_var->bapsprd;
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
    cls_var->nap = (cls_var->dapsprd * map) / (nson * cls_var->dapsprd + apsprd);
    del = cls_var->nap - map;
    tstvn += del * del * nson; /* adding variance round new nap */
    co0 = 0.5 * (tstvn + nson * del * del) + tssn;
    /*      Solve for new spread  */
    apsprd = 2.0 * co0 / (co1 + sqrt(co1 * co1 + 4.0 * co0 * co2));
    n--;
    if (n)
        goto adjloop;
    /*	Store new values  */
    cls_var->napsprd = apsprd;

    /*      Calc cost  */
    del = cls_var->nap - cls_var->dadnap;
    pcost = del * del;
    pcost = 0.5 * (pcost + apsprd / nson) / cls_var->dapsprd + (HALF_LOG_2PI + 0.5 * log(cls_var->dapsprd));
    /*      Add hlog Fisher, lattice  */
    pcost += 0.5 * log(0.5 * nson + nints) + 0.5 * log((double)nson) - 1.5 * log(apsprd) + 2.0 * LATTICE;
    /*	Add roundoff for params  */
    pcost += 1.0;
    exp_var->npcost = pcost;
    return;
}
