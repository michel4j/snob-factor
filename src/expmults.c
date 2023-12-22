/*	File of stuff for Multistate variables.   */
#include "glob.h"

#define MaxState 20 /* Max number of discrete states per variable */

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

/*	Common variable for holding a data value  */

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
    double napsprd; /* basically, the squared sprd among the ap[] params of children  */
    double sapsprd;
    double fapsprd;
    double bpsprd;
    double samplesize; /* Size of sample on which estimates based */
    double origin;
    /* This becomes the first element in a number of
    vectors, each of length 'states', which will be
    appended to the Basic sructure. Pointers to these
    vectors will be set in the static pointers declared
    below. A similar item happens to the Stats structure.
    */
} Basic;

/*	Pointers to vectors in Basic  */
// static double *nap;       /* Log odds as dad */
// static double *sap;       /*  Log odds without factor  */
// static double *fap, *fbp; /* Const and load params for logodds */
// static double *frate;
/*
    The factor model is that prob of state k for a case with score v
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
    many calls on exp().
*/

typedef struct Statsst { /* Stuff accumulated to revise Basic  */
                         /* First fields are standard  */
    double cnt;          /* Weighted count  */
    double btcost, ntcost, stcost, ftcost;
    double bpcost, npcost, spcost, fpcost;
    double vsq; /*  weighted sum of squared scores for this var */
    int id;     /* Variable number  */
    /********************  Following fields vary according to need ***/
    double parkftcost;                  /* Unweighted item costs of xn */
    double ff, parkb1p, parkb2p, conff; /* Useful quants calced in cost_var, used in deriv_var */
    double oldftcost;
    double adj;
    double apd2, bpd2;
    double mgg;    /* A maximum? for gg */
    double origin; /* First element of states vectors */
} Stats;

/*	Pointers to vectors in stats  */
// static double *scnt;          /*  vector of counts in states  */
// static double *scst;          /*  vector of non-fac state costs  */
// static double *pr;            /* vec. of factor state probs  */
// static double *fapd1, *fbpd1; /*  vectors of derivs of cost wrt params */

// /*	Static variables specific to this type   */
// static int states;
// static double statesm; /*  states - 1.0 */
// static double rstates; /*  1.0 / states  */
// static double rstatesm;
// static double qr[MaxState]; /*  - logs of state probs  */

static double *dadnap; /* To dad's nap[] */
static double dapsprd; /* Dad's napsprd */

/*Variables specific to this type   */
typedef struct ProbStruct {
    double b1p, b2p, b3p, b1p2, gg, ff;
} Prob;

/*	Stuff for a table of the function exp (-0.5 * x * x)
    for x in the range 0 to Grange in Gns steps per unit of x  */
#define Grange ((int)6)
#define Gns ((int)512)
#define Gh ((double)1.0 / Gns)
#define Gnt ((int)Grange * Gns + 3) /* Number of table entries */
static double gaustab[Gnt];
static double *gausorg; /* ptr to gaustab[1] */

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
types of variable, and this name must be copied into the file "definetypes.c"
when installing a new type of variable. It is also necessary to change the
"Ntypes" constant, and to decide on a type id (an integer) for the new type.
    */

void expmults_define(typindx) int typindx;
/*	typindx is the index in types[] of this type   */
{
    int ig;
    double xg;
    VarType *vtype;

    vtype = &Types[typindx];
    vtype->id = typindx;
    /* 	Set type name as string up to 59 chars  */
    vtype->name = "MultiState";
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

    /*	Make table of exp (-0.5 * x * x) in gaustab[]
        Entry for x = 0 is at gaustab[1]  */
    gausorg = gaustab + 1;
    for (ig = -1; ig <= (Grange * Gns + 1); ig++) {
        xg = ig * Gh;
        gausorg[ig] = exp(-0.5 * xg * xg);
    }
}

/*	----------------------- setvar --------------------------  */
void set_var(int iv, Class *cls) {}

/*	---------------------  readvaux ---------------------------   */
/*	To read any auxiliary info about a variable of this type in some
sample.
    */
