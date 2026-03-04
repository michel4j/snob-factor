/*	-----------------  fiddles with tree structure  ----------------  */
#include "glob.h"

/*     --------------------  flatten  --------------------------------  */
/*	Destroys all non-root Dads, leaving all old non-dads (leaf or sub)
which had no children as leaves, direct sons of root. Prepares for a rebuild */
/*	Locks the type of root to dad  */
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
  ctx->no_subs++;
  tidy(ctx, 0, ctx->no_subs);
  root->type = Dad;
  root->hold_type = ctx->forever;
  do_all(ctx, 1, 1);
  do_dads(ctx, 3);
  do_all(ctx, 3, 0);
  if (ctx->interactive & ctx->heard) {
    log_msg(ctx, 0, "Flatten ends prematurely");
  }
  if (ctx->no_subs > 0) {
    ctx->no_subs--;
  }
  if (ctx->debug < 1)
    print_tree(ctx);
}

/*	------------------------  insdad  ------------------------------  */
/*	Given 2 class serials, calcs reduction in tree pcost coming from
inserting a new dad with the given classes as sons. If either class is
a sub, or it they have different dads, returns a huge negative benefit  */
/*	Insdad will, however, accept classes s1 and s2 if one is the dad
of the other, provided neither is the root  */
/*	The change, if possible, is made to ctx.popln.
    dadid is set to the id of the new dad, if any.  */
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
    /*	They do not have the same dad, but one may be the son ot the other */
    /* cls1 is a son of cls2 */
    oldid = od2;          /* The dad of cls2 */
  } else if (od2 == k1) { /*  cls2 is a son of cls1  */
    oldid = od1;
  } else {
    return drop;
  }

  odad = popln->classes[oldid];
  newid = make_class(ctx);
  if (newid >= 0) {
    ndad = popln->classes[newid]; /*  The new dad  */
    /*	Copy old dad's basics, stats into new dad  */
    nch = ((char *)&odad->id) - ((char *)odad);
    memcpy(ndad, odad, nch);
    ndad->serial = popln->next_serial << 2;
    popln->next_serial++;
    ndad->age = ctx->min_fac_age - 3;
    ndad->hold_type = 0;
    /* Copy Basics. the structures should have been made.  */
    for (iv = 0; iv < ctx->state.vset->length; iv++) {
      fcls_var = odad->basics[iv];
      cls_var = ndad->basics[iv];
      nch = ctx->state.vset->variables[iv].basic_size;
      memcpy(cls_var, fcls_var, nch);
    }

    /*  Copy stats  */
    for (iv = 0; iv < ctx->state.vset->length; iv++) {
      fexp_var = odad->stats[iv];
      exp_var = ndad->stats[iv];
      nch = ctx->state.vset->variables[iv].stats_size;
      memcpy(exp_var, fexp_var, nch);
    }

    ndad->dad_id = oldid; /* So new dad is son of old dad */
    cls1->dad_id = cls2->dad_id = newid;
    /*	Set relab and cnt in new dad  */
    ndad->relab = cls1->relab + cls2->relab;
    ndad->weights_sum = cls1->weights_sum + cls2->weights_sum;

    /*	ctx->fix linkages  */
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

/*	----------------------  bestinsdad ---------------------   */
/*	Returns serial of new dad, or 0 if best no good, or -1 if none
to try  */
int best_insert_dad(SnobContext *ctx, int force) {
  State oldctx;
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
  ctx->no_subs++;
  /*	Do one pass over population to set costs   */
  do_all(ctx, 1, 1);
  bestdrop = -1.0e20;
  bser1 = bser2 = -1;
  newser = 0;

  root = ctx->state.popln->classes[ctx->state.popln->root];
  origcost = root->best_cost;
  hiid = ctx->state.popln->hi_class;
  if (ctx->state.popln->num_classes < 4) {
    log_msg(ctx, 0, "Model has only%2d class", ctx->state.popln->num_classes);
    succ = -1;
    goto alldone;
  }
  memcpy(&oldctx, &ctx->state, sizeof(State));

  i1 = 0;
outer:
  if (i1 == ctx->state.popln->root)
    goto i1done;
  cls1 = ctx->state.popln->classes[i1];
  if (!cls1)
    goto i1done;
  if ((cls1->type == Vacant) || (cls1->type == Sub))
    goto i1done;
  ser1 = cls1->serial;
  i2 = i1 + 1;
inner:
  if (i2 == ctx->state.popln->root)
    goto i2done;
  cls2 = ctx->state.popln->classes[i2];
  if (!cls2)
    goto i2done;
  if ((cls2->type == Vacant) || (cls2->type == Sub))
    goto i2done;
  if (cls1->dad_id != cls2->dad_id)
    goto i2done;
  ser2 = cls2->serial;
  if (chk_bad_move(ctx, 1, ser1, ser2))
    goto i2done;

  /*	Copy population to TrialPop, unfilled  */
  newp = copy_population(ctx, ctx->state.popln->id, 0, "TrialPop");
  if (newp < 0)
    goto popfails;
  ctx->state.popln = ctx->populations[newp];
  root = ctx->state.popln->classes[ctx->state.popln->root];
  res = insert_dad(ctx, ser1, ser2, &newid);
  if (newid < 0) {
    goto i2done;
  }
  if (res > bestdrop) {
    bestdrop = res;
    bser1 = ser1;
    bser2 = ser2;
  }
i2done:
  memcpy(&ctx->state, &oldctx, sizeof(State));

  root = ctx->state.popln->classes[ctx->state.popln->root];
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
    log_msg(ctx, 0, "No possible dad insertions");
    succ = newser = -1;
    goto finish;
  }
  /*	Copy population to TrialPop, filled  */
  newp = copy_population(ctx, ctx->state.popln->id, 1, "TrialPop");
  if (newp < 0)
    goto popfails;
  ctx->state.popln = ctx->populations[newp];
  root = ctx->state.popln->classes[ctx->state.popln->root];
  log_msg(ctx, 0, "TRYING INSERT %6d,%6d", bser1 >> 2, bser2 >> 2);
  res = insert_dad(ctx, bser1, bser2, &newid);
  /*	But check it is not killed off   */
  newser = ctx->state.popln->classes[newid]->serial;
  ctx->control = 0;
  do_all(ctx, 1, 1);
  ctx->control = AdjAll;
  if (ctx->heard) {
    log_msg(ctx, 0, "BestInsDad ends prematurely");
    return (0);
  }
  if (newser != ctx->state.popln->classes[newid]->serial)
    newser = 0;
  /*	See if the trial model has improved over original  */
  succ = 1;
  if (root->best_cost < origcost)
    goto winner;
  succ = 0;
  if (force)
    goto winner;
  set_bad_move(ctx, 1, bser1, bser2);
  newser = 0;
  memcpy(&ctx->state, &oldctx, sizeof(State));

  log_msg(ctx, 0, "Failed ******");
  goto finish;

popfails:
  succ = newser = -1;
  log_msg(ctx, 0, "Cannot make TrialPop");
  goto finish;

winner:
  log_msg(ctx, 0, "%s", (succ) ? "ACCEPTED !!!" : "FORCED");
  if (ctx->debug < 1)
    print_tree(ctx);
  clr_bad_move(ctx);
  /*	Reverse roles of 'work' and TrialPop  */
  strcpy(oldctx.popln->name, "TrialPop");
  strcpy(ctx->state.popln->name, "work");
  if (succ)
    track_best(ctx, 1);

finish:
  if (ctx->no_subs > 0)
    ctx->no_subs--;
  return (newser);
}

