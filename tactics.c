/*	-----------------  fiddles with tree structure  ----------------  */
#define NOTGLOB 1
#define TACTICS 1
#include "glob.h"

/*     --------------------  flatten  --------------------------------  */
/*	Destroys all non-root Dads, leaving all old non-dads (leaf or sub)
which had no children as leaves, direct sons of root. Prepares for a rebuild */
/*	Locks the type of root to dad  */
void flatten() {

    int i;

    set_population();
    tidy(0);
    if (rootcl->num_sons == 0) {
        printf("Nothing to flatten\n");
        return;
    }
    for (i = 0; i <= population->hi_class; i++) {
        if (i == root)
            goto donecls;
        cls = population->classes[i];
        if (cls->type == Vacant)
            goto donecls;
        if (cls->num_sons) { /*  Kill it  */
            cls->type = Vacant;
            cls->dad_id = Deadkilled;
            population->num_classes--;
        } else {
            if (cls->type == Sub) {
                cls->type = Leaf;
                cls->serial = population->next_serial << 2;
                population->next_serial++;
            }
            cls->dad_id = root;
        }
    donecls:;
    }
    nosubs++;
    tidy(0);
    rootcl->type = Dad;
    rootcl->hold_type = Forever;
    do_all(1, 1);
    do_dads(3);
    do_all(3, 0);
    if (heard)
        printf("Flatten ends prematurely\n");
    if (nosubs > 0)
        nosubs--;
    print_tree();
    return;
}

/*	------------------------  insdad  ------------------------------  */
/*	Given 2 class serials, calcs reduction in tree pcost coming from
inserting a new dad with the given classes as sons. If either class is
a sub, or it they have different dads, returns a huge negative benefit  */
/*	Insdad will, however, accept classes s1 and s2 if one is the dad
of the other, provided neither is the root  */
/*	The change, if possible, is made to ctx.popln.
    dadid is set to the id of the new dad, if any.  */
double insert_dad(int ser1, int ser2, int *dadid)
{
    Class *cls1, *cls2, *ndad, *odad;
    EVinst *evi, *fevi;
    CVinst *cvi, *fcvi;
    int nch, iv, k1, k2;
    double origcost, newcost, drop;
    int oldid, newid, od1, od2;

    origcost = rootcl->best_par_cost;
    *dadid = -1;
    k1 = serial_to_id(ser1);
    if (k1 < 0)
        goto nullit;
    k2 = serial_to_id(ser2);
    if (k2 < 0)
        goto nullit;
    cls1 = population->classes[k1];
    if (cls1->type == Sub)
        goto nullit;
    cls2 = population->classes[k2];
    if (cls2->type == Sub)
        goto nullit;
    od1 = cls1->dad_id;
    od2 = cls2->dad_id;
    /*	Normally we expect cls1, cls2 to have same dad, with at least one
        other son	*/
    if (od1 == od2) {
        oldid = od1;
        odad = population->classes[oldid];
        if (odad->num_sons < 3)
            goto nullit;
        goto configok;
    }
    /*	They do not have the same dad, but one may be the son ot the other */
    if (od1 == k2) { /* cls1 is a son of cls2 */
        oldid = od2; /* The dad of cls2 */
        goto configok;
    }
    if (od2 == k1) { /*  cls2 is a son of cls1  */
        oldid = od1;
        goto configok;
    }
    goto nullit;

configok:
    odad = population->classes[oldid];
    newid = make_class();
    if (newid < 0)
        goto nullit;
    ndad = population->classes[newid]; /*  The new dad  */
                                /*	Copy old dad's basics, stats into new dad  */
    nch = ((char *)&odad->id) - ((char *)odad);
    memcpy(ndad, odad, nch);
    ndad->serial = population->next_serial << 2;
    population->next_serial++;
    ndad->age = MinFacAge - 3;
    ndad->hold_type = 0;
    /*      Copy Basics. the structures should have been made.  */
    for (iv = 0; iv < nv; iv++) {
        fcvi = odad->basics[iv];
        cvi = ndad->basics[iv];
        nch = vlist[iv].basic_size;
        memcpy(cvi, fcvi, nch);
    }

    /*      Copy stats  */
    for (iv = 0; iv < nv; iv++) {
        fevi = odad->stats[iv];
        evi = ndad->stats[iv];
        nch = vlist[iv].stats_size;
        memcpy(evi, fevi, nch);
    }

    ndad->dad_id = oldid; /* So new dad is son of old dad */
    cls1->dad_id = cls2->dad_id = newid;
    /*	Set relab and cnt in new dad  */
    ndad->relab = cls1->relab + cls2->relab;
    ndad->weights_sum = cls1->weights_sum + cls2->weights_sum;

    /*	Fix linkages  */
    tidy(0);
    do_dads(20);
    newcost = rootcl->best_par_cost;
    drop = origcost - newcost;
    *dadid = newid;
    goto done;

nullit:
    drop = -1.0e20;

done:
    return (drop);
}

