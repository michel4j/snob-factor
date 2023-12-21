/*	Stuff dealing with classes as members of polpns     */
#include "glob.h"

/*	-----------------------  s2id  ---------------------------   */
/*	Finds class index (id) from serial, or -3 if error    */
int serial_to_id(int ss) {
    int k;
    Class *cls;
    Population *popln = CurCtx.popln;

    if (ss >= 1) {
        for (k = 0; k <= popln->hi_class; k++) {
            cls = popln->classes[k];
            if (cls && (cls->type != Vacant) && (cls->serial == ss))
                return (k);
        }
    }
    char suffix = ' ';
    if (ss & 3)
        suffix = (ss & 1) ? 'a' : 'b';
    log_msg(1, "No such class serial as %d %c", ss >> 2, suffix);
    return (-3);
}

/*	---------------------  set_class_score --------------------------   */
void set_class_score(Class *cls, int item) { 
    cls->case_score = Scores.CaseFacInt = cls->factor_scores[item]; 
}

/*	---------------------   makeclass  -------------------------   */
/*	Makes the basic structure of a class (Class) with vector of
ptrs to CVinsts and CVinsts filled in also EVinsts and ptrs.
    Uses nc = pop->nc.
    if nc = 0, that's it. Otherwize, if a sample is defined in pop,
    score vector.
    Returns index of class, or negative if no good  */

int make_class() {
    PopVar *pop_var;
    ClassVar **cls_var_list, *cls_var;
    ExplnVar **exp_var_list, *exp_var;
    Class *cls;
    int i, kk, found = -1;
    Population *popln = CurCtx.popln;
    VSetVar *vset_var;
    int num_cases = (CurCtx.popln) ? CurCtx.popln->sample_size : 0;

    /*	If nc, check that popln has an attached sample  */
    if (num_cases) {
        i = find_sample(popln->sample_name, 1);
        if (i < 0)
            return (-2);
        CurCtx.sample = Samples[i];
    }

    /*	Find a vacant slot in the popln's classes vec   */
    for (kk = 0; kk < popln->cls_vec_len; kk++) {
        if ((!popln->classes[kk]) || (popln->classes[kk]->type == Vacant)) { /*	Also test for a vacated class   */
            found = kk;
            break;
        }
    }
    if (found < 0) {
        return error_value("Popln full of classes\n", -1);
    } else if (popln->classes[found]) {
        /* Vacant type shows structure set up but vacated.  Use, but set new (Vacant) type,  */
        cls = popln->classes[found];
        cls_var_list = cls->basics;
        exp_var_list = cls->stats;
        cls->type = Vacant;
    } else {
        cls = (Class *)alloc_blocks(1, sizeof(Class));
        if (!cls)
            return error_value("Popln full of classes\n", -1);
        popln->classes[found] = cls;
        popln->hi_class = found; /* Highest used index in population->classes */
        cls->id = found;

        /*	Make vector of ptrs to CVinsts   */
        cls_var_list = cls->basics = (ClassVar **)alloc_blocks(1, CurCtx.vset->length * sizeof(ClassVar *));
        if (!cls_var_list)
            return error_value("Popln full of classes\n", -1);

        /*	Now make the ClassVar blocks, which are of varying size   */
        for (i = 0; i < CurCtx.vset->length; i++) {
            vset_var = &CurCtx.vset->variables[i];
            cls_var = cls_var_list[i] = (ClassVar *)alloc_blocks(1, vset_var->basic_size);
            if (!cls_var)
                return error_value("Popln full of classes\n", -1);
            /*	Fill in standard stuff  */
            cls_var->id = vset_var->id;
        }

        /*	Make ExplnVar blocks and vector of pointers  */
        exp_var_list = cls->stats = (ExplnVar **)alloc_blocks(1, CurCtx.vset->length * sizeof(ExplnVar *));
        if (!exp_var_list)
            return error_value("Popln full of classes\n", -1);

        for (i = 0; i < CurCtx.vset->length; i++) {
            pop_var = &popln->variables[i];
            vset_var = &CurCtx.vset->variables[i];
            exp_var = exp_var_list[i] = (ExplnVar *)alloc_blocks(1, vset_var->stats_size);
            if (!exp_var)
                return error_value("Popln full of classes\n", -1);
            exp_var->id = pop_var->id;
        }

        /*	Stomp on ptrs as yet undefined  */
        cls->factor_scores = 0;
        cls->type = 0;
    }

    cls->best_cost = cls->nofac_cost = cls->fac_cost = 0.0;
    cls->weights_sum = 0.0;
    /*	Invalidate hierarchy links  */
    cls->dad_id = cls->sib_id = cls->son_id = -1;
    cls->num_sons = 0;
    for (i = 0; i < CurCtx.vset->length; i++) {
        cls_var_list[i]->signif = 1;
    }
    popln->num_classes++;
    if (found > popln->hi_class) {
        popln->hi_class = found;
    }
    if (cls->type != Vacant) {
        /*	If nc = 0, this completes the make.  */
        if (num_cases) {
            cls->factor_scores = (short *)alloc_blocks(2, num_cases * sizeof(short));
        }
    } else {
        cls->type = 0;
    }
    if (num_cases) {
        for (i = 0; i < CurCtx.vset->length; i++)
            exp_var_list[i]->num_values = 0.0;
    }
    cls->age = 0;
    cls->hold_type = cls->hold_use = 0;
    cls->weights_sum = 0.0;
    return (found);
}

