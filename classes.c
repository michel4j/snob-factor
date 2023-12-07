/*	Stuff dealing with classes as members of polpns     */
#include "glob.h"

/*	-----------------------  s2id  ---------------------------   */
/*	Finds class index (id) from serial, or -3 if error    */
int serial_to_id(int ss) {
    int k;
    Class *cls;
    set_population();
    Population *popln = CurCtx.popln;

    if (ss >= 1) {
        for (k = 0; k <= popln->hi_class; k++) {
            cls = popln->classes[k];
            if (cls && (cls->type != Vacant) && (cls->serial == ss))
                return (k);
        }
    }

    printf("No such class serial as %d", ss >> 2);
    if (ss & 3)
        printf("%c", (ss & 1) ? 'a' : 'b');
    printf("\n");
    return (-3);
}

/*	---------------------  set_class_score --------------------------   */
void set_class_score(Class *cls) { cls->case_score = Scores.CaseFacInt = cls->factor_scores[CurItem]; }

/*	---------------------   makeclass  -------------------------   */
/*	Makes the basic structure of a class (Class) with vector of
ptrs to CVinsts and CVinsts filled in also EVinsts and ptrs.
    Uses nc = pop->nc.
    if nc = 0, that's it. Otherwize, if a sample is defined in pop,
    score vector.
    Returns index of class, or negative if no good  */

int make_class() {
    PopVar *pvars;
    ClassVar **cvars, *cvi;
    ExplnVar **evars, *evi;
    Class *cls;
    int i, kk;
    Population *popln = CurCtx.popln;
    NumCases = CurCtx.popln->sample_size;
    /*	If nc, check that popln has an attached sample  */
    if (NumCases) {
        i = find_sample(popln->sample_name, 1);
        if (i < 0)
            return (-2);
        CurCtx.sample = CurSample = Samples[i];
    }

    NumVars = CurVSet->length;
    pvars = popln->variables;
    /*	Find a vacant slot in the popln's classes vec   */
    for (kk = 0; kk < popln->cls_vec_len; kk++) {
        if (!popln->classes[kk])
            goto gotit;
        /*	Also test for a vacated class   */
        if (popln->classes[kk]->type == Vacant)
            goto vacant1;
    }
    printf("Popln full of classes\n");
    goto nospace;

gotit:
    cls = (Class *)alloc_blocks(1, sizeof(Class));
    if (!cls)
        goto nospace;
    popln->classes[kk] = cls;
    popln->hi_class = kk; /* Highest used index in population->classes */
    cls->id = kk;

    /*	Make vector of ptrs to CVinsts   */
    cvars = cls->basics = (ClassVar **)alloc_blocks(1, NumVars * sizeof(ClassVar *));
    if (!cvars)
        goto nospace;

    /*	Now make the ClassVar blocks, which are of varying size   */
    for (i = 0; i < NumVars; i++) {
        CurPopVar = pvars + i;
        CurAttr = VSetVarList + i;
        cvi = cvars[i] = (ClassVar *)alloc_blocks(1, CurAttr->basic_size);
        if (!cvi)
            goto nospace;
        /*	Fill in standard stuff  */
        cvi->id = CurAttr->id;
    }

    /*	Make ExplnVar blocks and vector of pointers  */
    evars = cls->stats = (ExplnVar **)alloc_blocks(1, NumVars * sizeof(ExplnVar *));
    if (!evars)
        goto nospace;

    for (i = 0; i < NumVars; i++) {
        CurPopVar = pvars + i;
        CurAttr = VSetVarList + i;
        evi = evars[i] = (ExplnVar *)alloc_blocks(1, CurAttr->stats_size);
        if (!evi)
            goto nospace;
        evi->id = CurPopVar->id;
    }

    /*	Stomp on ptrs as yet undefined  */
    cls->factor_scores = 0;
    cls->type = 0;
    goto donebasic;

vacant1: /* Vacant type shows structure set up but vacated.
     Use, but set new (Vacant) type,  */
    cls = popln->classes[kk];
    cvars = cls->basics;
    evars = cls->stats;
    cls->type = Vacant;

donebasic:
    cls->best_cost = cls->nofac_cost = cls->fac_cost = 0.0;
    cls->weights_sum = 0.0;
    /*	Invalidate hierarchy links  */
    cls->dad_id = cls->sib_id = cls->son_id = -1;
    cls->num_sons = 0;
    for (i = 0; i < NumVars; i++)
        cvars[i]->signif = 1;
    popln->num_classes++;
    if (kk > popln->hi_class)
        popln->hi_class = kk;
    if (cls->type == Vacant)
        goto vacant2;

    /*	If nc = 0, this completes the make.  */
    if (NumCases == 0)
        goto finish;
    cls->factor_scores = (short *)alloc_blocks(2, NumCases * sizeof(short));
    goto expanded;

vacant2:
    cls->type = 0;

expanded:
    for (i = 0; i < NumVars; i++)
        evars[i]->num_values = 0.0;
finish:
    cls->age = 0;
    cls->hold_type = cls->hold_use = 0;
    cls->weights_sum = 0.0;
    return (kk);

nospace:
    printf("No space for new class\n");
    return (-1);
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

    printf("\nS%s", serial_to_str(cls));
    printf(" %s", typstr[((int)cls->type)]);
    if (cls->dad_id < 0)
        printf("    Root");
    else
        printf(" Dad%s", serial_to_str(popln->classes[((int)cls->dad_id)]));
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
    if (!full)
        return;
    for (i = 0; i < CurVSet->length; i++) {
        CurVType = VSetVarList[i].vtype;
        (*CurVType->show)(cls, i);
    }
    return;
}

