/*	-----------------  fiddles with tree structure  ----------------  */
#include "glob.h"

/*     --------------------  flatten  --------------------------------  */
/*	Destroys all non-root Dads, leaving all old non-dads (leaf or sub)
which had no children as leaves, direct sons of root. Prepares for a rebuild */
/*	Locks the type of root to dad  */
void flatten() {

    int i;
    Class *cls;
    Population *popln = CurCtx.popln;
    Class *root = popln->classes[popln->root];

    tidy(0, NoSubs);
    if (root->num_sons == 0) {
        log_msg(0, "Nothing to flatten");
        return;
    }
    for (i = 0; i <= popln->hi_class; i++) {
        if (i == popln->root)
            continue;
        cls = popln->classes[i];
        if (cls->type == Vacant)
            continue;
        if (cls->num_sons) { /*  Kill it  */
            cls->type = Vacant;
            cls->dad_id = Deadkilled;
            popln->num_classes--;
        } else {
            if (cls->type == Sub) {
                cls->type = Leaf;
                cls->serial = popln->next_serial << 2;
                popln->next_serial++;
            }
            cls->dad_id = popln->root;
        }
    }
    NoSubs++;
    tidy(0, NoSubs);
    root->type = Dad;
    root->hold_type = Forever;
    do_all(1, 1);
    do_dads(3);
    do_all(3, 0);
    if (Interactive & Heard) {
        log_msg(0, "Flatten ends prematurely");
    }
    if (NoSubs > 0) {
        NoSubs--;
    }
    if (Debug < 1)
        print_tree();
}

/*	------------------------  insdad  ------------------------------  */
/*	Given 2 class serials, calcs reduction in tree pcost coming from
inserting a new dad with the given classes as sons. If either class is
a sub, or it they have different dads, returns a huge negative benefit  */
/*	Insdad will, however, accept classes s1 and s2 if one is the dad
of the other, provided neither is the root  */
/*	The change, if possible, is made to ctx.popln.
    dadid is set to the id of the new dad, if any.  */
double insert_dad(int ser1, int ser2, int *dadid) {
    Class *cls1, *cls2, *ndad, *odad;
    ExplnVar *exp_var, *fexp_var;
    ClassVar *cls_var, *fcls_var;
    int nch, iv, k1, k2;
    double origcost, newcost, drop = -1.0e20;
    int oldid, newid, od1, od2;

    Population *popln = CurCtx.popln;
    Class *root = popln->classes[popln->root];

    origcost = root->best_par_cost;
    *dadid = -1;
    k1 = serial_to_id(ser1);
    k2 = serial_to_id(ser2);
    if ((k1 < 0) || (k2 < 0)) {
        return drop;
    }

    cls1 = popln->classes[k1];
    cls2 = popln->classes[k2];

    if ((cls1->type == Sub) || (cls2->type == Sub)) {
        return drop;
    }

    od1 = cls1->dad_id;
    od2 = cls2->dad_id;
    /*	Normally we expect cls1, cls2 to have same dad, with at least one other son	*/
    if (od1 == od2) {
        oldid = od1;
        odad = popln->classes[oldid];
        if (odad->num_sons < 3) {
            return drop;
        }
    } else if (od1 == k2) {
        /*	They do not have the same dad, but one may be the son ot the other */
        /* cls1 is a son of cls2 */
        oldid = od2;        /* The dad of cls2 */
    } else if (od2 == k1) { /*  cls2 is a son of cls1  */
        oldid = od1;
    } else {
        return drop;
    }

    odad = popln->classes[oldid];
    newid = make_class();
    if (newid >= 0) {
        ndad = popln->classes[newid]; /*  The new dad  */
        /*	Copy old dad's basics, stats into new dad  */
        nch = ((char *)&odad->id) - ((char *)odad);
        memcpy(ndad, odad, nch);
        ndad->serial = popln->next_serial << 2;
        popln->next_serial++;
        ndad->age = MinFacAge - 3;
        ndad->hold_type = 0;
        /* Copy Basics. the structures should have been made.  */
        for (iv = 0; iv < CurCtx.vset->length; iv++) {
            fcls_var = odad->basics[iv];
            cls_var = ndad->basics[iv];
            nch = CurCtx.vset->variables[iv].basic_size;
            memcpy(cls_var, fcls_var, nch);
        }

        /*  Copy stats  */
        for (iv = 0; iv < CurCtx.vset->length; iv++) {
            fexp_var = odad->stats[iv];
            exp_var = ndad->stats[iv];
            nch = CurCtx.vset->variables[iv].stats_size;
            memcpy(exp_var, fexp_var, nch);
        }

        ndad->dad_id = oldid; /* So new dad is son of old dad */
        cls1->dad_id = cls2->dad_id = newid;
        /*	Set relab and cnt in new dad  */
        ndad->relab = cls1->relab + cls2->relab;
        ndad->weights_sum = cls1->weights_sum + cls2->weights_sum;

        /*	Fix linkages  */
        tidy(0, NoSubs);
        do_dads(20);
        newcost = root->best_par_cost;
        drop = origcost - newcost;
        *dadid = newid;
    } else {
        drop = -1.0e20;
    }
    return (drop);
}

