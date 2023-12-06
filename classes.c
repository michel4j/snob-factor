/*	Stuff dealing with classes as members of polpns     */
#define CLASSES 1
#include "glob.h"

/*	-----------------------  s2id  ---------------------------   */
/*	Finds class index (id) from serial, or -3 if error    */
int s2id(int ss) {
    int k;
    Class *clp;
    setpop();

    if (ss >= 1) {
        for (k = 0; k <= population->hicl; k++) {
            clp = population->classes[k];
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

/*	---------------------  setclass1 --------------------------   */
void setclass1(Class *ccl) {
    cls = ccl;
    idad = cls->idad;
    if (idad >= 0)
        dad = population->classes[idad];
    else
        dad = 0;
    return;
}

/*	---------------------  setclass2 --------------------------   */
void setclass2(Class *ccl) {
    cls = ccl;
    vv = cls->vv;
    cls->caseiv = icvv = vv[item];
    cwt = cls->casewt;
    idad = cls->idad;
    if (idad >= 0)
        dad = population->classes[idad];
    else
        dad = 0;
    return;
}

/*	---------------------   makeclass  -------------------------   */
/*	Makes the basic structure of a class (Class) with vector of
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

    nc = ctx.popln->nc;
    /*	If nc, check that popln has an attached sample  */
    if (nc) {
        i = sname2id(population->samplename, 1);
        if (i < 0)
            return (-2);
        ctx.sample = samp = samples[i];
    }

    nv = vst->length;
    pvars = population->pvars;
    /*	Find a vacant slot in the popln's classes vec   */
    for (kk = 0; kk < population->mncl; kk++) {
        if (!population->classes[kk])
            goto gotit;
        /*	Also test for a vacated class   */
        if (population->classes[kk]->type == Vacant)
            goto vacant1;
    }
    printf("Popln full of classes\n");
    goto nospace;

gotit:
    cls = (Class *)gtsp(1, sizeof(Class));
    if (!cls)
        goto nospace;
    population->classes[kk] = cls;
    population->hicl = kk; /* Highest used index in population->classes */
    cls->id = kk;

    /*	Make vector of ptrs to CVinsts   */
    cvars = cls->basics = (CVinst **)gtsp(1, nv * sizeof(CVinst *));
    if (!cvars)
        goto nospace;

    /*	Now make the CVinst blocks, which are of varying size   */
    for (i = 0; i < nv; i++) {
        pvi = pvars + i;
        avi = vlist + i;
        cvi = cvars[i] = (CVinst *)gtsp(1, avi->basic_size);
        if (!cvi)
            goto nospace;
        /*	Fill in standard stuff  */
        cvi->id = avi->id;
    }

    /*	Make EVinst blocks and vector of pointers  */
    evars = cls->stats = (EVinst **)gtsp(1, nv * sizeof(EVinst *));
    if (!evars)
        goto nospace;

    for (i = 0; i < nv; i++) {
        pvi = pvars + i;
        avi = vlist + i;
        evi = evars[i] = (EVinst *)gtsp(1, avi->stats_size);
        if (!evi)
            goto nospace;
        evi->id = pvi->id;
    }

    /*	Stomp on ptrs as yet undefined  */
    cls->vv = 0;
    cls->type = 0;
    goto donebasic;

vacant1: /* Vacant type shows structure set up but vacated.
     Use, but set new (Vacant) type,  */
    cls = population->classes[kk];
    cvars = cls->basics;
    evars = cls->stats;
    cls->type = Vacant;

donebasic:
    cls->cbcost = cls->cscost = cls->cfcost = 0.0;
    cls->cnt = 0.0;
    /*	Invalidate hierarchy links  */
    cls->idad = cls->isib = cls->ison = -1;
    cls->nson = 0;
    for (i = 0; i < nv; i++)
        cvars[i]->signif = 1;
    population->ncl++;
    if (kk > population->hicl)
        population->hicl = kk;
    if (cls->type == Vacant)
        goto vacant2;

    /*	If nc = 0, this completes the make.  */
    if (nc == 0)
        goto finish;
    cls->vv = (short *)gtsp(2, nc * sizeof(short));
    goto expanded;

vacant2:
    cls->type = 0;

expanded:
    for (i = 0; i < nv; i++)
        evars[i]->num_values = 0.0;
finish:
    cls->age = 0;
    cls->holdtype = cls->holduse = 0;
    cls->cnt = 0.0;
    return (kk);

nospace:
    printf("No space for new class\n");
    return (-1);
}

/*	-----------------------  printclass  -----------------------  */
/*	To print parameters of a class    */
/*	If kk = -1, prints all non-subs. If kk = -2, prints all  */
char *typstr[] = {"typ?", " DAD", "LEAF", "typ?", " Sub"};
char *usestr[] = {"Tny", "Pln", "Fac"};
void print1class(Class *clp, int full) {
    int i;
    double vrms;

    printf("\nS%s", sers(clp));
    printf(" %s", typstr[((int)clp->type)]);
    if (clp->idad < 0)
        printf("    Root");
    else
        printf(" Dad%s", sers(population->classes[((int)clp->idad)]));
    printf(" Age%4d  Sz%6.1f  Use %s", clp->age, clp->cnt, usestr[((int)clp->use)]);
    printf("%c", (clp->use == Fac) ? ' ' : '(');
    vrms = sqrt(clp->vsq / clp->cnt);
    printf("Vv%6.2f", vrms);
    printf("%c", (cls->boostcnt) ? 'B' : ' ');
    printf(" +-%5.3f", (clp->vav));
    printf(" Avv%6.3f", clp->avvv);
    printf("%c", (clp->use == Fac) ? ' ' : ')');
    if (clp->type == Dad) {
        printf("%4d sons", clp->nson);
    }
    printf("\n");
    printf("Pcosts  S:%9.2f  F:%9.2f  D:%9.2f  B:%9.2f\n", clp->cspcost, clp->cfpcost, clp->cnpcost, clp->cbpcost);
    printf("Tcosts  S:%9.2f  F:%9.2f  D:%9.2f  B:%9.2f\n", clp->cstcost, clp->cftcost - cls->cfvcost, clp->cntcost, clp->cbtcost);
    printf("Vcost     ---------  F:%9.2f\n", clp->cfvcost);
    printf("totals  S:%9.2f  F:%9.2f  D:%9.2f  B:%9.2f\n", clp->cscost, clp->cfcost, clp->cncost, clp->cbcost);
    if (!full)
        return;
    for (i = 0; i < vst->length; i++) {
        vtp = vlist[i].vtype;
        (*vtp->show)(clp, i);
    }
    return;
}

void printclass(int kk, int full) {
    Class *clp;
    if (kk >= 0) {
        clp = population->classes[kk];
        print1class(clp, full);
        return;
    }
    if (kk < -2) {
        printf("%d passed to printclass\n", kk);
        return;
    }
    setpop();
    clp = rootcl;

    do {
        if ((kk == -2) || (clp->type != Sub))
            print1class(clp, full);
        nextclass(&clp);
    } while (clp);

    return;
}

/*	-------------------------  cleartcosts  ------  */
/*	Clears tcosts (cntcost, cftcost, cstcost). Also clears newcnt,
newvsq, and calls clearstats for all variables   */
void cleartcosts(Class *ccl) {
    int i;

    setclass1(ccl);
    cls->mlogab = -log(cls->relab);
    cls->cstcost = cls->cftcost = cls->cntcost = 0.0;
    cls->cfvcost = 0.0;
    cls->newcnt = cls->newvsq = 0.0;
    cls->scorechange = 0;
    cls->vav = 0.0;
    cls->totvv = 0.0;
    if (!seeall)
        cls->scancnt = 0;
    for (i = 0; i < vst->length; i++) {
        vtp = vlist[i].vtype;
        (*vtp->clear_stats)(i);
    }
    return;
}

/*	-------------------  setbestparall  ------------------------   */
/*	To set 'b' params, costs to reflect current use   */
void setbestparall(Class *ccl) {
    int i;

    setclass1(ccl);
    if (cls->type == Dad) {
        cls->cbcost = cls->cncost;
        cls->cbpcost = cls->cnpcost;
        cls->cbtcost = cls->cntcost;
    } else if (cls->use != Fac) {
        cls->cbcost = cls->cscost;
        cls->cbpcost = cls->cspcost;
        cls->cbtcost = cls->cstcost;
    } else {
        cls->cbcost = cls->cfcost;
        cls->cbpcost = cls->cfpcost;
        cls->cbtcost = cls->cftcost;
    }
    for (i = 0; i < vst->length; i++) {
        vtp = vlist[i].vtype;
        (*vtp->set_best_pars)(i);
    }
    return;
}

/*	--------------------  scorevarall  --------------------    */
/*	To evaluate derivs of cost wrt score in all vars of a class.
    Does one item, number case  */
/*	Leaves data values set in stats, but does no scoring if class too
young. If class age = MinFacAge, will guess scores but not cost them */
/*	If control & AdjSc, will adjust score  */
void scorevarall(Class *ccl) {
    int i, igbit, oldicvv;
    double del;

    setclass2(ccl);
    if ((cls->age < MinFacAge) || (cls->use == Tiny)) {
        cvv = cls->avvv = cls->vsq = 0.0;
        icvv = 0;
        goto done;
    }
    if (cls->vsq > 0.0)
        goto started;
    /*	Generate a fake score to get started.   */
    cls->boostcnt = 0;
    cvvsprd = 0.1 / nv;
    oldicvv = igbit = 0;
    cvv = (sran() < 0) ? 1.0 : -1.0;
    goto fake;

started:
    /*	Get current score  */
    oldicvv = icvv;
    igbit = icvv & 1;
    cvv = icvv * ScoreRscale;
    /*	Subtract average from last pass  */
    /*xx
        cvv -= cls->avvv;
    */
    if (cls->boostcnt && ((control & AdjSP) == AdjSP)) {
        cvv *= cls->vboost;
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
    for (i = 0; i < vst->length; i++) {
        avi = vlist + i;
        if (avi->inactive)
            goto vdone;
        vtp = avi->vtype;
        (*vtp->score_var)(i);
    /*	scorevar should add to vvd1, vvd2, vvd3, mvvd2.  */
    vdone:;
    }
    vvd1 += cvv;
    vvd2 += 1.0;
    mvvd2 += 1.0; /*  From prior  */
    /*	There is a cost term 0.5 * cvvsprd from the prior (whence the additional
        1 in vvd2).  */
    cvvsprd = 1.0 / vvd2;
    /*	Also, overall cost includes 0.5*cvvsprd*vvd2, so there is a derivative
    term wrt cvv of 0.5*cvvsprd*vvd3  */
    vvd1 += 0.5 * cvvsprd * vvd3;
    del = vvd1 / mvvd2;
    if (control & AdjSc) {
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
            cls->scorechange++;
    }
    cls->cvv = cvv = icvv * ScoreRscale;
    cls->cvvsq = cvvsq = cvv * cvv;
    cls->cvvsprd = cvvsprd;
done:
    vv[item] = cls->caseiv = icvv;
    return;
}

/*	----------------------  costvarall  --------------------------  */
/*	Does costvar on all vars of class for the current item, setting
cls->casecost according to use of class  */
void costvarall(Class *ccl) {
    int fac;
    double tmp;

    setclass2(ccl);
    if ((cls->age < MinFacAge) || (cls->use == Tiny))
        fac = 0;
    else {
        fac = 1;
        cvvsq = cvv * cvv;
    }
    ncasecost = scasecost = fcasecost = cls->mlogab; /* Abundance cost */
    for (int iv = 0; iv < vst->length; iv++) {
        avi = vlist + iv;
        if (avi->inactive)
            goto vdone;
        vtp = avi->vtype;
        (*vtp->cost_var)(iv, fac);
    /*	will add to scasecost, fcasecost  */
    vdone:;
    }

    cls->casecost = cls->casescost = scasecost;
    cls->casefcost = scasecost + 10.0;
    if (cls->nson < 2)
        cls->casencost = 0.0;
    cls->casevcost = 0.0;
    if (!fac)
        goto finish;
    /*	Now we add the cost of coding the score, and its roundoff */
    /*	The simple form for costing a score is :
        tmp = hlg2pi + 0.5 * (cvvsq + cvvsprd - log (cvvsprd)) + lattice;
    However, we appeal to the large number of score parameters, which gives a
    more negative 'lattice' ((log 12)/2 for one parameter) approaching -(1/2)
    log (2 Pi e) which results in the reduced cost :  */
    cls->clvsprd = log(cvvsprd);
    tmp = 0.5 * (cvvsq + cvvsprd - cls->clvsprd - 1.0);
    /*	Over all scores for the class, the lattice effect will add approx
            ( log (2 Pi cnt)) / 2  + 1
        to the class cost. This is added later, once cnt is known.
        */
    fcasecost += tmp;
    cls->casefcost = fcasecost;
    cls->casevcost = tmp;
    if (cls->use == Fac)
        cls->casecost = fcasecost;
finish:
    return;
}

/*	----------------------  derivvarall  ---------------------    */
/*	To collect derivative statistics for all vars of a class   */
void derivvarall(Class *ccl) {
    int fac;

    setclass2(ccl);
    cls->newcnt += cwt;
    if ((cls->age < MinFacAge) || (cls->use == Tiny))
        fac = 0;
    else {
        fac = 1;
        cvv = cls->cvv;
        cvvsq = cls->cvvsq;
        cvvsprd = cls->cvvsprd;
        cls->newvsq += cwt * cvvsq;
        cls->vav += cwt * cls->clvsprd;
        cls->totvv += cvv * cwt;
    }
    for (int iv = 0; iv < nv; iv++) {
        avi = vlist + iv;
        if (avi->inactive)
            goto vdone;
        vtp = avi->vtype;
        (*vtp->deriv_var)(iv, fac);
    vdone:;
    }

    /*	Collect case item costs   */
    cls->cstcost += cwt * cls->casescost;
    cls->cftcost += cwt * cls->casefcost;
    cls->cntcost += cwt * cls->casencost;
    cls->cfvcost += cwt * cls->casevcost;

    return;
}

/*	--------------------  adjustclass  -----------------------   */
/*	To compute pcosts of a class, and if needed, adjust params  */
/*	Will process as-dad params only if 'dod'  */
void adjustclass(Class *ccl, int dod) {
    int iv, fac, npars, small;
    Class *son;
    double leafcost;

    setclass1(ccl);
    /*	Get root (logarithmic average of vvsprds)  */
    cls->vav = exp(0.5 * cls->vav / (cls->newcnt + 0.1));
    if (control & AdjSc)
        cls->vsq = cls->newvsq;
    if (control & AdjPr) {
        cls->cnt = cls->newcnt;
        /*	Count down holds   */
        if ((cls->holdtype) && (cls->holdtype < Forever))
            cls->holdtype--;
        if ((cls->holduse) && (cls->holduse < Forever))
            cls->holduse--;
        if (dad) {
            cls->relab = (dad->relab * (cls->cnt + 0.5)) / (dad->cnt + 0.5 * dad->nson);
            cls->mlogab = -log(cls->relab);
        } else {
            cls->relab = 1.0;
            cls->mlogab = 0.0;
        }
    }
    /*	But if a young subclass, make relab half of dad's  */
    if ((cls->type == Sub) && (cls->age < MinSubAge)) {
        cls->relab = 0.5 * dad->relab;
    }

    if ((cls->age < MinFacAge) || (cls->use == Tiny))
        fac = 0;
    else
        fac = 1;

    /*	Set npars to show if class may be treated as a dad  */
    npars = 1;
    if ((cls->age < MinAge) || (cls->nson < 2) || (population->classes[cls->ison]->age < MinSubAge))
        npars = 0;
    if (cls->type == Dad)
        npars = 1;

    /*	cls->cnpcost was zeroed in doall, and has accumulated the cbpcosts
    of cls's sons, so we don't zero it here. 'ncostvarall' will add to it.  */
    cls->cspcost = cls->cfpcost = 0.0;
    for (iv = 0; iv < vst->length; iv++) {
        avi = vlist + iv;
        if (avi->inactive)
            goto vdone;
        vtp = avi->vtype;
        (*vtp->adjust)(iv, fac);
    vdone:;
    }

    /*	If vsq less than 0.3, set vboost to inflate  */
    /*	but only if both scores and params are being adjusted  */
    if (((control & AdjSP) == AdjSP) && fac && (cls->vsq < (0.3 * cls->cnt))) {
        cls->vboost = sqrt((1.0 * cls->cnt) / (cls->vsq + 1.0));
        cls->boostcnt++;
    } else {
        cls->vboost = 1.0;
        cls->boostcnt = 0;
    }

    /*	Get average score   */
    cls->avvv = cls->totvv / cls->cnt;

    if (dod)
        ncostvarall(ccl, npars);

    cls->cscost = cls->cspcost + cls->cstcost;
    /*	The 'lattice' effect on the cost of coding scores is approx
        (log (2 Pi cnt))/2 + 1,  which adds to cftcost  */
    cls->cftcost += 0.5 * log(cls->newcnt + 1.0) + hlg2pi + 1.0;
    cls->cfcost = cls->cfpcost + cls->cftcost;
    if (npars)
        cls->cncost = cls->cnpcost + cls->cntcost;
    else
        cls->cncost = cls->cnpcost = cls->cntcost = 0.0;

    /*	Contemplate changes to class use and type   */
    if (cls->holduse || (!(control & AdjPr)))
        goto usechecked;
    /*	If scores boosted too many times, make Tiny and hold   */
    if (cls->boostcnt > 20) {
        cls->use = Tiny;
        cls->holduse = 10;
        goto usechecked;
    }
    /*	Check if size too small to support a factor  */
    /*	Add up the number of data values  */
    small = 0;
    leafcost = 0.0; /* Used to add up evi->cnts */
    for (iv = 0; iv < nv; iv++) {
        if (vlist[iv].inactive)
            goto vidle;
        small++;
        leafcost += cls->stats[iv]->num_values;
    vidle:;
    }
    /*	I want at least 1.5 data vals per param  */
    small = (leafcost < (9 * small + 1.5 * cls->cnt + 1)) ? 1 : 0;
    if (small) {
        if (cls->use != Tiny) {
            cls->use = Tiny;
        }
        goto usechecked;
    } else {
        if (cls->use == Tiny) {
            cls->use = Plain;
            cls->vsq = 0.0;
            goto usechecked;
        }
    }
    if (cls->age < MinFacAge)
        goto usechecked;
    if (cls->use == Plain) {
        if (cls->cfcost < cls->cscost) {
            cls->use = Fac;
            cls->boostcnt = 0;
            cls->vboost = 1.0;
        }
    } else {
        if (cls->cscost < cls->cfcost) {
            cls->use = Plain;
        }
    }
usechecked:
    if (!dod)
        goto typechecked;
    if (cls->holdtype)
        goto typechecked;
    if (!(control & AdjTr))
        goto typechecked;
    if (cls->nson < 2)
        goto typechecked;
    leafcost = (cls->use == Fac) ? cls->cfcost : cls->cscost;

    if ((cls->type == Dad) && (leafcost < cls->cncost) && (fix != Random)) {
        flp();
        printf("Changing type of class%s from Dad to Leaf\n", sers(cls));
        seeall = 4;
        /*	Kill all sons  */
        killsons(cls->id); /* which changes type to leaf */
    } else if (npars && (leafcost > cls->cncost) && (cls->type == Leaf)) {
        flp();
        printf("Changing type of class%s from Leaf to Dad\n", sers(cls));
        seeall = 4;
        cls->type = Dad;
        if (dad)
            dad->holdtype += 3;
        /*	Make subs into leafs  */
        son = population->classes[cls->ison];
        son->type = Leaf;
        son->serial = 4 * population->nextserial;
        population->nextserial++;
        son = population->classes[son->isib];
        son->type = Leaf;
        son->serial = 4 * population->nextserial;
        population->nextserial++;
    }

typechecked:
    setbestparall(cls);
    if (control & AdjPr)
        cls->age++;
    return;
}

/*	----------------  ncostvarall  ------------------------------   */
/*	To do parent costing on all vars of a class		*/
/*	If not 'valid', don't cost, and fake params  */
void ncostvarall(Class *ccl, int valid) {
    Class *son;
    EVinst *evi;
    int i, ison, nson;
    double abcost, rrelab;

    setclass1(ccl);
    abcost = 0.0;
    for (i = 0; i < vst->length; i++) {
        avi = vlist + i;
        vtp = avi->vtype;
        (*vtp->cost_var_nonleaf)(i, valid);
        evi = (EVinst *)cls->stats[i];
        if (!avi->inactive) {
            abcost += evi->npcost;
        }
    }

    if (!valid) {
        cls->cnpcost = cls->cncost = 0.0;
        return;
    }
    nson = cls->nson;
    /*	The sons of a dad may be listed in any order, so the param cost
    of the dad can be reduced by log (nson !)  */
    abcost -= faclog[nson];
    /*	The cost of saying 'dad' and number of sons is set at nson bits. */
    abcost += nson * bit;
    /*	Now add cost of specifying the relabs of the sons.  */
    /*	Their relabs are absolute, but we specify them as fractions of this
    dad's relab. The cost includes -0.5 * Sum_sons { log (sonab / dadab) }
        */
    rrelab = 1.0 / cls->relab;
    for (ison = cls->ison; ison >= 0; ison = son->isib) {
        son = population->classes[ison];
        abcost -= 0.5 * log(son->relab * rrelab);
    }
    /*	Add other terms from Fisher  */
    abcost += (nson - 1) * (log(cls->cnt) + lattice);
    /*	And from prior:  */
    abcost -= faclog[nson - 1];
    /*	The sons will have been processed by 'adjustclass' already, and
    this will have caused their best pcosts to be added into cls->cnpcost  */
    cls->cnpcost += abcost;
    return;
}

/*	--------------------  killsons ------------------------------- */
/*	To delete the sons of a class  */
void killsons(int kk) {
    Class *clp, *son;
    int kks;

    if (!(control & AdjTr))
        return;
    clp = population->classes[kk];
    if (clp->nson <= 0)
        return;
    seeall = 4;
    for (kks = clp->ison; kks > 0; kks = son->isib) {
        son = population->classes[kks];
        son->type = Vacant;
        son->idad = Deadkilled;
        killsons(kks);
    }
    clp->nson = 0;
    clp->ison = -1;
    if (clp->type == Dad)
        clp->type = Leaf;
    clp->holdtype = 0;
    return;
}

/*	--------------------  splitleaf ------------------------   */
/*	If kk is a leaf and has subs, turns it into a dad and makes
its subs into leaves.  Returns 0 if successful.  */
int splitleaf(int kk) {
    Class *son;
    int kks;
    cls = population->classes[kk];
    if ((cls->type != Leaf) || (cls->nson < 2) || cls->holdtype) {
        return (1);
    }
    cls->type = Dad;
    cls->holdtype = HoldTime;
    for (kks = cls->ison; kks >= 0; kks = son->isib) {
        son = population->classes[kks];
        son->type = Leaf;
        son->serial = 4 * population->nextserial;
        population->nextserial++;
    }
    seeall = 4;
    return (0);
}

/*	---------------  deleteallclasses  ------------ */
/*	To remove all classes of a popln except its root  */
void deleteallclasses() {
    int k;
    population = ctx.popln;
    root = population->root;
    for (k = 0; k <= population->hicl; k++) {
        cls = population->classes[k];
        if (cls->id != root) {
            cls->type = Vacant;
            cls->idad = Deadkilled;
        }
    }
    population->ncl = 1;
    population->hicl = root;
    rootcl = cls = population->classes[root];
    cls->ison = -1;
    cls->isib = -1;
    cls->nson = 0;
    cls->serial = 4;
    population->nextserial = 2;
    cls->age = 0;
    cls->holdtype = cls->holduse = 0;
    cls->type = Leaf;
    nosubs++;
    tidy(1);
    if (nosubs > 0)
        nosubs--;
    return;
}

/*	------------------  nextleaf  -------------------------   */
/*	given a ptr cpop to a popln and an integer iss, returns the index
of the next leaf in the popln with serial > iss, or -1 if there is none */
/*	Intended for scanning the leaves of a popln in serial order  */
int nextleaf(Population *cpop, int iss) {
    Class *clp;
    int i, j, k, n;

    n = -1;
    k = 10000000;
    for (i = 0; i <= cpop->hicl; i++) {
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
