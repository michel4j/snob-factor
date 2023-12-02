/*    File of stuff for Multistate variables.   */

#include "glob.h"

#define MaxState 20 /* Max number of discrete states per variable */
static void set_var();
static int read_aux_attr();
static int read_aux_smpl();
static int read_datum();
static void print_datum();
static void set_sizes();
static void set_best_pars();
static void clear_stats();
static void score_var();
static void cost_var();
static void deriv_var();
static void cost_var_nonleaf();
static void adjust();
static void show();
static void setprobs();

typedef struct Vauxst {
    int states;
    double lstatessq; /*  log (states^2) */
    double mff;       /* a max value for ff */
} Vaux;

typedef int Datum;

typedef struct Sauxst {
    int missing;
    double dummy;
    Datum xn;
} Saux;

typedef struct Pauxst {
    int dummy;
} Paux;

/*    Common variable for holding a data value  */

typedef struct Basicst { /* Basic parameter info about var in class.
            The first few fields are standard and must
            appear in the order shown.  */
    int id;              /* The variable number (index)  */
    int signif;
    int infac; /* shows if affected by factor  */
    /********************  Following fields vary according to need ***/
    double *bap;    /* ptr to current ap[] vector */
    double bapsprd; /*  Spread measure for ap[] parameters.
        This gives the expected squared round-off error in
        1 dimension of the ap[] vector. If F is the Fisher for
        ap[], then apsprd = 1 / F^(1/(states-1))   */
    double napsprd; /* basically, the squared sprd among the ap[] params
                of children  */
    double sapsprd;
    double fapsprd;
    double bpsprd;
    double samplesize; /* Size of sample on which estimates based */
    double origin;     /* This becomes the first element in a number of
               vectors, each of length 'states', which will be
               appended to the Basic sructure. Pointers to these
               vectors will be set in the static pointers declared
               below. A similar item happens to the Stats structure.
               */
} Basic;

/*    Pointers to vectors in Basic  */
static double *nap;       /* Log odds as dad */
static double *sap;       /*  Log odds without factor  */
static double *fap, *fbp; /* Const and load params for logodds */
static double *frate;
/*    The factor model is that prob of state k for a case with score v
    is proportional to
        exp (fap[k] + fbp[k] * v)
    frate[k] is defined as
        frate[k] = exp (fap[k] + 0.5 * fbp[k]^2)
    With this definition, it is easily shown that the probability of
    state k is proportional to
        frate[k] * Gauss (v - fbp[k])
    where Gauss(x) is the Normal(0,1) density at x.
        We use an (unnormalized) tabulation of Gauss() in gaustab[]
    to evaluate the relative state probs for a case, thereby eliminating
    many calls on exp().   */

typedef struct Statsst { /* Stuff accumulated to revise Basic  */
                         /* First fields are standard  */
    double cnt;          /* Weighted count  */
    double btcost, ntcost, stcost, ftcost;
    double bpcost, npcost, spcost, fpcost;
    double vsq; /*  weighted sum of squared scores for this var */
    int id;     /* Variable number  */
    /********************  Following fields vary according to need ***/
    double parkftcost;                  /* Unweighted item costs of xn */
    double ff, parkb1p, parkb2p, conff; /* Useful quants calced in cost_var,
               used in deriv_var */
    double oldftcost;
    double adj;
    double apd2, bpd2;
    double mgg;    /* A maximum? for gg */
    double origin; /* First element of states vectors */
} Stats;

/*    Pointers to vectors in stats  */
static double *scnt;          /*  vector of counts in states  */
static double *scst;          /*  vector of non-fac state costs  */
static double *pr;            /* vec. of factor state probs  */
static double *fapd1, *fbpd1; /*  vectors of derivs of cost wrt params */

/*    Static variables useful for many types of variable    */
static Saux *saux;
static Paux *paux;
static Vaux *vaux;
static Basic *cvi, *dcvi;
static Stats *stats;

/*    The macro below abbreviates a scan over states  */
#define Fork for (k = 0; k < states; k++)
#define Forj for (j = 0; j < states; j++)

/*    Static variables specific to this type   */
static int states;
static double statesm; /*  states - 1.0 */
static double rstates; /*  1.0 / states  */
static double rstatesm;
static double qr[MaxState]; /*  - logs of state probs  */
static double b1p, b2p, b3p, b1p2, gg, ff;
static double *dadnap; /* To dad's nap[] */
static double dapsprd; /* Dad's napsprd */