/*	---------------  rebuild  --------------------------------  */
/*	Flattens and the rebuilds the tree  */
void rebuild(SnobContext *ctx) { log_msg(ctx, 0, "Rebuild obsolete!"); }

/*	------------------  deldad  -------------------------  */
/*	If class ser is Dad (not root), it is removed, and its sons become
sons of its dad.
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
    goto finish;
  if (kk == popln->root)
    goto finish;
  kkd = cls->dad_id;
  if (kkd < 0)
    goto finish;
  if (cls->num_sons <= 0)
    goto finish;
  /*	All seems OK. ctx->fix idads in kk's sons  */
  origcost = root->best_par_cost;
  for (kks = cls->son_id; kks >= 0; kks = son->sib_id) {
    son = popln->classes[kks];
    son->dad_id = kkd;
  }
  /*	Now kill off class kk  */
  cls->type = Vacant;
  popln->num_classes--;
  /*	ctx->fix linkages  */
  tidy(ctx, 0, ctx->no_subs);
  do_dads(ctx, 20);
  newcost = root->best_par_cost;
  drop = origcost - newcost;
finish:
  return (drop);
}

/*	------------------  bestdeldad -------------------------   */
/*	Tries all feasible deldads, does dogood on best and installs
as work if an improvement.  */
/*	Returns 1 if accepted, 0 if tried and no good, -1 if none to try */
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
  /*	Do one pass of doall to set costs  */
  do_all(ctx, 1, 1);
  origcost = root->best_cost;
  hiid = ctx->state.popln->hi_class;
  memcpy(&oldctx, &ctx->state, sizeof(State));
  i1 = 0;
