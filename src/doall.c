
#define DOALL 1
#include "snob.h"
#ifdef _OPENMP
#include <omp.h>
#endif

// rand_int, rand_uint, rand_float
static double rcons = (1.0 / (2048.0 * 1024.0 * 1024.0));
#define M32 0xFFFFFFFF
#define B32 0x80000000
int rand_int(SnobContext *ctx) {
    int js;
    ctx->random_seed = 69069 * ctx->random_seed + 103322787;
    js = ctx->random_seed & M32;
    return (js);
}

int rand_uint(SnobContext *ctx) {
    int js;
    ctx->random_seed = 69069 * ctx->random_seed + 103322787;
    js = ctx->random_seed & M32;
    if (js & B32)
        js = M32 - js;
    return (js & M32);
}

double rand_float(SnobContext *ctx) {
    int js;
    ctx->random_seed = 69069 * ctx->random_seed + 103322787;
    js = ctx->random_seed & M32;
    if (js & B32)
        js = M32 - js;
    return (rcons * js);
}

/**
 * @brief Finds all classes of type(s) shown in bits of 'class_type'.
 * (Dad = 1, Leaf = 2, Sub = 4), so if typ = 7, will find all classes.
 * Sets the classes in 'sons[]'
 * Puts count of classes found in numson
 * @param ctx Pointer to the Snob context.
 * @param class_type Pointer to the class.
 */
int find_all(SnobContext *ctx, int class_type) {
    int i, j, num_son;
    Class *cls;
    Population *popln = ctx->state.popln;
    Class *root = ctx->state.popln->classes[ctx->state.popln->root];

    tidy(ctx, 1, ctx->no_subs);
    j = 0;
    cls = root;

    while (cls) {
        if (class_type & cls->type) {
            ctx->sons[j++] = cls;
        }
        next_class(ctx, &cls);
    }
    num_son = j;

    // Set indices in nextic[] for non-descendant classes
    for (i = 0; i < num_son; i++) {
        int idi = ctx->sons[i]->id;

        for (j = i + 1; j < num_son; j++) {
            cls = ctx->sons[j];
            while (cls->id != idi) {
                if (cls->dad_id < 0) {
                    break;
                }
                cls = popln->classes[cls->dad_id];
            }
            if (cls->dad_id < 0) {
                break;
            }
        }
        ctx->next_ic[i] = (j < num_son) ? j : num_son;
    }
    return num_son;
}

/**
 * @brief To re-arrange the sons in the son chain of class kk in order of
 * increasing serial number
 * @param ctx Pointer to the Snob context.
 * @param index Class index
 */
void sort_sons(SnobContext *ctx, int index) {
    Class *cls, *cls1, *cls2;
    int js, *prev, nsw;
    Population *popln = ctx->state.popln;
    cls = popln->classes[index];
    if (cls->num_sons < 2) {
        return;
    }

    do {
        prev = &(cls->son_id);
        nsw = 0;

        cls1 = popln->classes[*prev];
        while (cls1->sib_id >= 0) {
            cls2 = popln->classes[cls1->sib_id];
            if (cls1->serial > cls2->serial) {
                *prev = cls2->id;
                cls1->sib_id = cls2->sib_id;
                cls2->sib_id = cls1->id;
                prev = &cls2->sib_id;
                nsw = 1;
            } else {
                prev = &cls1->sib_id;
            }
            cls1 = popln->classes[*prev];
        }
    } while (nsw);
    //	Now sort sons
    for (js = cls->son_id; js >= 0; js = popln->classes[js]->sib_id) {
        sort_sons(ctx, js);
    }
}

/**
 * @brief Reconstructs ison, isib, nson linkages from CurDadId-s. If hit
 * and AdjTr, kills classes which are too small.
 * Also deletes singleton sonclasses.  Re-counts pop->ncl, pop->hicl,
 * pop->num_leaves.
 *
 * @param ctx Pointer to the Snob context.
 * @param hit Hit flag.
 * @param no_subs
 */
