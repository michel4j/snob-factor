/*    Stuff dealing with classes as members of polpns     */
#define CLASSES 1
#include "glob.h"

/*    -----------------------  serial_to_id  ---------------------------   */
/*    Finds class index (id) from serial, or -3 if error    */
int serial_to_id(int ss) {
    int k;
    Class *clp;
    set_population();

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

/*    ---------------------  set_class --------------------------   */
void set_class(Class *cls) {
    CurClass = cls;
    CurDadID = CurClass->dad_id;
    if (CurDadID >= 0)
        CurDad = CurPopln->classes[CurDadID];
    else
        CurDad = 0;
}

/*    ---------------------  set_class_with_scores --------------------------   */
void set_class_with_scores(Class *cls, int item) {
    set_class(cls);
    vv = CurClass->vv;
    CurClass->case_score = icvv = vv[item];
    CurCaseWeight = CurClass->case_weight;
}

/*    ---------------------   make_class  -------------------------   */
/*    Makes the basic structure of a class (Class) with vector of
ptrs to CVinsts and CVinsts filled in also EVinsts and ptrs.
    Uses nc = pop->nc.
    if nc = 0, that's it. Otherwize, if a sample is defined in pop,
    score vector.
    Returns index of class, or negative if no good  */

int error_and_message(const char *message) {
    printf("Error: %s\n", message);
    return -1;
}

int make_class() {
    PVinst *pvars;
    CVinst **cvars, *cvi;
    EVinst **evars, *evi;
    int i, kk, found = 0, vacant = 0;

    NumCases = CurCtx.popln->sample_size;
    /*    If nc, check that popln has an attached sample  */
    if (NumCases) {
        i = find_sample(CurPopln->sample_name, 1);
        if (i < 0) {
            return (-2);
        }
        CurCtx.sample = CurSample = Samples[i];
    }

    NumVars = CurVSet->num_vars;
    pvars = CurPopln->pvars;
    /*    Find a vacant slot in the popln's classes vec   */
    for (kk = 0; kk < CurPopln->cls_vec_len; kk++) {
        if (!CurPopln->classes[kk]) {
            found = 1;
            break;
        }
        /*    Also test for a vacated class   */
        if (CurPopln->classes[kk]->type == Vacant) {
            vacant = 1;
            break;
        }
    }
    if (!(found || vacant)) {
        return error_and_message("Popln full of classes\n");
    } else if (found) {

        CurClass = (Class *)alloc_blocks(1, sizeof(Class));
        if (!CurClass) {
            return error_and_message("No space for new class\n");
        }

        CurPopln->classes[kk] = CurClass;
        CurPopln->hi_class = kk; /* Highest used index in population->classes */
        CurClass->id = kk;

        /*    Make vector of ptrs to CVinsts   */
        cvars = CurClass->basics = (CVinst **)alloc_blocks(1, NumVars * sizeof(CVinst *));
        if (!cvars) {
            return error_and_message("No space for new class\n");
        }

        /*    Now make the CVinst blocks, which are of varying size   */
        for (i = 0; i < NumVars; i++) {
            pvi = pvars + i;
            CurAttr = CurAttrList + i;
            cvi = cvars[i] = (CVinst *)alloc_blocks(1, CurAttr->basic_size);
            if (!cvi) {
                return error_and_message("No space for new class\n");
            }
            /*    Fill in standard stuff  */
            cvi->id = CurAttr->id;
        }

        /*    Make EVinst blocks and vector of pointers  */
        evars = CurClass->stats = (EVinst **)alloc_blocks(1, NumVars * sizeof(EVinst *));
        if (!evars) {
            return error_and_message("No space for new class\n");
        }
        for (i = 0; i < NumVars; i++) {
            pvi = pvars + i;
            CurAttr = CurAttrList + i;
            evi = evars[i] = (EVinst *)alloc_blocks(1, CurAttr->stats_size);
            if (!evi) {
                return error_and_message("No space for new class\n");
            }
            evi->id = pvi->id;
        }

        /*    Stomp on ptrs as yet undefined  */
        CurClass->vv = 0;
        CurClass->type = 0;

    } else if (vacant) { /* Vacant type shows structure set up but vacated.
          Use, but set new (Vacant) type,  */
        CurClass = CurPopln->classes[kk];
        cvars = CurClass->basics;
        evars = CurClass->stats;
        CurClass->type = Vacant;
    }

    CurClass->best_cost = CurClass->nofac_cost = CurClass->fac_cost = 0.0;
    CurClass->weights_sum = 0.0;
    /*    Invalidate hierarchy links  */
    CurClass->dad_id = CurClass->sib_id = CurClass->son_id = -1;
    CurClass->num_sons = 0;
    for (i = 0; i < NumVars; i++) {
        cvars[i]->signif = 1;
    }
    CurPopln->num_classes++;
    if (kk > CurPopln->hi_class) {
        CurPopln->hi_class = kk;
    }
    if (CurClass->type != Vacant) {
        /* If nc = 0, this completes the make. */
        if (NumCases != 0) {
            CurClass->vv = (short *)alloc_blocks(2, NumCases * sizeof(short));

            for (int i = 0; i < NumVars; i++) {
                evars[i]->num_values = 0.0;
            }
        }
    } else {
        CurClass->type = 0;

        for (int i = 0; i < NumVars; i++) {
            evars[i]->num_values = 0.0;
        }
    }

    CurClass->age = 0;
    CurClass->hold_type = CurClass->hold_use = 0;
    CurClass->weights_sum = 0.0;
    return (kk);
}

/*    -----------------------  print_class  -----------------------  */
/*    To print parameters of a class    */
/*    If kk = -1, prints all non-subs. If kk = -2, prints all  */
char *typstr[] = {"typ?", " DAD", "LEAF", "typ?", " Sub"};
char *usestr[] = {"Tny", "Pln", "Fac"};
void print_one_class(Class *clp, int full) {
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
    printf("Tcosts  S:%9.2f  F:%9.2f  D:%9.2f  B:%9.2f\n", clp->cstcost, clp->cftcost - CurClass->cfvcost, clp->cntcost, clp->best_case_cost);
    printf("Vcost     ---------  F:%9.2f\n", clp->cfvcost);
    printf("totals  S:%9.2f  F:%9.2f  D:%9.2f  B:%9.2f\n", clp->nofac_cost, clp->fac_cost, clp->dad_cost, clp->best_cost);
    if (!full)
        return;
    for (i = 0; i < CurVSet->num_vars; i++) {
        CurVType = CurAttrList[i].vtype;
        (*CurVType->show)(clp, i);
    }
    return;
}

void print_class(int kk, int full) {
    Class *clp;
    if (kk >= 0) {
        clp = CurPopln->classes[kk];
        print_one_class(clp, full);
    } else if (kk < -2) {
        printf("%d passed to print_class\n", kk);
    } else {
        set_population();
        clp = CurRootClass;

        do {
            if ((kk == -2) || (clp->type != Sub))
                print_one_class(clp, full);
            next_class(&clp);
        } while (clp);
    }
}

/*    -------------------------  clear_costs  ------  */
/*    Clears tcosts (cntcost, cftcost, cstcost). Also clears newcnt,
newvsq, and calls clear_stats for all variables   */
void clear_costs(Class *ccl) {
    int i;

    set_class(ccl);
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
        CurVType = CurAttrList[i].vtype;
        (*CurVType->clear_stats)(i);
    }
}