/*    Stuff for a table of the function exp (-0.5 * x * x)
    for x in the range 0 to Grange in Gns steps per unit of x  */
#define Grange ((int)6)
#define Gns ((int)512)
#define Gh ((double)1.0 / Gns)
#define Gnt ((int)Grange * Gns + 3) /* Number of table entries */
static double gaustab[Gnt];
static double *gausorg; /* ptr to gaustab[1] */

/*--------------------------  define ------------------------------- */
/*    This routine is used to set up a VarType entry in the global "types"
array.  It is the only function whose name needs to be altered for different
types of variable, and this name must be copied into the file "definetypes.c"
when installing a new type of variable. It is also necessary to change the
"Ntypes" constant, and to decide on a type id (an integer) for the new type.
    */

void expmults_define(typindx) int typindx;
/*    typindx is the index in types[] of this type   */
{
    int ig;
    double xg;

    CurVType = Types + typindx;
    CurVType->id = typindx;
    /*     Set type name as string up to 59 chars  */
    CurVType->name = "ExpMultiState";
    CurVType->data_size = sizeof(Datum);
    CurVType->attr_aux_size = sizeof(Vaux);
    CurVType->pop_aux_size = sizeof(Paux);
    CurVType->smpl_aux_size = sizeof(Saux);
    CurVType->read_aux_attr = &read_aux_attr;
    CurVType->read_aux_smpl = &read_aux_smpl;
    CurVType->read_datum = &read_datum;
    CurVType->print_datum = &print_datum;
    CurVType->set_sizes = &set_sizes;
    CurVType->set_best_pars = &set_best_pars;
    CurVType->clear_stats = &clear_stats;
    CurVType->score_var = &score_var;
    CurVType->cost_var = &cost_var;
    CurVType->deriv_var = &deriv_var;
    CurVType->cost_var_nonleaf = &cost_var_nonleaf;
    CurVType->adjust = &adjust;
    CurVType->show = &show;
    CurVType->set_var = &set_var;

    /*    Make table of exp (-0.5 * x * x) in gaustab[]
        Entry for x = 0 is at gaustab[1]  */
    gausorg = gaustab + 1;
    for (ig = -1; ig <= (Grange * Gns + 1); ig++) {
        xg = ig * Gh;
        gausorg[ig] = exp(-0.5 * xg * xg);
    }
    return;
}

/*    ----------------------- set_var --------------------------  */
void set_var(iv) int iv;
{
    CurAttr = CurAttrList + iv;
    CurVType = CurAttr->vtype;
    pvi = CurCtx.popln->variables + iv;
    paux = (Paux *)pvi->paux;
    CurVar = CurCtx.sample->variables + iv;
    vaux = (Vaux *)CurAttr->vaux;
    saux = (Saux *)CurVar->saux;
    cvi = (Basic *)CurClass->basics[iv];
    stats = (Stats *)CurClass->stats[iv];
    states = vaux->states;
    statesm = states - 1;
    rstates = 1.0 / states;
    rstatesm = 1.0 / statesm;
    nap = &(cvi->origin);
    sap = nap + states;
    fap = sap + states;
    fbp = fap + states;
    frate = fbp + states;
    scnt = &(stats->origin);
    scst = scnt + states;
    pr = scst + states;
    fapd1 = pr + states;
    fbpd1 = fapd1 + states;
    return;
}

/*    ---------------------  read_aux_attr ---------------------------   */
/*    To read any auxiliary info about a variable of this type in some
sample.
    */
int read_aux_attr(vax)
Vaux *vax;
{
    int i;
    double lstates;

    /*    Read in auxiliary info into vaux, return 0 if OK else 1  */
    i = read_int(&(vax->states), 1);
    if (i < 0) {
        vax->states = 0;
        return (1);
    }
    if (vax->states > MaxState) {
        printf("Variable has more than %d states (%d)\n", MaxState, vax->states);
        return (2);
    }
    lstates = log((double)(vax->states));
    vax->lstatessq = 2.0 * lstates;
    /*    Calc a maximum value for ff, the (K-1)th root of the Fisher
        given by K^2 * Prod_k {pr_k}.  The maximum occurs when
        pr_k = 1/K for all k, so  */
    rstatesm = 1.0 / (vax->states - 1);
    vax->mff = exp(-rstatesm * (vax->states - 2) * lstates);
    /*    For safety, inflate by states/statesm  */
    vax->mff *= vax->states * rstatesm;
#ifdef UseBin
    /*    If states = 2, change type to binary by changing vtp, avi->vtp,
        and avi->itype  */
    if (vax->states == 2) {
        avi->itype++;
        vtp = avi->vtp = types + avi->itype;
    }
#endif
    return (0);
}

