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
        for (k = 0; k <= CurCtx.popln->hi_class; k++) {
            clp = CurCtx.popln->classes[k];
            if (clp && (clp->type != Vacant) && (clp->serial == ss))
                return (k);
        }
    }

    if (ss & 3) {
        log_msg(2, "No such class serial as %d %c", ss >> 2, (ss & 1) ? 'a' : 'b');
    } else {
        log_msg(2, "No such class serial as %d", ss >> 2);
    }

    return (-3);
}

/*    ---------------------  set_class --------------------------   */
void set_class(Class *cls) {
    CurClass = cls;
    CurDad = (cls->dad_id >= 0) ? CurCtx.popln->classes[cls->dad_id] : 0;
}

/*    ---------------------  set_class_with_scores --------------------------   */
void set_class_with_scores(Class *cls, int item) {
    set_class(cls);
    cls->case_score = CaseFacInt = cls->factor_scores[item];
    CurCaseWeight = cls->case_weight;
}

/*    ---------------------   make_class  -------------------------   */
/*    Makes the basic structure of a class (Class) with vector of
    ptrs to CVinsts and CVinsts filled in also EVinsts and ptrs.
    Uses nc = pop->nc.
    if nc = 0, that's it. Otherwize, if a sample is defined in pop,
    score vector.
    Returns index of class, or negative if no good  */

