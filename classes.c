/*    Stuff dealing with classes as members of polpns     */
#define CLASSES 1
#include "glob.h"

/*    -----------------------  s2id  ---------------------------   */
/*    Finds class index (id) from serial, or -3 if error    */
int s2id(int ss) {
    int k;
    Class *clp;
    setpop();

    if (ss >= 1) {
        for (k = 0; k <= CurPopln->hi_class; k++) {
            clp = CurPopln->classes[k];
            if (clp && (clp->type != Vacant) && (clp->serial == ss))
                return (k);
        }
    }

    printf("No such class serial as %d", ss >> 2);
    if (ss & 3)
        printf("%c", (ss & 1) ? 'a' : 'b');
    printf("\n");
    return (-3);
}

/*    ---------------------  setclass1 --------------------------   */
void setclass1(Class *ccl) {
    CurClass = ccl;
    CurDadID = CurClass->dad_id;
    if (CurDadID >= 0)
        CurDad = CurPopln->classes[CurDadID];
    else
        CurDad = 0;
    return;
}

/*    ---------------------  setclass2 --------------------------   */
void setclass2(Class *ccl) {
    CurClass = ccl;
    vv = CurClass->vv;
    CurClass->case_score = icvv = vv[CurItem];
    cwt = CurClass->case_weight;
    CurDadID = CurClass->dad_id;
    if (CurDadID >= 0)
        CurDad = CurPopln->classes[CurDadID];
    else
        CurDad = 0;
    return;
}

/*    ---------------------   makeclass  -------------------------   */
/*    Makes the basic structure of a class (Class) with vector of
ptrs to CVinsts and CVinsts filled in also EVinsts and ptrs.
    Uses nc = pop->nc.
    if nc = 0, that's it. Otherwize, if a sample is defined in pop,
    score vector.
    Returns index of class, or negative if no good  */

int makeclass() {
    PVinst *pvars;
    CVinst **cvars, *cvi;
    EVinst **evars, *evi;
    int i, kk;

    nc = CurCtx.popln->sample_size;
    /*    If nc, check that popln has an attached sample  */
    if (nc) {
        i = sname2id(CurPopln->sample_name, 1);
        if (i < 0)
            return (-2);
        CurCtx.sample = CurSample = samples[i];
    }

    nv = CurVSet->num_vars;
    pvars = CurPopln->pvars;
    /*    Find a vacant slot in the popln's classes vec   */
    for (kk = 0; kk < CurPopln->cls_vec_len; kk++) {
        if (!CurPopln->classes[kk])
            goto gotit;
        /*    Also test for a vacated class   */
        if (CurPopln->classes[kk]->type == Vacant)
            goto vacant1;
    }
    printf("Popln full of classes\n");
    goto nospace;

gotit:
    CurClass = (Class *)gtsp(1, sizeof(Class));
    if (!CurClass)
        goto nospace;
    CurPopln->classes[kk] = CurClass;
    CurPopln->hi_class = kk; /* Highest used index in population->classes */
    CurClass->id = kk;

    /*    Make vector of ptrs to CVinsts   */
    cvars = CurClass->basics = (CVinst **)gtsp(1, nv * sizeof(CVinst *));
    if (!cvars)
        goto nospace;

    /*    Now make the CVinst blocks, which are of varying size   */
    for (i = 0; i < nv; i++) {
        pvi = pvars + i;
        avi = CurAttrs + i;
        cvi = cvars[i] = (CVinst *)gtsp(1, avi->basic_size);
        if (!cvi)
            goto nospace;
        /*    Fill in standard stuff  */
        cvi->id = avi->id;
    }

    /*    Make EVinst blocks and vector of pointers  */
    evars = CurClass->stats = (EVinst **)gtsp(1, nv * sizeof(EVinst *));
    if (!evars)
        goto nospace;

    for (i = 0; i < nv; i++) {
        pvi = pvars + i;
        avi = CurAttrs + i;
        evi = evars[i] = (EVinst *)gtsp(1, avi->stats_size);
        if (!evi)
            goto nospace;
        evi->id = pvi->id;
    }

    /*    Stomp on ptrs as yet undefined  */
    CurClass->vv = 0;
    CurClass->type = 0;
    goto donebasic;

vacant1: /* Vacant type shows structure set up but vacated.
     Use, but set new (Vacant) type,  */
    CurClass = CurPopln->classes[kk];
    cvars = CurClass->basics;
    evars = CurClass->stats;
    CurClass->type = Vacant;

donebasic:
    CurClass->best_cost = CurClass->nofac_cost = CurClass->fac_cost = 0.0;
    CurClass->weights_sum = 0.0;
    /*    Invalidate hierarchy links  */
    CurClass->dad_id = CurClass->sib_id = CurClass->son_id = -1;
    CurClass->num_sons = 0;
    for (i = 0; i < nv; i++)
        cvars[i]->signif = 1;
    CurPopln->num_classes++;
    if (kk > CurPopln->hi_class)
        CurPopln->hi_class = kk;
    if (CurClass->type == Vacant)
        goto vacant2;

    /*    If nc = 0, this completes the make.  */
    if (nc == 0)
        goto finish;
    CurClass->vv = (short *)gtsp(2, nc * sizeof(short));
    goto expanded;

vacant2:
    CurClass->type = 0;

expanded:
    for (i = 0; i < nv; i++)
        evars[i]->num_values = 0.0;
finish:
    CurClass->age = 0;
    CurClass->hold_type = CurClass->hold_use = 0;
    CurClass->weights_sum = 0.0;
    return (kk);

nospace:
    printf("No space for new class\n");
    return (-1);
}