void print_class(int kk, int full) {
    Class *cls;
    if (kk >= 0) {
        cls = CurCtx.popln->classes[kk];
        print_one_class(cls, full);
        return;
    }
    if (kk < -2) {
        printf("%d passed to printclass\n", kk);
        return;
    }
    set_population();
    cls = CurRootClass;

    do {
        if ((kk == -2) || (cls->type != Sub))
            print_one_class(cls, full);
        next_class(&cls);
    } while (cls);

    return;
}

/*	-------------------------  cleartcosts  ------  */
/*	Clears tcosts (cntcost, cftcost, cstcost). Also clears newcnt,
newvsq, and calls clear_stats for all variables   */
void clear_costs(Class *cls) {
    int i;

    cls->mlogab = -log(cls->relab);
    cls->cstcost = cls->cftcost = cls->cntcost = 0.0;
    cls->cfvcost = 0.0;
    cls->newcnt = cls->newvsq = 0.0;
    cls->score_change_count = 0;
    cls->vav = 0.0;
    cls->totvv = 0.0;
    if (!SeeAll)
        cls->scancnt = 0;
    for (i = 0; i < CurVSet->length; i++) {
        CurVType = VSetVarList[i].vtype;
        (*CurVType->clear_stats)(i, cls);
    }
    return;
}

/*	-------------------  setbestparall  ------------------------   */
/*	To set 'b' params, costs to reflect current use   */
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
    for (i = 0; i < CurVSet->length; i++) {
        CurVType = VSetVarList[i].vtype;
        (*CurVType->set_best_pars)(i, cls);
    }
    return;
}

/*	--------------------  scorevarall  --------------------    */
/*	To evaluate derivs of cost wrt score in all vars of a class.
    Does one item, number case  */