int read_attr_aux(void *vaux) {
    int i, states;

    /*	Read in auxiliary info into vaux, return 0 if OK else 1  */
    i = read_int(&states, 1);
    if (i < 0) {
        return (1);
    } else {
        return set_attr_aux(vaux, states);
    }
}
int set_attr_aux(void *vaux, int states) {
    double lstates, rstatesm;
    Vaux *vax = (Vaux *)vaux;
    vax->states = states;
    if (vax->states > MaxState) {
        printf("Variable has more than %d states (%d)\n", MaxState, vax->states);
        return (2);
    }
    lstates = log((double)(vax->states));
    vax->lstatessq = 2.0 * lstates;
    /*	Calc a maximum value for ff, the (K-1)th root of the Fisher
        given by K^2 * Prod_k {pr_k}.  The maximum occurs when
        pr_k = 1/K for all k, so  */
    rstatesm = 1.0 / (vax->states - 1);
    vax->mff = exp(-rstatesm * (vax->states - 2) * lstates);
    /*	For safety, inflate by states/statesm  */
    vax->mff *= vax->states * rstatesm;
    return (0);
}

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
    VSetVar *vset_var = &CurCtx.vset->variables[iv];
    Vaux *vaux = (Vaux *)vset_var->vaux;
    if (!xn) {
        return -1 * (int)sizeof(int); /* Missing */
    }
    xn--;
    if ((xn < 0) || (xn >= vaux->states)) {
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
    Vaux *vaux = (Vaux *)vset_var->vaux;
    int states = vaux->states;

    /*	Set sizes of ClassVar (basic) and ExplnVar (stats) in VSetVar  */
    /*	Each inst has a number of vectors appended, of length 'states' */
    vset_var->basic_size = sizeof(Basic) + (5 * states - 1) * sizeof(double);
    vset_var->stats_size = sizeof(Stats) + (5 * states - 1) * sizeof(double);
    return;
}

/*	----------------------- set_best_pars -----------------------  */
void set_best_pars(int iv, Class *cls) {
    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];
    set_var(iv, cls);

    VSetVar *vset_var = &CurCtx.vset->variables[iv];
    Vaux *vaux = (Vaux *)vset_var->vaux;
    int states = vaux->states;
    double *nap = &(cls_var->origin);
    double *sap = &nap[states];
    double *fap = &sap[states];

    if (cls->type == Dad) {
        cls_var->bap = nap;
        cls_var->bapsprd = cls_var->napsprd;
        exp_var->btcost = exp_var->ntcost;
        exp_var->bpcost = exp_var->npcost;
    } else if ((cls->use == Fac) && cls_var->infac) {
        cls_var->bap = fap;
        cls_var->bapsprd = cls_var->fapsprd;
        exp_var->btcost = exp_var->ftcost;
        exp_var->bpcost = exp_var->fpcost;
    } else {
        cls_var->bap = sap;
        cls_var->bapsprd = cls_var->sapsprd;
        exp_var->btcost = exp_var->stcost;
        exp_var->bpcost = exp_var->spcost;
    }
    return;
}

/*	--------------------  set_probs -------------------------------- */
/*	Routine to set state probs for a item using fap, fbp, cvv */
/*	Sets probs in pr[], -log probs in qr[].  */
/*	Also calcs b1p, b2p, b3p, b1p2, gg  */
void set_probs(int iv, Prob *probs, Class *cls) {
    double sum, tt, lsum;
    int k, ig;

    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];
    VSetVar *vset_var = &CurCtx.vset->variables[iv];
    Vaux *vaux = (Vaux *)vset_var->vaux;
    int states = vaux->states;
    double statesm = states - 1;
    double rstatesm = 1.0 / statesm;
    double *nap = &(cls_var->origin);
    double *sap = &nap[states];
    double *fap = &sap[states];
    double *fbp = &fap[states];
    double *frate = &fbp[states];
    double *scnt = &(exp_var->origin);
    double *scst = &scnt[states];
    double *pr = &scst[states];

    probs->b3p = probs->b2p = probs->b1p = 0.0; /* For calculating dispersion of bp[] */
    sum = 0.0;
    for (k = 0; k < states; k++) {
        tt = fabs(Scores.CaseFacScore - fbp[k]);
        /*	Do table interpolation in gausorg   */
        tt = tt * Gns;
        ig = tt;
        tt = tt - ig;
        if (ig >= (Grange * Gns)) {
            lsum = gausorg[Grange * Gns];
        } else {
            lsum = (1.0 - tt) * gausorg[ig] + tt * gausorg[ig + 1];
        }
        pr[k] = frate[k] * lsum;
        sum += pr[k];
    }
    sum = 1.0 / sum;
    probs->ff = states * states;

    for (k = 0; k < states; k++) {
        pr[k] *= sum;
        probs->ff *= pr[k];
        tt = pr[k] * fbp[k];
        probs->b1p += tt;
        tt *= fbp[k];
        probs->b2p += tt;
        probs->b3p += tt * fbp[k];
    }

    probs->ff = exp(rstatesm * log(probs->ff + 1.0e-6));
    probs->b1p2 = probs->b1p * probs->b1p;
    probs->gg = probs->b2p - probs->b1p2;
}

