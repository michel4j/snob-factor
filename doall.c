
#define DOALL 1
#include "glob.h"

/*	-------------------- sran, fran, uran -------------------------- */
static double rcons = (1.0 / (2048.0 * 1024.0 * 1024.0));
#define M32 0xFFFFFFFF
#define B32 0x80000000
int rand_int() {
    int js;
    RSeed = 69069 * RSeed + 103322787;
    js = RSeed & M32;
    return (js);
}

int rand_uint() {
    int js;
    RSeed = 69069 * RSeed + 103322787;
    js = RSeed & M32;
    if (js & B32)
        js = M32 - js;
    return (js & M32);
}

double rand_float() {
    int js;
    RSeed = 69069 * RSeed + 103322787;
    js = RSeed & M32;
    if (js & B32)
        js = M32 - js;
    return (rcons * js);
}

/*	-------------------  findall  ---------------------------------  */
/*	Finds all classes of type(s) shown in bits of 'class_type'.
    (Dad = 1, Leaf = 2, Sub = 4), so if typ = 7, will find all classes.*/
/*	Sets the classes in 'sons[]'  */
/*	Puts count of classes found in numson */
int find_all(int class_type) {
    int i, j, num_son;
    Class *cls;

    set_population();
    tidy(1);
    j = 0;
    cls = CurRootClass;

    while (cls) {
        if (class_type & cls->type) {
            Sons[j++] = cls;
        }
        next_class(&cls);
    }
    num_son = j;

    // Set indices in nextic[] for non-descendant classes
    for (i = 0; i < num_son; i++) {
        int idi = Sons[i]->id;

        for (j = i + 1; j < num_son; j++) {
            cls = Sons[j];
            while (cls->id != idi) {
                if (cls->dad_id < 0) {
                    break;
                }
                cls = CurPopln->classes[cls->dad_id];
            }
            if (cls->dad_id < 0) {
                break;
            }
        }
        NextIc[i] = (j < num_son) ? j : num_son;
    }
    return num_son;
}

/*	-------------------  sortsons  -----------------------------  */
/*	To re-arrange the sons in the son chain of class kk in order of
increasing serial number  */
void sortsons(int kk) {
    Class *cls, *cls1, *cls2;
    int js, *prev, nsw;

    cls = CurPopln->classes[kk];
    if (cls->num_sons < 2) {
        return;
    }

    do {
        prev = &(cls->son_id);
        nsw = 0;

        cls1 = CurPopln->classes[*prev];
        while (cls1->sib_id >= 0) {
            cls2 = CurPopln->classes[cls1->sib_id];
            if (cls1->serial > cls2->serial) {
                *prev = cls2->id;
                cls1->sib_id = cls2->sib_id;
                cls2->sib_id = cls1->id;
                prev = &cls2->sib_id;
                nsw = 1;
            } else {
                prev = &cls1->sib_id;
            }
            cls1 = CurPopln->classes[*prev];
        }
    } while (nsw);
    /*	Now sort sons  */
    for (js = cls->son_id; js >= 0; js = CurPopln->classes[js]->sib_id) {
        sortsons(js);
    }
}

/*	--------------------  tidy  -------------------------------  */
/*	Reconstructs ison, isib, nson linkages from CurDadId-s. If hit
    and AdjTr, kills classes which are too small.
Also deletes singleton sonclasses.  Re-counts pop->ncl, pop->hicl, pop->num_leaves.
    */