/*    -----------------------  printclass  -----------------------  */
/*    To print parameters of a class    */
/*    If kk = -1, prints all non-subs. If kk = -2, prints all  */
char *typstr[] = {"typ?", " DAD", "LEAF", "typ?", " Sub"};
char *usestr[] = {"Tny", "Pln", "Fac"};
void print1class(Class *clp, int full) {
    int i;
    double vrms;

    printf("\nS%s", sers(clp));
    printf(" %s", typstr[((int)clp->type)]);
    if (clp->dad_id < 0)
        printf("    Root");
    else
        printf(" Dad%s", sers(CurPopln->classes[((int)clp->dad_id)]));
    printf(" Age%4d  Sz%6.1f  Use %s", clp->age, clp->weights_sum, usestr[((int)clp->use)]);
    printf("%c", (clp->use == Fac) ? ' ' : '(');
    vrms = sqrt(clp->sum_score_sq / clp->weights_sum);
    printf("Vv%6.2f", vrms);
    printf("%c", (CurClass->boost_count) ? 'B' : ' ');
    printf(" +-%5.3f", (clp->vav));
    printf(" Avv%6.3f", clp->avvv);
    printf("%c", (clp->use == Fac) ? ' ' : ')');
    if (clp->type == Dad) {
        printf("%4d sons", clp->num_sons);
    }
    printf("\n");
    printf("Pcosts  S:%9.2f  F:%9.2f  D:%9.2f  B:%9.2f\n", clp->nofac_par_cost, clp->fac_par_cost, clp->dad_par_cost, clp->best_par_cost);
    printf("Tcosts  S:%9.2f  F:%9.2f  D:%9.2f  B:%9.2f\n", clp->cstcost, clp->cftcost - CurClass->cfvcost, clp->cntcost, clp->cbtcost);
    printf("Vcost     ---------  F:%9.2f\n", clp->cfvcost);
    printf("totals  S:%9.2f  F:%9.2f  D:%9.2f  B:%9.2f\n", clp->nofac_cost, clp->fac_cost, clp->dad_cost, clp->best_cost);
    if (!full)
        return;
    for (i = 0; i < CurVSet->num_vars; i++) {
        vtp = CurAttrs[i].vtype;
        (*vtp->vprint)(clp, i);
    }
    return;
}

void printclass(int kk, int full) {
    Class *clp;
    if (kk >= 0) {
        clp = CurPopln->classes[kk];
        print1class(clp, full);
        return;
    }
    if (kk < -2) {
        printf("%d passed to printclass\n", kk);
        return;
    }
    setpop();
    clp = CurRootClass;

    do {
        if ((kk == -2) || (clp->type != Sub))
            print1class(clp, full);
        nextclass(&clp);
    } while (clp);

    return;
}