/*	---------------------------  clearstats  --------------------   */
/*	Clears stats to accumulate in cost_var, and derives useful functions
of basic params   */
void clear_stats(int iv, Class *cls) {
    double sum, tt;
    int k;

    VSetVar *vset_var = &CurCtx.vset->variables[iv];
    Vaux *vaux = (Vaux *)vset_var->vaux;
    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];

    int states = vaux->states;
    double statesm = states - 1;
    double rstates = 1.0 / states;
    double rstatesm = 1.0 / statesm;
    double *nap = &(cls_var->origin);
    double *sap = &nap[states];
    double *fap = &sap[states];
    double *fbp = &fap[states];
    double *frate = &fbp[states];
    double *scnt = &(exp_var->origin);
    double *scst = &scnt[states];
    double *pr = &scst[states];
    double *fapd1 = &pr[states];
    double *fbpd1 = &fapd1[states];
    double qr[MaxState];

    set_var(iv, cls);
    exp_var->cnt = 0.0;
    exp_var->stcost = exp_var->ftcost = 0.0;
    exp_var->vsq = 0.0;
    for (k = 0; k < states; k++) {
        scnt[k] = fapd1[k] = fbpd1[k] = 0.0;
    }
    exp_var->apd2 = exp_var->bpd2 = 0.0;
    if (cls->age == 0) {
        /*	Set nominal state costs in scst[]  */
        sum = log((double)states) + 1.0;
        for (k = 0; k < states; k++)
            scst[k] = sum;
        return;
    }
    /*	Some useful functions  */
    /*	Calc a maximum? for gg  */
    sum = 0.0;
    for (k = 0; k < states; k++)
        sum += fbp[k] * fbp[k];
    exp_var->mgg = 0.5 * sum * rstates;

    /*	Set up non-fac case costs in scst[]  */
    /*	This requires us to calculate probs and log probs of states.  */
    tt = sap[0];
    for (k = 1; k < states; k++) {
        if (sap[k] > tt)
            tt = sap[k];
    }
    sum = 0.0;
    for (k = 0; k < states; k++) {
        pr[k] = exp(sap[k] - tt);
        sum += pr[k];
    }
    sum = 1.0 / sum;
    tt = 0.0;
    for (k = 0; k < states; k++) {
        qr[k] = -log(pr[k] * sum + 0.0000001);
        tt -= qr[k];
    }
    /*	Have log of prod of probs in tt. Add log K^2 to get log Fisher  */
    tt += vaux->lstatessq;
    tt = exp(rstatesm * tt); /* 2nd deriv of cost wrt sap[] */
    tt = 0.5 * cls_var->sapsprd * tt;
    /*	Set state case costs  */
    for (k = 0; k < states; k++)
        scst[k] = qr[k] + tt;

    /*jjj*/
    sum = 0.0;
    for (k = 0; k < states; k++) {
        frate[k] = exp(fap[k] + 0.5 * fbp[k] * fbp[k]);
        sum += frate[k];
    }
    sum = 1.0 / sum;
    for (k = 0; k < states; k++)
        frate[k] *= sum;
    return;
}

/*	-------------------------  score_var  ------------------------   */
/*	To eval derivs of a case wrt score, scorespread. Adds to vvd1,vvd2.
 */
void score_var(int iv, Class *cls) {
    double t1d1, t2d1, t3d1;
    VSetVar *vset_var = &CurCtx.vset->variables[iv];
    SampleVar *smpl_var = &CurCtx.sample->variables[iv];
    Saux *saux = (Saux *)(smpl_var->saux);
    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];
    Vaux *vaux = (Vaux *)vset_var->vaux;

    int states = vaux->states;
    double statesm = states - 1;
    double rstatesm = 1.0 / statesm;
    double *nap = &(cls_var->origin);
    double *sap = &nap[states];
    double *fap = &sap[states];
    double *fbp = &fap[states];
    Prob probs;
    set_var(iv, cls);
    if ((!vset_var->inactive) && (!saux->missing)) {
        set_probs(iv, &probs, cls); /* Will calc pr[], qr[], gg, ff  */
        t1d1 = probs.b1p - fbp[saux->xn];
        t2d1 = -(0.5 * states * rstatesm) * (cls_var->fapsprd + Scores.CaseFacScoreSq * cls_var->bpsprd) * probs.ff * probs.b1p;
        t3d1 = Scores.CaseFacScore * cls_var->bpsprd * probs.ff;

        Scores.CaseFacScoreD1 += t1d1 + t3d1 + t2d1;
        Scores.CaseFacScoreD2 += probs.gg;
        /* xx	vvd2 += Mbeta * evi->mgg;  */
        Scores.EstFacScoreD2 += (probs.gg > exp_var->mgg) ? probs.gg : exp_var->mgg;
        /* Since we don't know vsprd, just calc and accumulate deriv of 'gg' */
        Scores.CaseFacScoreD3 += probs.b3p - probs.b1p * (3.0 * probs.gg + probs.b1p2);
    }
}