/*	----------------------  bestinsdad ---------------------   */
/*	Returns serial of new dad, or 0 if best no good, or -1 if none
to try  */
int best_insert_dad(int force)
{
    Context oldctx;
    Class *cls1, *cls2;
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
    population->hicl	*/
    nosubs++;
    /*	Do one pass over population to set costs   */
    do_all(1, 1);
    bestdrop = -1.0e20;
    bser1 = bser2 = -1;
    newser = 0;
    set_population();
    origcost = rootcl->best_cost;
    hiid = population->hi_class;
    if (population->num_classes < 4) {
        flp();
        printf("Model has only%2d class\n", population->num_classes);
        succ = -1;
        goto alldone;
    }
    memcpy(&oldctx, &ctx, sizeof(Context));

    i1 = 0;
outer:
    if (i1 == root)
        goto i1done;
    cls1 = population->classes[i1];
    if (!cls1)
        goto i1done;
    if ((cls1->type == Vacant) || (cls1->type == Sub))
        goto i1done;
    ser1 = cls1->serial;
    i2 = i1 + 1;
inner:
    if (i2 == root)
        goto i2done;
    cls2 = population->classes[i2];
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
    newp = copy_population(population->id, 0, "TrialPop");
    if (newp < 0)
        goto popfails;
    ctx.popln = poplns[newp];
    set_population();
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
    memcpy(&ctx, &oldctx, sizeof(Context));
    set_population(); /* Return to original popln */
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
        flp();
        printf("No possible dad insertions\n");
        succ = newser = -1;
        goto finish;
    }
    /*	Copy population to TrialPop, filled  */
    newp = copy_population(population->id, 1, "TrialPop");
    if (newp < 0)
        goto popfails;
    ctx.popln = poplns[newp];
    set_population();
    flp();
    printf("TRYING INSERT %6d,%6d\n", bser1 >> 2, bser2 >> 2);
    res = insert_dad(bser1, bser2, &newid);
    /*	But check it is not killed off   */
    newser = population->classes[newid]->serial;
    control = 0;
    do_all(1, 1);
    control = AdjAll;
    if (heard) {
        printf("BestInsDad ends prematurely\n");
        return (0);
    }
    if (newser != population->classes[newid]->serial)
        newser = 0;
    /*	See if the trial model has improved over original  */
    succ = 1;
    if (rootcl->best_cost < origcost)
        goto winner;
    succ = 0;
    if (force)
        goto winner;
    set_bad_move(1, bser1, bser2);
    newser = 0;
    memcpy(&ctx, &oldctx, sizeof(Context));
    set_population(); /* Return to original popln */
    flp();
    printf("Failed ******\n");
    goto finish;

popfails:
    succ = newser = -1;
    flp();
    printf("Cannot make TrialPop\n");
    goto finish;

winner:
    flp();
    printf("%s\n", (succ) ? "ACCEPTED !!!" : "FORCED");
    print_tree();
    clr_bad_move();
    /*	Reverse roles of 'work' and TrialPop  */
    strcpy(oldctx.popln->name, "TrialPop");
    strcpy(population->name, "work");
    if (succ)
        track_best(1);

finish:
    if (nosubs > 0)
        nosubs--;
    return (newser);
}

/*	---------------  rebuild  --------------------------------  */
/*	Flattens the rebuilds the tree  */
void rebuild() {
    flp();
    printf("Rebuild obsolete\n");
    return;
}

/*	------------------  deldad  -------------------------  */
/*	If class ser is Dad (not root), it is removed, and its sons become
sons of its dad.
    */