void tidy(SnobContext *ctx, int hit, int no_subs) {
    Class *cls, *dad, *son;
    int i, kkd, ndead, newhicl, cause;
    Population *popln = ctx->state.popln;

    if (!popln->sample_size)
        hit = 0;
    do {
        ndead = 0;
        for (i = 0; i <= popln->hi_class; i++) {
            cls = popln->classes[i];
            if ((!cls) || (cls->type == Vacant) || (i == popln->root)) {

                if (i == popln->root) {
                    cls->num_sons = 0;
                    cls->son_id = cls->sib_id = -1;
                }
                continue;
            }
            cls->num_sons = 0;
            cls->son_id = cls->sib_id = -1;

            kkd = cls->dad_id;
            if (kkd < 0) {
                log_msg(ctx, 2, "\nDad error in tidy\n");
                return; // Previously infinite loop for(;;) ;
            }
            int hard = 0;
            if (hit && (cls->weights_sum < ctx->min_size)) {
                cause = Deadsmall;
                hard = 1;
            } else if (hit && (cls->type == Sub) && ((cls->age > ctx->max_sub_age) || no_subs)) {
                cause = Dead;
                hard = 2;
            } else if (popln->classes[kkd]->type == Vacant) {
                cause = Deadorphan;
                hard = 2;
            }

            if ((hard == 2) || ((hard == 1) && (ctx->control & AdjTr))) {
                if (ctx->see_all < 2)
                    ctx->see_all = 2;
                cls->dad_id = cause;
                cls->type = Vacant;
                ndead++;
            }
        }
        if (ndead)
            continue;

        //	No more classes to kill for the moment.  Relink everyone
        popln->num_classes = 0;
        kkd = 0;

        for (i = 0; i <= popln->hi_class; i++) {
            cls = popln->classes[i];
            if ((cls->type == Vacant) || (i == popln->root)) {
                continue;
            }
            dad = popln->classes[cls->dad_id];
            cls->sib_id = dad->son_id;
            dad->son_id = i;
            dad->num_sons++;
        }

        //	Check for singleton sons
        for (kkd = 0; kkd <= popln->hi_class; kkd++) {
            dad = popln->classes[kkd];
            if ((dad->type == Vacant) || (dad->num_sons != 1)) {
                continue;
            }

            cls = popln->classes[dad->son_id];
            //	Clp is dad's only son. If a sub, kill it
            //	If not, make dad inherit clp's role, then kill clp
            if (cls->type == Sub) {
                cause = Dead;
            } else {
                if (ctx->see_all < 2)
                    ctx->see_all = 2;
                dad->type = cls->type;
                dad->use = cls->use;
                dad->hold_type = cls->hold_type;
                dad->hold_use = cls->hold_use;
                dad->num_sons = cls->num_sons;
                dad->son_id = cls->son_id;
                // Change the dad in clp's sons
                for (i = dad->son_id; i >= 0; i = son->sib_id) {
                    son = popln->classes[i];
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
    if (hit && (ctx->control & AdjTr) && ctx->new_subs) {
        // Add subclasses to large-enough leaves
        for (i = 0; i <= popln->hi_class; i++) {
            dad = popln->classes[i];
            // Check if conditions are met to make subclasses
            if (dad->type == Leaf && !dad->num_sons && dad->weights_sum >= (2.1 * ctx->min_size) &&
                dad->age >= ctx->min_age) {
                make_subclasses(ctx, i);
                kkd++;
            }
        }
    }

    // Re-count classes, leaves etc.
    popln->num_classes = popln->num_leaves = newhicl = kkd = 0;
    for (i = 0; i <= popln->hi_class; i++) {
        cls = popln->classes[i];
        if (cls && cls->type != Vacant) {
            if (cls->type == Leaf)
                popln->num_leaves++;
            popln->num_classes++;
            newhicl = i;
            if (cls->serial > kkd)
                kkd = cls->serial;
        }
    }
    popln->hi_class = newhicl;
    popln->next_serial = (kkd >> 2) + 1;
    sort_sons(ctx, popln->root);
}

/**
 * @brief Do a complete cost-assign-adjust cycle on all things.
 * If 'all', does it for all classes, else just leaves
 * Leaves in scorechanges a count of significant score changes in Leaf
 * classes whose use is Fac
 * @param ctx Pointer to the Snob context.
 * @param n_iter Number of iterations.
 * @param n_cycles Number of cycles.
 */

void update_seeall_newsubs(SnobContext *ctx, int n_iter, int n_cycles) {

    if ((n_iter % ctx->new_subs_time) == 0) {
        ctx->new_subs = 1;
        if (ctx->see_all < 2)
            ctx->see_all = 2;
    } else {
        ctx->new_subs = 0;
    }
    if ((n_cycles - n_iter) <= 2) {
        ctx->see_all = n_cycles - n_iter;
    }
    if (n_cycles < 2) {
        ctx->see_all = 2;
    }
    if (ctx->no_subs) {
        ctx->new_subs = 0;
    }
    if ((n_iter > ctx->new_subs_time) && (ctx->see_all == 1)) {
        track_best(ctx, 0);
    }
}

static void reduce_class(SnobContext *ctx, Class *dst, Class *src) {
    dst->newcnt += src->newcnt;
    dst->newvsq += src->newvsq;
    dst->vav += src->vav;
    dst->totvv += src->totvv;

    dst->cstcost += src->cstcost;
    dst->cftcost += src->cftcost;
    dst->cntcost += src->cntcost;
    dst->cfvcost += src->cfvcost;

    dst->score_change_count += src->score_change_count;
    dst->scancnt += src->scancnt;

    for (int i = 0; i < ctx->state.vset->length; i++) {
        VSetVar *vset_var = &ctx->state.vset->variables[i];
        if (!vset_var->inactive) {
            VarType *vtype = vset_var->vtype;
            if (vtype->reduce_stats) {
                (*vtype->reduce_stats)(ctx, i, dst, src);
            }
        }
    }
}

void find_and_estimate(SnobContext *ctx, int *all, int n_iter, int n_cycles) {
    int repeat, num_son;
    do {
        repeat = 0;
        if (ctx->fix == Random)
            ctx->see_all = 3;
        tidy(ctx, 1, ctx->no_subs);
        if (n_iter >= (n_cycles - 1))
            *all = (Dad + Leaf + Sub);
        num_son = find_all(ctx, *all);

        for (int k = 0; k < num_son; k++) {
            clear_costs(ctx, ctx->sons[k]);
        }

        int num_cases = ctx->state.sample->num_cases;

#pragma omp parallel
        {
            SnobContext local_ctx = *ctx;

            Population local_popln = *ctx->state.popln;
            Class **local_classes = malloc((local_popln.hi_class + 1) * sizeof(Class *));
            memset(local_classes, 0, (local_popln.hi_class + 1) * sizeof(Class *));
            local_popln.classes = local_classes;
            local_ctx.state.popln = &local_popln;

            Sample local_sample = *ctx->state.sample;
            SampleVar *local_sample_vars = malloc(ctx->state.vset->length * sizeof(SampleVar));
            for (int i = 0; i < ctx->state.vset->length; i++) {
                local_sample_vars[i] = ctx->state.sample->variables[i];
                int saux_size = ctx->state.vset->variables[i].vtype->smpl_aux_size;
                if (saux_size > 0 && ctx->state.sample->variables[i].saux) {
                    local_sample_vars[i].saux = malloc(saux_size + 128);
                    memcpy(local_sample_vars[i].saux, ctx->state.sample->variables[i].saux, saux_size);
                } else {
                    local_sample_vars[i].saux = NULL;
                }
            }
            local_sample.variables = local_sample_vars;
            local_ctx.state.sample = &local_sample;

            State local_state = ctx->state;
            local_state.popln = &local_popln;
            local_state.sample = &local_sample;
            local_ctx.state = local_state;

            for (int k = 0; k <= ctx->state.popln->hi_class; k++) {
                Class *src = ctx->state.popln->classes[k];
                if (src && src->type != Vacant) {
                    Class *dst = malloc(sizeof(Class) + 256); // Pad to prevent cache line false
                                                              // sharing of class variables
                    *dst = *src;
                    dst->scancnt = 0; // Prevent accumulating start value multiple times

                    // Fields that must start at zero for proper reduction, but might not
                    // have been cleared if not in ctx->sons
                    dst->newcnt = 0.0;
                    dst->newvsq = 0.0;
                    dst->vav = 0.0;
                    dst->totvv = 0.0;
                    dst->cstcost = 0.0;
                    dst->cftcost = 0.0;
                    dst->cntcost = 0.0;
                    dst->cfvcost = 0.0;
                    dst->score_change_count = 0;

                    dst->factor_scores = src->factor_scores;
                    dst->basics = src->basics;
                    dst->stats = malloc(ctx->state.vset->length * sizeof(ExplnVar *));
                    for (int i = 0; i < ctx->state.vset->length; i++) {
                        VSetVar *vset_var = &ctx->state.vset->variables[i];
                        dst->stats[i] = malloc(vset_var->stats_size + 128);
                        memcpy(dst->stats[i], src->stats[i], vset_var->stats_size);
                    }
                    local_classes[k] = dst;
                }
            }

            for (int k = 0; k < num_son; k++) {
                local_ctx.sons[k] = local_classes[ctx->sons[k]->id];
            }

#ifdef _OPENMP
            local_ctx.random_seed = ctx->random_seed + omp_get_thread_num();
#endif

#pragma omp for
            for (int j = 0; j < num_cases; j++) {
                do_case(&local_ctx, j, *all, 1, num_son);
            }

#pragma omp critical
            {
                for (int k = 0; k < num_son; k++) {
                    int id = ctx->sons[k]->id;
                    if (local_classes[id]) {
                        reduce_class(ctx, ctx->state.popln->classes[id], local_classes[id]);
                    }
                }
            }

            // Clean up classes
            for (int k = 0; k <= ctx->state.popln->hi_class; k++) {
                if (local_classes[k]) {
                    for (int i = 0; i < ctx->state.vset->length; i++) {
                        free(local_classes[k]->stats[i]);
                    }
                    free(local_classes[k]->stats);
                    free(local_classes[k]);
                }
            }
            free(local_classes);

            // Clean up sample vars
            for (int i = 0; i < ctx->state.vset->length; i++) {
                if (local_sample_vars[i].saux) {
                    free(local_sample_vars[i].saux);
                }
            }
            free(local_sample_vars);
        }

        if (ctx->control & (AdjPr + AdjTr)) {
            for (int k = 0; k < num_son; k++) {
                if (ctx->sons[k]->newcnt < ctx->min_size) {
                    ctx->sons[k]->weights_sum = 0.0;
                    ctx->sons[k]->type = Vacant;
                    ctx->see_all = 2;
                    ctx->new_subs = 0;
                    repeat = 1;
                    break;
                }
            }
        }
    } while (repeat);
}

/**
 * @brief Update leaf classes
 * @param ctx Pointer to the Snob context.
 * @param old_leaf_sum Pointer to the old leaf sum.
 * @param n_fail Pointer to the number of failures.
 * @param num_son Number of sons.
 */
double update_leaf_classes(SnobContext *ctx, double *old_leaf_sum, int *n_fail, int num_son) {
    double leafsum = 0.0;
    char token = ' ';
    leafsum = 0.0;

    for (int ic = 0; ic < num_son; ic++) {
        adjust_class(ctx, ctx->sons[ic], 0);
        //	The second para tells adjust not to do as-dad params
        leafsum += ctx->sons[ic]->best_cost;
    }
    if (ctx->see_all == 0) {
        token = '.';
    } else if (leafsum < (*old_leaf_sum - ctx->min_gain)) {
        (*n_fail) = 0;
        *old_leaf_sum = leafsum;
        token = 'L';
    } else {
        (*n_fail)++;
        token = 'l';
    }
    rep(ctx, token);

    return leafsum;
}

/**
 * @brief Update all classes
 * @param ctx Pointer to the Snob context.
 * @param oldcost Pointer to the old cost.
 * @param nfail Pointer to the number of failures.
 */
void update_all_classes(SnobContext *ctx, double *oldcost, int *nfail) {
    Population *popln = ctx->state.popln;
    Class *root = popln->classes[popln->root];
    Class *dad, *cls = root;
    int adjusted = 0;

    while (!adjusted) {
        cls->dad_par_cost = 0.0;
        if (cls->num_sons >= 2) {
            dad = cls;
            cls = popln->classes[cls->son_id];
            continue;
        } else {
            int complete = 0;
            while (!complete) {
                adjust_class(ctx, cls, 1);
                if (cls->dad_id < 0) {
                    adjusted = 1;
                    complete = 1;
                } else {
                    dad = popln->classes[cls->dad_id];
                    dad->dad_par_cost += cls->best_par_cost;
                    if (cls->sib_id >= 0) {
                        cls = popln->classes[cls->sib_id];
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
    if (ctx->see_all == 0) {
        rep(ctx, '.');
    } else {
        if (root->best_cost < (*oldcost - ctx->min_gain)) {
            (*nfail) = 0;
            *oldcost = root->best_cost;
            rep(ctx, 'A');
        } else {
            (*nfail)++;
            rep(ctx, 'a');
        }
    }
}

/**
 * @brief Scan leaf classes whose use is 'Fac' to accumulate significant
 * score changes
 * @param ctx Pointer to the Snob context.
 */
int count_score_changes(SnobContext *ctx) {
    Class *cls;
    Population *popln = ctx->state.popln;

    int scorechanges = 0;
    for (int k = 0; k <= popln->hi_class; k++) {
        cls = popln->classes[k];
        if (cls && (cls->type == Leaf) && (cls->use == Fac))
            scorechanges += cls->score_change_count;
    }
    return scorechanges;
}

/**
 * @brief Assigns data to classes and optimizes continuous parameters, using an Expectation-Maximization
 * (EM) algorithm. For each item in the dataset, evaluates its cost against all potential classes and assign
 * soft membership weights to each class. Once all items have calculated their weights for all classes,
 * the parameters of the classes are updated to minimize the local MML cost.
 * @param ctx Pointer to the Snob context.
 * @param n_cycles Number of cycles.
 * @param all All classes.
 */
int do_all(SnobContext *ctx, int n_cycles, int all) {
    int niter, nfail, ic, ncydone, ncyask, kicked = 0, num_son;
    double oldcost, oldleafsum;

    Population *popln = ctx->state.popln;
    Class *root = popln->classes[popln->root];

    nfail = niter = ncydone = 0;
    ncyask = n_cycles;
    all = (all) ? (Dad + Leaf + Sub) : Leaf;
    oldcost = root->best_cost;
    //	Get sum of class costs, meaningful only if 'all' = Leaf
    oldleafsum = 0.0;
    num_son = find_all(ctx, Leaf);
    for (ic = 0; ic < num_son; ic++) {
        oldleafsum += ctx->sons[ic]->best_cost;
    }

    while (niter < n_cycles) {
        update_seeall_newsubs(ctx, niter, n_cycles);
        find_and_estimate(ctx, &all, niter, n_cycles);

        if (all != (Dad + Leaf + Sub)) {
            update_leaf_classes(ctx, &oldleafsum, &nfail, num_son);
        } else {
            // all = 7, so we have dads, leaves and subs to do.
            // We do from bottom up, collecting as-dad pcosts.
            update_all_classes(ctx, &oldcost, &nfail);
        }

        if (nfail > ctx->give_up) {
            if (all != Leaf)
                break;
            /*	But if we were doing just leaves, wind up with a couple of
                'doall' cycles  */
            all = Dad + Leaf + Sub;
            n_cycles = 2;
            niter = nfail = 0;
            continue;
        }
        if (((ctx->interactive) && (!ctx->use_stdin) && hark(ctx, CommsBuffer.inl)) || (ctx->stop)) {
            kicked = 1;
            break;
        }
        if (ctx->see_all > 0)
            ctx->see_all--;
        ncydone++;
        niter++;
    }
    if (ncydone >= ncyask)
        ncydone = -1;

    if (kicked) {
        log_msg(ctx, 1, "\nDoall interrupted after %4d steps", ncydone);
    }
    /*	Scan leaf classes whose use is 'Fac' to accumulate significant
        score changes.  */
    ctx->score_changes = count_score_changes(ctx);
    return (ncydone);
}

/**
 * @brief Runs adjustclass on all leaves without adjustment.
 * This leaves class cb*costs set up. Adjustclass is told not to
 * consider a leaf as a potential dad.
 * Then runs ncostvarall on all dads, with param adjustment. The
 * result is to recost and readjust the tree hierarchy.
 *
 * @param ctx Pointer to the Snob context.
 * @param n_cycles Number of cycles.
 */
int do_dads(SnobContext *ctx, int n_cycles) {
    Class *dad, *cls;
    double oldcost;
    int nn, n_fail, num_son;
    Population *popln = ctx->state.popln;
    Class *root = popln->classes[popln->root];
    if (!(ctx->control & AdjPr))
        n_cycles = 1;

    //	Capture no-prior params for subless leaves
    num_son = find_all(ctx, Leaf);
    n_fail = ctx->control;
    ctx->control = Noprior;
    for (nn = 0; nn < num_son; nn++) {
        adjust_class(ctx, ctx->sons[nn], 0);
    }
    ctx->control = n_fail;
    nn = n_fail = 0;

    do {
        oldcost = root->dad_par_cost;
        if (root->type != Dad) {
            return (0);
        }
        //	Begin a recursive scan of classes down to leaves
        cls = root;

        while (1) {
            // Traverse down to the leftmost leaf
            while (cls->type != Leaf) {
                cls->dad_par_cost = 0.0;
                cls->relab = cls->weights_sum = 0.0;
                dad = cls;
                cls = popln->classes[cls->son_id];
            }

            // Traverse up and right
            int all_adjusted = 0;
            while (1) {
                //	If a leaf, use adjustclass, else use ncostvarall
                if (cls->type == Leaf) {
                    ctx->control = Tweak;
                    adjust_class(ctx, cls, 0);
                } else {
                    ctx->control = AdjPr;
                    parent_cost_all_vars(ctx, cls, 1);
                    cls->best_par_cost = cls->dad_par_cost;
                }

                if (cls->dad_id < 0) {
                    all_adjusted = 1;
                    break;
                }

                dad = popln->classes[cls->dad_id];
                dad->dad_par_cost += cls->best_par_cost;
                dad->weights_sum += cls->weights_sum;
                dad->relab += cls->relab;

                if (cls->sib_id >= 0) {
                    cls = popln->classes[cls->sib_id];
                    break; /* Break the inner traversal loop to go down again starting at
                              the sibling */
                }
                cls = dad;
            }

            if (all_adjusted) {
                break; // Done with tree traversal
            }
        }

        root->best_par_cost = root->dad_par_cost;
        root->best_cost = root->dad_par_cost + root->cntcost;
        //	Test for convergence
        nn++;
        n_fail++;
        if (root->dad_par_cost < (oldcost - ctx->min_gain))
            n_fail = 0;
        rep(ctx, (n_fail) ? 'd' : 'D');
        if (n_fail > 3) {
            ctx->control = ctx->d_control;
            return (nn);
        }
    } while (nn < n_cycles);
    ctx->control = ctx->d_control;
    return (-1);
}

/**
 * @brief Does cycles combining doall, doleaves, dodads
 */

//	Uses this table of old costs to see if useful change in last 5 cycles
double olddogcosts[6];

int do_good(SnobContext *ctx, int n_cycles, double target) {
    int j, nn, n_fail;
    double oldcost;
    Population *popln = ctx->state.popln;
    Class *root = popln->classes[popln->root];
    int done = 0;

    do_all(ctx, 1, 1);
    for (nn = 0; nn < 6; nn++)
        olddogcosts[nn] = root->best_cost + 10000.0;
    n_fail = 0;
    for (nn = 0; nn < n_cycles; nn++) {
        oldcost = root->best_cost;
        do_all(ctx, 2, 0);
        if (root->best_cost < (oldcost - ctx->min_gain))
            n_fail = 0;
        else
            n_fail++;
        rep(ctx, (n_fail) ? 'g' : 'G');
        if (ctx->heard) {
            log_msg(ctx, 1, "Dogood interrupted after %4d cycles", nn);
            done = 1;
            break;
        }
        if (n_fail > 2) {
            done = 1;
            break;
        }
        if (root->best_cost < target) {
            log_msg(ctx, 1, "Dogood reached target after %4d cycles", nn);
            done = 1;
            break;
        }
        //	See if new cost significantly better than cost 5 cycles ago
        for (j = 0; j < 5; j++)
            olddogcosts[j] = olddogcosts[j + 1];
        olddogcosts[5] = root->best_cost;
        if ((olddogcosts[0] - olddogcosts[5]) < 0.2) {
            done = 1;
            break;
        }
    }
    if (!done) {
        nn = -1;
    }
    return (nn);
}

/**
 * @brief It is assumed that all classes have parameter info set up, and
 * that all cases have scores in all classes.
 * Assumes findall() has been used to find classes and set up
 * sons[], numson
 * If 'derivs', calcs derivatives. Otherwize not.
 * @param ctx Pointer to the Snob context.
 * @param item The item to process.
 * @param all All classes.
 * @param derivs Derivatives.
 * @param num_son Number of sons.
 */
void do_case(SnobContext *ctx, int item, int all, int derivs, int num_son) {
    double mincost, sum, rootcost, low, diff, w1, w2;
    Class *sub1, *sub2, *cls;
    PSaux *psaux;
    int clc, i;
    Population *popln = ctx->state.popln;
    Class *root = popln->classes[popln->root];
    char *record = ctx->state.sample->records + item * ctx->state.sample->record_length; //  Set ptr to case record
    char *field;

    if (!*record) { // Inactive item
        return;
    }

    /*	Unpack data into 'xn' fields of the Saux for each variable. The
    'xn' field is at the beginning of the Saux. Also the "missing" flag. */
    for (i = 0; i < ctx->state.vset->length; i++) {
        field = record + ctx->state.sample->variables[i].offset;
        psaux = (PSaux *)ctx->state.sample->variables[i].saux;
        if (*field == 1) {
            psaux->missing = 1;
        } else {
            psaux->missing = 0;
            memcpy(&(psaux->xn), field + 1, ctx->state.vset->variables[i].vtype->data_size);
        }
    }

    //	Deal with every class, as set up in sons[]
    clc = 0;
    while (clc < num_son) {
        cls = ctx->sons[clc];
        set_class_score(ctx, cls, item);
        if ((!ctx->see_all) && (ctx->scores.case_fac_int & 1)) { // Ignore this and decendants
            clc = ctx->next_ic[clc];
            continue;
        } else if (!ctx->see_all)
            cls->scancnt++;
        //	Score and cost the class
        score_all_vars(ctx, cls, item);
        cost_all_vars(ctx, cls, item);
        clc++;
    }
    /*	Now have casescost, casefcost and casecost set in all classes for
        this case. We can distribute weight to all leaves, using their
        casecosts.  */
    //	The whole item is irrelevant if just starting on root
    if (num_son != 1) { //  Not Just doing root
        //	Clear all casewts
        for (clc = 0; clc < num_son; clc++)
            ctx->sons[clc]->case_weight = 0.0;
        mincost = 1.0e30;
        clc = 0;
        while (clc < num_son) {
            cls = ctx->sons[clc];
            if ((!ctx->see_all) && (cls->case_score & 1)) {
                cls->total_case_cost = 1.0e30;
                clc = ctx->next_ic[clc];
                continue;
            }
            clc++;
            if (ctx->fix == Random) {
                w1 = 2.0 * rand_float(ctx);
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
        if (ctx->fix != Most_likely) {
            //	Minimum cost is in mincost. Compute unnormalized weights
            clc = 0;
            while (clc < num_son) {
                cls = ctx->sons[clc];
                if ((cls->case_score & 1) && (!ctx->see_all)) {
                    clc = ctx->next_ic[clc];
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
                cls = ctx->sons[clc];
                if ((cls->type == Leaf) && (cls->total_case_cost == mincost)) {
                    sum += 1.0;
                    cls->case_weight = 1.0;
                }
            }
        }

        //	Normalize weights, and set root's casecost
        //	It can happen that sum = 0. If so, give up on this case
        if (sum <= 0.0) {
            return;
        }
        if (ctx->fix == Random)
            rootcost = mincost;
        else
            rootcost = mincost - log(sum);
        root->dad_case_cost = root->total_case_cost = rootcost;
        sum = 1.0 / sum;
        clc = 0;
        while (clc < num_son) {
            cls = ctx->sons[clc];
            if ((cls->case_score & 1) && (!ctx->see_all)) {
                clc = ctx->next_ic[clc];
                continue;
            }
            clc++;
            if (cls->type != Leaf) {
                continue;
            }
            cls->case_weight *= sum;
            //	Can distribute this weight among subs, if any
            //	But only if subs included
            if ((!(all & Sub)) || (cls->num_sons != 2) || (cls->case_weight == 0.0)) {
                continue;
            }
            sub1 = popln->classes[cls->son_id];
            sub2 = popln->classes[sub1->sib_id];

            //	Test subclass ignore flags unless seeall
            if (!(ctx->see_all)) {
                if (sub1->case_score & 1) {
                    if (!(sub2->case_score & 1)) {
                        sub2->case_weight = cls->case_weight;
                        cls->dad_case_cost = sub2->total_case_cost;
                        continue;
                    }
                } else {
                    if (sub2->case_score & 1) { // Only sub1 has weight
                        sub1->case_weight = cls->case_weight;
                        cls->dad_case_cost = sub1->total_case_cost;
                        continue;
                    }
                }
            }

            //	Both subs costed
            diff = sub1->total_case_cost - sub2->total_case_cost;
            //	Diff can be used to set cls's casencost
            if (diff < 0.0) {
                low = sub1->total_case_cost;
                w2 = exp(diff);
                w1 = 1.0 / (1.0 + w2);
                w2 *= w1;
                if (w2 < ctx->min_sub_weight)
                    sub2->case_score |= 1;
                else
                    sub2->case_score &= -2;
                sub2->factor_scores[item] = sub2->case_score;
                if (ctx->fix == Random)
                    cls->dad_case_cost = low;
                else
                    cls->dad_case_cost = low + log(w1);
            } else {
                low = sub2->total_case_cost;
                w1 = exp(-diff);
                w2 = 1.0 / (1.0 + w1);
                w1 *= w2;
                if (w1 < ctx->min_sub_weight)
                    sub1->case_score |= 1;
                else
                    sub1->case_score &= -2;
                sub1->factor_scores[item] = sub1->case_score;
                if (ctx->fix == Random)
                    cls->dad_case_cost = low;
                else
                    cls->dad_case_cost = low + log(w2);
            }
            /*	Assign randomly if sub age 0, or to-best if sub age <
             * ctx->min_age */
            if (sub1->age < ctx->min_age) {
                if (sub1->age == 0) {
                    w1 = (rand_int(ctx) < 0) ? 1.0 : 0.0;
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
        if (root->type != Leaf) { // skip when root is only leaf
            for (clc = num_son - 1; clc >= 0; clc--) {
                cls = ctx->sons[clc];
                if ((cls->type == Sub) || ((!ctx->see_all) && (cls->factor_scores[item] & 1))) {
                    continue;
                }
                if (cls->case_weight < ctx->min_weight)
                    cls->factor_scores[item] |= 1;
                else
                    cls->factor_scores[item] &= -2;
                if (cls->dad_id >= 0)
                    popln->classes[cls->dad_id]->case_weight += cls->case_weight;
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
    root->case_weight = 1.0;
    /*	Now all classes have casewt assigned, I hope. Can proceed to
    collect statistics from this case  */
    if (!derivs) {
        return;
    }
    for (clc = 0; clc < num_son; clc++) {
        cls = ctx->sons[clc];
        if (cls->case_weight > 0.0) {
            deriv_all_vars(ctx, cls, item);
        }
    }
}