/*    -------------------------  cleartcosts  ------  */
/*    Clears tcosts (cntcost, cftcost, cstcost). Also clears newcnt,
newvsq, and calls clearstats for all variables   */
void cleartcosts(Class *ccl) {
    int i;

    setclass1(ccl);
    CurClass->mlogab = -log(CurClass->relab);
    CurClass->cstcost = CurClass->cftcost = CurClass->cntcost = 0.0;
    CurClass->cfvcost = 0.0;
    CurClass->newcnt = CurClass->newvsq = 0.0;
    CurClass->score_change_count = 0;
    CurClass->vav = 0.0;
    CurClass->totvv = 0.0;
    if (!SeeAll)
        CurClass->scancnt = 0;
    for (i = 0; i < CurVSet->num_vars; i++) {
        vtp = CurAttrs[i].vtype;
        (*vtp->clearstats)(i);
    }
    return;
}

/*    -------------------  setbestparall  ------------------------   */
/*    To set 'b' params, costs to reflect current use   */
void setbestparall(Class *ccl) {
    int i;

    setclass1(ccl);
    if (CurClass->type == Dad) {
        CurClass->best_cost = CurClass->dad_cost;
        CurClass->best_par_cost = CurClass->dad_par_cost;
        CurClass->cbtcost = CurClass->cntcost;
    } else if (CurClass->use != Fac) {
        CurClass->best_cost = CurClass->nofac_cost;
        CurClass->best_par_cost = CurClass->nofac_par_cost;
        CurClass->cbtcost = CurClass->cstcost;
    } else {
        CurClass->best_cost = CurClass->fac_cost;
        CurClass->best_par_cost = CurClass->fac_par_cost;
        CurClass->cbtcost = CurClass->cftcost;
    }
    for (i = 0; i < CurVSet->num_vars; i++) {
        vtp = CurAttrs[i].vtype;
        (*vtp->setbestparam)(i);
    }
    return;
}

/*    --------------------  scorevarall  --------------------    */
/*    To evaluate derivs of cost wrt score in all vars of a class.
    Does one item, number case  */
/*    Leaves data values set in stats, but does no scoring if class too
young. If class age = MinFacAge, will guess scores but not cost them */
/*    If control & AdjSc, will adjust score  */
void scorevarall(Class *ccl) {
    int i, igbit, oldicvv;
    double del;

    setclass2(ccl);
    if ((CurClass->age < MinFacAge) || (CurClass->use == Tiny)) {
        cvv = CurClass->avvv = CurClass->sum_score_sq = 0.0;
        icvv = 0;
        goto done;
    }
    if (CurClass->sum_score_sq > 0.0)
        goto started;
    /*    Generate a fake score to get started.   */
    CurClass->boost_count = 0;
    cvvsprd = 0.1 / nv;
    oldicvv = igbit = 0;
    cvv = (sran() < 0) ? 1.0 : -1.0;
    goto fake;

started:
    /*    Get current score  */
    oldicvv = icvv;
    igbit = icvv & 1;
    cvv = icvv * ScoreRscale;
    /*    Subtract average from last pass  */
    /*xx
        cvv -= cls->avvv;
    */
    if (CurClass->boost_count && ((Control & AdjSP) == AdjSP)) {
        cvv *= CurClass->score_boost;
        if (cvv > Maxv)
            cvv = Maxv;
        else if (cvv < -Maxv)
            cvv = -Maxv;
        del = cvv * HScoreScale;
        if (del < 0.0)
            del -= 1.0;
        icvv = del + 0.5;
        icvv = icvv << 1; /* Round to nearest even times ScoreScale */
        igbit = 0;
        cvv = icvv * ScoreRscale;
    }

    cvvsq = cvv * cvv;
    vvd1 = vvd2 = mvvd2 = vvd3 = 0.0;
    for (i = 0; i < CurVSet->num_vars; i++) {
        avi = CurAttrs + i;
        if (avi->inactive)
            goto vdone;
        vtp = avi->vtype;
        (*vtp->scorevar)(i);
    /*    scorevar should add to vvd1, vvd2, vvd3, mvvd2.  */
    vdone:;
    }
    vvd1 += cvv;
    vvd2 += 1.0;
    mvvd2 += 1.0; /*  From prior  */
    /*    There is a cost term 0.5 * cvvsprd from the prior (whence the additional
        1 in vvd2).  */
    cvvsprd = 1.0 / vvd2;
    /*    Also, overall cost includes 0.5*cvvsprd*vvd2, so there is a derivative
    term wrt cvv of 0.5*cvvsprd*vvd3  */
    vvd1 += 0.5 * cvvsprd * vvd3;
    del = vvd1 / mvvd2;
    if (Control & AdjSc) {
        cvv -= del;
    }
fake:
    if (cvv > Maxv)
        cvv = Maxv;
    else if (cvv < -Maxv)
        cvv = -Maxv;
    del = cvv * HScoreScale;
    if (del < 0.0)
        del -= 1.0;
    icvv = del + fran();
    icvv = icvv << 1; /* Round to nearest even times ScoreScale */
    icvv |= igbit;    /* Restore original ignore bit */
    if (!igbit) {
        oldicvv -= icvv;
        if (oldicvv < 0)
            oldicvv = -oldicvv;
        if (oldicvv > SigScoreChange)
            CurClass->score_change_count++;
    }
    CurClass->cvv = cvv = icvv * ScoreRscale;
    CurClass->cvvsq = cvvsq = cvv * cvv;
    CurClass->cvvsprd = cvvsprd;
done:
    vv[CurItem] = CurClass->case_score = icvv;
    return;
}

