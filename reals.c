/*    A prototypical file for a variable type      */
/*    actually for plain Gaussians   */

#define NOTGLOB 1
#include "glob.h"

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

typedef double Datum;

typedef struct Sauxst {
    int missing;
    double dummy;
    Datum xn;
    double eps;
    double leps;  /* Log eps */
    double epssq; /*  eps^2 / 12  */
} Saux;

typedef struct Pauxst {
    int dummy;
} Paux;

typedef struct Vauxst {
    int dummy;
} Vaux;

/*    ***************  All of Basic should be distributed  */
typedef struct Basicst { /* Basic parameter info about var in class.
            The first few fields are standard and must
            appear in the order shown.  */
    int id;              /* The variable number (index)  */
    int signif;
    int infac; /* shows if affected by factor  */
    /********************  Following fields vary according to need ***/
    double bmu, bsdl;         /* current mean, log sd */
    double nmu, nsdl;         /* when dad */
    double smu, ssdl;         /* when sansfac */
    double fmu, fsdl;         /* when confac */
    double bmusprd, bsdlsprd; /* squared spreads of params */
    double nmusprd, nsdlsprd;
    double smusprd, ssdlsprd;
    double fmusprd, fsdlsprd;
    double ld; /* factor load */
    double ldsprd;
    double samplesize; /* Size of sample on which estimates based */
} Basic;

typedef struct Statsst { /* Stuff accumulated to revise Basic  */
                         /* First fields are standard  */
    /*    These fields are accumulated, need not be distributed,
            must be returned  *********************   */
    double cnt; /* Weighted count  */
    double btcost, ntcost, stcost, ftcost;
    double bpcost, npcost, spcost, fpcost;
    double vsq; /*  weighted sum of squared scores for this var */
    int id;     /* Variable number  */
                /* ******* needs to be distributed once */
    /********************  Following fields vary according to need ***/
    double tx, txx; /* Sum of vals and squared vals (no factor) */
    double fsdld1, fmud1, ldd1;
    double fsdld2, fmud2, ldd2;
    /*    This ends the fields to accumulate and return****************** */
    /*    Following fields are computed by clear_stats(), not accumulated.
        They could be computed locally by a copy of clear_stats in each
        machine, or computed centrally and distributed  */
    /***************  some useful quanties derived from basics  */
    double ssig, srsds; /* No-fac sigma and 1/sigsq */
    double fsig, frsds; /* Factor sigma and 1/sigsq */
    double ldsq;
    /*    Following fields set and used locally for each case************** */
    double parkstcost, parkftcost; /* Unweighted item costs of xn */
    double var;
} Stats;

/*
static Saux *saux;
static Paux *paux;
static Vaux *vaux;
static Basic *cvi, *dcvi;
static Stats *stats;
 */

/*--------------------------  define ------------------------------- */
/*    This routine is used to set up a VarType entry in the global "types"
array.  It is the only function whose name needs to be altered for different
types of variable, and this name must be copied into the file "definetypes.c"
when installing a new type of variable. It is also necessary to change the
"Ntypes" constant, and to decide on a type id (an integer) for the new type.
    */

void reals_define(typindx) int typindx;
/*    typindx is the index in types[] of this type   */
{
    VarType *vtp;

    vtp = Types + typindx;
    vtp->id = typindx;
    /*     Set type name as string up to 59 chars  */
    vtp->name = "Standard Gaussian";
    vtp->data_size = sizeof(Datum);
    vtp->attr_aux_size = sizeof(Vaux);
    vtp->pop_aux_size = sizeof(Paux);
    vtp->smpl_aux_size = sizeof(Saux);
    vtp->read_aux_attr = &read_aux_attr;
    vtp->read_aux_smpl = &read_aux_smpl;
    vtp->read_datum = &read_datum;
    vtp->print_datum = &print_datum;
    vtp->set_sizes = &set_sizes;
    vtp->set_best_pars = &set_best_pars;
    vtp->clear_stats = &clear_stats;
    vtp->score_var = &score_var;
    vtp->cost_var = &cost_var;
    vtp->deriv_var = &deriv_var;
    vtp->cost_var_nonleaf = &cost_var_nonleaf;
    vtp->adjust = &adjust;
    vtp->show = &show;
    vtp->set_var = &set_var;
}

