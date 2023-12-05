
#define DOALL 1
#include "glob.h"

/*	-------------------- sran, fran, uran -------------------------- */
static double rcons = (1.0 / (2048.0 * 1024.0 * 1024.0));
#define M32 0xFFFFFFFF
#define B32 0x80000000
int sran() {
    int js;
    rseed = 69069 * rseed + 103322787;
    js = rseed & M32;
    return (js);
}

int uran() {
    int js;
    rseed = 69069 * rseed + 103322787;
    js = rseed & M32;
    if (js & B32)
        js = M32 - js;
    return (js & M32);
}

double fran() {
    int js;
    rseed = 69069 * rseed + 103322787;
    js = rseed & M32;
    if (js & B32)
        js = M32 - js;
    return (rcons * js);
}

/*	-------------------  findall  ---------------------------------  */
/*	Finds all classes of type(s) shown in bits of 'class_type'.
    (Dad = 1, Leaf = 2, Sub = 4), so if typ = 7, will find all classes.*/
/*	Sets the classes in 'sons[]'  */
/*	Puts count of classes found in numson */
void findall(int class_type) {
    int i, j;
    Class *cls;

    setpop();
    tidy(1);
    j = 0;
    cls = rootcl;

    while (cls) {
        if (class_type & cls->type) {
            sons[j++] = cls;
        }
        nextclass(&cls);
    }
    numson = j;

    // Set indices in nextic[] for non-descendant classes
    for (i = 0; i < numson; i++) {
        int idi = sons[i]->id;

        for (j = i + 1; j < numson; j++) {
            cls = sons[j];
            while (cls->id != idi) {
                if (cls->idad < 0) {
                    break;
                }
                cls = population->classes[cls->idad];
            }
            if (cls->idad < 0) {
                break;
            }
        }
        nextic[i] = (j < numson) ? j : numson;
    }
}

/*	-------------------  sortsons  -----------------------------  */
/*	To re-arrange the sons in the son chain of class kk in order of
increasing serial number  */
void sortsons(int kk) {
    Class *cls, *cls1, *cls2;
    int js, *prev, nsw;

    cls = population->classes[kk];
    if (cls->nson < 2) {
        return;
    }

    do {
        prev = &(cls->ison);
        nsw = 0;

        cls1 = population->classes[*prev];
        while (cls1->isib >= 0) {
            cls2 = population->classes[cls1->isib];
            if (cls1->serial > cls2->serial) {
                *prev = cls2->id;
                cls1->isib = cls2->isib;
                cls2->isib = cls1->id;
                prev = &cls2->isib;
                nsw = 1;
            } else {
                prev = &cls1->isib;
            }
            cls1 = population->classes[*prev];
        }
    } while (nsw);
    /*	Now sort sons  */
    for (js = cls->ison; js >= 0; js = population->classes[js]->isib) {
        sortsons(js);
    }
}

/*	--------------------  tidy  -------------------------------  */
/*	Reconstructs ison, isib, nson linkages from idad-s. If hit
    and AdjTr, kills classes which are too small.
Also deletes singleton sonclasses.  Re-counts pop->ncl, pop->hicl, pop->nleaf.
    */