double splice_dad(int ser)
{
    Class *son;
    int kk, kkd, kks;
    double drop, origcost, newcost;

    drop = -1.0e20;
    kk = serial_to_id(ser);
    set_class(population->classes[kk]);
    if (cls->type != Dad)
        goto finish;
    if (kk == root)
        goto finish;
    kkd = cls->dad_id;
    if (kkd < 0)
        goto finish;
    if (cls->num_sons <= 0)
        goto finish;
    /*	All seems OK. Fix idads in kk's sons  */
    origcost = rootcl->best_par_cost;
    for (kks = cls->son_id; kks >= 0; kks = son->sib_id) {
        son = population->classes[kks];
        son->dad_id = kkd;
    }
    /*	Now kill off class kk  */
    cls->type = Vacant;
    population->num_classes--;
    /*	Fix linkages  */
    tidy(0);
    do_dads(20);
    newcost = rootcl->best_par_cost;
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
    Class *cls;
    int i1, hiid, newp;
    int bser, ser;
    double res, bestdrop, origcost;

    nosubs++;
    bestdrop = -1.0e20;
    bser = -1;
    set_population();
    /*	Do one pass of doall to set costs  */
    do_all(1, 1);
    origcost = rootcl->best_cost;
    hiid = population->hi_class;
    memcpy(&oldctx, &ctx, sizeof(Context));
    i1 = 0;
loop:
    if (i1 == root)
        goto i1done;
    cls = population->classes[i1];
    if (!cls)
        goto i1done;
    if (cls->type != Dad)
        goto i1done;
    ser = cls->serial;
    if (chk_bad_move(2, 0, ser))
        goto i1done;
    newp = copy_population(population->id, 0, "TrialPop");
    if (newp < 0)
        goto popfails;
    ctx.popln = poplns[newp];
    set_population();
    res = splice_dad(ser);
    if (res < -1000000.0) {
        goto i1done;
    }
    if (res > bestdrop) {
        bestdrop = res;
        bser = ser;
    }
i1done:
    memcpy(&ctx, &oldctx, sizeof(Context));
    set_population();
    i1++;
    if (i1 <= hiid)
        goto loop;

    if (bser < 0) {
        flp();
        printf("No possible dad deletions\n");
        goto finish;
    }
    newp = copy_population(population->id, 1, "TrialPop");
    if (newp < 0)
        goto popfails;
    ctx.popln = poplns[newp];
    set_population();
    flp();
    printf("TRYING DELETE %6d\n", bser >> 2);
    res = splice_dad(bser);
    control = 0;
    do_all(1, 1);
    control = AdjAll;
    if (heard) {
        printf("BestDelDad ends prematurely\n");
        return (0);
    }
    if (rootcl->best_cost < origcost)
        goto winner;
    set_bad_move(2, 0, bser); /* log failure in badmoves */
    bser = 0;
    memcpy(&ctx, &oldctx, sizeof(Context));
    set_population();
    flp();
    printf("Failed ******\n");
    goto finish;

popfails:
    flp();
    printf("BestDelDad cannot make TrialPop\n");
    bser = -1;
    goto finish;

winner:
    flp();
    clr_bad_move();
    printf("ACCEPTED !!!\n");
    print_tree();
    strcpy(oldctx.popln->name, "TrialPop");
    strcpy(population->name, "work");
    track_best(1);

finish:
    if (nosubs > 0)
        nosubs--;
    return (bser);
}

/*	---------------  binhier  --------------------------------  */
/*	If flat, flattens population. Then inserts dads to make a binary hierarchy.
    Then deletes dads as appropriate  */
void binary_hierarchy(int flat) 
{
    int nn;

    set_population();
    if (flat)
        flatten();
    nosubs++;
    if (heard)
        goto kicked;
    clr_bad_move();
insloop:
    nn = best_insert_dad(1);
    if (heard)
        goto kicked;
    if (nn > 0)
        goto insloop;

    try_moves(2);
    if (heard)
        goto kicked;

delloop:
    nn = best_remove_dad();
    if (heard)
        goto kicked;
    if (nn > 0)
        goto delloop;

    try_moves(2);
    if (heard)
        goto kicked;

finish:
    print_tree();
    if (nosubs > 0)
        nosubs--;
    clr_bad_move();
    return;

kicked:
    nn = find_population("work");
    ctx.popln = poplns[nn];
    set_population();
    printf("BinHier ends prematurely\n");
    goto finish;
}