/*    -------------------  set_var -----------------------------  */
void set_var(int iv) {}

/*    --------------------  read_aux_attr  ----------------------------  */
/*      Read in auxiliary info into vaux, return 0 if OK else 1  */
int read_aux_attr(VSetVar *vset_var) { return (0); }

/*    ---------------------  read_aux_smpl ---------------------------   */
/*    To read any auxiliary info about a variable of this type in some
sample.
    */
int read_aux_smpl(Saux *sax) {
    int i;

    /*    Read in auxiliary info into saux, return 0 if OK else 1  */
    i = read_double(&(sax->eps), 1);
    if (i < 0) {
        sax->eps = sax->leps = 0.0;
        return (1);
    }
    sax->epssq = sax->eps * sax->eps * (1.0 / 12.0);
    sax->leps = log(sax->eps);
    return (0);
}

/*    -------------------  read_datum -------------------------------  */
/*    To read a value for this variable type     */
int read_datum(char *loc, int iv) {
    int i;
    Datum xn;

    /*    Read datum into xn, return error.  */
    i = read_double(&xn, 1);
    if (!i)
        memcpy(loc, &xn, sizeof(Datum));
    return (i);
}

/*    ---------------------  print_datum --------------------------  */
/*    To print a Datum value   */
void print_datum(char *loc) {
    /*    Print datum from address loc   */
    printf("%9.2f", *((double *)loc));
    return;
}

/*    ---------------------  set_sizes  -----------------------   */
void set_sizes(int iv) {
    VSetVar *vset_var;
    vset_var = CurCtx.vset->variables + iv;
    vset_var->basic_size = sizeof(Basic);
    vset_var->stats_size = sizeof(Stats);
}

/*    ----------------------  set_best_pars --------------------------  */
void set_best_pars(int iv, Class *cls) {
    Basic *cvi = (Basic *)cls->basics[iv];
    Stats *stats = (Stats *)cls->stats[iv];

    if (cls->type == Dad) {
        cvi->bmu = cvi->nmu;
        cvi->bmusprd = cvi->nmusprd;
        cvi->bsdl = cvi->nsdl;
        cvi->bsdlsprd = cvi->nsdlsprd;
        stats->btcost = stats->ntcost;
        stats->bpcost = stats->npcost;
    } else if ((cls->use == Fac) && cvi->infac) {
        cvi->bmu = cvi->fmu;
        cvi->bmusprd = cvi->fmusprd;
        cvi->bsdl = cvi->fsdl;
        cvi->bsdlsprd = cvi->fsdlsprd;
        stats->btcost = stats->ftcost;
        stats->bpcost = stats->fpcost;
    } else {
        cvi->bmu = cvi->smu;
        cvi->bmusprd = cvi->smusprd;
        cvi->bsdl = cvi->ssdl;
        cvi->bsdlsprd = cvi->ssdlsprd;
        stats->btcost = stats->stcost;
        stats->bpcost = stats->spcost;
    }
    return;
}

/*    ------------------------  clear_stats  ------------------------  */
/*    Clears stats to accumulate in cost_var, and derives useful functions
of basic params  */
void clear_stats(int iv, Class* cls) {
    double tmp;

    Basic *cvi = (Basic *)cls->basics[iv];
    Stats *stats = (Stats *)cls->stats[iv];

    stats->cnt = 0.0;
    stats->stcost = stats->ftcost = 0.0;
    stats->vsq = 0.0;
    stats->tx = stats->txx = 0.0;
    stats->fsdld1 = stats->fmud1 = stats->ldd1 = 0.0;
    stats->fsdld2 = stats->fmud2 = stats->ldd2 = 0.0;

    if (cls->age == 0)
        return;
    stats->ssig = exp(cvi->ssdl);
    tmp = 1.0 / stats->ssig;
    stats->srsds = tmp * tmp;
    stats->fsig = exp(cvi->fsdl);
    tmp = 1.0 / stats->fsig;
    stats->frsds = tmp * tmp;
    stats->ldsq = cvi->ld * cvi->ld;
    return;
}

/*    -------------------------  score_var  ------------------------   */
/*    To eval derivs of a case cost wrt score, scorespread. Adds to
Scores.CaseFacScoreD1, CaseFacScoreD2.
    */