/*	----------------------  bestinsdad ---------------------   */
/*	Returns serial of new dad, or 0 if best no good, or -1 if none
to try  */
int best_insert_dad(int force) {
    Context oldctx;
    Class *cls1, *cls2, *root;
    int i1, i2, hiid, succ;
    int bser1, bser2, ser1, ser2, newp, newid, newser;
    double res, bestdrop, origcost;

    /*	We look for all pairs of non-Sub serials except root.
        For each pair, we copy the population to TrialPop, switch context to
    TrialPop, and do an insdad on the pair. We note the pair giving the largest
    insdad, again copy to TrialPop, repeat the insdad, and relax with
    doall, .
    */

    /*	To get all pairs, we need a double loop over class indexes. I use
    i1, i2 as the indices. i1 runs from 0 to population->hicl-1, i2 from i1+1 to
    population->hicl
    */
    NoSubs++;
    /*	Do one pass over population to set costs   */
    do_all(1, 1);
    bestdrop = -1.0e20;
    bser1 = bser2 = -1;
    newser = 0;

    root = CurCtx.popln->classes[CurCtx.popln->root];
    origcost = root->best_cost;
    hiid = CurCtx.popln->hi_class;
    if (CurCtx.popln->num_classes < 4) {
        log_msg(0, "Model has only%2d class", CurCtx.popln->num_classes);
        succ = -1;
        goto alldone;
    }
    memcpy(&oldctx, &CurCtx, sizeof(Context));

    i1 = 0;
outer:
    if (i1 == CurCtx.popln->root)
        goto i1done;
    cls1 = CurCtx.popln->classes[i1];
    if (!cls1)
        goto i1done;
    if ((cls1->type == Vacant) || (cls1->type == Sub))
        goto i1done;
    ser1 = cls1->serial;
    i2 = i1 + 1;
inner:
    if (i2 == CurCtx.popln->root)
        goto i2done;
    cls2 = CurCtx.popln->classes[i2];
    if (!cls2)
        goto i2done;
    if ((cls2->type == Vacant) || (cls2->type == Sub))
        goto i2done;
    if (cls1->dad_id != cls2->dad_id)
        goto i2done;
    ser2 = cls2->serial;
    if (chk_bad_move(1, ser1, ser2))
        goto i2done;

    /*	Copy population to TrialPop, unfilled  */
    newp = copy_population(CurCtx.popln->id, 0, "TrialPop");
    if (newp < 0)
        goto popfails;
    CurCtx.popln = Populations[newp];
    root = CurCtx.popln->classes[CurCtx.popln->root];
    res = insert_dad(ser1, ser2, &newid);
    if (newid < 0) {
        goto i2done;
    }
    if (res > bestdrop) {
        bestdrop = res;
        bser1 = ser1;
        bser2 = ser2;
    }
i2done:
    memcpy(&CurCtx, &oldctx, sizeof(Context));

    root = CurCtx.popln->classes[CurCtx.popln->root];
    i2++;
    if (i2 <= hiid)
        goto inner;
i1done:
    i1++;
    if (i1 < hiid)
        goto outer;

    goto alldone;

alldone:
    if (bser1 < 0) {
        log_msg(0, "No possible dad insertions");
        succ = newser = -1;
        goto finish;
    }
    /*	Copy population to TrialPop, filled  */
    newp = copy_population(CurCtx.popln->id, 1, "TrialPop");
    if (newp < 0)
        goto popfails;
    CurCtx.popln = Populations[newp];
    root = CurCtx.popln->classes[CurCtx.popln->root];
    log_msg(0, "TRYING INSERT %6d,%6d", bser1 >> 2, bser2 >> 2);
    res = insert_dad(bser1, bser2, &newid);
    /*	But check it is not killed off   */
    newser = CurCtx.popln->classes[newid]->serial;
    Control = 0;
    do_all(1, 1);
    Control = AdjAll;
    if (Heard) {
        log_msg(0, "BestInsDad ends prematurely");
        return (0);
    }
    if (newser != CurCtx.popln->classes[newid]->serial)
        newser = 0;
    /*	See if the trial model has improved over original  */
    succ = 1;
    if (root->best_cost < origcost)
        goto winner;
    succ = 0;
    if (force)
        goto winner;
    set_bad_move(1, bser1, bser2);
    newser = 0;
    memcpy(&CurCtx, &oldctx, sizeof(Context));

    log_msg(0, "Failed ******");
    goto finish;

popfails:
    succ = newser = -1;
    log_msg(0, "Cannot make TrialPop");
    goto finish;

winner:
    log_msg(0, "%s", (succ) ? "ACCEPTED !!!" : "FORCED");
    if (Debug < 1)
        print_tree();
    clr_bad_move();
    /*	Reverse roles of 'work' and TrialPop  */
    strcpy(oldctx.popln->name, "TrialPop");
    strcpy(CurCtx.popln->name, "work");
    if (succ)
        track_best(1);

finish:
    if (NoSubs > 0)
        NoSubs--;
    return (newser);
}