/*    -------------------  read_aux_smpl ------------------------------  */
/*    To read auxilliary info re sample for this attribute   */
int read_aux_smpl(sax)
Saux *sax;
{
    /*    Multistate has no auxilliary info re sample  */
    return (0);
}

/*    -------------------  read_datum -------------------------------  */
/*    To read a value for this variable type         */
int read_datum(char *loc, int iv) {
    int i;
    Datum xn;

    vaux = (Vaux *)(CurAttrList[iv].vaux);
    states = vaux->states;
    /*    Read datum into xn, return error.  */
    i = read_int(&xn, 1);
    if (i)
        return (i);
    if (!xn)
        return (-1); /* Missing */
    xn--;
    if ((xn < 0) || (xn >= states))
        return (2);
    memcpy(loc, &xn, sizeof(Datum));
    return (0);
}

/*    ---------------------  print_datum --------------------------  */
/*    To print a Datum value   */
void print_datum(loc) Datum *loc;
{
    /*    Print datum from address loc   */
    printf("%3d", (*((Datum *)loc) + 1));
    return;
}

/*    ---------------------  set_sizes  -----------------------   */
/*    To use info in ctx.vset to set sizes of basic and stats
blocks for variable, and place in AVinst basicsize, statssize.
    */
void set_sizes(iv) int iv;
{

    CurAttr = CurAttrList + iv;
    vaux = (Vaux *)CurAttr->vaux;
    states = vaux->states;

    /*    Set sizes of CVinst (basic) and EVinst (stats) in AVinst  */
    /*    Each inst has a number of vectors appended, of length 'states' */
    CurAttr->basic_size = sizeof(Basic) + (5 * states - 1) * sizeof(double);
    CurAttr->stats_size = sizeof(Stats) + (5 * states - 1) * sizeof(double);
    return;
}

/*    ----------------------- set_best_pars -----------------------  */
void set_best_pars(iv) int iv;
{

    set_var(iv);

    if (CurClass->type == Dad) {
        cvi->bap = nap;
        cvi->bapsprd = cvi->napsprd;
        stats->btcost = stats->ntcost;
        stats->bpcost = stats->npcost;
    } else if ((CurClass->use == Fac) && cvi->infac) {
        cvi->bap = fap;
        cvi->bapsprd = cvi->fapsprd;
        stats->btcost = stats->ftcost;
        stats->bpcost = stats->fpcost;
    } else {
        cvi->bap = sap;
        cvi->bapsprd = cvi->sapsprd;
        stats->btcost = stats->stcost;
        stats->bpcost = stats->spcost;
    }
    return;
}

/*    --------------------  setprobs -------------------------------- */
/*    Routine to set state probs for a item using fap, fbp, CurCaseFacScore */
/*    Sets probs in pr[], -log probs in qr[].  */
/*    Also calcs b1p, b2p, b3p, b1p2, gg  */
void setprobs() {
    double sum, tt, lsum;
    int k, ig;

    b3p = b2p = b1p = 0.0; /* For calculating dispersion of bp[] */
    sum = 0.0;
    Fork {
        tt = fabs(CurCaseFacScore - fbp[k]);
        /*    Do table interpolation in gausorg   */
        tt = tt * Gns;
        ig = tt;
        tt = tt - ig;
        if (ig >= (Grange * Gns))
            lsum = gausorg[Grange * Gns];
        else
            lsum = (1.0 - tt) * gausorg[ig] + tt * gausorg[ig + 1];
        pr[k] = frate[k] * lsum;
        sum += pr[k];
    }
    sum = 1.0 / sum;
    ff = states * states;

    Fork {
        pr[k] *= sum;
        ff *= pr[k];
        tt = pr[k] * fbp[k];
        b1p += tt;
        tt *= fbp[k];
        b2p += tt;
        b3p += tt * fbp[k];
    }

    ff = exp(rstatesm * log(ff + 1.0e-6));
    b1p2 = b1p * b1p;
    gg = b2p - b1p2;
    return;
}

