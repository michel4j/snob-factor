/*    Stuff dealing with classes as members of polpns     */
#define CLASSES 1
#include "glob.h"

/*    -----------------------  serial_to_id  ---------------------------   */
/*    Finds class index (id) from serial, or -3 if error    */
int serial_to_id(int ss) {
    int k;
    Class *cls;
    if (ss >= 1) {
        for (k = 0; k <= CurCtx.popln->hi_class; k++) {
            cls = CurCtx.popln->classes[k];
            if (cls && (cls->type != Vacant) && (cls->serial == ss))
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

/*    ---------------------  set_class_with_scores --------------------------   */
void set_class_with_scores(Class *cls, int item) {
    cls->case_score = Scores.CaseFacInt = cls->factor_scores[item];
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
    PVinst *pop_var_list, *pop_var;
    CVinst **cvars, *cvi;
    EVinst **evars, *stats;
    VSetVar *vset_var;
    Class *cls;
    int i, kk, found = 0, vacant = 0;

    /*    If nc, check that popln has an attached sample  */
    if (CurCtx.popln->sample_size) {
        i = find_sample(CurCtx.popln->sample_name, 1);
        if (i < 0) {
            return (-2);
        }
        CurCtx.sample = Samples[i];
    }

    pop_var_list = CurCtx.popln->variables;
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

        cls = (Class *)alloc_blocks(1, sizeof(Class));
        if (!cls) {
            return error_and_message("No space for new class");
        }

        CurCtx.popln->classes[kk] = cls;
        CurCtx.popln->hi_class = kk; /* Highest used index in population->classes */
        cls->id = kk;

        /*    Make vector of ptrs to CVinsts   */
        cvars = cls->basics = (CVinst **)alloc_blocks(1, CurCtx.vset->length * sizeof(CVinst *));
        if (!cvars) {
            return error_and_message("No space for new class");
        }

        /*    Now make the CVinst blocks, which are of varying size   */
        for (i = 0; i < CurCtx.vset->length; i++) {
            pop_var = pop_var_list + i;
            vset_var = CurCtx.vset->variables + i;
            cvi = cvars[i] = (CVinst *)alloc_blocks(1, vset_var->basic_size);
            if (!cvi) {
                return error_and_message("No space for new class");
            }
            /*    Fill in standard stuff  */
            cvi->id = vset_var->id;
        }

        /*    Make EVinst blocks and vector of pointers  */
        evars = cls->stats = (EVinst **)alloc_blocks(1, CurCtx.vset->length * sizeof(EVinst *));
        if (!evars) {
            return error_and_message("No space for new class");
        }
        for (i = 0; i < CurCtx.vset->length; i++) {
            pop_var = pop_var_list + i;
            vset_var = CurCtx.vset->variables + i;
            stats = evars[i] = (EVinst *)alloc_blocks(1, vset_var->stats_size);
            if (!stats) {
                return error_and_message("No space for new class");
            }
            stats->id = pop_var->id;
        }

        /*    Stomp on ptrs as yet undefined  */
        cls->factor_scores = 0;
        cls->type = 0;

    } else if (vacant) { /* Vacant type shows structure set up but vacated.
          Use, but set new (Vacant) type,  */
        cls = CurCtx.popln->classes[kk];
        cvars = cls->basics;
        evars = cls->stats;
        cls->type = Vacant;
    }

    cls->best_cost = cls->nofac_cost = cls->fac_cost = 0.0;
    cls->weights_sum = 0.0;
    /*    Invalidate hierarchy links  */
    cls->dad_id = cls->sib_id = cls->son_id = -1;
    cls->num_sons = 0;
    for (i = 0; i < CurCtx.vset->length; i++) {
        cvars[i]->signif = 1;
    }
    CurCtx.popln->num_classes++;
    if (kk > CurCtx.popln->hi_class) {
        CurCtx.popln->hi_class = kk;
    }
    if (cls->type != Vacant) {
        /* If nc = 0, this completes the make. */
        if (CurCtx.popln->sample_size != 0) {
            cls->factor_scores = (short *)alloc_blocks(2, CurCtx.popln->sample_size * sizeof(short));

            for (int i = 0; i < CurCtx.vset->length; i++) {
                evars[i]->num_values = 0.0;
            }
        }
    } else {
        cls->type = 0;

        for (int i = 0; i < CurCtx.vset->length; i++) {
            evars[i]->num_values = 0.0;
        }
    }

    cls->age = 0;
    cls->hold_type = cls->hold_use = 0;
    cls->weights_sum = 0.0;
    return (kk);
}

/*    -----------------------  print_class  -----------------------  */
/*    To print parameters of a class    */
/*    If kk = -1, prints all non-subs. If kk = -2, prints all  */
char *typstr[] = {"typ?", " DAD", "LEAF", "typ?", " Sub"};
char *usestr[] = {"Tny", "Pln", "Fac"};
void print_one_class(Class *cls, int full) {
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
        VarType *var_type;
        for (int i = 0; i < CurCtx.vset->length; i++) {
            var_type = CurCtx.vset->variables[i].vtype;
            (*var_type->show)(cls, i);
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
        (*(CurCtx.vset->variables[i].vtype)->clear_stats)(i, cls);
    }
}

/*    -------------------  set_best_costs  ------------------------   */
/*    To set 'b' params, costs to reflect current use   */
void set_best_costs(Class *cls) {
    int i;
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
        (*(CurCtx.vset->variables[i].vtype)->set_best_pars)(i, cls);
    }
}

/*    --------------------  score_all_vars  --------------------    */
/*    To evaluate derivs of cost wrt score in all vars of a class.
    Does one item, number case  */
/*    Leaves data values set in stats, but does no scoring if class too
young. If class age = MinFacAge, will guess scores but not cost them */
/*    If control & AdjSc, will adjust score  */

void score_all_vars(Class *cls, int item) {
    int i, igbit = 0, oldicvv = 0;
    double del;
    VSetVar *vset_var;

    set_class_with_scores(cls, item);
    if ((cls->age < MinFacAge) || (cls->use == Tiny)) {
        Scores.CurCaseFacScore = cls->avg_factor_scores = cls->sum_score_sq = 0.0;
        Scores.CaseFacInt = 0;
    } else {
        if (cls->sum_score_sq <= 0.0) {
            // Generate a fake score to get started.
            cls->boost_count = 0;
            Scores.cvvsprd = 0.1 / CurCtx.vset->length;
            oldicvv = igbit = 0;
            Scores.CurCaseFacScore = (rand_int() < 0) ? 1.0 : -1.0;
        } else {
            // Get current score
            oldicvv = Scores.CaseFacInt;
            igbit = Scores.CaseFacInt & 1;
            Scores.CurCaseFacScore = Scores.CaseFacInt * ScoreRscale;
            // Additional logic if cls has boost_count and AdjSP is set
            if (cls->boost_count && ((Control & AdjSP) == AdjSP)) {
                Scores.CurCaseFacScore *= cls->score_boost;
                Scores.CurCaseFacScore = fmax(fmin(Scores.CurCaseFacScore, Maxv), -Maxv);
                del = Scores.CurCaseFacScore * HScoreScale;
                del -= (del < 0.0) ? 1.0 : 0.0;
                Scores.CaseFacInt = del + 0.5;
                Scores.CaseFacInt <<= 1; // Round to nearest even times ScoreScale
                igbit = 0;
                Scores.CurCaseFacScore = Scores.CaseFacInt * ScoreRscale;
            }
        }

        Scores.CurCaseFacScoreSq = Scores.CurCaseFacScore * Scores.CurCaseFacScore;
        Scores.CaseFacScoreD1 = Scores.CaseFacScoreD2 = Scores.EstFacScoreD2 = Scores.CaseFacScoreD3 = 0.0;
        VarType *var_type;
        for (i = 0; i < CurCtx.vset->length; i++) {
            vset_var = CurCtx.vset->variables + i;
            if (!vset_var->inactive) {
                var_type = vset_var->vtype;
                (*var_type->score_var)(i, cls); // score_var should add to Scores.CaseFacScoreD1, Scores.CaseFacScoreD2, Scores.CaseFacScoreD3, Scores.EstFacScoreD2.
            }
        }

        Scores.CaseFacScoreD1 += Scores.CurCaseFacScore;
        Scores.CaseFacScoreD2 += 1.0;
        Scores.EstFacScoreD2 += 1.0; // From prior
        Scores.cvvsprd = 1.0 / Scores.CaseFacScoreD2;
        Scores.CaseFacScoreD1 += 0.5 * Scores.cvvsprd * Scores.CaseFacScoreD3;
        del = Scores.CaseFacScoreD1 / Scores.EstFacScoreD2;
        if (Control & AdjSc) {
            Scores.CurCaseFacScore -= del;
        }
    }

    // Code from the 'fake' label
    Scores.CurCaseFacScore = fmax(fmin(Scores.CurCaseFacScore, Maxv), -Maxv);
    del = Scores.CurCaseFacScore * HScoreScale;
    // if (del < 0.0) del -= 1.0; else del -= 0.0;
    del -= (del < 0.0) ? 1.0 : 0.0;
    Scores.CaseFacInt = del + rand_float();
    Scores.CaseFacInt <<= 1;    // Round to nearest even times ScoreScale
    Scores.CaseFacInt |= igbit; // Restore original ignore bit

    if (!igbit) {
        oldicvv -= Scores.CaseFacInt;
        oldicvv = abs(oldicvv);
        if (oldicvv > SigScoreChange)
            cls->score_change_count++;
    }

    cls->case_fac_score = Scores.CurCaseFacScore;
    cls->case_fac_score_sq = Scores.CurCaseFacScoreSq;
    cls->cvvsprd = Scores.cvvsprd;
    cls->factor_scores[item] = cls->case_score = Scores.CaseFacInt;
}

/*    ----------------------  cost_all_vars  --------------------------  */
/*    Does cost_var on all vars of class for the current item, setting
cls->casecost according to use of class  */
void cost_all_vars(Class *cls, int item) {
    int fac = 0;
    double tmp;
    VSetVar *vset_var;

    set_class_with_scores(cls, item);
    if ((cls->age >= MinFacAge) && (cls->use != Tiny)) {
        fac = 1;
        Scores.CurCaseFacScoreSq = Scores.CurCaseFacScore * Scores.CurCaseFacScore;
    }
    Scores.ncasecost = Scores.scasecost = Scores.fcasecost = cls->mlogab; /* Abundance cost */
    VarType *var_type;
    for (int iv = 0; iv < CurCtx.vset->length; iv++) {
        vset_var = CurCtx.vset->variables + iv;
        if (!(vset_var->inactive)) {
            var_type = vset_var->vtype;
            (*var_type->cost_var)(iv, fac, cls); /*    will add to scasecost, Scores.fcasecost  */
        }
    }

    cls->total_case_cost = cls->nofac_case_cost = Scores.scasecost;
    cls->fac_case_cost = Scores.scasecost + 10.0;
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
        cls->clvsprd = log(Scores.cvvsprd);
        tmp = 0.5 * (Scores.CurCaseFacScoreSq + Scores.cvvsprd - cls->clvsprd - 1.0);
        /*
            Over all scores for the class, the LATTICE effect will add approx
                ( log (2 Pi cnt)) / 2  + 1
            to the class cost. This is added later, once cnt is known.
        */
        Scores.fcasecost += tmp;
        cls->fac_case_cost = Scores.fcasecost;
        cls->coding_case_cost = tmp;
        if (cls->use == Fac)
            cls->total_case_cost = Scores.fcasecost;
    }
}