/*	Leaves data values set in stats, but does no scoring if class too
young. If class age = MinFacAge, will guess scores but not cost them */
/*	If control & AdjSc, will adjust score  */
void score_all_vars(Class *cls) {
    int i, igbit, oldicvv;
    double del;

    set_class_score(cls);
    if ((cls->age < MinFacAge) || (cls->use == Tiny)) {
        Scores.CaseFacScore = cls->avg_factor_scores = cls->sum_score_sq = 0.0;
        Scores.CaseFacInt = 0;
        goto done;
    }
    if (cls->sum_score_sq > 0.0)
        goto started;
    /*	Generate a fake score to get started.   */
    cls->boost_count = 0;
    Scores.cvvsprd = 0.1 / NumVars;
    oldicvv = igbit = 0;
    Scores.CaseFacScore = (rand_int() < 0) ? 1.0 : -1.0;
    goto fake;

started:
    /*	Get current score  */
    oldicvv = Scores.CaseFacInt;
    igbit = Scores.CaseFacInt & 1;
    Scores.CaseFacScore = Scores.CaseFacInt * ScoreRscale;
    /*	Subtract average from last pass  */
    /*xx
        cvv -= cls->avg_factor_scores;
    */
    if (cls->boost_count && ((Control & AdjSP) == AdjSP)) {
        Scores.CaseFacScore *= cls->score_boost;
        if (Scores.CaseFacScore > Maxv)
            Scores.CaseFacScore = Maxv;
        else if (Scores.CaseFacScore < -Maxv)
            Scores.CaseFacScore = -Maxv;
        del = Scores.CaseFacScore * HScoreScale;
        if (del < 0.0)
            del -= 1.0;
        Scores.CaseFacInt = del + 0.5;
        Scores.CaseFacInt = Scores.CaseFacInt << 1; /* Round to nearest even times ScoreScale */
        igbit = 0;
        Scores.CaseFacScore = Scores.CaseFacInt * ScoreRscale;
    }

    Scores.CaseFacScoreSq = Scores.CaseFacScore * Scores.CaseFacScore;
    Scores.CaseFacScoreD1 = Scores.CaseFacScoreD2 = Scores.EstFacScoreD2 = Scores.CaseFacScoreD3 = 0.0;
    for (i = 0; i < CurVSet->length; i++) {
        CurAttr = VSetVarList + i;
        if (CurAttr->inactive)
            goto vdone;
        CurVType = CurAttr->vtype;
        (*CurVType->score_var)(i, cls);
    /*	score_var should add to vvd1, vvd2, vvd3, mvvd2.  */
    vdone:;
    }
    Scores.CaseFacScoreD1 += Scores.CaseFacScore;
    Scores.CaseFacScoreD2 += 1.0;
    Scores.EstFacScoreD2 += 1.0; /*  From prior  */
    /*	There is a cost term 0.5 * cvvsprd from the prior (whence the additional
        1 in vvd2).  */
    Scores.cvvsprd = 1.0 / Scores.CaseFacScoreD2;
    /*	Also, overall cost includes 0.5*cvvsprd*vvd2, so there is a derivative
    term wrt cvv of 0.5*cvvsprd*vvd3  */
    Scores.CaseFacScoreD1 += 0.5 * Scores.cvvsprd * Scores.CaseFacScoreD3;
    del = Scores.CaseFacScoreD1 / Scores.EstFacScoreD2;
    if (Control & AdjSc) {
        Scores.CaseFacScore -= del;
    }
fake:
    if (Scores.CaseFacScore > Maxv)
        Scores.CaseFacScore = Maxv;
    else if (Scores.CaseFacScore < -Maxv)
        Scores.CaseFacScore = -Maxv;
    del = Scores.CaseFacScore * HScoreScale;
    if (del < 0.0)
        del -= 1.0;
    Scores.CaseFacInt = del + rand_float();
    Scores.CaseFacInt = Scores.CaseFacInt << 1; /* Round to nearest even times ScoreScale */
    Scores.CaseFacInt |= igbit;                 /* Restore original ignore bit */
    if (!igbit) {
        oldicvv -= Scores.CaseFacInt;
        if (oldicvv < 0)
            oldicvv = -oldicvv;
        if (oldicvv > SigScoreChange)
            cls->score_change_count++;
    }
    cls->case_fac_score = Scores.CaseFacScore = Scores.CaseFacInt * ScoreRscale;
    cls->case_fac_score_sq = Scores.CaseFacScoreSq = Scores.CaseFacScore * Scores.CaseFacScore;
    cls->cvvsprd = Scores.cvvsprd;
done:
    cls->factor_scores[CurItem] = cls->case_score = Scores.CaseFacInt;
    return;
}