/*    ---------------------------  clear_stats  --------------------   */
/*    Clears stats to accumulate in cost_var, and derives useful functions
of basic params   */
void clear_stats(iv) int iv;
{
    double sum, tt;
    int k;

    set_var(iv);
    stats->cnt = 0.0;
    stats->stcost = stats->ftcost = 0.0;
    stats->vsq = 0.0;
    Fork { scnt[k] = fapd1[k] = fbpd1[k] = 0.0; }
    stats->apd2 = stats->bpd2 = 0.0;
    if (CurClass->age == 0) {
        /*    Set nominal state costs in scst[]  */
        sum = log((double)states) + 1.0;
        Fork scst[k] = sum;
        return;
    }
    /*    Some useful functions  */
    /*    Calc a maximum? for gg  */
    sum = 0.0;
    Fork sum += fbp[k] * fbp[k];
    stats->mgg = 0.5 * sum * rstates;

    /*    Set up non-fac case costs in scst[]  */
    /*    This requires us to calculate probs and log probs of states.  */
    tt = sap[0];
    for (k = 1; k < states; k++) {
        if (sap[k] > tt)
            tt = sap[k];
    }
    sum = 0.0;
    Fork {
        pr[k] = exp(sap[k] - tt);
        sum += pr[k];
    }
    sum = 1.0 / sum;
    tt = 0.0;
    Fork {
        qr[k] = -log(pr[k] * sum + 0.0000001);
        tt -= qr[k];
    }
    /*    Have log of prod of probs in tt. Add log K^2 to get log Fisher  */
    tt += vaux->lstatessq;
    tt = exp(rstatesm * tt); /* 2nd deriv of cost wrt sap[] */
    tt = 0.5 * cvi->sapsprd * tt;
    /*    Set state case costs  */
    Fork scst[k] = qr[k] + tt;

    /*jjj*/
    sum = 0.0;
    Fork {
        frate[k] = exp(fap[k] + 0.5 * fbp[k] * fbp[k]);
        sum += frate[k];
    }
    sum = 1.0 / sum;
    Fork frate[k] *= sum;
    return;
}

/*    -------------------------  score_var  ------------------------   */
/*    To eval derivs of a case wrt score, scorespread. Adds to CaseFacScoreD1,vvd2.
 */
void score_var(iv) int iv;
{
    double t1d1, t2d1, t3d1;
    set_var(iv);
    if (CurAttr->inactive)
        return;
    if (saux->missing)
        return;
    setprobs(); /* Will calc pr[], qr[], gg, ff  */
    t1d1 = b1p - fbp[saux->xn];
    t2d1 = -(0.5 * states * rstatesm) * (cvi->fapsprd + CurCaseFacScoreSq * cvi->bpsprd) * ff * b1p;
    t3d1 = CurCaseFacScore * cvi->bpsprd * ff;

    CaseFacScoreD1 += t1d1 + t3d1 + t2d1;
    CaseFacScoreD2 += gg;
    /*xx    vvd2 += Mbeta * stats->mgg;  */
    EstFacScoreD2 += (gg > stats->mgg) ? gg : stats->mgg;
    /*    Since we don't know vsprd, just calc and accumulate deriv of 'gg' */
    CaseFacScoreD3 += b3p - b1p * (3.0 * gg + b1p2);
    return;
}

/*    ---------------------  cost_var  ---------------------------  */
/*    Accumulate item cost into scasecost, fcasecost  */
void cost_var(iv, fac) int iv, fac;
{
    double cost;

    set_var(iv);
    if (saux->missing)
        return;
    if (CurClass->age == 0) {
        stats->parkftcost = 0.0;
        return;
    }
    /*    Do nofac costing first  */
    cost = scst[saux->xn];
    scasecost += cost;

    /*    Only do faccost if fac  */
    if (!fac)
        goto facdone;
    setprobs();
    cost = -log(pr[saux->xn]); /* -log prob of xn */
    stats->conff = 0.5 * ff * (cvi->fapsprd + CurCaseFacScoreSq * cvi->bpsprd);
    stats->ff = ff;
    stats->parkb1p = b1p;
    stats->parkb2p = b2p;
    cost += stats->conff;
    /*    In cost calculation, use gg as is without Mbeta mod  */
    cost += 0.5 * cvvsprd * gg;

facdone:
    fcasecost += cost;
    stats->parkftcost = cost;
    return;
}