/*	---------------  rebuild  --------------------------------  */
/*	Flattens and the rebuilds the tree  */
void rebuild() { log_msg(0, "Rebuild obsolete!"); }

/*	------------------  deldad  -------------------------  */
/*	If class ser is Dad (not root), it is removed, and its sons become
sons of its dad.
    */
double splice_dad(int ser) {
    Class *son, *cls;
    int kk, kkd, kks;
    double drop, origcost, newcost;
    Population *popln = CurCtx.popln;
    Class *root = popln->classes[popln->root];

    drop = -1.0e20;
    kk = serial_to_id(ser);
    cls = popln->classes[kk];
    if (cls->type != Dad)
        goto finish;
    if (kk == popln->root)
        goto finish;
    kkd = cls->dad_id;
    if (kkd < 0)
        goto finish;
    if (cls->num_sons <= 0)
        goto finish;
    /*	All seems OK. Fix idads in kk's sons  */
    origcost = root->best_par_cost;
    for (kks = cls->son_id; kks >= 0; kks = son->sib_id) {
        son = popln->classes[kks];
        son->dad_id = kkd;
    }
    /*	Now kill off class kk  */
    cls->type = Vacant;
    popln->num_classes--;
    /*	Fix linkages  */
    tidy(0, NoSubs);
    do_dads(20);
    newcost = root->best_par_cost;
    drop = origcost - newcost;
finish:
    return (drop);
}

