/*	A prototypical file for a variable type      */
/*	actually for plain Gaussians   */

#include "glob.h"

static void set_var();
static int read_attr_aux();
static int read_smpl_aux();
static int read_datum();
static void print_datum();
static void set_sizes();
static void set_best_pars();
static void clear_stats();
static void score_var();
static void cost_var();
static void deriv_var();
static void nonleaf_cost_var();
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

static Saux *saux;
static Paux *paux;
static Vaux *vaux;
static Basic *cvi, *dcvi;
static Stats *evi;

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
    VarType *vtp;

    vtp = Types + typindx;
    vtp->id = typindx;
    /* 	Set type name as string up to 59 chars  */
    vtp->name = "Standard Gaussian";
    vtp->data_size = sizeof(Datum);
    vtp->attr_aux_size = sizeof(Vaux);
    vtp->pop_aux_size = sizeof(Paux);
    vtp->smpl_aux_size = sizeof(Saux);
    vtp->read_aux_attr = &read_attr_aux;
    vtp->read_aux_smpl = &read_smpl_aux;
    vtp->read_datum = &read_datum;
    vtp->print_datum = &print_datum;
    vtp->set_sizes = &set_sizes;
    vtp->set_best_pars = &set_best_pars;
    vtp->clear_stats = &clear_stats;
    vtp->score_var = &score_var;
    vtp->cost_var = &cost_var;
    vtp->deriv_var = &deriv_var;
    vtp->cost_var_nonleaf = &nonleaf_cost_var;
    vtp->adjust = &adjust;
    vtp->show = &show;
    vtp->set_var = &set_var;
}

/*	-------------------  setvar -----------------------------  */
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
    if (CurDad)
        dcvi = (Basic *)CurDad->basics[iv];
    else
        dcvi = 0;
    return;
}

/*	--------------------  readvaux  ----------------------------  */
/*      Read in auxiliary info into vaux, return 0 if OK else 1  */
int read_attr_aux(Vaux *vax) { return (0); }

/*	---------------------  readsaux ---------------------------   */
/*	To read any auxiliary info about a variable of this type in some
sample.
    */
int read_smpl_aux(Saux *sax) {
    int i;

    /*	Read in auxiliary info into saux, return 0 if OK else 1  */
    i = read_double(&(sax->eps), 1);
    if (i < 0) {
        sax->eps = sax->leps = 0.0;
        return (1);
    }
    sax->epssq = sax->eps * sax->eps * (1.0 / 12.0);
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
        memcpy(loc, &xn, sizeof(Datum));
    return (i);
}

/*	---------------------  print_datum --------------------------  */
/*	To print a Datum value   */
void print_datum(char *loc) {
    /*	Print datum from address loc   */
    printf("%9.2f", *((double *)loc));
    return;
}

/*	---------------------  setsizes  -----------------------   */
void set_sizes(int iv) 
{
    CurAttr = CurAttrList + iv;
    CurAttr->basic_size = sizeof(Basic);
    CurAttr->stats_size = sizeof(Stats);
    return;
}

/*	----------------------  set_best_pars --------------------------  */
void set_best_pars(int iv)
{

    set_var(iv);

    if (CurClass->type == Dad) {
        cvi->bmu = cvi->nmu;
        cvi->bmusprd = cvi->nmusprd;
        cvi->bsdl = cvi->nsdl;
        cvi->bsdlsprd = cvi->nsdlsprd;
        evi->btcost = evi->ntcost;
        evi->bpcost = evi->npcost;
    } else if ((CurClass->use == Fac) && cvi->infac) {
        cvi->bmu = cvi->fmu;
        cvi->bmusprd = cvi->fmusprd;
        cvi->bsdl = cvi->fsdl;
        cvi->bsdlsprd = cvi->fsdlsprd;
        evi->btcost = evi->ftcost;
        evi->bpcost = evi->fpcost;
    } else {
        cvi->bmu = cvi->smu;
        cvi->bmusprd = cvi->smusprd;
        cvi->bsdl = cvi->ssdl;
        cvi->bsdlsprd = cvi->ssdlsprd;
        evi->btcost = evi->stcost;
        evi->bpcost = evi->spcost;
    }
    return;
}