/*	---------------------  cost_var  ---------------------------  */
/*	Accumulate item cost into CaseNoFacCost, fcasecost  */
void cost_var(int iv, int fac, Class *cls) {
    double cost;

    SampleVar *smpl_var = &CurCtx.sample->variables[iv];
    Saux *saux = (Saux *)(smpl_var->saux);
    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];
    VSetVar *vset_var = &CurCtx.vset->variables[iv];
    Vaux *vaux = (Vaux *)vset_var->vaux;
    int states = vaux->states;
    double *scnt = &(exp_var->origin);
    double *scst = &scnt[states];
    double *pr = &scst[states];

    set_var(iv, cls);
    if (saux->missing) {
        return;
    }

    if (cls->age == 0) {
        exp_var->parkftcost = 0.0;
        return;
    }

    /*	Do nofac costing first  */
    cost = scst[saux->xn];
    Scores.CaseNoFacCost += cost;

    /*	Only do faccost if fac  */
    Prob probs;
    if (fac) {
        set_probs(iv, &probs, cls);
        cost = -log(pr[saux->xn]); /* -log prob of xn */
        exp_var->conff = 0.5 * probs.ff * (cls_var->fapsprd + Scores.CaseFacScoreSq * cls_var->bpsprd);
        exp_var->ff = probs.ff;
        exp_var->parkb1p = probs.b1p;
        exp_var->parkb2p = probs.b2p;
        cost += exp_var->conff;
        cost += 0.5 * Scores.cvvsprd * probs.gg; /* In cost calculation, use gg as is without Mbeta mod  */
    }
    Scores.CaseFacCost += cost;
    exp_var->parkftcost = cost;
}

/*	------------------  deriv_var  ------------------------------  */
/*	Given item weight in cwt, calcs derivs of item cost wrt basic
params and accumulates in paramd1, paramd2  */
void deriv_var(int iv, int fac, Class *cls) {
    double cons1, cons2, inc;
    const double case_weight = cls->case_weight;
    int k;
    SampleVar *smpl_var = &CurCtx.sample->variables[iv];
    Saux *saux = (Saux *)(smpl_var->saux);
    Stats *exp_var = (Stats *)cls->stats[iv];
    Basic *cls_var = (Basic *)cls->basics[iv];
    VSetVar *vset_var = &CurCtx.vset->variables[iv];
    Vaux *vaux = (Vaux *)vset_var->vaux;
    int states = vaux->states;
    double statesm = states - 1;
    double rstates = 1.0 / states;
    double rstatesm = 1.0 / statesm;
    double *nap = &(cls_var->origin);
    double *sap = &nap[states];
    double *fap = &sap[states];
    double *fbp = &fap[states];
    double *scnt = &(exp_var->origin);
    double *scst = &scnt[states];
    double *pr = &scst[states];
    double *fapd1 = &pr[states];
    double *fbpd1 = &fapd1[states];

    set_var(iv, cls);
    if (saux->missing) {
        return;
    }
    /*	Do no-fac first  */
    exp_var->cnt += case_weight;

    scnt[saux->xn] += case_weight;                        /* For non-fac, I just accumulate counts in scnt[]  */
    exp_var->stcost += case_weight * scst[saux->xn];      /* Accum. weighted item cost  */
    exp_var->ftcost += case_weight * exp_var->parkftcost; /* Accum. weighted item cost  */

    /*	Now for factor form  */
    Prob probs;

    exp_var->vsq += case_weight * Scores.CaseFacScoreSq;
    if (fac) {
        probs.b1p = exp_var->parkb1p;
        probs.b1p2 = probs.b1p * probs.b1p;
        probs.b2p = exp_var->parkb2p;

        /*	From 1st cost term:  */
        fapd1[saux->xn] -= case_weight;
        fbpd1[saux->xn] -= case_weight * Scores.CaseFacScore;
        for (k = 0; k < states; k++) {
            fapd1[k] += case_weight * pr[k];
            fbpd1[k] += case_weight * pr[k] * Scores.CaseFacScore;
        }

        /*	Second cost term :  */
        cons1 = case_weight * exp_var->conff * rstatesm;
        cons2 = states * cons1;
        for (k = 0; k < states; k++) {
            inc = cons1 - pr[k] * cons2;
            fapd1[k] += inc;
            fbpd1[k] += Scores.CaseFacScore * inc;
        }

        /*	Third cost term:  */
        cons1 = 2.0 * probs.b1p2 - probs.b2p;
        cons2 = case_weight * Scores.cvvsprd * Mbeta * rstates;
        for (k = 0; k < states; k++) {
            inc = 0.5 * case_weight * Scores.cvvsprd * pr[k] * (fbp[k] * fbp[k] - 2.0 * fbp[k] * probs.b1p + cons1);
            fapd1[k] += inc;
            fbpd1[k] += Scores.CaseFacScore * inc;
            /*	Terms I forgot :  */
            fbpd1[k] += case_weight * Scores.cvvsprd * pr[k] * (fbp[k] - probs.b1p);
            fbpd1[k] += cons2 * fbp[k];
        }

        /*	Second derivs (i.e. derivs wrt fapsprd, bpsprd)  */
        exp_var->apd2 += case_weight * exp_var->ff;
        exp_var->bpd2 += case_weight * exp_var->ff * Scores.CaseFacScoreSq;
    }
    return;
}