void tidy(int hit) {
    Class *cls, *dad, *son;
    int i, kkd, ndead, newhicl, cause;

    deaded = 0;
    if (!population->nc)
        hit = 0;

    do {
        ndead = 0;
        for (i = 0; i <= population->hicl; i++) {
            cls = population->classes[i];
            if ((!cls) || (cls->type == Vacant) || (i == root)) {
                if (i == root) {
                    cls->nson = 0;
                    cls->ison = cls->isib = -1;
                }
                continue;
            }
            cls->nson = 0;
            cls->ison = cls->isib = -1;

            kkd = cls->idad;
            if (kkd < 0) {
                printf("Dad error in tidy\n");
                for (;;)
                    ;
            }
            int hard = 0;
            if (hit && (cls->cnt < MinSize)) {
                cause = Deadsmall;
                hard = 1;
            } else if (hit && (cls->type == Sub) &&
                       ((cls->age > MaxSubAge) || nosubs)) {
                cause = Dead;
                hard = 2;
            } else if (population->classes[kkd]->type == Vacant) {
                cause = Deadorphan;
                hard = 2;
            }

            if ((hard == 2) || ((hard == 1) && (control & AdjTr))) {
                if (seeall < 2)
                    seeall = 2;
                cls->idad = cause;
                cls->type = Vacant;
                ndead++;
            }
        }

        deaded += ndead;
        if (ndead)
            continue;

        /*	No more classes to kill for the moment.  Relink everyone  */
        population->ncl = 0;
        kkd = 0;
        for (i = 0; i <= population->hicl; i++) {
            cls = population->classes[i];
            if ((cls->type == Vacant) || (i == root)) {
                continue;
            }
            dad = population->classes[cls->idad];
            cls->isib = dad->ison;
            dad->ison = i;
            dad->nson++;
        }

        /*	Check for singleton sons   */
        for (kkd = 0; kkd <= population->hicl; kkd++) {
            dad = population->classes[kkd];
            if ((dad->type == Vacant) || (dad->nson != 1)) {
                continue;
            }

            cls = population->classes[dad->ison];
            /*	Clp is dad's only son. If a sub, kill it   */
            /*	If not, make dad inherit clp's role, then kill clp  */
            if (cls->type == Sub) {
                cause = Dead;
            } else {
                if (seeall < 2)
                    seeall = 2;
                dad->type = cls->type;
                dad->use = cls->use;
                dad->holdtype = cls->holdtype;
                dad->holduse = cls->holduse;
                dad->nson = cls->nson;
                dad->ison = cls->ison;
                /* Change the dad in clp's sons */
                for (i = dad->ison; i >= 0; i = son->isib) {
                    son = population->classes[i];
                    son->idad = kkd;
                }
                cause = Deadsing;
            }
            cls->idad = cause;
            cls->type = Vacant;
            ndead++;
        }
    } while (ndead);

    kkd = 0;
    // Check conditions directly and proceed if true
    if (hit && (control & AdjTr) && newsubs) {
        /* Add subclasses to large-enough leaves */
        for (i = 0; i <= population->hicl; i++) {
            dad = population->classes[i];
            // Check if conditions are met to make subclasses
            if (dad->type == Leaf && !dad->nson &&
                dad->cnt >= (2.1 * MinSize) && dad->age >= MinAge) {
                makesubs(i);
                kkd++;
            }
        }
    }

    deaded = 0;
    // Re-count classes, leaves etc.
    population->ncl = population->nleaf = newhicl = kkd = 0;
    for (i = 0; i <= population->hicl; i++) {
        cls = population->classes[i];
        if (cls && cls->type != Vacant) {
            if (cls->type == Leaf)
                population->nleaf++;
            population->ncl++;
            newhicl = i;
            if (cls->serial > kkd)
                kkd = cls->serial;
        }
    }
    population->hicl = newhicl;
    population->nextserial = (kkd >> 2) + 1;
    sortsons(population->root);
    return;
}

/*	------------------------  doall  --------------------------   */
/*	To do a complete cost-assign-adjust cycle on all things.
    If 'all', does it for all classes, else just leaves  */
/*	Leaves in scorechanges a count of significant score changes in Leaf
    classes whose use is Fac  */

void update_seeall_newsubs(int niter, int ncycles) {
    // Reset newsubs based on NewSubsTime, unless nosubs is true
    newsubs = (nosubs || (niter % NewSubsTime) != 0) ? 0 : 1;

    // Set seeall to 2 if newsubs is true and seeall is less than 2
    if (newsubs && seeall < 2) {
        seeall = 2;
    }

    // Adjust seeall based on remaining cycles
    if ((ncycles - niter) <= 2) {
        seeall = ncycles - niter;
    } else if (ncycles < 2) {
        seeall = 2;
    }

    // Track best if conditions are met
    if (niter > NewSubsTime && seeall == 1) {
        trackbest(0);
    }
}