void tidy(int hit) {
    Class *cls, *dad, *son;
    int i, kkd, ndead, newhicl, cause;

    Deaded = 0;
    if (!CurPopln->sample_size)
        hit = 0;

    do {
        ndead = 0;
        for (i = 0; i <= CurPopln->hi_class; i++) {
            cls = CurPopln->classes[i];
            if ((!cls) || (cls->type == Vacant) || (i == CurRoot)) {
                if (i == CurRoot) {
                    cls->num_sons = 0;
                    cls->son_id = cls->sib_id = -1;
                }
                continue;
            }
            cls->num_sons = 0;
            cls->son_id = cls->sib_id = -1;

            kkd = cls->dad_id;
            if (kkd < 0) {
                printf("Dad error in tidy\n");
                for (;;)
                    ;
            }
            int hard = 0;
            if (hit && (cls->weights_sum < MinSize)) {
                cause = Deadsmall;
                hard = 1;
            } else if (hit && (cls->type == Sub) &&
                       ((cls->age > MaxSubAge) || NoSubs)) {
                cause = Dead;
                hard = 2;
            } else if (CurPopln->classes[kkd]->type == Vacant) {
                cause = Deadorphan;
                hard = 2;
            }

            if ((hard == 2) || ((hard == 1) && (Control & AdjTr))) {
                if (SeeAll < 2)
                    SeeAll = 2;
                cls->dad_id = cause;
                cls->type = Vacant;
                ndead++;
            }
        }

        Deaded += ndead;
        if (ndead)
            continue;

        /*	No more classes to kill for the moment.  Relink everyone  */
        CurPopln->num_classes = 0;
        kkd = 0;
        for (i = 0; i <= CurPopln->hi_class; i++) {
            cls = CurPopln->classes[i];
            if ((cls->type == Vacant) || (i == CurRoot)) {
                continue;
            }
            dad = CurPopln->classes[cls->dad_id];
            cls->sib_id = dad->son_id;
            dad->son_id = i;
            dad->num_sons++;
        }

        /*	Check for singleton sons   */
        for (kkd = 0; kkd <= CurPopln->hi_class; kkd++) {
            dad = CurPopln->classes[kkd];
            if ((dad->type == Vacant) || (dad->num_sons != 1)) {
                continue;
            }

            cls = CurPopln->classes[dad->son_id];
            /*	Clp is dad's only son. If a sub, kill it   */
            /*	If not, make dad inherit clp's role, then kill clp  */
            if (cls->type == Sub) {
                cause = Dead;
            } else {
                if (SeeAll < 2)
                    SeeAll = 2;
                dad->type = cls->type;
                dad->use = cls->use;
                dad->hold_type = cls->hold_type;
                dad->hold_use = cls->hold_use;
                dad->num_sons = cls->num_sons;
                dad->son_id = cls->son_id;
                /* Change the dad in clp's sons */
                for (i = dad->son_id; i >= 0; i = son->sib_id) {
                    son = CurPopln->classes[i];
                    son->dad_id = kkd;
                }
                cause = Deadsing;
            }
            cls->dad_id = cause;
            cls->type = Vacant;
            ndead++;
        }
    } while (ndead);

    kkd = 0;
    // Check conditions directly and proceed if true
    if (hit && (Control & AdjTr) && NewSubs) {
        /* Add subclasses to large-enough leaves */
        for (i = 0; i <= CurPopln->hi_class; i++) {
            dad = CurPopln->classes[i];
            // Check if conditions are met to make subclasses
            if (dad->type == Leaf && !dad->num_sons &&
                dad->weights_sum >= (2.1 * MinSize) && dad->age >= MinAge) {
                make_subclasses(i);
                kkd++;
            }
        }
    }

    Deaded = 0;
    // Re-count classes, leaves etc.
    CurPopln->num_classes = CurPopln->num_leaves = newhicl = kkd = 0;
    for (i = 0; i <= CurPopln->hi_class; i++) {
        cls = CurPopln->classes[i];
        if (cls && cls->type != Vacant) {
            if (cls->type == Leaf)
                CurPopln->num_leaves++;
            CurPopln->num_classes++;
            newhicl = i;
            if (cls->serial > kkd)
                kkd = cls->serial;
        }
    }
    CurPopln->hi_class = newhicl;
    CurPopln->next_serial = (kkd >> 2) + 1;
    sortsons(CurPopln->root);
    return;
}


/*	------------------------  doall  --------------------------   */
/*	To do a complete cost-assign-adjust cycle on all things.
    If 'all', does it for all classes, else just leaves  */
/*	Leaves in scorechanges a count of significant score changes in Leaf
    classes whose use is Fac  */

void update_seeall_newsubs(int niter, int ncycles) {

    if ((niter % NewSubsTime) == 0) {
        NewSubs = 1;
        if (SeeAll < 2)
            SeeAll = 2;
    } else {
        NewSubs = 0;
    }
    if ((ncycles - niter) <= 2) {
        SeeAll = ncycles - niter;
    }
    if (ncycles < 2) {
        SeeAll = 2;
    }
    if (NoSubs) {
        NewSubs = 0;
    }
    if ((niter > NewSubsTime) && (SeeAll == 1)) {
        track_best(0);
    }
}

