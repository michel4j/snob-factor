/*	A prototypical file for a variable type      */
/*	actually for plain Gaussians   */

#include "glob.h"

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

/*	***************  All of Basic should be distributed  */
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
    /*	These fields are accumulated, need not be distributed,
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
    /*	This ends the fields to accumulate and return****************** */
    /*	Following fields are computed by clear_stats(), not accumulated.
        They could be computed locally by a copy of clear_stats in each
        machine, or computed centrally and distributed  */
    /***************  some useful quanties derived from basics  */
    double ssig, srsds; /* No-fac sigma and 1/sigsq */
    double fsig, frsds; /* Factor sigma and 1/sigsq */
    double ldsq;
    /*	Following fields set and used locally for each case************** */
    double parkstcost, parkftcost; /* Unweighted item costs of xn */
    double var;
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
types of variable, and this name must be copied into the file "definetypes.c"
when installing a new type of variable. It is also necessary to change the
"Ntypes" constant, and to decide on a type id (an integer) for the new type.
    */

void reals_define(typindx) int typindx;
/*	typindx is the index in types[] of this type   */
{
    VarType *vtype;

    vtype = &Types[typindx];
    vtype->id = typindx;
    /* 	Set type name as string up to 59 chars  */
    vtype->name = "Standard Gaussian";
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

/*	-------------------  setvar -----------------------------  */
void set_var(int iv, Class *cls) {
    /*
        Basic *cls_var = (Basic *)cls->basics[iv];
        Stats *exp_var = (Stats *)cls->stats[iv];
        Class *dad = (cls->dad_id >= 0) ? CurCtx.popln->classes[cls->dad_id] : 0;
        Basic *dad_var = (dad) ? (Basic *)dad->basics[iv] : 0;
    */
}

/*	--------------------  readvaux  ----------------------------  */
/*      Read in auxiliary info into vaux, return 0 if OK else 1  */
int read_attr_aux(void *vax) { return (0); }
int set_attr_aux(void *vax, int aux) { return (0); }

/*	---------------------  readsaux ---------------------------   */
/*	To read any auxiliary info about a variable of this type in some
sample.
*/
int read_smpl_aux(void *saux) {
    int i;
    double prec;
    /*	Read in auxiliary info into saux, return 0 if OK else 1  */
    i = read_double(&prec, 1);
    if (i < 0) {
        set_smpl_aux(saux, 0, 0.0);
        return (1);
    }
    set_smpl_aux(saux, 0, prec);
    return (0);
}
int set_smpl_aux(void *saux, int unit, double prec) {
    Saux *sax = (Saux *)saux;
    sax->eps = prec;
    sax->epssq = sax->eps * sax->eps * (1.0 / 12.0);
    if (sax->eps > 0)
        sax->leps = log(sax->eps);
    return (0);
}

/*	-------------------  readdat -------------------------------  */
/*	To read a value for this variable type	 */
int read_datum(char *loc, int iv) {
    int i;
    Datum xn;

    /*	Read datum into xn, return error.  */
    i = read_double(&xn, 1);
    if (!i)
        set_datum(loc, iv, &xn);

    return (i);
}
int set_datum(char *loc, int iv, void *value) {
    double val = *(double *)(value);
    int active = (isnan(val)) ? -1 : 1;
    memcpy(loc, value, sizeof(double));
    return active * sizeof(double);
}

/*	---------------------  print_datum --------------------------  */
/*	To print a Datum value   */
void print_datum(char *loc) {
    /*	Print datum from address loc   */
    printf("%9.3g ", *((double *)(loc)));
}

/*	---------------------  setsizes  -----------------------   */
void set_sizes(int iv) {
    VSetVar *vset_var = &CurCtx.vset->variables[iv];
    vset_var->basic_size = sizeof(Basic);
    vset_var->stats_size = sizeof(Stats);
}

/*	----------------------  set_best_pars --------------------------  */
void set_best_pars(int iv, Class *cls) {
    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];

    set_var(iv, cls);

    if (cls->type == Dad) {
        cls_var->bmu = cls_var->nmu;
        cls_var->bmusprd = cls_var->nmusprd;
        cls_var->bsdl = cls_var->nsdl;
        cls_var->bsdlsprd = cls_var->nsdlsprd;
        exp_var->btcost = exp_var->ntcost;
        exp_var->bpcost = exp_var->npcost;
    } else if ((cls->use == Fac) && cls_var->infac) {
        cls_var->bmu = cls_var->fmu;
        cls_var->bmusprd = cls_var->fmusprd;
        cls_var->bsdl = cls_var->fsdl;
        cls_var->bsdlsprd = cls_var->fsdlsprd;
        exp_var->btcost = exp_var->ftcost;
        exp_var->bpcost = exp_var->fpcost;
    } else {
        cls_var->bmu = cls_var->smu;
        cls_var->bmusprd = cls_var->smusprd;
        cls_var->bsdl = cls_var->ssdl;
        cls_var->bsdlsprd = cls_var->ssdlsprd;
        exp_var->btcost = exp_var->stcost;
        exp_var->bpcost = exp_var->spcost;
    }
}