/*    -------------------  set_best_costs  ------------------------   */
/*    To set 'b' params, costs to reflect current use   */
void set_best_costs(Class *ccl) {
    int i;

    set_class(ccl);
    if (CurClass->type == Dad) {
        CurClass->best_cost = CurClass->dad_cost;
        CurClass->best_par_cost = CurClass->dad_par_cost;
        CurClass->best_case_cost = CurClass->cntcost;
    } else if (CurClass->use != Fac) {
        CurClass->best_cost = CurClass->nofac_cost;
        CurClass->best_par_cost = CurClass->nofac_par_cost;
        CurClass->best_case_cost = CurClass->cstcost;
    } else {
        CurClass->best_cost = CurClass->fac_cost;
        CurClass->best_par_cost = CurClass->fac_par_cost;
        CurClass->best_case_cost = CurClass->cftcost;
    }
    for (i = 0; i < CurVSet->num_vars; i++) {
        CurVType = CurAttrList[i].vtype;
        (*CurVType->set_best_pars)(i);
    }
}

/*    --------------------  score_all_vars  --------------------    */
/*    To evaluate derivs of cost wrt score in all vars of a class.
    Does one item, number case  */
/*    Leaves data values set in stats, but does no scoring if class too
young. If class age = MinFacAge, will guess scores but not cost them */
/*    If control & AdjSc, will adjust score  */
void score_all_vars(Class *ccl, int item) {
    double del, cvvTimesHScoreScale, oneOverVvd2;
    int icvv = 0, oldicvv, igbit;

    set_class_with_scores(ccl, item);
    if ((CurClass->age < MinFacAge) || (CurClass->use == Tiny)) {
        cvv = CurClass->avvv = CurClass->sum_score_sq = 0.0;
        icvv = 0;
    } else {
        if (CurClass->sum_score_sq <= 0.0) {
            //    Generate a fake score to get started.
            CurClass->boost_count = 0;
            cvvsprd = 0.1 / NumVars;
            oldicvv = igbit = 0;
            cvv = (rand_int() < 0) ? 1.0 : -1.0;
        } else {
            //    Get current score
            oldicvv = icvv;
            igbit = icvv & 1;
            cvv = icvv * ScoreRscale;
            if (CurClass->boost_count && ((Control & AdjSP) == AdjSP)) {
                cvv *= CurClass->score_boost;
                cvv = fmin(fmax(cvv, -Maxv), Maxv); // Clamp cvv to [-Maxv, Maxv]
                cvvTimesHScoreScale = cvv * HScoreScale;
                del = cvvTimesHScoreScale - (cvvTimesHScoreScale < 0.0 ? 1.0 : 0.0);
                icvv = (int)(del + 0.5) << 1; // Round to nearest even times ScoreScale
                cvv = icvv * ScoreRscale;
            }

            cvvsq = cvv * cvv;
            vvd1 = vvd2 = mvvd2 = vvd3 = 0.0;
            for (int i = 0; i < CurVSet->num_vars; i++) {
                CurAttr = CurAttrList + i;
                if (!CurAttr->inactive) {
                    CurVType = CurAttr->vtype;
                    (*CurVType->score_var)(i); // score_var should add to vvd1, vvd2, vvd3, mvvd2.
                }
            }
            vvd1 += cvv;
            vvd2 += 1.0;
            mvvd2 += 1.0;             //  From prior  There is a cost term 0.5 * cvvsprd from the prior (whence the additional 1 in vvd2).
            oneOverVvd2 = 1.0 / vvd2; // Also, overall cost includes 0.5*cvvsprd*vvd2, so there is a derivative term wrt cvv of 0.5*cvvsprd*vvd3
            cvvsprd = oneOverVvd2;
            vvd1 += 0.5 * cvvsprd * vvd3;
            del = vvd1 / mvvd2;
            if (Control & AdjSc) {
                cvv -= del;
            }
        }

        cvv = fmin(fmax(cvv, -Maxv), Maxv); // Clamp cvv to [-Maxv, Maxv]
        del = cvv * HScoreScale - (cvv * HScoreScale < 0.0 ? 1.0 : 0.0);
        icvv = ((int)(del + rand_float())) << 1; // Round to nearest even times ScoreScale
        icvv |= igbit;                           // Restore original ignore bit
        if (!igbit) {
            oldicvv -= icvv;
            oldicvv = abs(oldicvv);
            if (oldicvv > SigScoreChange)
                CurClass->score_change_count++;
        }
        CurClass->cvv = cvv = icvv * ScoreRscale;
        CurClass->cvvsq = cvvsq = cvv * cvv;
        CurClass->cvvsprd = cvvsprd;
    }
    vv[item] = CurClass->case_score = icvv;
}