void score_var(int iv, Class *cls) {

    double del, md2;

    SampleVar *smpl_var = CurCtx.sample->variables + iv;
    VSetVar *vset_var = CurCtx.vset->variables + iv;
    Saux *saux = (Saux *)smpl_var->saux;
    Basic *cvi = (Basic *)cls->basics[iv];
    Stats *stats = (Stats *)cls->stats[iv];

    vset_var = CurCtx.vset->variables + iv;
    if ((vset_var->inactive) || (saux->missing))
        return;
    del = cvi->fmu + Scores.CurCaseFacScore * cvi->ld - saux->xn;
    Scores.CaseFacScoreD1 += stats->frsds * (del * cvi->ld + Scores.CurCaseFacScore * cvi->ldsprd);
    md2 = stats->frsds * (stats->ldsq + cvi->ldsprd);
    Scores.CaseFacScoreD2 += md2;
    Scores.EstFacScoreD2 += 1.1 * md2;
}

/*    -----------------------  cost_var  --------------------------   */
/*    Accumulates item cost into scasecost, Scores.fcasecost    */
void cost_var(int iv, int fac, Class *cls) {
    double del, var, cost;

    SampleVar *smpl_var = CurCtx.sample->variables + iv;
    Saux *saux = (Saux *)smpl_var->saux;
    Basic *cvi = (Basic *)cls->basics[iv];
    Stats *stats = (Stats *)cls->stats[iv];

    if (saux->missing)
        return;
    if (cls->age == 0) {
        stats->parkftcost = stats->parkstcost = 0.0;
        return;
    }
    /*    Do no-fac cost first  */
    del = cvi->smu - saux->xn;
    var = del * del + cvi->smusprd + saux->epssq;
    cost = 0.5 * var * stats->srsds + cvi->ssdlsprd + HALF_LOG_2PI + cvi->ssdl - saux->leps;
    stats->parkstcost = cost;
    Scores.scasecost += cost;

    /*    Only do faccost if fac  */
    if (fac) {
        del += Scores.CurCaseFacScore * cvi->ld;
        var = del * del + cvi->fmusprd + saux->epssq + Scores.CurCaseFacScoreSq * cvi->ldsprd + Scores.cvvsprd * stats->ldsq;
        stats->var = var;
        cost = HALF_LOG_2PI + 0.5 * stats->frsds * var + cvi->fsdl + cvi->fsdlsprd * 2.0 - saux->leps;
    }
    Scores.fcasecost += cost;
    stats->parkftcost = cost;
}

/* -------------------- deriv_var -------------------------- */
/* Given the item weight in cwt, calculates derivatives of cost wrt basic
params and accumulates in paramd1, paramd2.
Factor derivatives done only if fac. */
void deriv_var(int iv, int fac, Class *cls) {
    double del, var, frsds;

    SampleVar *smpl_var = CurCtx.sample->variables + iv;
    Saux *saux = (Saux *)smpl_var->saux;
    Basic *cvi = (Basic *)cls->basics[iv];
    Stats *stats = (Stats *)cls->stats[iv];

    if (saux->missing)
        return;

    // Common operations for both fac and non-fac cases
    double curCaseWeightTimesXn = cls->case_weight * saux->xn;
    stats->cnt += cls->case_weight;
    stats->tx += curCaseWeightTimesXn;
    stats->txx += cls->case_weight * (saux->xn * saux->xn + saux->epssq);
    stats->stcost += cls->case_weight * stats->parkstcost;
    stats->ftcost += cls->case_weight * stats->parkftcost;

    // Processing for factor form
    if (fac) {
        frsds = stats->frsds;
        del = cvi->fmu + Scores.CurCaseFacScore * cvi->ld - saux->xn;
        var = stats->var;

        // Pre-calculate common term
        double curCaseWeightTimesFrsds = cls->case_weight * frsds;

        // Accumulate derivatives
        stats->fsdld1 += cls->case_weight - curCaseWeightTimesFrsds * var;
        stats->fsdld2 += 2.0 * cls->case_weight;
        stats->fmud1 += curCaseWeightTimesFrsds * del;
        stats->fmud2 += curCaseWeightTimesFrsds;
        stats->ldd1 += curCaseWeightTimesFrsds * (del * Scores.CurCaseFacScore + cvi->ld * Scores.cvvsprd);
        stats->ldd2 += curCaseWeightTimesFrsds * (Scores.CurCaseFacScoreSq + Scores.cvvsprd);
    }
}