/*	-----------------------  printclass  -----------------------  */
/*	To print parameters of a class    */
/*	If kk = -1, prints all non-subs. If kk = -2, prints all  */
const char *typstr[] = {"typ?", " DAD", "LEAF", "typ?", " Sub"};
const char *usestr[] = {"Tny", "Pln", "Fac"};
void print_one_class(Class *cls, int full) {
    int i;
    double vrms;
    Population *popln = CurCtx.popln;
    VarType *vtype;

    printf("\nS%s", serial_to_str(cls));
    printf(" %s", typstr[((int)cls->type)]);
    if (cls->dad_id < 0) {
        printf("    Root");
    } else {
        printf(" Dad%s", serial_to_str(popln->classes[((int)cls->dad_id)]));
    }
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
            vtype = CurCtx.vset->variables[i].vtype;
            (*vtype->show)(cls, i);
        }
    }
}

void print_class(int n, int full) {
    Class *cls;
    Class *root = CurCtx.popln->classes[CurCtx.popln->root];
    if (n >= 0) {
        cls = CurCtx.popln->classes[n];
        print_one_class(cls, full);
    } else if (n < -2) {
        printf("%d passed to printclass\n", n);
    } else {
        cls = root;
        do {
            if ((n == -2) || (cls->type != Sub)) {
                print_one_class(cls, full);
            }
            next_class(&cls);
        } while (cls);
    }
}

void get_details_for(Class *cls, MemBuffer *buffer) {
    int i;
    VarType *vtype;
    double vrms;
    Class *dad = (cls->dad_id >= 0) ? CurCtx.popln->classes[cls->dad_id] : 0;
    
    vrms = sqrt(cls->sum_score_sq / cls->weights_sum);
    print_buffer(buffer,  "{\"id\": %d, ", cls->serial);
    print_buffer(buffer,  "\"type\": %d,", ((int)cls->type));
    print_buffer(buffer,  "\"parent\": %d, ", (dad) ? dad->serial: -1);
    print_buffer(buffer,  "\"factor\": %s, ", (cls->use == 2) ? "true": "false");
    print_buffer(buffer,  "\"strength\": %0.3f, ", vrms);
    print_buffer(buffer,  "\"tiny\": %s, ", (cls->use == 0) ? "true": "false");
    print_buffer(buffer,  "\"age\": %4d, \"size\": %0.1f, ", cls->age, cls->weights_sum);
    print_buffer(buffer,  "\"costs\": {\"model\": %0.2f, \"data\": %0.2f, \"total\": %0.2f}, ", cls->best_par_cost, cls->best_case_cost, cls->best_cost);
    print_buffer(buffer, "\"attrs\": [");
    for (i = 0; i < CurCtx.vset->length; i++) {
        if (i > 0) {
            print_buffer(buffer, ", ");
        }
        vtype = CurCtx.vset->variables[i].vtype;
        (*vtype->details)(cls, i, buffer);
        
    }
    print_buffer(buffer, "]}");
}