int find_and_estimate(int *all, int niter, int ncycles) {
    int repeat = 0;
    if (fix == Random) {
        seeall = 3;
    }
    tidy(1);

    if (niter >= (ncycles - 1)) {
        *all = (Dad + Leaf + Sub);
    }
    findall(*all);

    for (int k = 0; k < numson; k++) {
        cleartcosts(sons[k]);
    }

    for (int j = 0; j < samp->nc; j++) {
        docase(j, *all, 1);
        // docase ignores classes with ignore bit in cls->vv[] for the
        // case unless seeall is on.
    }

    // All classes in sons[] now have stats assigned to them.
    // If all=Leaf, the classes are all leaves, so we just re-estimate
    // their parameters and get their pcosts for fac and plain uses,
    // using 'adjust'. But first, check all newcnt-s for vanishing
    // classes.
    if (control & (AdjPr + AdjTr)) {
        for (int k = 0; k < numson; k++) {
            if (sons[k]->newcnt < MinSize) {
                sons[k]->cnt = 0.0;
                sons[k]->type = Vacant;
                seeall = 2;
                newsubs = 0;
                repeat = 1;
                break;
            }
        }
    }
    return repeat;
}

double update_leaf_classes(double *oldleafsum, int *nfail) {
    double leafsum = 0.0;
    for (int k = 0; k < numson; k++) {
        adjustclass(sons[k], 0);
        /*	The second para tells adjust not to do as-dad params  */
        leafsum += sons[k]->cbcost;
    }
    if (seeall == 0) {
        rep('.');
    } else {
        if (leafsum < (*oldleafsum - MinGain)) {
            *nfail = 0;
            *oldleafsum = leafsum;
            rep('L');
        } else {
            (*nfail)++;
            rep('l');
        }
    }
    return leafsum;
}

void update_all_classes(double *oldcost, int *nfail) {
    /* all = 7, so we have dads, leaves and subs to do.
                We do from bottom up, collecting as-dad pcosts.  */
    cls = rootcl;
    int repeat;
    do {
        repeat = 0;
        cls->cnpcost = 0.0;
        if (cls->nson >= 2) {
            dad = cls;
            cls = population->classes[cls->ison];
            repeat = 1;
            continue;
        }
        while (1) {
            adjustclass(cls, 1);
            if (cls->idad >= 0) {
                dad = population->classes[cls->idad];
                dad->cnpcost += cls->cbpcost;
                if (cls->isib >= 0) {
                    cls = population->classes[cls->isib];
                    repeat = 1;
                    break;
                }
                /*	dad is now complete   */
                cls = dad;
            } else {

                break;
            }
        }
    } while (repeat);

    /*	Test for an improvement  */
    if (seeall == 0) {
        rep('.');
    } else if (rootcl->cbcost < (*oldcost - MinGain)) {
        *nfail = 0;
        *oldcost = rootcl->cbcost;
        rep('A');
    } else {
        (*nfail)++;
        rep('a');
    }
}

