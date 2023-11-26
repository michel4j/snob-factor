/*    Corrected 11-6-02    */
/*     Von Mises - Fisher distribution for circular angle data  (2-d) */
/*    Version 3. Shift angle w now taken as (2 * arctan (t)) with
    t = v * ld.
    */

#define VONM 1
#include "glob.h"

typedef struct Vmpackst {
    double kap;
    double rkap;
    double lgi0;
    double aa;
    double daa;
    double fh;
    double dfh;
} Vmpack;

void kapcode(double hx, double hy, double *vmst);

#define NullSprd ((double)10.0) /* Root prior spread */

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

typedef struct DatumStruct {
    double xx;
    double sinxx;
    double cosxx;
} Datum;

typedef struct Sauxst {
    int missing;
    double dummy;
    Datum xn;
    double parkfxx; /* angle - vv*ld for item n */
    double eps;
    double leps;   /* Log eps */
    double epsfac; /* It seems that the 'increased variance' effect
       of imprecise data is modeled by taking the sin and cosine
       of datum x as being  epsfac*sin(x) and epsfac*cos(x)
       rather than as sin(x), cos(x), where epsfac is the average
       value of cos(z) in the range +- eps/2, that is,
       epsfac = 2 sin (eps/2) / eps.   */
    int unit;
} Saux;

typedef struct Pauxst {
    int dummy;
} Paux;

typedef struct Vauxst {
    int dummy;
} Vaux;

/*    ***************  All of Basic should be distributed  */
typedef struct Basicst { /* Basic parameter info about var in class.
            The first few fields are standard and hxst
            appear in the order shown.  */
    int id;              /* The variable number (index)  */
    int signif;
    int infac; /* shows if affected by factor  */
    /********************  Following fields vary according to need ***/
    double bhx, bhy; /* current mean, log hy */
    double nhx, nhy; /* when dad */
    double shx, shy; /* when sansfac */
    double fhx, fhy; /* when confac */
    double bhsprd, nhsprd, shsprd, fhsprd;
    double ld; /* factor load */
    double ldsprd;
    double samplesize;      /* Size of sample on which estimates based */
                            /*    Derived quantities calced in 'adjust'    */
                            /*    For Plain case:    */
    double skappa, rskappa; /* Concentration param and reciprocal */
    double slgi0;           /* Normalization constant */
    double saa;             /* deriv of slgi0 wrt kappa */
    double sdaa;            /* deriv of saa */
    double sfh;             /* one-directional Fisher for hx, hy */
    double sdfh;            /* deriv of sfh */
                            /*    For Factor case:    */
    /*    Note that fields fkappa ... fdfh match skappa ... sdfh  */
    double fkappa, rfkappa; /* Concentration param and reciprocal */
    double flgi0;           /* Normalization constant */
    double faa;             /* deriv of flgi0 wrt kappa */
    double fdaa;            /* deriv of faa */
    double ffh;             /* one-directional Fisher for hx, hy */
    double fdfh;            /* deriv of ffh */
    double fmufish;         /*    Fish wrt mu  */
    double ldsq;            /* Square of load */
} Basic;

typedef struct Statsst { /* Stuff accumulated to revise Basic  */
                         /* First fields are standard  */
    double cnt;          /* Weighted count  */
    double btcost, ntcost, stcost, ftcost;
    double bpcost, npcost, spcost, fpcost;
    double vsq; /*  weighted sum of squared scores for this var */
    int id;     /* Variable number  */
    /********************  Following fields vary according to need ***/
    double parkstcost, parkftcost; /* Unweighted item costs of xn */
    double oldftcost, adj;
    double tssin, tscos; /* Sum of sines and cosines (no factor) */
    double tfsin, tfcos; /* Sum of sines and cosines (with factor) */
    double ldd1, ldd2;
    double fwd2; /* Total coeff of Fmu in item costs */
} Stats;

static Saux *saux;
static Paux *paux;
static Vaux *vaux;
static Basic *cvi, *dcvi;
static Stats *evi;

/*--------------------------  define ------------------------------- */
/*    This routine is used to set up a VarType entry in the global "types"
array.  It is the only function whose name needs to be altered for different
types of variable, and this name hxst be copied into the file "definetypes.c"
when installing a new type of variable. It is also necessary to change the
"Ntypes" constant, and to decide on a type id (an integer) for the new type.
    */