/*    ------------------  deriv_var  ------------------------------  */
/*    Given item weight in cwt, calcs derivs of item cost wrt basic
params and accumulates in paramd1, paramd2  */
void deriv_var(iv, fac) int iv, fac;
{
    double cons1, cons2, inc;
    int k;

    set_var(iv);
    if (saux->missing)
        return;
    /*    Do no-fac first  */
    stats->cnt += CurCaseWeight;
    /*    For non-fac, I just accumulate counts in scnt[]  */
    scnt[saux->xn] += CurCaseWeight;
    /*    Accum. weighted item cost  */
    stats->stcost += CurCaseWeight * scst[saux->xn];
    stats->ftcost += CurCaseWeight * stats->parkftcost;

    /*    Now for factor form  */
    stats->vsq += CurCaseWeight * CurCaseFacScoreSq;
    if (!fac)
        goto facdone;
    b1p = stats->parkb1p;
    b1p2 = b1p * b1p;
    b2p = stats->parkb2p;
    /*    From 1st cost term:  */
    fapd1[saux->xn] -= CurCaseWeight;
    fbpd1[saux->xn] -= CurCaseWeight * CurCaseFacScore;
    Fork {
        fapd1[k] += CurCaseWeight * pr[k];
        fbpd1[k] += CurCaseWeight * pr[k] * CurCaseFacScore;
    }

    /*    Second cost term :  */
    cons1 = CurCaseWeight * stats->conff * rstatesm;
    cons2 = states * cons1;
    Fork {
        inc = cons1 - pr[k] * cons2;
        fapd1[k] += inc;
        fbpd1[k] += CurCaseFacScore * inc;
    }

    /*    Third cost term:  */
    cons1 = 2.0 * b1p2 - b2p;
    cons2 = CurCaseWeight * cvvsprd * Mbeta * rstates;
    Fork {
        inc = 0.5 * CurCaseWeight * cvvsprd * pr[k] * (fbp[k] * fbp[k] - 2.0 * fbp[k] * b1p + cons1);
        fapd1[k] += inc;
        fbpd1[k] += CurCaseFacScore * inc;
        /*    Terms I forgot :  */
        fbpd1[k] += CurCaseWeight * cvvsprd * pr[k] * (fbp[k] - b1p);
        fbpd1[k] += cons2 * fbp[k];
    }

    /*    Second derivs (i.e. derivs wrt fapsprd, bpsprd)  */
    stats->apd2 += CurCaseWeight * stats->ff;
    stats->bpd2 += CurCaseWeight * stats->ff * CurCaseFacScoreSq;
facdone:
    return;
}