void get_class_details(char *buffer, size_t buffer_size) {
    MemBuffer dest;

    dest.buffer = buffer;
    dest.size = buffer_size;
    dest.offset = 0;
    int start = 0;
    Class *cls = CurCtx.popln->classes[CurCtx.popln->root];
    print_buffer(&dest, "[");
    do {
        if (cls->type != Sub) {
            if (start > 0) {
                print_buffer(&dest, ", ");
            }
            get_details_for(cls, &dest);
            start += 1;
        }
        next_class(&cls);
    } while (cls);
    print_buffer(&dest, "]");
}

/*	-------------------------  cleartcosts  ------  */
/*	Clears tcosts (cntcost, cftcost, cstcost). Also clears newcnt,
newvsq, and calls clear_stats for all variables   */
void clear_costs(Class *cls) {
    int i;
    VarType *vtype;

    cls->mlogab = -log(cls->relab);
    cls->cstcost = cls->cftcost = cls->cntcost = 0.0;
    cls->cfvcost = 0.0;
    cls->newcnt = cls->newvsq = 0.0;
    cls->score_change_count = 0;
    cls->vav = 0.0;
    cls->totvv = 0.0;
    if (!SeeAll)
        cls->scancnt = 0;
    for (i = 0; i < CurCtx.vset->length; i++) {
        vtype = CurCtx.vset->variables[i].vtype;
        (*vtype->clear_stats)(i, cls);
    }
    return;
}

/*	-------------------  setbestparall  ------------------------   */
/*	To set 'b' params, costs to reflect current use   */
void set_best_costs(Class *cls) {
    int i;
    VarType *vtype;

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
        vtype = CurCtx.vset->variables[i].vtype;
        (*vtype->set_best_pars)(i, cls);
    }
    return;
}

/*	--------------------  scorevarall  --------------------    */
/*	To evaluate derivs of cost wrt score in all vars of a class.
    Does one item, number case  */
/*	Leaves data values set in stats, but does no scoring if class too
young. If class age = MinFacAge, will guess scores but not cost them */
/*	If control & AdjSc, will adjust score  */
void score_all_vars(Class *cls, int item) {
    int i, igbit, oldicvv;
    double del;
    VSetVar *vset_var;
    VarType *vtype;

    set_class_score(cls, item);
    if ((cls->age < MinFacAge) || (cls->use == Tiny)) {
        Scores.CaseFacScore = cls->avg_factor_scores = cls->sum_score_sq = 0.0;
        Scores.CaseFacInt = 0;
    } else {
        if (cls->sum_score_sq <= 0.0) {
            /*	Generate a fake score to get started.   */
            cls->boost_count = 0;
            Scores.cvvsprd = 0.1 / CurCtx.vset->length;
            oldicvv = igbit = 0;
            Scores.CaseFacScore = (rand_int() < 0) ? 1.0 : -1.0;
        } else {
            /*	Get current score  */
            oldicvv = Scores.CaseFacInt;
            igbit = Scores.CaseFacInt & 1;
            Scores.CaseFacScore = Scores.CaseFacInt * ScoreRScale;
            /*	Subtract average from last pass  */
            /*xx
                cvv -= cls->avg_factor_scores;
            */
            if (cls->boost_count && ((Control & AdjSP) == AdjSP)) {
                Scores.CaseFacScore *= cls->score_boost;
                Scores.CaseFacScore = fmax(fmin(Scores.CaseFacScore, Maxv), -Maxv);
                del = Scores.CaseFacScore * HScoreScale;
                del -= (del < 0.0) ? 1.0 : 0.0;
                Scores.CaseFacInt = del + 0.5;
                Scores.CaseFacInt = Scores.CaseFacInt << 1; /* Round to nearest even times ScoreScale */
                igbit = 0;
                Scores.CaseFacScore = Scores.CaseFacInt * ScoreRScale;
            }

            Scores.CaseFacScoreSq = Scores.CaseFacScore * Scores.CaseFacScore;
            Scores.CaseFacScoreD1 = Scores.CaseFacScoreD2 = Scores.EstFacScoreD2 = Scores.CaseFacScoreD3 = 0.0;
            for (i = 0; i < CurCtx.vset->length; i++) {
                vset_var = &CurCtx.vset->variables[i];
                if (!vset_var->inactive) {
                    vtype = vset_var->vtype;
                    (*vtype->score_var)(i, cls); /*	score_var should add to vvd1, vvd2, vvd3, mvvd2.  */
                }
            }
            Scores.CaseFacScoreD1 += Scores.CaseFacScore;
            Scores.CaseFacScoreD2 += 1.0;
            Scores.EstFacScoreD2 += 1.0; /*  From prior  */
            // There is a cost term 0.5 * cvvsprd from the prior (whence the additional
            // 1 in vvd2).
            Scores.cvvsprd = 1.0 / Scores.CaseFacScoreD2;
            // Also, overall cost includes 0.5*cvvsprd*vvd2, so there is a derivative
            // term wrt cvv of 0.5*cvvsprd*vvd3
            Scores.CaseFacScoreD1 += 0.5 * Scores.cvvsprd * Scores.CaseFacScoreD3;
            del = Scores.CaseFacScoreD1 / Scores.EstFacScoreD2;
            if (Control & AdjSc) {
                Scores.CaseFacScore -= del;
            }
        }

        Scores.CaseFacScore = fmax(fmin(Scores.CaseFacScore, Maxv), -Maxv);
        del = Scores.CaseFacScore * HScoreScale;
        del -= (del < 0.0) ? 1.0 : 0.0;
        Scores.CaseFacInt = del + rand_float();
        Scores.CaseFacInt = Scores.CaseFacInt << 1; /* Round to nearest even times ScoreScale */
        Scores.CaseFacInt |= igbit;                 /* Restore original ignore bit */
        if (!igbit) {
            oldicvv -= Scores.CaseFacInt;
            oldicvv = (oldicvv < 0) ? -oldicvv : oldicvv;
            if (oldicvv > SigScoreChange)
                cls->score_change_count++;
        }
        cls->case_fac_score = Scores.CaseFacScore = Scores.CaseFacInt * ScoreRScale;
        cls->case_fac_score_sq = Scores.CaseFacScoreSq = Scores.CaseFacScore * Scores.CaseFacScore;
        cls->cvvsprd = Scores.cvvsprd;
    }
    cls->factor_scores[item] = cls->case_score = Scores.CaseFacInt;
}