/*	------------------------  clear_stats  ------------------------  */
/*	Clears stats to accumulate in cost_var, and derives useful functions
of basic params  */
void clear_stats(int iv)
{
    double tmp;
    set_var(iv);
    evi->cnt = 0.0;
    evi->stcost = evi->ftcost = 0.0;
    evi->vsq = 0.0;
    evi->tx = evi->txx = 0.0;
    evi->fsdld1 = evi->fmud1 = evi->ldd1 = 0.0;
    evi->fsdld2 = evi->fmud2 = evi->ldd2 = 0.0;

    if (CurClass->age == 0)
        return;
    evi->ssig = exp(cvi->ssdl);
    tmp = 1.0 / evi->ssig;
    evi->srsds = tmp * tmp;
    evi->fsig = exp(cvi->fsdl);
    tmp = 1.0 / evi->fsig;
    evi->frsds = tmp * tmp;
    evi->ldsq = cvi->ld * cvi->ld;
}

/*	-------------------------  score_var  ------------------------   */
/*	To eval derivs of a case cost wrt score, scorespread. Adds to
vvd1, vvd2.
*/
void score_var(int iv)
{

    double del, md2;

    set_var(iv);
    if (CurAttr->inactive)
        return;

    if (saux->missing)
        return;
    del = cvi->fmu + Scores.CaseFacScore * cvi->ld - saux->xn;
    Scores.CaseFacScoreD1 += evi->frsds * (del * cvi->ld + Scores.CaseFacScore * cvi->ldsprd);
    md2 = evi->frsds * (evi->ldsq + cvi->ldsprd);
    Scores.CaseFacScoreD2 += md2;
    Scores.EstFacScoreD2 += 1.1 * md2;
    return;
}

/*	-----------------------  cost_var  --------------------------   */
/*	Accumulates item cost into CaseNoFacCost, CaseFacCost    */
void cost_var( int iv,  int fac)
{
    double del, var, cost;
    set_var(iv);
    if (saux->missing)
        return;
    if (CurClass->age == 0) {
        evi->parkftcost = evi->parkstcost = 0.0;
        return;
    }
    /*	Do no-fac cost first  */
    del = cvi->smu - saux->xn;
    var = del * del + cvi->smusprd + saux->epssq;
    cost = 0.5 * var * evi->srsds + cvi->ssdlsprd + HALF_LOG_2PI + cvi->ssdl - saux->leps;
    evi->parkstcost = cost;
    Scores.CaseNoFacCost += cost;

    /*	Only do faccost if fac  */
    if (!fac)
        goto facdone;
    del += Scores.CaseFacScore * cvi->ld;
    var = del * del + cvi->fmusprd + saux->epssq + Scores.CaseFacScoreSq * cvi->ldsprd + Scores.cvvsprd * evi->ldsq;
    evi->var = var;
    cost = HALF_LOG_2PI + 0.5 * evi->frsds * var + cvi->fsdl + cvi->fsdlsprd * 2.0 - saux->leps;

facdone:
    Scores.CaseFacCost += cost;
    evi->parkftcost = cost;

    return;
}

/*	--------------------  deriv_var  --------------------------  */
/*	Given the item weight in cwt, calcs derivs of cost wrt basic
params and accumulates in paramd1, paramd2.
Factor derivs done only if fac.  */
void deriv_var( int iv,  int fac)
{
    double del, var, frsds;
    set_var(iv);
    if (saux->missing)
        return;
    /*	Do non-fac first  */
    evi->cnt += CurCaseWeight;
    /*	For non-fac, rather than getting derivatives I just collect
        the sufficient statistics, sum of xn, sum of xn^2  */
    evi->tx += CurCaseWeight * saux->xn;
    evi->txx += CurCaseWeight * (saux->xn * saux->xn + saux->epssq);
    /*	Accumulate weighted item cost  */
    evi->stcost += CurCaseWeight * evi->parkstcost;
    evi->ftcost += CurCaseWeight * evi->parkftcost;

    /*	Now for factor form  */
    if (!fac)
        goto facdone;
    frsds = evi->frsds;
    del = cvi->fmu + Scores.CaseFacScore * cvi->ld - saux->xn;
    /*	From cost_var, we have:
        cost = 0.5 * evi->frsds * var + cvi->fsdl + cvi->fsdlsprd*2.0 + consts
            where var is given by:
          del^2 + musprd + cvvsq*ldsprd + cvvsprd*ldsq + epssq
        var has been kept in stats.  */
    /*	Add to derivatives:  */
    var = evi->var;
    evi->fsdld1 += CurCaseWeight * (1.0 - frsds * var);
    evi->fsdld2 += 2.0 * CurCaseWeight;
    evi->fmud1 += CurCaseWeight * del * frsds;
    evi->fmud2 += CurCaseWeight * frsds;
    evi->ldd1 += CurCaseWeight * frsds * (del * Scores.CaseFacScore + cvi->ld * Scores.cvvsprd);
    evi->ldd2 += CurCaseWeight * frsds * (Scores.CaseFacScoreSq + Scores.cvvsprd);
facdone:
    return;
}