/*    ----------------------  costvarall  --------------------------  */
/*    Does costvar on all vars of class for the current item, setting
cls->casecost according to use of class  */
void costvarall(Class *ccl) {
    int fac;
    double tmp;

    setclass2(ccl);
    if ((CurClass->age < MinFacAge) || (CurClass->use == Tiny))
        fac = 0;
    else {
        fac = 1;
        cvvsq = cvv * cvv;
    }
    ncasecost = scasecost = fcasecost = CurClass->mlogab; /* Abundance cost */
    for (int iv = 0; iv < CurVSet->num_vars; iv++) {
        avi = CurAttrs + iv;
        if (avi->inactive)
            goto vdone;
        vtp = avi->vtype;
        (*vtp->costvar)(iv, fac);
    /*    will add to scasecost, fcasecost  */
    vdone:;
    }

    CurClass->total_case_cost = CurClass->nofac_case_cost = scasecost;
    CurClass->fac_case_cost = scasecost + 10.0;
    if (CurClass->num_sons < 2)
        CurClass->dad_case_cost = 0.0;
    CurClass->coding_case_cost = 0.0;
    if (!fac)
        goto finish;
    /*    Now we add the cost of coding the score, and its roundoff */
    /*    The simple form for costing a score is :
        tmp = hlg2pi + 0.5 * (cvvsq + cvvsprd - log (cvvsprd)) + lattice;
    However, we appeal to the large number of score parameters, which gives a
    more negative 'lattice' ((log 12)/2 for one parameter) approaching -(1/2)
    log (2 Pi e) which results in the reduced cost :  */
    CurClass->clvsprd = log(cvvsprd);
    tmp = 0.5 * (cvvsq + cvvsprd - CurClass->clvsprd - 1.0);
    /*    Over all scores for the class, the lattice effect will add approx
            ( log (2 Pi cnt)) / 2  + 1
        to the class cost. This is added later, once cnt is known.
        */
    fcasecost += tmp;
    CurClass->fac_case_cost = fcasecost;
    CurClass->coding_case_cost = tmp;
    if (CurClass->use == Fac)
        CurClass->total_case_cost = fcasecost;
finish:
    return;
}

/*    ----------------------  derivvarall  ---------------------    */
/*    To collect derivative statistics for all vars of a class   */
void derivvarall(Class *ccl) {
    int fac;

    setclass2(ccl);
    CurClass->newcnt += cwt;
    if ((CurClass->age < MinFacAge) || (CurClass->use == Tiny))
        fac = 0;
    else {
        fac = 1;
        cvv = CurClass->cvv;
        cvvsq = CurClass->cvvsq;
        cvvsprd = CurClass->cvvsprd;
        CurClass->newvsq += cwt * cvvsq;
        CurClass->vav += cwt * CurClass->clvsprd;
        CurClass->totvv += cvv * cwt;
    }
    for (int iv = 0; iv < nv; iv++) {
        avi = CurAttrs + iv;
        if (avi->inactive)
            goto vdone;
        vtp = avi->vtype;
        (*vtp->derivvar)(iv, fac);
    vdone:;
    }

    /*    Collect case item costs   */
    CurClass->cstcost += cwt * CurClass->nofac_case_cost;
    CurClass->cftcost += cwt * CurClass->fac_case_cost;
    CurClass->cntcost += cwt * CurClass->dad_case_cost;
    CurClass->cfvcost += cwt * CurClass->coding_case_cost;

    return;
}