/*	-------------------  adjust  ---------------------------    */
/*	To adjust parameters of a multistate variable     */
void adjust(int iv, int fac, Class *cls) {

    double adj, apd2, cnt, vara, del, tt, sum, spcost, fpcost;
    int k, n;

    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];
    VSetVar *vset_var = &CurCtx.vset->variables[iv];
    Vaux *vaux = (Vaux *)vset_var->vaux;

    int states = vaux->states;
    double statesm = states - 1;
    double rstatesm = 1.0 / statesm;
    double *nap = &(cls_var->origin);
    double *sap = &nap[states];
    double *fap = &sap[states];
    double *fbp = &fap[states];
    double *scnt = &(exp_var->origin);
    double *scst = &scnt[states];
    double *pr = &scst[states];
    double *fapd1 = &pr[states];
    double *fbpd1 = &fapd1[states];
    double qr[MaxState];

    Prob probs;

    set_var(iv, cls);
    cnt = exp_var->cnt;

    Class *dad = (cls->dad_id >= 0) ? CurCtx.popln->classes[cls->dad_id] : 0;
    Basic *dad_var = (dad) ? (Basic *)dad->basics[iv] : 0;

    if (dad) { /* Not root */
        dad_var = (Basic *)dad->basics[iv];
        dadnap = &(dad_var->origin);
        dapsprd = dad_var->napsprd;
    } else { /* Root */
        dad_var = 0;
        dadnap = ZeroVec;
        dapsprd = 1.0;
    }

    /*	If too few data, use dad's n-paras   */
    if ((Control & AdjPr) && (cnt < MinSize)) {
        for (k = 0; k < states; k++) {
            nap[k] = sap[k] = fap[k] = dadnap[k];
            fbp[k] = 0.0;
        }
        cls_var->sapsprd = cls_var->fapsprd = dapsprd * statesm;
        cls_var->napsprd = dapsprd;
        cls_var->bpsprd = statesm;

    } else if (!cls->age) {
        /*	If class age zero, make some preliminary estimates  */
        exp_var->oldftcost = 0.0;
        exp_var->adj = 1.0;
        sum = 0.0;
        for (k = 0; k < states; k++) {
            sap[k] = log((scnt[k] + 0.5) / (cnt + 0.5 * states));
            qr[k] = -sap[k];
            sum += sap[k]; /* Gives sum of log probs */
        }
        /*	Set sapsprd  */
        apd2 = exp(rstatesm * (sum + vaux->lstatessq)) * cnt;
        apd2 += 1.0 / dapsprd;
        cls_var->fapsprd = cls_var->bpsprd = cls_var->sapsprd = statesm / apd2;

        sum = -sum / states;
        for (k = 0; k < states; k++) {
            sap[k] += sum;
            fap[k] = sap[k];
            fbp[k] = 0.0;
        }
        /*	Make a stab at item cost   */
        sum = 0.0;
        for (k = 0; k < states; k++)
            sum += scnt[k] * qr[k];
        cls->cstcost -= sum + 0.5 * statesm;
        cls->cftcost = cls->cstcost + 100.0 * cnt;
    }

    /*	Calculate spcost for non-fac params  */
    vara = 0.0;
    for (k = 0; k < states; k++) {
        del = sap[k] - dadnap[k];
        vara += del * del;
    }
    vara += cls_var->sapsprd; /* Additional variance from roundoff */
    /*	Now vara holds squared difference from sap[] to dad's nap[]. This
    is a variance in (states-1) space with sum-sq spread dapsprd, Normal form */
    spcost = 0.5 * vara / dapsprd;          /* The squared deviations term */
    spcost += 0.5 * statesm * log(dapsprd); /* statesm * log sigma */
    spcost += statesm * (HALF_LOG_2PI + LATTICE);
    /*	This completes the prior density terms  */
    /*	The vol of uncertainty is (sapsprd/statesm)^(statesm/2)  */
    spcost -= 0.5 * statesm * log(cls_var->sapsprd / statesm);

    if (fac) {
        /*	Get factor pcost  */
        vara = 0.0;
        for (k = 0; k < states; k++) {
            del = fap[k] - dadnap[k];
            vara += del * del;
        }
        vara += cls_var->fapsprd; /* Additional variance from roundoff */
        /*	Now vara holds squared difference from fap[] to dad's nap[]. This
        is a variance in (states-1) space with sum-sq spread dapsprd, Normal form */
        fpcost = 0.5 * vara / dapsprd;          /* The squared deviations term */
        fpcost += 0.5 * statesm * log(dapsprd); /* statesm * log sigma */
        fpcost += statesm * (HALF_LOG_2PI + LATTICE);
        /*	The vol of uncertainty is (fapsprd/statesm)^(statesm/2)  */
        fpcost -= 0.5 * statesm * log(cls_var->fapsprd / statesm);

        /*	And for fbp[]:  (N(0,1) prior)  */
        vara = 0.0;
        for (k = 0; k < states; k++)
            vara += fbp[k] * fbp[k];
        vara += cls_var->bpsprd; /* Additional variance from roundoff */
        fpcost += 0.5 * vara;    /* The squared deviations term */
        fpcost += statesm * (HALF_LOG_2PI + LATTICE);
        /*	The vol of uncertainty is (bpsprd/statesm)^(statesm/2)  */
        fpcost -= 0.5 * statesm * log(cls_var->bpsprd / statesm);
    } else {
        fpcost = spcost + 100.0;
        cls_var->infac = 1;
    }

    /*	Store param costs  */
    exp_var->spcost = spcost;
    exp_var->fpcost = fpcost;
    /*	Add to class param costs  */
    cls->nofac_par_cost += spcost;
    cls->fac_par_cost += fpcost;
    if ((!(Control & AdjPr)) || (cnt < MinSize)) {
        return;
    }

    /*	Adjust non-fac params.  */
    for (n = 0; n < 3; n++) {
        /*	Get pr[], sum log p[r] from sap[]  */
        tt = sap[0];
        for (k = 1; k < states; k++) {
            if (sap[k] > tt)
                tt = sap[k];
        }
        sum = 0.0;
        for (k = 0; k < states; k++) {
            pr[k] = exp(sap[k] - tt);
            sum += pr[k];
        }
        tt = 0.0;
        sum = 1.0 / sum;
        for (k = 0; k < states; k++) {
            pr[k] *= sum;
            tt += log(pr[k]);
        }
        /*	Calc ff = (case Fisher)^(K-1)   */
        tt += vaux->lstatessq;
        probs.ff = exp(rstatesm * tt);
        /*	The deriv of cost wrt sap[k] contains the term:
                0.5*cnt*sapsprd* (ff/(K-1)) * (1 - K*pr[k])  */
        tt = 0.5 * cnt * cls_var->sapsprd * probs.ff * rstatesm;
        /*	Use dads's nap[], dapsprd for Normal prior.   */
        /*	Reduce corrections by statesm/states  */
        adj = InitialAdj * statesm / states;
        for (k = 0; k < states; k++) {
            del = (sap[k] - dadnap[k]) / dapsprd; /* 1st deriv from prior */
            del += pr[k] * cnt - scnt[k];         /* From data */
            del += 0.5 * pr[k];                   /*  Stabilization  */
            del += tt * (1.0 - states * pr[k]);   /* from roundoff */
            /*	2nd deriv approx cnt*pr[k], plus 1/dapsprd from prior  */
            sap[k] -= del * adj / (1.0 + scnt[k] + 1.0 / dapsprd);
        }
        sum = 0.0;
        for (k = 0; k < states; k++)
            sum += sap[k];
        sum = -sum / states;
        for (k = 0; k < states; k++)
            sap[k] += sum;
        /*	Compute sapsprd  */
        apd2 = (1.0 / dapsprd) + cnt * probs.ff;
        cls_var->sapsprd = statesm / apd2;
    /*	Repeat the adjustment  */}

    if (fac) {
        /*	Adjust factor parameters.  We have fapd1[], fbpd1[] from the data,
            but must add derivatives of pcost terms.  */
        for (k = 0; k < states; k++) {
            fapd1[k] += (fap[k] - dadnap[k]) / dapsprd;
            fbpd1[k] += fbp[k];
        }
        exp_var->apd2 += 1.0 / dapsprd;
        exp_var->bpd2 += 1.0;
        /*	Stabilization  */
        Scores.CaseFacScore = 0.0;
        set_probs(iv, &probs, cls);
        for (k = 0; k < states; k++)
            fapd1[k] += 0.5 * pr[k];
        exp_var->apd2 += 0.5 * states * probs.ff;
        /*	This section uses a slow but apparently safe adjustment of fa[[], fbp[]
         */
        /*	In an attempt to speed things, fiddle adjustment multiple   */
        sum = exp_var->ftcost / cnt;
        if (sum < exp_var->oldftcost)
            adj = exp_var->adj * 1.1;
        else
            adj = InitialAdj;
        if (adj > MaxAdj)
            adj = MaxAdj;
        exp_var->adj = adj;
        exp_var->oldftcost = sum;
        /*	To do the adjustments, divide 1st derivs by a 'max' value of 2nd
        derivs, held in vaux.  */
        adj = adj / (exp_var->cnt * vaux->mff);
        adj *= statesm / states;
        for (k = 0; k < states; k++) {
            fap[k] -= adj * fapd1[k];
            fbp[k] -= adj * fbpd1[k];
        }

        sum = 0.0;
        for (k = 0; k < states; k++)
            sum += fap[k];
        sum = -sum / states;
        for (k = 0; k < states; k++)
            fap[k] += sum;
        sum = 0.0;
        for (k = 0; k < states; k++)
            sum += fbp[k];
        sum = -sum / states;
        for (k = 0; k < states; k++)
            fbp[k] += sum;
        /*	Set fapsprd, bpsprd.   */
        cls_var->fapsprd = statesm / exp_var->apd2;
        cls_var->bpsprd = statesm / exp_var->bpd2;
    }

    /*	If no sons, set as-dad params from non-fac params  */
    if (cls->num_sons < 2) {
        for (k = 0; k < states; k++)
            nap[k] = sap[k];
        cls_var->napsprd = cls_var->sapsprd;
    }
    cls_var->samplesize = exp_var->cnt;
}