/*    -------------------  adjust  ---------------------------    */
/*    To adjust parameters of a real variable     */
void adjust(int iv, int fac, Class* cls) {
    double adj, srsds, frsds, temp1, temp2, cnt;
    double del1, del2, del3, del4, spcost, fpcost;
    double dadmu, dadsdl, dmusprd, dsdlsprd;
    double av, var, del, sdld1;

    Class *dad = (cls->dad_id >= 0) ? CurCtx.popln->classes[cls->dad_id] : 0;
    SampleVar *smpl_var = CurCtx.sample->variables + iv;
    Saux *saux = (Saux *)smpl_var->saux;
    Basic *cvi = (Basic *)cls->basics[iv];
    Stats *stats = (Stats *)cls->stats[iv];
    Basic *dcvi = (dad) ? (Basic *)dad->basics[iv] : 0;

    del3 = del4 = 0.0;
    adj = InitialAdj;
    cnt = stats->cnt;

    /*    Get prior constants from dad, or if root, fake them  */
    if (!dad) { /* Class is root */
        if (cls->age > 0) {
            dadmu = cvi->smu;
            dadsdl = cvi->ssdl;
            dmusprd = exp(2.0 * dadsdl);
            dsdlsprd = 1.0;
        } else {
            dadmu = dadsdl = 0.0;
            dmusprd = dsdlsprd = 1.0;
        }
    } else {
        dadmu = dcvi->nmu;
        dadsdl = dcvi->nsdl;
        dmusprd = dcvi->nmusprd;
        dsdlsprd = dcvi->nsdlsprd;
    }

    /*    If too few data for this variable, use dad's n-paras  */
    if ((Control & AdjPr) && (cnt < MinSize)) {
        cvi->nmu = cvi->smu = cvi->fmu = dadmu;
        cvi->nsdl = cvi->ssdl = cvi->fsdl = dadsdl;
        cvi->ld = 0.0;
        cvi->nmusprd = cvi->smusprd = cvi->fmusprd = dmusprd;
        cvi->nsdlsprd = cvi->ssdlsprd = cvi->fsdlsprd = dsdlsprd;
        cvi->ldsprd = 1.0;
        goto hasage;
    }
    /*    If class age is zero, make some preliminary estimates  */
    if (cls->age)
        goto hasage;
    cvi->smu = cvi->fmu = stats->tx / cnt;
    var = stats->txx / cnt - cvi->smu * cvi->smu;
    stats->ssig = stats->fsig = sqrt(var);
    cvi->ssdl = cvi->fsdl = log(stats->ssig);
    stats->frsds = stats->srsds = 1.0 / var;
    cvi->smusprd = cvi->fmusprd = var / cnt;
    cvi->ldsprd = var / cnt;
    cvi->ld = 0.0;
    cvi->ssdlsprd = cvi->fsdlsprd = 1.0 / cnt;
    if (!dad) { /*  First stab at root  */
        dadmu = cvi->smu;
        dadsdl = cvi->ssdl;
        dmusprd = exp(2.0 * dadsdl);
        dsdlsprd = 1.0;
    }
    /*    Make a stab at class tcost  */
    cls->cstcost += cnt * (HALF_LOG_2PI + cvi->ssdl - saux->leps + 0.5 + cls->mlogab) + 1.0;
    cls->cftcost = cls->cstcost + 100.0 * cnt;

hasage:
    temp1 = 1.0 / dmusprd;
    temp2 = 1.0 / dsdlsprd;
    srsds = stats->srsds;
    frsds = stats->frsds;

    /*    Compute parameter costs as they are  */
    del1 = dadmu - cvi->smu;
    spcost = HALF_LOG_2PI + 0.5 * (log(dmusprd) + temp1 * (del1 * del1 + cvi->smusprd));
    del2 = dadsdl - cvi->ssdl;
    spcost += HALF_LOG_2PI + 0.5 * (log(dsdlsprd) + temp2 * (del2 * del2 + cvi->ssdlsprd));
    spcost -= 0.5 * log(cvi->smusprd * cvi->ssdlsprd);
    spcost += 2.0 * LATTICE;

    if (!fac) {
        fpcost = spcost + 100.0;
        cvi->infac = 1;
        goto facdone1;
    }
    del3 = cvi->fmu - dadmu;
    fpcost = HALF_LOG_2PI + 0.5 * (log(dmusprd) + temp1 * (del3 * del3 + cvi->fmusprd));
    del4 = cvi->fsdl - dadsdl;
    fpcost += HALF_LOG_2PI + 0.5 * (log(dsdlsprd) + temp2 * (del4 * del4 + cvi->fsdlsprd));
    /*    The prior for load ld id N (0, sigsq)  */
    fpcost += HALF_LOG_2PI + 0.5 * (stats->ldsq + cvi->ldsprd) * frsds + cvi->fsdl;
    fpcost -= 0.5 * log(cvi->fmusprd * cvi->fsdlsprd * cvi->ldsprd);
    fpcost += 3.0 * LATTICE;

facdone1:

    /*    Store param costs for this variable  */
    stats->spcost = spcost;
    stats->fpcost = fpcost;
    /*    Add to class param costs  */
    cls->nofac_par_cost += spcost;
    cls->fac_par_cost += fpcost;
    if (cnt < MinSize)
        goto adjdone;
    if (!(Control & AdjPr))
        goto tweaks;
    /*    Adjust non-fac parameters.   */
    /*    From things, smud1 = (tx - cnt*mu) * srsds, from prior,
        smud1 = (dadmu - mu) / dmusprd.
        From things, smud2 = cnt * srsds, from prior = 1/dmusprd.
        Explicit optimum:  */
    cvi->smusprd = 1.0 / (cnt * srsds + temp1);
    cvi->smu = (stats->tx * srsds + dadmu * temp1) * cvi->smusprd;
    /*    Calculate variance about new mean, adding variance from musprd   */
    av = stats->tx / cnt;
    del = cvi->smu - av;
    var = stats->txx + cnt * (del * del - av * av + cvi->smusprd);
    /*    The deriv sdld1 from data is (cnt - rsds * var)  */
    sdld1 = cnt - srsds * var;
    /*    The deriv from prior is (sdl-dadsdl) / dsdlsprd  */
    sdld1 += (cvi->ssdl - dadsdl) * temp2;
    /*    The 2nd deriv sdld2 from data is just cnt  */
    /*    From prior, get 1/dsdlsprd  */
    cvi->ssdlsprd = 1.0 / (cnt + temp2);
    /*    sdlsprd is just 1 / sdld2  */
    /*    Adjust, but not too much  */
    del = sdld1 * cvi->ssdlsprd;
    if (del > 0.2)
        del = 0.2;
    else if (del < -0.2)
        del = -0.2;
    cvi->ssdl -= del;
    stats->ssig = exp(cvi->ssdl);
    stats->srsds = 1.0 / (stats->ssig * stats->ssig);
    if (!fac)
        goto facdone2;

    /*    Now for factor adjust.
        We have derivatives fmud1, fsdld1, ldd1 etc in stats. Add terms
        from priors
        */
    stats->fmud1 += del3 * temp1;
    stats->fmud2 += temp1;
    stats->fsdld1 += del4 * temp2;
    stats->fsdld2 += temp2;
    stats->ldd1 += cvi->ld * frsds;
    stats->fsdld2 += frsds;
    /*    The dependence of the load prior on frsds, and thus on fsdl,
        will give additional terms to sdld1, sdld2.
        */
    /*    The additional terms from the load prior :  */
    stats->fsdld1 += 1.0 - frsds * (stats->ldsq + cvi->ldsprd);
    stats->fsdld2 += 1.0;
    /*    Adjust sdl, but not too much.  */
    cvi->fsdlsprd = 1.0 / stats->fsdld2;
    del = stats->fsdld1 * cvi->fsdlsprd;
    if (del > 0.2)
        del = 0.2;
    else if (del < -0.2)
        del = -0.2;
    cvi->fsdl -= adj * del;
    cvi->fmusprd = 1.0 / stats->fmud2;
    cvi->fmu -= adj * stats->fmud1 * cvi->fmusprd;
    cvi->ldsprd = 1.0 / stats->ldd2;
    cvi->ld -= adj * stats->ldd1 * cvi->ldsprd;
    stats->fsig = exp(cvi->fsdl);
    stats->frsds = 1.0 / (stats->fsig * stats->fsig);
    stats->ldsq = cvi->ld * cvi->ld;

facdone2:
    cvi->samplesize = stats->cnt;
    goto adjdone;

tweaks: /* Come here if no adjustments made */
    /*    Deal only with sub-less leaves  */
    if ((cls->type != Leaf) || (cls->num_sons < 2))
        goto adjdone;

    /*    If Noprior, guess no-prior params, sprds and store instead of
        as-dad values. Actually store in nparsprd the val 1/bparsprd,
        and in npar the val bpar / bparsprd   */
    if (Control & Noprior) {
        cvi->nmusprd = (1.0 / cvi->bmusprd) - (1.0 / dmusprd);
        cvi->nmu = cvi->bmu / cvi->bmusprd - dadmu / dmusprd;
        cvi->nsdlsprd = (1.0 / cvi->bsdlsprd) - (1.0 / dsdlsprd);
        cvi->nsdl = cvi->bsdl / cvi->bsdlsprd - dadsdl / dsdlsprd;
    } else if (Control & Tweak) {
        /*    Adjust "best" params by applying effect of dad prior to
            the no-prior values  */
        cvi->bmusprd = 1.0 / (cvi->nmusprd + (1.0 / dmusprd));
        cvi->bmu = (cvi->nmu + dadmu / dmusprd) * cvi->bmusprd;
        cvi->bsdlsprd = 1.0 / (cvi->nsdlsprd + (1.0 / dsdlsprd));
        cvi->bsdl = (cvi->nsdl + dadsdl / dsdlsprd) * cvi->bsdlsprd;
        if (cls->use == Fac) {
            cvi->fmu = cvi->bmu;
            cvi->fmusprd = cvi->nmusprd;
        } else {
            cvi->smu = cvi->bmu;
            cvi->smusprd = cvi->nmusprd;
        }
    } else {
        /*    Copy non-fac params to as-dad   */
        cvi->nmu = cvi->smu;
        cvi->nmusprd = cvi->smusprd;
        cvi->nsdl = cvi->ssdl;
        cvi->nsdlsprd = cvi->ssdlsprd;
    }

adjdone:
    return;
}