/*    --------------------  adjustclass  -----------------------   */
/*    To compute pcosts of a class, and if needed, adjust params  */
/*    Will process as-dad params only if 'dod'  */
void adjustclass(Class *ccl, int dod) {
    int iv, fac, npars, small;
    Class *son;
    double leafcost;

    setclass1(ccl);
    /*    Get root (logarithmic average of vvsprds)  */
    CurClass->vav = exp(0.5 * CurClass->vav / (CurClass->newcnt + 0.1));
    if (Control & AdjSc)
        CurClass->sum_score_sq = CurClass->newvsq;
    if (Control & AdjPr) {
        CurClass->weights_sum = CurClass->newcnt;
        /*    Count down holds   */
        if ((CurClass->hold_type) && (CurClass->hold_type < Forever))
            CurClass->hold_type--;
        if ((CurClass->hold_use) && (CurClass->hold_use < Forever))
            CurClass->hold_use--;
        if (CurDad) {
            CurClass->relab = (CurDad->relab * (CurClass->weights_sum + 0.5)) / (CurDad->weights_sum + 0.5 * CurDad->num_sons);
            CurClass->mlogab = -log(CurClass->relab);
        } else {
            CurClass->relab = 1.0;
            CurClass->mlogab = 0.0;
        }
    }
    /*    But if a young subclass, make relab half of dad's  */
    if ((CurClass->type == Sub) && (CurClass->age < MinSubAge)) {
        CurClass->relab = 0.5 * CurDad->relab;
    }

    if ((CurClass->age < MinFacAge) || (CurClass->use == Tiny))
        fac = 0;
    else
        fac = 1;

    /*    Set npars to show if class may be treated as a dad  */
    npars = 1;
    if ((CurClass->age < MinAge) || (CurClass->num_sons < 2) || (CurPopln->classes[CurClass->son_id]->age < MinSubAge))
        npars = 0;
    if (CurClass->type == Dad)
        npars = 1;

    /*    cls->cnpcost was zeroed in doall, and has accumulated the cbpcosts
    of cls's sons, so we don't zero it here. 'ncostvarall' will add to it.  */
    CurClass->nofac_par_cost = CurClass->fac_par_cost = 0.0;
    for (iv = 0; iv < CurVSet->num_vars; iv++) {
        avi = CurAttrs + iv;
        if (avi->inactive)
            goto vdone;
        vtp = avi->vtype;
        (*vtp->adjust)(iv, fac);
    vdone:;
    }

    /*    If vsq less than 0.3, set vboost to inflate  */
    /*    but only if both scores and params are being adjusted  */
    if (((Control & AdjSP) == AdjSP) && fac && (CurClass->sum_score_sq < (0.3 * CurClass->weights_sum))) {
        CurClass->score_boost = sqrt((1.0 * CurClass->weights_sum) / (CurClass->sum_score_sq + 1.0));
        CurClass->boost_count++;
    } else {
        CurClass->score_boost = 1.0;
        CurClass->boost_count = 0;
    }

    /*    Get average score   */
    CurClass->avvv = CurClass->totvv / CurClass->weights_sum;

    if (dod)
        ncostvarall(ccl, npars);

    CurClass->nofac_cost = CurClass->nofac_par_cost + CurClass->cstcost;
    /*    The 'lattice' effect on the cost of coding scores is approx
        (log (2 Pi cnt))/2 + 1,  which adds to cftcost  */
    CurClass->cftcost += 0.5 * log(CurClass->newcnt + 1.0) + hlg2pi + 1.0;
    CurClass->fac_cost = CurClass->fac_par_cost + CurClass->cftcost;
    if (npars)
        CurClass->dad_cost = CurClass->dad_par_cost + CurClass->cntcost;
    else
        CurClass->dad_cost = CurClass->dad_par_cost = CurClass->cntcost = 0.0;

    /*    Contemplate changes to class use and type   */
    if (CurClass->hold_use || (!(Control & AdjPr)))
        goto usechecked;
    /*    If scores boosted too many times, make Tiny and hold   */
    if (CurClass->boost_count > 20) {
        CurClass->use = Tiny;
        CurClass->hold_use = 10;
        goto usechecked;
    }
    /*    Check if size too small to support a factor  */
    /*    Add up the number of data values  */
    small = 0;
    leafcost = 0.0; /* Used to add up evi->cnts */
    for (iv = 0; iv < nv; iv++) {
        if (CurAttrs[iv].inactive)
            goto vidle;
        small++;
        leafcost += CurClass->stats[iv]->num_values;
    vidle:;
    }
    /*    I want at least 1.5 data vals per param  */
    small = (leafcost < (9 * small + 1.5 * CurClass->weights_sum + 1)) ? 1 : 0;
    if (small) {
        if (CurClass->use != Tiny) {
            CurClass->use = Tiny;
        }
        goto usechecked;
    } else {
        if (CurClass->use == Tiny) {
            CurClass->use = Plain;
            CurClass->sum_score_sq = 0.0;
            goto usechecked;
        }
    }
    if (CurClass->age < MinFacAge)
        goto usechecked;
    if (CurClass->use == Plain) {
        if (CurClass->fac_cost < CurClass->nofac_cost) {
            CurClass->use = Fac;
            CurClass->boost_count = 0;
            CurClass->score_boost = 1.0;
        }
    } else {
        if (CurClass->nofac_cost < CurClass->fac_cost) {
            CurClass->use = Plain;
        }
    }
usechecked:
    if (!dod)
        goto typechecked;
    if (CurClass->hold_type)
        goto typechecked;
    if (!(Control & AdjTr))
        goto typechecked;
    if (CurClass->num_sons < 2)
        goto typechecked;
    leafcost = (CurClass->use == Fac) ? CurClass->fac_cost : CurClass->nofac_cost;

    if ((CurClass->type == Dad) && (leafcost < CurClass->dad_cost) && (Fix != Random)) {
        flp();
        printf("Changing type of class%s from Dad to Leaf\n", sers(CurClass));
        SeeAll = 4;
        /*    Kill all sons  */
        killsons(CurClass->id); /* which changes type to leaf */
    } else if (npars && (leafcost > CurClass->dad_cost) && (CurClass->type == Leaf)) {
        flp();
        printf("Changing type of class%s from Leaf to Dad\n", sers(CurClass));
        SeeAll = 4;
        CurClass->type = Dad;
        if (CurDad)
            CurDad->hold_type += 3;
        /*    Make subs into leafs  */
        son = CurPopln->classes[CurClass->son_id];
        son->type = Leaf;
        son->serial = 4 * CurPopln->next_serial;
        CurPopln->next_serial++;
        son = CurPopln->classes[son->sib_id];
        son->type = Leaf;
        son->serial = 4 * CurPopln->next_serial;
        CurPopln->next_serial++;
    }

typechecked:
    setbestparall(CurClass);
    if (Control & AdjPr)
        CurClass->age++;
    return;
}