/*	------------------------  prprint  -------------------  */
/*	prints the exponential params in 'ap' converted to probs   */
void prprint(double *ap, int states, double *pr) {
    int k;
    double max, sum;

    max = ap[0];
    for (k = 0; k < states; k++) {
        if (ap[k] > max)
            max = ap[k];
    }
    sum = 0.0;
    for (k = 0; k < states; k++) {
        pr[k] = exp(ap[k] - max);
        sum += pr[k];
    }
    sum = 1.0 / sum;
    for (k = 0; k < states; k++)
        printf("%7.3f", pr[k] * sum);
    return;
}

/*	------------------------  show  -----------------------   */
void show(Class *cls, int iv) {
    int k;
    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];
    VSetVar *vset_var = &CurCtx.vset->variables[iv];
    Vaux *vaux = (Vaux *)vset_var->vaux;
    int states = vaux->states;
    double statesm = states - 1;
    double rstatesm = 1.0 / statesm;
    double *nap = &(cls_var->origin);
    double *sap = &nap[states];
    double *fap = &sap[states];
    double *fbp = &fap[states];
    double *frate = &fbp[states];
    double *scnt = &(exp_var->origin);
    double *scst = &scnt[states];
    double *pr = &scst[states];

    set_var(iv, cls);
    printf("V%3d  Cnt%6.1f  %s  Adj%8.2f\n", iv + 1, exp_var->cnt, (cls_var->infac) ? " In" : "Out", exp_var->adj);

    if (cls->num_sons > 1) {
        printf(" NR: ");
        prprint(nap, states, pr);
        printf(" +-%7.3f\n", sqrt(cls_var->napsprd));
    }
    printf(" PR: ");
    prprint(sap, states, pr);
    printf("\n");
    if (cls->use != Tiny) {
        printf(" FR: ");
        for (k = 0; k < states; k++) {
            printf("%7.3f", frate[k]);
        }
        printf("\n BP: ");
        for (k = 0; k < states; k++) {
            printf("%7.3f", fbp[k]);
        }
        printf(" +-%7.3f\n", sqrt(cls_var->bpsprd * rstatesm));
    }
}