int count_score_changes() {
    /*	Scan leaf classes whose use is 'Fac' to accumulate significant
        score changes.  */
    int scorechanges = 0;
    for (int k = 0; k <= population->hicl; k++) {
        cls = population->classes[k];
        if (cls && (cls->type == Leaf) && (cls->use == Fac))
            scorechanges += cls->scorechange;
    }
    return scorechanges;
}
int doall(int ncycles, int all) {
    int niter, nfail, ic, ncydone, ncyask;
    double oldcost, leafsum, oldleafsum = 0.0;
    int kicked = 0;

    nfail = niter = ncydone = 0;
    ncyask = ncycles;
    all = (all) ? (Dad + Leaf + Sub) : Leaf;
    oldcost = rootcl->cbcost;
    /*	Get sum of class costs, meaningful only if 'all' = Leaf  */

    findall(Leaf);
    for (ic = 0; ic < numson; ic++) {
        oldleafsum += sons[ic]->cbcost;
    }

    while (niter < ncycles) {
        if ((niter % NewSubsTime) == 0) {
            newsubs = 1;
            if (seeall < 2)
                seeall = 2;
        } else
            newsubs = 0;
        if ((ncycles - niter) <= 2)
            seeall = ncycles - niter;
        if (ncycles < 2)
            seeall = 2;
        if (nosubs)
            newsubs = 0;
        if ((niter > newsubs) && (seeall == 1))
            trackbest(0);

        int repeat = 0;
        do {
            if (fix == Random)
                seeall = 3;
            tidy(1);
            if (niter >= (ncycles - 1))
                all = (Dad + Leaf + Sub);
            findall(all);
            for (int k = 0; k < numson; k++) {
                cleartcosts(sons[k]);
            }

            for (int j = 0; j < samp->nc; j++) {
                docase(j, all, 1);
                /*	docase ignores classes with ignore bit in cls->vv[] for the case
                    unless seeall is on.  */
            }

            // All classes in sons[] now have stats assigned to them.
            // If all=Leaf, the classes are all leaves, so we just re-estimate
            // their parameters and get their pcosts for fac and plain uses,
            // using 'adjust'. But first, check all newcnt-s for vanishing
            // classes.
            if (control & (AdjPr + AdjTr)) {
                for (int k = 0; k < numson; k++) {
                    if (sons[k]->newcnt < MinSize) {
                        sons[k]->cnt = 0.0;
                        sons[k]->type = Vacant;
                        seeall = 2;
                        newsubs = 0;
                        repeat = 1;
                        break;
                    }
                }
            }
        } while (repeat);

        if (!(all == (Dad + Leaf + Sub))) {
            leafsum = 0.0;
            for (ic = 0; ic < numson; ic++) {
                adjustclass(sons[ic], 0);
                /*	The second para tells adjust not to do as-dad params  */
                leafsum += sons[ic]->cbcost;
            }
            if (seeall == 0) {
                rep('.');
            } else {
                if (leafsum < (oldleafsum - MinGain)) {
                    nfail = 0;
                    oldleafsum = leafsum;
                    rep('L');
                } else {
                    nfail++;
                    rep('l');
                }
            }
        } else {

            /* all = 7, so we have dads, leaves and subs to do.
               We do from bottom up, collecting as-dad pcosts.
            */
            cls = rootcl;

            int alladjusted = 0;
            do {
                cls->cnpcost = 0.0;
                if (cls->nson >= 2) {
                    dad = cls;
                    cls = population->classes[cls->ison];
                    continue;
                }

                while (!alladjusted) {
                    adjustclass(cls, 1);
                    if (cls->idad < 0) {
                        alladjusted = 1;
                        break;
                    }
                    dad = population->classes[cls->idad];
                    dad->cnpcost += cls->cbpcost;
                    if (cls->isib >= 0) {
                        cls = population->classes[cls->isib];
                        break;
                    } else {
                        cls = dad; /*	dad is now complete   */
                    }
                }
            } while (!alladjusted);

            /*	Test for an improvement  */
            if (seeall == 0) {
                rep('.');
            } else {
                if (rootcl->cbcost < (oldcost - MinGain)) {
                    nfail = 0;
                    oldcost = rootcl->cbcost;
                    rep('A');
                } else {
                    nfail++;
                    rep('a');
                }
            }
        }

        if (nfail > GiveUp) {
            if (all != Leaf)
                break;
            /*	But if we were doing just leaves, wind up with a couple of
                'doall' cycles  */
            all = Dad + Leaf + Sub;
            ncycles = 2;
            niter = nfail = 0;
            continue;
        }
        if ((!usestdin) && hark(commsbuf.inl)) {
            kicked = 1;
            break;
        }

        if (seeall > 0)
            seeall--;
        ncydone++;
        niter++;
    }
    if (ncydone >= ncyask)
        ncydone = -1;

    if (kicked) {
        flp();
        printf("Doall interrupted after %4d steps\n", ncydone);
    }

    /*	Scan leaf classes whose use is 'Fac' to accumulate significant
        score changes.  */
    scorechanges = 0;
    for (ic = 0; ic <= population->hicl; ic++) {
        cls = population->classes[ic];
        if (cls && (cls->type == Leaf) && (cls->use == Fac))
            scorechanges += cls->scorechange;
    }
    return (ncydone);
}