/*    ----------------------  cost_all_vars  --------------------------  */
/*    Does cost_var on all vars of class for the current item, setting
cls->casecost according to use of class  */
void cost_all_vars(Class *ccl, int item) {
    int fac = 0;
    double tmp;

    set_class_with_scores(ccl, item);
    if ((CurClass->age >= MinFacAge) && (CurClass->use != Tiny)) {
        fac = 1;
        cvvsq = cvv * cvv;
    }
    ncasecost = scasecost = fcasecost = CurClass->mlogab; /* Abundance cost */
    for (int iv = 0; iv < CurVSet->num_vars; iv++) {
        CurAttr = CurAttrList + iv;
        if (!(CurAttr->inactive)) {
            CurVType = CurAttr->vtype;
            (*CurVType->cost_var)(iv, fac); /*    will add to scasecost, fcasecost  */
        }
    }

    CurClass->total_case_cost = CurClass->nofac_case_cost = scasecost;
    CurClass->fac_case_cost = scasecost + 10.0;
    if (CurClass->num_sons < 2)
        CurClass->dad_case_cost = 0.0;
    CurClass->coding_case_cost = 0.0;
    if (fac) {
        /*    Now we add the cost of coding the score, and its roundoff */
        /*    The simple form for costing a score is :
            tmp = hlg2pi + 0.5 * (cvvsq + cvvsprd - log (cvvsprd)) + lattice;
        However, we appeal to the large number of score parameters, which gives a
        more negative 'lattice' ((log 12)/2 for one parameter) approaching -(1/2)
        log (2 Pi e) which results in the reduced cost :  */
        CurClass->clvsprd = log(cvvsprd);
        tmp = 0.5 * (cvvsq + cvvsprd - CurClass->clvsprd - 1.0);
        /*
            Over all scores for the class, the lattice effect will add approx
                ( log (2 Pi cnt)) / 2  + 1
            to the class cost. This is added later, once cnt is known.
        */
        fcasecost += tmp;
        CurClass->fac_case_cost = fcasecost;
        CurClass->coding_case_cost = tmp;
        if (CurClass->use == Fac)
            CurClass->total_case_cost = fcasecost;
    }
}