/*    ----------------  ncostvarall  ------------------------------   */
/*    To do parent costing on all vars of a class        */
/*    If not 'valid', don't cost, and fake params  */
void ncostvarall(Class *ccl, int valid) {
    Class *son;
    EVinst *evi;
    int i, ison, nson;
    double abcost, rrelab;

    setclass1(ccl);
    abcost = 0.0;
    for (i = 0; i < CurVSet->num_vars; i++) {
        avi = CurAttrs + i;
        vtp = avi->vtype;
        (*vtp->ncostvar)(i, valid);
        evi = (EVinst *)CurClass->stats[i];
        if (!avi->inactive) {
            abcost += evi->npcost;
        }
    }

    if (!valid) {
        CurClass->dad_par_cost = CurClass->dad_cost = 0.0;
        return;
    }
    nson = CurClass->num_sons;
    /*    The sons of a dad may be listed in any order, so the param cost
    of the dad can be reduced by log (nson !)  */
    abcost -= FacLog[nson];
    /*    The cost of saying 'dad' and number of sons is set at nson bits. */
    abcost += nson * bit;
    /*    Now add cost of specifying the relabs of the sons.  */
    /*    Their relabs are absolute, but we specify them as fractions of this
    dad's relab. The cost includes -0.5 * Sum_sons { log (sonab / dadab) }
        */
    rrelab = 1.0 / CurClass->relab;
    for (ison = CurClass->son_id; ison >= 0; ison = son->sib_id) {
        son = CurPopln->classes[ison];
        abcost -= 0.5 * log(son->relab * rrelab);
    }
    /*    Add other terms from Fisher  */
    abcost += (nson - 1) * (log(CurClass->weights_sum) + lattice);
    /*    And from prior:  */
    abcost -= FacLog[nson - 1];
    /*    The sons will have been processed by 'adjustclass' already, and
    this will have caused their best pcosts to be added into cls->cnpcost  */
    CurClass->dad_par_cost += abcost;
    return;
}