int doall1(int ncycles, int all) {
    int niter, nfail, k, ncydone, ncyask;
    double oldcost, oldleafsum;
    int repeat;

    nfail = niter = ncydone = 0;
    ncyask = ncycles;
    all = (all) ? (Dad + Leaf + Sub) : Leaf;
    oldcost = rootcl->cbcost;
    /*	Get sum of class costs, meaningful only if 'all' = Leaf  */
    oldleafsum = 0.0;
    findall(Leaf);
    for (k = 0; k < numson; k++) {
        oldleafsum += sons[k]->cbcost;
    }

    while (1) {
        update_seeall_newsubs(niter, ncycles);

        do {
            repeat = find_and_estimate(&all, niter, ncycles);
        } while (repeat);

        if (!(all == (Dad + Leaf + Sub))) {
            update_leaf_classes(&oldleafsum, &nfail);
        } else {
            update_all_classes(&oldcost, &nfail);
        }

        if (nfail > GiveUp) {
            if (all != Leaf)
                break;
            /*	But if we were doing just leaves, wind up with a couple of
                'doall' cycles  */
            all = Dad + Leaf + Sub;
            ncycles = 2;
            niter = nfail = 0;
            continue;
        }
        if ((!usestdin) && hark(commsbuf.inl)) {
            flp();
            printf("Doall interrupted after %4d steps\n", ncydone);
            break;
        }

        if (seeall > 0) {
            seeall--;
        }
        ncydone++;
        niter++;
        if (niter >= ncycles) {
            if (ncydone >= ncyask) {
                ncydone = -1;
            }
            break;
        }
    }

    /*	Scan leaf classes whose use is 'Fac' to accumulate significant
        score changes.  */
    scorechanges = count_score_changes();
    return (ncydone);
}

/*	----------------------  dodads  -----------------------------  */
/*	Runs adjustclass on all leaves without adjustment.
    This leaves class cb*costs set up. Adjustclass is told not to
    consider a leaf as a potential dad.
    Then runs ncostvarall on all dads, with param adjustment. The
    result is to recost and readjust the tree hierarchy.
    */
int dodads(int ncy) {
    Class *dad;
    double oldcost;
    int nn, nfail;

    if (!(control & AdjPr))
        ncy = 1;

    /*	Capture no-prior params for subless leaves  */
    findall(Leaf);
    nfail = control;
    control = Noprior;
    for (nn = 0; nn < numson; nn++) {
        adjustclass(sons[nn], 0);
    }
    control = nfail;
    nn = nfail = 0;

    do {
        oldcost = rootcl->cnpcost;
        if (rootcl->type != Dad) {
            return (0);
        }
        /*	Begin a recursive scan of classes down to leaves   */
        cls = rootcl;

    newdad:
        if (cls->type == Leaf)
            goto complete;
        cls->cnpcost = 0.0;
        cls->relab = cls->cnt = 0.0;
        dad = cls;
        cls = population->classes[cls->ison];
        goto newdad;

    complete:
        /*	If a leaf, use adjustclass, else use ncostvarall  */
        if (cls->type == Leaf) {
            control = Tweak;
            adjustclass(cls, 0);
        } else {
            control = AdjPr;
            ncostvarall(cls, 1);
            cls->cbpcost = cls->cnpcost;
        }
        if (cls->idad < 0)
            goto alladjusted;
        dad = population->classes[cls->idad];
        dad->cnpcost += cls->cbpcost;
        dad->cnt += cls->cnt;
        dad->relab += cls->relab;
        if (cls->isib >= 0) {
            cls = population->classes[cls->isib];
            goto newdad;
        }
        cls = dad;
        goto complete;

    alladjusted:
        rootcl->cbpcost = rootcl->cnpcost;
        rootcl->cbcost = rootcl->cnpcost + rootcl->cntcost;
        /*	Test for convergence  */
        nn++;
        nfail++;
        if (rootcl->cnpcost < (oldcost - MinGain))
            nfail = 0;
        rep((nfail) ? 'd' : 'D');
        if (nfail > 3) {
            control = dcontrol;
            return (nn);
        }
    } while (nn < ncy);
    control = dcontrol;
    return (-1);
}

/*	-----------------  dogood  -----------------------------  */
/*	Does cycles combining doall, doleaves, dodads   */

/*	Uses this table of old costs to see if useful change in last 5 cycles */
double olddogcosts[6];