/*	-------------------  adjust  ---------------------------    */
/*	To adjust parameters of a real variable     */
void adjust( int iv,  int fac)
{
    double adj, srsds, frsds, temp1, temp2, cnt;
    double del1, del2, del3, del4, spcost, fpcost;
    double dadmu, dadsdl, dmusprd, dsdlsprd;
    double av, var, del, sdld1;

    del3 = del4 = 0.0;
    set_var(iv);
    adj = InitialAdj;
    cnt = evi->cnt;

    /*	Get prior constants from dad, or if root, fake them  */
    if (!CurDad) { /* Class is root */
        if (CurClass->age > 0) {
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

    /*	If too few data for this variable, use dad's n-paras  */
    if ((Control & AdjPr) && (cnt < MinSize)) {
        cvi->nmu = cvi->smu = cvi->fmu = dadmu;
        cvi->nsdl = cvi->ssdl = cvi->fsdl = dadsdl;
        cvi->ld = 0.0;
        cvi->nmusprd = cvi->smusprd = cvi->fmusprd = dmusprd;
        cvi->nsdlsprd = cvi->ssdlsprd = cvi->fsdlsprd = dsdlsprd;
        cvi->ldsprd = 1.0;
        goto hasage;
    }
    /*	If class age is zero, make some preliminary estimates  */
    if (CurClass->age)
        goto hasage;
    cvi->smu = cvi->fmu = evi->tx / cnt;
    var = evi->txx / cnt - cvi->smu * cvi->smu;
    evi->ssig = evi->fsig = sqrt(var);
    cvi->ssdl = cvi->fsdl = log(evi->ssig);
    evi->frsds = evi->srsds = 1.0 / var;
    cvi->smusprd = cvi->fmusprd = var / cnt;
    cvi->ldsprd = var / cnt;
    cvi->ld = 0.0;
    cvi->ssdlsprd = cvi->fsdlsprd = 1.0 / cnt;
    if (!CurDad) { /*  First stab at root  */
        dadmu = cvi->smu;
        dadsdl = cvi->ssdl;
        dmusprd = exp(2.0 * dadsdl);
        dsdlsprd = 1.0;
    }
    /*	Make a stab at class tcost  */
    CurClass->cstcost += cnt * (HALF_LOG_2PI + cvi->ssdl - saux->leps + 0.5 + CurClass->mlogab) + 1.0;
    CurClass->cftcost = CurClass->cstcost + 100.0 * cnt;

hasage:
    temp1 = 1.0 / dmusprd;
    temp2 = 1.0 / dsdlsprd;
    srsds = evi->srsds;
    frsds = evi->frsds;

    /*	Compute parameter costs as they are  */
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
    fpcost += HALF_LOG_2PI + 0.5 * (evi->ldsq + cvi->ldsprd) * frsds + cvi->fsdl;
    fpcost -= 0.5 * log(cvi->fmusprd * cvi->fsdlsprd * cvi->ldsprd);
    fpcost += 3.0 * LATTICE;

facdone1:

    /*	Store param costs for this variable  */
    evi->spcost = spcost;
    evi->fpcost = fpcost;
    /*	Add to class param costs  */
    CurClass->nofac_par_cost += spcost;
    CurClass->fac_par_cost += fpcost;
    if (cnt < MinSize)
        goto adjdone;
    if (!(Control & AdjPr))
        goto tweaks;
    /*	Adjust non-fac parameters.   */
    /*	From things, smud1 = (tx - cnt*mu) * srsds, from prior,
        smud1 = (dadmu - mu) / dmusprd.
        From things, smud2 = cnt * srsds, from prior = 1/dmusprd.
        Explicit optimum:  */
    cvi->smusprd = 1.0 / (cnt * srsds + temp1);
    cvi->smu = (evi->tx * srsds + dadmu * temp1) * cvi->smusprd;
    /*	Calculate variance about new mean, adding variance from musprd   */
    av = evi->tx / cnt;
    del = cvi->smu - av;
    var = evi->txx + cnt * (del * del - av * av + cvi->smusprd);
    /*	The deriv sdld1 from data is (cnt - rsds * var)  */
    sdld1 = cnt - srsds * var;
    /*	The deriv from prior is (sdl-dadsdl) / dsdlsprd  */
    sdld1 += (cvi->ssdl - dadsdl) * temp2;
    /*	The 2nd deriv sdld2 from data is just cnt  */
    /*	From prior, get 1/dsdlsprd  */
    cvi->ssdlsprd = 1.0 / (cnt + temp2);
    /*	sdlsprd is just 1 / sdld2  */
    /*	Adjust, but not too much  */
    del = sdld1 * cvi->ssdlsprd;
    if (del > 0.2)
        del = 0.2;
    else if (del < -0.2)
        del = -0.2;
    cvi->ssdl -= del;
    evi->ssig = exp(cvi->ssdl);
    evi->srsds = 1.0 / (evi->ssig * evi->ssig);
    if (!fac)
        goto facdone2;

    /*	Now for factor adjust.
        We have derivatives fmud1, fsdld1, ldd1 etc in stats. Add terms
        from priors
        */
    evi->fmud1 += del3 * temp1;
    evi->fmud2 += temp1;
    evi->fsdld1 += del4 * temp2;
    evi->fsdld2 += temp2;
    evi->ldd1 += cvi->ld * frsds;
    evi->fsdld2 += frsds;
    /*	The dependence of the load prior on frsds, and thus on fsdl,
        will give additional terms to sdld1, sdld2.
        */
    /*	The additional terms from the load prior :  */
    evi->fsdld1 += 1.0 - frsds * (evi->ldsq + cvi->ldsprd);
    evi->fsdld2 += 1.0;
    /*	Adjust sdl, but not too much.  */
    cvi->fsdlsprd = 1.0 / evi->fsdld2;
    del = evi->fsdld1 * cvi->fsdlsprd;
    if (del > 0.2)
        del = 0.2;
    else if (del < -0.2)
        del = -0.2;
    cvi->fsdl -= adj * del;
    cvi->fmusprd = 1.0 / evi->fmud2;
    cvi->fmu -= adj * evi->fmud1 * cvi->fmusprd;
    cvi->ldsprd = 1.0 / evi->ldd2;
    cvi->ld -= adj * evi->ldd1 * cvi->ldsprd;
    evi->fsig = exp(cvi->fsdl);
    evi->frsds = 1.0 / (evi->fsig * evi->fsig);
    evi->ldsq = cvi->ld * cvi->ld;

facdone2:
    cvi->samplesize = evi->cnt;
    goto adjdone;

tweaks: /* Come here if no adjustments made */
    /*	Deal only with sub-less leaves  */
    if ((CurClass->type != Leaf) || (CurClass->num_sons < 2))
        goto adjdone;

    /*	If Noprior, guess no-prior params, sprds and store instead of
        as-dad values. Actually store in nparsprd the val 1/bparsprd,
        and in npar the val bpar / bparsprd   */
    if (Control & Noprior) {
        cvi->nmusprd = (1.0 / cvi->bmusprd) - (1.0 / dmusprd);
        cvi->nmu = cvi->bmu / cvi->bmusprd - dadmu / dmusprd;
        cvi->nsdlsprd = (1.0 / cvi->bsdlsprd) - (1.0 / dsdlsprd);
        cvi->nsdl = cvi->bsdl / cvi->bsdlsprd - dadsdl / dsdlsprd;
    } else if (Control & Tweak) {
        /*	Adjust "best" params by applying effect of dad prior to
            the no-prior values  */
        cvi->bmusprd = 1.0 / (cvi->nmusprd + (1.0 / dmusprd));
        cvi->bmu = (cvi->nmu + dadmu / dmusprd) * cvi->bmusprd;
        cvi->bsdlsprd = 1.0 / (cvi->nsdlsprd + (1.0 / dsdlsprd));
        cvi->bsdl = (cvi->nsdl + dadsdl / dsdlsprd) * cvi->bsdlsprd;
        if (CurClass->use == Fac) {
            cvi->fmu = cvi->bmu;
            cvi->fmusprd = cvi->nmusprd;
        } else {
            cvi->smu = cvi->bmu;
            cvi->smusprd = cvi->nmusprd;
        }
    } else {
        /*	Copy non-fac params to as-dad   */
        cvi->nmu = cvi->smu;
        cvi->nmusprd = cvi->smusprd;
        cvi->nsdl = cvi->ssdl;
        cvi->nsdlsprd = cvi->ssdlsprd;
    }

adjdone:
    return;
}

/*	------------------------  show  -----------------------   */
void show(Class *ccl, int iv) 
{

    set_class(ccl);
    set_var(iv);

    printf("V%3d  Cnt%6.1f  %s\n", iv + 1, evi->cnt, (cvi->infac) ? " In" : "Out");
    if (CurClass->num_sons < 2)
        goto skipn;
    printf(" N: Cost%8.1f  Mu%8.3f+-%8.3f  SD%8.3f+-%8.3f\n", evi->npcost, cvi->nmu, sqrt(cvi->nmusprd), exp(cvi->nsdl), exp(cvi->nsdl) * sqrt(cvi->nsdlsprd));
skipn:
    printf(" S: Cost%8.1f  Mu%8.3f  SD%8.3f\n", evi->spcost + evi->stcost, cvi->smu, exp(cvi->ssdl));
    printf(" F: Cost%8.1f  Mu%8.3f  SD%8.3f  Ld%8.3f\n", evi->fpcost + evi->ftcost, cvi->fmu, exp(cvi->fsdl), cvi->ld);
    return;
}

/*	----------------------  nonleaf_cost_var  ------------------------   */
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

void nonleaf_cost_var(int iv, int vald)
{
    Basic *soncvi;
    Class *son;
    double pcost, tcost; /* Item, param and total item costs */
    double pp, ppsprd, dadpp, dppsprd, del;
    double co0, co1, co2, mean;
    double tssn, tstn, tstvn;
    double spp, sppsprd;
    int nints, nson, ison, k, n;

    set_var(iv);
    if (!vald) { /* Cannot define as-dad params, so fake it */
        evi->npcost = 0.0;
        cvi->nmu = cvi->smu;
        cvi->nmusprd = cvi->smusprd * evi->cnt;
        cvi->nsdl = cvi->ssdl;
        cvi->nsdlsprd = cvi->ssdlsprd * evi->cnt;
        return;
    }
    if (CurAttr->inactive) {
        evi->npcost = evi->ntcost = 0.0;
        return;
    }
    nson = CurClass->num_sons;
    pcost = tcost = 0.0;

    /*	There are two independent parameters, nmu and nsdl, to fiddle.
        As the info for these is stored in consecutive Sf words in Basic,
    we can do the two params in a two-count loop. We use p... to mean mu... and
    sdl... in the two times round the loop  */

    k = 0;
ploop:
    /*	Get prior constants from dad, or if root, fake them  */
    if (!CurDad) { /* Class is root */
        dadpp = *(&cvi->smu + k);
        dppsprd = *(&cvi->smusprd + k) * evi->cnt;
    } else {
        dadpp = *(&dcvi->nmu + k);
        dppsprd = *(&dcvi->nmusprd + k);
    }
    pp = *(&cvi->nmu + k);
    ppsprd = *(&cvi->nmusprd + k);

    /*	We need to accumlate things over sons. We need:   */
    nints = 0;   /* Number of internal sons (M) */
    tstn = 0.0;  /* Total sons' t_n */
    tstvn = 0.0; /* Total sum of sons' (t_n^2 + del_n)  */
    tssn = 0.0;  /* Total sons' s_n */

    for (ison = CurClass->son_id; ison > 0; ison = son->sib_id) {
        son = CurPopln->classes[ison];
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
    *(&cvi->nmu + k) = pp;
    *(&cvi->nmusprd + k) = ppsprd;

adjdone: /*	Calc cost  */
    del = pp - dadpp;
    pcost += HALF_LOG_2PI + 1.5 * log(dppsprd) + 0.5 * (del * del + ppsprd / nson) / dppsprd + ppsprd / dppsprd;
    /*	Add hlog Fisher, lattice  */
    pcost += 0.5 * log(nson * (0.5 * nson + nints)) - 1.5 * log(ppsprd) + 2.0 * LATTICE;

    /*	Add roundoff for 2 params  (pp, ppsprd)  */
    pcost += 1.0;
    /*	This completes the shenanigans for one param. See if another */
    k++;
    if (k < 2)
        goto ploop;

    evi->npcost = pcost;

    return;
}