void vonm_define(int typindx) {
    /*    typindx is the index in types[] of this type   */

    VarType *vtp;

    vtp = Types + typindx;
    vtp->id = typindx;
    /*     Set type name as string up to 59 chars  */
    vtp->name = "Von Mises";
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
void set_var(int iv) {
    CurAttr = CurAttrList + iv;
    CurVType = CurAttr->vtype;
    pvi = pvars + iv;
    paux = (Paux *)pvi->paux;
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

/*    --------------------  read_aux_attr  ----------------------------  */
/*      Read in auxiliary info into vaux, return 0 if OK else 1  */
int read_aux_attr(Vaux *vax) { return (0); }

/*    ---------------------  read_aux_smpl ---------------------------   */
/*    To read any auxiliary info about a variable of this type in some
sample.
    */
int read_aux_smpl(Saux *sax) {
    int i;

    /*    Read in auxiliary info into saux, return 0 if OK else 1  */
    i = read_int(&(sax->unit), 1);
    if (i < 0) {
        sax->eps = sax->leps = 0.0;
        return (1);
    }
    i = read_double(&(sax->eps), 1);
    if (i < 0) {
        sax->eps = sax->leps = 0.0;
        return (1);
    }
    if (sax->unit)
        sax->eps *= (pi / 180.0);
    if (sax->eps > 0.01)
        sax->epsfac = 2.0 * sin(0.5 * sax->eps) / sax->eps;
    else
        sax->epsfac = 1.0 - sax->eps * sax->eps / 24.0;
    sax->leps = log(sax->eps);
    return (0);
}

/*    -------------------  read_datum -------------------------------  */
/*    To read a value for this variable type     */
int read_datum(char *loc, int iv) {
    int i;
    int unit;
    double epsfac;
    Datum xn;

    /*    Read datum into xn.xx, return error.  */
    i = read_double(&(xn.xx), 1);
    if (!i) {
        /*    Get the unit code from Saux   */
        unit = ((Saux *)(CurVar->saux))->unit;
        /*    Get quantization effect from Saux  */
        epsfac = ((Saux *)(CurVar->saux))->epsfac;
        if (unit)
            xn.xx *= (pi / 180.0);
        xn.sinxx = epsfac * sin(xn.xx);
        xn.cosxx = epsfac * cos(xn.xx);
        memcpy(loc, &xn, sizeof(Datum));
    }
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
    CurAttr = CurAttrList + iv;
    CurAttr->basic_size = sizeof(Basic);
    CurAttr->stats_size = sizeof(Stats);
    return;
}

/*    ----------------------  set_best_pars --------------------------  */
void set_best_pars(int iv) {

    set_var(iv);

    if (CurClass->type == Dad) {
        cvi->bhx = cvi->nhx;
        cvi->bhy = cvi->nhy;
        cvi->bhsprd = cvi->nhsprd;
        evi->btcost = evi->ntcost;
        evi->bpcost = evi->npcost;
    } else if ((CurClass->use == Fac) && cvi->infac) {
        cvi->bhx = cvi->fhx;
        cvi->bhy = cvi->fhy;
        cvi->bhsprd = cvi->fhsprd;
        evi->btcost = evi->ftcost;
        evi->bpcost = evi->fpcost;
    } else {
        cvi->bhx = cvi->shx;
        cvi->bhy = cvi->shy;
        cvi->bhsprd = cvi->shsprd;
        evi->btcost = evi->stcost;
        evi->bpcost = evi->spcost;
    }
    return;
}

/*    ------------------------  clear_stats  ------------------------  */
/*    Clears stats to accumulate in cost_var, and derives useful functions
of basic params  */
void clear_stats(int iv) {
    set_var(iv);
    evi->cnt = 0.0;
    evi->stcost = evi->ftcost = 0.0;
    evi->vsq = 0.0;
    evi->tssin = evi->tscos = 0.0;
    evi->tfsin = evi->tfcos = 0.0;
    evi->ldd2 = evi->ldd1 = evi->fwd2 = 0.0;

    if (CurClass->age > 0)
        return;

    /*    Set some plausible values for initial pass  */
    if ((CurDad) && (CurClass->age == 0))
        return;
    cvi->shx = cvi->shy = cvi->fhx = cvi->fhy = 0.0;
    cvi->shsprd = cvi->fhsprd = 1.0;
    cvi->ld = cvi->fkappa = 0.0;
    cvi->fmufish = cvi->ldsq = 0.0;
    cvi->slgi0 = cvi->flgi0 = 1.8379; /* about log 2 Pi */
    cvi->ldsq = cvi->ld * cvi->ld;
    return;
}

/*    -------------------------  score_var  ------------------------   */
/*    To eval derivs of a case cost wrt score, scorespread. Adds to
vvd1, vvd2.
    The general form for VonMises message costs has the following
components:
    Density cost:
mc1 = -leps + lgi0 - kap * cos (mu + w - xx)
    where kap^2 = hx^2 + hy^2
        tan (mu) = hx / hy
        t = tan (w/2) = vv * ld

    Round-off costs:
mc2 = hsprd * Fh
mc3 = 0.5 * Fmu * wsprd
    where wsprd = tsprd * r2
        tsprd = vvsq * ldsprd + ldsq * vvsprd
        r2 = dwdt^2
        dwdt (= d_w / d_t) = 2.0 / (1 + t^2)

    Score prior cost are accounted in score_all_vars.
    */
void score_var(int iv) {

    double cosw, sinw, tt, wd1;
    double dwdt, dwdv, r2, dr2dw;

    set_var(iv);
    if (CurAttr->inactive)
        return;

    if (saux->missing)
        return;

    /*    Get t  */
    tt = cvi->ld * cvv;
    /*    tt is tan of w/2.  Use the formulae for cos and sin in terms of
        tan of half-angle.  */
    r2 = 1.0 / (1.0 + tt * tt);
    cosw = (1.0 - tt * tt) * r2; /* (1-t^2) / (1+t^2) */
    dwdt = 2.0 * r2;
    dwdv = dwdt * cvi->ld;      /* d_w / d_v */
    sinw = tt * dwdt;           /*   2t / (1+t^2)  */
    r2 = dwdt * dwdt;           /* Square of (d_w / d_t) */
    dr2dw = -2.0 * dwdt * sinw; /* deriv wrt w of r2 */

    /*    Now to get derivs wrt vv, I first get derivs wrt tt  */
    /*    The mc1 term -kap * cos (mu + w - xx) can be written using
        kap*cosmu = hy, kap*sinmu = hx as:
        mc1t = -{(hy.cx + hx.sx) cos(w) - (hx.cx - hy.sx) sin(w)}
        with deriv wrt w given by:
        wd1 = -{(hy.cx + hx.sx) (-sin(w)) - (hx.cx - hy.sx) cos (w)}
            =  {(hy.cx + hx.sx) sin (w) + (hx.cx - hy.sx) cos (w)}
        */

    wd1 = ((cvi->fhy * saux->xn.cosxx + cvi->fhx * saux->xn.sinxx) * sinw + (cvi->fhx * saux->xn.cosxx - cvi->fhy * saux->xn.sinxx) * cosw);

    /*    The mc3 term 0.5 * Fmu * r2 * (vsq * ldsprd + ldsq * vsprd)
        is treated in two parts, as we don't yet know vsprd. The part in
        (vsq * ldsprd) gives a contibution to wd1 via the r2 factor:  */
    wd1 += 0.5 * cvi->fmufish * cvi->ldsprd * cvvsq * dr2dw;

    /*    This part also directly contributes to vvd1 via the vsq factor: */
    vvd1 += cvi->fmufish * cvi->ldsprd * r2 * cvv;

    /*    This gives a term in mvvd2 :  */
    mvvd2 += cvi->fmufish * cvi->ldsprd * r2;

    /*    The second part, involving vsprd, is treated by accumulating
        in vvd3 the deriv wrt vv of the multiplier of half vsprd.   */
    vvd3 += cvi->fmufish * cvi->ldsq * dr2dw * dwdv;

    /*    The deriv wrt w leads to a deriv wrt t of wd1 * dwdt  */
    /*    and so to deriv wrt vv of:  wd1 * dwdv  */

    vvd1 += wd1 * dwdv;

    /*    Now for contribution to vvd2. This is 2 * the coeff of vsprd in
        the item cost.  */
    vvd2 += cvi->fmufish * cvi->ldsq * r2;
    mvvd2 += cvi->fmufish * cvi->ldsq * 4.0;
    /*        Note, the max value of r2 is 4  */
    return;
}

/*    -----------------------  cost_var  --------------------------   */
/*    Accumulates item cost into scasecost, fcasecost    */
void cost_var(int iv, int fac) {
    double del, cost, tt, tsprd, cosw, sinw, r2;
    set_var(iv);
    if (saux->missing)
        return;
    if (CurClass->age == 0) {
        evi->parkftcost = evi->parkstcost = 0.0;
        return;
    }
    /*    Do no-fac cost first  */
    /*    Get cosine of angle between xn and field  */
    del = saux->xn.cosxx * cvi->shy + saux->xn.sinxx * cvi->shx;
    /*    This is already multiplied by field strength kappa  */
    cost = cvi->slgi0 - del - saux->leps;
    /*    slgi0 contains the roundoff costs from shsprd   */
    evi->parkstcost = cost;
    scasecost += cost;

    /*    Only do faccost if fac  */
    if (!fac)
        goto facdone;
    /*    From density term mc1:    */
    cost = cvi->flgi0 - saux->leps;
    /*    flgi0 already contains the mc2 term hsprd * Fh */

    /*    And we need -kap * cos (mu + w - xx)   */
    tt = cvi->ld * cvv;
    r2 = 1.0 / (1.0 + tt * tt);
    cosw = (1.0 - tt * tt) * r2;
    r2 = 2.0 * r2;
    sinw = tt * r2;
    r2 = r2 * r2;

    cost -= (cvi->fhy * saux->xn.cosxx + cvi->fhx * saux->xn.sinxx) * cosw - (cvi->fhx * saux->xn.cosxx - cvi->fhy * saux->xn.sinxx) * sinw;

    /*    And cost term mc3, depending on tsprd:  */
    tsprd = cvvsq * cvi->ldsprd + cvi->ldsq * cvvsprd;
    cost += 0.5 * cvi->fmufish * tsprd * r2;

facdone:
    fcasecost += cost;
    evi->parkftcost = cost;

    return;
}

/*    --------------------  deriv_var  --------------------------  */
/*    Given the item weight in cwt, calcs derivs of cost wrt basic
params and accumulates in paramd1, paramd2.
Factor derivs done only if fac.  */
void deriv_var(int iv, int fac) {
    double tt, tsprd, r2, cosw, sinw, wtr2, wd1, dwdt, dr2dw;
    double coser, siner;
    set_var(iv);
    if (saux->missing)
        return;
    /*    Do non-fac first  */
    evi->cnt += CurCaseWeight;
    /*    For non-fac, rather than getting derivatives I just collect
        the sufficient statistics, sum of xn.sinxx, xn.cosxx  */
    evi->tssin += CurCaseWeight * saux->xn.sinxx;
    evi->tscos += CurCaseWeight * saux->xn.cosxx;
    /*    Accumulate weighted item cost  */
    evi->stcost += CurCaseWeight * evi->parkstcost;
    evi->ftcost += CurCaseWeight * evi->parkftcost;

    /*    Now for factor form  */
    if (fac) {

        tt = cvi->ld * cvv;
        /*    Hence cos(w), sin(w)  */
        r2 = 1.0 / (1.0 + tt * tt);
        cosw = (1.0 - tt * tt) * r2; /* (1-t^2) / (1+t^2) */
        dwdt = 2.0 * r2;
        sinw = tt * dwdt;           /*   2t / (1+t^2)  */
        r2 = dwdt * dwdt;           /* Square of (d_w / d_t) */
        dr2dw = -2.0 * dwdt * sinw; /* deriv wrt w of r2 */

        /*    First, we get the cos and sin of the "error" angle xx-w for
            accumulation as tfcos, tssin  */

        coser = saux->xn.cosxx * cosw + saux->xn.sinxx * sinw;
        evi->tfcos += CurCaseWeight * coser;
        siner = saux->xn.sinxx * cosw - saux->xn.cosxx * sinw;
        evi->tfsin += CurCaseWeight * siner;

        /*    These should be sufficient to get derivs of mc1 wrt hx, hy,
            also derivs of mc2, but there remains mc3, and load. */

        /*    Cost mc3 = 0.5 * Fmu * tsprd * r2  */
        tsprd = cvvsq * cvi->ldsprd + cvi->ldsq * cvvsprd;
        wtr2 = CurCaseWeight * r2;
        /*    Accumulate wsprd = tsprd * r2  */
        evi->fwd2 += tsprd * wtr2;

        /*    To get deriv wrt ld, deriv of mc1 wrt w is:  */
        wd1 = ((cvi->fhy * saux->xn.cosxx + cvi->fhx * saux->xn.sinxx) * sinw + (cvi->fhx * saux->xn.cosxx - cvi->fhy * saux->xn.sinxx) * cosw);

        /*    The mc3 term 0.5 * Fmu * wsprd = 0.5 * Fmu * tsprd * r2
            gives a further deriv wrt w:    */
        wd1 += 0.5 * cvi->fmufish * tsprd * dr2dw;

        /*    The deriv wrt w leads to a deriv wrt t of wd1 * dwdt  */
        /*    and so to a deriv wrt ld of: (vv * wd1 * dwdt)  */
        evi->ldd1 += CurCaseWeight * cvv * wd1 * dwdt;

        /*    There is also a deriv wrt ld via tsprd.  */
        evi->ldd1 += cvi->fmufish * wtr2 * cvi->ld * cvvsprd;

        /*    Accum as ldd2 twice the multiplier of ldsprd in mc3  */
        evi->ldd2 += 0.5 * cvi->fmufish * r2 * cvvsq;
    }
}

/*    -------------------  adjust  ---------------------------    */
/*    To adjust parameters of a vonmises variable     */
void adjust(int iv, int fac) {
    double adj, temp1, cnt, ldd2;
    double del1, del2, spcost, fpcost;
    double dadhx, dadhy, dhsprd;
    double hxd1, hyd1, hkd1, hkd2;

    set_var(iv);
    adj = InitialAdj;
    cnt = evi->cnt;

    /*    Get prior constants from dad, or if root, fake them  */
    if (!CurDad) { /* Class is root */
        dadhx = dadhy = 0.0;
        dhsprd = NullSprd;
    } else {
        dadhx = dcvi->nhx;
        dadhy = dcvi->nhy;
        dhsprd = dcvi->nhsprd;
    }

    /*    If too few data for this variable, use dad's n-paras  */
    if ((Control & AdjPr) && (cnt < MinSize)) {
        cvi->nhx = cvi->shx = cvi->fhx = dadhx;
        cvi->nhy = cvi->shy = cvi->fhy = dadhy;
        cvi->ld = 0.0;
        cvi->nhsprd = cvi->shsprd = cvi->fhsprd = dhsprd;
        cvi->ldsprd = 1.0;
        cvi->sfh = cvi->ffh = -1.0;
        goto hasage;
    }
    /*    If class age is zero, make some preliminary estimates  */
    if (CurClass->age)
        goto hasage;
    evi->oldftcost = 0.0;
    evi->adj = 1.0;
    cvi->shx = cvi->fhx = evi->tssin / cnt;
    cvi->shy = cvi->fhy = evi->tscos / cnt;
    cvi->shsprd = cvi->fhsprd = 1.0 / cnt;
    cvi->ldsprd = 1.0 / cnt;
    cvi->ld = 0.0;
    cvi->sfh = cvi->ffh = -1.0;
    /*    Make a stab at class tcost  */
    CurClass->cstcost += cnt * (2.0 * hlg2pi - saux->leps + CurClass->mlogab) + 1.0;
    CurClass->cftcost = CurClass->cstcost + 100.0 * cnt;

hasage:
    temp1 = 1.0 / dhsprd;

    /*    Compute parameter costs as they are  */
    del1 = (dadhx - cvi->shx) * (dadhx - cvi->shx) + (dadhy - cvi->shy) * (dadhy - cvi->shy);
    spcost = 2.0 * hlg2pi + log(dhsprd) + 0.5 * temp1 * (del1 + 2.0 * cvi->shsprd) - log(cvi->shsprd) + 2.0 * lattice;

    if (!fac) {
        fpcost = spcost + 100.0;
        cvi->infac = 1;
        goto facdone1;
    }
    del2 = (dadhx - cvi->fhx) * (dadhx - cvi->fhx) + (dadhy - cvi->fhy) * (dadhy - cvi->fhy);
    fpcost = 2.0 * hlg2pi + log(dhsprd) + 0.5 * temp1 * (del2 + 2.0 * cvi->fhsprd) - log(cvi->fhsprd) + 2.0 * lattice;
    /*    The prior for load ld is N (0, 1)  */
    fpcost += hlg2pi + 0.5 * (cvi->ldsq + cvi->ldsprd);
    fpcost -= 0.5 * log(cvi->ldsprd) + lattice;

facdone1:

    /*    Store param costs for this variable  */
    evi->spcost = spcost;
    evi->fpcost = fpcost;
    /*    Add to class param costs  */
    CurClass->nofac_par_cost += spcost;
    CurClass->fac_par_cost += fpcost;
    if (!(Control & AdjPr))
        goto adjdone;
    if (cnt < MinSize)
        goto adjdone;

    /*    Adjust non-fac parameters.   */
    adj = 1.0;
    /*    From things,
        cost = N * (-leps + LI0 + shsprd * Fh) - kap*Sum{cos(mu-x)}

        Must get kap, Sum, LI0, Fh etc  */
    if (cvi->sfh < 0.0)
        kapcode(cvi->shx, cvi->shy, &(cvi->skappa));

    /*    From item cost term - kap*Sum{cos(mu-x)} :  */
    hxd1 = -evi->tssin;
    hyd1 = -evi->tscos;

    /*    From item cost term N * LI0 : */
    /*    Deriv wrt kappa is : */
    hkd1 = cnt * cvi->saa;

    /*    From cost term cnt * hsprd * Fh:   */
    hkd1 += cnt * cvi->shsprd * cvi->sdfh;
    hkd2 = cnt * cvi->sfh;

    /*    Deriv of kappa wrt shx is shx / kappa, so  */
    hxd1 += cvi->shx * hkd1 * cvi->rskappa;
    hyd1 += cvi->shy * hkd1 * cvi->rskappa;

    /*    From prior cost:  */
    hxd1 += (cvi->shx - dadhx) * temp1;
    hyd1 += (cvi->shy - dadhy) * temp1;
    hkd2 += 2.0 * temp1;

    /*    Adjust shx, shy  */
    cvi->shsprd = 1.0 / hkd2;
    if ((cvi->shsprd * cnt) < 1.0)
        cvi->shsprd = 1.0 / cnt;
    cvi->shx -= adj * hxd1 * cvi->shsprd;
    cvi->shy -= adj * hyd1 * cvi->shsprd;
    /*    Recalc kappa and dependents   */
    kapcode(cvi->shx, cvi->shy, &(cvi->skappa));
    /*    Store the constant part of cost (sli0 + shsprd*Fh) in cvi->slgi0 */
    cvi->slgi0 += cvi->shsprd * cvi->sfh;
    if (!fac) { /* Set factor params from plain  */
        cvi->fhx = cvi->shx;
        cvi->fhy = cvi->shy;
        cvi->flgi0 = cvi->slgi0;
        cvi->ld = cvi->ldsq = 0.0;
        cvi->fmufish = cvi->skappa * cvi->saa;
        cvi->fkappa = cvi->skappa;
        cvi->fhsprd = cvi->shsprd;
        cvi->ldsprd = 1.0 / cnt;
        cvi->ffh = -1.0;
        goto facdone2;
    }

    /*    Adjust factor parameters   */
    /*    From things,
        cost = N * (-leps + LI0 - fhsprd * Fh) - kap*Sum{cos(mu-x)}
            + 0.5 * Fmu * Sum { wsprd }
        where:
            x = datum - atan (cvv * ld),  Fmu = kappa * aa
            wsprd = (cvvsprd * ldsq + ldsprd * cvvsq) * (dwdt)^2
            w = atan (cvv * ld),
            Sum {wsprd} is in evi->fwd2

        Must get kap, Sum, LI0, Fh etc  */
    if (cvi->ffh < 0.0)
        kapcode(cvi->fhx, cvi->fhy, &(cvi->fkappa));

    /*    From item cost term - kap*Sum{cos(mu-x)} :  */
    hxd1 = -evi->tfsin;
    hyd1 = -evi->tfcos;

    /*    From item cost term N * LI0 : */
    /*    Deriv wrt kappa is : */
    hkd1 = cnt * cvi->faa;

    /*    From cost term cnt * hsprd * Fh:   */
    hkd1 += cnt * cvi->fhsprd * cvi->fdfh;
    hkd2 = cnt * cvi->ffh;

    /*    We have Sum {wsprd} in evi->fwd2, so cost term 0.5 * Fmu * wsprd gives:
     */
    hkd1 += 0.5 * (cvi->faa + cvi->fkappa * cvi->fdaa) * evi->fwd2;

    /*    Deriv of kappa wrt fhx is fhx / kappa, so  */
    hxd1 += cvi->fhx * hkd1 * cvi->rfkappa;
    hyd1 += cvi->fhy * hkd1 * cvi->rfkappa;

    /*    Have evi->ldd1, evi->ldd2  */
    ldd2 = evi->ldd2;

    /*    From prior cost:  */
    hxd1 += (cvi->fhx - dadhx) * temp1;
    hyd1 += (cvi->fhy - dadhy) * temp1;
    hkd2 += 2.0 * temp1;
    evi->ldd1 += cvi->ld;
    ldd2 += 1.0;

    /*     Adjust fhx, fhy, ld  */
    del1 = evi->ftcost / cnt;
    adj = evi->adj * 1.1;
    if (adj > 1.25)
        adj = 1.25;
    if (del1 > evi->oldftcost)
        adj = 0.5 * adj;
    if (adj < InitialAdj)
        adj = InitialAdj;
    evi->adj = adj;
    evi->oldftcost = del1;
    cvi->fhsprd = 1.0 / hkd2;
    if ((cvi->fhsprd * cnt) < 1.0)
        cvi->fhsprd = 1.0 / cnt;
    cvi->fhx -= adj * hxd1 * cvi->fhsprd;
    cvi->fhy -= adj * hyd1 * cvi->fhsprd;
    cvi->ld -= adj * evi->ldd1 / ldd2;
    cvi->ldsprd = 1.0 / ldd2;

facdone2:
    /*    Adjust derived quantities  */
    kapcode(cvi->fhx, cvi->fhy, &(cvi->fkappa));
    cvi->flgi0 += cvi->fhsprd * cvi->ffh;
    cvi->fmufish = cvi->fkappa * cvi->faa;
    cvi->ldsq = cvi->ld * cvi->ld;

    /*    Adjust as-dad params, but if no sons, set from nonfac params */
    if (CurClass->num_sons < 2) {
        cvi->nhx = cvi->shx;
        cvi->nhy = cvi->shy;
        cvi->nhsprd = cvi->shsprd;
    }
    cvi->samplesize = evi->cnt;

adjdone:
    return;
}

/*    ------------------------  show  -----------------------   */
void show(Class *ccl, int iv) {
    double mu, kappa;

    set_class(ccl);
    set_var(iv);

    printf("V%3d  Cnt%6.1f  %s  Adj%6.3f\n", iv + 1, evi->cnt, (cvi->infac) ? " In" : "Out", evi->adj);
    if (CurClass->num_sons < 2)
        goto skipn;
    printf(" N: Cost%8.1f  Hx%8.3f  Hy%8.3f+-%8.3f\n", evi->npcost, cvi->nhx, cvi->nhy, sqrt(cvi->nhsprd));
skipn:
    printf(" S: Cost%8.1f  Hx%8.3f  Hy%8.3f+-%8.3f\n", evi->spcost + evi->stcost, cvi->shx, cvi->shy, sqrt(cvi->shsprd));
    printf(" F: Cost%8.1f  Hx%8.3f  Hy%8.3f  Ld%8.3f +-%5.2f\n", evi->fpcost + evi->ftcost, cvi->fhx, cvi->fhy, cvi->ld, sqrt(cvi->ldsprd));
    kappa = sqrt(cvi->bhx * cvi->bhx + cvi->bhy * cvi->bhy);
    mu = atan2(cvi->bhx, cvi->bhy);
    printf(" B:  Mean ");
    if (saux->unit)
        printf("%6.1f deg", (180.0 / pi) * mu);
    else
        printf("%6.3f rad", mu);
    printf("  Kappa %8.2f\n", kappa);
    return;
}

/*    ----------------------  cost_var_nonleaf  ------------------------   */
/*    To compute parameter cost for non-leaf (intrnl) class use   */

/*    The coding in an internal class of a simple scalar parameter such as
hx or hy is based on Normal priors. Let the parameter be "t". Every class
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

    Differentiating wrt t gives an explicit optihxm

    t = (sp*Sum_n{tn} + s * tp) / (N*sp + s)     !!!!!!!!!!!!

    Differentiating wrt t gives the equation for optihxm s:

    (1 + 0.5/N)/sp + (N/2 + M - 3/2)/s - (V/2 + S)/s^2  = 0
   or    s^2*(1 + 0.5/N)/sp  +  s*(N/2 + M - 3/2)  -  (V/2 + S) = 0
        which is quadratic in s.

Writing the quadratic as    a*s^2 + b*s -c = 0,   we want the root

    s = 2*c / { b + sqrt (b^2 + 4ac) }.  This is the form used in code.

    */

void cost_var_nonleaf(iv, vald) int iv, vald;
{
    Basic *soncvi;
    Class *son;
    double pcost;
    double nhx, nhy, nhsprd, dadhx, dadhy, dadhsprd;
    double del, co0, co1, co2, meanx, meany;
    double tsxn, tsyn, tsvn, tssn, sbhx, sbhy, sbhsprd;
    int nints, nson, ison, n;

    set_var(iv);
    if (!vald) { /* Cannot define as-dad params, so fake it */
        evi->npcost = 0.0;
        cvi->nhx = cvi->shx;
        cvi->nhy = cvi->shy;
        cvi->nhsprd = cvi->shsprd * evi->cnt;
        return;
    }
    if (CurAttr->inactive) {
        evi->npcost = evi->ntcost = 0.0;
        return;
    }
    nson = CurClass->num_sons;

    /*    There are two independent parameters, nhx and nhy, to fiddle.
        However, for vonmises variables, we use a single spread value
        for both, in both internal and leaf or sub classes.  */

    /*    Get prior constants from dad, or if root, fake them  */
    if (!CurDad) { /* Class is root */
        dadhx = dadhy = 0.0;
        dadhsprd = NullSprd;
    } else {
        dadhx = dcvi->nhx;
        dadhy = dcvi->nhy;
        dadhsprd = dcvi->nhsprd;
    }
    nhx = cvi->nhx;
    nhy = cvi->nhy;
    nhsprd = cvi->nhsprd;

    /*    We need to accumulate things over sons. We need:   */
    nints = 0;  /* Number of internal sons (M) */
    tsxn = 0.0; /* Total sons' hx */
    tsyn = 0.0; /* Total sons' hy */
    tsvn = 0.0; /* Total sum of sons' (hx_n^2 + hy_n^2 + del_n) */
    tssn = 0.0; /* Total sons' s_n */

    for (ison = CurClass->son_id; ison > 0; ison = son->sib_id) {
        son = CurPopln->classes[ison];
        soncvi = (Basic *)son->basics[iv];
        sbhx = soncvi->bhx;
        sbhy = soncvi->bhy;
        sbhsprd = soncvi->bhsprd;
        tsxn += sbhx;
        tsyn += sbhy;
        tsvn += sbhx * sbhx + sbhy * sbhy;
        if (son->type == Dad) { /* used as parent */
            nints++;
            tssn += sbhsprd;
            tsvn += sbhsprd / son->num_sons;
        } else
            tsvn += sbhsprd;
    }
    /*    Calc coeffs for quadratic c_2 * s^2 + c_1 * s - c_0  */
    co2 = (1.0 + 0.5 / nson) / dadhsprd;
    co1 = nints + nson - 2;
    /*    Can now compute V in the above comments.  tssn = S of the comments */
    /*    First we get the V around the sons' mean, then update nhx, nhy
        and correct the value of V  */
    meanx = tsxn / nson;
    meany = tsyn / nson;
    /*    Means of sons' params.  */
    tsvn -= meanx * tsxn + meany * tsyn;
    /*      Variance around mean  */
    if (!(Control & AdjPr))
        goto adjdone;

    /*    Iterate the adjustment of params, spread  */
    n = 5;
adjloop:
    /*    Update params  */
    nhx = (dadhsprd * tsxn + nhsprd * dadhx) / (nson * dadhsprd + nhsprd);
    nhy = (dadhsprd * tsyn + nhsprd * dadhy) / (nson * dadhsprd + nhsprd);
    del = (meanx - nhx) * (meanx - nhx) + (meany - nhy) * (meany - nhy);
    /*    The V of comments is tsvn + nson * del */
    co0 = 0.5 * (tsvn + nson * del) + tssn;
    /*    Solve for new spread  */
    nhsprd = 2.0 * co0 / (co1 + sqrt(co1 * co1 + 4.0 * co0 * co2));
    n--;
    if (n)
        goto adjloop;
    cvi->nhx = nhx;
    cvi->nhy = nhy;
    cvi->nhsprd = nhsprd;

adjdone: /*    Calc cost  */
    del = (nhx - dadhx) * (nhx - dadhx) + (nhy - dadhy) * (nhy - dadhy);
    pcost = 2.0 * hlg2pi + 2.0 * log(dadhsprd) + (0.5 * del + nhsprd / nson) / dadhsprd + nhsprd / dadhsprd;
    /*    Add hlog Fisher, lattice  */
    pcost += 0.5 * log(nson * nson * (nson + nints)) - 2.0 * log(nhsprd + 1.0e-8) + 3.0 * lattice;

    /*    Add roundoff for 3 params (nhx, nhy, nhsprd)  */
    pcost += 1.5;

    evi->npcost = pcost;

    return;
}

/*    -------------- tables and kapcode ------------------ */

/*    Table of log of the integral from -Pi to Pi of

      exp (kappa * cos theta) . dtheta

    There are 2501 values tabulated, for kappa from 0 to 50 in
steps of 0.02   */
/*    The following define the tabulation interval and highest
    value of kappa for this table (lgi0) and the following table
    (atab)  */

#define kapstep ((double)0.02)
#define kscale ((double)50.0) /* Must be reciprocal of kapstep */
#define hikap ((double)50.0)
#define maxik ((int)2500) /* Must be hikap*kscale */

#define twopi ((double)6.283185306)

static double lgi0[] = {
    /* LogI0 table */
    1.837877066409,  /*   0.00 */
    1.837977063909,  /*   0.02 */
    1.838277026416,  /*   0.04 */
    1.838776863990,  /*   0.06 */
    1.839476426864,  /*   0.08 */
    1.840375505643,  /*   0.10 */
    1.841473831584,  /*   0.12 */
    1.842771076949,  /*   0.14 */
    1.844266855441,  /*   0.16 */
    1.845960722713,  /*   0.18 */
    1.847852176951,  /*   0.20 */
    1.849940659530,  /*   0.22 */
    1.852225555741,  /*   0.24 */
    1.854706195591,  /*   0.26 */
    1.857381854660,  /*   0.28 */
    1.860251755031,  /*   0.30 */
    1.863315066281,  /*   0.32 */
    1.866570906524,  /*   0.34 */
    1.870018343520,  /*   0.36 */
    1.873656395830,  /*   0.38 */
    1.877484034023,  /*   0.40 */
    1.881500181936,  /*   0.42 */
    1.885703717966,  /*   0.44 */
    1.890093476415,  /*   0.46 */
    1.894668248867,  /*   0.48 */
    1.899426785595,  /*   0.50 */
    1.904367797008,  /*   0.52 */
    1.909489955118,  /*   0.54 */
    1.914791895032,  /*   0.56 */
    1.920272216468,  /*   0.58 */
    1.925929485284,  /*   0.60 */
    1.931762235024,  /*   0.62 */
    1.937768968470,  /*   0.64 */
    1.943948159208,  /*   0.66 */
    1.950298253194,  /*   0.68 */
    1.956817670320,  /*   0.70 */
    1.963504805981,  /*   0.72 */
    1.970358032635,  /*   0.74 */
    1.977375701361,  /*   0.76 */
    1.984556143394,  /*   0.78 */
    1.991897671666,  /*   0.80 */
    1.999398582311,  /*   0.82 */
    2.007057156170,  /*   0.84 */
    2.014871660267,  /*   0.86 */
    2.022840349262,  /*   0.88 */
    2.030961466889,  /*   0.90 */
    2.039233247356,  /*   0.92 */
    2.047653916728,  /*   0.94 */
    2.056221694278,  /*   0.96 */
    2.064934793803,  /*   0.98 */
    2.073791424917,  /*   1.00 */
    2.082789794301,  /*   1.02 */
    2.091928106931,  /*   1.04 */
    2.101204567258,  /*   1.06 */
    2.110617380364,  /*   1.08 */
    2.120164753074,  /*   1.10 */
    2.129844895036,  /*   1.12 */
    2.139656019762,  /*   1.14 */
    2.149596345627,  /*   1.16 */
    2.159664096842,  /*   1.18 */
    2.169857504372,  /*   1.20 */
    2.180174806833,  /*   1.22 */
    2.190614251338,  /*   1.24 */
    2.201174094314,  /*   1.26 */
    2.211852602275,  /*   1.28 */
    2.222648052560,  /*   1.30 */
    2.233558734037,  /*   1.32 */
    2.244582947766,  /*   1.34 */
    2.255719007623,  /*   1.36 */
    2.266965240902,  /*   1.38 */
    2.278319988864,  /*   1.40 */
    2.289781607264,  /*   1.42 */
    2.301348466840,  /*   1.44 */
    2.313018953772,  /*   1.46 */
    2.324791470100,  /*   1.48 */
    2.336664434125,  /*   1.50 */
    2.348636280763,  /*   1.52 */
    2.360705461885,  /*   1.54 */
    2.372870446613,  /*   1.56 */
    2.385129721600,  /*   1.58 */
    2.397481791277,  /*   1.60 */
    2.409925178075,  /*   1.62 */
    2.422458422617,  /*   1.64 */
    2.435080083895,  /*   1.66 */
    2.447788739416,  /*   1.68 */
    2.460582985325,  /*   1.70 */
    2.473461436510,  /*   1.72 */
    2.486422726683,  /*   1.74 */
    2.499465508443,  /*   1.76 */
    2.512588453319,  /*   1.78 */
    2.525790251792,  /*   1.80 */
    2.539069613303,  /*   1.82 */
    2.552425266246,  /*   1.84 */
    2.565855957934,  /*   1.86 */
    2.579360454569,  /*   1.88 */
    2.592937541175,  /*   1.90 */
    2.606586021538,  /*   1.92 */
    2.620304718119,  /*   1.94 */
    2.634092471964,  /*   1.96 */
    2.647948142594,  /*   1.98 */
    2.661870607892,  /*   2.00 */
    2.675858763978,  /*   2.02 */
    2.689911525071,  /*   2.04 */
    2.704027823344,  /*   2.06 */
    2.718206608778,  /*   2.08 */
    2.732446848994,  /*   2.10 */
    2.746747529090,  /*   2.12 */
    2.761107651471,  /*   2.14 */
    2.775526235661,  /*   2.16 */
    2.790002318129,  /*   2.18 */
    2.804534952091,  /*   2.20 */
    2.819123207319,  /*   2.22 */
    2.833766169943,  /*   2.24 */
    2.848462942252,  /*   2.26 */
    2.863212642482,  /*   2.28 */
    2.878014404617,  /*   2.30 */
    2.892867378170,  /*   2.32 */
    2.907770727980,  /*   2.34 */
    2.922723633990,  /*   2.36 */
    2.937725291037,  /*   2.38 */
    2.952774908632,  /*   2.40 */
    2.967871710746,  /*   2.42 */
    2.983014935588,  /*   2.44 */
    2.998203835390,  /*   2.46 */
    3.013437676189,  /*   2.48 */
    3.028715737605,  /*   2.50 */
    3.044037312628,  /*   2.52 */
    3.059401707398,  /*   2.54 */
    3.074808240987,  /*   2.56 */
    3.090256245190,  /*   2.58 */
    3.105745064301,  /*   2.60 */
    3.121274054908,  /*   2.62 */
    3.136842585680,  /*   2.64 */
    3.152450037151,  /*   2.66 */
    3.168095801518,  /*   2.68 */
    3.183779282432,  /*   2.70 */
    3.199499894793,  /*   2.72 */
    3.215257064546,  /*   2.74 */
    3.231050228482,  /*   2.76 */
    3.246878834038,  /*   2.78 */
    3.262742339103,  /*   2.80 */
    3.278640211818,  /*   2.82 */
    3.294571930392,  /*   2.84 */
    3.310536982907,  /*   2.86 */
    3.326534867133,  /*   2.88 */
    3.342565090343,  /*   2.90 */
    3.358627169131,  /*   2.92 */
    3.374720629234,  /*   2.94 */
    3.390845005352,  /*   2.96 */
    3.406999840978,  /*   2.98 */
    3.423184688223,  /*   3.00 */
    3.439399107648,  /*   3.02 */
    3.455642668100,  /*   3.04 */
    3.471914946543,  /*   3.06 */
    3.488215527903,  /*   3.08 */
    3.504544004907,  /*   3.10 */
    3.520899977924,  /*   3.12 */
    3.537283054820,  /*   3.14 */
    3.553692850798,  /*   3.16 */
    3.570128988258,  /*   3.18 */
    3.586591096648,  /*   3.20 */
    3.603078812321,  /*   3.22 */
    3.619591778398,  /*   3.24 */
    3.636129644627,  /*   3.26 */
    3.652692067251,  /*   3.28 */
    3.669278708871,  /*   3.30 */
    3.685889238324,  /*   3.32 */
    3.702523330547,  /*   3.34 */
    3.719180666458,  /*   3.36 */
    3.735860932828,  /*   3.38 */
    3.752563822166,  /*   3.40 */
    3.769289032599,  /*   3.42 */
    3.786036267756,  /*   3.44 */
    3.802805236653,  /*   3.46 */
    3.819595653586,  /*   3.48 */
    3.836407238018,  /*   3.50 */
    3.853239714477,  /*   3.52 */
    3.870092812445,  /*   3.54 */
    3.886966266263,  /*   3.56 */
    3.903859815025,  /*   3.58 */
    3.920773202482,  /*   3.60 */
    3.937706176949,  /*   3.62 */
    3.954658491202,  /*   3.64 */
    3.971629902396,  /*   3.66 */
    3.988620171969,  /*   3.68 */
    4.005629065553,  /*   3.70 */
    4.022656352891,  /*   3.72 */
    4.039701807749,  /*   3.74 */
    4.056765207838,  /*   3.76 */
    4.073846334724,  /*   3.78 */
    4.090944973758,  /*   3.80 */
    4.108060913994,  /*   3.82 */
    4.125193948111,  /*   3.84 */
    4.142343872343,  /*   3.86 */
    4.159510486402,  /*   3.88 */
    4.176693593412,  /*   3.90 */
    4.193892999832,  /*   3.92 */
    4.211108515394,  /*   3.94 */
    4.228339953035,  /*   3.96 */
    4.245587128828,  /*   3.98 */
    4.262849861925,  /*   4.00 */
    4.280127974486,  /*   4.02 */
    4.297421291626,  /*   4.04 */
    4.314729641350,  /*   4.06 */
    4.332052854497,  /*   4.08 */
    4.349390764680,  /*   4.10 */
    4.366743208236,  /*   4.12 */
    4.384110024165,  /*   4.14 */
    4.401491054078,  /*   4.16 */
    4.418886142147,  /*   4.18 */
    4.436295135053,  /*   4.20 */
    4.453717881934,  /*   4.22 */
    4.471154234339,  /*   4.24 */
    4.488604046177,  /*   4.26 */
    4.506067173672,  /*   4.28 */
    4.523543475319,  /*   4.30 */
    4.541032811834,  /*   4.32 */
    4.558535046116,  /*   4.34 */
    4.576050043201,  /*   4.36 */
    4.593577670218,  /*   4.38 */
    4.611117796355,  /*   4.40 */
    4.628670292812,  /*   4.42 */
    4.646235032764,  /*   4.44 */
    4.663811891325,  /*   4.46 */
    4.681400745506,  /*   4.48 */
    4.699001474184,  /*   4.50 */
    4.716613958061,  /*   4.52 */
    4.734238079631,  /*   4.54 */
    4.751873723145,  /*   4.56 */
    4.769520774581,  /*   4.58 */
    4.787179121605,  /*   4.60 */
    4.804848653543,  /*   4.62 */
    4.822529261349,  /*   4.64 */
    4.840220837575,  /*   4.66 */
    4.857923276337,  /*   4.68 */
    4.875636473291,  /*   4.70 */
    4.893360325601,  /*   4.72 */
    4.911094731911,  /*   4.74 */
    4.928839592318,  /*   4.76 */
    4.946594808345,  /*   4.78 */
    4.964360282916,  /*   4.80 */
    4.982135920327,  /*   4.82 */
    4.999921626223,  /*   4.84 */
    5.017717307573,  /*   4.86 */
    5.035522872647,  /*   4.88 */
    5.053338230989,  /*   4.90 */
    5.071163293396,  /*   4.92 */
    5.088997971897,  /*   4.94 */
    5.106842179728,  /*   4.96 */
    5.124695831310,  /*   4.98 */
    5.142558842232,  /*   5.00 */
    5.160431129224,  /*   5.02 */
    5.178312610143,  /*   5.04 */
    5.196203203948,  /*   5.06 */
    5.214102830683,  /*   5.08 */
    5.232011411458,  /*   5.10 */
    5.249928868428,  /*   5.12 */
    5.267855124778,  /*   5.14 */
    5.285790104702,  /*   5.16 */
    5.303733733390,  /*   5.18 */
    5.321685937003,  /*   5.20 */
    5.339646642665,  /*   5.22 */
    5.357615778440,  /*   5.24 */
    5.375593273320,  /*   5.26 */
    5.393579057205,  /*   5.28 */
    5.411573060893,  /*   5.30 */
    5.429575216058,  /*   5.32 */
    5.447585455243,  /*   5.34 */
    5.465603711838,  /*   5.36 */
    5.483629920072,  /*   5.38 */
    5.501664014993,  /*   5.40 */
    5.519705932460,  /*   5.42 */
    5.537755609126,  /*   5.44 */
    5.555812982426,  /*   5.46 */
    5.573877990564,  /*   5.48 */
    5.591950572500,  /*   5.50 */
    5.610030667939,  /*   5.52 */
    5.628118217316,  /*   5.54 */
    5.646213161788,  /*   5.56 */
    5.664315443221,  /*   5.58 */
    5.682425004174,  /*   5.60 */
    5.700541787896,  /*   5.62 */
    5.718665738309,  /*   5.64 */
    5.736796800000,  /*   5.66 */
    5.754934918206,  /*   5.68 */
    5.773080038813,  /*   5.70 */
    5.791232108335,  /*   5.72 */
    5.809391073911,  /*   5.74 */
    5.827556883294,  /*   5.76 */
    5.845729484839,  /*   5.78 */
    5.863908827498,  /*   5.80 */
    5.882094860806,  /*   5.82 */
    5.900287534873,  /*   5.84 */
    5.918486800381,  /*   5.86 */
    5.936692608566,  /*   5.88 */
    5.954904911216,  /*   5.90 */
    5.973123660660,  /*   5.92 */
    5.991348809763,  /*   5.94 */
    6.009580311912,  /*   5.96 */
    6.027818121014,  /*   5.98 */
    6.046062191485,  /*   6.00 */
    6.064312478244,  /*   6.02 */
    6.082568936703,  /*   6.04 */
    6.100831522764,  /*   6.06 */
    6.119100192808,  /*   6.08 */
    6.137374903688,  /*   6.10 */
    6.155655612725,  /*   6.12 */
    6.173942277700,  /*   6.14 */
    6.192234856845,  /*   6.16 */
    6.210533308839,  /*   6.18 */
    6.228837592801,  /*   6.20 */
    6.247147668283,  /*   6.22 */
    6.265463495267,  /*   6.24 */
    6.283785034151,  /*   6.26 */
    6.302112245753,  /*   6.28 */
    6.320445091297,  /*   6.30 */
    6.338783532414,  /*   6.32 */
    6.357127531129,  /*   6.34 */
    6.375477049861,  /*   6.36 */
    6.393832051418,  /*   6.38 */
    6.412192498985,  /*   6.40 */
    6.430558356128,  /*   6.42 */
    6.448929586779,  /*   6.44 */
    6.467306155240,  /*   6.46 */
    6.485688026171,  /*   6.48 */
    6.504075164590,  /*   6.50 */
    6.522467535864,  /*   6.52 */
    6.540865105708,  /*   6.54 */
    6.559267840178,  /*   6.56 */
    6.577675705667,  /*   6.58 */
    6.596088668900,  /*   6.60 */
    6.614506696931,  /*   6.62 */
    6.632929757136,  /*   6.64 */
    6.651357817212,  /*   6.66 */
    6.669790845171,  /*   6.68 */
    6.688228809335,  /*   6.70 */
    6.706671678334,  /*   6.72 */
    6.725119421099,  /*   6.74 */
    6.743572006862,  /*   6.76 */
    6.762029405149,  /*   6.78 */
    6.780491585777,  /*   6.80 */
    6.798958518852,  /*   6.82 */
    6.817430174761,  /*   6.84 */
    6.835906524173,  /*   6.86 */
    6.854387538033,  /*   6.88 */
    6.872873187559,  /*   6.90 */
    6.891363444239,  /*   6.92 */
    6.909858279827,  /*   6.94 */
    6.928357666338,  /*   6.96 */
    6.946861576049,  /*   6.98 */
    6.965369981492,  /*   7.00 */
    6.983882855451,  /*   7.02 */
    7.002400170962,  /*   7.04 */
    7.020921901306,  /*   7.06 */
    7.039448020008,  /*   7.08 */
    7.057978500834,  /*   7.10 */
    7.076513317789,  /*   7.12 */
    7.095052445109,  /*   7.14 */
    7.113595857267,  /*   7.16 */
    7.132143528960,  /*   7.18 */
    7.150695435117,  /*   7.20 */
    7.169251550884,  /*   7.22 */
    7.187811851635,  /*   7.24 */
    7.206376312956,  /*   7.26 */
    7.224944910653,  /*   7.28 */
    7.243517620742,  /*   7.30 */
    7.262094419452,  /*   7.32 */
    7.280675283220,  /*   7.34 */
    7.299260188686,  /*   7.36 */
    7.317849112695,  /*   7.38 */
    7.336442032294,  /*   7.40 */
    7.355038924726,  /*   7.42 */
    7.373639767431,  /*   7.44 */
    7.392244538044,  /*   7.46 */
    7.410853214390,  /*   7.48 */
    7.429465774485,  /*   7.50 */
    7.448082196529,  /*   7.52 */
    7.466702458912,  /*   7.54 */
    7.485326540202,  /*   7.56 */
    7.503954419152,  /*   7.58 */
    7.522586074691,  /*   7.60 */
    7.541221485925,  /*   7.62 */
    7.559860632136,  /*   7.64 */
    7.578503492778,  /*   7.66 */
    7.597150047476,  /*   7.68 */
    7.615800276025,  /*   7.70 */
    7.634454158384,  /*   7.72 */
    7.653111674681,  /*   7.74 */
    7.671772805205,  /*   7.76 */
    7.690437530407,  /*   7.78 */
    7.709105830897,  /*   7.80 */
    7.727777687445,  /*   7.82 */
    7.746453080975,  /*   7.84 */
    7.765131992568,  /*   7.86 */
    7.783814403455,  /*   7.88 */
    7.802500295021,  /*   7.90 */
    7.821189648799,  /*   7.92 */
    7.839882446470,  /*   7.94 */
    7.858578669861,  /*   7.96 */
    7.877278300945,  /*   7.98 */
    7.895981321837,  /*   8.00 */
    7.914687714795,  /*   8.02 */
    7.933397462216,  /*   8.04 */
    7.952110546637,  /*   8.06 */
    7.970826950729,  /*   8.08 */
    7.989546657304,  /*   8.10 */
    8.008269649304,  /*   8.12 */
    8.026995909805,  /*   8.14 */
    8.045725422016,  /*   8.16 */
    8.064458169274,  /*   8.18 */
    8.083194135046,  /*   8.20 */
    8.101933302928,  /*   8.22 */
    8.120675656638,  /*   8.24 */
    8.139421180023,  /*   8.26 */
    8.158169857051,  /*   8.28 */
    8.176921671814,  /*   8.30 */
    8.195676608523,  /*   8.32 */
    8.214434651511,  /*   8.34 */
    8.233195785228,  /*   8.36 */
    8.251959994243,  /*   8.38 */
    8.270727263238,  /*   8.40 */
    8.289497577013,  /*   8.42 */
    8.308270920482,  /*   8.44 */
    8.327047278669,  /*   8.46 */
    8.345826636711,  /*   8.48 */
    8.364608979857,  /*   8.50 */
    8.383394293464,  /*   8.52 */
    8.402182562995,  /*   8.54 */
    8.420973774025,  /*   8.56 */
    8.439767912231,  /*   8.58 */
    8.458564963396,  /*   8.60 */
    8.477364913409,  /*   8.62 */
    8.496167748260,  /*   8.64 */
    8.514973454042,  /*   8.66 */
    8.533782016948,  /*   8.68 */
    8.552593423271,  /*   8.70 */
    8.571407659405,  /*   8.72 */
    8.590224711840,  /*   8.74 */
    8.609044567165,  /*   8.76 */
    8.627867212062,  /*   8.78 */
    8.646692633313,  /*   8.80 */
    8.665520817789,  /*   8.82 */
    8.684351752460,  /*   8.84 */
    8.703185424383,  /*   8.86 */
    8.722021820711,  /*   8.88 */
    8.740860928687,  /*   8.90 */
    8.759702735641,  /*   8.92 */
    8.778547228997,  /*   8.94 */
    8.797394396262,  /*   8.96 */
    8.816244225035,  /*   8.98 */
    8.835096702998,  /*   9.00 */
    8.853951817922,  /*   9.02 */
    8.872809557660,  /*   9.04 */
    8.891669910151,  /*   9.06 */
    8.910532863418,  /*   9.08 */
    8.929398405565,  /*   9.10 */
    8.948266524778,  /*   9.12 */
    8.967137209326,  /*   9.14 */
    8.986010447556,  /*   9.16 */
    9.004886227896,  /*   9.18 */
    9.023764538852,  /*   9.20 */
    9.042645369011,  /*   9.22 */
    9.061528707033,  /*   9.24 */
    9.080414541659,  /*   9.26 */
    9.099302861702,  /*   9.28 */
    9.118193656055,  /*   9.30 */
    9.137086913681,  /*   9.32 */
    9.155982623621,  /*   9.34 */
    9.174880774988,  /*   9.36 */
    9.193781356965,  /*   9.38 */
    9.212684358812,  /*   9.40 */
    9.231589769857,  /*   9.42 */
    9.250497579499,  /*   9.44 */
    9.269407777208,  /*   9.46 */
    9.288320352525,  /*   9.48 */
    9.307235295057,  /*   9.50 */
    9.326152594480,  /*   9.52 */
    9.345072240539,  /*   9.54 */
    9.363994223046,  /*   9.56 */
    9.382918531879,  /*   9.58 */
    9.401845156980,  /*   9.60 */
    9.420774088361,  /*   9.62 */
    9.439705316094,  /*   9.64 */
    9.458638830318,  /*   9.66 */
    9.477574621235,  /*   9.68 */
    9.496512679111,  /*   9.70 */
    9.515452994272,  /*   9.72 */
    9.534395557109,  /*   9.74 */
    9.553340358073,  /*   9.76 */
    9.572287387676,  /*   9.78 */
    9.591236636491,  /*   9.80 */
    9.610188095151,  /*   9.82 */
    9.629141754348,  /*   9.84 */
    9.648097604832,  /*   9.86 */
    9.667055637415,  /*   9.88 */
    9.686015842962,  /*   9.90 */
    9.704978212399,  /*   9.92 */
    9.723942736707,  /*   9.94 */
    9.742909406926,  /*   9.96 */
    9.761878214150,  /*   9.98 */
    9.780849149528,  /*  10.00 */
    9.799822204266,  /*  10.02 */
    9.818797369622,  /*  10.04 */
    9.837774636913,  /*  10.06 */
    9.856753997503,  /*  10.08 */
    9.875735442816,  /*  10.10 */
    9.894718964323,  /*  10.12 */
    9.913704553552,  /*  10.14 */
    9.932692202081,  /*  10.16 */
    9.951681901539,  /*  10.18 */
    9.970673643607,  /*  10.20 */
    9.989667420016,  /*  10.22 */
    10.008663222548, /*  10.24 */
    10.027661043035, /*  10.26 */
    10.046660873359, /*  10.28 */
    10.065662705448, /*  10.30 */
    10.084666531283, /*  10.32 */
    10.103672342890, /*  10.34 */
    10.122680132345, /*  10.36 */
    10.141689891770, /*  10.38 */
    10.160701613335, /*  10.40 */
    10.179715289256, /*  10.42 */
    10.198730911797, /*  10.44 */
    10.217748473266, /*  10.46 */
    10.236767966019, /*  10.48 */
    10.255789382454, /*  10.50 */
    10.274812715016, /*  10.52 */
    10.293837956196, /*  10.54 */
    10.312865098527, /*  10.56 */
    10.331894134586, /*  10.58 */
    10.350925056994, /*  10.60 */
    10.369957858415, /*  10.62 */
    10.388992531558, /*  10.64 */
    10.408029069170, /*  10.66 */
    10.427067464044, /*  10.68 */
    10.446107709014, /*  10.70 */
    10.465149796955, /*  10.72 */
    10.484193720783, /*  10.74 */
    10.503239473455, /*  10.76 */
    10.522287047970, /*  10.78 */
    10.541336437366, /*  10.80 */
    10.560387634720, /*  10.82 */
    10.579440633150, /*  10.84 */
    10.598495425814, /*  10.86 */
    10.617552005907, /*  10.88 */
    10.636610366665, /*  10.90 */
    10.655670501361, /*  10.92 */
    10.674732403306, /*  10.94 */
    10.693796065850, /*  10.96 */
    10.712861482380, /*  10.98 */
    10.731928646320, /*  11.00 */
    10.750997551131, /*  11.02 */
    10.770068190311, /*  11.04 */
    10.789140557395, /*  11.06 */
    10.808214645954, /*  11.08 */
    10.827290449594, /*  11.10 */
    10.846367961957, /*  11.12 */
    10.865447176721, /*  11.14 */
    10.884528087600, /*  11.16 */
    10.903610688340, /*  11.18 */
    10.922694972724, /*  11.20 */
    10.941780934568, /*  11.22 */
    10.960868567724, /*  11.24 */
    10.979957866075, /*  11.26 */
    10.999048823540, /*  11.28 */
    11.018141434071, /*  11.30 */
    11.037235691652, /*  11.32 */
    11.056331590301, /*  11.34 */
    11.075429124068, /*  11.36 */
    11.094528287034, /*  11.38 */
    11.113629073316, /*  11.40 */
    11.132731477059, /*  11.42 */
    11.151835492442, /*  11.44 */
    11.170941113675, /*  11.46 */
    11.190048334998, /*  11.48 */
    11.209157150684, /*  11.50 */
    11.228267555035, /*  11.52 */
    11.247379542385, /*  11.54 */
    11.266493107096, /*  11.56 */
    11.285608243564, /*  11.58 */
    11.304724946212, /*  11.60 */
    11.323843209493, /*  11.62 */
    11.342963027890, /*  11.64 */
    11.362084395915, /*  11.66 */
    11.381207308110, /*  11.68 */
    11.400331759045, /*  11.70 */
    11.419457743318, /*  11.72 */
    11.438585255556, /*  11.74 */
    11.457714290416, /*  11.76 */
    11.476844842581, /*  11.78 */
    11.495976906761, /*  11.80 */
    11.515110477697, /*  11.82 */
    11.534245550155, /*  11.84 */
    11.553382118929, /*  11.86 */
    11.572520178839, /*  11.88 */
    11.591659724734, /*  11.90 */
    11.610800751488, /*  11.92 */
    11.629943254002, /*  11.94 */
    11.649087227204, /*  11.96 */
    11.668232666048, /*  11.98 */
    11.687379565512, /*  12.00 */
    11.706527920603, /*  12.02 */
    11.725677726351, /*  12.04 */
    11.744828977814, /*  12.06 */
    11.763981670072, /*  12.08 */
    11.783135798233, /*  12.10 */
    11.802291357429, /*  12.12 */
    11.821448342817, /*  12.14 */
    11.840606749577, /*  12.16 */
    11.859766572915, /*  12.18 */
    11.878927808062, /*  12.20 */
    11.898090450271, /*  12.22 */
    11.917254494820, /*  12.24 */
    11.936419937011, /*  12.26 */
    11.955586772170, /*  12.28 */
    11.974754995646, /*  12.30 */
    11.993924602810, /*  12.32 */
    12.013095589058, /*  12.34 */
    12.032267949809, /*  12.36 */
    12.051441680502, /*  12.38 */
    12.070616776604, /*  12.40 */
    12.089793233599, /*  12.42 */
    12.108971046997, /*  12.44 */
    12.128150212328, /*  12.46 */
    12.147330725146, /*  12.48 */
    12.166512581026, /*  12.50 */
    12.185695775565, /*  12.52 */
    12.204880304381, /*  12.54 */
    12.224066163115, /*  12.56 */
    12.243253347428, /*  12.58 */
    12.262441853002, /*  12.60 */
    12.281631675542, /*  12.62 */
    12.300822810772, /*  12.64 */
    12.320015254437, /*  12.66 */
    12.339209002305, /*  12.68 */
    12.358404050162, /*  12.70 */
    12.377600393814, /*  12.72 */
    12.396798029091, /*  12.74 */
    12.415996951839, /*  12.76 */
    12.435197157926, /*  12.78 */
    12.454398643241, /*  12.80 */
    12.473601403689, /*  12.82 */
    12.492805435199, /*  12.84 */
    12.512010733717, /*  12.86 */
    12.531217295208, /*  12.88 */
    12.550425115659, /*  12.90 */
    12.569634191074, /*  12.92 */
    12.588844517475, /*  12.94 */
    12.608056090906, /*  12.96 */
    12.627268907428, /*  12.98 */
    12.646482963120, /*  13.00 */
    12.665698254080, /*  13.02 */
    12.684914776426, /*  13.04 */
    12.704132526293, /*  13.06 */
    12.723351499833, /*  13.08 */
    12.742571693219, /*  13.10 */
    12.761793102639, /*  13.12 */
    12.781015724302, /*  13.14 */
    12.800239554431, /*  13.16 */
    12.819464589269, /*  13.18 */
    12.838690825077, /*  13.20 */
    12.857918258132, /*  13.22 */
    12.877146884729, /*  13.24 */
    12.896376701181, /*  13.26 */
    12.915607703815, /*  13.28 */
    12.934839888979, /*  13.30 */
    12.954073253035, /*  13.32 */
    12.973307792364, /*  13.34 */
    12.992543503361, /*  13.36 */
    13.011780382440, /*  13.38 */
    13.031018426031, /*  13.40 */
    13.050257630579, /*  13.42 */
    13.069497992546, /*  13.44 */
    13.088739508412, /*  13.46 */
    13.107982174670, /*  13.48 */
    13.127225987831, /*  13.50 */
    13.146470944421, /*  13.52 */
    13.165717040982, /*  13.54 */
    13.184964274072, /*  13.56 */
    13.204212640265, /*  13.58 */
    13.223462136150, /*  13.60 */
    13.242712758329, /*  13.62 */
    13.261964503424, /*  13.64 */
    13.281217368069, /*  13.66 */
    13.300471348915, /*  13.68 */
    13.319726442625, /*  13.70 */
    13.338982645880, /*  13.72 */
    13.358239955374, /*  13.74 */
    13.377498367819, /*  13.76 */
    13.396757879936, /*  13.78 */
    13.416018488466, /*  13.80 */
    13.435280190162, /*  13.82 */
    13.454542981792, /*  13.84 */
    13.473806860137, /*  13.86 */
    13.493071821994, /*  13.88 */
    13.512337864174, /*  13.90 */
    13.531604983501, /*  13.92 */
    13.550873176813, /*  13.94 */
    13.570142440965, /*  13.96 */
    13.589412772821, /*  13.98 */
    13.608684169261, /*  14.00 */
    13.627956627181, /*  14.02 */
    13.647230143487, /*  14.04 */
    13.666504715099, /*  14.06 */
    13.685780338953, /*  14.08 */
    13.705057011996, /*  14.10 */
    13.724334731190, /*  14.12 */
    13.743613493507, /*  14.14 */
    13.762893295937, /*  14.16 */
    13.782174135478, /*  14.18 */
    13.801456009145, /*  14.20 */
    13.820738913963, /*  14.22 */
    13.840022846973, /*  14.24 */
    13.859307805224, /*  14.26 */
    13.878593785784, /*  14.28 */
    13.897880785727, /*  14.30 */
    13.917168802144, /*  14.32 */
    13.936457832138, /*  14.34 */
    13.955747872822, /*  14.36 */
    13.975038921323, /*  14.38 */
    13.994330974781, /*  14.40 */
    14.013624030347, /*  14.42 */
    14.032918085185, /*  14.44 */
    14.052213136468, /*  14.46 */
    14.071509181387, /*  14.48 */
    14.090806217138, /*  14.50 */
    14.110104240934, /*  14.52 */
    14.129403249998, /*  14.54 */
    14.148703241564, /*  14.56 */
    14.168004212878, /*  14.58 */
    14.187306161199, /*  14.60 */
    14.206609083796, /*  14.62 */
    14.225912977949, /*  14.64 */
    14.245217840951, /*  14.66 */
    14.264523670105, /*  14.68 */
    14.283830462727, /*  14.70 */
    14.303138216141, /*  14.72 */
    14.322446927686, /*  14.74 */
    14.341756594709, /*  14.76 */
    14.361067214569, /*  14.78 */
    14.380378784638, /*  14.80 */
    14.399691302295, /*  14.82 */
    14.419004764933, /*  14.84 */
    14.438319169954, /*  14.86 */
    14.457634514771, /*  14.88 */
    14.476950796810, /*  14.90 */
    14.496268013503, /*  14.92 */
    14.515586162296, /*  14.94 */
    14.534905240646, /*  14.96 */
    14.554225246017, /*  14.98 */
    14.573546175886, /*  15.00 */
    14.592868027740, /*  15.02 */
    14.612190799076, /*  15.04 */
    14.631514487401, /*  15.06 */
    14.650839090231, /*  15.08 */
    14.670164605096, /*  15.10 */
    14.689491029531, /*  15.12 */
    14.708818361084, /*  15.14 */
    14.728146597312, /*  15.16 */
    14.747475735783, /*  15.18 */
    14.766805774073, /*  15.20 */
    14.786136709769, /*  15.22 */
    14.805468540468, /*  15.24 */
    14.824801263775, /*  15.26 */
    14.844134877307, /*  15.28 */
    14.863469378688, /*  15.30 */
    14.882804765552, /*  15.32 */
    14.902141035545, /*  15.34 */
    14.921478186320, /*  15.36 */
    14.940816215540, /*  15.38 */
    14.960155120876, /*  15.40 */
    14.979494900011, /*  15.42 */
    14.998835550635, /*  15.44 */
    15.018177070447, /*  15.46 */
    15.037519457158, /*  15.48 */
    15.056862708484, /*  15.50 */
    15.076206822153, /*  15.52 */
    15.095551795901, /*  15.54 */
    15.114897627472, /*  15.56 */
    15.134244314620, /*  15.58 */
    15.153591855109, /*  15.60 */
    15.172940246708, /*  15.62 */
    15.192289487199, /*  15.64 */
    15.211639574370, /*  15.66 */
    15.230990506018, /*  15.68 */
    15.250342279950, /*  15.70 */
    15.269694893979, /*  15.72 */
    15.289048345930, /*  15.74 */
    15.308402633633, /*  15.76 */
    15.327757754928, /*  15.78 */
    15.347113707664, /*  15.80 */
    15.366470489697, /*  15.82 */
    15.385828098892, /*  15.84 */
    15.405186533123, /*  15.86 */
    15.424545790270, /*  15.88 */
    15.443905868223, /*  15.90 */
    15.463266764880, /*  15.92 */
    15.482628478147, /*  15.94 */
    15.501991005937, /*  15.96 */
    15.521354346171, /*  15.98 */
    15.540718496781, /*  16.00 */
    15.560083455702, /*  16.02 */
    15.579449220882, /*  16.04 */
    15.598815790272, /*  16.06 */
    15.618183161834, /*  16.08 */
    15.637551333538, /*  16.10 */
    15.656920303358, /*  16.12 */
    15.676290069281, /*  16.14 */
    15.695660629297, /*  16.16 */
    15.715031981406, /*  16.18 */
    15.734404123615, /*  16.20 */
    15.753777053938, /*  16.22 */
    15.773150770399, /*  16.24 */
    15.792525271025, /*  16.26 */
    15.811900553854, /*  16.28 */
    15.831276616931, /*  16.30 */
    15.850653458306, /*  16.32 */
    15.870031076039, /*  16.34 */
    15.889409468196, /*  16.36 */
    15.908788632850, /*  16.38 */
    15.928168568081, /*  16.40 */
    15.947549271978, /*  16.42 */
    15.966930742635, /*  16.44 */
    15.986312978155, /*  16.46 */
    16.005695976645, /*  16.48 */
    16.025079736223, /*  16.50 */
    16.044464255010, /*  16.52 */
    16.063849531137, /*  16.54 */
    16.083235562741, /*  16.56 */
    16.102622347965, /*  16.58 */
    16.122009884960, /*  16.60 */
    16.141398171883, /*  16.62 */
    16.160787206899, /*  16.64 */
    16.180176988177, /*  16.66 */
    16.199567513896, /*  16.68 */
    16.218958782239, /*  16.70 */
    16.238350791399, /*  16.72 */
    16.257743539571, /*  16.74 */
    16.277137024960, /*  16.76 */
    16.296531245777, /*  16.78 */
    16.315926200239, /*  16.80 */
    16.335321886569, /*  16.82 */
    16.354718302997, /*  16.84 */
    16.374115447760, /*  16.86 */
    16.393513319101, /*  16.88 */
    16.412911915269, /*  16.90 */
    16.432311234519, /*  16.92 */
    16.451711275113, /*  16.94 */
    16.471112035319, /*  16.96 */
    16.490513513412, /*  16.98 */
    16.509915707672, /*  17.00 */
    16.529318616386, /*  17.02 */
    16.548722237846, /*  17.04 */
    16.568126570352, /*  17.06 */
    16.587531612208, /*  17.08 */
    16.606937361726, /*  17.10 */
    16.626343817223, /*  17.12 */
    16.645750977022, /*  17.14 */
    16.665158839452, /*  17.16 */
    16.684567402849, /*  17.18 */
    16.703976665552, /*  17.20 */
    16.723386625910, /*  17.22 */
    16.742797282274, /*  17.24 */
    16.762208633003, /*  17.26 */
    16.781620676462, /*  17.28 */
    16.801033411021, /*  17.30 */
    16.820446835056, /*  17.32 */
    16.839860946947, /*  17.34 */
    16.859275745084, /*  17.36 */
    16.878691227857, /*  17.38 */
    16.898107393667, /*  17.40 */
    16.917524240918, /*  17.42 */
    16.936941768019, /*  17.44 */
    16.956359973385, /*  17.46 */
    16.975778855438, /*  17.48 */
    16.995198412604, /*  17.50 */
    17.014618643315, /*  17.52 */
    17.034039546008, /*  17.54 */
    17.053461119126, /*  17.56 */
    17.072883361118, /*  17.58 */
    17.092306270437, /*  17.60 */
    17.111729845542, /*  17.62 */
    17.131154084897, /*  17.64 */
    17.150578986972, /*  17.66 */
    17.170004550242, /*  17.68 */
    17.189430773187, /*  17.70 */
    17.208857654293, /*  17.72 */
    17.228285192050, /*  17.74 */
    17.247713384954, /*  17.76 */
    17.267142231505, /*  17.78 */
    17.286571730211, /*  17.80 */
    17.306001879583, /*  17.82 */
    17.325432678136, /*  17.84 */
    17.344864124392, /*  17.86 */
    17.364296216878, /*  17.88 */
    17.383728954125, /*  17.90 */
    17.403162334670, /*  17.92 */
    17.422596357054, /*  17.94 */
    17.442031019823, /*  17.96 */
    17.461466321529, /*  17.98 */
    17.480902260729, /*  18.00 */
    17.500338835983, /*  18.02 */
    17.519776045858, /*  18.04 */
    17.539213888924, /*  18.06 */
    17.558652363758, /*  18.08 */
    17.578091468939, /*  18.10 */
    17.597531203053, /*  18.12 */
    17.616971564690, /*  18.14 */
    17.636412552445, /*  18.16 */
    17.655854164918, /*  18.18 */
    17.675296400712, /*  18.20 */
    17.694739258437, /*  18.22 */
    17.714182736707, /*  18.24 */
    17.733626834138, /*  18.26 */
    17.753071549355, /*  18.28 */
    17.772516880985, /*  18.30 */
    17.791962827659, /*  18.32 */
    17.811409388014, /*  18.34 */
    17.830856560691, /*  18.36 */
    17.850304344336, /*  18.38 */
    17.869752737600, /*  18.40 */
    17.889201739135, /*  18.42 */
    17.908651347602, /*  18.44 */
    17.928101561664, /*  18.46 */
    17.947552379988, /*  18.48 */
    17.967003801248, /*  18.50 */
    17.986455824119, /*  18.52 */
    18.005908447282, /*  18.54 */
    18.025361669423, /*  18.56 */
    18.044815489232, /*  18.58 */
    18.064269905402, /*  18.60 */
    18.083724916631, /*  18.62 */
    18.103180521623, /*  18.64 */
    18.122636719083, /*  18.66 */
    18.142093507723, /*  18.68 */
    18.161550886258, /*  18.70 */
    18.181008853407, /*  18.72 */
    18.200467407894, /*  18.74 */
    18.219926548446, /*  18.76 */
    18.239386273794, /*  18.78 */
    18.258846582676, /*  18.80 */
    18.278307473831, /*  18.82 */
    18.297768946003, /*  18.84 */
    18.317230997939, /*  18.86 */
    18.336693628393, /*  18.88 */
    18.356156836121, /*  18.90 */
    18.375620619882, /*  18.92 */
    18.395084978441, /*  18.94 */
    18.414549910566, /*  18.96 */
    18.434015415030, /*  18.98 */
    18.453481490608, /*  19.00 */
    18.472948136080, /*  19.02 */
    18.492415350230, /*  19.04 */
    18.511883131846, /*  19.06 */
    18.531351479720, /*  19.08 */
    18.550820392647, /*  19.10 */
    18.570289869427, /*  19.12 */
    18.589759908863, /*  19.14 */
    18.609230509761, /*  19.16 */
    18.628701670934, /*  19.18 */
    18.648173391194, /*  19.20 */
    18.667645669361, /*  19.22 */
    18.687118504256, /*  19.24 */
    18.706591894706, /*  19.26 */
    18.726065839540, /*  19.28 */
    18.745540337592, /*  19.30 */
    18.765015387697, /*  19.32 */
    18.784490988697, /*  19.34 */
    18.803967139436, /*  19.36 */
    18.823443838762, /*  19.38 */
    18.842921085526, /*  19.40 */
    18.862398878583, /*  19.42 */
    18.881877216793, /*  19.44 */
    18.901356099017, /*  19.46 */
    18.920835524121, /*  19.48 */
    18.940315490974, /*  19.50 */
    18.959795998450, /*  19.52 */
    18.979277045425, /*  19.54 */
    18.998758630778, /*  19.56 */
    19.018240753393, /*  19.58 */
    19.037723412158, /*  19.60 */
    19.057206605961, /*  19.62 */
    19.076690333698, /*  19.64 */
    19.096174594265, /*  19.66 */
    19.115659386563, /*  19.68 */
    19.135144709496, /*  19.70 */
    19.154630561971, /*  19.72 */
    19.174116942899, /*  19.74 */
    19.193603851194, /*  19.76 */
    19.213091285774, /*  19.78 */
    19.232579245560, /*  19.80 */
    19.252067729475, /*  19.82 */
    19.271556736447, /*  19.84 */
    19.291046265407, /*  19.86 */
    19.310536315289, /*  19.88 */
    19.330026885030, /*  19.90 */
    19.349517973571, /*  19.92 */
    19.369009579855, /*  19.94 */
    19.388501702829, /*  19.96 */
    19.407994341444, /*  19.98 */
    19.427487494653, /*  20.00 */
    19.446981161413, /*  20.02 */
    19.466475340682, /*  20.04 */
    19.485970031425, /*  20.06 */
    19.505465232607, /*  20.08 */
    19.524960943198, /*  20.10 */
    19.544457162169, /*  20.12 */
    19.563953888497, /*  20.14 */
    19.583451121159, /*  20.16 */
    19.602948859137, /*  20.18 */
    19.622447101417, /*  20.20 */
    19.641945846985, /*  20.22 */
    19.661445094833, /*  20.24 */
    19.680944843953, /*  20.26 */
    19.700445093344, /*  20.28 */
    19.719945842005, /*  20.30 */
    19.739447088939, /*  20.32 */
    19.758948833152, /*  20.34 */
    19.778451073651, /*  20.36 */
    19.797953809451, /*  20.38 */
    19.817457039564, /*  20.40 */
    19.836960763008, /*  20.42 */
    19.856464978805, /*  20.44 */
    19.875969685977, /*  20.46 */
    19.895474883552, /*  20.48 */
    19.914980570557, /*  20.50 */
    19.934486746027, /*  20.52 */
    19.953993408995, /*  20.54 */
    19.973500558499, /*  20.56 */
    19.993008193581, /*  20.58 */
    20.012516313283, /*  20.60 */
    20.032024916653, /*  20.62 */
    20.051534002739, /*  20.64 */
    20.071043570594, /*  20.66 */
    20.090553619272, /*  20.68 */
    20.110064147831, /*  20.70 */
    20.129575155331, /*  20.72 */
    20.149086640835, /*  20.74 */
    20.168598603410, /*  20.76 */
    20.188111042124, /*  20.78 */
    20.207623956048, /*  20.80 */
    20.227137344256, /*  20.82 */
    20.246651205825, /*  20.84 */
    20.266165539835, /*  20.86 */
    20.285680345367, /*  20.88 */
    20.305195621507, /*  20.90 */
    20.324711367342, /*  20.92 */
    20.344227581962, /*  20.94 */
    20.363744264459, /*  20.96 */
    20.383261413930, /*  20.98 */
    20.402779029472, /*  21.00 */
    20.422297110186, /*  21.02 */
    20.441815655175, /*  21.04 */
    20.461334663546, /*  21.06 */
    20.480854134405, /*  21.08 */
    20.500374066865, /*  21.10 */
    20.519894460039, /*  21.12 */
    20.539415313042, /*  21.14 */
    20.558936624995, /*  21.16 */
    20.578458395017, /*  21.18 */
    20.597980622233, /*  21.20 */
    20.617503305769, /*  21.22 */
    20.637026444753, /*  21.24 */
    20.656550038317, /*  21.26 */
    20.676074085594, /*  21.28 */
    20.695598585721, /*  21.30 */
    20.715123537836, /*  21.32 */
    20.734648941080, /*  21.34 */
    20.754174794597, /*  21.36 */
    20.773701097533, /*  21.38 */
    20.793227849036, /*  21.40 */
    20.812755048257, /*  21.42 */
    20.832282694350, /*  21.44 */
    20.851810786469, /*  21.46 */
    20.871339323774, /*  21.48 */
    20.890868305424, /*  21.50 */
    20.910397730582, /*  21.52 */
    20.929927598413, /*  21.54 */
    20.949457908086, /*  21.56 */
    20.968988658769, /*  21.58 */
    20.988519849635, /*  21.60 */
    21.008051479858, /*  21.62 */
    21.027583548616, /*  21.64 */
    21.047116055087, /*  21.66 */
    21.066648998453, /*  21.68 */
    21.086182377898, /*  21.70 */
    21.105716192607, /*  21.72 */
    21.125250441769, /*  21.74 */
    21.144785124576, /*  21.76 */
    21.164320240218, /*  21.78 */
    21.183855787892, /*  21.80 */
    21.203391766796, /*  21.82 */
    21.222928176127, /*  21.84 */
    21.242465015089, /*  21.86 */
    21.262002282886, /*  21.88 */
    21.281539978723, /*  21.90 */
    21.301078101809, /*  21.92 */
    21.320616651355, /*  21.94 */
    21.340155626574, /*  21.96 */
    21.359695026680, /*  21.98 */
    21.379234850891, /*  22.00 */
    21.398775098426, /*  22.02 */
    21.418315768506, /*  22.04 */
    21.437856860356, /*  22.06 */
    21.457398373201, /*  22.08 */
    21.476940306268, /*  22.10 */
    21.496482658789, /*  22.12 */
    21.516025429995, /*  22.14 */
    21.535568619120, /*  22.16 */
    21.555112225401, /*  22.18 */
    21.574656248076, /*  22.20 */
    21.594200686386, /*  22.22 */
    21.613745539573, /*  22.24 */
    21.633290806882, /*  22.26 */
    21.652836487560, /*  22.28 */
    21.672382580855, /*  22.30 */
    21.691929086019, /*  22.32 */
    21.711476002304, /*  22.34 */
    21.731023328966, /*  22.36 */
    21.750571065260, /*  22.38 */
    21.770119210447, /*  22.40 */
    21.789667763786, /*  22.42 */
    21.809216724542, /*  22.44 */
    21.828766091979, /*  22.46 */
    21.848315865363, /*  22.48 */
    21.867866043965, /*  22.50 */
    21.887416627054, /*  22.52 */
    21.906967613904, /*  22.54 */
    21.926519003790, /*  22.56 */
    21.946070795988, /*  22.58 */
    21.965622989776, /*  22.60 */
    21.985175584437, /*  22.62 */
    22.004728579252, /*  22.64 */
    22.024281973506, /*  22.66 */
    22.043835766485, /*  22.68 */
    22.063389957478, /*  22.70 */
    22.082944545774, /*  22.72 */
    22.102499530667, /*  22.74 */
    22.122054911450, /*  22.76 */
    22.141610687419, /*  22.78 */
    22.161166857872, /*  22.80 */
    22.180723422108, /*  22.82 */
    22.200280379430, /*  22.84 */
    22.219837729140, /*  22.86 */
    22.239395470544, /*  22.88 */
    22.258953602948, /*  22.90 */
    22.278512125663, /*  22.92 */
    22.298071037998, /*  22.94 */
    22.317630339267, /*  22.96 */
    22.337190028783, /*  22.98 */
    22.356750105862, /*  23.00 */
    22.376310569824, /*  23.02 */
    22.395871419988, /*  23.04 */
    22.415432655674, /*  23.06 */
    22.434994276208, /*  23.08 */
    22.454556280913, /*  23.10 */
    22.474118669118, /*  23.12 */
    22.493681440150, /*  23.14 */
    22.513244593340, /*  23.16 */
    22.532808128020, /*  23.18 */
    22.552372043525, /*  23.20 */
    22.571936339190, /*  23.22 */
    22.591501014353, /*  23.24 */
    22.611066068352, /*  23.26 */
    22.630631500530, /*  23.28 */
    22.650197310227, /*  23.30 */
    22.669763496789, /*  23.32 */
    22.689330059563, /*  23.34 */
    22.708896997895, /*  23.36 */
    22.728464311134, /*  23.38 */
    22.748031998634, /*  23.40 */
    22.767600059745, /*  23.42 */
    22.787168493823, /*  23.44 */
    22.806737300224, /*  23.46 */
    22.826306478305, /*  23.48 */
    22.845876027427, /*  23.50 */
    22.865445946950, /*  23.52 */
    22.885016236237, /*  23.54 */
    22.904586894653, /*  23.56 */
    22.924157921564, /*  23.58 */
    22.943729316338, /*  23.60 */
    22.963301078343, /*  23.62 */
    22.982873206951, /*  23.64 */
    23.002445701535, /*  23.66 */
    23.022018561468, /*  23.68 */
    23.041591786127, /*  23.70 */
    23.061165374889, /*  23.72 */
    23.080739327132, /*  23.74 */
    23.100313642239, /*  23.76 */
    23.119888319590, /*  23.78 */
    23.139463358569, /*  23.80 */
    23.159038758563, /*  23.82 */
    23.178614518957, /*  23.84 */
    23.198190639140, /*  23.86 */
    23.217767118503, /*  23.88 */
    23.237343956436, /*  23.90 */
    23.256921152333, /*  23.92 */
    23.276498705589, /*  23.94 */
    23.296076615599, /*  23.96 */
    23.315654881762, /*  23.98 */
    23.335233503476, /*  24.00 */
    23.354812480143, /*  24.02 */
    23.374391811164, /*  24.04 */
    23.393971495944, /*  24.06 */
    23.413551533888, /*  24.08 */
    23.433131924402, /*  24.10 */
    23.452712666895, /*  24.12 */
    23.472293760776, /*  24.14 */
    23.491875205457, /*  24.16 */
    23.511457000351, /*  24.18 */
    23.531039144872, /*  24.20 */
    23.550621638435, /*  24.22 */
    23.570204480458, /*  24.24 */
    23.589787670358, /*  24.26 */
    23.609371207557, /*  24.28 */
    23.628955091476, /*  24.30 */
    23.648539321538, /*  24.32 */
    23.668123897166, /*  24.34 */
    23.687708817787, /*  24.36 */
    23.707294082829, /*  24.38 */
    23.726879691719, /*  24.40 */
    23.746465643888, /*  24.42 */
    23.766051938768, /*  24.44 */
    23.785638575791, /*  24.46 */
    23.805225554391, /*  24.48 */
    23.824812874005, /*  24.50 */
    23.844400534070, /*  24.52 */
    23.863988534023, /*  24.54 */
    23.883576873306, /*  24.56 */
    23.903165551359, /*  24.58 */
    23.922754567624, /*  24.60 */
    23.942343921547, /*  24.62 */
    23.961933612573, /*  24.64 */
    23.981523640147, /*  24.66 */
    24.001114003719, /*  24.68 */
    24.020704702738, /*  24.70 */
    24.040295736654, /*  24.72 */
    24.059887104920, /*  24.74 */
    24.079478806990, /*  24.76 */
    24.099070842319, /*  24.78 */
    24.118663210362, /*  24.80 */
    24.138255910577, /*  24.82 */
    24.157848942423, /*  24.84 */
    24.177442305360, /*  24.86 */
    24.197035998850, /*  24.88 */
    24.216630022356, /*  24.90 */
    24.236224375341, /*  24.92 */
    24.255819057272, /*  24.94 */
    24.275414067614, /*  24.96 */
    24.295009405836, /*  24.98 */
    24.314605071408, /*  25.00 */
    24.334201063800, /*  25.02 */
    24.353797382483, /*  25.04 */
    24.373394026932, /*  25.06 */
    24.392990996620, /*  25.08 */
    24.412588291023, /*  25.10 */
    24.432185909618, /*  25.12 */
    24.451783851884, /*  25.14 */
    24.471382117300, /*  25.16 */
    24.490980705346, /*  25.18 */
    24.510579615506, /*  25.20 */
    24.530178847261, /*  25.22 */
    24.549778400097, /*  25.24 */
    24.569378273499, /*  25.26 */
    24.588978466955, /*  25.28 */
    24.608578979952, /*  25.30 */
    24.628179811980, /*  25.32 */
    24.647780962530, /*  25.34 */
    24.667382431093, /*  25.36 */
    24.686984217163, /*  25.38 */
    24.706586320234, /*  25.40 */
    24.726188739801, /*  25.42 */
    24.745791475362, /*  25.44 */
    24.765394526413, /*  25.46 */
    24.784997892455, /*  25.48 */
    24.804601572988, /*  25.50 */
    24.824205567513, /*  25.52 */
    24.843809875532, /*  25.54 */
    24.863414496550, /*  25.56 */
    24.883019430072, /*  25.58 */
    24.902624675604, /*  25.60 */
    24.922230232653, /*  25.62 */
    24.941836100727, /*  25.64 */
    24.961442279338, /*  25.66 */
    24.981048767995, /*  25.68 */
    25.000655566210, /*  25.70 */
    25.020262673497, /*  25.72 */
    25.039870089370, /*  25.74 */
    25.059477813345, /*  25.76 */
    25.079085844937, /*  25.78 */
    25.098694183666, /*  25.80 */
    25.118302829049, /*  25.82 */
    25.137911780607, /*  25.84 */
    25.157521037861, /*  25.86 */
    25.177130600333, /*  25.88 */
    25.196740467547, /*  25.90 */
    25.216350639027, /*  25.92 */
    25.235961114298, /*  25.94 */
    25.255571892888, /*  25.96 */
    25.275182974324, /*  25.98 */
    25.294794358135, /*  26.00 */
    25.314406043852, /*  26.02 */
    25.334018031004, /*  26.04 */
    25.353630319126, /*  26.06 */
    25.373242907749, /*  26.08 */
    25.392855796408, /*  26.10 */
    25.412468984639, /*  26.12 */
    25.432082471979, /*  26.14 */
    25.451696257964, /*  26.16 */
    25.471310342134, /*  26.18 */
    25.490924724028, /*  26.20 */
    25.510539403187, /*  26.22 */
    25.530154379154, /*  26.24 */
    25.549769651470, /*  26.26 */
    25.569385219681, /*  26.28 */
    25.589001083330, /*  26.30 */
    25.608617241965, /*  26.32 */
    25.628233695132, /*  26.34 */
    25.647850442380, /*  26.36 */
    25.667467483257, /*  26.38 */
    25.687084817315, /*  26.40 */
    25.706702444103, /*  26.42 */
    25.726320363176, /*  26.44 */
    25.745938574085, /*  26.46 */
    25.765557076386, /*  26.48 */
    25.785175869634, /*  26.50 */
    25.804794953385, /*  26.52 */
    25.824414327197, /*  26.54 */
    25.844033990628, /*  26.56 */
    25.863653943237, /*  26.58 */
    25.883274184586, /*  26.60 */
    25.902894714235, /*  26.62 */
    25.922515531747, /*  26.64 */
    25.942136636685, /*  26.66 */
    25.961758028614, /*  26.68 */
    25.981379707100, /*  26.70 */
    26.001001671708, /*  26.72 */
    26.020623922006, /*  26.74 */
    26.040246457563, /*  26.76 */
    26.059869277948, /*  26.78 */
    26.079492382731, /*  26.80 */
    26.099115771483, /*  26.82 */
    26.118739443777, /*  26.84 */
    26.138363399187, /*  26.86 */
    26.157987637285, /*  26.88 */
    26.177612157648, /*  26.90 */
    26.197236959852, /*  26.92 */
    26.216862043473, /*  26.94 */
    26.236487408089, /*  26.96 */
    26.256113053281, /*  26.98 */
    26.275738978627, /*  27.00 */
    26.295365183708, /*  27.02 */
    26.314991668107, /*  27.04 */
    26.334618431405, /*  27.06 */
    26.354245473188, /*  27.08 */
    26.373872793038, /*  27.10 */
    26.393500390543, /*  27.12 */
    26.413128265287, /*  27.14 */
    26.432756416860, /*  27.16 */
    26.452384844848, /*  27.18 */
    26.472013548842, /*  27.20 */
    26.491642528431, /*  27.22 */
    26.511271783206, /*  27.24 */
    26.530901312760, /*  27.26 */
    26.550531116685, /*  27.28 */
    26.570161194575, /*  27.30 */
    26.589791546025, /*  27.32 */
    26.609422170630, /*  27.34 */
    26.629053067987, /*  27.36 */
    26.648684237693, /*  27.38 */
    26.668315679347, /*  27.40 */
    26.687947392547, /*  27.42 */
    26.707579376894, /*  27.44 */
    26.727211631988, /*  27.46 */
    26.746844157432, /*  27.48 */
    26.766476952827, /*  27.50 */
    26.786110017778, /*  27.52 */
    26.805743351890, /*  27.54 */
    26.825376954766, /*  27.56 */
    26.845010826014, /*  27.58 */
    26.864644965241, /*  27.60 */
    26.884279372053, /*  27.62 */
    26.903914046061, /*  27.64 */
    26.923548986874, /*  27.66 */
    26.943184194102, /*  27.68 */
    26.962819667356, /*  27.70 */
    26.982455406250, /*  27.72 */
    27.002091410394, /*  27.74 */
    27.021727679405, /*  27.76 */
    27.041364212896, /*  27.78 */
    27.061001010482, /*  27.80 */
    27.080638071781, /*  27.82 */
    27.100275396409, /*  27.84 */
    27.119912983985, /*  27.86 */
    27.139550834127, /*  27.88 */
    27.159188946455, /*  27.90 */
    27.178827320590, /*  27.92 */
    27.198465956152, /*  27.94 */
    27.218104852765, /*  27.96 */
    27.237744010051, /*  27.98 */
    27.257383427634, /*  28.00 */
    27.277023105139, /*  28.02 */
    27.296663042190, /*  28.04 */
    27.316303238415, /*  28.06 */
    27.335943693440, /*  28.08 */
    27.355584406894, /*  28.10 */
    27.375225378404, /*  28.12 */
    27.394866607601, /*  28.14 */
    27.414508094114, /*  28.16 */
    27.434149837575, /*  28.18 */
    27.453791837615, /*  28.20 */
    27.473434093868, /*  28.22 */
    27.493076605966, /*  28.24 */
    27.512719373544, /*  28.26 */
    27.532362396237, /*  28.28 */
    27.552005673680, /*  28.30 */
    27.571649205511, /*  28.32 */
    27.591292991366, /*  28.34 */
    27.610937030884, /*  28.36 */
    27.630581323703, /*  28.38 */
    27.650225869463, /*  28.40 */
    27.669870667805, /*  28.42 */
    27.689515718370, /*  28.44 */
    27.709161020799, /*  28.46 */
    27.728806574736, /*  28.48 */
    27.748452379824, /*  28.50 */
    27.768098435707, /*  28.52 */
    27.787744742030, /*  28.54 */
    27.807391298439, /*  28.56 */
    27.827038104580, /*  28.58 */
    27.846685160101, /*  28.60 */
    27.866332464650, /*  28.62 */
    27.885980017875, /*  28.64 */
    27.905627819426, /*  28.66 */
    27.925275868954, /*  28.68 */
    27.944924166108, /*  28.70 */
    27.964572710541, /*  28.72 */
    27.984221501905, /*  28.74 */
    28.003870539853, /*  28.76 */
    28.023519824040, /*  28.78 */
    28.043169354120, /*  28.80 */
    28.062819129748, /*  28.82 */
    28.082469150580, /*  28.84 */
    28.102119416273, /*  28.86 */
    28.121769926485, /*  28.88 */
    28.141420680874, /*  28.90 */
    28.161071679099, /*  28.92 */
    28.180722920819, /*  28.94 */
    28.200374405695, /*  28.96 */
    28.220026133387, /*  28.98 */
    28.239678103559, /*  29.00 */
    28.259330315872, /*  29.02 */
    28.278982769989, /*  29.04 */
    28.298635465575, /*  29.06 */
    28.318288402294, /*  29.08 */
    28.337941579810, /*  29.10 */
    28.357594997792, /*  29.12 */
    28.377248655904, /*  29.14 */
    28.396902553814, /*  29.16 */
    28.416556691191, /*  29.18 */
    28.436211067704, /*  29.20 */
    28.455865683021, /*  29.22 */
    28.475520536813, /*  29.24 */
    28.495175628750, /*  29.26 */
    28.514830958505, /*  29.28 */
    28.534486525749, /*  29.30 */
    28.554142330155, /*  29.32 */
    28.573798371397, /*  29.34 */
    28.593454649149, /*  29.36 */
    28.613111163086, /*  29.38 */
    28.632767912883, /*  29.40 */
    28.652424898217, /*  29.42 */
    28.672082118765, /*  29.44 */
    28.691739574203, /*  29.46 */
    28.711397264211, /*  29.48 */
    28.731055188467, /*  29.50 */
    28.750713346651, /*  29.52 */
    28.770371738443, /*  29.54 */
    28.790030363524, /*  29.56 */
    28.809689221574, /*  29.58 */
    28.829348312278, /*  29.60 */
    28.849007635316, /*  29.62 */
    28.868667190374, /*  29.64 */
    28.888326977134, /*  29.66 */
    28.907986995282, /*  29.68 */
    28.927647244502, /*  29.70 */
    28.947307724482, /*  29.72 */
    28.966968434907, /*  29.74 */
    28.986629375465, /*  29.76 */
    29.006290545845, /*  29.78 */
    29.025951945733, /*  29.80 */
    29.045613574821, /*  29.82 */
    29.065275432797, /*  29.84 */
    29.084937519352, /*  29.86 */
    29.104599834177, /*  29.88 */
    29.124262376964, /*  29.90 */
    29.143925147405, /*  29.92 */
    29.163588145193, /*  29.94 */
    29.183251370023, /*  29.96 */
    29.202914821587, /*  29.98 */
    29.222578499581, /*  30.00 */
    29.242242403700, /*  30.02 */
    29.261906533641, /*  30.04 */
    29.281570889099, /*  30.06 */
    29.301235469773, /*  30.08 */
    29.320900275360, /*  30.10 */
    29.340565305559, /*  30.12 */
    29.360230560068, /*  30.14 */
    29.379896038588, /*  30.16 */
    29.399561740819, /*  30.18 */
    29.419227666462, /*  30.20 */
    29.438893815217, /*  30.22 */
    29.458560186789, /*  30.24 */
    29.478226780878, /*  30.26 */
    29.497893597188, /*  30.28 */
    29.517560635424, /*  30.30 */
    29.537227895290, /*  30.32 */
    29.556895376490, /*  30.34 */
    29.576563078731, /*  30.36 */
    29.596231001719, /*  30.38 */
    29.615899145161, /*  30.40 */
    29.635567508763, /*  30.42 */
    29.655236092234, /*  30.44 */
    29.674904895283, /*  30.46 */
    29.694573917619, /*  30.48 */
    29.714243158951, /*  30.50 */
    29.733912618991, /*  30.52 */
    29.753582297448, /*  30.54 */
    29.773252194034, /*  30.56 */
    29.792922308461, /*  30.58 */
    29.812592640442, /*  30.60 */
    29.832263189691, /*  30.62 */
    29.851933955920, /*  30.64 */
    29.871604938844, /*  30.66 */
    29.891276138178, /*  30.68 */
    29.910947553637, /*  30.70 */
    29.930619184938, /*  30.72 */
    29.950291031797, /*  30.74 */
    29.969963093930, /*  30.76 */
    29.989635371057, /*  30.78 */
    30.009307862894, /*  30.80 */
    30.028980569162, /*  30.82 */
    30.048653489578, /*  30.84 */
    30.068326623863, /*  30.86 */
    30.087999971738, /*  30.88 */
    30.107673532923, /*  30.90 */
    30.127347307139, /*  30.92 */
    30.147021294110, /*  30.94 */
    30.166695493558, /*  30.96 */
    30.186369905205, /*  30.98 */
    30.206044528775, /*  31.00 */
    30.225719363993, /*  31.02 */
    30.245394410584, /*  31.04 */
    30.265069668272, /*  31.06 */
    30.284745136784, /*  31.08 */
    30.304420815846, /*  31.10 */
    30.324096705185, /*  31.12 */
    30.343772804528, /*  31.14 */
    30.363449113604, /*  31.16 */
    30.383125632140, /*  31.18 */
    30.402802359866, /*  31.20 */
    30.422479296512, /*  31.22 */
    30.442156441807, /*  31.24 */
    30.461833795482, /*  31.26 */
    30.481511357269, /*  31.28 */
    30.501189126898, /*  31.30 */
    30.520867104102, /*  31.32 */
    30.540545288614, /*  31.34 */
    30.560223680167, /*  31.36 */
    30.579902278494, /*  31.38 */
    30.599581083331, /*  31.40 */
    30.619260094410, /*  31.42 */
    30.638939311469, /*  31.44 */
    30.658618734242, /*  31.46 */
    30.678298362467, /*  31.48 */
    30.697978195878, /*  31.50 */
    30.717658234215, /*  31.52 */
    30.737338477214, /*  31.54 */
    30.757018924615, /*  31.56 */
    30.776699576155, /*  31.58 */
    30.796380431574, /*  31.60 */
    30.816061490612, /*  31.62 */
    30.835742753009, /*  31.64 */
    30.855424218506, /*  31.66 */
    30.875105886844, /*  31.68 */
    30.894787757765, /*  31.70 */
    30.914469831011, /*  31.72 */
    30.934152106326, /*  31.74 */
    30.953834583451, /*  31.76 */
    30.973517262132, /*  31.78 */
    30.993200142111, /*  31.80 */
    31.012883223135, /*  31.82 */
    31.032566504947, /*  31.84 */
    31.052249987295, /*  31.86 */
    31.071933669923, /*  31.88 */
    31.091617552579, /*  31.90 */
    31.111301635010, /*  31.92 */
    31.130985916963, /*  31.94 */
    31.150670398186, /*  31.96 */
    31.170355078428, /*  31.98 */
    31.190039957439, /*  32.00 */
    31.209725034966, /*  32.02 */
    31.229410310762, /*  32.04 */
    31.249095784575, /*  32.06 */
    31.268781456157, /*  32.08 */
    31.288467325259, /*  32.10 */
    31.308153391634, /*  32.12 */
    31.327839655033, /*  32.14 */
    31.347526115209, /*  32.16 */
    31.367212771916, /*  32.18 */
    31.386899624907, /*  32.20 */
    31.406586673936, /*  32.22 */
    31.426273918758, /*  32.24 */
    31.445961359129, /*  32.26 */
    31.465648994804, /*  32.28 */
    31.485336825538, /*  32.30 */
    31.505024851088, /*  32.32 */
    31.524713071212, /*  32.34 */
    31.544401485667, /*  32.36 */
    31.564090094210, /*  32.38 */
    31.583778896599, /*  32.40 */
    31.603467892595, /*  32.42 */
    31.623157081955, /*  32.44 */
    31.642846464439, /*  32.46 */
    31.662536039808, /*  32.48 */
    31.682225807822, /*  32.50 */
    31.701915768242, /*  32.52 */
    31.721605920830, /*  32.54 */
    31.741296265346, /*  32.56 */
    31.760986801555, /*  32.58 */
    31.780677529218, /*  32.60 */
    31.800368448099, /*  32.62 */
    31.820059557961, /*  32.64 */
    31.839750858568, /*  32.66 */
    31.859442349685, /*  32.68 */
    31.879134031077, /*  32.70 */
    31.898825902510, /*  32.72 */
    31.918517963748, /*  32.74 */
    31.938210214559, /*  32.76 */
    31.957902654709, /*  32.78 */
    31.977595283965, /*  32.80 */
    31.997288102095, /*  32.82 */
    32.016981108866, /*  32.84 */
    32.036674304047, /*  32.86 */
    32.056367687408, /*  32.88 */
    32.076061258716, /*  32.90 */
    32.095755017743, /*  32.92 */
    32.115448964257, /*  32.94 */
    32.135143098030, /*  32.96 */
    32.154837418832, /*  32.98 */
    32.174531926435, /*  33.00 */
    32.194226620610, /*  33.02 */
    32.213921501131, /*  33.04 */
    32.233616567769, /*  33.06 */
    32.253311820297, /*  33.08 */
    32.273007258489, /*  33.10 */
    32.292702882119, /*  33.12 */
    32.312398690961, /*  33.14 */
    32.332094684789, /*  33.16 */
    32.351790863380, /*  33.18 */
    32.371487226507, /*  33.20 */
    32.391183773948, /*  33.22 */
    32.410880505479, /*  33.24 */
    32.430577420876, /*  33.26 */
    32.450274519916, /*  33.28 */
    32.469971802378, /*  33.30 */
    32.489669268038, /*  33.32 */
    32.509366916676, /*  33.34 */
    32.529064748070, /*  33.36 */
    32.548762761999, /*  33.38 */
    32.568460958243, /*  33.40 */
    32.588159336581, /*  33.42 */
    32.607857896795, /*  33.44 */
    32.627556638664, /*  33.46 */
    32.647255561971, /*  33.48 */
    32.666954666496, /*  33.50 */
    32.686653952022, /*  33.52 */
    32.706353418330, /*  33.54 */
    32.726053065204, /*  33.56 */
    32.745752892427, /*  33.58 */
    32.765452899781, /*  33.60 */
    32.785153087052, /*  33.62 */
    32.804853454023, /*  33.64 */
    32.824554000480, /*  33.66 */
    32.844254726206, /*  33.68 */
    32.863955630988, /*  33.70 */
    32.883656714611, /*  33.72 */
    32.903357976861, /*  33.74 */
    32.923059417526, /*  33.76 */
    32.942761036392, /*  33.78 */
    32.962462833247, /*  33.80 */
    32.982164807878, /*  33.82 */
    33.001866960073, /*  33.84 */
    33.021569289621, /*  33.86 */
    33.041271796310, /*  33.88 */
    33.060974479931, /*  33.90 */
    33.080677340272, /*  33.92 */
    33.100380377124, /*  33.94 */
    33.120083590277, /*  33.96 */
    33.139786979522, /*  33.98 */
    33.159490544650, /*  34.00 */
    33.179194285452, /*  34.02 */
    33.198898201720, /*  34.04 */
    33.218602293247, /*  34.06 */
    33.238306559825, /*  34.08 */
    33.258011001247, /*  34.10 */
    33.277715617306, /*  34.12 */
    33.297420407797, /*  34.14 */
    33.317125372512, /*  34.16 */
    33.336830511248, /*  34.18 */
    33.356535823797, /*  34.20 */
    33.376241309957, /*  34.22 */
    33.395946969521, /*  34.24 */
    33.415652802286, /*  34.26 */
    33.435358808049, /*  34.28 */
    33.455064986606, /*  34.30 */
    33.474771337753, /*  34.32 */
    33.494477861289, /*  34.34 */
    33.514184557010, /*  34.36 */
    33.533891424715, /*  34.38 */
    33.553598464202, /*  34.40 */
    33.573305675271, /*  34.42 */
    33.593013057719, /*  34.44 */
    33.612720611347, /*  34.46 */
    33.632428335955, /*  34.48 */
    33.652136231342, /*  34.50 */
    33.671844297309, /*  34.52 */
    33.691552533656, /*  34.54 */
    33.711260940186, /*  34.56 */
    33.730969516700, /*  34.58 */
    33.750678262999, /*  34.60 */
    33.770387178885, /*  34.62 */
    33.790096264162, /*  34.64 */
    33.809805518633, /*  34.66 */
    33.829514942100, /*  34.68 */
    33.849224534367, /*  34.70 */
    33.868934295238, /*  34.72 */
    33.888644224517, /*  34.74 */
    33.908354322010, /*  34.76 */
    33.928064587521, /*  34.78 */
    33.947775020855, /*  34.80 */
    33.967485621818, /*  34.82 */
    33.987196390216, /*  34.84 */
    34.006907325856, /*  34.86 */
    34.026618428544, /*  34.88 */
    34.046329698086, /*  34.90 */
    34.066041134292, /*  34.92 */
    34.085752736967, /*  34.94 */
    34.105464505920, /*  34.96 */
    34.125176440960, /*  34.98 */
    34.144888541894, /*  35.00 */
    34.164600808533, /*  35.02 */
    34.184313240686, /*  35.04 */
    34.204025838161, /*  35.06 */
    34.223738600769, /*  35.08 */
    34.243451528321, /*  35.10 */
    34.263164620627, /*  35.12 */
    34.282877877498, /*  35.14 */
    34.302591298745, /*  35.16 */
    34.322304884180, /*  35.18 */
    34.342018633615, /*  35.20 */
    34.361732546862, /*  35.22 */
    34.381446623733, /*  35.24 */
    34.401160864043, /*  35.26 */
    34.420875267603, /*  35.28 */
    34.440589834227, /*  35.30 */
    34.460304563730, /*  35.32 */
    34.480019455925, /*  35.34 */
    34.499734510627, /*  35.36 */
    34.519449727651, /*  35.38 */
    34.539165106812, /*  35.40 */
    34.558880647925, /*  35.42 */
    34.578596350806, /*  35.44 */
    34.598312215271, /*  35.46 */
    34.618028241137, /*  35.48 */
    34.637744428220, /*  35.50 */
    34.657460776338, /*  35.52 */
    34.677177285307, /*  35.54 */
    34.696893954946, /*  35.56 */
    34.716610785071, /*  35.58 */
    34.736327775503, /*  35.60 */
    34.756044926058, /*  35.62 */
    34.775762236556, /*  35.64 */
    34.795479706817, /*  35.66 */
    34.815197336659, /*  35.68 */
    34.834915125902, /*  35.70 */
    34.854633074368, /*  35.72 */
    34.874351181875, /*  35.74 */
    34.894069448244, /*  35.76 */
    34.913787873298, /*  35.78 */
    34.933506456856, /*  35.80 */
    34.953225198741, /*  35.82 */
    34.972944098775, /*  35.84 */
    34.992663156779, /*  35.86 */
    35.012382372576, /*  35.88 */
    35.032101745990, /*  35.90 */
    35.051821276843, /*  35.92 */
    35.071540964958, /*  35.94 */
    35.091260810160, /*  35.96 */
    35.110980812272, /*  35.98 */
    35.130700971119, /*  36.00 */
    35.150421286525, /*  36.02 */
    35.170141758315, /*  36.04 */
    35.189862386315, /*  36.06 */
    35.209583170349, /*  36.08 */
    35.229304110244, /*  36.10 */
    35.249025205825, /*  36.12 */
    35.268746456920, /*  36.14 */
    35.288467863354, /*  36.16 */
    35.308189424955, /*  36.18 */
    35.327911141549, /*  36.20 */
    35.347633012965, /*  36.22 */
    35.367355039030, /*  36.24 */
    35.387077219572, /*  36.26 */
    35.406799554419, /*  36.28 */
    35.426522043401, /*  36.30 */
    35.446244686346, /*  36.32 */
    35.465967483084, /*  36.34 */
    35.485690433443, /*  36.36 */
    35.505413537254, /*  36.38 */
    35.525136794347, /*  36.40 */
    35.544860204552, /*  36.42 */
    35.564583767700, /*  36.44 */
    35.584307483622, /*  36.46 */
    35.604031352148, /*  36.48 */
    35.623755373111, /*  36.50 */
    35.643479546342, /*  36.52 */
    35.663203871673, /*  36.54 */
    35.682928348937, /*  36.56 */
    35.702652977966, /*  36.58 */
    35.722377758593, /*  36.60 */
    35.742102690651, /*  36.62 */
    35.761827773974, /*  36.64 */
    35.781553008395, /*  36.66 */
    35.801278393748, /*  36.68 */
    35.821003929868, /*  36.70 */
    35.840729616589, /*  36.72 */
    35.860455453745, /*  36.74 */
    35.880181441173, /*  36.76 */
    35.899907578706, /*  36.78 */
    35.919633866182, /*  36.80 */
    35.939360303435, /*  36.82 */
    35.959086890302, /*  36.84 */
    35.978813626619, /*  36.86 */
    35.998540512223, /*  36.88 */
    36.018267546951, /*  36.90 */
    36.037994730640, /*  36.92 */
    36.057722063128, /*  36.94 */
    36.077449544252, /*  36.96 */
    36.097177173851, /*  36.98 */
    36.116904951762, /*  37.00 */
    36.136632877824, /*  37.02 */
    36.156360951876, /*  37.04 */
    36.176089173757, /*  37.06 */
    36.195817543307, /*  37.08 */
    36.215546060364, /*  37.10 */
    36.235274724770, /*  37.12 */
    36.255003536363, /*  37.14 */
    36.274732494985, /*  37.16 */
    36.294461600475, /*  37.18 */
    36.314190852675, /*  37.20 */
    36.333920251427, /*  37.22 */
    36.353649796571, /*  37.24 */
    36.373379487949, /*  37.26 */
    36.393109325402, /*  37.28 */
    36.412839308774, /*  37.30 */
    36.432569437907, /*  37.32 */
    36.452299712643, /*  37.34 */
    36.472030132825, /*  37.36 */
    36.491760698297, /*  37.38 */
    36.511491408902, /*  37.40 */
    36.531222264483, /*  37.42 */
    36.550953264885, /*  37.44 */
    36.570684409951, /*  37.46 */
    36.590415699527, /*  37.48 */
    36.610147133457, /*  37.50 */
    36.629878711585, /*  37.52 */
    36.649610433758, /*  37.54 */
    36.669342299821, /*  37.56 */
    36.689074309618, /*  37.58 */
    36.708806462997, /*  37.60 */
    36.728538759804, /*  37.62 */
    36.748271199884, /*  37.64 */
    36.768003783085, /*  37.66 */
    36.787736509254, /*  37.68 */
    36.807469378237, /*  37.70 */
    36.827202389883, /*  37.72 */
    36.846935544038, /*  37.74 */
    36.866668840552, /*  37.76 */
    36.886402279271, /*  37.78 */
    36.906135860045, /*  37.80 */
    36.925869582721, /*  37.82 */
    36.945603447150, /*  37.84 */
    36.965337453179, /*  37.86 */
    36.985071600660, /*  37.88 */
    37.004805889440, /*  37.90 */
    37.024540319370, /*  37.92 */
    37.044274890300, /*  37.94 */
    37.064009602080, /*  37.96 */
    37.083744454562, /*  37.98 */
    37.103479447595, /*  38.00 */
    37.123214581030, /*  38.02 */
    37.142949854720, /*  38.04 */
    37.162685268516, /*  38.06 */
    37.182420822269, /*  38.08 */
    37.202156515831, /*  38.10 */
    37.221892349054, /*  38.12 */
    37.241628321792, /*  38.14 */
    37.261364433896, /*  38.16 */
    37.281100685220, /*  38.18 */
    37.300837075617, /*  38.20 */
    37.320573604940, /*  38.22 */
    37.340310273043, /*  38.24 */
    37.360047079780, /*  38.26 */
    37.379784025004, /*  38.28 */
    37.399521108570, /*  38.30 */
    37.419258330333, /*  38.32 */
    37.438995690147, /*  38.34 */
    37.458733187868, /*  38.36 */
    37.478470823350, /*  38.38 */
    37.498208596450, /*  38.40 */
    37.517946507022, /*  38.42 */
    37.537684554922, /*  38.44 */
    37.557422740008, /*  38.46 */
    37.577161062135, /*  38.48 */
    37.596899521160, /*  38.50 */
    37.616638116939, /*  38.52 */
    37.636376849330, /*  38.54 */
    37.656115718189, /*  38.56 */
    37.675854723376, /*  38.58 */
    37.695593864746, /*  38.60 */
    37.715333142158, /*  38.62 */
    37.735072555471, /*  38.64 */
    37.754812104542, /*  38.66 */
    37.774551789230, /*  38.68 */
    37.794291609394, /*  38.70 */
    37.814031564894, /*  38.72 */
    37.833771655587, /*  38.74 */
    37.853511881335, /*  38.76 */
    37.873252241996, /*  38.78 */
    37.892992737430, /*  38.80 */
    37.912733367498, /*  38.82 */
    37.932474132060, /*  38.84 */
    37.952215030976, /*  38.86 */
    37.971956064108, /*  38.88 */
    37.991697231315, /*  38.90 */
    38.011438532461, /*  38.92 */
    38.031179967405, /*  38.94 */
    38.050921536010, /*  38.96 */
    38.070663238137, /*  38.98 */
    38.090405073649, /*  39.00 */
    38.110147042407, /*  39.02 */
    38.129889144275, /*  39.04 */
    38.149631379115, /*  39.06 */
    38.169373746790, /*  39.08 */
    38.189116247163, /*  39.10 */
    38.208858880097, /*  39.12 */
    38.228601645456, /*  39.14 */
    38.248344543105, /*  39.16 */
    38.268087572906, /*  39.18 */
    38.287830734724, /*  39.20 */
    38.307574028423, /*  39.22 */
    38.327317453869, /*  39.24 */
    38.347061010925, /*  39.26 */
    38.366804699457, /*  39.28 */
    38.386548519330, /*  39.30 */
    38.406292470410, /*  39.32 */
    38.426036552562, /*  39.34 */
    38.445780765652, /*  39.36 */
    38.465525109546, /*  39.38 */
    38.485269584111, /*  39.40 */
    38.505014189212, /*  39.42 */
    38.524758924717, /*  39.44 */
    38.544503790493, /*  39.46 */
    38.564248786405, /*  39.48 */
    38.583993912323, /*  39.50 */
    38.603739168113, /*  39.52 */
    38.623484553643, /*  39.54 */
    38.643230068781, /*  39.56 */
    38.662975713395, /*  39.58 */
    38.682721487353, /*  39.60 */
    38.702467390524, /*  39.62 */
    38.722213422776, /*  39.64 */
    38.741959583979, /*  39.66 */
    38.761705874001, /*  39.68 */
    38.781452292712, /*  39.70 */
    38.801198839981, /*  39.72 */
    38.820945515679, /*  39.74 */
    38.840692319674, /*  39.76 */
    38.860439251837, /*  39.78 */
    38.880186312038, /*  39.80 */
    38.899933500149, /*  39.82 */
    38.919680816038, /*  39.84 */
    38.939428259578, /*  39.86 */
    38.959175830640, /*  39.88 */
    38.978923529094, /*  39.90 */
    38.998671354812, /*  39.92 */
    39.018419307666, /*  39.94 */
    39.038167387528, /*  39.96 */
    39.057915594269, /*  39.98 */
    39.077663927762, /*  40.00 */
    39.097412387880, /*  40.02 */
    39.117160974495, /*  40.04 */
    39.136909687479, /*  40.06 */
    39.156658526707, /*  40.08 */
    39.176407492050, /*  40.10 */
    39.196156583383, /*  40.12 */
    39.215905800579, /*  40.14 */
    39.235655143511, /*  40.16 */
    39.255404612054, /*  40.18 */
    39.275154206083, /*  40.20 */
    39.294903925470, /*  40.22 */
    39.314653770091, /*  40.24 */
    39.334403739821, /*  40.26 */
    39.354153834534, /*  40.28 */
    39.373904054106, /*  40.30 */
    39.393654398412, /*  40.32 */
    39.413404867326, /*  40.34 */
    39.433155460726, /*  40.36 */
    39.452906178486, /*  40.38 */
    39.472657020483, /*  40.40 */
    39.492407986593, /*  40.42 */
    39.512159076693, /*  40.44 */
    39.531910290658, /*  40.46 */
    39.551661628366, /*  40.48 */
    39.571413089694, /*  40.50 */
    39.591164674519, /*  40.52 */
    39.610916382718, /*  40.54 */
    39.630668214168, /*  40.56 */
    39.650420168748, /*  40.58 */
    39.670172246335, /*  40.60 */
    39.689924446807, /*  40.62 */
    39.709676770042, /*  40.64 */
    39.729429215920, /*  40.66 */
    39.749181784317, /*  40.68 */
    39.768934475114, /*  40.70 */
    39.788687288190, /*  40.72 */
    39.808440223422, /*  40.74 */
    39.828193280691, /*  40.76 */
    39.847946459876, /*  40.78 */
    39.867699760857, /*  40.80 */
    39.887453183513, /*  40.82 */
    39.907206727725, /*  40.84 */
    39.926960393373, /*  40.86 */
    39.946714180337, /*  40.88 */
    39.966468088498, /*  40.90 */
    39.986222117736, /*  40.92 */
    40.005976267933, /*  40.94 */
    40.025730538969, /*  40.96 */
    40.045484930725, /*  40.98 */
    40.065239443084, /*  41.00 */
    40.084994075926, /*  41.02 */
    40.104748829134, /*  41.04 */
    40.124503702590, /*  41.06 */
    40.144258696175, /*  41.08 */
    40.164013809771, /*  41.10 */
    40.183769043262, /*  41.12 */
    40.203524396531, /*  41.14 */
    40.223279869459, /*  41.16 */
    40.243035461929, /*  41.18 */
    40.262791173826, /*  41.20 */
    40.282547005032, /*  41.22 */
    40.302302955431, /*  41.24 */
    40.322059024906, /*  41.26 */
    40.341815213342, /*  41.28 */
    40.361571520621, /*  41.30 */
    40.381327946630, /*  41.32 */
    40.401084491251, /*  41.34 */
    40.420841154369, /*  41.36 */
    40.440597935870, /*  41.38 */
    40.460354835638, /*  41.40 */
    40.480111853557, /*  41.42 */
    40.499868989514, /*  41.44 */
    40.519626243393, /*  41.46 */
    40.539383615080, /*  41.48 */
    40.559141104461, /*  41.50 */
    40.578898711421, /*  41.52 */
    40.598656435847, /*  41.54 */
    40.618414277625, /*  41.56 */
    40.638172236641, /*  41.58 */
    40.657930312782, /*  41.60 */
    40.677688505934, /*  41.62 */
    40.697446815984, /*  41.64 */
    40.717205242819, /*  41.66 */
    40.736963786327, /*  41.68 */
    40.756722446395, /*  41.70 */
    40.776481222910, /*  41.72 */
    40.796240115759, /*  41.74 */
    40.815999124832, /*  41.76 */
    40.835758250015, /*  41.78 */
    40.855517491197, /*  41.80 */
    40.875276848267, /*  41.82 */
    40.895036321112, /*  41.84 */
    40.914795909621, /*  41.86 */
    40.934555613684, /*  41.88 */
    40.954315433189, /*  41.90 */
    40.974075368025, /*  41.92 */
    40.993835418081, /*  41.94 */
    41.013595583247, /*  41.96 */
    41.033355863413, /*  41.98 */
    41.053116258468, /*  42.00 */
    41.072876768303, /*  42.02 */
    41.092637392806, /*  42.04 */
    41.112398131869, /*  42.06 */
    41.132158985382, /*  42.08 */
    41.151919953236, /*  42.10 */
    41.171681035320, /*  42.12 */
    41.191442231527, /*  42.14 */
    41.211203541746, /*  42.16 */
    41.230964965870, /*  42.18 */
    41.250726503789, /*  42.20 */
    41.270488155396, /*  42.22 */
    41.290249920581, /*  42.24 */
    41.310011799236, /*  42.26 */
    41.329773791254, /*  42.28 */
    41.349535896526, /*  42.30 */
    41.369298114945, /*  42.32 */
    41.389060446403, /*  42.34 */
    41.408822890793, /*  42.36 */
    41.428585448007, /*  42.38 */
    41.448348117939, /*  42.40 */
    41.468110900481, /*  42.42 */
    41.487873795526, /*  42.44 */
    41.507636802968, /*  42.46 */
    41.527399922700, /*  42.48 */
    41.547163154617, /*  42.50 */
    41.566926498611, /*  42.52 */
    41.586689954577, /*  42.54 */
    41.606453522408, /*  42.56 */
    41.626217201999, /*  42.58 */
    41.645980993245, /*  42.60 */
    41.665744896040, /*  42.62 */
    41.685508910278, /*  42.64 */
    41.705273035854, /*  42.66 */
    41.725037272664, /*  42.68 */
    41.744801620602, /*  42.70 */
    41.764566079564, /*  42.72 */
    41.784330649445, /*  42.74 */
    41.804095330141, /*  42.76 */
    41.823860121548, /*  42.78 */
    41.843625023560, /*  42.80 */
    41.863390036076, /*  42.82 */
    41.883155158989, /*  42.84 */
    41.902920392197, /*  42.86 */
    41.922685735597, /*  42.88 */
    41.942451189085, /*  42.90 */
    41.962216752557, /*  42.92 */
    41.981982425910, /*  42.94 */
    42.001748209042, /*  42.96 */
    42.021514101850, /*  42.98 */
    42.041280104231, /*  43.00 */
    42.061046216082, /*  43.02 */
    42.080812437301, /*  43.04 */
    42.100578767786, /*  43.06 */
    42.120345207435, /*  43.08 */
    42.140111756146, /*  43.10 */
    42.159878413817, /*  43.12 */
    42.179645180346, /*  43.14 */
    42.199412055631, /*  43.16 */
    42.219179039572, /*  43.18 */
    42.238946132067, /*  43.20 */
    42.258713333016, /*  43.22 */
    42.278480642316, /*  43.24 */
    42.298248059867, /*  43.26 */
    42.318015585569, /*  43.28 */
    42.337783219321, /*  43.30 */
    42.357550961023, /*  43.32 */
    42.377318810573, /*  43.34 */
    42.397086767873, /*  43.36 */
    42.416854832823, /*  43.38 */
    42.436623005321, /*  43.40 */
    42.456391285270, /*  43.42 */
    42.476159672568, /*  43.44 */
    42.495928167117, /*  43.46 */
    42.515696768817, /*  43.48 */
    42.535465477570, /*  43.50 */
    42.555234293276, /*  43.52 */
    42.575003215836, /*  43.54 */
    42.594772245151, /*  43.56 */
    42.614541381124, /*  43.58 */
    42.634310623655, /*  43.60 */
    42.654079972647, /*  43.62 */
    42.673849428000, /*  43.64 */
    42.693618989617, /*  43.66 */
    42.713388657400, /*  43.68 */
    42.733158431252, /*  43.70 */
    42.752928311074, /*  43.72 */
    42.772698296768, /*  43.74 */
    42.792468388239, /*  43.76 */
    42.812238585388, /*  43.78 */
    42.832008888118, /*  43.80 */
    42.851779296333, /*  43.82 */
    42.871549809935, /*  43.84 */
    42.891320428827, /*  43.86 */
    42.911091152914, /*  43.88 */
    42.930861982099, /*  43.90 */
    42.950632916284, /*  43.92 */
    42.970403955376, /*  43.94 */
    42.990175099276, /*  43.96 */
    43.009946347889, /*  43.98 */
    43.029717701120, /*  44.00 */
    43.049489158872, /*  44.02 */
    43.069260721051, /*  44.04 */
    43.089032387561, /*  44.06 */
    43.108804158306, /*  44.08 */
    43.128576033192, /*  44.10 */
    43.148348012123, /*  44.12 */
    43.168120095004, /*  44.14 */
    43.187892281742, /*  44.16 */
    43.207664572241, /*  44.18 */
    43.227436966406, /*  44.20 */
    43.247209464144, /*  44.22 */
    43.266982065359, /*  44.24 */
    43.286754769959, /*  44.26 */
    43.306527577849, /*  44.28 */
    43.326300488935, /*  44.30 */
    43.346073503124, /*  44.32 */
    43.365846620321, /*  44.34 */
    43.385619840434, /*  44.36 */
    43.405393163369, /*  44.38 */
    43.425166589033, /*  44.40 */
    43.444940117333, /*  44.42 */
    43.464713748176, /*  44.44 */
    43.484487481469, /*  44.46 */
    43.504261317119, /*  44.48 */
    43.524035255033, /*  44.50 */
    43.543809295121, /*  44.52 */
    43.563583437288, /*  44.54 */
    43.583357681443, /*  44.56 */
    43.603132027493, /*  44.58 */
    43.622906475348, /*  44.60 */
    43.642681024914, /*  44.62 */
    43.662455676101, /*  44.64 */
    43.682230428816, /*  44.66 */
    43.702005282968, /*  44.68 */
    43.721780238467, /*  44.70 */
    43.741555295220, /*  44.72 */
    43.761330453136, /*  44.74 */
    43.781105712125, /*  44.76 */
    43.800881072096, /*  44.78 */
    43.820656532959, /*  44.80 */
    43.840432094621, /*  44.82 */
    43.860207756994, /*  44.84 */
    43.879983519986, /*  44.86 */
    43.899759383508, /*  44.88 */
    43.919535347469, /*  44.90 */
    43.939311411779, /*  44.92 */
    43.959087576349, /*  44.94 */
    43.978863841088, /*  44.96 */
    43.998640205908, /*  44.98 */
    44.018416670718, /*  45.00 */
    44.038193235429, /*  45.02 */
    44.057969899953, /*  45.04 */
    44.077746664199, /*  45.06 */
    44.097523528079, /*  45.08 */
    44.117300491504, /*  45.10 */
    44.137077554385, /*  45.12 */
    44.156854716633, /*  45.14 */
    44.176631978160, /*  45.16 */
    44.196409338878, /*  45.18 */
    44.216186798698, /*  45.20 */
    44.235964357531, /*  45.22 */
    44.255742015291, /*  45.24 */
    44.275519771888, /*  45.26 */
    44.295297627236, /*  45.28 */
    44.315075581246, /*  45.30 */
    44.334853633830, /*  45.32 */
    44.354631784902, /*  45.34 */
    44.374410034374, /*  45.36 */
    44.394188382158, /*  45.38 */
    44.413966828167, /*  45.40 */
    44.433745372315, /*  45.42 */
    44.453524014515, /*  45.44 */
    44.473302754679, /*  45.46 */
    44.493081592722, /*  45.48 */
    44.512860528555, /*  45.50 */
    44.532639562094, /*  45.52 */
    44.552418693252, /*  45.54 */
    44.572197921942, /*  45.56 */
    44.591977248078, /*  45.58 */
    44.611756671575, /*  45.60 */
    44.631536192346, /*  45.62 */
    44.651315810305, /*  45.64 */
    44.671095525368, /*  45.66 */
    44.690875337449, /*  45.68 */
    44.710655246461, /*  45.70 */
    44.730435252320, /*  45.72 */
    44.750215354941, /*  45.74 */
    44.769995554238, /*  45.76 */
    44.789775850127, /*  45.78 */
    44.809556242522, /*  45.80 */
    44.829336731339, /*  45.82 */
    44.849117316494, /*  45.84 */
    44.868897997901, /*  45.86 */
    44.888678775476, /*  45.88 */
    44.908459649136, /*  45.90 */
    44.928240618795, /*  45.92 */
    44.948021684370, /*  45.94 */
    44.967802845776, /*  45.96 */
    44.987584102931, /*  45.98 */
    45.007365455749, /*  46.00 */
    45.027146904148, /*  46.02 */
    45.046928448044, /*  46.04 */
    45.066710087354, /*  46.06 */
    45.086491821993, /*  46.08 */
    45.106273651880, /*  46.10 */
    45.126055576930, /*  46.12 */
    45.145837597061, /*  46.14 */
    45.165619712191, /*  46.16 */
    45.185401922235, /*  46.18 */
    45.205184227112, /*  46.20 */
    45.224966626739, /*  46.22 */
    45.244749121033, /*  46.24 */
    45.264531709913, /*  46.26 */
    45.284314393296, /*  46.28 */
    45.304097171099, /*  46.30 */
    45.323880043241, /*  46.32 */
    45.343663009641, /*  46.34 */
    45.363446070215, /*  46.36 */
    45.383229224883, /*  46.38 */
    45.403012473562, /*  46.40 */
    45.422795816172, /*  46.42 */
    45.442579252630, /*  46.44 */
    45.462362782857, /*  46.46 */
    45.482146406769, /*  46.48 */
    45.501930124287, /*  46.50 */
    45.521713935330, /*  46.52 */
    45.541497839816, /*  46.54 */
    45.561281837665, /*  46.56 */
    45.581065928796, /*  46.58 */
    45.600850113129, /*  46.60 */
    45.620634390583, /*  46.62 */
    45.640418761078, /*  46.64 */
    45.660203224534, /*  46.66 */
    45.679987780870, /*  46.68 */
    45.699772430006, /*  46.70 */
    45.719557171864, /*  46.72 */
    45.739342006362, /*  46.74 */
    45.759126933420, /*  46.76 */
    45.778911952961, /*  46.78 */
    45.798697064903, /*  46.80 */
    45.818482269168, /*  46.82 */
    45.838267565676, /*  46.84 */
    45.858052954348, /*  46.86 */
    45.877838435105, /*  46.88 */
    45.897624007868, /*  46.90 */
    45.917409672558, /*  46.92 */
    45.937195429096, /*  46.94 */
    45.956981277404, /*  46.96 */
    45.976767217403, /*  46.98 */
    45.996553249014, /*  47.00 */
    46.016339372159, /*  47.02 */
    46.036125586760, /*  47.04 */
    46.055911892739, /*  47.06 */
    46.075698290017, /*  47.08 */
    46.095484778517, /*  47.10 */
    46.115271358160, /*  47.12 */
    46.135058028869, /*  47.14 */
    46.154844790567, /*  47.16 */
    46.174631643174, /*  47.18 */
    46.194418586615, /*  47.20 */
    46.214205620812, /*  47.22 */
    46.233992745687, /*  47.24 */
    46.253779961163, /*  47.26 */
    46.273567267163, /*  47.28 */
    46.293354663611, /*  47.30 */
    46.313142150429, /*  47.32 */
    46.332929727540, /*  47.34 */
    46.352717394867, /*  47.36 */
    46.372505152335, /*  47.38 */
    46.392292999866, /*  47.40 */
    46.412080937385, /*  47.42 */
    46.431868964814, /*  47.44 */
    46.451657082079, /*  47.46 */
    46.471445289101, /*  47.48 */
    46.491233585806, /*  47.50 */
    46.511021972118, /*  47.52 */
    46.530810447960, /*  47.54 */
    46.550599013258, /*  47.56 */
    46.570387667935, /*  47.58 */
    46.590176411916, /*  47.60 */
    46.609965245125, /*  47.62 */
    46.629754167487, /*  47.64 */
    46.649543178927, /*  47.66 */
    46.669332279369, /*  47.68 */
    46.689121468739, /*  47.70 */
    46.708910746962, /*  47.72 */
    46.728700113963, /*  47.74 */
    46.748489569666, /*  47.76 */
    46.768279113998, /*  47.78 */
    46.788068746883, /*  47.80 */
    46.807858468247, /*  47.82 */
    46.827648278017, /*  47.84 */
    46.847438176116, /*  47.86 */
    46.867228162472, /*  47.88 */
    46.887018237011, /*  47.90 */
    46.906808399657, /*  47.92 */
    46.926598650337, /*  47.94 */
    46.946388988978, /*  47.96 */
    46.966179415505, /*  47.98 */
    46.985969929845, /*  48.00 */
    47.005760531924, /*  48.02 */
    47.025551221670, /*  48.04 */
    47.045341999008, /*  48.06 */
    47.065132863865, /*  48.08 */
    47.084923816168, /*  48.10 */
    47.104714855843, /*  48.12 */
    47.124505982819, /*  48.14 */
    47.144297197022, /*  48.16 */
    47.164088498379, /*  48.18 */
    47.183879886817, /*  48.20 */
    47.203671362264, /*  48.22 */
    47.223462924648, /*  48.24 */
    47.243254573895, /*  48.26 */
    47.263046309933, /*  48.28 */
    47.282838132691, /*  48.30 */
    47.302630042095, /*  48.32 */
    47.322422038074, /*  48.34 */
    47.342214120556, /*  48.36 */
    47.362006289469, /*  48.38 */
    47.381798544740, /*  48.40 */
    47.401590886299, /*  48.42 */
    47.421383314074, /*  48.44 */
    47.441175827993, /*  48.46 */
    47.460968427984, /*  48.48 */
    47.480761113976, /*  48.50 */
    47.500553885898, /*  48.52 */
    47.520346743679, /*  48.54 */
    47.540139687248, /*  48.56 */
    47.559932716533, /*  48.58 */
    47.579725831463, /*  48.60 */
    47.599519031968, /*  48.62 */
    47.619312317977, /*  48.64 */
    47.639105689420, /*  48.66 */
    47.658899146225, /*  48.68 */
    47.678692688322, /*  48.70 */
    47.698486315641, /*  48.72 */
    47.718280028111, /*  48.74 */
    47.738073825662, /*  48.76 */
    47.757867708225, /*  48.78 */
    47.777661675728, /*  48.80 */
    47.797455728102, /*  48.82 */
    47.817249865278, /*  48.84 */
    47.837044087184, /*  48.86 */
    47.856838393752, /*  48.88 */
    47.876632784912, /*  48.90 */
    47.896427260594, /*  48.92 */
    47.916221820729, /*  48.94 */
    47.936016465247, /*  48.96 */
    47.955811194080, /*  48.98 */
    47.975606007157, /*  49.00 */
    47.995400904411, /*  49.02 */
    48.015195885771, /*  49.04 */
    48.034990951169, /*  49.06 */
    48.054786100535, /*  49.08 */
    48.074581333802, /*  49.10 */
    48.094376650901, /*  49.12 */
    48.114172051762, /*  49.14 */
    48.133967536317, /*  49.16 */
    48.153763104499, /*  49.18 */
    48.173558756238, /*  49.20 */
    48.193354491466, /*  49.22 */
    48.213150310115, /*  49.24 */
    48.232946212116, /*  49.26 */
    48.252742197403, /*  49.28 */
    48.272538265907, /*  49.30 */
    48.292334417559, /*  49.32 */
    48.312130652293, /*  49.34 */
    48.331926970041, /*  49.36 */
    48.351723370734, /*  49.38 */
    48.371519854306, /*  49.40 */
    48.391316420689, /*  49.42 */
    48.411113069815, /*  49.44 */
    48.430909801618, /*  49.46 */
    48.450706616029, /*  49.48 */
    48.470503512983, /*  49.50 */
    48.490300492411, /*  49.52 */
    48.510097554248, /*  49.54 */
    48.529894698425, /*  49.56 */
    48.549691924877, /*  49.58 */
    48.569489233537, /*  49.60 */
    48.589286624337, /*  49.62 */
    48.609084097212, /*  49.64 */
    48.628881652094, /*  49.66 */
    48.648679288918, /*  49.68 */
    48.668477007618, /*  49.70 */
    48.688274808126, /*  49.72 */
    48.708072690377, /*  49.74 */
    48.727870654305, /*  49.76 */
    48.747668699843, /*  49.78 */
    48.767466826927, /*  49.80 */
    48.787265035489, /*  49.82 */
    48.807063325465, /*  49.84 */
    48.826861696788, /*  49.86 */
    48.846660149393, /*  49.88 */
    48.866458683215, /*  49.90 */
    48.886257298188, /*  49.92 */
    48.906055994246, /*  49.94 */
    48.925854771325, /*  49.96 */
    48.945653629359, /*  49.98 */
    48.965452568283, /*  50.00 */
    0};

/*    Table of the differential of lgi0 (kappa) wrt kappa. This
function is also known as A() in the techical report. It is the
expected value of cos(theta) when theta has a VonMises2 density with
zero mean and concentration theta.
    There are 2501 values tabulated for kappa from 0 to 50 in steps
of 0.02  */

static double atab[] = {
    /* AA table */
    0.000000000000, /*   0.00 */
    0.009999500033, /*   0.02 */
    0.019996001066, /*   0.04 */
    0.029986508095, /*   0.06 */
    0.039968034096, /*   0.08 */
    0.049937603988, /*   0.10 */
    0.059892258560, /*   0.12 */
    0.069829058352, /*   0.14 */
    0.079745087482, /*   0.16 */
    0.089637457400, /*   0.18 */
    0.099503310574, /*   0.20 */
    0.109339824079, /*   0.22 */
    0.119144213094, /*   0.24 */
    0.128913734294, /*   0.26 */
    0.138645689122, /*   0.28 */
    0.148337426941, /*   0.30 */
    0.157986348060, /*   0.32 */
    0.167589906616, /*   0.34 */
    0.177145613314, /*   0.36 */
    0.186651038025, /*   0.38 */
    0.196103812218, /*   0.40 */
    0.205501631245, /*   0.42 */
    0.214842256457, /*   0.44 */
    0.224123517160, /*   0.46 */
    0.233343312400, /*   0.48 */
    0.242499612581, /*   0.50 */
    0.251590460915, /*   0.52 */
    0.260613974699, /*   0.54 */
    0.269568346425, /*   0.56 */
    0.278451844724, /*   0.58 */
    0.287262815131, /*   0.60 */
    0.295999680705, /*   0.62 */
    0.304660942467, /*   0.64 */
    0.313245179692, /*   0.66 */
    0.321751050039, /*   0.68 */
    0.330177289533, /*   0.70 */
    0.338522712399, /*   0.72 */
    0.346786210752, /*   0.74 */
    0.354966754153, /*   0.76 */
    0.363063389033, /*   0.78 */
    0.371075237988, /*   0.80 */
    0.379001498958, /*   0.82 */
    0.386841444292, /*   0.84 */
    0.394594419701, /*   0.86 */
    0.402259843119, /*   0.88 */
    0.409837203460, /*   0.90 */
    0.417326059292, /*   0.92 */
    0.424726037429, /*   0.94 */
    0.432036831449, /*   0.96 */
    0.439258200142, /*   0.98 */
    0.446389965897, /*   1.00 */
    0.453432013032, /*   1.02 */
    0.460384286081, /*   1.04 */
    0.467246788030, /*   1.06 */
    0.474019578520, /*   1.08 */
    0.480702772020, /*   1.10 */
    0.487296535974, /*   1.12 */
    0.493801088924, /*   1.14 */
    0.500216698624, /*   1.16 */
    0.506543680142, /*   1.18 */
    0.512782393958, /*   1.20 */
    0.518933244059, /*   1.22 */
    0.524996676041, /*   1.24 */
    0.530973175222, /*   1.26 */
    0.536863264758, /*   1.28 */
    0.542667503786, /*   1.30 */
    0.548386485580, /*   1.32 */
    0.554020835729, /*   1.34 */
    0.559571210341, /*   1.36 */
    0.565038294282, /*   1.38 */
    0.570422799433, /*   1.40 */
    0.575725462991, /*   1.42 */
    0.580947045799, /*   1.44 */
    0.586088330712, /*   1.46 */
    0.591150121008, /*   1.48 */
    0.596133238831, /*   1.50 */
    0.601038523678, /*   1.52 */
    0.605866830928, /*   1.54 */
    0.610619030416, /*   1.56 */
    0.615296005044, /*   1.58 */
    0.619898649445, /*   1.60 */
    0.624427868685, /*   1.62 */
    0.628884577008, /*   1.64 */
    0.633269696635, /*   1.66 */
    0.637584156598, /*   1.68 */
    0.641828891624, /*   1.70 */
    0.646004841062, /*   1.72 */
    0.650112947853, /*   1.74 */
    0.654154157548, /*   1.76 */
    0.658129417360, /*   1.78 */
    0.662039675273, /*   1.80 */
    0.665885879176, /*   1.82 */
    0.669668976053, /*   1.84 */
    0.673389911203, /*   1.86 */
    0.677049627505, /*   1.88 */
    0.680649064722, /*   1.90 */
    0.684189158841, /*   1.92 */
    0.687670841446, /*   1.94 */
    0.691095039137, /*   1.96 */
    0.694462672975, /*   1.98 */
    0.697774657964, /*   2.00 */
    0.701031902568, /*   2.02 */
    0.704235308255, /*   2.04 */
    0.707385769077, /*   2.06 */
    0.710484171277, /*   2.08 */
    0.713531392925, /*   2.10 */
    0.716528303583, /*   2.12 */
    0.719475763993, /*   2.14 */
    0.722374625800, /*   2.16 */
    0.725225731285, /*   2.18 */
    0.728029913137, /*   2.20 */
    0.730787994236, /*   2.22 */
    0.733500787465, /*   2.24 */
    0.736169095541, /*   2.26 */
    0.738793710862, /*   2.28 */
    0.741375415377, /*   2.30 */
    0.743914980475, /*   2.32 */
    0.746413166889, /*   2.34 */
    0.748870724615, /*   2.36 */
    0.751288392849, /*   2.38 */
    0.753666899938, /*   2.40 */
    0.756006963346, /*   2.42 */
    0.758309289628, /*   2.44 */
    0.760574574427, /*   2.46 */
    0.762803502471, /*   2.48 */
    0.764996747589, /*   2.50 */
    0.767154972731, /*   2.52 */
    0.769278830009, /*   2.54 */
    0.771368960730, /*   2.56 */
    0.773425995455, /*   2.58 */
    0.775450554056, /*   2.60 */
    0.777443245779, /*   2.62 */
    0.779404669324, /*   2.64 */
    0.781335412920, /*   2.66 */
    0.783236054413, /*   2.68 */
    0.785107161357, /*   2.70 */
    0.786949291112, /*   2.72 */
    0.788762990943, /*   2.74 */
    0.790548798126, /*   2.76 */
    0.792307240060, /*   2.78 */
    0.794038834374, /*   2.80 */
    0.795744089051, /*   2.82 */
    0.797423502538, /*   2.84 */
    0.799077563874, /*   2.86 */
    0.800706752812, /*   2.88 */
    0.802311539941, /*   2.90 */
    0.803892386818, /*   2.92 */
    0.805449746093, /*   2.94 */
    0.806984061641, /*   2.96 */
    0.808495768690, /*   2.98 */
    0.809985293957, /*   3.00 */
    0.811453055774, /*   3.02 */
    0.812899464228, /*   3.04 */
    0.814324921284, /*   3.06 */
    0.815729820928, /*   3.08 */
    0.817114549291, /*   3.10 */
    0.818479484787, /*   3.12 */
    0.819824998240, /*   3.14 */
    0.821151453020, /*   3.16 */
    0.822459205171, /*   3.18 */
    0.823748603543, /*   3.20 */
    0.825019989916, /*   3.22 */
    0.826273699136, /*   3.24 */
    0.827510059237, /*   3.26 */
    0.828729391566, /*   3.28 */
    0.829932010913, /*   3.30 */
    0.831118225630, /*   3.32 */
    0.832288337755, /*   3.34 */
    0.833442643135, /*   3.36 */
    0.834581431542, /*   3.38 */
    0.835704986794, /*   3.40 */
    0.836813586868, /*   3.42 */
    0.837907504023, /*   3.44 */
    0.838987004903, /*   3.46 */
    0.840052350658, /*   3.48 */
    0.841103797051, /*   3.50 */
    0.842141594564, /*   3.52 */
    0.843165988512, /*   3.54 */
    0.844177219140, /*   3.56 */
    0.845175521737, /*   3.58 */
    0.846161126729, /*   3.60 */
    0.847134259785, /*   3.62 */
    0.848095141912, /*   3.64 */
    0.849043989559, /*   3.66 */
    0.849981014704, /*   3.68 */
    0.850906424954, /*   3.70 */
    0.851820423636, /*   3.72 */
    0.852723209885, /*   3.74 */
    0.853614978736, /*   3.76 */
    0.854495921209, /*   3.78 */
    0.855366224399, /*   3.80 */
    0.856226071552, /*   3.82 */
    0.857075642158, /*   3.84 */
    0.857915112021, /*   3.86 */
    0.858744653348, /*   3.88 */
    0.859564434819, /*   3.90 */
    0.860374621669, /*   3.92 */
    0.861175375760, /*   3.94 */
    0.861966855652, /*   3.96 */
    0.862749216682, /*   3.98 */
    0.863522611025, /*   4.00 */
    0.864287187770, /*   4.02 */
    0.865043092984, /*   4.04 */
    0.865790469780, /*   4.06 */
    0.866529458379, /*   4.08 */
    0.867260196177, /*   4.10 */
    0.867982817802, /*   4.12 */
    0.868697455178, /*   4.14 */
    0.869404237587, /*   4.16 */
    0.870103291720, /*   4.18 */
    0.870794741739, /*   4.20 */
    0.871478709331, /*   4.22 */
    0.872155313764, /*   4.24 */
    0.872824671937, /*   4.26 */
    0.873486898435, /*   4.28 */
    0.874142105578, /*   4.30 */
    0.874790403472, /*   4.32 */
    0.875431900058, /*   4.34 */
    0.876066701157, /*   4.36 */
    0.876694910521, /*   4.38 */
    0.877316629872, /*   4.40 */
    0.877931958954, /*   4.42 */
    0.878540995571, /*   4.44 */
    0.879143835632, /*   4.46 */
    0.879740573191, /*   4.48 */
    0.880331300490, /*   4.50 */
    0.880916107995, /*   4.52 */
    0.881495084439, /*   4.54 */
    0.882068316857, /*   4.56 */
    0.882635890624, /*   4.58 */
    0.883197889492, /*   4.60 */
    0.883754395623, /*   4.62 */
    0.884305489626, /*   4.64 */
    0.884851250590, /*   4.66 */
    0.885391756117, /*   4.68 */
    0.885927082354, /*   4.70 */
    0.886457304025, /*   4.72 */
    0.886982494460, /*   4.74 */
    0.887502725630, /*   4.76 */
    0.888018068169, /*   4.78 */
    0.888528591409, /*   4.80 */
    0.889034363405, /*   4.82 */
    0.889535450965, /*   4.84 */
    0.890031919672, /*   4.86 */
    0.890523833916, /*   4.88 */
    0.891011256914, /*   4.90 */
    0.891494250740, /*   4.92 */
    0.891972876345, /*   4.94 */
    0.892447193585, /*   4.96 */
    0.892917261242, /*   4.98 */
    0.893383137044, /*   5.00 */
    0.893844877694, /*   5.02 */
    0.894302538886, /*   5.04 */
    0.894756175328, /*   5.06 */
    0.895205840764, /*   5.08 */
    0.895651587991, /*   5.10 */
    0.896093468882, /*   5.12 */
    0.896531534406, /*   5.14 */
    0.896965834642, /*   5.16 */
    0.897396418802, /*   5.18 */
    0.897823335248, /*   5.20 */
    0.898246631507, /*   5.22 */
    0.898666354294, /*   5.24 */
    0.899082549521, /*   5.26 */
    0.899495262320, /*   5.28 */
    0.899904537058, /*   5.30 */
    0.900310417347, /*   5.32 */
    0.900712946068, /*   5.34 */
    0.901112165379, /*   5.36 */
    0.901508116734, /*   5.38 */
    0.901900840893, /*   5.40 */
    0.902290377942, /*   5.42 */
    0.902676767300, /*   5.44 */
    0.903060047738, /*   5.46 */
    0.903440257388, /*   5.48 */
    0.903817433757, /*   5.50 */
    0.904191613743, /*   5.52 */
    0.904562833640, /*   5.54 */
    0.904931129155, /*   5.56 */
    0.905296535421, /*   5.58 */
    0.905659087003, /*   5.60 */
    0.906018817914, /*   5.62 */
    0.906375761622, /*   5.64 */
    0.906729951064, /*   5.66 */
    0.907081418655, /*   5.68 */
    0.907430196297, /*   5.70 */
    0.907776315391, /*   5.72 */
    0.908119806845, /*   5.74 */
    0.908460701086, /*   5.76 */
    0.908799028066, /*   5.78 */
    0.909134817273, /*   5.80 */
    0.909468097738, /*   5.82 */
    0.909798898048, /*   5.84 */
    0.910127246351, /*   5.86 */
    0.910453170363, /*   5.88 */
    0.910776697381, /*   5.90 */
    0.911097854286, /*   5.92 */
    0.911416667553, /*   5.94 */
    0.911733163260, /*   5.96 */
    0.912047367092, /*   5.98 */
    0.912359304353, /*   6.00 */
    0.912668999967, /*   6.02 */
    0.912976478490, /*   6.04 */
    0.913281764116, /*   6.06 */
    0.913584880681, /*   6.08 */
    0.913885851673, /*   6.10 */
    0.914184700234, /*   6.12 */
    0.914481449173, /*   6.14 */
    0.914776120966, /*   6.16 */
    0.915068737762, /*   6.18 */
    0.915359321396, /*   6.20 */
    0.915647893384, /*   6.22 */
    0.915934474939, /*   6.24 */
    0.916219086969, /*   6.26 */
    0.916501750087, /*   6.28 */
    0.916782484613, /*   6.30 */
    0.917061310582, /*   6.32 */
    0.917338247747, /*   6.34 */
    0.917613315584, /*   6.36 */
    0.917886533298, /*   6.38 */
    0.918157919829, /*   6.40 */
    0.918427493852, /*   6.42 */
    0.918695273786, /*   6.44 */
    0.918961277797, /*   6.46 */
    0.919225523803, /*   6.48 */
    0.919488029475, /*   6.50 */
    0.919748812246, /*   6.52 */
    0.920007889313, /*   6.54 */
    0.920265277639, /*   6.56 */
    0.920520993961, /*   6.58 */
    0.920775054789, /*   6.60 */
    0.921027476416, /*   6.62 */
    0.921278274916, /*   6.64 */
    0.921527466149, /*   6.66 */
    0.921775065767, /*   6.68 */
    0.922021089216, /*   6.70 */
    0.922265551739, /*   6.72 */
    0.922508468378, /*   6.74 */
    0.922749853981, /*   6.76 */
    0.922989723202, /*   6.78 */
    0.923228090507, /*   6.80 */
    0.923464970173, /*   6.82 */
    0.923700376296, /*   6.84 */
    0.923934322788, /*   6.86 */
    0.924166823388, /*   6.88 */
    0.924397891655, /*   6.90 */
    0.924627540981, /*   6.92 */
    0.924855784586, /*   6.94 */
    0.925082635523, /*   6.96 */
    0.925308106683, /*   6.98 */
    0.925532210794, /*   7.00 */
    0.925754960428, /*   7.02 */
    0.925976367997, /*   7.04 */
    0.926196445762, /*   7.06 */
    0.926415205832, /*   7.08 */
    0.926632660166, /*   7.10 */
    0.926848820579, /*   7.12 */
    0.927063698738, /*   7.14 */
    0.927277306170, /*   7.16 */
    0.927489654262, /*   7.18 */
    0.927700754264, /*   7.20 */
    0.927910617287, /*   7.22 */
    0.928119254311, /*   7.24 */
    0.928326676185, /*   7.26 */
    0.928532893627, /*   7.28 */
    0.928737917227, /*   7.30 */
    0.928941757450, /*   7.32 */
    0.929144424637, /*   7.34 */
    0.929345929007, /*   7.36 */
    0.929546280660, /*   7.38 */
    0.929745489576, /*   7.40 */
    0.929943565619, /*   7.42 */
    0.930140518537, /*   7.44 */
    0.930336357967, /*   7.46 */
    0.930531093433, /*   7.48 */
    0.930724734349, /*   7.50 */
    0.930917290022, /*   7.52 */
    0.931108769651, /*   7.54 */
    0.931299182329, /*   7.56 */
    0.931488537049, /*   7.58 */
    0.931676842697, /*   7.60 */
    0.931864108063, /*   7.62 */
    0.932050341833, /*   7.64 */
    0.932235552600, /*   7.66 */
    0.932419748857, /*   7.68 */
    0.932602939003, /*   7.70 */
    0.932785131344, /*   7.72 */
    0.932966334093, /*   7.74 */
    0.933146555372, /*   7.76 */
    0.933325803213, /*   7.78 */
    0.933504085560, /*   7.80 */
    0.933681410268, /*   7.82 */
    0.933857785109, /*   7.84 */
    0.934033217767, /*   7.86 */
    0.934207715845, /*   7.88 */
    0.934381286860, /*   7.90 */
    0.934553938251, /*   7.92 */
    0.934725677374, /*   7.94 */
    0.934896511508, /*   7.96 */
    0.935066447852, /*   7.98 */
    0.935235493529, /*   8.00 */
    0.935403655586, /*   8.02 */
    0.935570940995, /*   8.04 */
    0.935737356652, /*   8.06 */
    0.935902909383, /*   8.08 */
    0.936067605940, /*   8.10 */
    0.936231453004, /*   8.12 */
    0.936394457187, /*   8.14 */
    0.936556625031, /*   8.16 */
    0.936717963010, /*   8.18 */
    0.936878477530, /*   8.20 */
    0.937038174931, /*   8.22 */
    0.937197061488, /*   8.24 */
    0.937355143409, /*   8.26 */
    0.937512426842, /*   8.28 */
    0.937668917867, /*   8.30 */
    0.937824622505, /*   8.32 */
    0.937979546716, /*   8.34 */
    0.938133696397, /*   8.36 */
    0.938287077387, /*   8.38 */
    0.938439695463, /*   8.40 */
    0.938591556348, /*   8.42 */
    0.938742665705, /*   8.44 */
    0.938893029138, /*   8.46 */
    0.939042652199, /*   8.48 */
    0.939191540382, /*   8.50 */
    0.939339699126, /*   8.52 */
    0.939487133818, /*   8.54 */
    0.939633849790, /*   8.56 */
    0.939779852320, /*   8.58 */
    0.939925146638, /*   8.60 */
    0.940069737918, /*   8.62 */
    0.940213631286, /*   8.64 */
    0.940356831817, /*   8.66 */
    0.940499344538, /*   8.68 */
    0.940641174423, /*   8.70 */
    0.940782326403, /*   8.72 */
    0.940922805357, /*   8.74 */
    0.941062616118, /*   8.76 */
    0.941201763475, /*   8.78 */
    0.941340252167, /*   8.80 */
    0.941478086891, /*   8.82 */
    0.941615272295, /*   8.84 */
    0.941751812988, /*   8.86 */
    0.941887713531, /*   8.88 */
    0.942022978443, /*   8.90 */
    0.942157612200, /*   8.92 */
    0.942291619237, /*   8.94 */
    0.942425003945, /*   8.96 */
    0.942557770676, /*   8.98 */
    0.942689923740, /*   9.00 */
    0.942821467406, /*   9.02 */
    0.942952405904, /*   9.04 */
    0.943082743426, /*   9.06 */
    0.943212484122, /*   9.08 */
    0.943341632107, /*   9.10 */
    0.943470191455, /*   9.12 */
    0.943598166204, /*   9.14 */
    0.943725560356, /*   9.16 */
    0.943852377873, /*   9.18 */
    0.943978622684, /*   9.20 */
    0.944104298680, /*   9.22 */
    0.944229409718, /*   9.24 */
    0.944353959618, /*   9.26 */
    0.944477952169, /*   9.28 */
    0.944601391122, /*   9.30 */
    0.944724280197, /*   9.32 */
    0.944846623077, /*   9.34 */
    0.944968423416, /*   9.36 */
    0.945089684832, /*   9.38 */
    0.945210410913, /*   9.40 */
    0.945330605215, /*   9.42 */
    0.945450271259, /*   9.44 */
    0.945569412539, /*   9.46 */
    0.945688032516, /*   9.48 */
    0.945806134621, /*   9.50 */
    0.945923722254, /*   9.52 */
    0.946040798785, /*   9.54 */
    0.946157367557, /*   9.56 */
    0.946273431880, /*   9.58 */
    0.946388995037, /*   9.60 */
    0.946504060283, /*   9.62 */
    0.946618630844, /*   9.64 */
    0.946732709918, /*   9.66 */
    0.946846300675, /*   9.68 */
    0.946959406258, /*   9.70 */
    0.947072029783, /*   9.72 */
    0.947184174339, /*   9.74 */
    0.947295842989, /*   9.76 */
    0.947407038769, /*   9.78 */
    0.947517764689, /*   9.80 */
    0.947628023736, /*   9.82 */
    0.947737818867, /*   9.84 */
    0.947847153017, /*   9.86 */
    0.947956029097, /*   9.88 */
    0.948064449991, /*   9.90 */
    0.948172418559, /*   9.92 */
    0.948279937638, /*   9.94 */
    0.948387010042, /*   9.96 */
    0.948493638559, /*   9.98 */
    0.948599825955, /*  10.00 */
    0.948705574973, /*  10.02 */
    0.948810888334, /*  10.04 */
    0.948915768735, /*  10.06 */
    0.949020218851, /*  10.08 */
    0.949124241335, /*  10.10 */
    0.949227838818, /*  10.12 */
    0.949331013910, /*  10.14 */
    0.949433769200, /*  10.16 */
    0.949536107253, /*  10.18 */
    0.949638030617, /*  10.20 */
    0.949739541816, /*  10.22 */
    0.949840643354, /*  10.24 */
    0.949941337717, /*  10.26 */
    0.950041627367, /*  10.28 */
    0.950141514750, /*  10.30 */
    0.950241002290, /*  10.32 */
    0.950340092391, /*  10.34 */
    0.950438787440, /*  10.36 */
    0.950537089802, /*  10.38 */
    0.950635001825, /*  10.40 */
    0.950732525839, /*  10.42 */
    0.950829664152, /*  10.44 */
    0.950926419057, /*  10.46 */
    0.951022792828, /*  10.48 */
    0.951118787719, /*  10.50 */
    0.951214405969, /*  10.52 */
    0.951309649798, /*  10.54 */
    0.951404521408, /*  10.56 */
    0.951499022984, /*  10.58 */
    0.951593156694, /*  10.60 */
    0.951686924691, /*  10.62 */
    0.951780329107, /*  10.64 */
    0.951873372061, /*  10.66 */
    0.951966055654, /*  10.68 */
    0.952058381970, /*  10.70 */
    0.952150353078, /*  10.72 */
    0.952241971032, /*  10.74 */
    0.952333237867, /*  10.76 */
    0.952424155605, /*  10.78 */
    0.952514726252, /*  10.80 */
    0.952604951798, /*  10.82 */
    0.952694834217, /*  10.84 */
    0.952784375470, /*  10.86 */
    0.952873577501, /*  10.88 */
    0.952962442241, /*  10.90 */
    0.953050971604, /*  10.92 */
    0.953139167493, /*  10.94 */
    0.953227031792, /*  10.96 */
    0.953314566376, /*  10.98 */
    0.953401773101, /*  11.00 */
    0.953488653812, /*  11.02 */
    0.953575210338, /*  11.04 */
    0.953661444498, /*  11.06 */
    0.953747358093, /*  11.08 */
    0.953832952913, /*  11.10 */
    0.953918230734, /*  11.12 */
    0.954003193319, /*  11.14 */
    0.954087842418, /*  11.16 */
    0.954172179768, /*  11.18 */
    0.954256207092, /*  11.20 */
    0.954339926102, /*  11.22 */
    0.954423338498, /*  11.24 */
    0.954506445963, /*  11.26 */
    0.954589250174, /*  11.28 */
    0.954671752790, /*  11.30 */
    0.954753955462, /*  11.32 */
    0.954835859827, /*  11.34 */
    0.954917467509, /*  11.36 */
    0.954998780123, /*  11.38 */
    0.955079799271, /*  11.40 */
    0.955160526542, /*  11.42 */
    0.955240963516, /*  11.44 */
    0.955321111759, /*  11.46 */
    0.955400972827, /*  11.48 */
    0.955480548266, /*  11.50 */
    0.955559839609, /*  11.52 */
    0.955638848379, /*  11.54 */
    0.955717576088, /*  11.56 */
    0.955796024236, /*  11.58 */
    0.955874194314, /*  11.60 */
    0.955952087802, /*  11.62 */
    0.956029706168, /*  11.64 */
    0.956107050872, /*  11.66 */
    0.956184123363, /*  11.68 */
    0.956260925077, /*  11.70 */
    0.956337457444, /*  11.72 */
    0.956413721881, /*  11.74 */
    0.956489719797, /*  11.76 */
    0.956565452589, /*  11.78 */
    0.956640921645, /*  11.80 */
    0.956716128346, /*  11.82 */
    0.956791074059, /*  11.84 */
    0.956865760144, /*  11.86 */
    0.956940187952, /*  11.88 */
    0.957014358822, /*  11.90 */
    0.957088274087, /*  11.92 */
    0.957161935068, /*  11.94 */
    0.957235343078, /*  11.96 */
    0.957308499423, /*  11.98 */
    0.957381405395, /*  12.00 */
    0.957454062283, /*  12.02 */
    0.957526471362, /*  12.04 */
    0.957598633902, /*  12.06 */
    0.957670551163, /*  12.08 */
    0.957742224395, /*  12.10 */
    0.957813654842, /*  12.12 */
    0.957884843738, /*  12.14 */
    0.957955792309, /*  12.16 */
    0.958026501772, /*  12.18 */
    0.958096973338, /*  12.20 */
    0.958167208207, /*  12.22 */
    0.958237207572, /*  12.24 */
    0.958306972619, /*  12.26 */
    0.958376504525, /*  12.28 */
    0.958445804459, /*  12.30 */
    0.958514873582, /*  12.32 */
    0.958583713049, /*  12.34 */
    0.958652324005, /*  12.36 */
    0.958720707589, /*  12.38 */
    0.958788864931, /*  12.40 */
    0.958856797155, /*  12.42 */
    0.958924505377, /*  12.44 */
    0.958991990705, /*  12.46 */
    0.959059254240, /*  12.48 */
    0.959126297076, /*  12.50 */
    0.959193120301, /*  12.52 */
    0.959259724992, /*  12.54 */
    0.959326112224, /*  12.56 */
    0.959392283061, /*  12.58 */
    0.959458238562, /*  12.60 */
    0.959523979779, /*  12.62 */
    0.959589507755, /*  12.64 */
    0.959654823530, /*  12.66 */
    0.959719928135, /*  12.68 */
    0.959784822593, /*  12.70 */
    0.959849507922, /*  12.72 */
    0.959913985135, /*  12.74 */
    0.959978255236, /*  12.76 */
    0.960042319223, /*  12.78 */
    0.960106178087, /*  12.80 */
    0.960169832815, /*  12.82 */
    0.960233284386, /*  12.84 */
    0.960296533773, /*  12.86 */
    0.960359581942, /*  12.88 */
    0.960422429853, /*  12.90 */
    0.960485078462, /*  12.92 */
    0.960547528716, /*  12.94 */
    0.960609781559, /*  12.96 */
    0.960671837926, /*  12.98 */
    0.960733698747, /*  13.00 */
    0.960795364948, /*  13.02 */
    0.960856837447, /*  13.04 */
    0.960918117157, /*  13.06 */
    0.960979204985, /*  13.08 */
    0.961040101833, /*  13.10 */
    0.961100808596, /*  13.12 */
    0.961161326165, /*  13.14 */
    0.961221655425, /*  13.16 */
    0.961281797254, /*  13.18 */
    0.961341752526, /*  13.20 */
    0.961401522111, /*  13.22 */
    0.961461106869, /*  13.24 */
    0.961520507660, /*  13.26 */
    0.961579725334, /*  13.28 */
    0.961638760740, /*  13.30 */
    0.961697614719, /*  13.32 */
    0.961756288107, /*  13.34 */
    0.961814781736, /*  13.36 */
    0.961873096432, /*  13.38 */
    0.961931233016, /*  13.40 */
    0.961989192304, /*  13.42 */
    0.962046975109, /*  13.44 */
    0.962104582235, /*  13.46 */
    0.962162014484, /*  13.48 */
    0.962219272653, /*  13.50 */
    0.962276357533, /*  13.52 */
    0.962333269912, /*  13.54 */
    0.962390010571, /*  13.56 */
    0.962446580287, /*  13.58 */
    0.962502979833, /*  13.60 */
    0.962559209977, /*  13.62 */
    0.962615271483, /*  13.64 */
    0.962671165109, /*  13.66 */
    0.962726891609, /*  13.68 */
    0.962782451734, /*  13.70 */
    0.962837846228, /*  13.72 */
    0.962893075832, /*  13.74 */
    0.962948141282, /*  13.76 */
    0.963003043310, /*  13.78 */
    0.963057782645, /*  13.80 */
    0.963112360008, /*  13.82 */
    0.963166776120, /*  13.84 */
    0.963221031695, /*  13.86 */
    0.963275127443, /*  13.88 */
    0.963329064071, /*  13.90 */
    0.963382842281, /*  13.92 */
    0.963436462770, /*  13.94 */
    0.963489926233, /*  13.96 */
    0.963543233360, /*  13.98 */
    0.963596384836, /*  14.00 */
    0.963649381344, /*  14.02 */
    0.963702223560, /*  14.04 */
    0.963754912159, /*  14.06 */
    0.963807447811, /*  14.08 */
    0.963859831182, /*  14.10 */
    0.963912062934, /*  14.12 */
    0.963964143725, /*  14.14 */
    0.964016074209, /*  14.16 */
    0.964067855039, /*  14.18 */
    0.964119486859, /*  14.20 */
    0.964170970314, /*  14.22 */
    0.964222306044, /*  14.24 */
    0.964273494683, /*  14.26 */
    0.964324536865, /*  14.28 */
    0.964375433217, /*  14.30 */
    0.964426184365, /*  14.32 */
    0.964476790931, /*  14.34 */
    0.964527253531, /*  14.36 */
    0.964577572781, /*  14.38 */
    0.964627749291, /*  14.40 */
    0.964677783669, /*  14.42 */
    0.964727676518, /*  14.44 */
    0.964777428440, /*  14.46 */
    0.964827040030, /*  14.48 */
    0.964876511884, /*  14.50 */
    0.964925844591, /*  14.52 */
    0.964975038738, /*  14.54 */
    0.965024094909, /*  14.56 */
    0.965073013685, /*  14.58 */
    0.965121795642, /*  14.60 */
    0.965170441355, /*  14.62 */
    0.965218951395, /*  14.64 */
    0.965267326329, /*  14.66 */
    0.965315566721, /*  14.68 */
    0.965363673134, /*  14.70 */
    0.965411646124, /*  14.72 */
    0.965459486247, /*  14.74 */
    0.965507194055, /*  14.76 */
    0.965554770097, /*  14.78 */
    0.965602214919, /*  14.80 */
    0.965649529063, /*  14.82 */
    0.965696713070, /*  14.84 */
    0.965743767476, /*  14.86 */
    0.965790692816, /*  14.88 */
    0.965837489620, /*  14.90 */
    0.965884158416, /*  14.92 */
    0.965930699730, /*  14.94 */
    0.965977114084, /*  14.96 */
    0.966023401997, /*  14.98 */
    0.966069563987, /*  15.00 */
    0.966115600566, /*  15.02 */
    0.966161512246, /*  15.04 */
    0.966207299535, /*  15.06 */
    0.966252962939, /*  15.08 */
    0.966298502960, /*  15.10 */
    0.966343920098, /*  15.12 */
    0.966389214851, /*  15.14 */
    0.966434387714, /*  15.16 */
    0.966479439178, /*  15.18 */
    0.966524369733, /*  15.20 */
    0.966569179865, /*  15.22 */
    0.966613870059, /*  15.24 */
    0.966658440795, /*  15.26 */
    0.966702892553, /*  15.28 */
    0.966747225810, /*  15.30 */
    0.966791441039, /*  15.32 */
    0.966835538711, /*  15.34 */
    0.966879519295, /*  15.36 */
    0.966923383258, /*  15.38 */
    0.966967131063, /*  15.40 */
    0.967010763171, /*  15.42 */
    0.967054280042, /*  15.44 */
    0.967097682132, /*  15.46 */
    0.967140969895, /*  15.48 */
    0.967184143783, /*  15.50 */
    0.967227204244, /*  15.52 */
    0.967270151727, /*  15.54 */
    0.967312986675, /*  15.56 */
    0.967355709531, /*  15.58 */
    0.967398320734, /*  15.60 */
    0.967440820723, /*  15.62 */
    0.967483209933, /*  15.64 */
    0.967525488796, /*  15.66 */
    0.967567657744, /*  15.68 */
    0.967609717205, /*  15.70 */
    0.967651667605, /*  15.72 */
    0.967693509369, /*  15.74 */
    0.967735242919, /*  15.76 */
    0.967776868674, /*  15.78 */
    0.967818387052, /*  15.80 */
    0.967859798468, /*  15.82 */
    0.967901103335, /*  15.84 */
    0.967942302065, /*  15.86 */
    0.967983395067, /*  15.88 */
    0.968024382748, /*  15.90 */
    0.968065265512, /*  15.92 */
    0.968106043763, /*  15.94 */
    0.968146717900, /*  15.96 */
    0.968187288323, /*  15.98 */
    0.968227755428, /*  16.00 */
    0.968268119610, /*  16.02 */
    0.968308381262, /*  16.04 */
    0.968348540773, /*  16.06 */
    0.968388598534, /*  16.08 */
    0.968428554929, /*  16.10 */
    0.968468410345, /*  16.12 */
    0.968508165163, /*  16.14 */
    0.968547819764, /*  16.16 */
    0.968587374528, /*  16.18 */
    0.968626829832, /*  16.20 */
    0.968666186050, /*  16.22 */
    0.968705443555, /*  16.24 */
    0.968744602719, /*  16.26 */
    0.968783663912, /*  16.28 */
    0.968822627500, /*  16.30 */
    0.968861493851, /*  16.32 */
    0.968900263327, /*  16.34 */
    0.968938936290, /*  16.36 */
    0.968977513102, /*  16.38 */
    0.969015994120, /*  16.40 */
    0.969054379702, /*  16.42 */
    0.969092670202, /*  16.44 */
    0.969130865973, /*  16.46 */
    0.969168967368, /*  16.48 */
    0.969206974735, /*  16.50 */
    0.969244888422, /*  16.52 */
    0.969282708777, /*  16.54 */
    0.969320436143, /*  16.56 */
    0.969358070864, /*  16.58 */
    0.969395613280, /*  16.60 */
    0.969433063731, /*  16.62 */
    0.969470422555, /*  16.64 */
    0.969507690088, /*  16.66 */
    0.969544866664, /*  16.68 */
    0.969581952617, /*  16.70 */
    0.969618948278, /*  16.72 */
    0.969655853977, /*  16.74 */
    0.969692670041, /*  16.76 */
    0.969729396797, /*  16.78 */
    0.969766034570, /*  16.80 */
    0.969802583683, /*  16.82 */
    0.969839044459, /*  16.84 */
    0.969875417216, /*  16.86 */
    0.969911702274, /*  16.88 */
    0.969947899950, /*  16.90 */
    0.969984010560, /*  16.92 */
    0.970020034417, /*  16.94 */
    0.970055971834, /*  16.96 */
    0.970091823123, /*  16.98 */
    0.970127588592, /*  17.00 */
    0.970163268550, /*  17.02 */
    0.970198863303, /*  17.04 */
    0.970234373157, /*  17.06 */
    0.970269798414, /*  17.08 */
    0.970305139379, /*  17.10 */
    0.970340396350, /*  17.12 */
    0.970375569628, /*  17.14 */
    0.970410659510, /*  17.16 */
    0.970445666294, /*  17.18 */
    0.970480590273, /*  17.20 */
    0.970515431742, /*  17.22 */
    0.970550190993, /*  17.24 */
    0.970584868318, /*  17.26 */
    0.970619464005, /*  17.28 */
    0.970653978343, /*  17.30 */
    0.970688411618, /*  17.32 */
    0.970722764117, /*  17.34 */
    0.970757036123, /*  17.36 */
    0.970791227919, /*  17.38 */
    0.970825339787, /*  17.40 */
    0.970859372007, /*  17.42 */
    0.970893324858, /*  17.44 */
    0.970927198617, /*  17.46 */
    0.970960993561, /*  17.48 */
    0.970994709964, /*  17.50 */
    0.971028348101, /*  17.52 */
    0.971061908243, /*  17.54 */
    0.971095390663, /*  17.56 */
    0.971128795629, /*  17.58 */
    0.971162123411, /*  17.60 */
    0.971195374276, /*  17.62 */
    0.971228548490, /*  17.64 */
    0.971261646318, /*  17.66 */
    0.971294668025, /*  17.68 */
    0.971327613871, /*  17.70 */
    0.971360484120, /*  17.72 */
    0.971393279030, /*  17.74 */
    0.971425998862, /*  17.76 */
    0.971458643872, /*  17.78 */
    0.971491214317, /*  17.80 */
    0.971523710453, /*  17.82 */
    0.971556132533, /*  17.84 */
    0.971588480811, /*  17.86 */
    0.971620755540, /*  17.88 */
    0.971652956968, /*  17.90 */
    0.971685085347, /*  17.92 */
    0.971717140925, /*  17.94 */
    0.971749123949, /*  17.96 */
    0.971781034665, /*  17.98 */
    0.971812873318, /*  18.00 */
    0.971844640153, /*  18.02 */
    0.971876335412, /*  18.04 */
    0.971907959338, /*  18.06 */
    0.971939512170, /*  18.08 */
    0.971970994150, /*  18.10 */
    0.972002405514, /*  18.12 */
    0.972033746501, /*  18.14 */
    0.972065017347, /*  18.16 */
    0.972096218288, /*  18.18 */
    0.972127349558, /*  18.20 */
    0.972158411390, /*  18.22 */
    0.972189404017, /*  18.24 */
    0.972220327669, /*  18.26 */
    0.972251182577, /*  18.28 */
    0.972281968970, /*  18.30 */
    0.972312687076, /*  18.32 */
    0.972343337122, /*  18.34 */
    0.972373919335, /*  18.36 */
    0.972404433939, /*  18.38 */
    0.972434881159, /*  18.40 */
    0.972465261217, /*  18.42 */
    0.972495574337, /*  18.44 */
    0.972525820738, /*  18.46 */
    0.972556000642, /*  18.48 */
    0.972586114267, /*  18.50 */
    0.972616161832, /*  18.52 */
    0.972646143554, /*  18.54 */
    0.972676059650, /*  18.56 */
    0.972705910334, /*  18.58 */
    0.972735695822, /*  18.60 */
    0.972765416326, /*  18.62 */
    0.972795072059, /*  18.64 */
    0.972824663234, /*  18.66 */
    0.972854190061, /*  18.68 */
    0.972883652749, /*  18.70 */
    0.972913051507, /*  18.72 */
    0.972942386545, /*  18.74 */
    0.972971658067, /*  18.76 */
    0.973000866282, /*  18.78 */
    0.973030011394, /*  18.80 */
    0.973059093607, /*  18.82 */
    0.973088113125, /*  18.84 */
    0.973117070152, /*  18.86 */
    0.973145964888, /*  18.88 */
    0.973174797534, /*  18.90 */
    0.973203568291, /*  18.92 */
    0.973232277357, /*  18.94 */
    0.973260924932, /*  18.96 */
    0.973289511213, /*  18.98 */
    0.973318036395, /*  19.00 */
    0.973346500676, /*  19.02 */
    0.973374904250, /*  19.04 */
    0.973403247310, /*  19.06 */
    0.973431530052, /*  19.08 */
    0.973459752666, /*  19.10 */
    0.973487915345, /*  19.12 */
    0.973516018280, /*  19.14 */
    0.973544061660, /*  19.16 */
    0.973572045675, /*  19.18 */
    0.973599970513, /*  19.20 */
    0.973627836362, /*  19.22 */
    0.973655643409, /*  19.24 */
    0.973683391839, /*  19.26 */
    0.973711081839, /*  19.28 */
    0.973738713592, /*  19.30 */
    0.973766287283, /*  19.32 */
    0.973793803093, /*  19.34 */
    0.973821261207, /*  19.36 */
    0.973848661803, /*  19.38 */
    0.973876005065, /*  19.40 */
    0.973903291171, /*  19.42 */
    0.973930520300, /*  19.44 */
    0.973957692631, /*  19.46 */
    0.973984808342, /*  19.48 */
    0.974011867609, /*  19.50 */
    0.974038870609, /*  19.52 */
    0.974065817517, /*  19.54 */
    0.974092708508, /*  19.56 */
    0.974119543755, /*  19.58 */
    0.974146323433, /*  19.60 */
    0.974173047713, /*  19.62 */
    0.974199716768, /*  19.64 */
    0.974226330768, /*  19.66 */
    0.974252889884, /*  19.68 */
    0.974279394286, /*  19.70 */
    0.974305844142, /*  19.72 */
    0.974332239622, /*  19.74 */
    0.974358580892, /*  19.76 */
    0.974384868120, /*  19.78 */
    0.974411101471, /*  19.80 */
    0.974437281112, /*  19.82 */
    0.974463407208, /*  19.84 */
    0.974489479921, /*  19.86 */
    0.974515499417, /*  19.88 */
    0.974541465858, /*  19.90 */
    0.974567379406, /*  19.92 */
    0.974593240222, /*  19.94 */
    0.974619048468, /*  19.96 */
    0.974644804304, /*  19.98 */
    0.974670507890, /*  20.00 */
    0.974696159384, /*  20.02 */
    0.974721758944, /*  20.04 */
    0.974747306729, /*  20.06 */
    0.974772802895, /*  20.08 */
    0.974798247598, /*  20.10 */
    0.974823640995, /*  20.12 */
    0.974848983241, /*  20.14 */
    0.974874274490, /*  20.16 */
    0.974899514896, /*  20.18 */
    0.974924704612, /*  20.20 */
    0.974949843790, /*  20.22 */
    0.974974932584, /*  20.24 */
    0.974999971144, /*  20.26 */
    0.975024959621, /*  20.28 */
    0.975049898166, /*  20.30 */
    0.975074786927, /*  20.32 */
    0.975099626054, /*  20.34 */
    0.975124415696, /*  20.36 */
    0.975149156000, /*  20.38 */
    0.975173847113, /*  20.40 */
    0.975198489182, /*  20.42 */
    0.975223082354, /*  20.44 */
    0.975247626773, /*  20.46 */
    0.975272122584, /*  20.48 */
    0.975296569933, /*  20.50 */
    0.975320968962, /*  20.52 */
    0.975345319815, /*  20.54 */
    0.975369622634, /*  20.56 */
    0.975393877562, /*  20.58 */
    0.975418084740, /*  20.60 */
    0.975442244309, /*  20.62 */
    0.975466356409, /*  20.64 */
    0.975490421181, /*  20.66 */
    0.975514438763, /*  20.68 */
    0.975538409294, /*  20.70 */
    0.975562332913, /*  20.72 */
    0.975586209757, /*  20.74 */
    0.975610039963, /*  20.76 */
    0.975633823668, /*  20.78 */
    0.975657561008, /*  20.80 */
    0.975681252119, /*  20.82 */
    0.975704897135, /*  20.84 */
    0.975728496191, /*  20.86 */
    0.975752049420, /*  20.88 */
    0.975775556958, /*  20.90 */
    0.975799018935, /*  20.92 */
    0.975822435485, /*  20.94 */
    0.975845806739, /*  20.96 */
    0.975869132829, /*  20.98 */
    0.975892413886, /*  21.00 */
    0.975915650040, /*  21.02 */
    0.975938841421, /*  21.04 */
    0.975961988159, /*  21.06 */
    0.975985090381, /*  21.08 */
    0.976008148217, /*  21.10 */
    0.976031161794, /*  21.12 */
    0.976054131241, /*  21.14 */
    0.976077056683, /*  21.16 */
    0.976099938246, /*  21.18 */
    0.976122776058, /*  21.20 */
    0.976145570244, /*  21.22 */
    0.976168320927, /*  21.24 */
    0.976191028234, /*  21.26 */
    0.976213692287, /*  21.28 */
    0.976236313211, /*  21.30 */
    0.976258891129, /*  21.32 */
    0.976281426162, /*  21.34 */
    0.976303918434, /*  21.36 */
    0.976326368065, /*  21.38 */
    0.976348775178, /*  21.40 */
    0.976371139892, /*  21.42 */
    0.976393462328, /*  21.44 */
    0.976415742607, /*  21.46 */
    0.976437980847, /*  21.48 */
    0.976460177167, /*  21.50 */
    0.976482331685, /*  21.52 */
    0.976504444521, /*  21.54 */
    0.976526515791, /*  21.56 */
    0.976548545612, /*  21.58 */
    0.976570534102, /*  21.60 */
    0.976592481376, /*  21.62 */
    0.976614387550, /*  21.64 */
    0.976636252740, /*  21.66 */
    0.976658077061, /*  21.68 */
    0.976679860627, /*  21.70 */
    0.976701603552, /*  21.72 */
    0.976723305951, /*  21.74 */
    0.976744967936, /*  21.76 */
    0.976766589619, /*  21.78 */
    0.976788171115, /*  21.80 */
    0.976809712534, /*  21.82 */
    0.976831213988, /*  21.84 */
    0.976852675589, /*  21.86 */
    0.976874097446, /*  21.88 */
    0.976895479671, /*  21.90 */
    0.976916822373, /*  21.92 */
    0.976938125663, /*  21.94 */
    0.976959389648, /*  21.96 */
    0.976980614438, /*  21.98 */
    0.977001800141, /*  22.00 */
    0.977022946866, /*  22.02 */
    0.977044054718, /*  22.04 */
    0.977065123807, /*  22.06 */
    0.977086154238, /*  22.08 */
    0.977107146117, /*  22.10 */
    0.977128099552, /*  22.12 */
    0.977149014646, /*  22.14 */
    0.977169891507, /*  22.16 */
    0.977190730237, /*  22.18 */
    0.977211530942, /*  22.20 */
    0.977232293726, /*  22.22 */
    0.977253018693, /*  22.24 */
    0.977273705945, /*  22.26 */
    0.977294355585, /*  22.28 */
    0.977314967717, /*  22.30 */
    0.977335542442, /*  22.32 */
    0.977356079862, /*  22.34 */
    0.977376580079, /*  22.36 */
    0.977397043193, /*  22.38 */
    0.977417469305, /*  22.40 */
    0.977437858516, /*  22.42 */
    0.977458210925, /*  22.44 */
    0.977478526633, /*  22.46 */
    0.977498805738, /*  22.48 */
    0.977519048339, /*  22.50 */
    0.977539254534, /*  22.52 */
    0.977559424423, /*  22.54 */
    0.977579558102, /*  22.56 */
    0.977599655670, /*  22.58 */
    0.977619717223, /*  22.60 */
    0.977639742857, /*  22.62 */
    0.977659732671, /*  22.64 */
    0.977679686759, /*  22.66 */
    0.977699605217, /*  22.68 */
    0.977719488141, /*  22.70 */
    0.977739335625, /*  22.72 */
    0.977759147765, /*  22.74 */
    0.977778924655, /*  22.76 */
    0.977798666389, /*  22.78 */
    0.977818373060, /*  22.80 */
    0.977838044762, /*  22.82 */
    0.977857681588, /*  22.84 */
    0.977877283631, /*  22.86 */
    0.977896850983, /*  22.88 */
    0.977916383736, /*  22.90 */
    0.977935881982, /*  22.92 */
    0.977955345812, /*  22.94 */
    0.977974775317, /*  22.96 */
    0.977994170588, /*  22.98 */
    0.978013531716, /*  23.00 */
    0.978032858791, /*  23.02 */
    0.978052151902, /*  23.04 */
    0.978071411139, /*  23.06 */
    0.978090636591, /*  23.08 */
    0.978109828347, /*  23.10 */
    0.978128986496, /*  23.12 */
    0.978148111125, /*  23.14 */
    0.978167202323, /*  23.16 */
    0.978186260177, /*  23.18 */
    0.978205284775, /*  23.20 */
    0.978224276204, /*  23.22 */
    0.978243234550, /*  23.24 */
    0.978262159900, /*  23.26 */
    0.978281052339, /*  23.28 */
    0.978299911955, /*  23.30 */
    0.978318738831, /*  23.32 */
    0.978337533054, /*  23.34 */
    0.978356294708, /*  23.36 */
    0.978375023879, /*  23.38 */
    0.978393720649, /*  23.40 */
    0.978412385104, /*  23.42 */
    0.978431017327, /*  23.44 */
    0.978449617401, /*  23.46 */
    0.978468185410, /*  23.48 */
    0.978486721437, /*  23.50 */
    0.978505225563, /*  23.52 */
    0.978523697872, /*  23.54 */
    0.978542138446, /*  23.56 */
    0.978560547366, /*  23.58 */
    0.978578924714, /*  23.60 */
    0.978597270570, /*  23.62 */
    0.978615585017, /*  23.64 */
    0.978633868134, /*  23.66 */
    0.978652120001, /*  23.68 */
    0.978670340700, /*  23.70 */
    0.978688530309, /*  23.72 */
    0.978706688909, /*  23.74 */
    0.978724816578, /*  23.76 */
    0.978742913395, /*  23.78 */
    0.978760979440, /*  23.80 */
    0.978779014790, /*  23.82 */
    0.978797019525, /*  23.84 */
    0.978814993721, /*  23.86 */
    0.978832937456, /*  23.88 */
    0.978850850809, /*  23.90 */
    0.978868733855, /*  23.92 */
    0.978886586672, /*  23.94 */
    0.978904409337, /*  23.96 */
    0.978922201925, /*  23.98 */
    0.978939964514, /*  24.00 */
    0.978957697178, /*  24.02 */
    0.978975399994, /*  24.04 */
    0.978993073036, /*  24.06 */
    0.979010716380, /*  24.08 */
    0.979028330101, /*  24.10 */
    0.979045914273, /*  24.12 */
    0.979063468971, /*  24.14 */
    0.979080994268, /*  24.16 */
    0.979098490238, /*  24.18 */
    0.979115956956, /*  24.20 */
    0.979133394493, /*  24.22 */
    0.979150802925, /*  24.24 */
    0.979168182322, /*  24.26 */
    0.979185532758, /*  24.28 */
    0.979202854306, /*  24.30 */
    0.979220147037, /*  24.32 */
    0.979237411023, /*  24.34 */
    0.979254646336, /*  24.36 */
    0.979271853047, /*  24.38 */
    0.979289031228, /*  24.40 */
    0.979306180949, /*  24.42 */
    0.979323302281, /*  24.44 */
    0.979340395295, /*  24.46 */
    0.979357460060, /*  24.48 */
    0.979374496647, /*  24.50 */
    0.979391505126, /*  24.52 */
    0.979408485566, /*  24.54 */
    0.979425438036, /*  24.56 */
    0.979442362605, /*  24.58 */
    0.979459259343, /*  24.60 */
    0.979476128318, /*  24.62 */
    0.979492969598, /*  24.64 */
    0.979509783251, /*  24.66 */
    0.979526569346, /*  24.68 */
    0.979543327950, /*  24.70 */
    0.979560059131, /*  24.72 */
    0.979576762955, /*  24.74 */
    0.979593439491, /*  24.76 */
    0.979610088804, /*  24.78 */
    0.979626710962, /*  24.80 */
    0.979643306031, /*  24.82 */
    0.979659874077, /*  24.84 */
    0.979676415166, /*  24.86 */
    0.979692929364, /*  24.88 */
    0.979709416736, /*  24.90 */
    0.979725877347, /*  24.92 */
    0.979742311264, /*  24.94 */
    0.979758718550, /*  24.96 */
    0.979775099271, /*  24.98 */
    0.979791453491, /*  25.00 */
    0.979807781273, /*  25.02 */
    0.979824082683, /*  25.04 */
    0.979840357785, /*  25.06 */
    0.979856606641, /*  25.08 */
    0.979872829315, /*  25.10 */
    0.979889025871, /*  25.12 */
    0.979905196372, /*  25.14 */
    0.979921340879, /*  25.16 */
    0.979937459457, /*  25.18 */
    0.979953552168, /*  25.20 */
    0.979969619073, /*  25.22 */
    0.979985660235, /*  25.24 */
    0.980001675716, /*  25.26 */
    0.980017665576, /*  25.28 */
    0.980033629879, /*  25.30 */
    0.980049568685, /*  25.32 */
    0.980065482054, /*  25.34 */
    0.980081370049, /*  25.36 */
    0.980097232729, /*  25.38 */
    0.980113070155, /*  25.40 */
    0.980128882388, /*  25.42 */
    0.980144669487, /*  25.44 */
    0.980160431512, /*  25.46 */
    0.980176168523, /*  25.48 */
    0.980191880580, /*  25.50 */
    0.980207567741, /*  25.52 */
    0.980223230067, /*  25.54 */
    0.980238867616, /*  25.56 */
    0.980254480446, /*  25.58 */
    0.980270068617, /*  25.60 */
    0.980285632186, /*  25.62 */
    0.980301171213, /*  25.64 */
    0.980316685754, /*  25.66 */
    0.980332175868, /*  25.68 */
    0.980347641613, /*  25.70 */
    0.980363083046, /*  25.72 */
    0.980378500223, /*  25.74 */
    0.980393893204, /*  25.76 */
    0.980409262043, /*  25.78 */
    0.980424606799, /*  25.80 */
    0.980439927527, /*  25.82 */
    0.980455224285, /*  25.84 */
    0.980470497128, /*  25.86 */
    0.980485746112, /*  25.88 */
    0.980500971293, /*  25.90 */
    0.980516172728, /*  25.92 */
    0.980531350471, /*  25.94 */
    0.980546504578, /*  25.96 */
    0.980561635103, /*  25.98 */
    0.980576742103, /*  26.00 */
    0.980591825632, /*  26.02 */
    0.980606885745, /*  26.04 */
    0.980621922495, /*  26.06 */
    0.980636935938, /*  26.08 */
    0.980651926128, /*  26.10 */
    0.980666893118, /*  26.12 */
    0.980681836963, /*  26.14 */
    0.980696757716, /*  26.16 */
    0.980711655430, /*  26.18 */
    0.980726530160, /*  26.20 */
    0.980741381957, /*  26.22 */
    0.980756210876, /*  26.24 */
    0.980771016968, /*  26.26 */
    0.980785800288, /*  26.28 */
    0.980800560886, /*  26.30 */
    0.980815298816, /*  26.32 */
    0.980830014130, /*  26.34 */
    0.980844706879, /*  26.36 */
    0.980859377116, /*  26.38 */
    0.980874024893, /*  26.40 */
    0.980888650260, /*  26.42 */
    0.980903253270, /*  26.44 */
    0.980917833973, /*  26.46 */
    0.980932392421, /*  26.48 */
    0.980946928664, /*  26.50 */
    0.980961442754, /*  26.52 */
    0.980975934741, /*  26.54 */
    0.980990404675, /*  26.56 */
    0.981004852607, /*  26.58 */
    0.981019278587, /*  26.60 */
    0.981033682665, /*  26.62 */
    0.981048064891, /*  26.64 */
    0.981062425314, /*  26.66 */
    0.981076763985, /*  26.68 */
    0.981091080951, /*  26.70 */
    0.981105376264, /*  26.72 */
    0.981119649971, /*  26.74 */
    0.981133902122, /*  26.76 */
    0.981148132766, /*  26.78 */
    0.981162341951, /*  26.80 */
    0.981176529726, /*  26.82 */
    0.981190696139, /*  26.84 */
    0.981204841238, /*  26.86 */
    0.981218965072, /*  26.88 */
    0.981233067688, /*  26.90 */
    0.981247149134, /*  26.92 */
    0.981261209458, /*  26.94 */
    0.981275248708, /*  26.96 */
    0.981289266930, /*  26.98 */
    0.981303264172, /*  27.00 */
    0.981317240481, /*  27.02 */
    0.981331195904, /*  27.04 */
    0.981345130488, /*  27.06 */
    0.981359044279, /*  27.08 */
    0.981372937324, /*  27.10 */
    0.981386809669, /*  27.12 */
    0.981400661361, /*  27.14 */
    0.981414492445, /*  27.16 */
    0.981428302968, /*  27.18 */
    0.981442092976, /*  27.20 */
    0.981455862513, /*  27.22 */
    0.981469611627, /*  27.24 */
    0.981483340361, /*  27.26 */
    0.981497048762, /*  27.28 */
    0.981510736874, /*  27.30 */
    0.981524404743, /*  27.32 */
    0.981538052413, /*  27.34 */
    0.981551679930, /*  27.36 */
    0.981565287337, /*  27.38 */
    0.981578874680, /*  27.40 */
    0.981592442003, /*  27.42 */
    0.981605989349, /*  27.44 */
    0.981619516763, /*  27.46 */
    0.981633024290, /*  27.48 */
    0.981646511972, /*  27.50 */
    0.981659979854, /*  27.52 */
    0.981673427978, /*  27.54 */
    0.981686856390, /*  27.56 */
    0.981700265131, /*  27.58 */
    0.981713654246, /*  27.60 */
    0.981727023776, /*  27.62 */
    0.981740373766, /*  27.64 */
    0.981753704258, /*  27.66 */
    0.981767015294, /*  27.68 */
    0.981780306918, /*  27.70 */
    0.981793579171, /*  27.72 */
    0.981806832096, /*  27.74 */
    0.981820065736, /*  27.76 */
    0.981833280132, /*  27.78 */
    0.981846475326, /*  27.80 */
    0.981859651360, /*  27.82 */
    0.981872808276, /*  27.84 */
    0.981885946115, /*  27.86 */
    0.981899064919, /*  27.88 */
    0.981912164729, /*  27.90 */
    0.981925245587, /*  27.92 */
    0.981938307533, /*  27.94 */
    0.981951350609, /*  27.96 */
    0.981964374855, /*  27.98 */
    0.981977380313, /*  28.00 */
    0.981990367022, /*  28.02 */
    0.982003335024, /*  28.04 */
    0.982016284358, /*  28.06 */
    0.982029215065, /*  28.08 */
    0.982042127186, /*  28.10 */
    0.982055020760, /*  28.12 */
    0.982067895827, /*  28.14 */
    0.982080752427, /*  28.16 */
    0.982093590600, /*  28.18 */
    0.982106410385, /*  28.20 */
    0.982119211822, /*  28.22 */
    0.982131994950, /*  28.24 */
    0.982144759809, /*  28.26 */
    0.982157506437, /*  28.28 */
    0.982170234874, /*  28.30 */
    0.982182945159, /*  28.32 */
    0.982195637330, /*  28.34 */
    0.982208311426, /*  28.36 */
    0.982220967486, /*  28.38 */
    0.982233605548, /*  28.40 */
    0.982246225651, /*  28.42 */
    0.982258827832, /*  28.44 */
    0.982271412130, /*  28.46 */
    0.982283978584, /*  28.48 */
    0.982296527230, /*  28.50 */
    0.982309058107, /*  28.52 */
    0.982321571253, /*  28.54 */
    0.982334066705, /*  28.56 */
    0.982346544501, /*  28.58 */
    0.982359004677, /*  28.60 */
    0.982371447272, /*  28.62 */
    0.982383872323, /*  28.64 */
    0.982396279866, /*  28.66 */
    0.982408669939, /*  28.68 */
    0.982421042579, /*  28.70 */
    0.982433397822, /*  28.72 */
    0.982445735704, /*  28.74 */
    0.982458056264, /*  28.76 */
    0.982470359536, /*  28.78 */
    0.982482645558, /*  28.80 */
    0.982494914365, /*  28.82 */
    0.982507165995, /*  28.84 */
    0.982519400482, /*  28.86 */
    0.982531617863, /*  28.88 */
    0.982543818174, /*  28.90 */
    0.982556001450, /*  28.92 */
    0.982568167727, /*  28.94 */
    0.982580317041, /*  28.96 */
    0.982592449427, /*  28.98 */
    0.982604564921, /*  29.00 */
    0.982616663558, /*  29.02 */
    0.982628745372, /*  29.04 */
    0.982640810400, /*  29.06 */
    0.982652858675, /*  29.08 */
    0.982664890234, /*  29.10 */
    0.982676905110, /*  29.12 */
    0.982688903338, /*  29.14 */
    0.982700884954, /*  29.16 */
    0.982712849990, /*  29.18 */
    0.982724798483, /*  29.20 */
    0.982736730466, /*  29.22 */
    0.982748645972, /*  29.24 */
    0.982760545038, /*  29.26 */
    0.982772427696, /*  29.28 */
    0.982784293980, /*  29.30 */
    0.982796143924, /*  29.32 */
    0.982807977563, /*  29.34 */
    0.982819794928, /*  29.36 */
    0.982831596055, /*  29.38 */
    0.982843380977, /*  29.40 */
    0.982855149726, /*  29.42 */
    0.982866902337, /*  29.44 */
    0.982878638842, /*  29.46 */
    0.982890359275, /*  29.48 */
    0.982902063668, /*  29.50 */
    0.982913752054, /*  29.52 */
    0.982925424467, /*  29.54 */
    0.982937080939, /*  29.56 */
    0.982948721502, /*  29.58 */
    0.982960346189, /*  29.60 */
    0.982971955033, /*  29.62 */
    0.982983548066, /*  29.64 */
    0.982995125321, /*  29.66 */
    0.983006686828, /*  29.68 */
    0.983018232622, /*  29.70 */
    0.983029762733, /*  29.72 */
    0.983041277193, /*  29.74 */
    0.983052776035, /*  29.76 */
    0.983064259290, /*  29.78 */
    0.983075726990, /*  29.80 */
    0.983087179167, /*  29.82 */
    0.983098615851, /*  29.84 */
    0.983110037075, /*  29.86 */
    0.983121442869, /*  29.88 */
    0.983132833266, /*  29.90 */
    0.983144208295, /*  29.92 */
    0.983155567989, /*  29.94 */
    0.983166912378, /*  29.96 */
    0.983178241493, /*  29.98 */
    0.983189555365, /*  30.00 */
    0.983200854025, /*  30.02 */
    0.983212137503, /*  30.04 */
    0.983223405831, /*  30.06 */
    0.983234659037, /*  30.08 */
    0.983245897154, /*  30.10 */
    0.983257120210, /*  30.12 */
    0.983268328238, /*  30.14 */
    0.983279521266, /*  30.16 */
    0.983290699324, /*  30.18 */
    0.983301862444, /*  30.20 */
    0.983313010654, /*  30.22 */
    0.983324143985, /*  30.24 */
    0.983335262466, /*  30.26 */
    0.983346366127, /*  30.28 */
    0.983357454998, /*  30.30 */
    0.983368529108, /*  30.32 */
    0.983379588487, /*  30.34 */
    0.983390633164, /*  30.36 */
    0.983401663168, /*  30.38 */
    0.983412678529, /*  30.40 */
    0.983423679276, /*  30.42 */
    0.983434665437, /*  30.44 */
    0.983445637042, /*  30.46 */
    0.983456594120, /*  30.48 */
    0.983467536699, /*  30.50 */
    0.983478464809, /*  30.52 */
    0.983489378478, /*  30.54 */
    0.983500277734, /*  30.56 */
    0.983511162607, /*  30.58 */
    0.983522033124, /*  30.60 */
    0.983532889314, /*  30.62 */
    0.983543731206, /*  30.64 */
    0.983554558827, /*  30.66 */
    0.983565372206, /*  30.68 */
    0.983576171370, /*  30.70 */
    0.983586956349, /*  30.72 */
    0.983597727169, /*  30.74 */
    0.983608483859, /*  30.76 */
    0.983619226446, /*  30.78 */
    0.983629954958, /*  30.80 */
    0.983640669423, /*  30.82 */
    0.983651369869, /*  30.84 */
    0.983662056322, /*  30.86 */
    0.983672728811, /*  30.88 */
    0.983683387363, /*  30.90 */
    0.983694032004, /*  30.92 */
    0.983704662763, /*  30.94 */
    0.983715279666, /*  30.96 */
    0.983725882740, /*  30.98 */
    0.983736472013, /*  31.00 */
    0.983747047511, /*  31.02 */
    0.983757609262, /*  31.04 */
    0.983768157291, /*  31.06 */
    0.983778691626, /*  31.08 */
    0.983789212294, /*  31.10 */
    0.983799719320, /*  31.12 */
    0.983810212732, /*  31.14 */
    0.983820692556, /*  31.16 */
    0.983831158819, /*  31.18 */
    0.983841611546, /*  31.20 */
    0.983852050764, /*  31.22 */
    0.983862476499, /*  31.24 */
    0.983872888777, /*  31.26 */
    0.983883287624, /*  31.28 */
    0.983893673067, /*  31.30 */
    0.983904045131, /*  31.32 */
    0.983914403841, /*  31.34 */
    0.983924749225, /*  31.36 */
    0.983935081307, /*  31.38 */
    0.983945400113, /*  31.40 */
    0.983955705669, /*  31.42 */
    0.983965997999, /*  31.44 */
    0.983976277131, /*  31.46 */
    0.983986543088, /*  31.48 */
    0.983996795897, /*  31.50 */
    0.984007035582, /*  31.52 */
    0.984017262170, /*  31.54 */
    0.984027475684, /*  31.56 */
    0.984037676150, /*  31.58 */
    0.984047863592, /*  31.60 */
    0.984058038037, /*  31.62 */
    0.984068199509, /*  31.64 */
    0.984078348032, /*  31.66 */
    0.984088483631, /*  31.68 */
    0.984098606331, /*  31.70 */
    0.984108716157, /*  31.72 */
    0.984118813133, /*  31.74 */
    0.984128897283, /*  31.76 */
    0.984138968633, /*  31.78 */
    0.984149027205, /*  31.80 */
    0.984159073026, /*  31.82 */
    0.984169106119, /*  31.84 */
    0.984179126507, /*  31.86 */
    0.984189134216, /*  31.88 */
    0.984199129269, /*  31.90 */
    0.984209111691, /*  31.92 */
    0.984219081504, /*  31.94 */
    0.984229038734, /*  31.96 */
    0.984238983404, /*  31.98 */
    0.984248915537, /*  32.00 */
    0.984258835158, /*  32.02 */
    0.984268742289, /*  32.04 */
    0.984278636956, /*  32.06 */
    0.984288519180, /*  32.08 */
    0.984298388986, /*  32.10 */
    0.984308246397, /*  32.12 */
    0.984318091436, /*  32.14 */
    0.984327924127, /*  32.16 */
    0.984337744492, /*  32.18 */
    0.984347552556, /*  32.20 */
    0.984357348340, /*  32.22 */
    0.984367131869, /*  32.24 */
    0.984376903165, /*  32.26 */
    0.984386662251, /*  32.28 */
    0.984396409149, /*  32.30 */
    0.984406143884, /*  32.32 */
    0.984415866477, /*  32.34 */
    0.984425576951, /*  32.36 */
    0.984435275329, /*  32.38 */
    0.984444961633, /*  32.40 */
    0.984454635887, /*  32.42 */
    0.984464298112, /*  32.44 */
    0.984473948331, /*  32.46 */
    0.984483586566, /*  32.48 */
    0.984493212839, /*  32.50 */
    0.984502827174, /*  32.52 */
    0.984512429591, /*  32.54 */
    0.984522020114, /*  32.56 */
    0.984531598764, /*  32.58 */
    0.984541165563, /*  32.60 */
    0.984550720534, /*  32.62 */
    0.984560263697, /*  32.64 */
    0.984569795076, /*  32.66 */
    0.984579314692, /*  32.68 */
    0.984588822566, /*  32.70 */
    0.984598318721, /*  32.72 */
    0.984607803178, /*  32.74 */
    0.984617275959, /*  32.76 */
    0.984626737084, /*  32.78 */
    0.984636186577, /*  32.80 */
    0.984645624458, /*  32.82 */
    0.984655050748, /*  32.84 */
    0.984664465469, /*  32.86 */
    0.984673868642, /*  32.88 */
    0.984683260289, /*  32.90 */
    0.984692640430, /*  32.92 */
    0.984702009088, /*  32.94 */
    0.984711366282, /*  32.96 */
    0.984720712033, /*  32.98 */
    0.984730046364, /*  33.00 */
    0.984739369294, /*  33.02 */
    0.984748680846, /*  33.04 */
    0.984757981038, /*  33.06 */
    0.984767269893, /*  33.08 */
    0.984776547431, /*  33.10 */
    0.984785813672, /*  33.12 */
    0.984795068638, /*  33.14 */
    0.984804312348, /*  33.16 */
    0.984813544824, /*  33.18 */
    0.984822766086, /*  33.20 */
    0.984831976154, /*  33.22 */
    0.984841175048, /*  33.24 */
    0.984850362790, /*  33.26 */
    0.984859539399, /*  33.28 */
    0.984868704895, /*  33.30 */
    0.984877859299, /*  33.32 */
    0.984887002630, /*  33.34 */
    0.984896134910, /*  33.36 */
    0.984905256157, /*  33.38 */
    0.984914366392, /*  33.40 */
    0.984923465635, /*  33.42 */
    0.984932553906, /*  33.44 */
    0.984941631224, /*  33.46 */
    0.984950697610, /*  33.48 */
    0.984959753082, /*  33.50 */
    0.984968797662, /*  33.52 */
    0.984977831368, /*  33.54 */
    0.984986854219, /*  33.56 */
    0.984995866236, /*  33.58 */
    0.985004867439, /*  33.60 */
    0.985013857846, /*  33.62 */
    0.985022837476, /*  33.64 */
    0.985031806350, /*  33.66 */
    0.985040764487, /*  33.68 */
    0.985049711906, /*  33.70 */
    0.985058648625, /*  33.72 */
    0.985067574665, /*  33.74 */
    0.985076490045, /*  33.76 */
    0.985085394783, /*  33.78 */
    0.985094288898, /*  33.80 */
    0.985103172410, /*  33.82 */
    0.985112045338, /*  33.84 */
    0.985120907700, /*  33.86 */
    0.985129759516, /*  33.88 */
    0.985138600803, /*  33.90 */
    0.985147431581, /*  33.92 */
    0.985156251869, /*  33.94 */
    0.985165061686, /*  33.96 */
    0.985173861049, /*  33.98 */
    0.985182649978, /*  34.00 */
    0.985191428491, /*  34.02 */
    0.985200196606, /*  34.04 */
    0.985208954343, /*  34.06 */
    0.985217701719, /*  34.08 */
    0.985226438753, /*  34.10 */
    0.985235165463, /*  34.12 */
    0.985243881868, /*  34.14 */
    0.985252587985, /*  34.16 */
    0.985261283834, /*  34.18 */
    0.985269969431, /*  34.20 */
    0.985278644796, /*  34.22 */
    0.985287309946, /*  34.24 */
    0.985295964899, /*  34.26 */
    0.985304609674, /*  34.28 */
    0.985313244288, /*  34.30 */
    0.985321868759, /*  34.32 */
    0.985330483105, /*  34.34 */
    0.985339087344, /*  34.36 */
    0.985347681494, /*  34.38 */
    0.985356265572, /*  34.40 */
    0.985364839596, /*  34.42 */
    0.985373403584, /*  34.44 */
    0.985381957553, /*  34.46 */
    0.985390501522, /*  34.48 */
    0.985399035506, /*  34.50 */
    0.985407559525, /*  34.52 */
    0.985416073595, /*  34.54 */
    0.985424577734, /*  34.56 */
    0.985433071959, /*  34.58 */
    0.985441556288, /*  34.60 */
    0.985450030737, /*  34.62 */
    0.985458495325, /*  34.64 */
    0.985466950067, /*  34.66 */
    0.985475394982, /*  34.68 */
    0.985483830087, /*  34.70 */
    0.985492255399, /*  34.72 */
    0.985500670934, /*  34.74 */
    0.985509076709, /*  34.76 */
    0.985517472743, /*  34.78 */
    0.985525859051, /*  34.80 */
    0.985534235650, /*  34.82 */
    0.985542602558, /*  34.84 */
    0.985550959791, /*  34.86 */
    0.985559307366, /*  34.88 */
    0.985567645300, /*  34.90 */
    0.985575973609, /*  34.92 */
    0.985584292310, /*  34.94 */
    0.985592601420, /*  34.96 */
    0.985600900955, /*  34.98 */
    0.985609190931, /*  35.00 */
    0.985617471366, /*  35.02 */
    0.985625742276, /*  35.04 */
    0.985634003677, /*  35.06 */
    0.985642255585, /*  35.08 */
    0.985650498018, /*  35.10 */
    0.985658730990, /*  35.12 */
    0.985666954519, /*  35.14 */
    0.985675168621, /*  35.16 */
    0.985683373312, /*  35.18 */
    0.985691568608, /*  35.20 */
    0.985699754525, /*  35.22 */
    0.985707931080, /*  35.24 */
    0.985716098288, /*  35.26 */
    0.985724256165, /*  35.28 */
    0.985732404728, /*  35.30 */
    0.985740543992, /*  35.32 */
    0.985748673973, /*  35.34 */
    0.985756794688, /*  35.36 */
    0.985764906151, /*  35.38 */
    0.985773008380, /*  35.40 */
    0.985781101389, /*  35.42 */
    0.985789185194, /*  35.44 */
    0.985797259811, /*  35.46 */
    0.985805325256, /*  35.48 */
    0.985813381544, /*  35.50 */
    0.985821428690, /*  35.52 */
    0.985829466712, /*  35.54 */
    0.985837495623, /*  35.56 */
    0.985845515439, /*  35.58 */
    0.985853526177, /*  35.60 */
    0.985861527850, /*  35.62 */
    0.985869520475, /*  35.64 */
    0.985877504068, /*  35.66 */
    0.985885478642, /*  35.68 */
    0.985893444214, /*  35.70 */
    0.985901400799, /*  35.72 */
    0.985909348412, /*  35.74 */
    0.985917287067, /*  35.76 */
    0.985925216782, /*  35.78 */
    0.985933137569, /*  35.80 */
    0.985941049445, /*  35.82 */
    0.985948952425, /*  35.84 */
    0.985956846523, /*  35.86 */
    0.985964731755, /*  35.88 */
    0.985972608135, /*  35.90 */
    0.985980475678, /*  35.92 */
    0.985988334399, /*  35.94 */
    0.985996184313, /*  35.96 */
    0.986004025436, /*  35.98 */
    0.986011857780, /*  36.00 */
    0.986019681362, /*  36.02 */
    0.986027496196, /*  36.04 */
    0.986035302297, /*  36.06 */
    0.986043099679, /*  36.08 */
    0.986050888357, /*  36.10 */
    0.986058668345, /*  36.12 */
    0.986066439658, /*  36.14 */
    0.986074202311, /*  36.16 */
    0.986081956318, /*  36.18 */
    0.986089701693, /*  36.20 */
    0.986097438451, /*  36.22 */
    0.986105166606, /*  36.24 */
    0.986112886173, /*  36.26 */
    0.986120597165, /*  36.28 */
    0.986128299598, /*  36.30 */
    0.986135993485, /*  36.32 */
    0.986143678840, /*  36.34 */
    0.986151355679, /*  36.36 */
    0.986159024014, /*  36.38 */
    0.986166683860, /*  36.40 */
    0.986174335232, /*  36.42 */
    0.986181978143, /*  36.44 */
    0.986189612607, /*  36.46 */
    0.986197238638, /*  36.48 */
    0.986204856250, /*  36.50 */
    0.986212465457, /*  36.52 */
    0.986220066274, /*  36.54 */
    0.986227658713, /*  36.56 */
    0.986235242789, /*  36.58 */
    0.986242818516, /*  36.60 */
    0.986250385907, /*  36.62 */
    0.986257944976, /*  36.64 */
    0.986265495737, /*  36.66 */
    0.986273038204, /*  36.68 */
    0.986280572389, /*  36.70 */
    0.986288098308, /*  36.72 */
    0.986295615973, /*  36.74 */
    0.986303125398, /*  36.76 */
    0.986310626596, /*  36.78 */
    0.986318119581, /*  36.80 */
    0.986325604367, /*  36.82 */
    0.986333080967, /*  36.84 */
    0.986340549395, /*  36.86 */
    0.986348009663, /*  36.88 */
    0.986355461785, /*  36.90 */
    0.986362905775, /*  36.92 */
    0.986370341645, /*  36.94 */
    0.986377769410, /*  36.96 */
    0.986385189082, /*  36.98 */
    0.986392600674, /*  37.00 */
    0.986400004200, /*  37.02 */
    0.986407399673, /*  37.04 */
    0.986414787107, /*  37.06 */
    0.986422166513, /*  37.08 */
    0.986429537905, /*  37.10 */
    0.986436901297, /*  37.12 */
    0.986444256701, /*  37.14 */
    0.986451604131, /*  37.16 */
    0.986458943598, /*  37.18 */
    0.986466275117, /*  37.20 */
    0.986473598700, /*  37.22 */
    0.986480914360, /*  37.24 */
    0.986488222110, /*  37.26 */
    0.986495521962, /*  37.28 */
    0.986502813930, /*  37.30 */
    0.986510098026, /*  37.32 */
    0.986517374263, /*  37.34 */
    0.986524642653, /*  37.36 */
    0.986531903210, /*  37.38 */
    0.986539155946, /*  37.40 */
    0.986546400874, /*  37.42 */
    0.986553638006, /*  37.44 */
    0.986560867355, /*  37.46 */
    0.986568088933, /*  37.48 */
    0.986575302753, /*  37.50 */
    0.986582508827, /*  37.52 */
    0.986589707169, /*  37.54 */
    0.986596897789, /*  37.56 */
    0.986604080702, /*  37.58 */
    0.986611255918, /*  37.60 */
    0.986618423451, /*  37.62 */
    0.986625583313, /*  37.64 */
    0.986632735516, /*  37.66 */
    0.986639880072, /*  37.68 */
    0.986647016994, /*  37.70 */
    0.986654146294, /*  37.72 */
    0.986661267984, /*  37.74 */
    0.986668382077, /*  37.76 */
    0.986675488583, /*  37.78 */
    0.986682587517, /*  37.80 */
    0.986689678889, /*  37.82 */
    0.986696762712, /*  37.84 */
    0.986703838997, /*  37.86 */
    0.986710907758, /*  37.88 */
    0.986717969005, /*  37.90 */
    0.986725022751, /*  37.92 */
    0.986732069008, /*  37.94 */
    0.986739107788, /*  37.96 */
    0.986746139102, /*  37.98 */
    0.986753162962, /*  38.00 */
    0.986760179381, /*  38.02 */
    0.986767188370, /*  38.04 */
    0.986774189941, /*  38.06 */
    0.986781184105, /*  38.08 */
    0.986788170875, /*  38.10 */
    0.986795150262, /*  38.12 */
    0.986802122278, /*  38.14 */
    0.986809086935, /*  38.16 */
    0.986816044243, /*  38.18 */
    0.986822994216, /*  38.20 */
    0.986829936864, /*  38.22 */
    0.986836872199, /*  38.24 */
    0.986843800232, /*  38.26 */
    0.986850720975, /*  38.28 */
    0.986857634440, /*  38.30 */
    0.986864540638, /*  38.32 */
    0.986871439581, /*  38.34 */
    0.986878331279, /*  38.36 */
    0.986885215745, /*  38.38 */
    0.986892092989, /*  38.40 */
    0.986898963023, /*  38.42 */
    0.986905825859, /*  38.44 */
    0.986912681508, /*  38.46 */
    0.986919529980, /*  38.48 */
    0.986926371288, /*  38.50 */
    0.986933205442, /*  38.52 */
    0.986940032454, /*  38.54 */
    0.986946852335, /*  38.56 */
    0.986953665096, /*  38.58 */
    0.986960470748, /*  38.60 */
    0.986967269302, /*  38.62 */
    0.986974060770, /*  38.64 */
    0.986980845162, /*  38.66 */
    0.986987622490, /*  38.68 */
    0.986994392765, /*  38.70 */
    0.987001155997, /*  38.72 */
    0.987007912197, /*  38.74 */
    0.987014661377, /*  38.76 */
    0.987021403548, /*  38.78 */
    0.987028138720, /*  38.80 */
    0.987034866905, /*  38.82 */
    0.987041588112, /*  38.84 */
    0.987048302354, /*  38.86 */
    0.987055009640, /*  38.88 */
    0.987061709982, /*  38.90 */
    0.987068403390, /*  38.92 */
    0.987075089876, /*  38.94 */
    0.987081769449, /*  38.96 */
    0.987088442122, /*  38.98 */
    0.987095107903, /*  39.00 */
    0.987101766804, /*  39.02 */
    0.987108418836, /*  39.04 */
    0.987115064010, /*  39.06 */
    0.987121702335, /*  39.08 */
    0.987128333822, /*  39.10 */
    0.987134958483, /*  39.12 */
    0.987141576327, /*  39.14 */
    0.987148187366, /*  39.16 */
    0.987154791608, /*  39.18 */
    0.987161389066, /*  39.20 */
    0.987167979749, /*  39.22 */
    0.987174563669, /*  39.24 */
    0.987181140834, /*  39.26 */
    0.987187711256, /*  39.28 */
    0.987194274946, /*  39.30 */
    0.987200831912, /*  39.32 */
    0.987207382167, /*  39.34 */
    0.987213925719, /*  39.36 */
    0.987220462580, /*  39.38 */
    0.987226992760, /*  39.40 */
    0.987233516268, /*  39.42 */
    0.987240033115, /*  39.44 */
    0.987246543312, /*  39.46 */
    0.987253046868, /*  39.48 */
    0.987259543793, /*  39.50 */
    0.987266034099, /*  39.52 */
    0.987272517794, /*  39.54 */
    0.987278994889, /*  39.56 */
    0.987285465394, /*  39.58 */
    0.987291929319, /*  39.60 */
    0.987298386674, /*  39.62 */
    0.987304837470, /*  39.64 */
    0.987311281715, /*  39.66 */
    0.987317719421, /*  39.68 */
    0.987324150596, /*  39.70 */
    0.987330575251, /*  39.72 */
    0.987336993397, /*  39.74 */
    0.987343405042, /*  39.76 */
    0.987349810196, /*  39.78 */
    0.987356208871, /*  39.80 */
    0.987362601074, /*  39.82 */
    0.987368986817, /*  39.84 */
    0.987375366108, /*  39.86 */
    0.987381738958, /*  39.88 */
    0.987388105377, /*  39.90 */
    0.987394465374, /*  39.92 */
    0.987400818958, /*  39.94 */
    0.987407166141, /*  39.96 */
    0.987413506930, /*  39.98 */
    0.987419841336, /*  40.00 */
    0.987426169369, /*  40.02 */
    0.987432491038, /*  40.04 */
    0.987438806353, /*  40.06 */
    0.987445115323, /*  40.08 */
    0.987451417958, /*  40.10 */
    0.987457714268, /*  40.12 */
    0.987464004261, /*  40.14 */
    0.987470287948, /*  40.16 */
    0.987476565338, /*  40.18 */
    0.987482836440, /*  40.20 */
    0.987489101264, /*  40.22 */
    0.987495359819, /*  40.24 */
    0.987501612115, /*  40.26 */
    0.987507858161, /*  40.28 */
    0.987514097966, /*  40.30 */
    0.987520331540, /*  40.32 */
    0.987526558892, /*  40.34 */
    0.987532780032, /*  40.36 */
    0.987538994968, /*  40.38 */
    0.987545203711, /*  40.40 */
    0.987551406268, /*  40.42 */
    0.987557602650, /*  40.44 */
    0.987563792866, /*  40.46 */
    0.987569976924, /*  40.48 */
    0.987576154835, /*  40.50 */
    0.987582326607, /*  40.52 */
    0.987588492249, /*  40.54 */
    0.987594651771, /*  40.56 */
    0.987600805181, /*  40.58 */
    0.987606952489, /*  40.60 */
    0.987613093704, /*  40.62 */
    0.987619228835, /*  40.64 */
    0.987625357890, /*  40.66 */
    0.987631480880, /*  40.68 */
    0.987637597812, /*  40.70 */
    0.987643708697, /*  40.72 */
    0.987649813542, /*  40.74 */
    0.987655912357, /*  40.76 */
    0.987662005150, /*  40.78 */
    0.987668091932, /*  40.80 */
    0.987674172710, /*  40.82 */
    0.987680247493, /*  40.84 */
    0.987686316291, /*  40.86 */
    0.987692379111, /*  40.88 */
    0.987698435964, /*  40.90 */
    0.987704486858, /*  40.92 */
    0.987710531801, /*  40.94 */
    0.987716570802, /*  40.96 */
    0.987722603870, /*  40.98 */
    0.987728631015, /*  41.00 */
    0.987734652243, /*  41.02 */
    0.987740667565, /*  41.04 */
    0.987746676989, /*  41.06 */
    0.987752680524, /*  41.08 */
    0.987758678178, /*  41.10 */
    0.987764669960, /*  41.12 */
    0.987770655878, /*  41.14 */
    0.987776635942, /*  41.16 */
    0.987782610159, /*  41.18 */
    0.987788578538, /*  41.20 */
    0.987794541089, /*  41.22 */
    0.987800497818, /*  41.24 */
    0.987806448736, /*  41.26 */
    0.987812393850, /*  41.28 */
    0.987818333169, /*  41.30 */
    0.987824266701, /*  41.32 */
    0.987830194455, /*  41.34 */
    0.987836116439, /*  41.36 */
    0.987842032662, /*  41.38 */
    0.987847943133, /*  41.40 */
    0.987853847858, /*  41.42 */
    0.987859746847, /*  41.44 */
    0.987865640109, /*  41.46 */
    0.987871527651, /*  41.48 */
    0.987877409482, /*  41.50 */
    0.987883285610, /*  41.52 */
    0.987889156044, /*  41.54 */
    0.987895020791, /*  41.56 */
    0.987900879860, /*  41.58 */
    0.987906733260, /*  41.60 */
    0.987912580998, /*  41.62 */
    0.987918423082, /*  41.64 */
    0.987924259522, /*  41.66 */
    0.987930090325, /*  41.68 */
    0.987935915498, /*  41.70 */
    0.987941735051, /*  41.72 */
    0.987947548992, /*  41.74 */
    0.987953357328, /*  41.76 */
    0.987959160068, /*  41.78 */
    0.987964957220, /*  41.80 */
    0.987970748791, /*  41.82 */
    0.987976534791, /*  41.84 */
    0.987982315226, /*  41.86 */
    0.987988090106, /*  41.88 */
    0.987993859437, /*  41.90 */
    0.987999623228, /*  41.92 */
    0.988005381487, /*  41.94 */
    0.988011134223, /*  41.96 */
    0.988016881442, /*  41.98 */
    0.988022623153, /*  42.00 */
    0.988028359363, /*  42.02 */
    0.988034090081, /*  42.04 */
    0.988039815315, /*  42.06 */
    0.988045535072, /*  42.08 */
    0.988051249360, /*  42.10 */
    0.988056958187, /*  42.12 */
    0.988062661562, /*  42.14 */
    0.988068359490, /*  42.16 */
    0.988074051982, /*  42.18 */
    0.988079739043, /*  42.20 */
    0.988085420683, /*  42.22 */
    0.988091096908, /*  42.24 */
    0.988096767727, /*  42.26 */
    0.988102433148, /*  42.28 */
    0.988108093177, /*  42.30 */
    0.988113747822, /*  42.32 */
    0.988119397093, /*  42.34 */
    0.988125040995, /*  42.36 */
    0.988130679536, /*  42.38 */
    0.988136312725, /*  42.40 */
    0.988141940569, /*  42.42 */
    0.988147563076, /*  42.44 */
    0.988153180252, /*  42.46 */
    0.988158792106, /*  42.48 */
    0.988164398645, /*  42.50 */
    0.988169999878, /*  42.52 */
    0.988175595810, /*  42.54 */
    0.988181186450, /*  42.56 */
    0.988186771806, /*  42.58 */
    0.988192351884, /*  42.60 */
    0.988197926693, /*  42.62 */
    0.988203496239, /*  42.64 */
    0.988209060531, /*  42.66 */
    0.988214619575, /*  42.68 */
    0.988220173379, /*  42.70 */
    0.988225721951, /*  42.72 */
    0.988231265298, /*  42.74 */
    0.988236803426, /*  42.76 */
    0.988242336345, /*  42.78 */
    0.988247864060, /*  42.80 */
    0.988253386580, /*  42.82 */
    0.988258903911, /*  42.84 */
    0.988264416061, /*  42.86 */
    0.988269923038, /*  42.88 */
    0.988275424848, /*  42.90 */
    0.988280921498, /*  42.92 */
    0.988286412997, /*  42.94 */
    0.988291899351, /*  42.96 */
    0.988297380567, /*  42.98 */
    0.988302856653, /*  43.00 */
    0.988308327616, /*  43.02 */
    0.988313793463, /*  43.04 */
    0.988319254202, /*  43.06 */
    0.988324709838, /*  43.08 */
    0.988330160381, /*  43.10 */
    0.988335605836, /*  43.12 */
    0.988341046211, /*  43.14 */
    0.988346481512, /*  43.16 */
    0.988351911748, /*  43.18 */
    0.988357336925, /*  43.20 */
    0.988362757050, /*  43.22 */
    0.988368172130, /*  43.24 */
    0.988373582173, /*  43.26 */
    0.988378987184, /*  43.28 */
    0.988384387173, /*  43.30 */
    0.988389782144, /*  43.32 */
    0.988395172106, /*  43.34 */
    0.988400557065, /*  43.36 */
    0.988405937028, /*  43.38 */
    0.988411312002, /*  43.40 */
    0.988416681995, /*  43.42 */
    0.988422047012, /*  43.44 */
    0.988427407061, /*  43.46 */
    0.988432762150, /*  43.48 */
    0.988438112284, /*  43.50 */
    0.988443457470, /*  43.52 */
    0.988448797717, /*  43.54 */
    0.988454133029, /*  43.56 */
    0.988459463415, /*  43.58 */
    0.988464788880, /*  43.60 */
    0.988470109432, /*  43.62 */
    0.988475425078, /*  43.64 */
    0.988480735825, /*  43.66 */
    0.988486041678, /*  43.68 */
    0.988491342645, /*  43.70 */
    0.988496638733, /*  43.72 */
    0.988501929949, /*  43.74 */
    0.988507216298, /*  43.76 */
    0.988512497788, /*  43.78 */
    0.988517774426, /*  43.80 */
    0.988523046218, /*  43.82 */
    0.988528313171, /*  43.84 */
    0.988533575291, /*  43.86 */
    0.988538832585, /*  43.88 */
    0.988544085061, /*  43.90 */
    0.988549332723, /*  43.92 */
    0.988554575580, /*  43.94 */
    0.988559813637, /*  43.96 */
    0.988565046902, /*  43.98 */
    0.988570275380, /*  44.00 */
    0.988575499079, /*  44.02 */
    0.988580718005, /*  44.04 */
    0.988585932164, /*  44.06 */
    0.988591141563, /*  44.08 */
    0.988596346209, /*  44.10 */
    0.988601546108, /*  44.12 */
    0.988606741266, /*  44.14 */
    0.988611931690, /*  44.16 */
    0.988617117387, /*  44.18 */
    0.988622298362, /*  44.20 */
    0.988627474623, /*  44.22 */
    0.988632646176, /*  44.24 */
    0.988637813026, /*  44.26 */
    0.988642975182, /*  44.28 */
    0.988648132648, /*  44.30 */
    0.988653285432, /*  44.32 */
    0.988658433540, /*  44.34 */
    0.988663576978, /*  44.36 */
    0.988668715752, /*  44.38 */
    0.988673849869, /*  44.40 */
    0.988678979336, /*  44.42 */
    0.988684104157, /*  44.44 */
    0.988689224341, /*  44.46 */
    0.988694339893, /*  44.48 */
    0.988699450819, /*  44.50 */
    0.988704557125, /*  44.52 */
    0.988709658819, /*  44.54 */
    0.988714755905, /*  44.56 */
    0.988719848392, /*  44.58 */
    0.988724936283, /*  44.60 */
    0.988730019587, /*  44.62 */
    0.988735098308, /*  44.64 */
    0.988740172454, /*  44.66 */
    0.988745242030, /*  44.68 */
    0.988750307043, /*  44.70 */
    0.988755367498, /*  44.72 */
    0.988760423403, /*  44.74 */
    0.988765474762, /*  44.76 */
    0.988770521582, /*  44.78 */
    0.988775563870, /*  44.80 */
    0.988780601631, /*  44.82 */
    0.988785634872, /*  44.84 */
    0.988790663598, /*  44.86 */
    0.988795687816, /*  44.88 */
    0.988800707531, /*  44.90 */
    0.988805722750, /*  44.92 */
    0.988810733479, /*  44.94 */
    0.988815739724, /*  44.96 */
    0.988820741490, /*  44.98 */
    0.988825738785, /*  45.00 */
    0.988830731613, /*  45.02 */
    0.988835719981, /*  45.04 */
    0.988840703894, /*  45.06 */
    0.988845683360, /*  45.08 */
    0.988850658383, /*  45.10 */
    0.988855628970, /*  45.12 */
    0.988860595126, /*  45.14 */
    0.988865556858, /*  45.16 */
    0.988870514171, /*  45.18 */
    0.988875467071, /*  45.20 */
    0.988880415565, /*  45.22 */
    0.988885359658, /*  45.24 */
    0.988890299355, /*  45.26 */
    0.988895234664, /*  45.28 */
    0.988900165589, /*  45.30 */
    0.988905092137, /*  45.32 */
    0.988910014312, /*  45.34 */
    0.988914932122, /*  45.36 */
    0.988919845572, /*  45.38 */
    0.988924754668, /*  45.40 */
    0.988929659415, /*  45.42 */
    0.988934559820, /*  45.44 */
    0.988939455887, /*  45.46 */
    0.988944347623, /*  45.48 */
    0.988949235034, /*  45.50 */
    0.988954118125, /*  45.52 */
    0.988958996903, /*  45.54 */
    0.988963871371, /*  45.56 */
    0.988968741538, /*  45.58 */
    0.988973607407, /*  45.60 */
    0.988978468985, /*  45.62 */
    0.988983326278, /*  45.64 */
    0.988988179291, /*  45.66 */
    0.988993028030, /*  45.68 */
    0.988997872500, /*  45.70 */
    0.989002712707, /*  45.72 */
    0.989007548657, /*  45.74 */
    0.989012380356, /*  45.76 */
    0.989017207808, /*  45.78 */
    0.989022031019, /*  45.80 */
    0.989026849996, /*  45.82 */
    0.989031664744, /*  45.84 */
    0.989036475268, /*  45.86 */
    0.989041281573, /*  45.88 */
    0.989046083666, /*  45.90 */
    0.989050881552, /*  45.92 */
    0.989055675236, /*  45.94 */
    0.989060464725, /*  45.96 */
    0.989065250022, /*  45.98 */
    0.989070031135, /*  46.00 */
    0.989074808068, /*  46.02 */
    0.989079580827, /*  46.04 */
    0.989084349417, /*  46.06 */
    0.989089113844, /*  46.08 */
    0.989093874114, /*  46.10 */
    0.989098630231, /*  46.12 */
    0.989103382201, /*  46.14 */
    0.989108130030, /*  46.16 */
    0.989112873723, /*  46.18 */
    0.989117613285, /*  46.20 */
    0.989122348722, /*  46.22 */
    0.989127080040, /*  46.24 */
    0.989131807242, /*  46.26 */
    0.989136530336, /*  46.28 */
    0.989141249326, /*  46.30 */
    0.989145964218, /*  46.32 */
    0.989150675016, /*  46.34 */
    0.989155381727, /*  46.36 */
    0.989160084356, /*  46.38 */
    0.989164782907, /*  46.40 */
    0.989169477387, /*  46.42 */
    0.989174167800, /*  46.44 */
    0.989178854151, /*  46.46 */
    0.989183536447, /*  46.48 */
    0.989188214693, /*  46.50 */
    0.989192888892, /*  46.52 */
    0.989197559052, /*  46.54 */
    0.989202225177, /*  46.56 */
    0.989206887272, /*  46.58 */
    0.989211545342, /*  46.60 */
    0.989216199393, /*  46.62 */
    0.989220849430, /*  46.64 */
    0.989225495458, /*  46.66 */
    0.989230137483, /*  46.68 */
    0.989234775509, /*  46.70 */
    0.989239409541, /*  46.72 */
    0.989244039586, /*  46.74 */
    0.989248665647, /*  46.76 */
    0.989253287730, /*  46.78 */
    0.989257905841, /*  46.80 */
    0.989262519984, /*  46.82 */
    0.989267130164, /*  46.84 */
    0.989271736387, /*  46.86 */
    0.989276338657, /*  46.88 */
    0.989280936980, /*  46.90 */
    0.989285531361, /*  46.92 */
    0.989290121805, /*  46.94 */
    0.989294708316, /*  46.96 */
    0.989299290901, /*  46.98 */
    0.989303869564, /*  47.00 */
    0.989308444309, /*  47.02 */
    0.989313015143, /*  47.04 */
    0.989317582070, /*  47.06 */
    0.989322145095, /*  47.08 */
    0.989326704223, /*  47.10 */
    0.989331259459, /*  47.12 */
    0.989335810808, /*  47.14 */
    0.989340358275, /*  47.16 */
    0.989344901865, /*  47.18 */
    0.989349441583, /*  47.20 */
    0.989353977434, /*  47.22 */
    0.989358509423, /*  47.24 */
    0.989363037555, /*  47.26 */
    0.989367561834, /*  47.28 */
    0.989372082266, /*  47.30 */
    0.989376598855, /*  47.32 */
    0.989381111607, /*  47.34 */
    0.989385620526, /*  47.36 */
    0.989390125617, /*  47.38 */
    0.989394626886, /*  47.40 */
    0.989399124336, /*  47.42 */
    0.989403617973, /*  47.44 */
    0.989408107802, /*  47.46 */
    0.989412593827, /*  47.48 */
    0.989417076053, /*  47.50 */
    0.989421554486, /*  47.52 */
    0.989426029129, /*  47.54 */
    0.989430499988, /*  47.56 */
    0.989434967068, /*  47.58 */
    0.989439430373, /*  47.60 */
    0.989443889909, /*  47.62 */
    0.989448345679, /*  47.64 */
    0.989452797688, /*  47.66 */
    0.989457245943, /*  47.68 */
    0.989461690446, /*  47.70 */
    0.989466131203, /*  47.72 */
    0.989470568219, /*  47.74 */
    0.989475001498, /*  47.76 */
    0.989479431046, /*  47.78 */
    0.989483856866, /*  47.80 */
    0.989488278963, /*  47.82 */
    0.989492697343, /*  47.84 */
    0.989497112010, /*  47.86 */
    0.989501522968, /*  47.88 */
    0.989505930223, /*  47.90 */
    0.989510333778, /*  47.92 */
    0.989514733639, /*  47.94 */
    0.989519129810, /*  47.96 */
    0.989523522296, /*  47.98 */
    0.989527911101, /*  48.00 */
    0.989532296231, /*  48.02 */
    0.989536677689, /*  48.04 */
    0.989541055481, /*  48.06 */
    0.989545429610, /*  48.08 */
    0.989549800082, /*  48.10 */
    0.989554166902, /*  48.12 */
    0.989558530072, /*  48.14 */
    0.989562889599, /*  48.16 */
    0.989567245487, /*  48.18 */
    0.989571597740, /*  48.20 */
    0.989575946364, /*  48.22 */
    0.989580291361, /*  48.24 */
    0.989584632738, /*  48.26 */
    0.989588970498, /*  48.28 */
    0.989593304646, /*  48.30 */
    0.989597635187, /*  48.32 */
    0.989601962125, /*  48.34 */
    0.989606285464, /*  48.36 */
    0.989610605209, /*  48.38 */
    0.989614921365, /*  48.40 */
    0.989619233936, /*  48.42 */
    0.989623542926, /*  48.44 */
    0.989627848340, /*  48.46 */
    0.989632150183, /*  48.48 */
    0.989636448458, /*  48.50 */
    0.989640743170, /*  48.52 */
    0.989645034325, /*  48.54 */
    0.989649321925, /*  48.56 */
    0.989653605975, /*  48.58 */
    0.989657886481, /*  48.60 */
    0.989662163446, /*  48.62 */
    0.989666436875, /*  48.64 */
    0.989670706771, /*  48.66 */
    0.989674973141, /*  48.68 */
    0.989679235987, /*  48.70 */
    0.989683495314, /*  48.72 */
    0.989687751126, /*  48.74 */
    0.989692003429, /*  48.76 */
    0.989696252226, /*  48.78 */
    0.989700497521, /*  48.80 */
    0.989704739320, /*  48.82 */
    0.989708977625, /*  48.84 */
    0.989713212442, /*  48.86 */
    0.989717443775, /*  48.88 */
    0.989721671628, /*  48.90 */
    0.989725896005, /*  48.92 */
    0.989730116911, /*  48.94 */
    0.989734334351, /*  48.96 */
    0.989738548327, /*  48.98 */
    0.989742758845, /*  49.00 */
    0.989746965908, /*  49.02 */
    0.989751169522, /*  49.04 */
    0.989755369690, /*  49.06 */
    0.989759566416, /*  49.08 */
    0.989763759705, /*  49.10 */
    0.989767949561, /*  49.12 */
    0.989772135989, /*  49.14 */
    0.989776318991, /*  49.16 */
    0.989780498573, /*  49.18 */
    0.989784674739, /*  49.20 */
    0.989788847493, /*  49.22 */
    0.989793016839, /*  49.24 */
    0.989797182782, /*  49.26 */
    0.989801345325, /*  49.28 */
    0.989805504472, /*  49.30 */
    0.989809660228, /*  49.32 */
    0.989813812597, /*  49.34 */
    0.989817961584, /*  49.36 */
    0.989822107191, /*  49.38 */
    0.989826249424, /*  49.40 */
    0.989830388286, /*  49.42 */
    0.989834523782, /*  49.44 */
    0.989838655915, /*  49.46 */
    0.989842784690, /*  49.48 */
    0.989846910111, /*  49.50 */
    0.989851032182, /*  49.52 */
    0.989855150907, /*  49.54 */
    0.989859266291, /*  49.56 */
    0.989863378336, /*  49.58 */
    0.989867487047, /*  49.60 */
    0.989871592429, /*  49.62 */
    0.989875694485, /*  49.64 */
    0.989879793220, /*  49.66 */
    0.989883888637, /*  49.68 */
    0.989887980740, /*  49.70 */
    0.989892069534, /*  49.72 */
    0.989896155022, /*  49.74 */
    0.989900237209, /*  49.76 */
    0.989904316098, /*  49.78 */
    0.989908391693, /*  49.80 */
    0.989912463999, /*  49.82 */
    0.989916533020, /*  49.84 */
    0.989920598759, /*  49.86 */
    0.989924661220, /*  49.88 */
    0.989928720407, /*  49.90 */
    0.989932776325, /*  49.92 */
    0.989936828977, /*  49.94 */
    0.989940878368, /*  49.96 */
    0.989944924500, /*  49.98 */
    0.989948967378, /*  50.00 */
    0};

/*    ------------------  kapcode  --------------------------   */
/*    This routine takes values of params hx, hy, and calculates
various derived quantities which are place in Basic, as either the
Plain or the Factor quantites. The input param 'vmp' must be set to
either the Plain or the Factor blocks for the derived quantities in some
Basic structure.
    Normally it uses an interpolation between tabulated values which
    is cubically correct for lgi0 and quadratic for aa and strictly
    continuous for both.
    It returns a linear approx to derivative of aa in daa.
    For values of kap greater than hikap it uses an asymtotic
    approximation based on a Gaussian with sigmasq = 1/kap, with
    a small correction quadratic in (1/kap).
        The values returned are:
    kap        sqrt (hx^2 + hy^2)
    rkap        recip of kap or 10e6 if kap very small
    logi0        log of normalization const for VonMises density
    aa        deriv of logi0 wrt kap
    daa        deriv of aa wrt kap
    fh        sqrt of the Fisher
    dfh        deriv of fh wrt kap
    */

void kapcode(double hx, double hy, double *vmst) {
    double del, skap, c2, c3;
    double va, vb, da, db;
    double kap;
    int ik;
    Vmpack *vmp;

    vmp = (Vmpack *)vmst;
    vmp->kap = kap = sqrt(hx * hx + hy * hy);
    if (kap > 1.0e-6)
        vmp->rkap = 1.0 / kap;
    else
        vmp->rkap = 1.0e6;
    skap = kap * kscale;
    ik = (int)skap;
    del = skap - ik;
    if (ik >= maxik)
        goto asymp;

    /*    Interpolate   */
    va = lgi0[ik];
    da = atab[ik] * kapstep;
    ik++;
    vb = lgi0[ik];
    db = atab[ik] * kapstep;

    /*    Calculate coeffs of cubic in del  */
    /*    Const term is va, linear co-eff is da
        Quadratic coeff is (-3va + 3vb - 2da - db)
        Cubic coeff is (2va - 2vb + da + db).
        Note we only use vb via the difference (vb-va)  */

    vb -= va;

    c2 = 3.0 * vb - 2.0 * da - db;
    c3 = -2.0 * vb + da + db;

    vmp->lgi0 = ((c3 * del + c2) * del + da) * del + va;

    /*    We get aa as derivative of the cubic interpolation,
        times kscale  */

    vmp->aa = ((3.0 * c3 * del + 2.0 * c2) * del + da) * kscale;

    vmp->daa = (6.0 * c3 * del + 2.0 * c2) * (kscale * kscale);
    goto derivsdone;

asymp:
    /*    For large kap, logi0 asymptotes to:
            kap + 0.5 log (2Pi / kap)
        */

    del = 1.0 / kap;

    vmp->lgi0 = kap + 0.5 * log(twopi * del) + ((0.067 * del + 0.0625) * del + 0.125) * del;
    vmp->aa = 1.0 - 0.5 * del - del * del * (0.125 + del * (0.125 + del * 0.201));
    vmp->daa = del * del * (0.5 + del * (0.25 + del * (0.375 + del * 0.804)));

derivsdone:
#ifdef ACC
    /*    Now, Fh is the square root of a/k - a^2/k^2 - a^3/k
            where a = aa, k = kappa,
        so the deriv of Fh wrt kappa is:
        (1/2Fh) {-a/k^2 +2a^2/k^3 + a^3/k^2
              + da * (1/k - 2a/k^2 -3a^2/k) }
        where da = d_aa/d_kappa.
        so from cost term N * Fh * fhsprd :  */
    aonk = aa * rkappa;
    *fsh = fh = sqrt(aonk * (1.0 - aonk - aa * aa));
    dfh = -aonk * rkappa + 2.0 * aonk * aonk * rkappa + aonk * aonk * aa + da * rkappa * (1.0 - 2.0 * aonk - 3.0 * aa * aa);
    /*    This gives deriv of Fh^2 wrt kappa  */
    *dfsh = dfh * (0.5 / fh);
#endif

#ifndef ACC
    /*    Approximate Fh by 0.5 * (1 - aa*aa)  */
    vmp->fh = 0.5 * (1.0 - vmp->aa * vmp->aa);
    vmp->dfh = -vmp->aa * vmp->daa;
#endif
    return;
}