int error_and_message(const char *message) {
    log_msg(2, "Error: %s", message);
    return -1;
}
int make_class() {
    PVinst *pvars;
    CVinst **cvars, *cvi;
    EVinst **evars, *stats;
    int i, kk, found = 0, vacant = 0;

    /*    If nc, check that popln has an attached sample  */
    if (CurCtx.popln->sample_size) {
        i = find_sample(CurCtx.popln->sample_name, 1);
        if (i < 0) {
            return (-2);
        }
        CurCtx.sample = Samples[i];
    }

    pvars = CurCtx.popln->variables;
    /*    Find a vacant slot in the popln's classes vec   */
    for (kk = 0; kk < CurCtx.popln->cls_vec_len; kk++) {
        if (!CurCtx.popln->classes[kk]) {
            found = 1;
            break;
        }
        /*    Also test for a vacated class   */
        if (CurCtx.popln->classes[kk]->type == Vacant) {
            vacant = 1;
            break;
        }
    }
    if (!(found || vacant)) {
        return error_and_message("Popln full of classes");
    } else if (found) {

        CurClass = (Class *)alloc_blocks(1, sizeof(Class));
        if (!CurClass) {
            return error_and_message("No space for new class");
        }

        CurCtx.popln->classes[kk] = CurClass;
        CurCtx.popln->hi_class = kk; /* Highest used index in population->classes */
        CurClass->id = kk;

        /*    Make vector of ptrs to CVinsts   */
        cvars = CurClass->basics = (CVinst **)alloc_blocks(1, CurCtx.vset->length * sizeof(CVinst *));
        if (!cvars) {
            return error_and_message("No space for new class");
        }

        /*    Now make the CVinst blocks, which are of varying size   */
        for (i = 0; i < CurCtx.vset->length; i++) {
            CurPopVar = pvars + i;
            CurVSetVar = CurCtx.vset->variables + i;
            cvi = cvars[i] = (CVinst *)alloc_blocks(1, CurVSetVar->basic_size);
            if (!cvi) {
                return error_and_message("No space for new class");
            }
            /*    Fill in standard stuff  */
            cvi->id = CurVSetVar->id;
        }

        /*    Make EVinst blocks and vector of pointers  */
        evars = CurClass->stats = (EVinst **)alloc_blocks(1, CurCtx.vset->length * sizeof(EVinst *));
        if (!evars) {
            return error_and_message("No space for new class");
        }
        for (i = 0; i < CurCtx.vset->length; i++) {
            CurPopVar = pvars + i;
            CurVSetVar = CurCtx.vset->variables + i;
            stats = evars[i] = (EVinst *)alloc_blocks(1, CurVSetVar->stats_size);
            if (!stats) {
                return error_and_message("No space for new class");
            }
            stats->id = CurPopVar->id;
        }

        /*    Stomp on ptrs as yet undefined  */
        CurClass->factor_scores = 0;
        CurClass->type = 0;

    } else if (vacant) { /* Vacant type shows structure set up but vacated.
          Use, but set new (Vacant) type,  */
        CurClass = CurCtx.popln->classes[kk];
        cvars = CurClass->basics;
        evars = CurClass->stats;
        CurClass->type = Vacant;
    }

    CurClass->best_cost = CurClass->nofac_cost = CurClass->fac_cost = 0.0;
    CurClass->weights_sum = 0.0;
    /*    Invalidate hierarchy links  */
    CurClass->dad_id = CurClass->sib_id = CurClass->son_id = -1;
    CurClass->num_sons = 0;
    for (i = 0; i < CurCtx.vset->length; i++) {
        cvars[i]->signif = 1;
    }
    CurCtx.popln->num_classes++;
    if (kk > CurCtx.popln->hi_class) {
        CurCtx.popln->hi_class = kk;
    }
    if (CurClass->type != Vacant) {
        /* If nc = 0, this completes the make. */
        if (CurCtx.popln->sample_size != 0) {
            CurClass->factor_scores = (short *)alloc_blocks(2, CurCtx.popln->sample_size * sizeof(short));

            for (int i = 0; i < CurCtx.vset->length; i++) {
                evars[i]->num_values = 0.0;
            }
        }
    } else {
        CurClass->type = 0;

        for (int i = 0; i < CurCtx.vset->length; i++) {
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
void print_one_class(Class *cls, int full) {
    int i;
    double vrms;
    printf("\n--------------------------------------------------------------------------------\n");
    printf("S%s", serial_to_str(cls));
    printf(" %s", typstr[((int)cls->type)]);
    if (cls->dad_id < 0)
        printf("    Root");
    else
        printf(" Dad%s", serial_to_str(CurCtx.popln->classes[((int)cls->dad_id)]));
    printf(" Age%4d  Sz%6.1f  Use %s", cls->age, cls->weights_sum, usestr[((int)cls->use)]);
    printf("%c", (cls->use == Fac) ? ' ' : '(');
    vrms = sqrt(cls->sum_score_sq / cls->weights_sum);
    printf("Vv%6.2f", vrms);
    printf("%c", (cls->boost_count) ? 'B' : ' ');
    printf(" +-%5.3f", (cls->vav));
    printf(" Avv%6.3f", cls->avg_factor_scores);
    printf("%c", (cls->use == Fac) ? ' ' : ')');
    if (cls->type == Dad) {
        printf("%4d sons", cls->num_sons);
    }
    printf("\n");
    printf("Pcosts  S:%9.2f  F:%9.2f  D:%9.2f  B:%9.2f\n", cls->nofac_par_cost, cls->fac_par_cost, cls->dad_par_cost, cls->best_par_cost);
    printf("Tcosts  S:%9.2f  F:%9.2f  D:%9.2f  B:%9.2f\n", cls->cstcost, cls->cftcost - cls->cfvcost, cls->cntcost, cls->best_case_cost);
    printf("Vcost     ---------  F:%9.2f\n", cls->cfvcost);
    printf("totals  S:%9.2f  F:%9.2f  D:%9.2f  B:%9.2f\n", cls->nofac_cost, cls->fac_cost, cls->dad_cost, cls->best_cost);
    if (full) {
        for (i = 0; i < CurCtx.vset->length; i++) {
            CurVType = CurCtx.vset->variables[i].vtype;
            (*CurVType->show)(cls, i);
        }
    }
    printf("--------------------------------------------------------------------------------\n");
}

void print_class(int kk, int full) {
    Class *cls;
    if (kk >= 0) {
        cls = CurCtx.popln->classes[kk];
        print_one_class(cls, full);
    } else if (kk < -2) {
        log_msg(0, "%d passed to print_class", kk);
    } else {
        set_population();
        cls = CurCtx.popln->classes[CurCtx.popln->root];      
        do {
            if ((kk == -2) || (cls->type != Sub))
                print_one_class(cls, full);
            next_class(&cls);
        } while (cls);
    }
}

/*    -------------------------  clear_costs  ------  */
/*    Clears tcosts (cntcost, cftcost, cstcost). Also clears newcnt,
newvsq, and calls clear_stats for all variables   */
void clear_costs(Class *cls) {
    int i;

    set_class(cls);
    cls->mlogab = -log(cls->relab);
    cls->cstcost = cls->cftcost = cls->cntcost = 0.0;
    cls->cfvcost = 0.0;
    cls->newcnt = cls->newvsq = 0.0;
    cls->score_change_count = 0;
    cls->vav = 0.0;
    cls->factor_score_sum = 0.0;
    if (!SeeAll)
        cls->scancnt = 0;
    for (i = 0; i < CurCtx.vset->length; i++) {
        CurVType = CurCtx.vset->variables[i].vtype;
        (*CurVType->clear_stats)(i);
    }
}

/*    -------------------  set_best_costs  ------------------------   */
/*    To set 'b' params, costs to reflect current use   */
void set_best_costs(Class *cls) {
    int i;

    set_class(cls);
    if (cls->type == Dad) {
        cls->best_cost = cls->dad_cost;
        cls->best_par_cost = cls->dad_par_cost;
        cls->best_case_cost = cls->cntcost;
    } else if (cls->use != Fac) {
        cls->best_cost = cls->nofac_cost;
        cls->best_par_cost = cls->nofac_par_cost;
        cls->best_case_cost = cls->cstcost;
    } else {
        cls->best_cost = cls->fac_cost;
        cls->best_par_cost = cls->fac_par_cost;
        cls->best_case_cost = cls->cftcost;
    }
    for (i = 0; i < CurCtx.vset->length; i++) {
        CurVType = CurCtx.vset->variables[i].vtype;
        (*CurVType->set_best_pars)(i);
    }
}

/*    --------------------  score_all_vars  --------------------    */
/*    To evaluate derivs of cost wrt score in all vars of a class.
    Does one item, number case  */
/*    Leaves data values set in stats, but does no scoring if class too
young. If class age = MinFacAge, will guess scores but not cost them */
/*    If control & AdjSc, will adjust score  */

// DONT USE YET: alternative score_all_vars, testing, doesn't produce the same results
void score_all_vars_alt(Class *cls, int item) {
    double del;
    int oldicvv = 0 , igbit = 0;

    set_class_with_scores(cls, item);
    if ((cls->age < MinFacAge) || (cls->use == Tiny)) {
        CurCaseFacScore = cls->avg_factor_scores = cls->sum_score_sq = 0.0;
        CaseFacInt = 0;
    } else {
        if (cls->sum_score_sq <= 0.0) {
            //    Generate a fake score to get started.
            cls->boost_count = 0;
            cvvsprd = 0.1 / CurCtx.vset->length;
            oldicvv = igbit = 0;
            CurCaseFacScore = (rand_int() < 0) ? 1.0 : -1.0;
        } else {
            //    Get current score
            oldicvv = CaseFacInt;
            igbit = CaseFacInt & 1;
            CurCaseFacScore = CaseFacInt * ScoreRscale;
            if (cls->boost_count && ((Control & AdjSP) == AdjSP)) {
                CurCaseFacScore *= cls->score_boost;
                CurCaseFacScore = fmin(fmax(CurCaseFacScore, -Maxv), Maxv); // Clamp CurCaseFacScore to [-Maxv, Maxv]
                del = CurCaseFacScore * HScoreScale;
                del -= (del < 0) ? -1.0 : 0.0;
                CaseFacInt = (int)(del + 0.5) << 1; // Round to nearest even times ScoreScale
                igbit = 0;
                CurCaseFacScore = CaseFacInt * ScoreRscale;
            }

            CurCaseFacScoreSq = CurCaseFacScore * CurCaseFacScore;
            CaseFacScoreD1 = CaseFacScoreD2 = EstFacScoreD2 = CaseFacScoreD3 = 0.0;
            for (int i = 0; i < CurCtx.vset->length; i++) {
                CurVSetVar = CurCtx.vset->variables + i;
                if (!CurVSetVar->inactive) {
                    CurVType = CurVSetVar->vtype;
                    (*CurVType->score_var)(i); // score_var should add to CaseFacScoreD1, CaseFacScoreD2, CaseFacScoreD3, EstFacScoreD2.
                }
            }
            CaseFacScoreD1 += CurCaseFacScore;
            CaseFacScoreD2 += 1.0;
            EstFacScoreD2 += 1.0; //  From prior  There is a cost term 0.5 * cvvsprd from the prior (whence the additional 1 in CaseFacScoreD2).
                                     // Also, overall cost includes 0.5*cvvsprd*CaseFacScoreD2, so there is a derivative term wrt CurCaseFacScore of
                                     // 0.5*cvvsprd*CaseFacScoreD3
            cvvsprd = 1.0 / CaseFacScoreD2;
            CaseFacScoreD1 += 0.5 * cvvsprd * CaseFacScoreD3;
            del = CaseFacScoreD1 / EstFacScoreD2;
            if (Control & AdjSc) {
                CurCaseFacScore -= del;
            }
        }

        CurCaseFacScore = fmax(fmin(CurCaseFacScore, Maxv), -Maxv);
        del = CurCaseFacScore * HScoreScale;
        del -= ((del < 0) ? -1.0 : 0.0);
        CaseFacInt = del + rand_float();
        CaseFacInt <<= 1;    // Round to nearest even times ScoreScale
        CaseFacInt |= igbit; // Restore original ignore bit
        if (!igbit) {
            oldicvv -= CaseFacInt;
            oldicvv = abs(oldicvv);
            if (oldicvv > SigScoreChange)
                cls->score_change_count++;
        }
        cls->case_fac_score = CurCaseFacScore;
        cls->case_fac_score_sq = CurCaseFacScoreSq;
        cls->cvvsprd = cvvsprd;
    }
    cls->factor_scores[item] = cls->case_score = CaseFacInt;
}

void score_all_vars(Class *cls, int item) {
    int i, igbit = 0, oldicvv = 0;
    double del;

    set_class_with_scores(cls, item);
    if ((cls->age < MinFacAge) || (cls->use == Tiny)) {
        CurCaseFacScore = cls->avg_factor_scores = cls->sum_score_sq = 0.0;
        CaseFacInt = 0;
    } else {
        if (cls->sum_score_sq <= 0.0) {
            // Generate a fake score to get started.
            cls->boost_count = 0;
            cvvsprd = 0.1 / CurCtx.vset->length;
            oldicvv = igbit = 0;
            CurCaseFacScore = (rand_int() < 0) ? 1.0 : -1.0;
        } else {
            // Get current score
            oldicvv = CaseFacInt;
            igbit = CaseFacInt & 1;
            CurCaseFacScore = CaseFacInt * ScoreRscale;
            // Additional logic if cls has boost_count and AdjSP is set
            if (cls->boost_count && ((Control & AdjSP) == AdjSP)) {
                CurCaseFacScore *= cls->score_boost;
                CurCaseFacScore = fmax(fmin(CurCaseFacScore, Maxv), -Maxv);
                del = CurCaseFacScore * HScoreScale;
                del -= (del < 0.0) ? 1.0 : 0.0;
                CaseFacInt = del + 0.5;
                CaseFacInt <<= 1; // Round to nearest even times ScoreScale
                igbit = 0;
                CurCaseFacScore = CaseFacInt * ScoreRscale;
            }
        }

        CurCaseFacScoreSq = CurCaseFacScore * CurCaseFacScore;
        CaseFacScoreD1 = CaseFacScoreD2 = EstFacScoreD2 = CaseFacScoreD3 = 0.0;
        for (i = 0; i < CurCtx.vset->length; i++) {
            CurVSetVar = CurCtx.vset->variables + i;
            if (!CurVSetVar->inactive) {
                CurVType = CurVSetVar->vtype;
                (*CurVType->score_var)(i); // score_var should add to CaseFacScoreD1, CaseFacScoreD2, CaseFacScoreD3, EstFacScoreD2.
            }
        }

        CaseFacScoreD1 += CurCaseFacScore;
        CaseFacScoreD2 += 1.0;
        EstFacScoreD2 += 1.0; // From prior
        cvvsprd = 1.0 / CaseFacScoreD2;
        CaseFacScoreD1 += 0.5 * cvvsprd * CaseFacScoreD3;
        del = CaseFacScoreD1 / EstFacScoreD2;
        if (Control & AdjSc) {
            CurCaseFacScore -= del;
        }
    }

    // Code from the 'fake' label
    CurCaseFacScore = fmax(fmin(CurCaseFacScore, Maxv), -Maxv);
    del = CurCaseFacScore * HScoreScale;
    // if (del < 0.0) del -= 1.0; else del -= 0.0;
    del -= (del < 0.0) ? 1.0 : 0.0;
    CaseFacInt = del + rand_float();
    CaseFacInt <<= 1;    // Round to nearest even times ScoreScale
    CaseFacInt |= igbit; // Restore original ignore bit

    if (!igbit) {
        oldicvv -= CaseFacInt;
        oldicvv = abs(oldicvv);
        if (oldicvv > SigScoreChange)
            cls->score_change_count++;
    }

    cls->case_fac_score = CurCaseFacScore;
    cls->case_fac_score_sq = CurCaseFacScoreSq;
    cls->cvvsprd = cvvsprd;

    cls->factor_scores[item] = cls->case_score = CaseFacInt;
}

/*    ----------------------  cost_all_vars  --------------------------  */
/*    Does cost_var on all vars of class for the current item, setting
cls->casecost according to use of class  */
void cost_all_vars(Class *cls, int item) {
    int fac = 0;
    double tmp;

    set_class_with_scores(cls, item);
    if ((cls->age >= MinFacAge) && (cls->use != Tiny)) {
        fac = 1;
        CurCaseFacScoreSq = CurCaseFacScore * CurCaseFacScore;
    }
    ncasecost = scasecost = fcasecost = cls->mlogab; /* Abundance cost */
    for (int iv = 0; iv < CurCtx.vset->length; iv++) {
        CurVSetVar = CurCtx.vset->variables + iv;
        if (!(CurVSetVar->inactive)) {
            CurVType = CurVSetVar->vtype;
            (*CurVType->cost_var)(iv, fac); /*    will add to scasecost, fcasecost  */
        }
    }

    cls->total_case_cost = cls->nofac_case_cost = scasecost;
    cls->fac_case_cost = scasecost + 10.0;
    if (cls->num_sons < 2)
        cls->dad_case_cost = 0.0;
    cls->coding_case_cost = 0.0;
    if (fac) {
        /*    Now we add the cost of coding the score, and its roundoff */
        /*    The simple form for costing a score is :
            tmp = HALF_LOG_2PI + 0.5 * (case_fac_score_sq + cvvsprd - log (cvvsprd)) + LATTICE;
        However, we appeal to the large number of score parameters, which gives a
        more negative 'LATTICE' ((log 12)/2 for one parameter) approaching -(1/2)
        log (2 Pi e) which results in the reduced cost :  */
        cls->clvsprd = log(cvvsprd);
        tmp = 0.5 * (CurCaseFacScoreSq + cvvsprd - cls->clvsprd - 1.0);
        /*
            Over all scores for the class, the LATTICE effect will add approx
                ( log (2 Pi cnt)) / 2  + 1
            to the class cost. This is added later, once cnt is known.
        */
        fcasecost += tmp;
        cls->fac_case_cost = fcasecost;
        cls->coding_case_cost = tmp;
        if (cls->use == Fac)
            cls->total_case_cost = fcasecost;
    }
}

/*    ----------------------  deriv_all_vars  ---------------------    */
/*    To collect derivative statistics for all vars of a class   */
void deriv_all_vars(Class *cls, int item) {
    int fac = 0;
    VSetVar *attr;

    set_class_with_scores(cls, item);
    cls->newcnt += CurCaseWeight;
    if ((cls->age >= MinFacAge) && (cls->use != Tiny)) {
        fac = 1;
        CurCaseFacScore = cls->case_fac_score;
        CurCaseFacScoreSq = cls->case_fac_score_sq;
        cvvsprd = cls->cvvsprd;
        cls->newvsq += CurCaseWeight * CurCaseFacScoreSq;
        cls->vav += CurCaseWeight * cls->clvsprd;
        cls->factor_score_sum += CurCaseFacScore * CurCaseWeight;
    }

    for (int iv = 0; iv < CurCtx.vset->length; iv++) {
        attr = CurCtx.vset->variables + iv;
        if (!(attr->inactive)) {
            (*attr->vtype->deriv_var)(iv, fac);
        }
    }

    /*    Collect case item costs   */
    cls->cstcost += CurCaseWeight * cls->nofac_case_cost;
    cls->cftcost += CurCaseWeight * cls->fac_case_cost;
    cls->cntcost += CurCaseWeight * cls->dad_case_cost;
    cls->cfvcost += CurCaseWeight * cls->coding_case_cost;
}

/*    --------------------  adjust_class  -----------------------   */
/*    To compute pcosts of a class, and if needed, adjust params  */
/*    Will process as-dad params only if 'dod'  */
void adjust_class(Class *cls, int dod) {
    int iv, fac, npars, small;
    Class *son;
    VSetVar *attr;
    double leafcost;

    set_class(cls);
    /*    Get root (logarithmic average of vvsprds)  */
    cls->vav = exp(0.5 * cls->vav / (cls->newcnt + 0.1));
    if (Control & AdjSc)
        cls->sum_score_sq = cls->newvsq;
    if (Control & AdjPr) {
        cls->weights_sum = cls->newcnt;
        /*    Count down holds   */
        if ((cls->hold_type) && (cls->hold_type < Forever))
            cls->hold_type--;
        if ((cls->hold_use) && (cls->hold_use < Forever))
            cls->hold_use--;
        if (CurDad) {
            cls->relab = (CurDad->relab * (cls->weights_sum + 0.5)) / (CurDad->weights_sum + 0.5 * CurDad->num_sons);
            cls->mlogab = -log(cls->relab);
        } else {
            cls->relab = 1.0;
            cls->mlogab = 0.0;
        }
    }
    /*    But if a young subclass, make relab half of dad's  */
    if ((cls->type == Sub) && (cls->age < MinSubAge)) {
        cls->relab = 0.5 * CurDad->relab;
    }

    if ((cls->age < MinFacAge) || (cls->use == Tiny))
        fac = 0;
    else
        fac = 1;

    /*    Set npars to show if class may be treated as a dad  */
    npars = 1;
    if ((cls->age < MinAge) || (cls->num_sons < 2) || (CurCtx.popln->classes[cls->son_id]->age < MinSubAge))
        npars = 0;
    if (cls->type == Dad)
        npars = 1;

    /*    cls->cnpcost was zeroed in do_all, and has accumulated the cbpcosts
    of cls's sons, so we don't zero it here. 'parent_cost_all_vars' will add to it.  */
    cls->nofac_par_cost = cls->fac_par_cost = 0.0;

    for (iv = 0; iv < CurCtx.vset->length; iv++) {
        attr = CurCtx.vset->variables + iv;
        if (!(attr->inactive)) {
            //CurVType = CurVSetVar->vtype;
            (*attr->vtype->adjust)(iv, fac);
        }
    }

    /*    If vsq less than 0.3, set vboost to inflate  */
    /*    but only if both scores and params are being adjusted  */
    if (((Control & AdjSP) == AdjSP) && fac && (cls->sum_score_sq < (0.3 * cls->weights_sum))) {
        cls->score_boost = sqrt((1.0 * cls->weights_sum) / (cls->sum_score_sq + 1.0));
        cls->boost_count++;
    } else {
        cls->score_boost = 1.0;
        cls->boost_count = 0;
    }

    /*    Get average score   */
    cls->avg_factor_scores = cls->factor_score_sum / cls->weights_sum;
    if (dod)
        parent_cost_all_vars(cls, npars);

    cls->nofac_cost = cls->nofac_par_cost + cls->cstcost;
    /*    The 'lattice' effect on the cost of coding scores is approx
        (log (2 Pi cnt))/2 + 1,  which adds to cftcost  */
    cls->cftcost += 0.5 * log(cls->newcnt + 1.0) + HALF_LOG_2PI + 1.0;
    cls->fac_cost = cls->fac_par_cost + cls->cftcost;
    if (npars)
        cls->dad_cost = cls->dad_par_cost + cls->cntcost;
    else
        cls->dad_cost = cls->dad_par_cost = cls->cntcost = 0.0;

    /*    Contemplate changes to class use and type   */
    if ((!cls->hold_use) && (Control & AdjPr)) {
        /*    If scores boosted too many times, make Tiny and hold   */
        if (cls->boost_count > 20) {
            cls->use = Tiny;
            cls->hold_use = 10;
        } else {
            /*    Check if size too small to support a factor  */
            /*    Add up the number of data values  */
            small = 0;
            leafcost = 0.0; /* Used to add up stats->cnts */
            for (iv = 0; iv < CurCtx.vset->length; iv++) {
                if (!(CurCtx.vset->variables[iv].inactive)) {
                    small++;
                    leafcost += cls->stats[iv]->num_values;
                }
            }
            /*    I want at least 1.5 data vals per param  */
            small = (leafcost < (9 * small + 1.5 * cls->weights_sum + 1)) ? 1 : 0;
            if (small) {
                if (cls->use != Tiny) {
                    cls->use = Tiny;
                }
            } else if (cls->use == Tiny) {
                cls->use = Plain;
                cls->sum_score_sq = 0.0;
            } else if (cls->age >= MinFacAge) {
                if ((cls->use == Plain && cls->fac_cost < cls->nofac_cost) ||
                    (cls->use != Plain && cls->nofac_cost < cls->fac_cost)) {
                    cls->use = (cls->use == Plain) ? Fac : Plain;
                    if (cls->use == Fac) {
                        cls->boost_count = 0;
                        cls->score_boost = 1.0;
                    }
                }
            }
        }
    }

    if (dod && !(cls->hold_type) && (Control & AdjTr) && (cls->num_sons >= 2)) {
        leafcost = (cls->use == Fac) ? cls->fac_cost : cls->nofac_cost;

        if ((cls->type == Dad) && (leafcost < cls->dad_cost) && (Fix != Random)) {

            log_msg(0, "\nChanging type of class %s from DAD to LEAF", serial_to_str(cls));
            SeeAll = 4;
            delete_sons(cls->id); // Changes type to leaf
        } else if (npars && (leafcost > cls->dad_cost) && (cls->type == Leaf)) {
            log_msg(0, "\nChanging type of class %s from LEAF to DAD", serial_to_str(cls));
            SeeAll = 4;
            cls->type = Dad;
            if (CurDad)
                CurDad->hold_type += 3;

            for (int i = 0; i < 2; i++) {
                son = CurCtx.popln->classes[i == 0 ? cls->son_id : son->sib_id];
                son->type = Leaf;
                son->serial = 4 * CurCtx.popln->next_serial++;
            }
        }
    }

    set_best_costs(cls);

    if (Control & AdjPr)
        cls->age++;
}

/*    ----------------  parent_cost_all_vars  ------------------------------   */
/*    To do parent costing on all vars of a class        */
/*    If not 'valid', don't cost, and fake params  */
void parent_cost_all_vars(Class *cls, int valid) {
    Class *son;
    EVinst *stats;
    int ison, nson;
    double abcost, rrelab;

    set_class(cls);
    abcost = 0.0;
    for (int i = 0; i < CurCtx.vset->length; i++) {
        CurVSetVar = CurCtx.vset->variables + i;
        CurVType = CurVSetVar->vtype;
        (*CurVType->cost_var_nonleaf)(i, valid);
        stats = (EVinst *)cls->stats[i];
        if (!CurVSetVar->inactive) {
            abcost += stats->npcost;
        }
    }

    if (!valid) {
        cls->dad_par_cost = cls->dad_cost = 0.0;
    } else {
        nson = cls->num_sons;
        /*    The sons of a dad may be listed in any order, so the param cost
        of the dad can be reduced by log (nson !)  */
        /*    The cost of saying 'dad' and number of sons is set at nson bits. */
        abcost += nson * BIT - FacLog[nson];

        /*    Now add cost of specifying the relabs of the sons.  */
        /*    Their relabs are absolute, but we specify them as fractions of this
        dad's relab. The cost includes -0.5 * Sum_sons { log (sonab / dadab) }
            */
        rrelab = 1.0 / cls->relab;
        for (ison = cls->son_id; ison >= 0; ison = son->sib_id) {
            son = CurCtx.popln->classes[ison];
            abcost -= 0.5 * log(son->relab * rrelab);
        }
        /*    Add other terms from Fisher  */
        /*    And from prior:  */
        abcost += (nson - 1) * (log(cls->weights_sum) + LATTICE) - FacLog[nson - 1];

        /*    The sons will have been processed by 'adjust_class' already, and
        this will have caused their best pcosts to be added into cls->cnpcost  */
        cls->dad_par_cost += abcost;
    }
}

/*    --------------------  delete_sons ------------------------------- */
/*    To delete the sons of a class  */
void delete_sons(int index) {
    Class *clp, *son;

    if (Control & AdjTr) {
        clp = CurCtx.popln->classes[index];
        if (clp->num_sons > 0) {
            SeeAll = 4;
            for (int k = clp->son_id; k > 0; k = son->sib_id) {
                son = CurCtx.popln->classes[k];
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
    Class *son, *cls;

    cls = CurCtx.popln->classes[index];
    if ((cls->type == Leaf) && (cls->num_sons > 1) && !cls->hold_type) {
        cls->type = Dad;
        cls->hold_type = HoldTime;
        for (int k = cls->son_id; k >= 0; k = son->sib_id) {
            son = CurCtx.popln->classes[k];
            son->type = Leaf;
            son->serial = 4 * CurCtx.popln->next_serial;
            CurCtx.popln->next_serial++;
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
    int k, root_id;
    Class *cls;
    root_id = CurCtx.popln->root;
    for (k = 0; k <= CurCtx.popln->hi_class; k++) {
        cls = CurCtx.popln->classes[k];
        if (cls->id != root_id) {
            cls->type = Vacant;
            cls->dad_id = Deadkilled;
        }
    }
    CurCtx.popln->num_classes = 1;
    CurCtx.popln->hi_class = root_id;
    
    cls = CurCtx.popln->classes[root_id];
    cls->son_id = -1;
    cls->sib_id = -1;
    cls->num_sons = 0;
    cls->serial = 4;
    CurCtx.popln->next_serial = 2;
    cls->age = 0;
    cls->hold_type = cls->hold_use = 0;
    cls->type = Leaf;
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