/*	----------------------  costvarall  --------------------------  */
/*	Does cost_var on all vars of class for the current item, setting
cls->casecost according to use of class  */
void cost_all_vars(Class *cls) {
    int fac;
    double tmp;

    set_class_score(cls);
    if ((cls->age < MinFacAge) || (cls->use == Tiny))
        fac = 0;
    else {
        fac = 1;
        Scores.CaseFacScoreSq = Scores.CaseFacScore * Scores.CaseFacScore;
    }
    Scores.CaseCost = Scores.CaseNoFacCost = Scores.CaseFacCost = cls->mlogab; /* Abundance cost */
    for (int iv = 0; iv < CurVSet->length; iv++) {
        CurAttr = VSetVarList + iv;
        if (CurAttr->inactive)
            goto vdone;
        CurVType = CurAttr->vtype;
        (*CurVType->cost_var)(iv, fac, cls);
    /*	will add to CaseNoFacCost, CaseFacCost  */
    vdone:;
    }

    cls->total_case_cost = cls->nofac_case_cost = Scores.CaseNoFacCost;
    cls->fac_case_cost = Scores.CaseNoFacCost + 10.0;
    if (cls->num_sons < 2)
        cls->dad_case_cost = 0.0;
    cls->coding_case_cost = 0.0;
    if (!fac)
        goto finish;
    /*	Now we add the cost of coding the score, and its roundoff */
    /*	The simple form for costing a score is :
        tmp = hlg2pi + 0.5 * (cvvsq + cvvsprd - log (cvvsprd)) + lattice;
    However, we appeal to the large number of score parameters, which gives a
    more negative 'lattice' ((log 12)/2 for one parameter) approaching -(1/2)
    log (2 Pi e) which results in the reduced cost :  */
    cls->clvsprd = log(Scores.cvvsprd);
    tmp = 0.5 * (Scores.CaseFacScoreSq + Scores.cvvsprd - cls->clvsprd - 1.0);
    /*	Over all scores for the class, the lattice effect will add approx
            ( log (2 Pi cnt)) / 2  + 1
        to the class cost. This is added later, once cnt is known.
        */
    Scores.CaseFacCost += tmp;
    cls->fac_case_cost = Scores.CaseFacCost;
    cls->coding_case_cost = tmp;
    if (cls->use == Fac)
        cls->total_case_cost = Scores.CaseFacCost;
finish:
    return;
}

/*	----------------------  derivvarall  ---------------------    */
/*	To collect derivative statistics for all vars of a class   */
void deriv_all_vars(Class *cls) {
    int fac;
    const double case_weight = cls->case_weight;

    set_class_score(cls);
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
    for (int iv = 0; iv < NumVars; iv++) {
        CurAttr = VSetVarList + iv;
        if (CurAttr->inactive)
            goto vdone;
        CurVType = CurAttr->vtype;
        (*CurVType->deriv_var)(iv, fac, cls);
    vdone:;
    }

    /*	Collect case item costs   */
    cls->cstcost += case_weight * cls->nofac_case_cost;
    cls->cftcost += case_weight * cls->fac_case_cost;
    cls->cntcost += case_weight * cls->dad_case_cost;
    cls->cfvcost += case_weight * cls->coding_case_cost;

    return;
}