/*    --------------------  killsons ------------------------------- */
/*    To delete the sons of a class  */
void killsons(int kk) {
    Class *clp, *son;
    int kks;

    if (!(Control & AdjTr))
        return;
    clp = CurPopln->classes[kk];
    if (clp->num_sons <= 0)
        return;
    SeeAll = 4;
    for (kks = clp->son_id; kks > 0; kks = son->sib_id) {
        son = CurPopln->classes[kks];
        son->type = Vacant;
        son->dad_id = Deadkilled;
        killsons(kks);
    }
    clp->num_sons = 0;
    clp->son_id = -1;
    if (clp->type == Dad)
        clp->type = Leaf;
    clp->hold_type = 0;
    return;
}

/*    --------------------  splitleaf ------------------------   */
/*    If kk is a leaf and has subs, turns it into a dad and makes
its subs into leaves.  Returns 0 if successful.  */
int splitleaf(int kk) {
    Class *son;
    int kks;
    CurClass = CurPopln->classes[kk];
    if ((CurClass->type != Leaf) || (CurClass->num_sons < 2) || CurClass->hold_type) {
        return (1);
    }
    CurClass->type = Dad;
    CurClass->hold_type = HoldTime;
    for (kks = CurClass->son_id; kks >= 0; kks = son->sib_id) {
        son = CurPopln->classes[kks];
        son->type = Leaf;
        son->serial = 4 * CurPopln->next_serial;
        CurPopln->next_serial++;
    }
    SeeAll = 4;
    return (0);
}

/*    ---------------  deleteallclasses  ------------ */
/*    To remove all classes of a popln except its root  */
void deleteallclasses() {
    int k;
    CurPopln = CurCtx.popln;
    CurRoot = CurPopln->root;
    for (k = 0; k <= CurPopln->hi_class; k++) {
        CurClass = CurPopln->classes[k];
        if (CurClass->id != CurRoot) {
            CurClass->type = Vacant;
            CurClass->dad_id = Deadkilled;
        }
    }
    CurPopln->num_classes = 1;
    CurPopln->hi_class = CurRoot;
    CurRootClass = CurClass = CurPopln->classes[CurRoot];
    CurClass->son_id = -1;
    CurClass->sib_id = -1;
    CurClass->num_sons = 0;
    CurClass->serial = 4;
    CurPopln->next_serial = 2;
    CurClass->age = 0;
    CurClass->hold_type = CurClass->hold_use = 0;
    CurClass->type = Leaf;
    NoSubs++;
    tidy(1);
    if (NoSubs > 0)
        NoSubs--;
    return;
}

/*    ------------------  nextleaf  -------------------------   */
/*    given a ptr cpop to a popln and an integer iss, returns the index
of the next leaf in the popln with serial > iss, or -1 if there is none */
/*    Intended for scanning the leaves of a popln in serial order  */
int nextleaf(Population *cpop, int iss) {
    Class *clp;
    int i, j, k, n;

    n = -1;
    k = 10000000;
    for (i = 0; i <= cpop->hi_class; i++) {
        clp = cpop->classes[i];
        if (clp->type != Leaf)
            goto idone;
        j = clp->serial;
        if (j <= iss)
            goto idone;
        if (j < k) {
            k = j;
            n = i;
        }
    idone:;
    }
    return (n);
}
/*zzzz*/