/*	------------------------  clear_stats  ------------------------  */
/*	Clears stats to accumulate in cost_var, and derives useful functions
of basic params  */
void clear_stats(int iv, Class *cls) {
    double tmp;

    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];

    set_var(iv, cls);
    exp_var->cnt = 0.0;
    exp_var->stcost = exp_var->ftcost = 0.0;
    exp_var->vsq = 0.0;
    exp_var->tx = exp_var->txx = 0.0;
    exp_var->fsdld1 = exp_var->fmud1 = exp_var->ldd1 = 0.0;
    exp_var->fsdld2 = exp_var->fmud2 = exp_var->ldd2 = 0.0;

    if (cls->age == 0)
        return;
    exp_var->ssig = exp(cls_var->ssdl);
    tmp = 1.0 / exp_var->ssig;
    exp_var->srsds = tmp * tmp;
    exp_var->fsig = exp(cls_var->fsdl);
    tmp = 1.0 / exp_var->fsig;
    exp_var->frsds = tmp * tmp;
    exp_var->ldsq = cls_var->ld * cls_var->ld;
}

/*	-------------------------  score_var  ------------------------   */
/*	To eval derivs of a case cost wrt score, scorespread. Adds to
vvd1, vvd2.
*/
void score_var(int iv, Class *cls) {

    double del, md2;
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
    del = cls_var->fmu + Scores.CaseFacScore * cls_var->ld - saux->xn;
    Scores.CaseFacScoreD1 += exp_var->frsds * (del * cls_var->ld + Scores.CaseFacScore * cls_var->ldsprd);
    md2 = exp_var->frsds * (exp_var->ldsq + cls_var->ldsprd);
    Scores.CaseFacScoreD2 += md2;
    Scores.EstFacScoreD2 += 1.1 * md2;
    return;
}

/*	-----------------------  cost_var  --------------------------   */
/*	Accumulates item cost into CaseNoFacCost, CaseFacCost    */
void cost_var(int iv, int fac, Class *cls) {
    double del, var, cost;
    SampleVar *smpl_var = &CurCtx.sample->variables[iv];
    Saux *saux = (Saux *)(smpl_var->saux);
    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];

    set_var(iv, cls);
    if (saux->missing)
        return;
    if (cls->age == 0) {
        exp_var->parkftcost = exp_var->parkstcost = 0.0;
        return;
    }
    /*	Do no-fac cost first  */
    del = cls_var->smu - saux->xn;
    var = del * del + cls_var->smusprd + saux->epssq;
    cost = 0.5 * var * exp_var->srsds + cls_var->ssdlsprd + HALF_LOG_2PI + cls_var->ssdl - saux->leps;
    exp_var->parkstcost = cost;
    Scores.CaseNoFacCost += cost;

    /*	Only do faccost if fac  */
    if (!fac)
        goto facdone;
    del += Scores.CaseFacScore * cls_var->ld;
    var = del * del + cls_var->fmusprd + saux->epssq + Scores.CaseFacScoreSq * cls_var->ldsprd + Scores.cvvsprd * exp_var->ldsq;
    exp_var->var = var;
    cost = HALF_LOG_2PI + 0.5 * exp_var->frsds * var + cls_var->fsdl + cls_var->fsdlsprd * 2.0 - saux->leps;