/*	------------------------  details  -----------------------   */
/*	prints the exponential params in 'ap' converted to probs   */
void write_probs(double *ap, int states, double *pr, MemBuffer *buffer) {
    int k;
    double max, sum;

    max = ap[0];
    for (k = 0; k < states; k++) {
        if (ap[k] > max)
            max = ap[k];
    }
    sum = 0.0;
    for (k = 0; k < states; k++) {
        pr[k] = exp(ap[k] - max);
        sum += pr[k];
    }
    sum = 1.0 / sum;
    for (k = 0; k < states; k++) {
        if (k > 0) {
            print_buffer(buffer, ", ");
        }
        print_buffer(buffer, "%0.3f", pr[k] * sum);
    }
    return;
}

void details(Class *cls, int iv, MemBuffer *buffer) {
    int k;
    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];
    VSetVar *vset_var = &CurCtx.vset->variables[iv];
    Vaux *vaux = (Vaux *)vset_var->vaux;
    int states = vaux->states;
    double *nap = &(cls_var->origin);
    double *sap = &nap[states];
    double *fap = &sap[states];
    double *fbp = &fap[states];
    double *frate = &fbp[states];
    double *scnt = &(exp_var->origin);
    double *scst = &scnt[states];
    double *pr = &scst[states];

    set_var(iv, cls);
    print_buffer(buffer, "{\"index\": %d, \"name\": \"%s\", \"weight\": %0.1f, \"factor\": %s, ", iv + 1, vset_var->name, exp_var->cnt,
                 (cls_var->infac) ? "true" : "false");
    print_buffer(buffer, "\"type\": %d, ", vset_var->type);
    if (cls->num_sons > 1) {
        print_buffer(buffer, "\"frequency\": [");
        write_probs(nap, states, pr, buffer);
        print_buffer(buffer, "]");
    } else if (cls->use == Fac) {
        print_buffer(buffer, "\"frequency\": [");
        for (k = 0; k < states; k++) {
            if (k > 0) {
                print_buffer(buffer, ", ");
            }
            print_buffer(buffer, "%0.3f", frate[k]);
        }
        print_buffer(buffer, "], ");
        print_buffer(buffer, "\"influence\": [");
        for (k = 0; k < states; k++) {
            if (k > 0) {
                print_buffer(buffer, ", ");
            }
            print_buffer(buffer, "%0.3f", frate[k]);
        }
        print_buffer(buffer, "]");
    } else {
        print_buffer(buffer, "\"frequency\": [");
        write_probs(sap, states, pr, buffer);
        print_buffer(buffer, "]");
    }
    print_buffer(buffer, "}");
}