void find_and_estimate(int *all, int niter, int ncycles) {
    int repeat = 0, num_son;
    do {
        if (Fix == Random)
            SeeAll = 3;
        tidy(1);
        if (niter >= (ncycles - 1))
            *all = (Dad + Leaf + Sub);
        num_son = find_all(*all);
        for (int k = 0; k < num_son; k++) {
            clear_costs(Sons[k]);
        }

        for (int j = 0; j < CurSample->num_cases; j++) {
            do_case(j, *all, 1, num_son);
            /*	docase ignores classes with ignore bit in cls->vv[] for the case
                unless SeeAll is on.  */
        }

        // All classes in Sons[] now have stats assigned to them.
        // If all=Leaf, the classes are all leaves, so we just re-estimate
        // their parameters and get their pcosts for fac and plain uses,
        // using 'adjust'. But first, check all newcnt-s for vanishing
        // classes.
        if (Control & (AdjPr + AdjTr)) {
            for (int k = 0; k < num_son; k++) {
                if (Sons[k]->newcnt < MinSize) {
                    Sons[k]->weights_sum = 0.0;
                    Sons[k]->type = Vacant;
                    SeeAll = 2;
                    NewSubs = 0;
                    repeat = 1;
                    break;
                }
            }
        }
    } while (repeat);
}

