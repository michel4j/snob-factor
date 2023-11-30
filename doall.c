
#define DOALL 1
#include "glob.h"
#include <omp.h>

/*    -------------------- rand_int, rand_float, -------------------------- */
static double rcons = (1.0 / (2048.0 * 1024.0 * 1024.0));

#define M32 0xFFFFFFFF
#define B32 0x80000000

double rand_float() {
    int js;
    RSeed = 69069 * RSeed + 103322787;
    js = RSeed & M32;
    if (js & B32)
        js = M32 - js;
    return (rcons * js);
}

int rand_int() {
    int js;
    RSeed = 69069 * RSeed + 103322787;
    js = RSeed & M32;
    return (js);
}

/*    -------------------  find_all  ---------------------------------  */
/*    Finds all classes of type(s) shown in bits of 'class_type'.
    (Dad = 1, Leaf = 2, Sub = 4), so if typ = 7, will find all classes.*/
/*    Sets the classes in 'sons[]'  */
/*    Puts count of classes found in numson */
void find_all(int class_type) {
    int i, j;
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
    NumSon = j;

    // Set indices in nextic[] for non-descendant classes
    for (i = 0; i < NumSon; i++) {
        int idi = Sons[i]->id;
        for (j = i + 1; j < NumSon; j++) {
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
        NextIc[i] = (j < NumSon) ? j : NumSon;
    }
}

/*    -------------------  sort_sons  -----------------------------  */
/*    To re-arrange the sons in the son chain of class kk in order of
increasing serial number  */
void sort_sons(int kk) {
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
    /*    Now sort sons  */
    for (js = cls->son_id; js >= 0; js = CurPopln->classes[js]->sib_id) {
        sort_sons(js);
    }
}

/*    --------------------  tidy  -------------------------------  */
/*    Reconstructs ison, isib, nson linkages from idad-s. If hit
    and AdjTr, kills classes which are too small.
Also deletes singleton sonclasses.  Re-counts pop->ncl, pop->hicl, pop->nleaf.
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
                log_msg(2, "Dad error in tidy!");
                return;
            }
            int hard = 0;
            if (hit && (cls->weights_sum < MinSize)) {
                cause = Deadsmall;
                hard = 1;
            } else if (hit && (cls->type == Sub) && ((cls->age > MaxSubAge) || NoSubs)) {
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

        /*    No more classes to kill for the moment.  Relink everyone  */
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

        /*    Check for singleton sons   */
        for (kkd = 0; kkd <= CurPopln->hi_class; kkd++) {
            dad = CurPopln->classes[kkd];
            if ((dad->type == Vacant) || (dad->num_sons != 1)) {
                continue;
            }

            cls = CurPopln->classes[dad->son_id];
            /*    Clp is dad's only son. If a sub, kill it   */
            /*    If not, make dad inherit clp's role, then kill clp  */
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
            if (dad->type == Leaf && !dad->num_sons && dad->weights_sum >= (2.1 * MinSize) && dad->age >= MinAge) {
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
    sort_sons(CurPopln->root);
}

/*    ------------------------  do_all  --------------------------   */
/*    To do a complete cost-assign-adjust cycle on all things.
    If 'all', does it for all classes, else just leaves  */
/*    Leaves in scorechanges a count of significant score changes in Leaf
    classes whose use is Fac  */

void update_seeall_newsubs(int niter, int ncycles) {
    // Reset newsubs based on NewSubsTime, unless nosubs is true
    NewSubs = (NoSubs || (niter % NewSubsTime) != 0) ? 0 : 1;

    // Set seeall to 2 if newsubs is true and seeall is less than 2
    if (NewSubs && SeeAll < 2) {
        SeeAll = 2;
    }

    // Adjust seeall based on remaining cycles
    if ((ncycles - niter) <= 2) {
        SeeAll = ncycles - niter;
    } else if (ncycles < 2) {
        SeeAll = 2;
    }

    // Track best if conditions are met
    if (niter > NewSubsTime && SeeAll == 1) {
        track_best(0);
    }
}

int find_and_estimate(int *all, int niter, int ncycles) {
    int repeat = 0;
    if (Fix == Random) {
        SeeAll = 3;
    }
    tidy(1);

    if (niter >= (ncycles - 1)) {
        *all = (Dad + Leaf + Sub);
    }
    find_all(*all);

    for (int k = 0; k < NumSon; k++) {
        clear_costs(Sons[k]);
    }

    for (int j = 0; j < CurSample->num_cases; j++) {
        do_case(j, *all, 1);
        // do_case ignores classes with ignore bit in cls->vv[] for the
        // case unless seeall is on.
    }

    // All classes in sons[] now have stats assigned to them.
    // If all=Leaf, the classes are all leaves, so we just re-estimate
    // their parameters and get their pcosts for fac and plain uses,
    // using 'adjust'. But first, check all newcnt-s for vanishing
    // classes.
    if (Control & (AdjPr + AdjTr)) {
        for (int k = 0; k < NumSon; k++) {
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
    return repeat;
}

double update_leaf_classes(double *oldleafsum, int *nfail) {
    double leafsum = 0.0;
    char token;
    for (int k = 0; k < NumSon; k++) {
        adjust_class(Sons[k], 0); // The second par tells adjust not to do as-dad params
        leafsum += Sons[k]->best_cost;
    }
    if (SeeAll == 0) {
        token = '.';
    } else {
        if (leafsum < (*oldleafsum - MinGain)) {
            *nfail = 0;
            *oldleafsum = leafsum;
            token = 'L';
        } else {
            (*nfail)++;
            token = 'l';
        }
    }
    rep(token);
    return leafsum;
}

void update_all_classes(double *oldcost, int *nfail) {
    /* all = 7, so we have dads, leaves and subs to do.
                We do from bottom up, collecting as-dad pcosts.  */
    CurClass = CurRootClass;
    int repeat;
    char token;

    do {
        repeat = 0;
        CurClass->dad_par_cost = 0.0;
        if (CurClass->num_sons >= 2) {
            CurDad = CurClass;
            CurClass = CurPopln->classes[CurClass->son_id];
            repeat = 1;
            continue;
        }
        while (1) {
            adjust_class(CurClass, 1);
            if (CurClass->dad_id >= 0) {
                CurDad = CurPopln->classes[CurClass->dad_id];
                CurDad->dad_par_cost += CurClass->best_par_cost;
                if (CurClass->sib_id >= 0) {
                    CurClass = CurPopln->classes[CurClass->sib_id];
                    repeat = 1;
                    break;
                }
                /*    dad is now complete   */
                CurClass = CurDad;
            } else {

                break;
            }
        }
    } while (repeat);

    /*    Test for an improvement  */
    if (SeeAll == 0) {
        token = '.';
    } else if (CurRootClass->best_cost < (*oldcost - MinGain)) {
        *nfail = 0;
        *oldcost = CurRootClass->best_cost;
        token = 'A';
    } else {
        (*nfail)++;
        token = 'a';
    }
    rep(token);
}

int count_score_changes() {
    /*    Scan leaf classes whose use is 'Fac' to accumulate significant
        score changes.  */
    int scorechanges = 0;
    for (int k = 0; k <= CurPopln->hi_class; k++) {
        CurClass = CurPopln->classes[k];
        if (CurClass && (CurClass->type == Leaf) && (CurClass->use == Fac))
            scorechanges += CurClass->score_change_count;
    }
    return scorechanges;
}

int do_all(int ncycles, int all) {
    int niter, nfail, k, ncydone, ncyask;
    double oldcost, oldleafsum;
    int repeat;

    nfail = niter = ncydone = 0;
    ncyask = ncycles;
    all = (all) ? (Dad + Leaf + Sub) : Leaf;
    oldcost = CurRootClass->best_cost;
    /*    Get sum of class costs, meaningful only if 'all' = Leaf  */
    oldleafsum = 0.0;
    find_all(Leaf);
    for (k = 0; k < NumSon; k++) {
        oldleafsum += Sons[k]->best_cost;
    }

    while (niter < ncycles) {

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
            /*    But if we were doing just leaves, wind up with a couple of
                'do_all' cycles  */
            all = Dad + Leaf + Sub;
            ncycles = 2;
            niter = nfail = 0;
            continue;
        }
        ncydone++;
        if ((!UseLib) && (!UseStdIn) && hark(commsbuf.inl)) {
            log_msg(2, "\nDOALL: Interrupted after %4d steps", ncydone);
            break;
        }

        if (SeeAll > 0) {
            SeeAll--;
        }

        niter++;
        print_progress(niter, ncycles);
        if (niter >= ncycles) {
            if (ncydone >= ncyask) {
                ncydone = -1;
            }
            break;
        }
    }

    /*    Scan leaf classes whose use is 'Fac' to accumulate significant
        score changes.  */
    ScoreChanges = count_score_changes();
    printf("\n");
    return (niter);
}

/*    ----------------------  do_dads  -----------------------------  */
/*    Runs adjust_class on all leaves without adjustment.
    This leaves class cb*costs set up. Adjustclass is told not to
    consider a leaf as a potential dad.
    Then runs parent_cost_all_vars on all dads, with param adjustment. The
    result is to recost and readjust the tree hierarchy.
    */
int do_dads(int ncy) {
    Class *dad;
    double oldcost;
    int nn, nfail;

    if (!(Control & AdjPr))
        ncy = 1;

    /*    Capture no-prior params for subless leaves  */
    find_all(Leaf);
    nfail = Control;
    Control = Noprior;
    for (nn = 0; nn < NumSon; nn++) {
        adjust_class(Sons[nn], 0);
    }
    Control = nfail;
    nn = nfail = 0;

    do {
        oldcost = CurRootClass->dad_par_cost;
        if (CurRootClass->type != Dad) {
            return (0);
        }
        /*    Begin a recursive scan of classes down to leaves   */
        CurClass = CurRootClass;

    newdad:
        if (CurClass->type == Leaf)
            goto complete;
        CurClass->dad_par_cost = 0.0;
        CurClass->relab = CurClass->weights_sum = 0.0;
        dad = CurClass;
        CurClass = CurPopln->classes[CurClass->son_id];
        goto newdad;

    complete:
        /*    If a leaf, use adjust_class, else use parent_cost_all_vars  */
        if (CurClass->type == Leaf) {
            Control = Tweak;
            adjust_class(CurClass, 0);
        } else {
            Control = AdjPr;
            parent_cost_all_vars(CurClass, 1);
            CurClass->best_par_cost = CurClass->dad_par_cost;
        }
        if (CurClass->dad_id < 0)
            goto alladjusted;
        dad = CurPopln->classes[CurClass->dad_id];
        dad->dad_par_cost += CurClass->best_par_cost;
        dad->weights_sum += CurClass->weights_sum;
        dad->relab += CurClass->relab;
        if (CurClass->sib_id >= 0) {
            CurClass = CurPopln->classes[CurClass->sib_id];
            goto newdad;
        }
        CurClass = dad;
        goto complete;

    alladjusted:
        CurRootClass->best_par_cost = CurRootClass->dad_par_cost;
        CurRootClass->best_cost = CurRootClass->dad_par_cost + CurRootClass->cntcost;
        /*    Test for convergence  */
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

/*    -----------------  do_good  -----------------------------  */
/*    Does cycles combining doall, doleaves, dodads   */

/*    Uses this table of old costs to see if useful change in last 5 cycles */
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
        if (!UseLib && Heard)
            goto kicked;
        if (nfail > 2)
            goto done;
        if (CurRootClass->best_cost < target)
            goto bullseye;
        /*    See if new cost significantly better than cost 5 cycles ago */
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

/*  -----------------------  do_case  -----------------------------
    It is assumed that all classes have parameter info set up, and
    that all cases have scores in all classes.
    Assumes find_all() has been used to find classes and set up sons[], numson
    If 'derivs', calcs derivatives. Otherwize not.
    Defines and uses a prototypical 'Saux' containing missing and Datum
    fields
*/
typedef struct PSauxst {
    int missing;
    double dummy;
    double xn;
} PSaux;

void do_case(int item, int all, int derivs) {
    double mincost, sum, rootcost, low, diff, w1, w2;
    Class *sub1, *sub2;
    PSaux *psaux;
    char *record, *field;
    int clc, i;

    record = CurRecords + item * CurRecLen; //  Set ptr to case record
    if (!*record) {                         // Inactive item
        return;
    }

    /*    Unpack data into 'xn' fields of the Saux for each variable. The
    'xn' field is at the beginning of the Saux. Also the "missing" flag. */

    for (i = 0; i < NumVars; i++) {
        field = record + CurVarList[i].offset;
        psaux = (PSaux *)CurVarList[i].saux;
        if (*field == 1) {
            psaux->missing = 1;
        } else {
            psaux->missing = 0;
            memcpy(&(psaux->xn), field + 1, CurAttrList[i].vtype->data_size);
        }
    }

    /*    Deal with every class, as set up in sons[]  */

    clc = 0;
    while (clc < NumSon) {
        set_class_with_scores(Sons[clc], item);
        if ((!SeeAll) && (case_fac_int & 1)) { /* Ignore this and decendants */
            clc = NextIc[clc];
            continue;
        } else if (!SeeAll)
            CurClass->scancnt++;
        /*    Score and cost the class  */
        score_all_vars(CurClass, item);
        cost_all_vars(CurClass, item);
        clc++;
    }
    /*    Now have casescost, casefcost and casecost set in all classes for
        this case. We can distribute weight to all leaves, using their
        casecosts.  */
    /*    The whole item is irrelevant if just starting on root  */
    if (NumSon != 1) { /*  Not Just doing root  */
        /*    Clear all casewts   */

        for (clc = 0; clc < NumSon; clc++)
            Sons[clc]->case_weight = 0.0;

        mincost = 1.0e30;
        clc = 0;
        while (clc < NumSon) {
            CurClass = Sons[clc];
            if ((!SeeAll) && (CurClass->case_score & 1)) {
                CurClass->total_case_cost = 1.0e30;
                clc = NextIc[clc];
                continue;
            }
            clc++;
            if (Fix == Random) {
                w1 = 2.0 * rand_float();
                CurClass->total_case_cost += w1;
                CurClass->fac_case_cost += w1;
                CurClass->nofac_case_cost += w1;
            }
            if (CurClass->type != Leaf) {
                continue;
            }
            if (CurClass->total_case_cost < mincost) {
                mincost = CurClass->total_case_cost;
            }
        }

        sum = 0.0;
        if (Fix != Most_likely) {
            /*    Minimum cost is in mincost. Compute unnormalized weights  */
            clc = 0;
            while (clc < NumSon) {
                CurClass = Sons[clc];
                if ((CurClass->case_score & 1) && (!SeeAll)) {
                    clc = NextIc[clc];
                    continue;
                }
                clc++;
                if (CurClass->type != Leaf) {
                    continue;
                }
                CurClass->case_weight = exp(mincost - CurClass->total_case_cost);
                sum += CurClass->case_weight;
            }
        } else {
            for (clc = 0; clc < NumSon; clc++) {
                CurClass = Sons[clc];
                if ((CurClass->type == Leaf) && (CurClass->total_case_cost == mincost)) {
                    sum += 1.0;
                    CurClass->case_weight = 1.0;
                }
            }
        }

        /*    Normalize weights, and set root's casecost  */
        /*    It can happen that sum = 0. If so, give up on this case   */
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
        while (clc < NumSon) {
            CurClass = Sons[clc];
            if ((CurClass->case_score & 1) && (!SeeAll)) {
                clc = NextIc[clc];
                continue;
            }
            clc++;
            if (CurClass->type != Leaf) {
                continue;
            }
            CurClass->case_weight *= sum;
            /*    Can distribute this weight among subs, if any  */
            /*    But only if subs included  */
            if ((!(all & Sub)) || (CurClass->num_sons != 2) || (CurClass->case_weight == 0.0)) {
                continue;
            }
            sub1 = CurPopln->classes[CurClass->son_id];
            sub2 = CurPopln->classes[sub1->sib_id];

            /*    Test subclass ignore flags unless seeall   */
            if (!(SeeAll)) {
                if (sub1->case_score & 1) {
                    if (!(sub2->case_score & 1)) {
                        sub2->case_weight = CurClass->case_weight;
                        CurClass->dad_case_cost = sub2->total_case_cost;
                        continue;
                    }
                } else {
                    if (sub2->case_score & 1) { /* Only sub1 has weight */
                        sub1->case_weight = CurClass->case_weight;
                        CurClass->dad_case_cost = sub1->total_case_cost;
                        continue;
                    }
                }
            }

            /*    Both subs costed  */
            diff = sub1->total_case_cost - sub2->total_case_cost;
            /*    Diff can be used to set cls's casencost  */
            if (diff < 0.0) {
                low = sub1->total_case_cost;
                w2 = exp(diff);
                w1 = 1.0 / (1.0 + w2);
                w2 *= w1;
                if (w2 < MinSubWt)
                    sub2->case_score |= 1;
                else
                    sub2->case_score &= -2;
                sub2->factor_scores[item] = sub2->case_score;
                if (Fix == Random)
                    CurClass->dad_case_cost = low;
                else
                    CurClass->dad_case_cost = low + log(w1);
            } else {
                low = sub2->total_case_cost;
                w1 = exp(-diff);
                w2 = 1.0 / (1.0 + w1);
                w1 *= w2;
                if (w1 < MinSubWt)
                    sub1->case_score |= 1;
                else
                    sub1->case_score &= -2;
                sub1->factor_scores[item] = sub1->case_score;
                if (Fix == Random)
                    CurClass->dad_case_cost = low;
                else
                    CurClass->dad_case_cost = low + log(w2);
            }
            /*    Assign randomly if sub age 0, or to-best if sub age < MinAge */
            if (sub1->age < MinAge) {
                if (sub1->age == 0) {
                    w1 = (rand_int() < 0) ? 1.0 : 0.0;
                } else {
                    w1 = (diff < 0) ? 1.0 : 0.0;
                }
                w2 = 1.0 - w1;
            }
            sub1->case_weight = CurClass->case_weight * w1;
            sub2->case_weight = CurClass->case_weight * w2;
        }

        /*    We have now assigned caseweights to all Leafs and Subs.
            Collect weights from leaves into Dads, setting their casecosts  */
        if (CurRootClass->type != Leaf) { /* skip when root is only leaf */
            for (clc = NumSon - 1; clc >= 0; clc--) {
                CurClass = Sons[clc];
                if ((CurClass->type == Sub) || ((!SeeAll) && (CurClass->factor_scores[item] & 1))) {
                    continue;
                }
                if (CurClass->case_weight < MinWt)
                    CurClass->factor_scores[item] |= 1;
                else
                    CurClass->factor_scores[item] &= -2;
                if (CurClass->dad_id >= 0)
                    CurPopln->classes[CurClass->dad_id]->case_weight += CurClass->case_weight;
                if (CurClass->type == Dad) {
                    /*    casecost for the completed dad is root's cost - log
                     * dad's wt
                     */
                    if (CurClass->case_weight > 0.0)
                        CurClass->dad_case_cost = rootcost - log(CurClass->case_weight);
                    else
                        CurClass->dad_case_cost = rootcost + 200.0;
                    CurClass->total_case_cost = CurClass->dad_case_cost;
                }
            }
        }
    }
    CurRootClass->case_weight = 1.0;
    /*    Now all classes have casewt assigned, I hope. Can proceed to
    collect statistics from this case  */
    if (!derivs) {
        return;
    }
    for (clc = 0; clc < NumSon; clc++) {
        CurClass = Sons[clc];
        if (CurClass->case_weight > 0.0) {
            deriv_all_vars(CurClass, item);
        }
    }
}