int dogood(int ncy, double target) {
    int j, nn, nfail;
    double oldcost;

    doall(1, 1);
    for (nn = 0; nn < 6; nn++)
        olddogcosts[nn] = rootcl->cbcost + 10000.0;
    nfail = 0;
    for (nn = 0; nn < ncy; nn++) {
        oldcost = rootcl->cbcost;
        doall(2, 0);
        if (rootcl->cbcost < (oldcost - MinGain))
            nfail = 0;
        else
            nfail++;
        rep((nfail) ? 'g' : 'G');
        if (heard)
            goto kicked;
        if (nfail > 2)
            goto done;
        if (rootcl->cbcost < target)
            goto bullseye;
        /*	See if new cost significantly better than cost 5 cycles ago */
        for (j = 0; j < 5; j++)
            olddogcosts[j] = olddogcosts[j + 1];
        olddogcosts[5] = rootcl->cbcost;
        if ((olddogcosts[0] - olddogcosts[5]) < 0.2)
            goto done;
    }
    nn = -1;
    goto done;

bullseye:
    flp();
    printf("Dogood reached target after %4d cycles\n", nn);
    goto done;

kicked:
    flp();
    printf("Dogood interrupted after %4d cycles\n", nn);
done:
    return (nn);
}

/*	-----------------------  docase  -----------------------------   */
/*	It is assumed that all classes have parameter info set up, and
    that all cases have scores in all classes.
    Assumes findall() has been used to find classes and set up
sons[], numson
    If 'derivs', calcs derivatives. Otherwize not.
    */
/*	Defines and uses a prototypical 'Saux' containing missing and Datum
    fields */
typedef struct PSauxst {
    int missing;
    double dummy;
    double xn;
} PSaux;