facdone:
    Scores.CaseFacCost += cost;
    exp_var->parkftcost = cost;

    return;
}

/*	--------------------  deriv_var  --------------------------  */
/*	Given the item weight in cwt, calcs derivs of cost wrt basic
params and accumulates in paramd1, paramd2.
Factor derivs done only if fac.  */
void deriv_var(int iv, int fac, Class *cls) {
    const double case_weight = cls->case_weight;
    double del, var, frsds;

    SampleVar *smpl_var = &CurCtx.sample->variables[iv];
    Saux *saux = (Saux *)(smpl_var->saux);
    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];

    set_var(iv, cls);
    if (saux->missing)
        return;
    /*	Do non-fac first  */
    exp_var->cnt += case_weight;
    /*	For non-fac, rather than getting derivatives I just collect
        the sufficient statistics, sum of xn, sum of xn^2  */
    exp_var->tx += case_weight * saux->xn;
    exp_var->txx += case_weight * (saux->xn * saux->xn + saux->epssq);
    /*	Accumulate weighted item cost  */
    exp_var->stcost += case_weight * exp_var->parkstcost;
    exp_var->ftcost += case_weight * exp_var->parkftcost;

    /*	Now for factor form  */
    if (fac) {

        frsds = exp_var->frsds;
        del = cls_var->fmu + Scores.CaseFacScore * cls_var->ld - saux->xn;
        /*	From cost_var, we have:
            cost = 0.5 * evi->frsds * var + cls_var->fsdl + cls_var->fsdlsprd*2.0 + consts
                where var is given by:
              del^2 + musprd + cvvsq*ldsprd + cvvsprd*ldsq + epssq
            var has been kept in stats.  */
        /*	Add to derivatives:  */
        var = exp_var->var;
        exp_var->fsdld1 += case_weight * (1.0 - frsds * var);
        exp_var->fsdld2 += 2.0 * case_weight;
        exp_var->fmud1 += case_weight * del * frsds;
        exp_var->fmud2 += case_weight * frsds;
        exp_var->ldd1 += case_weight * frsds * (del * Scores.CaseFacScore + cls_var->ld * Scores.cvvsprd);
        exp_var->ldd2 += case_weight * frsds * (Scores.CaseFacScoreSq + Scores.cvvsprd);
    }
}