double update_leaf_classes(double *oldleafsum, int *nfail, int num_son) {
    double leafsum = 0.0;
    leafsum = 0.0;
    for (int ic = 0; ic < num_son; ic++) {
        adjust_class(Sons[ic], 0);
        /*	The second para tells adjust not to do as-dad params  */
        leafsum += Sons[ic]->best_cost;
    }
    if (SeeAll == 0) {
        rep('.');
    } else {
        if (leafsum < (*oldleafsum - MinGain)) {
            (*nfail) = 0;
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

    Class *dad, *cls = CurRootClass;

    int adjusted = 0;

    while (!adjusted) {
        cls->dad_par_cost = 0.0;
        if (cls->num_sons >= 2) {
            dad = cls;
            cls = CurPopln->classes[cls->son_id];
            continue;
        } else {
            int complete = 0;
            while (!complete) {
                adjust_class(cls, 1);
                if (cls->dad_id < 0) {
                    adjusted = 1;
                    complete = 1;
                } else {
                    dad = CurPopln->classes[cls->dad_id];
                    dad->dad_par_cost += cls->best_par_cost;
                    if (cls->sib_id >= 0) {
                        cls = CurPopln->classes[cls->sib_id];
                        complete = 1;
                        break;
                    }
                }
                //	dad is now complete
                cls = dad;
            }
        }
    }

    //	Test for an improvement
    if (SeeAll == 0) {
        rep('.');
    } else {
        if (CurRootClass->best_cost < (*oldcost - MinGain)) {
            (*nfail) = 0;
            *oldcost = CurRootClass->best_cost;
            rep('A');
        } else {
            (*nfail)++;
            rep('a');
        }
    }
}

int count_score_changes() {
    /*	Scan leaf classes whose use is 'Fac' to accumulate significant
        score changes.  */
    Class *cls;

    int scorechanges = 0;
    for (int k = 0; k <= CurPopln->hi_class; k++) {
        cls = CurPopln->classes[k];
        if (cls && (cls->type == Leaf) && (cls->use == Fac))
            scorechanges += cls->score_change_count;
    }
    return scorechanges;
}

int do_all(int ncycles, int all) {
    int niter, nfail, ic, ncydone, ncyask, kicked = 0, num_son;
    double oldcost, oldleafsum;

    nfail = niter = ncydone = 0;
    ncyask = ncycles;
    all = (all) ? (Dad + Leaf + Sub) : Leaf;
    oldcost = CurRootClass->best_cost;
    /*	Get sum of class costs, meaningful only if 'all' = Leaf  */
    oldleafsum = 0.0;
    num_son = find_all(Leaf);
    for (ic = 0; ic < num_son; ic++) {
        oldleafsum += Sons[ic]->best_cost;
    }

    while (niter < ncycles) {
        update_seeall_newsubs(niter, ncycles);
        find_and_estimate(&all, niter, ncycles);

        if (all != (Dad + Leaf + Sub)) {
            update_leaf_classes(&oldleafsum, &nfail, num_son);
            
        } else {
            // all = 7, so we have dads, leaves and subs to do.
            // We do from bottom up, collecting as-dad pcosts.
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
        if ((Interactive) && (!UseStdIn) && hark(CommsBuffer.inl)) {
            kicked = 1;
            break;
        }
        if (SeeAll > 0)
            SeeAll--;
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
    ScoreChanges = count_score_changes();
    return (ncydone);
}


/*	----------------------  dodads  -----------------------------  */
/*	Runs adjustclass on all leaves without adjustment.
    This leaves class cb*costs set up. Adjustclass is told not to
    consider a leaf as a potential dad.
    Then runs ncostvarall on all dads, with param adjustment. The
    result is to recost and readjust the tree hierarchy.
    */
int do_dads(int ncy) {
    Class *dad, *cls;
    double oldcost;
    int nn, nfail, num_son;

    if (!(Control & AdjPr))
        ncy = 1;

    /*	Capture no-prior params for subless leaves  */
    num_son = find_all(Leaf);
    nfail = Control;
    Control = Noprior;
    for (nn = 0; nn < num_son; nn++) {
        adjust_class(Sons[nn], 0);
    }
    Control = nfail;
    nn = nfail = 0;

    do {
        oldcost = CurRootClass->dad_par_cost;
        if (CurRootClass->type != Dad) {
            return (0);
        }
        /*	Begin a recursive scan of classes down to leaves   */
        cls = CurRootClass;

    newdad:
        if (cls->type == Leaf)
            goto complete;
        cls->dad_par_cost = 0.0;
        cls->relab = cls->weights_sum = 0.0;
        dad = cls;
        cls = CurPopln->classes[cls->son_id];
        goto newdad;

    complete:
        /*	If a leaf, use adjustclass, else use ncostvarall  */
        if (cls->type == Leaf) {
            Control = Tweak;
            adjust_class(cls, 0);
        } else {
            Control = AdjPr;
            parent_cost_all_vars(cls, 1);
            cls->best_par_cost = cls->dad_par_cost;
        }
        if (cls->dad_id < 0)
            goto alladjusted;
        dad = CurPopln->classes[cls->dad_id];
        dad->dad_par_cost += cls->best_par_cost;
        dad->weights_sum += cls->weights_sum;
        dad->relab += cls->relab;
        if (cls->sib_id >= 0) {
            cls = CurPopln->classes[cls->sib_id];
            goto newdad;
        }
        cls = dad;
        goto complete;

    alladjusted:
        CurRootClass->best_par_cost = CurRootClass->dad_par_cost;
        CurRootClass->best_cost = CurRootClass->dad_par_cost + CurRootClass->cntcost;
        /*	Test for convergence  */
        nn++;
        nfail++;
        if (CurRootClass->dad_par_cost < (oldcost - MinGain))
            nfail = 0;
        rep((nfail) ? 'd' : 'D');
        if (nfail > 3) {
            Control = DControl;
            return (nn);
        }
    } while (nn < ncy);
    Control = DControl;
    return (-1);
}

/*	-----------------  dogood  -----------------------------  */
/*	Does cycles combining doall, doleaves, dodads   */

/*	Uses this table of old costs to see if useful change in last 5 cycles */
double olddogcosts[6];

int do_good(int ncy, double target) {
    int j, nn, nfail;
    double oldcost;

    do_all(1, 1);
    for (nn = 0; nn < 6; nn++)
        olddogcosts[nn] = CurRootClass->best_cost + 10000.0;
    nfail = 0;
    for (nn = 0; nn < ncy; nn++) {
        oldcost = CurRootClass->best_cost;
        do_all(2, 0);
        if (CurRootClass->best_cost < (oldcost - MinGain))
            nfail = 0;
        else
            nfail++;
        rep((nfail) ? 'g' : 'G');
        if (Heard)
            goto kicked;
        if (nfail > 2)
            goto done;
        if (CurRootClass->best_cost < target)
            goto bullseye;
        /*	See if new cost significantly better than cost 5 cycles ago */
        for (j = 0; j < 5; j++)
            olddogcosts[j] = olddogcosts[j + 1];
        olddogcosts[5] = CurRootClass->best_cost;
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

void do_case(int cse, int all, int derivs, int num_son) {
    double mincost, sum, rootcost, low, diff, w1, w2;
    Class *sub1, *sub2, *cls;
    PSaux *psaux;
    int clc, i;

    CurRecord = Records + cse * RecLen; /*  Set ptr to case record  */
    CurItem = cse;
    if (!*CurRecord) { // Inactive item
        return;
    }

    /*	Unpack data into 'xn' fields of the Saux for each variable. The
    'xn' field is at the beginning of the Saux. Also the "missing" flag. */
    for (i = 0; i < NumVars; i++) {
        CurField = CurRecord + SmplVarList[i].offset;
        psaux = (PSaux *)SmplVarList[i].saux;
        if (*CurField == 1) {
            psaux->missing = 1;
        } else {
            psaux->missing = 0;
            memcpy(&(psaux->xn), CurField + 1, VSetVarList[i].vtype->data_size);
        }
    }

    /*	Deal with every class, as set up in sons[]  */

    clc = 0;
    while (clc < num_son) {
        cls = Sons[clc];
        set_class_score(Sons[clc]);
        if ((!SeeAll) && (Scores.CaseFacInt & 1)) { /* Ignore this and decendants */
            clc = NextIc[clc];
            continue;
        } else if (!SeeAll)
            cls->scancnt++;
        /*	Score and cost the class  */
        score_all_vars(cls);
        cost_all_vars(cls);
        clc++;
    }
    /*	Now have casescost, casefcost and casecost set in all classes for
        this case. We can distribute weight to all leaves, using their
        casecosts.  */
    /*	The whole item is irrelevant if just starting on root  */
    if (num_son != 1) { /*  Not Just doing root  */
        /*	Clear all casewts   */
        for (clc = 0; clc < num_son; clc++)
            Sons[clc]->case_weight = 0.0;
        mincost = 1.0e30;
        clc = 0;
        while (clc < num_son) {
            cls = Sons[clc];
            if ((!SeeAll) && (cls->case_score & 1)) {
                cls->total_case_cost = 1.0e30;
                clc = NextIc[clc];
                continue;
            }
            clc++;
            if (Fix == Random) {
                w1 = 2.0 * rand_float();
                cls->total_case_cost += w1;
                cls->fac_case_cost += w1;
                cls->nofac_case_cost += w1;
            }
            if (cls->type != Leaf) {
                continue;
            }
            if (cls->total_case_cost < mincost) {
                mincost = cls->total_case_cost;
            }
        }

        sum = 0.0;
        if (Fix != Most_likely) {
            /*	Minimum cost is in mincost. Compute unnormalized weights  */
            clc = 0;
            while (clc < num_son) {
                cls = Sons[clc];
                if ((cls->case_score & 1) && (!SeeAll)) {
                    clc = NextIc[clc];
                    continue;
                }
                clc++;
                if (cls->type != Leaf) {
                    continue;
                }
                cls->case_weight = exp(mincost - cls->total_case_cost);
                sum += cls->case_weight;
            }
        } else {
            for (clc = 0; clc < num_son; clc++) {
                cls = Sons[clc];
                if ((cls->type == Leaf) && (cls->total_case_cost == mincost)) {
                    sum += 1.0;
                    cls->case_weight = 1.0;
                }
            }
        }

        /*	Normalize weights, and set root's casecost  */
        /*	It can happen that sum = 0. If so, give up on this case   */
        if (sum <= 0.0) {
            return;
        }
        if (Fix == Random)
            rootcost = mincost;
        else
            rootcost = mincost - log(sum);
        CurRootClass->dad_case_cost = CurRootClass->total_case_cost = rootcost;
        sum = 1.0 / sum;
        clc = 0;
        while (clc < num_son) {
            cls = Sons[clc];
            if ((cls->case_score & 1) && (!SeeAll)) {
                clc = NextIc[clc];
                continue;
            }
            clc++;
            if (cls->type != Leaf) {
                continue;
            }
            cls->case_weight *= sum;
            /*	Can distribute this weight among subs, if any  */
            /*	But only if subs included  */
            if ((!(all & Sub)) || (cls->num_sons != 2) || (cls->case_weight == 0.0)) {
                continue;
            }
            sub1 = CurPopln->classes[cls->son_id];
            sub2 = CurPopln->classes[sub1->sib_id];

            /*	Test subclass ignore flags unless seeall   */
            if (!(SeeAll)) {
                if (sub1->case_score & 1) {
                    if (!(sub2->case_score & 1)) {
                        sub2->case_weight = cls->case_weight;
                        cls->dad_case_cost = sub2->total_case_cost;
                        continue;
                    }
                } else {
                    if (sub2->case_score & 1) { /* Only sub1 has weight */
                        sub1->case_weight = cls->case_weight;
                        cls->dad_case_cost = sub1->total_case_cost;
                        continue;
                    }
                }
            }

            /*	Both subs costed  */
            diff = sub1->total_case_cost - sub2->total_case_cost;
            /*	Diff can be used to set cls's casencost  */
            if (diff < 0.0) {
                low = sub1->total_case_cost;
                w2 = exp(diff);
                w1 = 1.0 / (1.0 + w2);
                w2 *= w1;
                if (w2 < MinSubWt)
                    sub2->case_score |= 1;
                else
                    sub2->case_score &= -2;
                sub2->factor_scores[CurItem] = sub2->case_score;
                if (Fix == Random)
                    cls->dad_case_cost = low;
                else
                    cls->dad_case_cost = low + log(w1);
            } else {
                low = sub2->total_case_cost;
                w1 = exp(-diff);
                w2 = 1.0 / (1.0 + w1);
                w1 *= w2;
                if (w1 < MinSubWt)
                    sub1->case_score |= 1;
                else
                    sub1->case_score &= -2;
                sub1->factor_scores[CurItem] = sub1->case_score;
                if (Fix == Random)
                    cls->dad_case_cost = low;
                else
                    cls->dad_case_cost = low + log(w2);
            }
            /*	Assign randomly if sub age 0, or to-best if sub age < MinAge */
            if (sub1->age < MinAge) {
                if (sub1->age == 0) {
                    w1 = (rand_int() < 0) ? 1.0 : 0.0;
                } else {
                    w1 = (diff < 0) ? 1.0 : 0.0;
                }
                w2 = 1.0 - w1;
            }
            sub1->case_weight = cls->case_weight * w1;
            sub2->case_weight = cls->case_weight * w2;
        }

        /*	We have now assigned caseweights to all Leafs and Subs.
            Collect weights from leaves into Dads, setting their casecosts  */
        if (CurRootClass->type != Leaf) { /* skip when root is only leaf */
            for (clc = num_son - 1; clc >= 0; clc--) {
                cls = Sons[clc];
                if ((cls->type == Sub) || ((!SeeAll) && (cls->factor_scores[CurItem] & 1))) {
                    continue;
                }
                if (cls->case_weight < MinWt)
                    cls->factor_scores[CurItem] |= 1;
                else
                    cls->factor_scores[CurItem] &= -2;
                if (cls->dad_id >= 0)
                    CurPopln->classes[cls->dad_id]->case_weight += cls->case_weight;
                if (cls->type == Dad) {
                    /*	casecost for the completed dad is root's cost - log
                     * dad's wt
                     */
                    if (cls->case_weight > 0.0)
                        cls->dad_case_cost = rootcost - log(cls->case_weight);
                    else
                        cls->dad_case_cost = rootcost + 200.0;
                    cls->total_case_cost = cls->dad_case_cost;
                }
            }
        }
    }
    CurRootClass->case_weight = 1.0;
    /*	Now all classes have casewt assigned, I hope. Can proceed to
    collect statistics from this case  */
    if (!derivs) {
        return;
    }
    for (clc = 0; clc < num_son; clc++) {
        cls = Sons[clc];
        if (cls->case_weight > 0.0) {
            deriv_all_vars(cls);
        }
    }
}