/*    ----------------------  deriv_all_vars  ---------------------    */
/*    To collect derivative statistics for all vars of a class   */
void deriv_all_vars(Class *ccl, int item) {
    int fac = 0;

    set_class_with_scores(ccl, item);
    CurClass->newcnt += CurCaseWeight;
    if ((CurClass->age >= MinFacAge) && (CurClass->use != Tiny)) {
        fac = 1;
        cvv = CurClass->cvv;
        cvvsq = CurClass->cvvsq;
        cvvsprd = CurClass->cvvsprd;
        CurClass->newvsq += CurCaseWeight * cvvsq;
        CurClass->vav += CurCaseWeight * CurClass->clvsprd;
        CurClass->totvv += cvv * CurCaseWeight;
    }
    for (int iv = 0; iv < NumVars; iv++) {
        CurAttr = CurAttrList + iv;
        if (!(CurAttr->inactive)) {
            CurVType = CurAttr->vtype;
            (*CurVType->deriv_var)(iv, fac);
        }
    }

    /*    Collect case item costs   */
    CurClass->cstcost += CurCaseWeight * CurClass->nofac_case_cost;
    CurClass->cftcost += CurCaseWeight * CurClass->fac_case_cost;
    CurClass->cntcost += CurCaseWeight * CurClass->dad_case_cost;
    CurClass->cfvcost += CurCaseWeight * CurClass->coding_case_cost;
}