/*    -------------------  adjust  ---------------------------    */
/*    To adjust parameters of a multistate variable     */
void adjust(iv, fac) int iv, fac;
{

    double adj, apd2, cnt, vara, del, tt, sum, spcost, fpcost;
    int k, n;

    set_var(iv);
    cnt = stats->cnt;

    if (CurDad) { /* Not root */
        dcvi = (Basic *)CurDad->basics[iv];
        dadnap = &(dcvi->origin);
        dapsprd = dcvi->napsprd;
    } else { /* Root */
        dcvi = 0;
        dadnap = ZeroVec;
        dapsprd = 1.0;
    }

    /*    If too few data, use dad's n-paras   */
    if ((Control & AdjPr) && (cnt < MinSize)) {
        Fork {
            nap[k] = sap[k] = fap[k] = dadnap[k];
            fbp[k] = 0.0;
        }
        cvi->sapsprd = cvi->fapsprd = dapsprd * statesm;
        cvi->napsprd = dapsprd;
        cvi->bpsprd = statesm;
        goto hasage;
    }

    /*    If class age zero, make some preliminary estimates  */
    if (CurClass->age)
        goto hasage;
    stats->oldftcost = 0.0;
    stats->adj = 1.0;
    sum = 0.0;
    Fork {
        sap[k] = log((scnt[k] + 0.5) / (cnt + 0.5 * states));
        qr[k] = -sap[k];
        sum += sap[k]; /* Gives sum of log probs */
    }
    /*    Set sapsprd  */
    apd2 = exp(rstatesm * (sum + vaux->lstatessq)) * cnt;
    apd2 += 1.0 / dapsprd;
    cvi->fapsprd = cvi->bpsprd = cvi->sapsprd = statesm / apd2;

    sum = -sum / states;
    Fork {
        sap[k] += sum;
        fap[k] = sap[k];
        fbp[k] = 0.0;
    }
    /*    Make a stab at item cost   */
    sum = 0.0;
    Fork sum += scnt[k] * qr[k];
    CurClass->cstcost -= sum + 0.5 * statesm;
    CurClass->cftcost = CurClass->cstcost + 100.0 * cnt;

hasage:
    /*    Calculate spcost for non-fac params  */
    vara = 0.0;
    Fork {
        del = sap[k] - dadnap[k];
        vara += del * del;
    }
    vara += cvi->sapsprd; /* Additional variance from roundoff */
    /*    Now vara holds squared difference from sap[] to dad's nap[]. This
    is a variance in (states-1) space with sum-sq spread dapsprd, Normal form */
    spcost = 0.5 * vara / dapsprd;          /* The squared deviations term */
    spcost += 0.5 * statesm * log(dapsprd); /* statesm * log sigma */
    spcost += statesm * (HALF_LOG_2PI + LATTICE);
    /*    This completes the prior density terms  */
    /*    The vol of uncertainty is (sapsprd/statesm)^(statesm/2)  */
    spcost -= 0.5 * statesm * log(cvi->sapsprd / statesm);

    if (!fac) {
        fpcost = spcost + 100.0;
        cvi->infac = 1;
        goto facdone1;
    }
    /*    Get factor pcost  */
    vara = 0.0;
    Fork {
        del = fap[k] - dadnap[k];
        vara += del * del;
    }
    vara += cvi->fapsprd; /* Additional variance from roundoff */
    /*    Now vara holds squared difference from fap[] to dad's nap[]. This
    is a variance in (states-1) space with sum-sq spread dapsprd, Normal form */
    fpcost = 0.5 * vara / dapsprd;          /* The squared deviations term */
    fpcost += 0.5 * statesm * log(dapsprd); /* statesm * log sigma */
    fpcost += statesm * (HALF_LOG_2PI + LATTICE);
    /*    The vol of uncertainty is (fapsprd/statesm)^(statesm/2)  */
    fpcost -= 0.5 * statesm * log(cvi->fapsprd / statesm);

    /*    And for fbp[]:  (N(0,1) prior)  */
    vara = 0.0;
    Fork vara += fbp[k] * fbp[k];
    vara += cvi->bpsprd;  /* Additional variance from roundoff */
    fpcost += 0.5 * vara; /* The squared deviations term */
    fpcost += statesm * (HALF_LOG_2PI + LATTICE);
    /*    The vol of uncertainty is (bpsprd/statesm)^(statesm/2)  */
    fpcost -= 0.5 * statesm * log(cvi->bpsprd / statesm);

facdone1:
    /*    Store param costs  */
    stats->spcost = spcost;
    stats->fpcost = fpcost;
    /*    Add to class param costs  */
    CurClass->nofac_par_cost += spcost;
    CurClass->fac_par_cost += fpcost;
    if (!(Control & AdjPr))
        goto adjdone;
    if (cnt < MinSize)
        goto adjdone;

    /*    Adjust non-fac params.  */
    n = 3;
adjloop:
    /*    Get pr[], sum log p[r] from sap[]  */
    tt = sap[0];
    for (k = 1; k < states; k++) {
        if (sap[k] > tt)
            tt = sap[k];
    }
    sum = 0.0;
    Fork {
        pr[k] = exp(sap[k] - tt);
        sum += pr[k];
    }
    tt = 0.0;
    sum = 1.0 / sum;
    Fork {
        pr[k] *= sum;
        tt += log(pr[k]);
    }
    /*    Calc ff = (case Fisher)^(K-1)   */
    tt += vaux->lstatessq;
    ff = exp(rstatesm * tt);
    /*    The deriv of cost wrt sap[k] contains the term:
            0.5*cnt*sapsprd* (ff/(K-1)) * (1 - K*pr[k])  */
    tt = 0.5 * cnt * cvi->sapsprd * ff * rstatesm;
    /*    Use dads's nap[], dapsprd for Normal prior.   */
    /*    Reduce corrections by statesm/states  */
    adj = InitialAdj * statesm / states;
    for (k = 0; k < states; k++) {
        del = (sap[k] - dadnap[k]) / dapsprd; /* 1st deriv from prior */
        del += pr[k] * cnt - scnt[k];         /* From data */
        del += 0.5 * pr[k];                   /*  Stabilization  */
        del += tt * (1.0 - states * pr[k]);   /* from roundoff */
        /*    2nd deriv approx cnt*pr[k], plus 1/dapsprd from prior  */
        sap[k] -= del * adj / (1.0 + scnt[k] + 1.0 / dapsprd);
    }
    sum = 0.0;
    for (k = 0; k < states; k++)
        sum += sap[k];
    sum = -sum / states;
    for (k = 0; k < states; k++)
        sap[k] += sum;
    /*    Compute sapsprd  */
    apd2 = (1.0 / dapsprd) + cnt * ff;
    cvi->sapsprd = statesm / apd2;
    /*    Repeat the adjustment  */
    if (--n)
        goto adjloop;

    if (!fac)
        goto facdone2;

    /*    Adjust factor parameters.  We have fapd1[], fbpd1[] from the data,
        but must add derivatives of pcost terms.  */
    for (k = 0; k < states; k++) {
        fapd1[k] += (fap[k] - dadnap[k]) / dapsprd;
        fbpd1[k] += fbp[k];
    }
    stats->apd2 += 1.0 / dapsprd;
    stats->bpd2 += 1.0;
    /*    Stabilization  */
    CurCaseFacScore = 0.0;
    setprobs();
    for (k = 0; k < states; k++)
        fapd1[k] += 0.5 * pr[k];
    stats->apd2 += 0.5 * states * ff;
    /*    This section uses a slow but apparently safe adjustment of fa[[], fbp[]
     */
    /*    In an attempt to speed things, fiddle adjustment multiple   */
    sum = stats->ftcost / cnt;
    if (sum < stats->oldftcost)
        adj = stats->adj * 1.1;
    else
        adj = InitialAdj;
    if (adj > MaxAdj)
        adj = MaxAdj;
    stats->adj = adj;
    stats->oldftcost = sum;
    /*    To do the adjustments, divide 1st derivs by a 'max' value of 2nd
    derivs, held in vaux.  */
    adj = adj / (stats->cnt * vaux->mff);
    adj *= statesm / states;
    for (k = 0; k < states; k++) {
        fap[k] -= adj * fapd1[k];
        fbp[k] -= adj * fbpd1[k];
    }

    sum = 0.0;
    for (k = 0; k < states; k++) sum += fap[k];
    sum = -sum / states;
    for (k = 0; k < states; k++) fap[k] += sum;
    sum = 0.0;
    for (k = 0; k < states; k++) sum += fbp[k];
    sum = -sum / states;
    for (k = 0; k < states; k++) fbp[k] += sum;
    /*    Set fapsprd, bpsprd.   */
    cvi->fapsprd = statesm / stats->apd2;
    cvi->bpsprd = statesm / stats->bpd2;

facdone2:
    /*    If no sons, set as-dad params from non-fac params  */
    if (CurClass->num_sons < 2) {
        for (k = 0; k < states; k++) nap[k] = sap[k];
        cvi->napsprd = cvi->sapsprd;
    }
    cvi->samplesize = stats->cnt;

adjdone:
    return;
}