/*    ----------------------  deriv_all_vars  ---------------------    */
/*    To collect derivative statistics for all vars of a class   */
void deriv_all_vars(Class *cls, int item) {
    int fac = 0;
    VSetVar *vset_var;

    set_class_with_scores(cls, item);
    double case_weight = cls->case_weight;
    cls->newcnt += case_weight;
    if ((cls->age >= MinFacAge) && (cls->use != Tiny)) {
        fac = 1;
        Scores.CurCaseFacScore = cls->case_fac_score;
        Scores.CurCaseFacScoreSq = cls->case_fac_score_sq;
        Scores.cvvsprd = cls->cvvsprd;
        cls->newvsq += case_weight * Scores.CurCaseFacScoreSq;
        cls->vav += case_weight * cls->clvsprd;
        cls->factor_score_sum += Scores.CurCaseFacScore * case_weight;
    }

    for (int iv = 0; iv < CurCtx.vset->length; iv++) {
        vset_var = CurCtx.vset->variables + iv;
        if (!(vset_var->inactive)) {
            (*vset_var->vtype->deriv_var)(iv, fac, cls);
        }
    }

    /*    Collect case item costs   */
    cls->cstcost += case_weight * cls->nofac_case_cost;
    cls->cftcost += case_weight * cls->fac_case_cost;
    cls->cntcost += case_weight * cls->dad_case_cost;
    cls->cfvcost += case_weight * cls->coding_case_cost;
}