/*	------------------  bestdeldad -------------------------   */
/*	Tries all feasible deldads, does dogood on best and installs
as work if an improvement.  */
/*	Returns 1 if accepted, 0 if tried and no good, -1 if none to try */
int best_remove_dad() {
    Context oldctx;
    Class *cls, *root;
    int i1, hiid, newp;
    int bser, ser;
    double res, bestdrop, origcost;
    Population *popln = CurCtx.popln;

    NoSubs++;
    bestdrop = -1.0e20;
    bser = -1;

    root = popln->classes[popln->root];
    /*	Do one pass of doall to set costs  */
    do_all(1, 1);
    origcost = root->best_cost;
    hiid = CurCtx.popln->hi_class;
    memcpy(&oldctx, &CurCtx, sizeof(Context));
    i1 = 0;
loop:
    if (i1 == popln->root)
        goto i1done;
    cls = CurCtx.popln->classes[i1];
    if (!cls)
        goto i1done;
    if (cls->type != Dad)
        goto i1done;
    ser = cls->serial;
    if (chk_bad_move(2, 0, ser))
        goto i1done;
    newp = copy_population(CurCtx.popln->id, 0, "TrialPop");
    if (newp < 0)
        goto popfails;
    popln = CurCtx.popln = Populations[newp];

    root = popln->classes[popln->root];
    res = splice_dad(ser);
    if (res < -1000000.0) {
        goto i1done;
    }
    if (res > bestdrop) {
        bestdrop = res;
        bser = ser;
    }
i1done:
    memcpy(&CurCtx, &oldctx, sizeof(Context));

    i1++;
    if (i1 <= hiid)
        goto loop;

    if (bser < 0) {
        log_msg(0, "No possible dad deletions");
        goto finish;
    }
    newp = copy_population(CurCtx.popln->id, 1, "TrialPop");
    if (newp < 0)
        goto popfails;
    popln = CurCtx.popln = Populations[newp];

    root = popln->classes[popln->root];
    log_msg(0, "TRYING DELETE %6d", bser >> 2);
    res = splice_dad(bser);
    Control = 0;
    do_all(1, 1);
    Control = AdjAll;
    if (Heard) {
        log_msg(0, "BestDelDad ends prematurely");
        return (0);
    }
    if (root->best_cost < origcost)
        goto winner;
    set_bad_move(2, 0, bser); /* log failure in badmoves */
    bser = 0;
    memcpy(&CurCtx, &oldctx, sizeof(Context));

    log_msg(0, "Failed ******");
    goto finish;

popfails:
    log_msg(0, "BestDelDad cannot make TrialPop");
    bser = -1;
    goto finish;

winner:
    clr_bad_move();
    log_msg(0, "ACCEPTED !!!");
    if (Debug < 1)
        print_tree();
    strcpy(oldctx.popln->name, "TrialPop");
    strcpy(CurCtx.popln->name, "work");
    track_best(1);

finish:
    if (NoSubs > 0)
        NoSubs--;
    return (bser);
}

/*	---------------  binhier  --------------------------------  */
/*	If flat, flattens population. Then inserts dads to make a binary hierarchy.
    Then deletes dads as appropriate  */
void binary_hierarchy(int flat) {
    int nn;

    if (flat)
        flatten();
    NoSubs++;
    if (Heard)
        goto kicked;
    clr_bad_move();
insloop:
    nn = best_insert_dad(1);
    if (Heard)
        goto kicked;
    if (nn > 0)
        goto insloop;

    try_moves(2);
    if (Heard)
        goto kicked;

delloop:
    nn = best_remove_dad();
    if (Heard)
        goto kicked;
    if (nn > 0)
        goto delloop;

    try_moves(2);
    if (Heard)
        goto kicked;

finish:
    if (Debug < 1)
        print_tree();
    if (NoSubs > 0)
        NoSubs--;
    clr_bad_move();
    return;

kicked:
    nn = find_population("work");
    CurCtx.popln = Populations[nn];

    log_msg(0, "BinHier ends prematurely");
    goto finish;
}

/*	------------------  ranclass  --------------------------  */
/*	To make nn random classes  */
void ranclass(int nn) {
    int n, ic, ib, num_son;
    double bs;
    Class *sub, *dad, *cls;
    Population *popln = CurCtx.popln;
    Class *root = popln->classes[popln->root];

    if (!popln) {
        log_msg(0, "Ranclass needs a model");
        goto finish;
    }
    if (!popln->sample_size) {
        log_msg(0, "Model has no sample");
        goto finish;
    }
    if (nn > (popln->cls_vec_len - 2)) {
        log_msg(0, "Too many classes");
        goto finish;
    }

    NoSubs = 0;
    delete_all_classes();
    n = 1;
    if (nn < 2)
        goto finish;

again:
    if (n >= nn)
        goto windup;
    num_son = find_all(Leaf);
    /*	Locate biggest leaf with subs aged at least MinAge  */
    ib = -1;
    bs = 0.0;
    for (ic = 0; ic < num_son; ic++) {
        cls = Sons[ic];
        if (cls->num_sons < 2)
            goto icdone;
        sub = popln->classes[cls->son_id];
        if (sub->age < MinAge)
            goto icdone;
        if (cls->weights_sum > bs) {
            bs = cls->weights_sum;
            ib = ic;
        }
    icdone:;
    }

    if (ib < 0) {
        do_all(1, 1);
        goto again;
    }
    /*	Split sons[ib]  */
    dad = Sons[ib];
    if (split_leaf(dad->id))
        goto windup;
    log_msg(0, "Splitting %s size%8.1f", serial_to_str(dad), dad->weights_sum);
    dad->hold_type = Forever;
    n++;
    goto again;

windup:
    NoSubs = 1;
    do_all(5, 1);
    flatten();
    do_all(6, 0);
    do_all(4, 1);
    if (Debug < 1)
        print_tree();
    root->hold_type = 0;

finish:
    return;
}