loop:
  if (i1 == popln->root)
    goto i1done;
  cls = ctx->state.popln->classes[i1];
  if (!cls)
    goto i1done;
  if (cls->type != Dad)
    goto i1done;
  ser = cls->serial;
  if (chk_bad_move(ctx, 2, 0, ser))
    goto i1done;
  newp = copy_population(ctx, ctx->state.popln->id, 0, "TrialPop");
  if (newp < 0)
    goto popfails;
  popln = ctx->state.popln = ctx->populations[newp];

  root = popln->classes[popln->root];
  res = splice_dad(ctx, ser);
  if (res < -1000000.0) {
    goto i1done;
  }
  if (res > bestdrop) {
    bestdrop = res;
    bser = ser;
  }
i1done:
  memcpy(&ctx->state, &oldctx, sizeof(State));

  i1++;
  if (i1 <= hiid)
    goto loop;

  if (bser < 0) {
    log_msg(ctx, 0, "No possible dad deletions");
    goto finish;
  }
  newp = copy_population(ctx, ctx->state.popln->id, 1, "TrialPop");
  if (newp < 0)
    goto popfails;
  popln = ctx->state.popln = ctx->populations[newp];

  root = popln->classes[popln->root];
  log_msg(ctx, 0, "TRYING DELETE %6d", bser >> 2);
  res = splice_dad(ctx, bser);
  ctx->control = 0;
  do_all(ctx, 1, 1);
  ctx->control = AdjAll;
  if (ctx->heard) {
    log_msg(ctx, 0, "BestDelDad ends prematurely");
    return (0);
  }
  if (root->best_cost < origcost)
    goto winner;
  set_bad_move(ctx, 2, 0, bser); /* log failure in badmoves */
  bser = 0;
  memcpy(&ctx->state, &oldctx, sizeof(State));

  log_msg(ctx, 0, "Failed ******");
  goto finish;

popfails:
  log_msg(ctx, 0, "BestDelDad cannot make TrialPop");
  bser = -1;
  goto finish;

winner:
  clr_bad_move(ctx);
  log_msg(ctx, 0, "ACCEPTED !!!");
  if (ctx->debug < 1)
    print_tree(ctx);
  strcpy(oldctx.popln->name, "TrialPop");
  strcpy(ctx->state.popln->name, "work");
  track_best(ctx, 1);

finish:
  if (ctx->no_subs > 0)
    ctx->no_subs--;
  return (bser);
}

/*	---------------  binhier  --------------------------------  */
/*	If flat, flattens population. Then inserts dads to make a binary
   hierarchy. Then deletes dads as appropriate  */
void binary_hierarchy(SnobContext *ctx, int flat) {
  int nn;

  if (flat)
    flatten(ctx);
  ctx->no_subs++;
  if (ctx->heard)
    goto kicked;
  clr_bad_move(ctx);
insloop:
  nn = best_insert_dad(ctx, 1);
  if (ctx->heard)
    goto kicked;
  if (nn > 0)
    goto insloop;

  try_moves(ctx, 2);
  if (ctx->heard)
    goto kicked;

delloop:
  nn = best_remove_dad(ctx);
  if (ctx->heard)
    goto kicked;
  if (nn > 0)
    goto delloop;

  try_moves(ctx, 2);
  if (ctx->heard)
    goto kicked;

finish:
  if (ctx->debug < 1)
    print_tree(ctx);
  if (ctx->no_subs > 0)
    ctx->no_subs--;
  clr_bad_move(ctx);
  return;

kicked:
  nn = find_population(ctx, "work");
  ctx->state.popln = ctx->populations[nn];

  log_msg(ctx, 0, "BinHier ends prematurely");
  goto finish;
}