/*	----------------------  costvarall  --------------------------  */
/*	Does cost_var on all vars of class for the current item, setting
cls->casecost according to use of class  */
void cost_all_vars(Class *cls, int item) {
    int fac;
    double tmp;
    VSetVar *vset_var;
    VarType *vtype;

    set_class_score(cls, item);
    if ((cls->age < MinFacAge) || (cls->use == Tiny))
        fac = 0;
    else {
        fac = 1;
        Scores.CaseFacScoreSq = Scores.CaseFacScore * Scores.CaseFacScore;
    }
    Scores.CaseCost = Scores.CaseNoFacCost = Scores.CaseFacCost = cls->mlogab; /* Abundance cost */
    for (int iv = 0; iv < CurCtx.vset->length; iv++) {
        vset_var = &CurCtx.vset->variables[iv];
        if (!vset_var->inactive) {
            vtype = vset_var->vtype;
            (*vtype->cost_var)(iv, fac, cls); /* will add to CaseNoFacCost, CaseFacCost  */
        }
    }

    cls->total_case_cost = cls->nofac_case_cost = Scores.CaseNoFacCost;
    cls->fac_case_cost = Scores.CaseNoFacCost + 10.0;
    if (cls->num_sons < 2)
        cls->dad_case_cost = 0.0;
    cls->coding_case_cost = 0.0;
    if (fac) {
        // Now we add the cost of coding the score, and its roundoff
        // The simple form for costing a score is :
        //      tmp = hlg2pi + 0.5 * (cvvsq + cvvsprd - log (cvvsprd)) + lattice;
        // However, we appeal to the large number of score parameters, which gives a
        // more negative 'lattice' ((log 12)/2 for one parameter) approaching -(1/2)
        // log (2 Pi e) which results in the reduced cost :
        cls->clvsprd = log(Scores.cvvsprd);
        tmp = 0.5 * (Scores.CaseFacScoreSq + Scores.cvvsprd - cls->clvsprd - 1.0);
        // Over all scores for the class, the lattice effect will add approx
        //         ( log (2 Pi cnt)) / 2  + 1
        // to the class cost. This is added later, once cnt is known.
        Scores.CaseFacCost += tmp;
        cls->fac_case_cost = Scores.CaseFacCost;
        cls->coding_case_cost = tmp;
        if (cls->use == Fac)
            cls->total_case_cost = Scores.CaseFacCost;
    }
}