/*	---------------  moveclass  --------------------------------  */
/*	To move class ser1 to be a child of class ser2  */
double move_class(int ser1, int ser2) {
    Class *cls1, *cls2, *odad;
    int k1, k2, od2;
    double origcost, newcost, drop;

    Population *popln = CurCtx.popln;
    Class *root = popln->classes[popln->root];

    origcost = root->best_par_cost;
    k1 = serial_to_id(ser1);
    if (k1 < 0)
        goto nullit;
    k2 = serial_to_id(ser2);
    if (k2 < 0)
        goto nullit;
    cls1 = popln->classes[k1];
    if (cls1->type == Sub)
        goto nullit;
    cls2 = popln->classes[k2];
    if (cls2->type != Dad) {
        log_msg(0, "Class %4d is not a dad", ser2);
        goto nullit;
    }
    /*	Check that a change is needed  */
    if (cls1->dad_id == k2) {
        log_msg(0, "No change needed");
        goto nullit;
    }
    /*	Check that cls1 is not an ancestor of cls2  */
    for (od2 = cls2->dad_id; od2 >= 0; od2 = odad->dad_id) {
        if (od2 == k1) {
            log_msg(0, "Class %4d is ancestor of class %4d", ser1, ser2);
            goto nullit;
        }
        odad = popln->classes[od2];
    }
    /*	All seems OK, so make change in links   */
    cls1->dad_id = k2;
    /*	Fix linkages  */
    tidy(0, NoSubs);
    do_dads(30);
    if (popln->sample_size) {
        do_all(4, 0);
        if (Heard)
            goto kicked;
        /* 	To collect weights, counts */
        do_all(4, 1);
        if (Heard)
            goto kicked;
    }
    newcost = root->best_par_cost;
    drop = origcost - newcost;
    goto done;

kicked:
    log_msg(0, "Moveclass interrupted prematurely");

nullit:
    drop = -1.0e20;

done:
    return (drop);
}

/*	-----------------  trial  ---------------------------------  */
void trial(int param) {
    log_msg(0, "Running TRIAL");
    correlpops(param);
}