/*	----------------------  cost_var_nonleaf -----------------------------  */
void cost_var_nonleaf(int iv, int vald, Class *cls) {
    Basic *son_var;
    Class *son;
    double del, co0, co1, co2, tstvn, tssn;
    double pcost;
    double apsprd;
    int n, k, ison, nson, nints;
    Population *popln = CurCtx.popln;
    VSetVar *vset_var = &CurCtx.vset->variables[iv];
    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];
    Vaux *vaux = (Vaux *)vset_var->vaux;

    int states = vaux->states;
    double statesm = states - 1;
    double *nap = &(cls_var->origin);
    double *sap = &nap[states];
    double *scnt = &(exp_var->origin);
    double *scst = &scnt[states];
    double *pr = &scst[states];
    double qr[MaxState];

    set_var(iv, cls);
    if (vset_var->inactive) {
        exp_var->npcost = exp_var->ntcost = 0.0;
        return;
    }

    nson = cls->num_sons;
    if (nson < 2) { /* cannot define parameters */
        exp_var->npcost = exp_var->ntcost = 0.0;
        for (k = 0; k < states; k++)
            nap[k] = sap[k];
        cls_var->napsprd = 1.0;
        return;
    }
    /*      We need to accumlate things over sons. We need:   */
    nints = 0; /* Number of internal sons (M) */
    for (k = 0; k < states; k++)
        pr[k] = 0.0; /* Total of sons' bap[] vectors */
    tstvn = 0.0;     /* Total variance of sons' vectors */
    tssn = 0.0;      /* Total internal sons' bapsprd  */

    apsprd = cls_var->napsprd;
    /*	The calculation is like that in reals.c (q.v.) but now we have
    states-1 parameter components for each son. Thus, 'nson' in the reals case
    sometimes becomes (statesm * nson) in this case. We use the static vector
    'pr[]' to accumulate the sum of the sons' bap[]s, in place of the 'tstn' of
    the reals code. */
    for (ison = cls->son_id; ison > 0; ison = son->sib_id) {
        son = popln->classes[ison];
        son_var = (Basic *)son->basics[iv];
        for (k = 0; k < states; k++) {
            pr[k] += son_var->bap[k];
            tstvn += son_var->bap[k] * son_var->bap[k];
        }
        if (son->type == Dad) { /* used as parent */
            nints++;
            tssn += son_var->bapsprd;
            tstvn += son_var->bapsprd * statesm / son->num_sons;
        } else
            tstvn += son_var->bapsprd * statesm;
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
    /*	tstvn now gives total variance about sons' mean in (states-1) space */

    /*      Iterate the adjustment of param, spread  */
    for (n = 0; n < 5; n++) {
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
    }
    /*	Store new values  */
    cls_var->napsprd = apsprd;

    /*      Calc cost  */
    pcost = 0.0;
    for (k = 0; k < states; k++) {
        del = nap[k] - dadnap[k];
        pcost += del * del;
    }
    pcost = 0.5 * (pcost + statesm * apsprd / nson) / dapsprd + statesm * (HALF_LOG_2PI + 0.5 * log(dapsprd));
    /*      Add hlog Fisher, lattice  */
    pcost += 0.5 * log(0.5 * nson * statesm + nints) + 0.5 * statesm * log((double)nson) - 0.5 * (statesm + 2.0) * log(apsprd) + states * LATTICE;
    /*	Add roundoff for states params  */
    pcost += 0.5 * states;
    exp_var->npcost = pcost;
}