/*	----------------------  derivvarall  ---------------------    */
/*	To collect derivative statistics for all vars of a class   */
void deriv_all_vars(Class *cls, int item) {
    int fac;
    VSetVar *vset_var;
    VarType *vtype;
    const double case_weight = cls->case_weight;

    set_class_score(cls, item);
    cls->newcnt += case_weight;
    if ((cls->age < MinFacAge) || (cls->use == Tiny))
        fac = 0;
    else {
        fac = 1;
        Scores.CaseFacScore = cls->case_fac_score;
        Scores.CaseFacScoreSq = cls->case_fac_score_sq;
        Scores.cvvsprd = cls->cvvsprd;
        cls->newvsq += case_weight * Scores.CaseFacScoreSq;
        cls->vav += case_weight * cls->clvsprd;
        cls->totvv += Scores.CaseFacScore * case_weight;
    }
    for (int iv = 0; iv < CurCtx.vset->length; iv++) {
        vset_var = &CurCtx.vset->variables[iv];
        if (!vset_var->inactive) {
            vtype = vset_var->vtype;
            (*vtype->deriv_var)(iv, fac, cls);
        }
    }

    /*	Collect case item costs   */
    cls->cstcost += case_weight * cls->nofac_case_cost;
    cls->cftcost += case_weight * cls->fac_case_cost;
    cls->cntcost += case_weight * cls->dad_case_cost;
    cls->cfvcost += case_weight * cls->coding_case_cost;
}

