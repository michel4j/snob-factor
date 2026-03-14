// fiddles with tree structure
#include "snob.h"

/**
 * @brief Destroys all non-root Dads, leaving all old non-dads (leaf or sub)
 * which had no children as leaves, direct sons of root. Prepares for a rebuild
 * Locks the type of root to dad
 * @param ctx Pointer to the Snob context.
 */
void flatten(SnobContext *ctx) {

    int i;
    Class *cls;
    Population *popln = ctx->state.popln;
    Class *root = popln->classes[popln->root];

    tidy(ctx, 0, ctx->no_subs);
    if (root->num_sons == 0) {
        log_msg(ctx, 0, "Nothing to flatten");
        return;
    }
    for (i = 0; i <= popln->hi_class; i++) {
        if (i == popln->root)
            continue;
        cls = popln->classes[i];
        if (cls->type == Vacant)
            continue;
        if (cls->num_sons) { //  Kill it
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
    ctx->no_subs++;
    tidy(ctx, 0, ctx->no_subs);
    root->type = Dad;
    root->hold_type = ctx->forever;
    do_all(ctx, 1, 1);
    do_dads(ctx, 3);
    do_all(ctx, 3, 0);
    if (ctx->interactive & ctx->heard) {
        log_msg(ctx, 0, "Tree flattening ended prematurely");
    }
    if (ctx->no_subs > 0) {
        ctx->no_subs--;
    }
    if (ctx->debug < 1)
        print_tree(ctx);
}

/**
 * @brief Given 2 class serials, calcs reduction in tree pcost coming from
 * inserting a new dad with the given classes as sons. If either class is
 * a sub, or it they have different dads, returns a huge negative benefit
 * Insdad will, however, accept classes s1 and s2 if one is the dad
 * of the other, provided neither is the root
 * The change, if possible, is made to ctx.popln.
 * dadid is set to the id of the new dad, if any.
 * @param ctx Pointer to the Snob context.
 * @param ser1 Identifier or serial number.
 * @param ser2 Identifier or serial number.
 * @param dadid Identifier or serial number.
 */
double insert_dad(SnobContext *ctx, int ser1, int ser2, int *dadid) {
    Class *cls1, *cls2, *ndad, *odad;
    ExplnVar *exp_var, *fexp_var;
    ClassVar *cls_var, *fcls_var;
    int nch, iv, k1, k2;
    double origcost, newcost, drop = -1.0e20;
    int oldid, newid, od1, od2;

    Population *popln = ctx->state.popln;
    Class *root = popln->classes[popln->root];

    origcost = root->best_par_cost;
    *dadid = -1;
    k1 = serial_to_id(ctx, ser1);
    k2 = serial_to_id(ctx, ser2);
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
    /*	Normally we expect cls1, cls2 to have same dad, with at least one other
     * son	*/
    if (od1 == od2) {
        oldid = od1;
        odad = popln->classes[oldid];
        if (odad->num_sons < 3) {
            return drop;
        }
    } else if (od1 == k2) {
        //	They do not have the same dad, but one may be the son ot the other
        // cls1 is a son of cls2
        oldid = od2;        // The dad of cls2
    } else if (od2 == k1) { //  cls2 is a son of cls1
        oldid = od1;
    } else {
        return drop;
    }

    odad = popln->classes[oldid];
    newid = make_class(ctx);
    if (newid >= 0) {
        ndad = popln->classes[newid]; //  The new dad
        //	Copy old dad's basics, stats into new dad
        nch = ((char *)&odad->id) - ((char *)odad);
        memcpy(ndad, odad, nch);
        ndad->serial = popln->next_serial << 2;
        popln->next_serial++;
        ndad->age = ctx->min_fac_age - 3;
        ndad->hold_type = 0;
        // Copy Basics. the structures should have been made.
        for (iv = 0; iv < ctx->state.vset->length; iv++) {
            fcls_var = odad->basics[iv];
            cls_var = ndad->basics[iv];
            nch = ctx->state.vset->variables[iv].basic_size;
            memcpy(cls_var, fcls_var, nch);
        }

        //  Copy stats
        for (iv = 0; iv < ctx->state.vset->length; iv++) {
            fexp_var = odad->stats[iv];
            exp_var = ndad->stats[iv];
            nch = ctx->state.vset->variables[iv].stats_size;
            memcpy(exp_var, fexp_var, nch);
        }

        ndad->dad_id = oldid; // So new dad is son of old dad
        cls1->dad_id = cls2->dad_id = newid;
        //	Set relab and cnt in new dad
        ndad->relab = cls1->relab + cls2->relab;
        ndad->weights_sum = cls1->weights_sum + cls2->weights_sum;

        //	ctx->fix linkages
        tidy(ctx, 0, ctx->no_subs);
        do_dads(ctx, 20);
        newcost = root->best_par_cost;
        drop = origcost - newcost;
        *dadid = newid;
    } else {
        drop = -1.0e20;
    }
    return (drop);
}

/**
 * @brief Returns serial of new dad, or 0 if best no good, or -1 if none
 * to try
 * @param ctx Pointer to the Snob context.
 * @param force Flag to force operation.
 */
int best_insert_dad(SnobContext *ctx, int force) {
    State oldctx;
    Class *cls1, *cls2, *root;
    int i1, i2, hiid;
    int bser1, bser2, ser1, ser2, newp, newid, newser;
    double res, bestdrop, origcost;

    /*	We look for all pairs of non-Sub serials except root.
        For each pair, we copy the population to TrialPop, switch context to
    TrialPop, and do an insdad on the pair. We note the pair giving the largest
    insdad, again copy to TrialPop, repeat the insdad, and relax with
    doall, .
    */

    ctx->no_subs++;
    //	Do one pass over population to set costs
    do_all(ctx, 1, 1);
    bestdrop = -1.0e20;
    bser1 = bser2 = -1;
    newser = 0;

    root = ctx->state.popln->classes[ctx->state.popln->root];
    origcost = root->best_cost;
    hiid = ctx->state.popln->hi_class;
    if (ctx->state.popln->num_classes < 4) {
        log_msg(ctx, 0, "Model has only %2d class", ctx->state.popln->num_classes);
        if (ctx->no_subs > 0)
            ctx->no_subs--;
        return -1;
    }
    memcpy(&oldctx, &ctx->state, sizeof(State));

    for (i1 = 0; i1 < hiid; i1++) {
        if (i1 == ctx->state.popln->root)
            continue;
        cls1 = ctx->state.popln->classes[i1];
        if (!cls1)
            continue;
        if ((cls1->type == Vacant) || (cls1->type == Sub))
            continue;
        ser1 = cls1->serial;
        for (i2 = i1 + 1; i2 <= hiid; i2++) {
            if (i2 == ctx->state.popln->root)
                continue;
            cls2 = ctx->state.popln->classes[i2];
            if (!cls2)
                continue;
            if ((cls2->type == Vacant) || (cls2->type == Sub))
                continue;
            if (cls1->dad_id != cls2->dad_id)
                continue;
            ser2 = cls2->serial;
            if (chk_bad_move(ctx, 1, ser1, ser2))
                continue;

            //	Copy population to TrialPop, unfilled
            newp = copy_population(ctx, ctx->state.popln->id, 0, "TrialPop");
            if (newp < 0) {
                log_msg(ctx, 0, "Cannot make TrialPop");
                if (ctx->no_subs > 0)
                    ctx->no_subs--;
                return -1;
            }
            ctx->state.popln = ctx->populations[newp];
            root = ctx->state.popln->classes[ctx->state.popln->root];
            res = insert_dad(ctx, ser1, ser2, &newid);
            if (newid >= 0) {
                if (res > bestdrop) {
                    bestdrop = res;
                    bser1 = ser1;
                    bser2 = ser2;
                }
            }
            memcpy(&ctx->state, &oldctx, sizeof(State));
            root = ctx->state.popln->classes[ctx->state.popln->root];
        }
    }

    if (bser1 < 0) {
        log_msg(ctx, 0, "No possible dad insertions");
        if (ctx->no_subs > 0)
            ctx->no_subs--;
        return -1;
    }
    //	Copy population to TrialPop, filled
    newp = copy_population(ctx, ctx->state.popln->id, 1, "TrialPop");
    if (newp < 0) {
        log_msg(ctx, 0, "Cannot make TrialPop");
        if (ctx->no_subs > 0)
            ctx->no_subs--;
        return -1;
    }
    ctx->state.popln = ctx->populations[newp];
    root = ctx->state.popln->classes[ctx->state.popln->root];
    log_msg(ctx, 0, "TRYING INSERT %6d,%6d", bser1 >> 2, bser2 >> 2);
    res = insert_dad(ctx, bser1, bser2, &newid);
    //	But check it is not killed off
    newser = ctx->state.popln->classes[newid]->serial;
    ctx->control = 0;
    do_all(ctx, 1, 1);
    ctx->control = AdjAll;
    if (ctx->heard) {
        log_msg(ctx, 0, "BestInsDad ends prematurely");
        if (ctx->no_subs > 0)
            ctx->no_subs--;
        return 0;
    }
    if (newser != ctx->state.popln->classes[newid]->serial)
        newser = 0;

    //	See if the trial model has improved over original
    if (root->best_cost < origcost || force) {
        log_msg(ctx, 0, "%s", (root->best_cost < origcost) ? "ACCEPTED !!!" : "FORCED");
        if (ctx->debug < 1)
            print_tree(ctx);
        clr_bad_move(ctx);
        //	Reverse roles of 'work' and TrialPop
        strcpy(oldctx.popln->name, "TrialPop");
        strcpy(ctx->state.popln->name, "work");
        track_best(ctx, 1);
    } else {
        set_bad_move(ctx, 1, bser1, bser2);
        newser = 0;
        memcpy(&ctx->state, &oldctx, sizeof(State));
        log_msg(ctx, 0, "Attempted Move Unsuccessful ******");
    }

    if (ctx->no_subs > 0)
        ctx->no_subs--;
    return newser;
}

/**
 * @brief Flattens and the rebuilds the tree
 * @param ctx Pointer to the Snob context.
 */
void rebuild(SnobContext *ctx) { log_msg(ctx, 0, "Rebuild obsolete!"); }

/**
 * @brief If class ser is Dad (not root), it is removed, and its sons become
 * sons of its dad.
 *
 * @param ctx Pointer to the Snob context.
 * @param ser Identifier or serial number.
 */
double splice_dad(SnobContext *ctx, int ser) {
    Class *son, *cls;
    int kk, kkd, kks;
    double drop, origcost, newcost;
    Population *popln = ctx->state.popln;
    Class *root = popln->classes[popln->root];

    drop = -1.0e20;
    kk = serial_to_id(ctx, ser);
    cls = popln->classes[kk];
    if (cls->type != Dad)
        return drop;
    if (kk == popln->root)
        return drop;
    kkd = cls->dad_id;
    if (kkd < 0)
        return drop;
    if (cls->num_sons <= 0)
        return drop;
    //	All seems OK. ctx->fix idads in kk's sons
    origcost = root->best_par_cost;
    for (kks = cls->son_id; kks >= 0; kks = son->sib_id) {
        son = popln->classes[kks];
        son->dad_id = kkd;
    }
    //	Now kill off class kk
    cls->type = Vacant;
    popln->num_classes--;
    //	ctx->fix linkages
    tidy(ctx, 0, ctx->no_subs);
    do_dads(ctx, 20);
    newcost = root->best_par_cost;
    drop = origcost - newcost;

    return drop;
}

/**
 * @brief Tries all feasible deldads, does dogood on best and installs
 * as work if an improvement.
 * Returns 1 if accepted, 0 if tried and no good, -1 if none to try
 * @param ctx Pointer to the Snob context.
 */
int best_remove_dad(SnobContext *ctx) {
    State oldctx;
    Class *cls, *root;
    int i1, hiid, newp;
    int bser, ser;
    double res, bestdrop, origcost;
    Population *popln = ctx->state.popln;

    ctx->no_subs++;
    bestdrop = -1.0e20;
    bser = -1;

    root = popln->classes[popln->root];
    //	Do one pass of doall to set costs
    do_all(ctx, 1, 1);
    origcost = root->best_cost;
    hiid = ctx->state.popln->hi_class;
    memcpy(&oldctx, &ctx->state, sizeof(State));

    for (i1 = 0; i1 <= hiid; i1++) {
        if (i1 == popln->root)
            continue;
        cls = ctx->state.popln->classes[i1];
        if (!cls)
            continue;
        if (cls->type != Dad)
            continue;
        ser = cls->serial;
        if (chk_bad_move(ctx, 2, 0, ser))
            continue;

        newp = copy_population(ctx, ctx->state.popln->id, 0, "TrialPop");
        if (newp < 0) {
            log_msg(ctx, 0, "Cannot make trial population during parent removal");
            if (ctx->no_subs > 0)
                ctx->no_subs--;
            return -1;
        }
        popln = ctx->state.popln = ctx->populations[newp];

        root = popln->classes[popln->root];
        res = splice_dad(ctx, ser);
        if (res >= -1000000.0) {
            if (res > bestdrop) {
                bestdrop = res;
                bser = ser;
            }
        }
        memcpy(&ctx->state, &oldctx, sizeof(State));
        popln = ctx->state.popln;
    }

    if (bser < 0) {
        log_msg(ctx, 0, "No possible dad deletions");
        if (ctx->no_subs > 0)
            ctx->no_subs--;
        return -1;
    }

    newp = copy_population(ctx, ctx->state.popln->id, 1, "TrialPop");
    if (newp < 0) {
        log_msg(ctx, 0, "Cannot make trial population during parent removal");
        if (ctx->no_subs > 0)
            ctx->no_subs--;
        return -1;
    }
    popln = ctx->state.popln = ctx->populations[newp];

    root = popln->classes[popln->root];
    log_msg(ctx, 0, "TRYING DELETE %6d", bser >> 2);
    res = splice_dad(ctx, bser);
    ctx->control = 0;
    do_all(ctx, 1, 1);
    ctx->control = AdjAll;
    if (ctx->heard) {
        log_msg(ctx, 0, "Parent removals ended prematurely");
        if (ctx->no_subs > 0)
            ctx->no_subs--;
        return 0;
    }

    if (root->best_cost < origcost) {
        clr_bad_move(ctx);
        log_msg(ctx, 0, "ACCEPTED !!!");
        if (ctx->debug < 1)
            print_tree(ctx);
        strcpy(oldctx.popln->name, "TrialPop");
        strcpy(ctx->state.popln->name, "work");
        track_best(ctx, 1);
    } else {
        set_bad_move(ctx, 2, 0, bser); // log failure in badmoves
        bser = 0;
        memcpy(&ctx->state, &oldctx, sizeof(State));
        log_msg(ctx, 0, "Attempted Move Unsuccessful ******");
    }

    if (ctx->no_subs > 0)
        ctx->no_subs--;
    return bser;
}

/**
 * @brief If flat, flattens population. Then inserts dads to make a binary
 * hierarchy. Then deletes dads as appropriate
 * @param ctx Pointer to the Snob context.
 * @param flat
 */
void binary_hierarchy(SnobContext *ctx, int flat) {
    int nn;

    if (flat)
        flatten(ctx);
    ctx->no_subs++;

    if (ctx->heard) {
        nn = find_population(ctx, "work");
        ctx->state.popln = ctx->populations[nn];
        log_msg(ctx, 0, "Binary hierarchy ended prematurely");
    } else {
        clr_bad_move(ctx);

        do {
            nn = best_insert_dad(ctx, 1);
            if (ctx->heard)
                break;
        } while (nn > 0);

        if (!ctx->heard) {
            try_moves(ctx, 2);
            if (!ctx->heard) {
                do {
                    nn = best_remove_dad(ctx);
                    if (ctx->heard)
                        break;
                } while (nn > 0);

                if (!ctx->heard) {
                    try_moves(ctx, 2);
                }
            }
        }

        if (ctx->heard) {
            nn = find_population(ctx, "work");
            ctx->state.popln = ctx->populations[nn];
            log_msg(ctx, 0, "Binary hierarchy ended prematurely");
        }
    }

    if (ctx->debug < 1)
        print_tree(ctx);
    if (ctx->no_subs > 0)
        ctx->no_subs--;
    clr_bad_move(ctx);
    return;
}

/**
 * @brief To make nn random classes
 * @param ctx Pointer to the Snob context.
 * @param nn
 */
void ranclass(SnobContext *ctx, int nn) {
    int n, ic, ib, num_son;
    double bs;
    Class *sub, *dad, *cls;
    Population *popln = ctx->state.popln;
    Class *root = popln->classes[popln->root];

    if (!popln) {
        log_msg(ctx, 0, "Ranclass needs a model");
        return;
    }
    if (!popln->sample_size) {
        log_msg(ctx, 0, "Model has no sample");
        return;
    }
    if (nn > (popln->cls_vec_len - 2)) {
        log_msg(ctx, 0, "Too many classes");
        return;
    }

    ctx->no_subs = 0;
    delete_all_classes(ctx);
    n = 1;
    if (nn < 2)
        return;

    while (n < nn) {
        num_son = find_all(ctx, Leaf);
        //	Locate biggest leaf with subs aged at least ctx->min_age
        ib = -1;
        bs = 0.0;
        for (ic = 0; ic < num_son; ic++) {
            cls = ctx->sons[ic];
            if (cls->num_sons < 2)
                continue;
            sub = popln->classes[cls->son_id];
            if (sub->age < ctx->min_age)
                continue;
            if (cls->weights_sum > bs) {
                bs = cls->weights_sum;
                ib = ic;
            }
        }

        if (ib < 0) {
            do_all(ctx, 1, 1);
            continue; // try again
        }

        //	Split sons[ib]
        dad = ctx->sons[ib];
        if (split_leaf(ctx, dad->id))
            break; // go to windup

        log_msg(ctx, 0, "Splitting %s size%8.1f", serial_to_str(ctx, dad), dad->weights_sum);
        dad->hold_type = ctx->forever;
        n++;
    }

    ctx->no_subs = 1;
    do_all(ctx, 5, 1);
    flatten(ctx);
    do_all(ctx, 6, 0);
    do_all(ctx, 4, 1);
    if (ctx->debug < 1)
        print_tree(ctx);
    root->hold_type = 0;

    return;
}

/**
 * @brief To move class ser1 to be a child of class ser2
 * @param ctx Pointer to the Snob context.
 * @param ser1 Identifier or serial number.
 * @param ser2 Identifier or serial number.
 */
double move_class(SnobContext *ctx, int ser1, int ser2) {
    Class *cls1, *cls2, *odad;
    int k1, k2, od2;
    double origcost, newcost, drop;

    Population *popln = ctx->state.popln;
    Class *root = popln->classes[popln->root];

    origcost = root->best_par_cost;
    k1 = serial_to_id(ctx, ser1);
    if (k1 < 0)
        return -1.0e20;
    k2 = serial_to_id(ctx, ser2);
    if (k2 < 0)
        return -1.0e20;
    cls1 = popln->classes[k1];
    if (cls1->type == Sub)
        return -1.0e20;
    cls2 = popln->classes[k2];
    if (cls2->type != Dad) {
        log_msg(ctx, 0, "Class %4d is not a dad", ser2);
        return -1.0e20;
    }
    //	Check that a change is needed
    if (cls1->dad_id == k2) {
        log_msg(ctx, 0, "No change needed");
        return -1.0e20;
    }
    //	Check that cls1 is not an ancestor of cls2
    for (od2 = cls2->dad_id; od2 >= 0; od2 = odad->dad_id) {
        if (od2 == k1) {
            log_msg(ctx, 0, "Class %4d is ancestor of class %4d", ser1, ser2);
            return -1.0e20;
        }
        odad = popln->classes[od2];
    }
    //	All seems OK, so make change in links
    cls1->dad_id = k2;
    //	ctx->fix linkages
    tidy(ctx, 0, ctx->no_subs);
    do_dads(ctx, 30);
    if (popln->sample_size) {
        do_all(ctx, 4, 0);
        if (ctx->heard) {
            log_msg(ctx, 0, "Class moves ended prematurely");
            return -1.0e20;
        }
        // 	To collect weights, counts
        do_all(ctx, 4, 1);
        if (ctx->heard) {
            log_msg(ctx, 0, "Class moves ended prematurely");
            return -1.0e20;
        }
    }
    newcost = root->best_par_cost;
    drop = origcost - newcost;

    return drop;
}

/**
 * @param ctx Pointer to the Snob context.
 * @param param
 */
void trial(SnobContext *ctx, int param) {
    log_msg(ctx, 0, "Running TRIAL");
    correlpops(ctx, param);
}

/**
 *
 * @brief Looks for the best moveclass. If force, or if an improvement,
 * does it. Returns 1 if an improvement, 0 if best no improvement, -1 if none
 * possible.
 *
 * @param ctx Pointer to the Snob context.
 * @param force Flag to force operation.
 */
int best_move_class(SnobContext *ctx, int force) {
    State oldctx;
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
    ctx->no_subs++;
    bestdrop = -1.0e20;
    bser1 = bser2 = -1;

    root = ctx->state.popln->classes[ctx->state.popln->root];
    do_all(ctx, 1, 1); // To set costs
    origcost = root->best_cost;
    hiid = ctx->state.popln->hi_class;
    if (ctx->state.popln->num_classes < 4) {
        log_msg(ctx, 0, "Model has only%2d class", ctx->state.popln->num_classes);
        if (ctx->no_subs > 0)
            ctx->no_subs--;
        return -1;
    }
    memcpy(&oldctx, &ctx->state, sizeof(State));

    for (i1 = 0; i1 <= hiid; i1++) {
        if (i1 == ctx->state.popln->root) {
            continue;
        }
        cls1 = ctx->state.popln->classes[i1];
        if ((!cls1) || (cls1->type == Vacant) || (cls1->type == Sub)) {
            continue;
        }
        ser1 = cls1->serial;
        for (i2 = 0; i2 <= hiid; i2++) {
            if (i2 == i1) {
                continue;
            }
            cls2 = ctx->state.popln->classes[i2];
            if ((!cls2) || (cls2->type != Dad) || (cls1->dad_id == i2)) {
                continue;
            }
            //	Check i1 not an ancestor of i2
            int is_ancestor = 0;
            for (od2 = cls2->dad_id; od2 >= 0; od2 = odad->dad_id) {
                if (od2 == i1) {
                    is_ancestor = 1;
                    break;
                }
                odad = ctx->state.popln->classes[od2];
            }
            if (is_ancestor)
                continue;

            ser2 = cls2->serial;
            if (chk_bad_move(ctx, 3, ser1, ser2)) {
                continue;
            }

            //	Copy pop to TrialPop, unfilled
            newp = copy_population(ctx, ctx->state.popln->id, 0, "TrialPop");
            if (newp < 0) {
                log_msg(ctx, 0, "Cannot make trial population during class move");
                if (ctx->no_subs > 0)
                    ctx->no_subs--;
                return -1;
            }
            ctx->state.popln = ctx->populations[newp];
            root = ctx->state.popln->classes[ctx->state.popln->root];
            res = move_class(ctx, ser1, ser2);
            if (res > bestdrop) {
                bestdrop = res;
                bser1 = ser1;
                bser2 = ser2;
            }
            memcpy(&ctx->state, &oldctx, sizeof(State));
        }
    }

    if (bser1 < 0) {
        log_msg(ctx, 0, "No possible class move");
        if (ctx->no_subs > 0)
            ctx->no_subs--;
        return -1;
    }

    //	Copy pop to TrialPop, filled
    newp = copy_population(ctx, ctx->state.popln->id, 1, "TrialPop");
    if (newp < 0) {
        log_msg(ctx, 0, "Cannot make trial population during class move");
        if (ctx->no_subs > 0)
            ctx->no_subs--;
        return -1;
    }
    ctx->state.popln = ctx->populations[newp];

    root = ctx->state.popln->classes[ctx->state.popln->root];
    log_msg(ctx, 0, "TRYING MOVE CLASS %6d TO DAD %6d", bser1 >> 2, bser2 >> 2);
    res = move_class(ctx, bser1, bser2);
    ctx->control = 0;
    do_all(ctx, 1, 1);
    ctx->control = AdjAll;
    if (ctx->heard)
        log_msg(ctx, 0, "Class moves ended prematurely");

    //	Setting dogood's target to origcost-1 allows early exit
    //	See if the trial model has improved over original
    if (root->best_cost < origcost || force) {
        succ = 1;
        log_msg(ctx, 0, "%s", (root->best_cost < origcost) ? "ACCEPTED !!!" : "FORCED");
        if (ctx->debug < 1)
            print_tree(ctx);
        clr_bad_move(ctx);
        //	Reverse roles of 'work' and TrialPop
        strcpy(oldctx.popln->name, "TrialPop");
        strcpy(ctx->state.popln->name, "work");
        track_best(ctx, 1);
    } else {
        succ = 0;
        set_bad_move(ctx, 3, bser1, bser2);
        memcpy(&ctx->state, &oldctx, sizeof(State));
        log_msg(ctx, 0, "Attempted Move Unsuccessful ******");
    }

    if (ctx->no_subs > 0)
        ctx->no_subs--;
    return succ;
}

/**
 * @brief Tries moving classes using bestmoveclass until ntry attempts in
 * succession have failed, or until all possible moves have been tried
 * @param ctx Pointer to the Snob context.
 * @param ntry
 */
void try_moves(SnobContext *ctx, int ntry) {
    int nfail, succ;

    ctx->no_subs++;
    do_all(ctx, 1, 1);
    clr_bad_move(ctx);
    nfail = 0;

    while (nfail < ntry) {
        succ = best_move_class(ctx, 0);
        if (succ < 0)
            break;
        nfail++;
        if (succ)
            nfail = 0;
        if ((ctx->heard) || (ctx->stop)) {
            log_msg(ctx, 0, "Move attempts ended prematurely");
            break;
        }
    }

    if (ctx->no_subs > 0)
        ctx->no_subs--;
    return;
}