/*	----------------------  bestmoveclass ---------------------   */
/*
Looks for the best moveclass. If force, or if an improvement,
does it. Returns 1 if an improvement, 0 if best no improvement, -1 if none
possible.
*/
int best_move_class(int force) {
    Context oldctx;
    Class *cls1, *cls2, *odad, *root;
    int i1, i2, hiid;
    int bser1, bser2, ser1, ser2, od2, newp, succ;
    double res, bestdrop, origcost;

    /*
    We look for all pairs ser1,ser2 of serials where:
    ser2 is a dad, ser1 is not an ancestor of ser2, ser1 is non-sub, and
    ser1 is not a son of ser2.
    For each pair, we copy the pop to TrialPop, switch context to
    TrialPop, and do a moveclass on the pair. We note the pair giving the
    largest moveclass, again copy to TrialPop, repeat the moveclass, and relax
    with doall, .
    */

    /*	To get all pairs, we need a double loop over class indexes. I use
    i1, i2 as the indices.   */
    NoSubs++;
    bestdrop = -1.0e20;
    bser1 = bser2 = -1;

    root = CurCtx.popln->classes[CurCtx.popln->root];
    do_all(1, 1); /* To set costs */
    origcost = root->best_cost;
    hiid = CurCtx.popln->hi_class;
    if (CurCtx.popln->num_classes < 4) {
        log_msg(0, "Model has only%2d class", CurCtx.popln->num_classes);
        succ = -1;
        goto alldone;
    }
    memcpy(&oldctx, &CurCtx, sizeof(Context));

    i1 = 0;
outer:
    if (i1 == CurCtx.popln->root) {
        goto i1done;
    }
    cls1 = CurCtx.popln->classes[i1];
    if ((!cls1) || (cls1->type == Vacant) || (cls1->type == Sub)) {
        goto i1done;
    }
    ser1 = cls1->serial;
    i2 = 0;
    while (i2 <= hiid) {

        if (i2 == i1) {
            goto i2done;
        }
        cls2 = CurCtx.popln->classes[i2];
        if ((!cls2) || (cls2->type != Dad) || (cls1->dad_id == i2)) {
            goto i2done;
        }
        /*	Check i1 not an ancestor of i2  */
        for (od2 = cls2->dad_id; od2 >= 0; od2 = odad->dad_id) {
            if (od2 == i1)
                goto i2done;
            odad = CurCtx.popln->classes[od2];
        }
        ser2 = cls2->serial;
        if (chk_bad_move(3, ser1, ser2)) {
            goto i2done;
        }

        /*	Copy pop to TrialPop, unfilled  */
        newp = copy_population(CurCtx.popln->id, 0, "TrialPop");
        if (newp < 0) {
            goto popfails;
        }
        CurCtx.popln = Populations[newp];
        root = CurCtx.popln->classes[CurCtx.popln->root];
        res = move_class(ser1, ser2);
        if (res > bestdrop) {
            bestdrop = res;
            bser1 = ser1;
            bser2 = ser2;
        }
    i2done:
        memcpy(&CurCtx, &oldctx, sizeof(Context));
        i2++;
    }

i1done:
    i1++;
    if (i1 <= hiid)
        goto outer;

    goto alldone;

alldone:
    if (bser1 < 0) {
        succ = -1;
        log_msg(0, "No possible class move");
        goto finish;
    }
    /*	Copy pop to TrialPop, filled  */
    newp = copy_population(CurCtx.popln->id, 1, "TrialPop");
    if (newp < 0)
        goto popfails;
    CurCtx.popln = Populations[newp];

    root = CurCtx.popln->classes[CurCtx.popln->root];
    log_msg(0, "TRYING MOVE CLASS %6d TO DAD %6d", bser1 >> 2, bser2 >> 2);
    res = move_class(bser1, bser2);
    Control = 0;
    do_all(1, 1);
    Control = AdjAll;
    if (Heard)
        log_msg(0, "BestMoveClass ends prematurely");
    /*	Setting dogood's target to origcost-1 allows early exit  */
    /*	See if the trial model has improved over original  */
    succ = 1;
    if (root->best_cost < origcost)
        goto winner;
    succ = 0;
    if (force)
        goto winner;
    set_bad_move(3, bser1, bser2);
    memcpy(&CurCtx, &oldctx, sizeof(Context));

    log_msg(0, "Failed ******");
    goto finish;

popfails:
    succ = -1;
    log_msg(0, "Cannot make TrialPop");
    goto finish;

winner:
    log_msg(0, "%s", (succ) ? "ACCEPTED !!!" : "FORCED");
    if (Debug < 1)
        print_tree();
    clr_bad_move();
    /*	Reverse roles of 'work' and TrialPop  */
    strcpy(oldctx.popln->name, "TrialPop");
    strcpy(CurCtx.popln->name, "work");
    if (succ)
        track_best(1);

finish:
    if (NoSubs > 0)
        NoSubs--;
    return (succ);
}

/*	--------------------  trymoves  ----------------------------   */
/*	Tries moving classes using bestmoveclass until ntry attempts in
succession have failed, or until all possible moves have been tried   */
void try_moves(int ntry) {
    int nfail, succ;

    NoSubs++;
    do_all(1, 1);
    clr_bad_move();
    nfail = 0;

    while (nfail < ntry) {
        succ = best_move_class(0);
        if (succ < 0)
            break;
        nfail++;
        if (succ)
            nfail = 0;
        if ((Heard) || (Stop)) {
            log_msg(0, "Trymoves ends prematurely");
            break;
        }
    }

    if (NoSubs > 0)
        NoSubs--;
    return;
}