/*	--------------------  adjustclass  -----------------------   */
/*	To compute pcosts of a class, and if needed, adjust params  */
/*	Will process as-dad params only if 'dod'  */
void adjust_class(Class *cls, int dod) {
    int iv, fac, npars, small;
    Class *son;
    double leafcost;
    Population *popln = CurCtx.popln;
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
    if ((cls->type == Sub) && (cls->age < MinSubAge)) {
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
    for (iv = 0; iv < CurVSet->length; iv++) {
        CurAttr = VSetVarList + iv;
        if (CurAttr->inactive)
            goto vdone;
        CurVType = CurAttr->vtype;
        (*CurVType->adjust)(iv, fac, cls);
    vdone:;
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

    if (dod)
        parent_cost_all_vars(cls, npars);

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
    if (cls->hold_use || (!(Control & AdjPr)))
        goto usechecked;
    /*	If scores boosted too many times, make Tiny and hold   */
    if (cls->boost_count > 20) {
        cls->use = Tiny;
        cls->hold_use = 10;
        goto usechecked;
    }
    /*	Check if size too small to support a factor  */
    /*	Add up the number of data values  */
    small = 0;
    leafcost = 0.0; /* Used to add up evi->cnts */
    for (iv = 0; iv < NumVars; iv++) {
        if (VSetVarList[iv].inactive)
            goto vidle;
        small++;
        leafcost += cls->stats[iv]->num_values;
    vidle:;
    }
    /*	I want at least 1.5 data vals per param  */
    small = (leafcost < (9 * small + 1.5 * cls->weights_sum + 1)) ? 1 : 0;
    if (small) {
        if (cls->use != Tiny) {
            cls->use = Tiny;
        }
        goto usechecked;
    } else {
        if (cls->use == Tiny) {
            cls->use = Plain;
            cls->sum_score_sq = 0.0;
            goto usechecked;
        }
    }
    if (cls->age < MinFacAge)
        goto usechecked;
    if (cls->use == Plain) {
        if (cls->fac_cost < cls->nofac_cost) {
            cls->use = Fac;
            cls->boost_count = 0;
            cls->score_boost = 1.0;
        }
    } else {
        if (cls->nofac_cost < cls->fac_cost) {
            cls->use = Plain;
        }
    }
usechecked:
    if (!dod)
        goto typechecked;
    if (cls->hold_type)
        goto typechecked;
    if (!(Control & AdjTr))
        goto typechecked;
    if (cls->num_sons < 2)
        goto typechecked;
    leafcost = (cls->use == Fac) ? cls->fac_cost : cls->nofac_cost;

    if ((cls->type == Dad) && (leafcost < cls->dad_cost) && (Fix != Random)) {
        flp();
        printf("Changing type of class%s from Dad to Leaf\n", serial_to_str(cls));
        SeeAll = 4;
        /*	Kill all sons  */
        delete_sons(cls->id); /* which changes type to leaf */
    } else if (npars && (leafcost > cls->dad_cost) && (cls->type == Leaf)) {
        flp();
        printf("Changing type of class%s from Leaf to Dad\n", serial_to_str(cls));
        SeeAll = 4;
        cls->type = Dad;
        if (dad)
            dad->hold_type += 3;
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

typechecked:
    set_best_costs(cls);
    if (Control & AdjPr)
        cls->age++;
    return;
}

/*	----------------  ncostvarall  ------------------------------   */
/*	To do parent costing on all vars of a class		*/
/*	If not 'valid', don't cost, and fake params  */
void parent_cost_all_vars(Class *cls, int valid) {
    Class *son;
    ExplnVar *evi;
    int i, son_id, nson;
    double abcost, rrelab;
    Population *popln = CurCtx.popln;

    abcost = 0.0;
    for (i = 0; i < CurVSet->length; i++) {
        CurAttr = VSetVarList + i;
        CurVType = CurAttr->vtype;
        (*CurVType->cost_var_nonleaf)(i, valid, cls);
        evi = (ExplnVar *)cls->stats[i];
        if (!CurAttr->inactive) {
            abcost += evi->npcost;
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
    return;
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

    CurRoot = popln->root;
    for (k = 0; k <= popln->hi_class; k++) {
        cls = popln->classes[k];
        if (cls->id != CurRoot) {
            cls->type = Vacant;
            cls->dad_id = Deadkilled;
        }
    }
    popln->num_classes = 1;
    popln->hi_class = CurRoot;
    CurRootClass = cls = popln->classes[CurRoot];
    cls->son_id = -1;
    cls->sib_id = -1;
    cls->num_sons = 0;
    cls->serial = 4;
    popln->next_serial = 2;
    cls->age = 0;
    cls->hold_type = cls->hold_use = 0;
    cls->type = Leaf;
    NoSubs++;
    tidy(1);
    if (NoSubs > 0)
        NoSubs--;
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
/*zzzz*/