/*	-------------------  adjust  ---------------------------    */
/*	To adjust parameters of a real variable     */
void adjust(int iv, int fac, Class *cls) {
    double adj, srsds, frsds, temp1, temp2, cnt;
    double del1, del2, del3, del4, spcost, fpcost;
    double dadmu = 0.0, dadsdl = 0.0, dmusprd = 0.0, dsdlsprd = 0.0;
    double av, var, del, sdld1;
    SampleVar *smpl_var = &CurCtx.sample->variables[iv];
    Saux *saux = (Saux *)(smpl_var->saux);
    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];
    Class *dad = (cls->dad_id >= 0) ? CurCtx.popln->classes[cls->dad_id] : 0;
    Basic *dad_var = (dad) ? (Basic *)dad->basics[iv] : 0;

    del3 = del4 = 0.0;
    set_var(iv, cls);
    adj = InitialAdj;
    cnt = exp_var->cnt;

    /*	Get prior constants from dad, or if root, fake them  */
    if (!dad) { /* Class is root */
        if (cls->age > 0) {
            dadmu = cls_var->smu;
            dadsdl = cls_var->ssdl;
            dmusprd = exp(2.0 * dadsdl);
            dsdlsprd = 1.0;
        } else {
            dadmu = dadsdl = 0.0;
            dmusprd = dsdlsprd = 1.0;
        }
    } else if (dad_var) {
        dadmu = dad_var->nmu;
        dadsdl = dad_var->nsdl;
        dmusprd = dad_var->nmusprd;
        dsdlsprd = dad_var->nsdlsprd;
    }

    /*	If too few data for this variable, use dad's n-paras  */
    if ((Control & AdjPr) && (cnt < MinSize)) {
        cls_var->nmu = cls_var->smu = cls_var->fmu = dadmu;
        cls_var->nsdl = cls_var->ssdl = cls_var->fsdl = dadsdl;
        cls_var->ld = 0.0;
        cls_var->nmusprd = cls_var->smusprd = cls_var->fmusprd = dmusprd;
        cls_var->nsdlsprd = cls_var->ssdlsprd = cls_var->fsdlsprd = dsdlsprd;
        cls_var->ldsprd = 1.0;

    } else if (!cls->age) { /*	If class age is zero, make some preliminary estimates  */
        cls_var->smu = cls_var->fmu = exp_var->tx / cnt;
        var = exp_var->txx / cnt - cls_var->smu * cls_var->smu;
        exp_var->ssig = exp_var->fsig = sqrt(var);
        cls_var->ssdl = cls_var->fsdl = log(exp_var->ssig);
        exp_var->frsds = exp_var->srsds = 1.0 / var;
        cls_var->smusprd = cls_var->fmusprd = var / cnt;
        cls_var->ldsprd = var / cnt;
        cls_var->ld = 0.0;
        cls_var->ssdlsprd = cls_var->fsdlsprd = 1.0 / cnt;
        if (!dad) { /*  First stab at root  */
            dadmu = cls_var->smu;
            dadsdl = cls_var->ssdl;
            dmusprd = exp(2.0 * dadsdl);
            dsdlsprd = 1.0;
        }
        /*	Make a stab at class tcost  */
        cls->cstcost += cnt * (HALF_LOG_2PI + cls_var->ssdl - saux->leps + 0.5 + cls->mlogab) + 1.0;
        cls->cftcost = cls->cstcost + 100.0 * cnt;
    }

    temp1 = 1.0 / dmusprd;
    temp2 = 1.0 / dsdlsprd;
    srsds = exp_var->srsds;
    frsds = exp_var->frsds;

    /*	Compute parameter costs as they are  */
    del1 = dadmu - cls_var->smu;
    spcost = HALF_LOG_2PI + 0.5 * (log(dmusprd) + temp1 * (del1 * del1 + cls_var->smusprd));
    del2 = dadsdl - cls_var->ssdl;
    spcost += HALF_LOG_2PI + 0.5 * (log(dsdlsprd) + temp2 * (del2 * del2 + cls_var->ssdlsprd));
    spcost -= 0.5 * log(cls_var->smusprd * cls_var->ssdlsprd);
    spcost += 2.0 * LATTICE;

    if (!fac) {
        fpcost = spcost + 100.0;
        cls_var->infac = 1;
        goto facdone1;
    }
    del3 = cls_var->fmu - dadmu;
    fpcost = HALF_LOG_2PI + 0.5 * (log(dmusprd) + temp1 * (del3 * del3 + cls_var->fmusprd));
    del4 = cls_var->fsdl - dadsdl;
    fpcost += HALF_LOG_2PI + 0.5 * (log(dsdlsprd) + temp2 * (del4 * del4 + cls_var->fsdlsprd));
    /*    The prior for load ld id N (0, sigsq)  */
    fpcost += HALF_LOG_2PI + 0.5 * (exp_var->ldsq + cls_var->ldsprd) * frsds + cls_var->fsdl;
    fpcost -= 0.5 * log(cls_var->fmusprd * cls_var->fsdlsprd * cls_var->ldsprd);
    fpcost += 3.0 * LATTICE;