/*	--------------------  adjustclass  -----------------------   */
/*	To compute pcosts of a class, and if needed, adjust params  */
/*	Will process as-dad params only if 'dod'  */
void adjust_class(Class *cls, int dod) {
    int iv, fac, npars, small;
    Class *son;
    double leafcost;
    Population *popln = CurCtx.popln;
    VSetVar *vset_var;
    VarType *vtype;
    Class *dad = (cls->dad_id >= 0) ? popln->classes[cls->dad_id] : 0;

    /*	Get root (logarithmic average of vvsprds)  */
    cls->vav = exp(0.5 * cls->vav / (cls->newcnt + 0.1));
    if (Control & AdjSc)
        cls->sum_score_sq = cls->newvsq;
    if (Control & AdjPr) {
        cls->weights_sum = cls->newcnt;
        /*	Count down holds   */
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
    /*	But if a young subclass, make relab half of dad's  */
    if ((dad) && (cls->type == Sub) && (cls->age < MinSubAge)) {
        cls->relab = 0.5 * dad->relab;
    }

    if ((cls->age < MinFacAge) || (cls->use == Tiny))
        fac = 0;
    else
        fac = 1;

    /*	Set npars to show if class may be treated as a dad  */
    npars = 1;
    if ((cls->age < MinAge) || (cls->num_sons < 2) || (popln->classes[cls->son_id]->age < MinSubAge))
        npars = 0;
    if (cls->type == Dad)
        npars = 1;

    /*	cls->cnpcost was zeroed in doall, and has accumulated the cbpcosts
    of cls's sons, so we don't zero it here. 'ncostvarall' will add to it.  */
    cls->nofac_par_cost = cls->fac_par_cost = 0.0;
    for (iv = 0; iv < CurCtx.vset->length; iv++) {
        vset_var = &CurCtx.vset->variables[iv];
        if (!vset_var->inactive) {
            vtype = vset_var->vtype;
            (*vtype->adjust)(iv, fac, cls);
        }
    }

    /*	If vsq less than 0.3, set vboost to inflate  */
    /*	but only if both scores and params are being adjusted  */
    if (((Control & AdjSP) == AdjSP) && fac && (cls->sum_score_sq < (0.3 * cls->weights_sum))) {
        cls->score_boost = sqrt((1.0 * cls->weights_sum) / (cls->sum_score_sq + 1.0));
        cls->boost_count++;
    } else {
        cls->score_boost = 1.0;
        cls->boost_count = 0;
    }

    /*	Get average score   */
    cls->avg_factor_scores = cls->totvv / cls->weights_sum;

    if (dod) {
        parent_cost_all_vars(cls, npars);
    }
    cls->nofac_cost = cls->nofac_par_cost + cls->cstcost;
    /*	The 'lattice' effect on the cost of coding scores is approx
        (log (2 Pi cnt))/2 + 1,  which adds to cftcost  */
    cls->cftcost += 0.5 * log(cls->newcnt + 1.0) + HALF_LOG_2PI + 1.0;
    cls->fac_cost = cls->fac_par_cost + cls->cftcost;
    if (npars)
        cls->dad_cost = cls->dad_par_cost + cls->cntcost;
    else
        cls->dad_cost = cls->dad_par_cost = cls->cntcost = 0.0;

    /*	Contemplate changes to class use and type   */
    if (!cls->hold_use && (Control & AdjPr)) {
        /*	If scores boosted too many times, make Tiny and hold   */
        if (cls->boost_count > 20) {
            cls->use = Tiny;
            cls->hold_use = 10;
        } else {
            /*	Check if size too small to support a factor  */
            /*	Add up the number of data values  */
            small = 0;
            leafcost = 0.0; /* Used to add up exp_var->cnts */
            for (iv = 0; iv < CurCtx.vset->length; iv++) {
                if (!CurCtx.vset->variables[iv].inactive) {
                    small++;
                    leafcost += cls->stats[iv]->num_values;
                }
            }
            /*	I want at least 1.5 data vals per param  */
            small = (leafcost < (9 * small + 1.5 * cls->weights_sum + 1)) ? 1 : 0;
            if (small) {
                cls->use = Tiny;
            } else if (cls->use == Tiny) {
                cls->use = Plain;
                cls->sum_score_sq = 0.0;
            } else if (cls->age >= MinFacAge) {
                if (cls->use == Plain) {
                    if (cls->fac_cost < cls->nofac_cost) {
                        cls->use = Fac;
                        cls->boost_count = 0;
                        cls->score_boost = 1.0;
                    }
                } else if (cls->nofac_cost < cls->fac_cost) {
                    cls->use = Plain;
                }
            }
        }
    }
    if (dod && !cls->hold_type && (Control & AdjTr) && (cls->num_sons >= 2)) {
        leafcost = (cls->use == Fac) ? cls->fac_cost : cls->nofac_cost;
        if ((cls->type == Dad) && (leafcost < cls->dad_cost) && (Fix != Random)) {
            log_msg(1, "Changing type of class%s from Dad to Leaf", serial_to_str(cls));
            SeeAll = 4;
            /*	Kill all sons  */
            delete_sons(cls->id); /* which changes type to leaf */
        } else if (npars && (leafcost > cls->dad_cost) && (cls->type == Leaf)) {
            log_msg(1, "Changing type of class%s from Leaf to Dad", serial_to_str(cls));
            SeeAll = 4;
            cls->type = Dad;
            if (dad) {
                dad->hold_type += 3;
            }
            /*	Make subs into leafs  */
            son = popln->classes[cls->son_id];
            son->type = Leaf;
            son->serial = 4 * popln->next_serial;
            popln->next_serial++;
            son = popln->classes[son->sib_id];
            son->type = Leaf;
            son->serial = 4 * popln->next_serial;
            popln->next_serial++;
        }
    }
    set_best_costs(cls);
    if (Control & AdjPr) {
        cls->age++;
    }
}

/*	----------------  ncostvarall  ------------------------------   */
/*	To do parent costing on all vars of a class		*/
/*	If not 'valid', don't cost, and fake params  */
void parent_cost_all_vars(Class *cls, int valid) {
    Class *son;
    ExplnVar *exp_var;
    int i, son_id, nson;
    double abcost, rrelab;
    Population *popln = CurCtx.popln;
    VSetVar *vset_var;
    VarType *vtype;

    abcost = 0.0;
    for (i = 0; i < CurCtx.vset->length; i++) {
        vset_var = &CurCtx.vset->variables[i];
        vtype = vset_var->vtype;
        (*vtype->cost_var_nonleaf)(i, valid, cls);
        exp_var = (ExplnVar *)cls->stats[i];
        if (!vset_var->inactive) {
            abcost += exp_var->npcost;
        }
    }

    if (!valid) {
        cls->dad_par_cost = cls->dad_cost = 0.0;
        return;
    }
    nson = cls->num_sons;
    /*	The sons of a dad may be listed in any order, so the param cost
    of the dad can be reduced by log (nson !)  */
    abcost -= FacLog[nson];
    /*	The cost of saying 'dad' and number of sons is set at nson bits. */
    abcost += nson * BIT;
    /*	Now add cost of specifying the relabs of the sons.  */
    /*	Their relabs are absolute, but we specify them as fractions of this
    dad's relab. The cost includes -0.5 * Sum_sons { log (sonab / dadab) }
        */
    rrelab = 1.0 / cls->relab;
    for (son_id = cls->son_id; son_id >= 0; son_id = son->sib_id) {
        son = popln->classes[son_id];
        abcost -= 0.5 * log(son->relab * rrelab);
    }
    /*	Add other terms from Fisher  */
    abcost += (nson - 1) * (log(cls->weights_sum) + LATTICE);
    /*	And from prior:  */
    abcost -= FacLog[nson - 1];
    /*	The sons will have been processed by 'adjustclass' already, and
    this will have caused their best pcosts to be added into cls->cnpcost  */
    cls->dad_par_cost += abcost;
    return;
}

/*	--------------------  killsons ------------------------------- */
/*	To delete the sons of a class  */
void delete_sons(int kk) {
    Class *cls, *son;
    int kks;
    Population *popln = CurCtx.popln;

    if (!(Control & AdjTr))
        return;
    cls = popln->classes[kk];
    if (cls->num_sons <= 0)
        return;
    SeeAll = 4;
    
    for (kks = cls->son_id; kks > 0; kks = son->sib_id) {
        son = popln->classes[kks];
        son->type = Vacant;
        son->dad_id = Deadkilled;
        delete_sons(kks);
    }
    cls->num_sons = 0;
    cls->son_id = -1;
    if (cls->type == Dad)
        cls->type = Leaf;
    cls->hold_type = 0;
}

/*	--------------------  splitleaf ------------------------   */
/*	If kk is a leaf and has subs, turns it into a dad and makes
its subs into leaves.  Returns 0 if successful.  */
int split_leaf(int kk) {
    Class *son, *cls;
    int kks;
    Population *popln = CurCtx.popln;
    cls = popln->classes[kk];
    if ((cls->type != Leaf) || (cls->num_sons < 2) || cls->hold_type) {
        return (1);
    }
    cls->type = Dad;
    cls->hold_type = HoldTime;
    for (kks = cls->son_id; kks >= 0; kks = son->sib_id) {
        son = popln->classes[kks];
        son->type = Leaf;
        son->serial = 4 * popln->next_serial;
        popln->next_serial++;
    }
    SeeAll = 4;
    return (0);
}

/*	---------------  delete_all_classes  ------------ */
/*	To remove all classes of a popln except its root  */
void delete_all_classes() {
    int k;
    Class *cls;
    Population *popln = CurCtx.popln;

    for (k = 0; k <= popln->hi_class; k++) {
        cls = popln->classes[k];
        if (cls->id != popln->root) {
            cls->type = Vacant;
            cls->dad_id = Deadkilled;
        }
    }
    popln->num_classes = 1;
    popln->hi_class = popln->root;
    cls = popln->classes[popln->root];
    cls->son_id = -1;
    cls->sib_id = -1;
    cls->num_sons = 0;
    cls->serial = 4;
    popln->next_serial = 2;
    cls->age = 0;
    cls->hold_type = cls->hold_use = 0;
    cls->type = Leaf;
    tidy(1, NoSubs + 1);
    return;
}

/*	------------------  nextleaf  -------------------------   */
/*	given a ptr cpop to a popln and an integer iss, returns the index
of the next leaf in the popln with serial > iss, or -1 if there is none */
/*	Intended for scanning the leaves of a popln in serial order  */
int next_leaf(Population *cpop, int iss) {
    Class *cls;
    int i, j, k, n;

    n = -1;
    k = 10000000;
    for (i = 0; i <= cpop->hi_class; i++) {
        cls = cpop->classes[i];
        if (cls->type != Leaf)
            goto idone;
        j = cls->serial;
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

