/*	-----------------  fiddles with tree structure  ----------------  */
#define NOTGLOB 1
#define TACTICS 1
#include "glob.c"

/*     --------------------  flatten  --------------------------------  */
/*	Destroys all non-root Dads, leaving all old non-dads (leaf or sub)
which had no children as leaves, direct sons of root. Prepares for a rebuild */
/*	Locks the type of root to dad  */
void flatten ()
{

	Sw i;

	setpop ();
	tidy (0);
	if (rootcl->nson == 0)	{
		printf ("Nothing to flatten\n");   return;
		}
	for (i = 0; i <= pop->hicl; i++)	{
		if (i == root) goto donecls;
		cls = pop->classes [i];
		if (cls->type == Vacant) goto donecls;
		if (cls->nson)	{   /*  Kill it  */
			cls->type = Vacant;  cls->idad = Deadkilled;
			pop->ncl --;
			}
		else	{
			if (cls->type == Sub)	{
				cls->type = Leaf;
				cls->serial = pop->nextserial << 2;
				pop->nextserial++;
				}
			cls->idad = root;
			}
	donecls:	;
		}
	nosubs ++;
	tidy (0);
	rootcl->type = Dad;
	rootcl->holdtype = Forever;
	doall (1, 1);  dodads (3);  doall (3, 0);
	if (heard) printf ("Flatten ends prematurely\n");
	if (nosubs > 0) nosubs --;
	printtree ();
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
Sf insdad (ser1, ser2, dadid)
	Sw ser1, ser2, *dadid;
{
	Class *cls1, *cls2, *ndad, *odad;
	EVinst *evi, *fevi;
	CVinst *cvi, *fcvi;
	Sw nch, iv, k1, k2;
	Sf origcost, newcost, drop;
	Sw oldid, newid, od1, od2;

	origcost = rootcl->cbpcost;
	*dadid = -1;
	k1 = s2id (ser1);  if (k1 < 0) goto nullit;
	k2 = s2id (ser2);  if (k2 < 0) goto nullit;
	cls1 = pop->classes[k1];  if (cls1->type == Sub) goto nullit;
	cls2 = pop->classes[k2];  if (cls2->type == Sub) goto nullit;
	od1 = cls1->idad;  od2 = cls2->idad;
/*	Normally we expect cls1, cls2 to have same dad, with at least one
	other son	*/
	if (od1 == od2)	{
		oldid = od1;  odad = pop->classes [oldid];
		if (odad->nson < 3) goto nullit;
		goto configok;
		}
/*	They do not have the same dad, but one may be the son ot the other */
	if (od1 == k2)	{		/* cls1 is a son of cls2 */
		oldid = od2;	/* The dad of cls2 */
		goto configok;
		}
	if (od2 == k1)	{	/*  cls2 is a son of cls1  */
		oldid = od1;  goto configok;
		}
	goto nullit;

configok:
	odad = pop->classes [oldid];
	newid = makeclass ();
	if (newid < 0) goto nullit;
	ndad = pop->classes [newid];   /*  The new dad  */
/*	Copy old dad's basics, stats into new dad  */
	nch = ((char*) & odad->id) - ((char*) odad);
	cmcpy (ndad, odad, nch);
	ndad->serial = pop->nextserial << 2;
	pop->nextserial++;
	ndad->age = MinFacAge - 3;
	ndad->holdtype = 0;
/*      Copy Basics. the structures should have been made.  */
        for (iv = 0; iv < nv; iv++)     {
                fcvi = odad->basics[iv];
                cvi = ndad->basics[iv];
                nch = vlist[iv].basicsize;
                cmcpy (cvi, fcvi, nch);
                }

/*      Copy stats  */
        for (iv = 0; iv < nv; iv++)        {
                fevi = odad->stats[iv];
                evi = ndad->stats[iv];
                nch = vlist[iv].statssize;
                cmcpy (evi, fevi, nch);
		}

	ndad->idad = oldid;   /* So new dad is son of old dad */
	cls1->idad = cls2->idad = newid;
/*	Set relab and cnt in new dad  */
	ndad->relab = cls1->relab + cls2->relab;
	ndad->cnt = cls1->cnt + cls2->cnt;

/*	Fix linkages  */
	tidy (0);
	dodads (20);
	newcost = rootcl->cbpcost;
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
Sw bestinsdad (force)
	Sw force;
{
	Context oldctx;
	Class *cls1, *cls2;
	Sw i1, i2, hiid, succ;
	Sw bser1, bser2, ser1, ser2, newp, newid, newser;
	Sf res, bestdrop, origcost;


/*	We look for all pairs of non-Sub serials except root.
	For each pair, we copy the pop to TrialPop, switch context to
TrialPop, and do an insdad on the pair. We note the pair giving the largest
insdad, again copy to TrialPop, repeat the insdad, and relax with
doall, .
	*/

/*	To get all pairs, we need a double loop over class indexes. I use
i1, i2 as the indices. i1 runs from 0 to pop->hicl-1, i2 from i1+1 to
pop->hicl	*/
	nosubs ++;
/*	Do one pass over pop to set costs   */
	doall (1, 1);
	bestdrop = -1.0e20;  bser1 = bser2 = -1;
	newser = 0;  setpop ();
	origcost = rootcl->cbcost;
	hiid = pop->hicl;
	if (pop->ncl < 4) {
		flp(); printf("Model has only%2d class\n", pop->ncl);
		succ = -1;  goto alldone;  }
	cmcpy (&oldctx, &ctx, sizeof (Context));

	i1 = 0;
outer:
	if (i1 == root) goto i1done;
	cls1 = pop->classes [i1];
	if (! cls1) goto i1done;
	if ((cls1->type == Vacant) || (cls1->type == Sub)) goto i1done;
	ser1 = cls1->serial;
	i2 = i1 + 1;
inner:
	if (i2 == root) goto i2done;
	cls2 = pop->classes [i2];
	if (! cls2) goto i2done;
	if ((cls2->type == Vacant) || (cls2->type == Sub)) goto i2done;
	if (cls1->idad != cls2->idad) goto i2done;
	ser2 = cls2->serial;
	if (testbadm (1, ser1, ser2)) goto i2done;

/*	Copy pop to TrialPop, unfilled  */
	newp = copypop (pop->id, 0, "TrialPop");
	if (newp < 0) goto popfails;
	ctx.popln = poplns [newp];
	setpop ();
	res = insdad (ser1, ser2, &newid);
	if (newid < 0)	{
		goto i2done;	}
	if (res > bestdrop)	{
		bestdrop = res;
		bser1 = ser1;  bser2 = ser2;
		}
i2done:
	cmcpy (&ctx, &oldctx, sizeof (Context));
	setpop ();	/* Return to original popln */
	i2++;  if (i2 <= hiid) goto inner;
i1done:
	i1++;  if (i1 < hiid) goto outer;

	goto alldone;

alldone:
	if (bser1 < 0)
		{flp(); printf ("No possible dad insertions\n");
			succ = newser = -1;  goto finish; }
/*	Copy pop to TrialPop, filled  */
	newp = copypop (pop->id, 1, "TrialPop");
	if (newp < 0) goto popfails;
	ctx.popln = poplns [newp];
	setpop ();
flp(); printf("TRYING INSERT %6d,%6d\n", bser1>>2, bser2>>2);
	res = insdad (bser1, bser2, &newid);
/*	But check it is not killed off   */
	newser = pop->classes[newid]->serial;
	control = 0;  doall (1, 1);  control = AdjAll;
	if (heard)	{ printf ("BestInsDad ends prematurely\n");
			return (0);  }
	if (newser != pop->classes[newid]->serial) newser = 0;
/*	See if the trial model has improved over original  */
	succ = 1;
	if (rootcl->cbcost < origcost) goto winner;
	succ = 0;
	if (force) goto winner;
	setbadm (1, bser1, bser2);
	newser = 0;
	cmcpy (&ctx, &oldctx, sizeof (Context));
	setpop ();      /* Return to original popln */
	flp(); printf ("Failed ******\n");
	goto finish;

popfails:
	succ = newser = -1;
	flp(); printf ("Cannot make TrialPop\n");  goto finish;

winner:
	flp();
	printf ("%s\n", (succ) ? "ACCEPTED !!!" : "FORCED");
	printtree();  clearbadm();
/*	Reverse roles of 'work' and TrialPop  */
	strcpy (oldctx.popln->name, "TrialPop");
	strcpy (pop->name, "work");
	if (succ) trackbest(1);

finish:		if (nosubs > 0) nosubs --;  return (newser);
}

/*	---------------  rebuild  --------------------------------  */
/*	Flattens the rebuilds the tree  */
void rebuild ()
{
	flp(); printf("Rebuild obsolete\n");  return;
}


/*	------------------  deldad  -------------------------  */
/*	If class ser is Dad (not root), it is removed, and its sons become
sons of its dad.
	*/
Sf deldad (ser)
	Sw ser;
{
	Class *son;
	Sw kk, kkd, kks;
	Sf drop, origcost, newcost;

	drop = -1.0e20;
	kk = s2id (ser);
	setclass1 (pop->classes [kk]);
	if (cls->type != Dad) goto finish;
	if (kk == root) goto finish;
	kkd = cls->idad;  if (kkd < 0) goto finish;
	if (cls->nson <= 0) goto finish;
/*	All seems OK. Fix idads in kk's sons  */
	origcost = rootcl->cbpcost;
	for (kks = cls->ison;  kks >= 0;  kks = son->isib)	{
		son = pop->classes[kks];
		son->idad = kkd;
		}
/*	Now kill off class kk  */
	cls->type = Vacant;  pop->ncl --;
/*	Fix linkages  */
	tidy (0);
	dodads (20);
	newcost = rootcl->cbpcost;
	drop = origcost - newcost;
finish: return (drop);
}


/*	------------------  bestdeldad -------------------------   */
/*	Tries all feasible deldads, does dogood on best and installs
as work if an improvement.  */
/*	Returns 1 if accepted, 0 if tried and no good, -1 if none to try */
Sw bestdeldad ()
{
	Context oldctx;
	Class *cls;
	Sw i1, hiid, newp;
	Sw bser, ser;
	Sf res, bestdrop, origcost;

	nosubs ++;
	bestdrop = -1.0e20;  bser = -1;
	setpop();
/*	Do one pass of doall to set costs  */
	doall (1, 1);
	origcost = rootcl->cbcost;
	hiid = pop->hicl;
	cmcpy (&oldctx, &ctx, sizeof (Context));
	i1 = 0;
loop:
	if (i1 == root) goto i1done;
	cls = pop->classes[i1];
	if (! cls) goto i1done;
	if (cls->type != Dad) goto i1done;
	ser = cls->serial;
	if (testbadm (2, 0, ser)) goto i1done;
	newp = copypop (pop->id, 0, "TrialPop");
	if (newp < 0) goto popfails;
	ctx.popln = poplns [newp];  setpop ();
	res = deldad (ser);
	if (res < -1000000.0)	{
		goto i1done;  }
	if (res > bestdrop) { bestdrop = res;  bser = ser; }
i1done:
	cmcpy (&ctx, &oldctx, sizeof (Context));  setpop ();
	i1++;  if (i1 <= hiid) goto loop;

	if (bser < 0)
		{flp(); printf("No possible dad deletions\n");  goto finish; }
	newp = copypop (pop->id, 1, "TrialPop");
	if (newp < 0) goto popfails;
	ctx.popln = poplns[newp];  setpop ();
flp(); printf("TRYING DELETE %6d\n", bser>>2);
	res = deldad (bser);
	control = 0;  doall (1, 1);  control = AdjAll;
	if (heard)  { printf ("BestDelDad ends prematurely\n");
			return (0);  }
	if (rootcl->cbcost < origcost) goto winner;
	setbadm (2, 0, bser);	/* log failure in badmoves */
	bser = 0;
	cmcpy (&ctx, &oldctx, sizeof (Context));  setpop ();
	flp(); printf ("Failed ******\n");
	goto finish;

popfails:
	flp(); printf ("BestDelDad cannot make TrialPop\n");
	bser = -1; goto finish;

winner:
	flp();  clearbadm();
	printf ("ACCEPTED !!!\n");
	printtree ();
	strcpy (oldctx.popln->name, "TrialPop");
	strcpy (pop->name, "work");
	trackbest(1);

finish:		if (nosubs > 0) nosubs --;  return (bser);
}


/*	---------------  binhier  --------------------------------  */
/*	If flat, flattens pop. Then inserts dads to make a binary hierarchy.
	Then deletes dads as appropriate  */
void binhier (flat)
	Sw flat;
{
	Sw nn;

	setpop ();
	if (flat) flatten ();
	nosubs ++;
	if (heard) goto kicked;
	clearbadm ();
insloop:
	nn = bestinsdad (1);   if (heard) goto kicked;
	if (nn > 0) goto insloop;

	trymoves (2);
	if (heard) goto kicked;

delloop:
	nn = bestdeldad ();	if (heard) goto kicked;
	if (nn > 0) goto delloop;

	trymoves (2);
	if (heard) goto kicked;

finish:
	printtree();
	if (nosubs > 0) nosubs --;
	clearbadm ();
	return;

kicked:
	nn = pname2id ("work");
	ctx.popln = poplns [nn];  setpop ();
	printf ("BinHier ends prematurely\n");  goto finish;
}


/*	------------------  ranclass  --------------------------  */
/*	To make nn random classes  */
void ranclass (nn)
	Sw nn;
{
	Sw n, ic, ib;
	Sf bs;
	Class *sub, *dad;

	setpop ();
	if (! pop) { printf("Ranclass needs a model\n");  goto finish; }
	if (! pop->nc) { printf("Model has no sample\n"); goto finish; }
	if (nn > (pop->mncl - 2)) {printf("Too many classes\n"); goto finish; }

	nosubs = 0;
	deleteallclasses ();
	n = 1;
	if (nn < 2) goto finish;

again:
	if (n >= nn) goto windup;
	findall (Leaf);
/*	Locate biggest leaf with subs aged at least MinAge  */
	ib = -1;
	bs = 0.0;
	for (ic = 0; ic < numson; ic++)	{
		cls = sons[ic];
		if (cls->nson < 2) goto icdone;
		sub = pop->classes[cls->ison];
		if (sub->age < MinAge) goto icdone;
		if (cls->cnt > bs)  { bs = cls->cnt;  ib = ic; }
	icdone:	;
		}

	if (ib < 0) { doall (1, 1);  goto again;  }
/*	Split sons[ib]  */
	dad = sons[ib];
	if (splitleaf (dad->id)) goto windup;
printf("Splitting %s size%8.1f\n", sers(dad), dad->cnt);
	dad->holdtype = Forever;
	n++;
	goto again;

windup:
	nosubs = 1;
	doall (5, 1);
	flatten ();
	doall (6, 0);  doall (4, 1);
	printtree();
	rootcl->holdtype = 0;

finish:	return;
}


/*	---------------  moveclass  --------------------------------  */
/*	To move class ser1 to be a child of class ser2  */
Sf moveclass (ser1, ser2)
	Sw ser1, ser2;
{
	Class *cls1, *cls2, *odad;
	Sw k1, k2, od2;
	Sf origcost, newcost, drop;

	origcost = rootcl->cbpcost;
	k1 = s2id (ser1);  if (k1 < 0) goto nullit;
	k2 = s2id (ser2);  if (k2 < 0) goto nullit;
	cls1 = pop->classes[k1];  if (cls1->type == Sub) goto nullit;
	cls2 = pop->classes[k2];  if (cls2->type != Dad)	{
		printf ("Class %4d is not a dad\n", ser2);  goto nullit;
		}
/*	Check that a change is needed  */
	if (cls1->idad == k2)	{
		printf ("No change needed\n");  goto nullit;
		}
/*	Check that cls1 is not an ancestor of cls2  */
	for (od2 = cls2->idad; od2 >= 0; od2 = odad->idad)	{
		if (od2 == k1)	{
			printf ("Class %4d is ancestor of class %4d\n",
				ser1, ser2);   goto nullit;
			}
		odad = pop->classes[od2];
		}
/*	All seems OK, so make change in links   */
	cls1->idad = k2;
/*	Fix linkages  */
	tidy (0);
	dodads (30);
	if (pop->nc)	{
		doall (4, 0);
		if (heard) goto kicked;
	/* 	To collect weights, counts */
		doall (4, 1);
		if (heard) goto kicked;
		}
	newcost = rootcl->cbpcost;
	drop = origcost - newcost;
	goto done;

kicked:
	printf ("Moveclass interrupted prematurely\n");

nullit:
	drop = -1.0e20;

done:
	return (drop);
}


/*	-----------------  trial  ---------------------------------  */
void trial (param)
	Sw param;
{
	printf("Running TRIAL\n");
	correlpops (param);
}

/*	----------------------  bestmoveclass ---------------------   */
/*	Lokks for the best moveclass. If force, or if an improvement,
does it. Returns 1 if an improvement, 0 if best no improvement, -1 if none
possible.	*/
Sw bestmoveclass (force)
	Sw force;
{
	Context oldctx;
	Class *cls1, *cls2, *odad;
	Sw i1, i2, hiid;
	Sw bser1, bser2, ser1, ser2, od2, newp, succ;
	Sf res, bestdrop, origcost;


/*	We look for all pairs ser1,ser2 of serials where:
ser2 is a dad, ser1 is not an ancestor of ser2, ser1 is non-sub, and
ser1 is not a son of ser2.
	For each pair, we copy the pop to TrialPop, switch context to
TrialPop, and do a moveclass on the pair. We note the pair giving the largest
moveclass, again copy to TrialPop, repeat the moveclass, and relax with
doall, .
	*/

/*	To get all pairs, we need a double loop over class indexes. I use
i1, i2 as the indices.   */
	nosubs ++;
	bestdrop = -1.0e20;  bser1 = bser2 = -1;
	setpop ();
	doall (1, 1);	/* To set costs */
	origcost = rootcl->cbcost;
	hiid = pop->hicl;
	if (pop->ncl < 4) {
		flp(); printf("Model has only%2d class\n", pop->ncl);
		succ = -1;  goto alldone;  }
	cmcpy (&oldctx, &ctx, sizeof (Context));

	i1 = 0;
outer:
	if (i1 == root) goto i1done;
	cls1 = pop->classes [i1];
	if (! cls1) goto i1done;
	if ((cls1->type == Vacant) || (cls1->type == Sub)) goto i1done;
	ser1 = cls1->serial;
	i2 = 0;
inner:
	if (i2 == i1) goto i2done;
	cls2 = pop->classes [i2];
	if (! cls2) goto i2done;
	if (cls2->type != Dad) goto i2done;
	if (cls1->idad == i2) goto i2done;
/*	Check i1 not an ancestor of i2  */
	for (od2 = cls2->idad; od2 >= 0; od2 = odad->idad)	{
		if (od2 == i1)	goto i2done;
		odad = pop->classes[od2];
		}
	ser2 = cls2->serial;
	if (testbadm (3, ser1, ser2)) goto i2done;

/*	Copy pop to TrialPop, unfilled  */
	newp = copypop (pop->id, 0, "TrialPop");
	if (newp < 0) goto popfails;
	ctx.popln = poplns [newp];
	setpop ();
	res = moveclass (ser1, ser2);
	if (res > bestdrop)	{
		bestdrop = res;
		bser1 = ser1;  bser2 = ser2;
		}
i2done:
	cmcpy (&ctx, &oldctx, sizeof (Context));
	setpop ();	/* Return to original popln */
	i2++;  if (i2 <= hiid) goto inner;
i1done:
	i1++;  if (i1 <= hiid) goto outer;

	goto alldone;

alldone:
	if (bser1 < 0)
		{flp();  succ = -1;
		printf ("No possible class move\n"); goto finish; }
/*	Copy pop to TrialPop, filled  */
	newp = copypop (pop->id, 1, "TrialPop");
	if (newp < 0) goto popfails;
	ctx.popln = poplns [newp];
	setpop ();
flp(); printf("TRYING MOVE CLASS %6d TO DAD %6d\n", bser1>>2, bser2>>2);
	res = moveclass (bser1, bser2);
	control = 0;  doall (1, 1);  control = AdjAll;
	if (heard) printf ("BestMoveClass ends prematurely\n");
/*	Setting dogood's target to origcost-1 allows early exit  */
/*	See if the trial model has improved over original  */
	succ = 1;
	if (rootcl->cbcost < origcost) goto winner;
	succ = 0;
	if (force) goto winner;
	setbadm (3, bser1, bser2);
	cmcpy (&ctx, &oldctx, sizeof (Context));
	setpop ();      /* Return to original popln */
	flp(); printf ("Failed ******\n");
	goto finish;

popfails:
	succ = -1;
	flp(); printf ("Cannot make TrialPop\n");  goto finish;

winner:
	flp();
	printf ("%s\n", (succ) ? "ACCEPTED !!!" : "FORCED");
	printtree();  clearbadm();
/*	Reverse roles of 'work' and TrialPop  */
	strcpy (oldctx.popln->name, "TrialPop");
	strcpy (pop->name, "work");
	if (succ) trackbest(1);

finish:		if (nosubs > 0) nosubs --;  return (succ);
}


/*	--------------------  trymoves  ----------------------------   */
/*	Tries moving classes using bestmoveclass until ntry attempts in
succession have failed, or until all possible moves have been tried   */
void trymoves (ntry)
	Sw ntry;
{
	Sw nfail, succ;

	nosubs ++;
	doall (1, 1);
	clearbadm ();
	nfail = 0;

	while (nfail < ntry)	{
		succ = bestmoveclass (0);
		if (succ < 0) goto finish;
		nfail ++;
		if (succ) nfail = 0;
		if (heard) {
			printf ("Trymoves ends prematurely\n");
			goto finish;
			}
		}

finish:
	if (nosubs > 0) nosubs --;
	return;
}