facdone1:

    /*	Store param costs for this variable  */
    exp_var->spcost = spcost;
    exp_var->fpcost = fpcost;
    /*	Add to class param costs  */
    cls->nofac_par_cost += spcost;
    cls->fac_par_cost += fpcost;
    if (cnt < MinSize)
        goto adjdone;
    if (!(Control & AdjPr))
        goto tweaks;
    /*	Adjust non-fac parameters.   */
    /*	From things, smud1 = (tx - cnt*mu) * srsds, from prior,
        smud1 = (dadmu - mu) / dmusprd.
        From things, smud2 = cnt * srsds, from prior = 1/dmusprd.
        Explicit optimum:  */
    cls_var->smusprd = 1.0 / (cnt * srsds + temp1);
    cls_var->smu = (exp_var->tx * srsds + dadmu * temp1) * cls_var->smusprd;
    /*	Calculate variance about new mean, adding variance from musprd   */
    av = exp_var->tx / cnt;
    del = cls_var->smu - av;
    var = exp_var->txx + cnt * (del * del - av * av + cls_var->smusprd);
    /*	The deriv sdld1 from data is (cnt - rsds * var)  */
    sdld1 = cnt - srsds * var;
    /*	The deriv from prior is (sdl-dadsdl) / dsdlsprd  */
    sdld1 += (cls_var->ssdl - dadsdl) * temp2;
    /*	The 2nd deriv sdld2 from data is just cnt  */
    /*	From prior, get 1/dsdlsprd  */
    cls_var->ssdlsprd = 1.0 / (cnt + temp2);
    /*	sdlsprd is just 1 / sdld2  */
    /*	Adjust, but not too much  */
    del = sdld1 * cls_var->ssdlsprd;
    if (del > 0.2)
        del = 0.2;
    else if (del < -0.2)
        del = -0.2;
    cls_var->ssdl -= del;
    exp_var->ssig = exp(cls_var->ssdl);
    exp_var->srsds = 1.0 / (exp_var->ssig * exp_var->ssig);
    if (!fac)
        goto facdone2;

    /*	Now for factor adjust.
        We have derivatives fmud1, fsdld1, ldd1 etc in stats. Add terms
        from priors
        */
    exp_var->fmud1 += del3 * temp1;
    exp_var->fmud2 += temp1;
    exp_var->fsdld1 += del4 * temp2;
    exp_var->fsdld2 += temp2;
    exp_var->ldd1 += cls_var->ld * frsds;
    exp_var->fsdld2 += frsds;
    /*	The dependence of the load prior on frsds, and thus on fsdl,
        will give additional terms to sdld1, sdld2.
        */
    /*	The additional terms from the load prior :  */
    exp_var->fsdld1 += 1.0 - frsds * (exp_var->ldsq + cls_var->ldsprd);
    exp_var->fsdld2 += 1.0;
    /*	Adjust sdl, but not too much.  */
    cls_var->fsdlsprd = 1.0 / exp_var->fsdld2;
    del = exp_var->fsdld1 * cls_var->fsdlsprd;
    if (del > 0.2)
        del = 0.2;
    else if (del < -0.2)
        del = -0.2;
    cls_var->fsdl -= adj * del;
    cls_var->fmusprd = 1.0 / exp_var->fmud2;
    cls_var->fmu -= adj * exp_var->fmud1 * cls_var->fmusprd;
    cls_var->ldsprd = 1.0 / exp_var->ldd2;
    cls_var->ld -= adj * exp_var->ldd1 * cls_var->ldsprd;
    exp_var->fsig = exp(cls_var->fsdl);
    exp_var->frsds = 1.0 / (exp_var->fsig * exp_var->fsig);
    exp_var->ldsq = cls_var->ld * cls_var->ld;

facdone2:
    cls_var->samplesize = exp_var->cnt;
    goto adjdone;

tweaks: /* Come here if no adjustments made */
    /*	Deal only with sub-less leaves  */
    if ((cls->type != Leaf) || (cls->num_sons < 2))
        goto adjdone;

    /*	If Noprior, guess no-prior params, sprds and store instead of
        as-dad values. Actually store in nparsprd the val 1/bparsprd,
        and in npar the val bpar / bparsprd   */
    if (Control & Noprior) {
        cls_var->nmusprd = (1.0 / cls_var->bmusprd) - (1.0 / dmusprd);
        cls_var->nmu = cls_var->bmu / cls_var->bmusprd - dadmu / dmusprd;
        cls_var->nsdlsprd = (1.0 / cls_var->bsdlsprd) - (1.0 / dsdlsprd);
        cls_var->nsdl = cls_var->bsdl / cls_var->bsdlsprd - dadsdl / dsdlsprd;
    } else if (Control & Tweak) {
        /*	Adjust "best" params by applying effect of dad prior to
            the no-prior values  */
        cls_var->bmusprd = 1.0 / (cls_var->nmusprd + (1.0 / dmusprd));
        cls_var->bmu = (cls_var->nmu + dadmu / dmusprd) * cls_var->bmusprd;
        cls_var->bsdlsprd = 1.0 / (cls_var->nsdlsprd + (1.0 / dsdlsprd));
        cls_var->bsdl = (cls_var->nsdl + dadsdl / dsdlsprd) * cls_var->bsdlsprd;
        if (cls->use == Fac) {
            cls_var->fmu = cls_var->bmu;
            cls_var->fmusprd = cls_var->nmusprd;
        } else {
            cls_var->smu = cls_var->bmu;
            cls_var->smusprd = cls_var->nmusprd;
        }
    } else {
        /*	Copy non-fac params to as-dad   */
        cls_var->nmu = cls_var->smu;
        cls_var->nmusprd = cls_var->smusprd;
        cls_var->nsdl = cls_var->ssdl;
        cls_var->nsdlsprd = cls_var->ssdlsprd;
    }