/*    --------------------  adjust_class  -----------------------   */
/*    To compute pcosts of a class, and if needed, adjust params  */
/*    Will process as-dad params only if 'dod'  */
void adjust_class(Class *cls, int dod) {
    int iv, fac, npars, small;
    Class *son;
    VSetVar *vset_var;
    VarType *var_type;
    double leafcost;
    Class *dad = (cls->dad_id >= 0) ? CurCtx.popln->classes[cls->dad_id] : 0;
    
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
        if (dad) {
            cls->relab = (dad->relab * (cls->weights_sum + 0.5)) / (dad->weights_sum + 0.5 * dad->num_sons);
            cls->mlogab = -log(cls->relab);
        } else {
            cls->relab = 1.0;
            cls->mlogab = 0.0;
        }
    }
    /*    But if a young subclass, make relab half of dad's  */
    if ((cls->type == Sub) && (cls->age < MinSubAge)) {
        cls->relab = 0.5 * dad->relab;
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
        vset_var = CurCtx.vset->variables + iv;
        if (!(vset_var->inactive)) {
            var_type = vset_var->vtype;
            (*var_type->adjust)(iv, fac, cls);
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
                if ((cls->use == Plain && cls->fac_cost < cls->nofac_cost) || (cls->use != Plain && cls->nofac_cost < cls->fac_cost)) {
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
            if (dad)
                dad->hold_type += 3;

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
    VarType *var_type;
    VSetVar *vset_var;

    abcost = 0.0;
    for (int i = 0; i < CurCtx.vset->length; i++) {
        vset_var = CurCtx.vset->variables + i;
        var_type = vset_var->vtype;
        (*var_type->cost_var_nonleaf)(i, valid, cls);
        stats = (EVinst *)cls->stats[i];
        if (!vset_var->inactive) {
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