/*	------------------  ranclass  --------------------------  */
/*	To make nn random classes  */
void ranclass(SnobContext *ctx, int nn) {
  int n, ic, ib, num_son;
  double bs;
  Class *sub, *dad, *cls;
  Population *popln = ctx->state.popln;
  Class *root = popln->classes[popln->root];

  if (!popln) {
    log_msg(ctx, 0, "Ranclass needs a model");
    goto finish;
  }
  if (!popln->sample_size) {
    log_msg(ctx, 0, "Model has no sample");
    goto finish;
  }
  if (nn > (popln->cls_vec_len - 2)) {
    log_msg(ctx, 0, "Too many classes");
    goto finish;
  }

  ctx->no_subs = 0;
  delete_all_classes(ctx);
  n = 1;
  if (nn < 2)
    goto finish;

again:
  if (n >= nn)
    goto windup;
  num_son = find_all(ctx, Leaf);
  /*	Locate biggest leaf with subs aged at least ctx->min_age  */
  ib = -1;
  bs = 0.0;
  for (ic = 0; ic < num_son; ic++) {
    cls = ctx->sons[ic];
    if (cls->num_sons < 2)
      goto icdone;
    sub = popln->classes[cls->son_id];
    if (sub->age < ctx->min_age)
      goto icdone;
    if (cls->weights_sum > bs) {
      bs = cls->weights_sum;
      ib = ic;
    }
  icdone:;
  }

  if (ib < 0) {
    do_all(ctx, 1, 1);
    goto again;
  }
  /*	Split sons[ib]  */
  dad = ctx->sons[ib];
  if (split_leaf(ctx, dad->id))
    goto windup;
  log_msg(ctx, 0, "Splitting %s size%8.1f", serial_to_str(ctx, dad),
          dad->weights_sum);
  dad->hold_type = ctx->forever;
  n++;
  goto again;

windup:
  ctx->no_subs = 1;
  do_all(ctx, 5, 1);
  flatten(ctx);
  do_all(ctx, 6, 0);
  do_all(ctx, 4, 1);
  if (ctx->debug < 1)
    print_tree(ctx);
  root->hold_type = 0;

finish:
  return;
}

/*	---------------  moveclass  --------------------------------  */
/*	To move class ser1 to be a child of class ser2  */
double move_class(SnobContext *ctx, int ser1, int ser2) {
  Class *cls1, *cls2, *odad;
  int k1, k2, od2;
  double origcost, newcost, drop;

  Population *popln = ctx->state.popln;
  Class *root = popln->classes[popln->root];

  origcost = root->best_par_cost;
  k1 = serial_to_id(ctx, ser1);
  if (k1 < 0)
    goto nullit;
  k2 = serial_to_id(ctx, ser2);
  if (k2 < 0)
    goto nullit;
  cls1 = popln->classes[k1];
  if (cls1->type == Sub)
    goto nullit;
  cls2 = popln->classes[k2];
  if (cls2->type != Dad) {
    log_msg(ctx, 0, "Class %4d is not a dad", ser2);
    goto nullit;
  }
  /*	Check that a change is needed  */
  if (cls1->dad_id == k2) {
    log_msg(ctx, 0, "No change needed");
    goto nullit;
  }
  /*	Check that cls1 is not an ancestor of cls2  */
  for (od2 = cls2->dad_id; od2 >= 0; od2 = odad->dad_id) {
    if (od2 == k1) {
      log_msg(ctx, 0, "Class %4d is ancestor of class %4d", ser1, ser2);
      goto nullit;
    }
    odad = popln->classes[od2];
  }
  /*	All seems OK, so make change in links   */
  cls1->dad_id = k2;
  /*	ctx->fix linkages  */
  tidy(ctx, 0, ctx->no_subs);
  do_dads(ctx, 30);
  if (popln->sample_size) {
    do_all(ctx, 4, 0);
    if (ctx->heard)
      goto kicked;
    /* 	To collect weights, counts */
    do_all(ctx, 4, 1);
    if (ctx->heard)
      goto kicked;
  }
  newcost = root->best_par_cost;
  drop = origcost - newcost;
  goto done;

kicked:
  log_msg(ctx, 0, "Moveclass interrupted prematurely");

nullit:
  drop = -1.0e20;

done:
  return (drop);
}

/*	-----------------  trial  ---------------------------------  */
void trial(SnobContext *ctx, int param) {
  log_msg(ctx, 0, "Running TRIAL");
  correlpops(ctx, param);
}