adjdone:
    return;
}

/*	------------------------  show  -----------------------   */
void show(Class *cls, int iv) {
    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];
    set_var(iv, cls);

    printf("V%3d  Cnt%6.1f  %s\n", iv + 1, exp_var->cnt, (cls_var->infac) ? " In" : "Out");
    if (cls->num_sons > 1) {
        printf(" N: Cost%8.1f  Mu%8.3f+-%8.3f  SD%8.3f+-%8.3f\n", exp_var->npcost, cls_var->nmu, sqrt(cls_var->nmusprd), exp(cls_var->nsdl),
               exp(cls_var->nsdl) * sqrt(cls_var->nsdlsprd));
    }
    printf(" S: Cost%8.1f  Mu%8.3f  SD%8.3f\n", exp_var->spcost + exp_var->stcost, cls_var->smu, exp(cls_var->ssdl));
    printf(" F: Cost%8.1f  Mu%8.3f  SD%8.3f  Ld%8.3f\n", exp_var->fpcost + exp_var->ftcost, cls_var->fmu, exp(cls_var->fsdl), cls_var->ld);
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
        print_buffer(buffer, "\"mean\": %0.5f, \"stdev\": %0.5f, \"loading\": %0.3f", cls_var->fmu, exp(cls_var->fsdl), cls_var->ld);
    } else {
        print_buffer(buffer, "\"mean\": %0.5f, \"stdev\": %0.5f", cls_var->smu, exp(cls_var->ssdl));
    }
    print_buffer(buffer, "}");
}

/*	----------------------  cost_var_nonleaf  ------------------------   */
/*	To compute parameter cost for non-leaf (intrnl) class use   */

/*	The coding in an internal class of a simple scalar parameter such as
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
    N*hlg2pi + 0.5*N*log(s) + (1/2s)*Sum_n{(tn-t)^2 + deln}
For {sm} :
    M*log(s) + (1/s)*Sum_m{sm}    where we index the internal sons by m
                    rather than by n.

Calling the sum of these L, the expected second differential of L wrt t
is
    F_t = N/s	 and the expected second differential of L wrt s
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
   or	s^2*(1 + 0.5/N)/sp  +  s*(N/2 + M - 3/2)  -  (V/2 + S) = 0
        which is quadratic in s.

Writing the quadratic as    a*s^2 + b*s -c = 0,   we want the root

    s = 2*c / { b + sqrt (b^2 + 4ac) }.  This is the form used in code.

    */