/*	------------------  ranclass  --------------------------  */
/*	To make nn random classes  */
void ranclass( int nn)
{
    int n, ic, ib;
    double bs;
    Class *sub, *dad;

    set_population();
    if (!population) {
        printf("Ranclass needs a model\n");
        goto finish;
    }
    if (!population->sample_size) {
        printf("Model has no sample\n");
        goto finish;
    }
    if (nn > (population->cls_vec_len - 2)) {
        printf("Too many classes\n");
        goto finish;
    }

    nosubs = 0;
    deleteallclasses();
    n = 1;
    if (nn < 2)
        goto finish;

again:
    if (n >= nn)
        goto windup;
    find_all(Leaf);
    /*	Locate biggest leaf with subs aged at least MinAge  */
    ib = -1;
    bs = 0.0;
    for (ic = 0; ic < numson; ic++) {
        cls = sons[ic];
        if (cls->num_sons < 2)
            goto icdone;
        sub = population->classes[cls->son_id];
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
    dad = sons[ib];
    if (split_leaf(dad->id))
        goto windup;
    printf("Splitting %s size%8.1f\n", serial_to_str(dad), dad->weights_sum);
    dad->hold_type = Forever;
    n++;
    goto again;

windup:
    nosubs = 1;
    do_all(5, 1);
    flatten();
    do_all(6, 0);
    do_all(4, 1);
    print_tree();
    rootcl->hold_type = 0;

finish:
    return;
}

/*	---------------  moveclass  --------------------------------  */
/*	To move class ser1 to be a child of class ser2  */
double move_class(int ser1, int ser2)
{
    Class *cls1, *cls2, *odad;
    int k1, k2, od2;
    double origcost, newcost, drop;

    origcost = rootcl->best_par_cost;
    k1 = serial_to_id(ser1);
    if (k1 < 0)
        goto nullit;
    k2 = serial_to_id(ser2);
    if (k2 < 0)
        goto nullit;
    cls1 = population->classes[k1];
    if (cls1->type == Sub)
        goto nullit;
    cls2 = population->classes[k2];
    if (cls2->type != Dad) {
        printf("Class %4d is not a dad\n", ser2);
        goto nullit;
    }
    /*	Check that a change is needed  */
    if (cls1->dad_id == k2) {
        printf("No change needed\n");
        goto nullit;
    }
    /*	Check that cls1 is not an ancestor of cls2  */
    for (od2 = cls2->dad_id; od2 >= 0; od2 = odad->dad_id) {
        if (od2 == k1) {
            printf("Class %4d is ancestor of class %4d\n", ser1, ser2);
            goto nullit;
        }
        odad = population->classes[od2];
    }
    /*	All seems OK, so make change in links   */
    cls1->dad_id = k2;
    /*	Fix linkages  */
    tidy(0);
    do_dads(30);
    if (population->sample_size) {
        do_all(4, 0);
        if (heard)
            goto kicked;
        /* 	To collect weights, counts */
        do_all(4, 1);
        if (heard)
            goto kicked;
    }
    newcost = rootcl->best_par_cost;
    drop = origcost - newcost;
    goto done;

kicked:
    printf("Moveclass interrupted prematurely\n");

nullit:
    drop = -1.0e20;

done:
    return (drop);
}

/*	-----------------  trial  ---------------------------------  */
void trial(int param)
{
    printf("Running TRIAL\n");
    correlpops(param);
}

/*	----------------------  bestmoveclass ---------------------   */
/*	Lokks for the best moveclass. If force, or if an improvement,
does it. Returns 1 if an improvement, 0 if best no improvement, -1 if none
possible.	*/
int best_move_class(int force)
{
    Context oldctx;
    Class *cls1, *cls2, *odad;
    int i1, i2, hiid;
    int bser1, bser2, ser1, ser2, od2, newp, succ;
    double res, bestdrop, origcost;

    /*	We look for all pairs ser1,ser2 of serials where:
    ser2 is a dad, ser1 is not an ancestor of ser2, ser1 is non-sub, and
    ser1 is not a son of ser2.
        For each pair, we copy the pop to TrialPop, switch context to
    TrialPop, and do a moveclass on the pair. We note the pair giving the
    largest moveclass, again copy to TrialPop, repeat the moveclass, and relax
    with doall, .
        */

    /*	To get all pairs, we need a double loop over class indexes. I use
    i1, i2 as the indices.   */
    nosubs++;
    bestdrop = -1.0e20;
    bser1 = bser2 = -1;
    set_population();
    do_all(1, 1); /* To set costs */
    origcost = rootcl->best_cost;
    hiid = population->hi_class;
    if (population->num_classes < 4) {
        flp();
        printf("Model has only%2d class\n", population->num_classes);
        succ = -1;
        goto alldone;
    }
    memcpy(&oldctx, &ctx, sizeof(Context));

    i1 = 0;
outer:
    if (i1 == root)
        goto i1done;
    cls1 = population->classes[i1];
    if (!cls1)
        goto i1done;
    if ((cls1->type == Vacant) || (cls1->type == Sub))
        goto i1done;
    ser1 = cls1->serial;
    i2 = 0;
inner:
    if (i2 == i1)
        goto i2done;
    cls2 = population->classes[i2];
    if (!cls2)
        goto i2done;
    if (cls2->type != Dad)
        goto i2done;
    if (cls1->dad_id == i2)
        goto i2done;
    /*	Check i1 not an ancestor of i2  */
    for (od2 = cls2->dad_id; od2 >= 0; od2 = odad->dad_id) {
        if (od2 == i1)
            goto i2done;
        odad = population->classes[od2];
    }
    ser2 = cls2->serial;
    if (chk_bad_move(3, ser1, ser2))
        goto i2done;

    /*	Copy pop to TrialPop, unfilled  */
    newp = copy_population(population->id, 0, "TrialPop");
    if (newp < 0)
        goto popfails;
    ctx.popln = poplns[newp];
    set_population();
    res = move_class(ser1, ser2);
    if (res > bestdrop) {
        bestdrop = res;
        bser1 = ser1;
        bser2 = ser2;
    }
i2done:
    memcpy(&ctx, &oldctx, sizeof(Context));
    set_population(); /* Return to original popln */
    i2++;
    if (i2 <= hiid)
        goto inner;
i1done:
    i1++;
    if (i1 <= hiid)
        goto outer;

    goto alldone;

alldone:
    if (bser1 < 0) {
        flp();
        succ = -1;
        printf("No possible class move\n");
        goto finish;
    }
    /*	Copy pop to TrialPop, filled  */
    newp = copy_population(population->id, 1, "TrialPop");
    if (newp < 0)
        goto popfails;
    ctx.popln = poplns[newp];
    set_population();
    flp();
    printf("TRYING MOVE CLASS %6d TO DAD %6d\n", bser1 >> 2, bser2 >> 2);
    res = move_class(bser1, bser2);
    control = 0;
    do_all(1, 1);
    control = AdjAll;
    if (heard)
        printf("BestMoveClass ends prematurely\n");
    /*	Setting dogood's target to origcost-1 allows early exit  */
    /*	See if the trial model has improved over original  */
    succ = 1;
    if (rootcl->best_cost < origcost)
        goto winner;
    succ = 0;
    if (force)
        goto winner;
    set_bad_move(3, bser1, bser2);
    memcpy(&ctx, &oldctx, sizeof(Context));
    set_population(); /* Return to original popln */
    flp();
    printf("Failed ******\n");
    goto finish;

popfails:
    succ = -1;
    flp();
    printf("Cannot make TrialPop\n");
    goto finish;

winner:
    flp();
    printf("%s\n", (succ) ? "ACCEPTED !!!" : "FORCED");
    print_tree();
    clr_bad_move();
    /*	Reverse roles of 'work' and TrialPop  */
    strcpy(oldctx.popln->name, "TrialPop");
    strcpy(population->name, "work");
    if (succ)
        track_best(1);

finish:
    if (nosubs > 0)
        nosubs--;
    return (succ);
}

/*	--------------------  trymoves  ----------------------------   */
/*	Tries moving classes using bestmoveclass until ntry attempts in
succession have failed, or until all possible moves have been tried   */
void try_moves(int ntry)
{
    int nfail, succ;

    nosubs++;
    do_all(1, 1);
    clr_bad_move();
    nfail = 0;

    while (nfail < ntry) {
        succ = best_move_class(0);
        if (succ < 0)
            goto finish;
        nfail++;
        if (succ)
            nfail = 0;
        if (heard) {
            printf("Trymoves ends prematurely\n");
            goto finish;
        }
    }

finish:
    if (nosubs > 0)
        nosubs--;
    return;
}