/*    --------------------  adjust_class  -----------------------   */
/*    To compute pcosts of a class, and if needed, adjust params  */
/*    Will process as-dad params only if 'dod'  */
void adjust_class(Class *ccl, int dod) {
    int iv, fac, npars, small;
    Class *son;
    double leafcost;

    set_class(ccl);
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

    /*    cls->cnpcost was zeroed in do_all, and has accumulated the cbpcosts
    of cls's sons, so we don't zero it here. 'parent_cost_all_vars' will add to it.  */
    CurClass->nofac_par_cost = CurClass->fac_par_cost = 0.0;
    for (iv = 0; iv < CurVSet->num_vars; iv++) {
        CurAttr = CurAttrList + iv;
        if (!(CurAttr->inactive)) {
            CurVType = CurAttr->vtype;
            (*CurVType->adjust)(iv, fac);
        }
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
        parent_cost_all_vars(ccl, npars);

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
    if ((!CurClass->hold_use) && (Control & AdjPr)) {
        /*    If scores boosted too many times, make Tiny and hold   */
        if (CurClass->boost_count > 20) {
            CurClass->use = Tiny;
            CurClass->hold_use = 10;
        } else {
            /*    Check if size too small to support a factor  */
            /*    Add up the number of data values  */
            small = 0;
            leafcost = 0.0; /* Used to add up evi->cnts */
            for (iv = 0; iv < NumVars; iv++) {
                if (!(CurAttrList[iv].inactive)) {
                    small++;
                    leafcost += CurClass->stats[iv]->num_values;
                }
            }
            /*    I want at least 1.5 data vals per param  */
            small = (leafcost < (9 * small + 1.5 * CurClass->weights_sum + 1)) ? 1 : 0;
            if (small) {
                if (CurClass->use != Tiny) {
                    CurClass->use = Tiny;
                }
            } else if (CurClass->use == Tiny) {
                CurClass->use = Plain;
                CurClass->sum_score_sq = 0.0;
            } else if (CurClass->age >= MinFacAge) {
                if ((CurClass->use == Plain && CurClass->fac_cost < CurClass->nofac_cost) ||
                    (CurClass->use != Plain && CurClass->nofac_cost < CurClass->fac_cost)) {
                    CurClass->use = (CurClass->use == Plain) ? Fac : Plain;
                    if (CurClass->use == Fac) {
                        CurClass->boost_count = 0;
                        CurClass->score_boost = 1.0;
                    }
                }
            }
        }
    }

    if (dod && !(CurClass->hold_type) && (Control & AdjTr) && (CurClass->num_sons >= 2)) {
        leafcost = (CurClass->use == Fac) ? CurClass->fac_cost : CurClass->nofac_cost;

        if ((CurClass->type == Dad) && (leafcost < CurClass->dad_cost) && (Fix != Random)) {
            flp();
            printf("Changing type of class%s from Dad to Leaf\n", sers(CurClass));
            SeeAll = 4;
            delete_sons(CurClass->id); // Changes type to leaf
        } else if (npars && (leafcost > CurClass->dad_cost) && (CurClass->type == Leaf)) {
            flp();
            printf("Changing type of class%s from Leaf to Dad\n", sers(CurClass));
            SeeAll = 4;
            CurClass->type = Dad;
            if (CurDad)
                CurDad->hold_type += 3;

            for (int i = 0; i < 2; i++) {
                son = CurPopln->classes[i == 0 ? CurClass->son_id : son->sib_id];
                son->type = Leaf;
                son->serial = 4 * CurPopln->next_serial++;
            }
        }
    }
    set_best_costs(CurClass);
    if (Control & AdjPr)
        CurClass->age++;
}

/*    ----------------  parent_cost_all_vars  ------------------------------   */
/*    To do parent costing on all vars of a class        */
/*    If not 'valid', don't cost, and fake params  */
void parent_cost_all_vars(Class *ccl, int valid) {
    Class *son;
    EVinst *evi;
    int ison, nson;
    double abcost, rrelab;

    set_class(ccl);
    abcost = 0.0;
    for (int i = 0; i < CurVSet->num_vars; i++) {
        CurAttr = CurAttrList + i;
        CurVType = CurAttr->vtype;
        (*CurVType->cost_var_nonleaf)(i, valid);
        evi = (EVinst *)CurClass->stats[i];
        if (!CurAttr->inactive) {
            abcost += evi->npcost;
        }
    }

    if (!valid) {
        CurClass->dad_par_cost = CurClass->dad_cost = 0.0;
    } else {
        nson = CurClass->num_sons;
        /*    The sons of a dad may be listed in any order, so the param cost
        of the dad can be reduced by log (nson !)  */
        /*    The cost of saying 'dad' and number of sons is set at nson bits. */
        abcost += nson * bit - FacLog[nson];

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
        /*    And from prior:  */
        abcost += (nson - 1) * (log(CurClass->weights_sum) + lattice) - FacLog[nson - 1];

        /*    The sons will have been processed by 'adjust_class' already, and
        this will have caused their best pcosts to be added into cls->cnpcost  */
        CurClass->dad_par_cost += abcost;
    }
}

/*    --------------------  delete_sons ------------------------------- */
/*    To delete the sons of a class  */
void delete_sons(int index) {
    Class *clp, *son;

    if (Control & AdjTr) {
        clp = CurPopln->classes[index];
        if (clp->num_sons > 0) {
            SeeAll = 4;
            for (int k = clp->son_id; k > 0; k = son->sib_id) {
                son = CurPopln->classes[k];
                son->type = Vacant;
                son->dad_id = Deadkilled;
                delete_sons(k);
            }
            clp->num_sons = 0;
            clp->son_id = -1;
            if (clp->type == Dad)
                clp->type = Leaf;
            clp->hold_type = 0;
        }
    }
}

/*    --------------------  split_leaf ------------------------   */
/*    If index is a leaf and has subs, turns it into a dad and makes
its subs into leaves.  Returns 0 if successful.  */
int split_leaf(int index) {
    Class *son;

    CurClass = CurPopln->classes[index];
    if ((CurClass->type == Leaf) && (CurClass->num_sons > 1) && !CurClass->hold_type) {
        CurClass->type = Dad;
        CurClass->hold_type = HoldTime;
        for (int k = CurClass->son_id; k >= 0; k = son->sib_id) {
            son = CurPopln->classes[k];
            son->type = Leaf;
            son->serial = 4 * CurPopln->next_serial;
            CurPopln->next_serial++;
        }
        SeeAll = 4;
        return (0);
    } else {
        return (1);
    }
}

/*    ---------------  delete_all_classes  ------------ */
/*    To remove all classes of a popln except its root  */
void delete_all_classes() {
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

/*    ------------------  next_leaf  -------------------------   */
/*    given a ptr cpop to a popln and an integer iss, returns the index
of the next leaf in the popln with serial > iss, or -1 if there is none */
/*    Intended for scanning the leaves of a popln in serial order  */

int next_leaf(Population *cpop, int iss) {
    Class *clp;
    int n = -1;
    int minSerial = 10000000;

    for (int i = 0; i <= cpop->hi_class; i++) {
        clp = cpop->classes[i];
        if (clp->type == Leaf && clp->serial > iss && clp->serial < minSerial) {
            minSerial = clp->serial;
            n = i;
        }
    }
    return n;
}
/*zzzz*/