void cost_var_nonleaf(int iv, int vald, Class *cls) {
    Basic *son_var;
    Class *son;
    double pcost, tcost; /* Item, param and total item costs */
    double pp, ppsprd, dadpp, dppsprd, del;
    double co0, co1, co2, mean;
    double tssn, tstn, tstvn;
    double spp, sppsprd;
    int nints, nson, ison, k, n;
    Population *popln = CurCtx.popln;
    Class *dad = (cls->dad_id >= 0) ? popln->classes[cls->dad_id] : 0;
    VSetVar *vset_var = &CurCtx.vset->variables[iv];
    Basic *cls_var = (Basic *)cls->basics[iv];
    Stats *exp_var = (Stats *)cls->stats[iv];
    Basic *dad_var = (dad) ? (Basic *)dad->basics[iv] : 0;

    set_var(iv, cls);
    if (!vald) { /* Cannot define as-dad params, so fake it */
        exp_var->npcost = 0.0;
        cls_var->nmu = cls_var->smu;
        cls_var->nmusprd = cls_var->smusprd * exp_var->cnt;
        cls_var->nsdl = cls_var->ssdl;
        cls_var->nsdlsprd = cls_var->ssdlsprd * exp_var->cnt;
        return;
    }
    if (vset_var->inactive) {
        exp_var->npcost = exp_var->ntcost = 0.0;
        return;
    }
    nson = cls->num_sons;
    pcost = tcost = 0.0;

    /*	There are two independent parameters, nmu and nsdl, to fiddle.
        As the info for these is stored in consecutive Sf words in Basic,
    we can do the two params in a two-count loop. We use p... to mean mu... and
    sdl... in the two times round the loop  */

    k = 0;
    while (k < 2) {
        /*	Get prior constants from dad, or if root, fake them  */
        if (!dad) { /* Class is root */
            dadpp = *(&cls_var->smu + k);
            dppsprd = *(&cls_var->smusprd + k) * exp_var->cnt;
        } else {
            dadpp = *(&dad_var->nmu + k);
            dppsprd = *(&dad_var->nmusprd + k);
        }
        pp = *(&cls_var->nmu + k);
        ppsprd = *(&cls_var->nmusprd + k);

        /*	We need to accumlate things over sons. We need:   */
        nints = 0;   /* Number of internal sons (M) */
        tstn = 0.0;  /* Total sons' t_n */
        tstvn = 0.0; /* Total sum of sons' (t_n^2 + del_n)  */
        tssn = 0.0;  /* Total sons' s_n */

        for (ison = cls->son_id; ison > 0; ison = son->sib_id) {
            son = popln->classes[ison];
            son_var = (Basic *)son->basics[iv];
            spp = *(&son_var->bmu + k);
            sppsprd = *(&son_var->bmusprd + k);
            tstn += spp;
            tstvn += spp * spp;
            if (son->type == Dad) { /* used as parent */
                nints++;
                tssn += sppsprd;
                tstvn += sppsprd / son->num_sons;
            } else
                tstvn += sppsprd;
        }
        /*	Calc coeffs for quadratic c_2 * s^2 + c_1 * s - c_0  */
        co2 = (1.0 + 0.5 / nson) / dppsprd;
        co1 = nints + 0.5 * (nson - 3.0);
        /*	Can now compute V in the above comments.  tssn = S of the comments */
        /*	First we get the V around the sons' mean, then update pp and correct
            the value of V  */
        mean = tstn / nson;   /* Mean of sons' param  */
        tstvn -= mean * tstn; /*	Variance around mean  */
        if (!(Control & AdjPr))
            goto adjdone;

        /*	Iterate the adjustment of param, spread  */
        n = 5;
    adjloop:
        /*	Update param  */
        pp = (dppsprd * tstn + ppsprd * dadpp) / (nson * dppsprd + ppsprd);
        del = pp - mean;
        /*	The V of comments is tstvn + nson * del * del */
        co0 = 0.5 * (tstvn + nson * del * del) + tssn;
        /*	Solve for new spread  */
        ppsprd = 2.0 * co0 / (co1 + sqrt(co1 * co1 + 4.0 * co0 * co2));
        n--;
        if (n)
            goto adjloop;
        *(&cls_var->nmu + k) = pp;
        *(&cls_var->nmusprd + k) = ppsprd;

    adjdone: /*	Calc cost  */
        del = pp - dadpp;
        pcost += HALF_LOG_2PI + 1.5 * log(dppsprd) + 0.5 * (del * del + ppsprd / nson) / dppsprd + ppsprd / dppsprd;
        /*	Add hlog Fisher, lattice  */
        pcost += 0.5 * log(nson * (0.5 * nson + nints)) - 1.5 * log(ppsprd) + 2.0 * LATTICE;

        /*	Add roundoff for 2 params  (pp, ppsprd)  */
        pcost += 1.0;
        /*	This completes the shenanigans for one param. See if another */
        k++;
    }

    exp_var->npcost = pcost;

    return;
}