/*    ------------------------  show  -----------------------   */
void show(Class *cls, int iv) {

    Basic *cvi = (Basic *)cls->basics[iv];
    Stats *stats = (Stats *)cls->stats[iv];
    VSetVar *vset_var = CurCtx.vset->variables + iv;
    
    printf("V%3d  Cnt%6.1f  %s               Type: %s\n", iv + 1, stats->cnt, (cvi->infac) ? " In" : "Out", vset_var->vtype->name);
    if (cls->num_sons > 1) {
        printf(" N: Cost%8.1f  Mu%8.3f+-%8.3f  SD%8.3f+-%8.3f\n", stats->npcost, cvi->nmu, sqrt(cvi->nmusprd), exp(cvi->nsdl),
               exp(cvi->nsdl) * sqrt(cvi->nsdlsprd));
    }
    printf(" S: Cost%8.1f  Mu%8.3f  SD%8.3f\n", stats->spcost + stats->stcost, cvi->smu, exp(cvi->ssdl));
    printf(" F: Cost%8.1f  Mu%8.3f  SD%8.3f  Ld%8.3f\n", stats->fpcost + stats->ftcost, cvi->fmu, exp(cvi->fsdl), cvi->ld);
}

/*    ----------------------  cost_var_nonleaf  ------------------------   */
/*    To compute parameter cost for non-leaf (intrnl) class use   */