/*    ------------------------  prprint  -------------------  */
/*    prints the exponential params in 'ap' converted to probs   */
void prprint(ap) double *ap;
{
    int k;
    double max, sum;

    max = ap[0];
    Fork {
        if (ap[k] > max)
            max = ap[k];
    }
    sum = 0.0;
    Fork {
        pr[k] = exp(ap[k] - max);
        sum += pr[k];
    }
    sum = 1.0 / sum;
    Fork printf("%7.3f", pr[k] * sum);
    return;
}

/*    ------------------------  show  -----------------------   */
void show(ccl, iv) Class *ccl;
int iv;
{
    int k;

    set_class(ccl);
    set_var(iv);
    printf("V%3d  Cnt%6.1f  %s  Adj%8.2f\n", iv + 1, stats->cnt, (cvi->infac) ? " In" : "Out", stats->adj);

    if (CurClass->num_sons < 2)
        goto skipn;
    printf(" NR: ");
    prprint(nap);
    printf(" +-%7.3f\n", sqrt(cvi->napsprd));
skipn:
    printf(" PR: ");
    prprint(sap);
    printf("\n");
    if (CurClass->use == Tiny)
        goto skipf;
    printf(" FR: ");
    for (k = 0; k < states; k++) printf("%7.3f", frate[k]);
    printf("\n BP: ");
    for (k = 0; k < states; k++) printf("%7.3f", fbp[k]);
    printf(" +-%7.3f\n", sqrt(cvi->bpsprd * rstatesm));
skipf:;
    return;
}