void docase(int cse, int all, int derivs) {
    double mincost, sum, rootcost, low, diff, w1, w2;
    Class *sub1, *sub2;
    PSaux *psaux;
    int clc, i;

    record = recs + cse * reclen; /*  Set ptr to case record  */
    item = cse;
    if (!*record) { // Inactive item
        return;
    }

    /*	Unpack data into 'xn' fields of the Saux for each variable. The
    'xn' field is at the beginning of the Saux. Also the "missing" flag. */
    for (i = 0; i < nv; i++) {
        loc = record + svars[i].offset;
        psaux = (PSaux *)svars[i].saux;
        if (*loc == 1) {
            psaux->missing = 1;
        } else {
            psaux->missing = 0;
            cmcpy(&(psaux->xn), loc + 1, vlist[i].vtp->datsize);
        }
    }

    /*	Deal with every class, as set up in sons[]  */

    clc = 0;
    while (clc < numson) {
        setclass2(sons[clc]);
        if ((!seeall) && (icvv & 1)) { /* Ignore this and decendants */
            clc = nextic[clc];
            continue;
        } else if (!seeall)
            cls->scancnt++;
        /*	Score and cost the class  */
        scorevarall(cls);
        costvarall(cls);
        clc++;
    }
    /*	Now have casescost, casefcost and casecost set in all classes for
        this case. We can distribute weight to all leaves, using their
        casecosts.  */
    /*	The whole item is irrelevant if just starting on root  */
    if (numson != 1) { /*  Not Just doing root  */
        /*	Clear all casewts   */
        for (clc = 0; clc < numson; clc++)
            sons[clc]->casewt = 0.0;
        mincost = 1.0e30;
        clc = 0;
        while (clc < numson) {
            cls = sons[clc];
            if ((!seeall) && (cls->caseiv & 1)) {
                cls->casecost = 1.0e30;
                clc = nextic[clc];
                continue;
            }
            clc++;
            if (fix == Random) {
                w1 = 2.0 * fran();
                cls->casecost += w1;
                cls->casefcost += w1;
                cls->casescost += w1;
            }
            if (cls->type != Leaf) {
                continue;
            }
            if (cls->casecost < mincost) {
                mincost = cls->casecost;
            }
        }

        sum = 0.0;
        if (fix != Most_likely) {
            /*	Minimum cost is in mincost. Compute unnormalized weights  */
            clc = 0;
            while (clc < numson) {
                cls = sons[clc];
                if ((cls->caseiv & 1) && (!seeall)) {
                    clc = nextic[clc];
                    continue;
                }
                clc++;
                if (cls->type != Leaf) {
                    continue;
                }
                cls->casewt = exp(mincost - cls->casecost);
                sum += cls->casewt;
            }
        } else {
            for (clc = 0; clc < numson; clc++) {
                cls = sons[clc];
                if ((cls->type == Leaf) && (cls->casecost == mincost)) {
                    sum += 1.0;
                    cls->casewt = 1.0;
                }
            }
        }

        /*	Normalize weights, and set root's casecost  */
        /*	It can happen that sum = 0. If so, give up on this case   */
        if (sum <= 0.0) {
            return;
        }
        if (fix == Random)
            rootcost = mincost;
        else
            rootcost = mincost - log(sum);
        rootcl->casencost = rootcl->casecost = rootcost;
        sum = 1.0 / sum;
        clc = 0;
        while (clc < numson) {
            cls = sons[clc];
            if ((cls->caseiv & 1) && (!seeall)) {
                clc = nextic[clc];
                continue;
            }
            clc++;
            if (cls->type != Leaf) {
                continue;
            }
            cls->casewt *= sum;
            /*	Can distribute this weight among subs, if any  */
            /*	But only if subs included  */
            if ((!(all & Sub)) || (cls->nson != 2) || (cls->casewt == 0.0)) {
                continue;
            }
            sub1 = population->classes[cls->ison];
            sub2 = population->classes[sub1->isib];

            /*	Test subclass ignore flags unless seeall   */
            if (!(seeall)) {
                if (sub1->caseiv & 1) {
                    if (!(sub2->caseiv & 1)) {
                        sub2->casewt = cls->casewt;
                        cls->casencost = sub2->casecost;
                        continue;
                    }
                } else {
                    if (sub2->caseiv & 1) { /* Only sub1 has weight */
                        sub1->casewt = cls->casewt;
                        cls->casencost = sub1->casecost;
                        continue;
                    }
                }
            }

            /*	Both subs costed  */
            diff = sub1->casecost - sub2->casecost;
            /*	Diff can be used to set cls's casencost  */
            if (diff < 0.0) {
                low = sub1->casecost;
                w2 = exp(diff);
                w1 = 1.0 / (1.0 + w2);
                w2 *= w1;
                if (w2 < MinSubWt)
                    sub2->caseiv |= 1;
                else
                    sub2->caseiv &= -2;
                sub2->vv[item] = sub2->caseiv;
                if (fix == Random)
                    cls->casencost = low;
                else
                    cls->casencost = low + log(w1);
            } else {
                low = sub2->casecost;
                w1 = exp(-diff);
                w2 = 1.0 / (1.0 + w1);
                w1 *= w2;
                if (w1 < MinSubWt)
                    sub1->caseiv |= 1;
                else
                    sub1->caseiv &= -2;
                sub1->vv[item] = sub1->caseiv;
                if (fix == Random)
                    cls->casencost = low;
                else
                    cls->casencost = low + log(w2);
            }
            /*	Assign randomly if sub age 0, or to-best if sub age < MinAge */
            if (sub1->age < MinAge) {
                if (sub1->age == 0) {
                    w1 = (sran() < 0) ? 1.0 : 0.0;
                } else {
                    w1 = (diff < 0) ? 1.0 : 0.0;
                }
                w2 = 1.0 - w1;
            }
            sub1->casewt = cls->casewt * w1;
            sub2->casewt = cls->casewt * w2;
        }

        /*	We have now assigned caseweights to all Leafs and Subs.
            Collect weights from leaves into Dads, setting their casecosts  */
        if (rootcl->type != Leaf) { /* skip when root is only leaf */
            for (clc = numson - 1; clc >= 0; clc--) {
                cls = sons[clc];
                if ((cls->type == Sub) || ((!seeall) && (cls->vv[item] & 1))) {
                    continue;
                }
                if (cls->casewt < MinWt)
                    cls->vv[item] |= 1;
                else
                    cls->vv[item] &= -2;
                if (cls->idad >= 0)
                    population->classes[cls->idad]->casewt += cls->casewt;
                if (cls->type == Dad) {
                    /*	casecost for the completed dad is root's cost - log
                     * dad's wt
                     */
                    if (cls->casewt > 0.0)
                        cls->casencost = rootcost - log(cls->casewt);
                    else
                        cls->casencost = rootcost + 200.0;
                    cls->casecost = cls->casencost;
                }
            }
        }
    }
    rootcl->casewt = 1.0;
    /*	Now all classes have casewt assigned, I hope. Can proceed to
    collect statistics from this case  */
    if (!derivs) {
        return;
    }
    for (clc = 0; clc < numson; clc++) {
        cls = sons[clc];
        if (cls->casewt > 0.0) {
            derivvarall(cls);
        }
    }
}