/*    The coding in an internal class of a simple scalar parameter such as
mu or sdl is based on Normal priors. Let the parameter be "t". Every class
will hold an estimate for t, and an assocoated "spread" value "s". In a
leaf use of a class, "s" shows the expected squared roundoff in the estimate
't'. In an internal use, 's' shows a measure of the variance among the 't'
parameters of the class's sons (or subclasses). 's' also is a measure of the
average value of the 's' values of the sons.
    A class will normally carry 3 versions of each of t and s.
'ft', 'fs' apply when the class is used as a leaf or sub class best described
with a factor.
    'st', 'ss' apply when the class is uses as a leaf- or sub-class best
described without a factor.
    'nt', 'ns' apply when the class is used as the parent of N > 1 'sons'
or 2 subclasses. It is these which we are concerned with here.

    We drop the prefix letter and use just 't', 's' as the values for the
internal class. We use 'tp', 'sp' for the nt and ns values of its parent.
We use 'tn' for the t-value of the n-th son. In fact, 'tn' may be the
ft, st or nt value of the n-th son, depending on the current use of the son
class (internal or not, with or without factor).

We use 'sn' for the spread value of the n-th son. Again, it may be any of
the fs, ss or ns values of that son.

    An internal class is used only to specify the priors for its sons,
and should appear in a classification if and only if there is a group of
leaf classes with sufficiently similar t-estimates that it is worth the
cost of an internal class to specify a 'tight' prior over the t-s.

    Let our internal class, with estimate t and spread s, have N sons,
of which M are internal. The t and s specify a prior density over {tn: n=1..N}:

Pr(tn) = 1/sqrt(2Pi * s) * exp (-0.5 * ((tn-t)^2 + deln) / s)

    which is basically Normal(t,s) but which is modified to allow for
the expected squared roundoff in tn, which we call deln.

    's' also specifies a prior density over the sn values of the M
internal sons. This is taken to have negative-exponential form:

Pr(sn) = (1/s) * exp (-sn / s).

    Note that the sn values for leaf- and sub-class sons is not coded
using a prior specified in the internal parent, since these sn are just the
reciprocals of the Fishers for the corresponding tn, and their values are
implied by the tn, or at least by the set of estimate values in the son.


    With the above uses of t and s, the total cost of the densities of
{tn, sn} is

For {tn} :
    N*HALF_LOG_2PI + 0.5*N*log(s) + (1/2s)*Sum_n{(tn-t)^2 + deln}
For {sm} :
    M*log(s) + (1/s)*Sum_m{sm}    where we index the internal sons by m
                    rather than by n.

Calling the sum of these L, the expected second differential of L wrt t
is
    F_t = N/s     and the expected second differential of L wrt s
is
    F_s = (N/2 + M) / s^2.     The overall fisher is n((N/2 + M) / s^3.

    A simplifying note:  if son n is internal, then deln = sn/n_n where
n_n is the number of sons of son n.

    The joint prior density of (t,s) is

1/sqrt (2Pi * sp) * exp (-0.5*((t-tp)^2) + del) / sp * (1/sp) * exp (-s/sp)

where tp, sp are dad's values and del = squared roundoff in t = s/N.

    The message length terms involving t, s are thus:

From Pr({tn,sm}) :
    (N/2 + M) log(s) + 0.5 * V / s  +  S / s
   where V = Sum_n {(tn-t)^2 + deln}   and S = Sum_m {sm}

From prior(t,s) :
    (1/2) log(sp) + (1/2*sp)*((t-tp)^2+del) + log(sp) + s/sp
        with del = s/N

From (1/2) log (F_t*F_s):
    -(3/2) log (s)

    Differentiating wrt t gives an explicit optimum

    t = (sp*Sum_n{tn} + s * tp) / (N*sp + s)     !!!!!!!!!!!!

    Differentiating wrt t gives the equation for optimum s:

    (1 + 0.5/N)/sp + (N/2 + M - 3/2)/s - (V/2 + S)/s^2  = 0
   or    s^2*(1 + 0.5/N)/sp  +  s*(N/2 + M - 3/2)  -  (V/2 + S) = 0
        which is quadratic in s.

Writing the quadratic as    a*s^2 + b*s -c = 0,   we want the root

    s = 2*c / { b + sqrt (b^2 + 4ac) }.  This is the form used in code.

    */