/*	----------------------  bestmoveclass ---------------------   */
/*
Looks for the best moveclass. If force, or if an improvement,
does it. Returns 1 if an improvement, 0 if best no improvement, -1 if none
possible.
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
  do_all(ctx, 1, 1); /* To set costs */
  origcost = root->best_cost;
  hiid = ctx->state.popln->hi_class;
  if (ctx->state.popln->num_classes < 4) {
    log_msg(ctx, 0, "Model has only%2d class", ctx->state.popln->num_classes);
    succ = -1;
    goto alldone;
  }
  memcpy(&oldctx, &ctx->state, sizeof(State));

  i1 = 0;
outer:
  if (i1 == ctx->state.popln->root) {
    goto i1done;
  }
  cls1 = ctx->state.popln->classes[i1];
  if ((!cls1) || (cls1->type == Vacant) || (cls1->type == Sub)) {
    goto i1done;
  }
  ser1 = cls1->serial;
  i2 = 0;
  while (i2 <= hiid) {

    if (i2 == i1) {
      goto i2done;
    }
    cls2 = ctx->state.popln->classes[i2];
    if ((!cls2) || (cls2->type != Dad) || (cls1->dad_id == i2)) {
      goto i2done;
    }
    /*	Check i1 not an ancestor of i2  */
    for (od2 = cls2->dad_id; od2 >= 0; od2 = odad->dad_id) {
      if (od2 == i1)
        goto i2done;
      odad = ctx->state.popln->classes[od2];
    }
    ser2 = cls2->serial;
    if (chk_bad_move(ctx, 3, ser1, ser2)) {
      goto i2done;
    }

    /*	Copy pop to TrialPop, unfilled  */
    newp = copy_population(ctx, ctx->state.popln->id, 0, "TrialPop");
    if (newp < 0) {
      goto popfails;
    }
    ctx->state.popln = ctx->populations[newp];
    root = ctx->state.popln->classes[ctx->state.popln->root];
    res = move_class(ctx, ser1, ser2);
    if (res > bestdrop) {
      bestdrop = res;
      bser1 = ser1;
      bser2 = ser2;
    }
  i2done:
    memcpy(&ctx->state, &oldctx, sizeof(State));
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
    log_msg(ctx, 0, "No possible class move");
    goto finish;
  }
  /*	Copy pop to TrialPop, filled  */
  newp = copy_population(ctx, ctx->state.popln->id, 1, "TrialPop");
  if (newp < 0)
    goto popfails;
  ctx->state.popln = ctx->populations[newp];

  root = ctx->state.popln->classes[ctx->state.popln->root];
  log_msg(ctx, 0, "TRYING MOVE CLASS %6d TO DAD %6d", bser1 >> 2, bser2 >> 2);
  res = move_class(ctx, bser1, bser2);
  ctx->control = 0;
  do_all(ctx, 1, 1);
  ctx->control = AdjAll;
  if (ctx->heard)
    log_msg(ctx, 0, "BestMoveClass ends prematurely");
  /*	Setting dogood's target to origcost-1 allows early exit  */
  /*	See if the trial model has improved over original  */
  succ = 1;
  if (root->best_cost < origcost)
    goto winner;
  succ = 0;
  if (force)
    goto winner;
  set_bad_move(ctx, 3, bser1, bser2);
  memcpy(&ctx->state, &oldctx, sizeof(State));

  log_msg(ctx, 0, "Failed ******");
  goto finish;

popfails:
  succ = -1;
  log_msg(ctx, 0, "Cannot make TrialPop");
  goto finish;

winner:
  log_msg(ctx, 0, "%s", (succ) ? "ACCEPTED !!!" : "FORCED");
  if (ctx->debug < 1)
    print_tree(ctx);
  clr_bad_move(ctx);
  /*	Reverse roles of 'work' and TrialPop  */
  strcpy(oldctx.popln->name, "TrialPop");
  strcpy(ctx->state.popln->name, "work");
  if (succ)
    track_best(ctx, 1);

finish:
  if (ctx->no_subs > 0)
    ctx->no_subs--;
  return (succ);
}

/*	--------------------  trymoves  ----------------------------   */
/*	Tries moving classes using bestmoveclass until ntry attempts in
succession have failed, or until all possible moves have been tried   */
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
      log_msg(ctx, 0, "Trymoves ends prematurely");
      break;
    }
  }

  if (ctx->no_subs > 0)
    ctx->no_subs--;
  return;
}