/*    ----------------------  cost_var_nonleaf -----------------------------  */
void cost_var_nonleaf(iv) int iv;
{
    Basic *soncvi;
    Class *son;
    double del, co0, co1, co2, tstvn, tssn;
    double pcost;
    double apsprd;
    int n, k, ison, nson, nints;

    set_var(iv);
    if (CurAttr->inactive) {
        stats->npcost = stats->ntcost = 0.0;
        return;
    }
    nson = CurClass->num_sons;
    if (nson < 2) { /* cannot define parameters */
        stats->npcost = stats->ntcost = 0.0;
        Fork nap[k] = sap[k];
        cvi->napsprd = 1.0;
        return;
    }
    /*      We need to accumlate things over sons. We need:   */
    nints = 0;        /* Number of internal sons (M) */
    Fork pr[k] = 0.0; /* Total of sons' bap[] vectors */
    tstvn = 0.0;      /* Total variance of sons' vectors */
    tssn = 0.0;       /* Total internal sons' bapsprd  */

    apsprd = cvi->napsprd;
    /*    The calculation is like that in reals.c (q.v.) but now we have
    states-1 parameter components for each son. Thus, 'nson' in the reals case
    sometimes becomes (statesm * nson) in this case. We use the static vector
    'pr[]' to accumulate the sum of the sons' bap[]s, in place of the 'tstn' of
    the reals code. */
    for (ison = CurClass->son_id; ison > 0; ison = son->sib_id) {
        son = CurCtx.popln->classes[ison];
        soncvi = (Basic *)son->basics[iv];
        Fork {
            pr[k] += soncvi->bap[k];
            tstvn += soncvi->bap[k] * soncvi->bap[k];
        }
        if (son->type == Dad) { /* used as parent */
            nints++;
            tssn += soncvi->bapsprd;
            tstvn += soncvi->bapsprd * statesm / son->num_sons;
        } else
            tstvn += soncvi->bapsprd * statesm;
    }
    /*      Calc coeffs for quadratic c_2 * s^2 + c_1 * s - c_0  */
    co2 = (1.0 + 0.5 * statesm / nson);
    co1 = nints + 0.5 * (statesm * nson - 3.0);
    /*      Can now compute V in the 'reals' comments.  tssn = S of the comments
     */
    /*      First we get the V around the sons' mean, then update nap and
       correct the value of V.  We use static qr[] for the mean. */
    for (k = 0; k < states; k++) {
        qr[k] = pr[k] / nson;
        tstvn -= pr[k] * qr[k];
    }
    /*    tstvn now gives total variance about sons' mean in (states-1) space */

    /*      Iterate the adjustment of param, spread  */
    n = 5;
adjloop:
    /*      Update param  */
    /*      The V of comments is tstvn + nson * del * del */
    del = 0.0;
    for (k = 0; k < states; k++) {
        nap[k] = (dapsprd * qr[k] + apsprd * dadnap[k]) / (nson * dapsprd + apsprd);
        del = nap[k] - qr[k];
        tstvn += del * del * nson; /* adding variance round new nap */
    }
    co0 = 0.5 * (tstvn + nson * del * del) + tssn;
    /*      Solve for new spread  */
    apsprd = 2.0 * co0 / (co1 + sqrt(co1 * co1 + 4.0 * co0 * co2));
    n--;
    if (n)
        goto adjloop;
    /*    Store new values  */
    cvi->napsprd = apsprd;

    /*      Calc cost  */
    pcost = 0.0;
    for (k = 0; k < states; k++) {
        del = nap[k] - dadnap[k];
        pcost += del * del;
    }
    pcost = 0.5 * (pcost + statesm * apsprd / nson) / dapsprd + statesm * (HALF_LOG_2PI + 0.5 * log(dapsprd));
    /*      Add hlog Fisher, lattice  */
    pcost += 0.5 * log(0.5 * nson * statesm + nints) + 0.5 * statesm * log((double)nson) - 0.5 * (statesm + 2.0) * log(apsprd) + states * LATTICE;
    /*    Add roundoff for states params  */
    pcost += 0.5 * states;
    stats->npcost = pcost;
    return;
}
/*zzzz*/