void cost_var_nonleaf(int iv, int vald, Class* cls) {
    Basic *soncvi;
    Class *son;
    double pcost, tcost; /* Item, param and total item costs */
    double pp, ppsprd, dadpp, dppsprd, del;
    double co0, co1, co2, mean;
    double tssn, tstn, tstvn;
    double spp, sppsprd;
    int nints, nson, ison, k, n;

    Class *dad = (cls->dad_id >= 0) ? CurCtx.popln->classes[cls->dad_id] : 0;
    VSetVar *vset_var = CurCtx.vset->variables + iv;
    Basic *cvi = (Basic *)cls->basics[iv];
    Stats *stats = (Stats *)cls->stats[iv];
    Basic *dcvi = (dad) ? (Basic *)dad->basics[iv] : 0;

    vset_var = CurCtx.vset->variables + iv;
    if (!vald) { /* Cannot define as-dad params, so fake it */
        stats->npcost = 0.0;
        cvi->nmu = cvi->smu;
        cvi->nmusprd = cvi->smusprd * stats->cnt;
        cvi->nsdl = cvi->ssdl;
        cvi->nsdlsprd = cvi->ssdlsprd * stats->cnt;
        return;
    }
    if (vset_var->inactive) {
        stats->npcost = stats->ntcost = 0.0;
        return;
    }
    nson = cls->num_sons;
    pcost = tcost = 0.0;

    /*    There are two independent parameters, nmu and nsdl, to fiddle.
        As the info for these is stored in consecutive Sf words in Basic,
    we can do the two params in a two-count loop. We use p... to mean mu... and
    sdl... in the two times round the loop  */

    k = 0;
ploop:
    /*    Get prior constants from dad, or if root, fake them  */
    if (!dad) { /* Class is root */
        dadpp = *(&cvi->smu + k);
        dppsprd = *(&cvi->smusprd + k) * stats->cnt;
    } else {
        dadpp = *(&dcvi->nmu + k);
        dppsprd = *(&dcvi->nmusprd + k);
    }
    pp = *(&cvi->nmu + k);
    ppsprd = *(&cvi->nmusprd + k);

    /*    We need to accumlate things over sons. We need:   */
    nints = 0;   /* Number of internal sons (M) */
    tstn = 0.0;  /* Total sons' t_n */
    tstvn = 0.0; /* Total sum of sons' (t_n^2 + del_n)  */
    tssn = 0.0;  /* Total sons' s_n */

    for (ison = cls->son_id; ison > 0; ison = son->sib_id) {
        son = CurCtx.popln->classes[ison];
        soncvi = (Basic *)son->basics[iv];
        spp = *(&soncvi->bmu + k);
        sppsprd = *(&soncvi->bmusprd + k);
        tstn += spp;
        tstvn += spp * spp;
        if (son->type == Dad) { /* used as parent */
            nints++;
            tssn += sppsprd;
            tstvn += sppsprd / son->num_sons;
        } else
            tstvn += sppsprd;
    }
    /*    Calc coeffs for quadratic c_2 * s^2 + c_1 * s - c_0  */
    co2 = (1.0 + 0.5 / nson) / dppsprd;
    co1 = nints + 0.5 * (nson - 3.0);
    /*    Can now compute V in the above comments.  tssn = S of the comments */
    /*    First we get the V around the sons' mean, then update pp and correct
        the value of V  */
    mean = tstn / nson;   /* Mean of sons' param  */
    tstvn -= mean * tstn; /*    Variance around mean  */
    if (!(Control & AdjPr))
        goto adjdone;

    /*    Iterate the adjustment of param, spread  */
    n = 5;
adjloop:
    /*    Update param  */
    pp = (dppsprd * tstn + ppsprd * dadpp) / (nson * dppsprd + ppsprd);
    del = pp - mean;
    /*    The V of comments is tstvn + nson * del * del */
    co0 = 0.5 * (tstvn + nson * del * del) + tssn;
    /*    Solve for new spread  */
    ppsprd = 2.0 * co0 / (co1 + sqrt(co1 * co1 + 4.0 * co0 * co2));
    n--;
    if (n)
        goto adjloop;
    *(&cvi->nmu + k) = pp;
    *(&cvi->nmusprd + k) = ppsprd;

adjdone: /*    Calc cost  */
    del = pp - dadpp;
    pcost += HALF_LOG_2PI + 1.5 * log(dppsprd) + 0.5 * (del * del + ppsprd / nson) / dppsprd + ppsprd / dppsprd;
    /*    Add hlog Fisher, lattice  */
    pcost += 0.5 * log(nson * (0.5 * nson + nints)) - 1.5 * log(ppsprd) + 2.0 * LATTICE;

    /*    Add roundoff for 2 params  (pp, ppsprd)  */
    pcost += 1.0;
    /*    This completes the shenanigans for one param. See if another */
    k++;
    if (k < 2)
        goto ploop;

    stats->npcost = pcost;

    return;
}
