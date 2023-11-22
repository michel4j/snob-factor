#define NOTGLOB 1
#define DOALL 1
#include "glob.c"




/*	-------------------- sran, fran, uran -------------------------- */
Sw rseed;  static Sf rcons = (1.0 / (2048.0 * 1024.0 * 1024.0));
#define M32 0xFFFFFFFF
#define B32 0x80000000
Sw sran()
{
	Sw js;
	rseed = 69069 * rseed + 103322787;
	js = rseed & M32;
	return (js);
}

Sw uran()
{
	Sw js;
	rseed  = 69069 * rseed + 103322787;
	js = rseed & M32;
	if (js & B32) js = M32 - js;
	return (js & M32);
}

Sf fran()
{
	Sw js;
	rseed  = 69069 * rseed + 103322787;
	js = rseed & M32;
	if (js & B32) js = M32 - js;
	return (rcons * js);
}


/*	-------------------  findall  ---------------------------------  */
/*	Finds all classes of type(s) shown in bits of 'typ'.
	(Dad = 1, Leaf = 2, Sub = 4), so if typ = 7, will find all classes.*/
/*	Sets the classes in 'sons[]'  */
/*	Puts count of classes found in numson */
void findall (typ)
	Sw typ;
{
	Sw i, j, idi;
	Class *clp;

	setpop ();
	tidy (1);
	j = 0;
	clp = rootcl;
newcl:
	if (! (typ & clp->type)) goto skip;
	sons[j] = clp;
	j++;
skip:
	nextclass (&clp);
	if (clp) goto newcl;

	numson = j;

/*	Now set in nextic[i] the index in sons[] of the next class which is
not a decendent of sons[i], or numson if there is no such class.  */
	for (i = 0; i < numson; i++)	{
		idi = sons[i]->id;
		for (j = i+1; j < numson; j++)	{
			clp = sons[j];
		test:
			if (clp->id == idi) goto jfails;
			if (clp->idad < 0) goto found;
			clp = pop->classes [clp->idad];
			goto test;
		jfails:	;
			}
	/*	No non-descendant found  */
		j = numson;
	found:
		nextic [i] = j;
		}

	return;
}


/*	-------------------  sortsons  -----------------------------  */
/*	To re-arrange the sons in the son chain of class kk in order of
increasing serial number  */
void sortsons(kk)
	Sw kk;
{
	Class *clp, *cls1, *cls2;
	Sw js, *prev, nsw;

	clp = pop->classes[kk];
	if (clp->nson < 2) return;
scanchain:
	prev = & (clp->ison);
	nsw = 0;
nextsib:
	cls1 = pop->classes [*prev];
	if (cls1->isib < 0) goto endchain;
	cls2 = pop->classes [cls1->isib];
	if (cls1->serial > cls2->serial)	{
		*prev = cls2->id;  cls1->isib = cls2->isib;
		cls2->isib = cls1->id;  prev = & cls2->isib;  nsw = 1;
		}
	else prev = & cls1->isib;
	goto nextsib;

endchain:
	if (nsw) goto scanchain;

/*	Now sort sons  */
	for (js = clp->ison; js >= 0; js = pop->classes[js]->isib)
		sortsons (js);

	return;
}


/*	--------------------  tidy  -------------------------------  */
/*	Reconstructs ison, isib, nson linkages from idad-s. If hit
	and AdjTr, kills classes which are too small.
Also deletes singleton sonclasses.  Re-counts pop->ncl, pop->hicl, pop->nleaf.
	*/
void tidy (hit)
	Sw hit;
{
	Class *clp, *dad, *son;
	Sw i, kkd, ndead, newhicl, cause;

	deaded = 0;
	if (! pop->nc) hit = 0;
recheck:
	ndead = 0;
	for (i = 0; i <= pop->hicl; i++)	{
		clp = pop->classes[i];
		if (! clp) goto skip1;
		if (clp->type == Vacant) goto skip1;
		clp->nson = 0;  clp->ison = clp->isib = -1;
		if (i == root) goto skip1;
		kkd = clp->idad;
		if (kkd < 0) {printf("Dad error in tidy\n"); for(;;); }
		if (pop->classes[kkd]->type == Vacant)
			{ cause = Deadorphan;  goto diehard;  }
		if (hit)	{
		if ((clp->type==Sub) &&
			((clp->age > MaxSubAge) || nosubs))
			{ cause = Dead;  goto diehard; }	}
		if (hit & (clp->cnt < MinSize))
			{ cause = Deadsmall; goto die; }
		goto skip1;

	die:
		if (! (control & AdjTr)) goto skip1;
	diehard:
		if (seeall < 2) seeall = 2;
		clp->idad = cause;  clp->type = Vacant;  ndead++;
	skip1:	;
		}

	deaded += ndead;
	if (ndead) goto recheck;

/*	No more classes to kill for the moment.  Relink everyone  */
	pop->ncl = 0;  kkd = 0;
	for (i = 0; i <= pop->hicl; i++)	{
		clp = pop->classes[i];
		if (clp->type == Vacant) goto skip2;
		if (i == root) goto skip2;
		dad = pop->classes[clp->idad];
		clp->isib = dad->ison;  dad->ison = i;  dad->nson++;
	skip2:	;
		}

/*	Check for singleton sons   */
	for (kkd = 0; kkd <= pop->hicl; kkd++)	{
		dad = pop->classes[kkd];
		if (dad->type == Vacant) goto skip3;
		if (dad->nson != 1) goto skip3;
		clp = pop->classes[dad->ison];
	/*	Clp is dad's only son. If a sub, kill it   */
	/*	If not, make dad inherit clp's role, then kill clp  */
		if (clp->type == Sub)  { cause = Dead; goto killsing; }
		if (seeall < 2) seeall = 2;
		dad->type = clp->type;  dad->use = clp->use;
		dad->holdtype = clp->holdtype;
		dad->holduse = clp->holduse;
		dad->nson = clp->nson;  dad->ison = clp->ison;
	/* Change the dad in clp's sons */
		for (i = dad->ison; i>=0; i = son->isib)	{
			son = pop->classes[i];
			son->idad = kkd;
			}
		cause = Deadsing;
	killsing:
               clp->idad = cause;  clp->type = Vacant;  ndead++;
	skip3:	;
		}
	if (ndead) goto recheck;

	kkd = 0;
	if (! hit) goto finish;
	if (! (control & AdjTr)) goto finish;
	if (! newsubs) goto finish;
/*	Add subclasses to large-enough leaves  */
	for (i = 0; i <= pop->hicl; i++)	{
		dad = pop->classes[i];
		if (dad->type != Leaf) goto skip4;
		if (dad->nson) goto skip4;
		if (dad->cnt < (2.1 * MinSize)) goto skip4;
		if (dad->age < MinAge) goto skip4;
		makesubs (i);  kkd++;
	skip4:	;
		}

finish:
	deaded = 0;
/*	Re-count classes, leaves etc.  */
	pop->ncl = pop->nleaf = newhicl = kkd = 0;
	for (i = 0; i <= pop->hicl; i++)	{
		clp = pop->classes[i];
		if (! clp) goto skip5;
		if (clp->type == Vacant) goto skip5;
		if (clp->type == Leaf) pop->nleaf++;  pop->ncl++;
		newhicl = i;
		if (clp->serial > kkd) kkd = clp->serial;
	skip5:	;
		}
	pop->hicl = newhicl;  pop->nextserial = (kkd >> 2) + 1;
	sortsons (pop->root);
	return;
}


/*	------------------------  doall  --------------------------   */
/*	To do a complete cost-assign-adjust cycle on all things.
	If 'all', does it for all classes, else just leaves  */
/*	Leaves in scorechanges a count of significant score changes in Leaf
	classes whose use is Fac  */
Sw doall (ncy, all)
	Sw ncy, all;
{
	Sw nit, nfail, nn, ic, ncydone, ncyask;
	Sf oldcost, leafsum, oldleafsum;

	nfail = nit = ncydone = 0;  ncyask = ncy;
	all = (all) ?(Dad+Leaf+Sub) : Leaf;
	oldcost = rootcl->cbcost;
/*	Get sum of class costs, meaningful only if 'all' = Leaf  */
	oldleafsum = 0.0;
	findall (Leaf);
	for (ic = 0; ic < numson; ic++)	{
		oldleafsum += sons[ic]->cbcost;
		}


again:
	if ((nit % NewSubsTime) == 0) { newsubs = 1;
				if (seeall < 2) seeall = 2; }
		else newsubs = 0;
	if ((ncy - nit) <= 2) seeall = ncy - nit;
	if (ncy < 2) seeall = 2;
	if (nosubs) newsubs = 0;
	if ((nit > NewSubsTime) && (seeall == 1)) trackbest (0);
again2:
	if (fix == Random) seeall = 3;
	tidy (1);
	if (nit >= (ncy-1)) all = (Dad+Leaf+Sub);
	findall (all);
	for (ic = 0; ic < numson; ic++)	{
		cleartcosts (sons[ic]);
		}

	for (nn = 0; nn < samp->nc; nn++)	{
		docase (nn, all, 1);
	/*	docase ignores classes with ignore bit in cls->vv[] for the case
		unless seeall is on.  */
		}

/*	All classes in sons[] now have stats assigned to them.
	If all=Leaf, the classes are all leaves, so we just re-estimate their
	parameters and get their pcosts for fac and plain uses, using 'adjust'.
	But first, check all newcnt-s for vanishing classes.
	*/
	if (control & (AdjPr + AdjTr))  {
		for (ic = 0; ic < numson; ic++) {
			if (sons[ic]->newcnt < MinSize)  {
				sons[ic]->cnt = 0.0;  sons[ic]->type = Vacant;
				seeall = 2;  newsubs = 0;  goto again2;
				}
			}	
		}
	if (all == (Dad+Leaf+Sub)) goto dothelot;
	leafsum = 0.0;
	for (ic = 0; ic < numson; ic++) {
		adjustclass (sons[ic], 0);
	/*	The second para tells adjust not to do as-dad params  */
		leafsum += sons[ic]->cbcost;
		}
	if (seeall == 0) { rep ('.');  goto testnfail; }
	if (leafsum < (oldleafsum - MinGain))	{
		nfail = 0;  oldleafsum = leafsum;  rep ('L');
		}
	else	{ nfail++;  rep('l'); }
	goto testnfail;

dothelot:	/* all = 7, so we have dads, leaves and subs to do.
			We do from bottom up, collecting as-dad pcosts.  */
	cls = rootcl;
newdad:
	cls->cnpcost = 0.0;
	if (cls->nson < 2) goto complete;
	dad = cls;  cls = pop->classes [cls->ison];  goto newdad;

complete:
	adjustclass (cls, 1);
	if (cls->idad < 0) goto alladjusted;
	dad = pop->classes [cls->idad];
	dad->cnpcost += cls->cbpcost;
	if (cls->isib >= 0)  {
		cls = pop->classes [cls->isib];  goto newdad;
		}
/*	dad is now complete   */
	cls = dad;
	goto complete;

alladjusted:
/*	Test for an improvement  */
	if (seeall == 0) { rep ('.');  goto testnfail; }
	if (rootcl->cbcost < (oldcost - MinGain))  {
		nfail = 0;  oldcost = rootcl->cbcost;  rep('A');
		}
	else  { nfail++;  rep('a'); }

testnfail:
	if (nfail > GiveUp)	{
		if (all != Leaf) goto finish;
	/*	But if we were doing just leaves, wind up with a couple of
		'doall' cycles  */
		all = Dad + Leaf + Sub;
		ncy = 2;  nit = nfail = 0;  goto again;
		}
	if ((! usestdin) && hark (commsbuf.inl)) goto kicked;
	if (seeall > 0) seeall --;
	ncydone++;  nit++;  if (nit < ncy) goto again;
	if (ncydone >= ncyask) ncydone = -1;  goto finish;

kicked:
	flp(); printf ("Doall interrupted after %4d steps\n", ncydone);

finish:
/*	Scan leaf classes whose use is 'Fac' to accumulate significant
	score changes.  */
	scorechanges = 0;
	for (ic = 0; ic <= pop->hicl; ic++)	{
		cls = pop->classes[ic];
		if (cls && (cls->type == Leaf) && (cls->use == Fac))
			scorechanges += cls->scorechange;
		}
	return (ncydone);
}


/*	----------------------  dodads  -----------------------------  */
/*	Runs adjustclass on all leaves without adjustment.
	This leaves class cb*costs set up. Adjustclass is told not to
	consider a leaf as a potential dad.
	Then runs ncostvarall on all dads, with param adjustment. The
	result is to recost and readjust the tree hierarchy.
	*/
Sw dodads (ncy)
	Sw ncy;
{
	Class *dad;
	Sf oldcost;
	Sw nn, nfail;

	if (! (control & AdjPr)) ncy = 1;

/*	Capture no-prior params for subless leaves  */
	findall (Leaf);
	nfail = control;  control = Noprior;
	for (nn = 0;  nn < numson;  nn++)
		adjustclass (sons[nn], 0);
	control = nfail;
	nn = nfail = 0;
cycle:
	oldcost = rootcl->cnpcost;
	if (rootcl->type != Dad) return (0);
/*	Begin a recursive scan of classes down to leaves   */
	cls = rootcl;

newdad:
	if (cls->type == Leaf) goto complete;
	cls->cnpcost = 0.0;
	cls->relab = cls->cnt = 0.0;
	dad = cls;  cls = pop->classes [cls->ison];  goto newdad;

complete:
/*	If a leaf, use adjustclass, else use ncostvarall  */
	if (cls->type == Leaf)	{
		control = Tweak;
		adjustclass (cls, 0);
		}
	else	{
		control = AdjPr;
		ncostvarall (cls, 1);
		cls->cbpcost = cls->cnpcost;
		}
	if (cls->idad < 0) goto alladjusted;
	dad = pop->classes [cls->idad];
	dad->cnpcost += cls->cbpcost;
	dad->cnt += cls->cnt;
	dad->relab += cls->relab;
	if (cls->isib >= 0) { cls = pop->classes[cls->isib]; goto newdad; }
	cls = dad;  goto complete;

alladjusted:
	rootcl->cbpcost = rootcl->cnpcost;
	rootcl->cbcost = rootcl->cnpcost + rootcl->cntcost;
/*	Test for convergence  */
	nn++;  nfail++;
	if (rootcl->cnpcost < (oldcost-MinGain))	nfail = 0;
	rep ((nfail) ? 'd' : 'D');
	if (nfail > 3) goto finish;
	if (nn < ncy) goto cycle;
	nn = -1;
finish:
	control = dcontrol;
	return (nn);
}


/*	-----------------  dogood  -----------------------------  */
/*	Does cycles combining doall, doleaves, dodads   */

/*	Uses this table of old costs to see if useful change in last 5 cycles */
	Sf olddogcosts [6];

Sw dogood (ncy, target)
	Sw ncy;  Sf target;
{
	Sw j, nn, nfail;
	Sf oldcost;

	doall (1, 1);
	for (nn = 0; nn < 6; nn++) olddogcosts [nn] = rootcl->cbcost + 10000.0;
	nfail = 0;
	for (nn = 0; nn < ncy; nn++)	{
		oldcost = rootcl->cbcost;
		doall (2, 0);
		if (rootcl->cbcost < (oldcost-MinGain)) nfail = 0; else nfail++;
		rep ((nfail) ? 'g' : 'G');
		if (heard) goto kicked;
		if (nfail > 2) goto done;
		if (rootcl->cbcost < target) goto bullseye;
	/*	See if new cost significantly better than cost 5 cycles ago */
		for (j = 0; j < 5; j++) olddogcosts [j] = olddogcosts [j+1];
		olddogcosts [5] = rootcl->cbcost;
		if ((olddogcosts [0] - olddogcosts [5]) < 0.2) goto done;
		}
	nn = -1;  goto done;

bullseye:
	flp();
	printf("Dogood reached target after %4d cycles\n", nn);
	goto done;

kicked:
	flp();
	printf ("Dogood interrupted after %4d cycles\n", nn);
done:
	return (nn);
}


/*	-----------------------  docase  -----------------------------   */
/*	It is assumed that all classes have parameter info set up, and
	that all cases have scores in all classes.
	Assumes findall() has been used to find classes and set up
sons[], numson
	If 'derivs', calcs derivatives. Otherwize not.
	*/
/*	Defines and uses a prototypical 'Saux' containing missing and Datum
	fields */
typedef struct PSauxst	{
	Sw missing;
	Sf dummy;
	Sf xn;
	} PSaux;

void docase (cse, all, derivs)
	Sw cse, all, derivs;
{
	Sf mincost, sum, rootcost, low, diff, w1, w2;
	Class *sub1, *sub2;
	PSaux *psaux;
	Sw clc, i;

	rec = recs + cse * reclen;  /*  Set ptr to case record  */
	thing = cse;
	if (! *rec) goto casedone1;  /* Inactive thing */

/*	Unpack data into 'xn' fields of the Saux for each variable. The
'xn' field is at the beginning of the Saux. Also the "missing" flag. */
	for (i = 0; i < nv; i++)	{
		loc = rec + svars[i].offset;
		psaux = (PSaux *) svars[i].saux;
		if (*loc == 1)	{ psaux->missing = 1; }
		else	{
			psaux->missing = 0;
			cmcpy (& (psaux->xn), loc+1, vlist[i].vtp->datsize);
			}
		}

/*	Deal with every class, as set up in sons[]  */

	clc = 0;
	while (clc < numson)	{
		setclass2 (sons[clc]);
		if ((!seeall) && (icvv & 1)) { /* Ignore this and decendants */
				clc = nextic [clc];  goto doneclc;
				}
		else if (! seeall) cls->scancnt ++;
	/*	Score and cost the class  */
		scorevarall (cls);
		costvarall (cls);
		clc ++;
	doneclc:	;
		}
/*	Now have casescost, casefcost and casecost set in all classes for
	this case. We can distribute weight to all leaves, using their
	casecosts.  */
/*	The whole thing is irrelevant if just starting on root  */
	if (numson == 1) goto collected;   /*  Just doing root  */
/*	Clear all casewts   */
	for (clc = 0;  clc < numson;  clc++) sons[clc]->casewt = 0.0;
	mincost = 1.0e30;
	clc = 0;
	while (clc < numson)	{
		cls = sons[clc];
		if ((! seeall) && (cls->caseiv & 1)) {
			cls->casecost = 1.0e30;
			clc = nextic [clc];  goto doneclass1;
			}
		clc++;
		if (fix == Random)	{
			w1 = 2.0 * fran();
			cls->casecost += w1;  cls->casefcost += w1;
			cls->casescost += w1;
			}
		if (cls->type != Leaf) goto doneclass1;
		if (cls->casecost < mincost)  {
			mincost = cls->casecost;
			}
	doneclass1:	;
		}

	if (fix != Most_likely) goto getweights;
/*	Assign all weight to the most likely leaf, or split it if
	more than one most likely. Count costs equal to mincost  */
	sum = 0.0;
	for (clc = 0; clc < numson; clc++)	{
		cls = sons [clc];
		if ((cls->type == Leaf) && (cls->casecost == mincost))  {
			sum += 1.0;  cls->casewt = 1.0;
			}
		}
	goto normweights;

getweights:
/*	Minimum cost is in mincost. Compute unnormalized weights  */
	sum = 0.0;
	clc = 0;
	while (clc < numson)	{
		cls = sons[clc];
		if ((cls->caseiv & 1) && (! seeall))
			{ clc = nextic [clc];  goto doneclass2; }
		clc++;
		if (cls->type != Leaf) goto doneclass2;
		cls->casewt = exp (mincost - cls->casecost);
		sum += cls->casewt;
	doneclass2:	;
		}

normweights:
/*	Normalize weights, and set root's casecost  */
/*	It can happen that sum = 0. If so, give up on this case   */
	if (sum <= 0.0) goto casedone1;
	if (fix == Random) rootcost = mincost;
		else rootcost = mincost - log (sum);
	rootcl->casencost = rootcl->casecost = rootcost;
	sum = 1.0 / sum;
	clc = 0;
	while (clc < numson)    {
		cls = sons[clc];
		if ((cls->caseiv & 1) && (! seeall))
			{ clc = nextic [clc];  goto doneclass3; }
		clc++;
		if (cls->type != Leaf) goto doneclass3;
		cls->casewt *= sum;
	/*	Can distribute this weight among subs, if any  */
	/*	But only if subs included  */
		if (! (all & Sub)) goto doneclass3;
		if (cls->nson != 2) goto doneclass3;
		if (cls->casewt == 0.0) goto doneclass3;
		sub1 = pop->classes[cls->ison];
		sub2 = pop->classes[sub1->isib];
	/*	Test subclass ignore flags unless seeall   */
		if (seeall) goto subscosted;
		if (sub1->caseiv & 1)	{
			if (! (sub2->caseiv & 1))	{
				sub2->casewt = cls->casewt;
				cls->casencost = sub2->casecost;
				goto doneclass3;
				}
			}
		else	{
			if (sub2->caseiv & 1)   {  /* Only sub1 has weight */
				sub1->casewt = cls->casewt;
				cls->casencost = sub1->casecost;
				goto doneclass3;
				}
			}
	subscosted:
	/*	Both subs costed  */
		diff = sub1->casecost - sub2->casecost;
	/*	Diff can be used to set cls's casencost  */
		if (diff < 0.0)	{
			low = sub1->casecost;
			w2 = exp (diff);   w1 = 1.0 / (1.0 + w2);  w2 *= w1;
			if (w2 < MinSubWt) sub2->caseiv |= 1;
				else sub2->caseiv &= -2;
			sub2->vv[thing] = sub2->caseiv;
			if (fix == Random) cls->casencost = low;
				else cls->casencost = low + log (w1);
			}
		else	{
			low = sub2->casecost;
			w1 = exp(-diff);  w2 = 1.0 / (1.0 + w1);  w1 *= w2;
			if (w1 < MinSubWt) sub1->caseiv |= 1;
				else sub1->caseiv &= -2;
			sub1->vv[thing] = sub1->caseiv;
			if (fix == Random) cls->casencost = low;
				else cls->casencost = low + log (w2);
			}
	/*	Assign randomly if sub age 0, or to-best if sub age < MinAge */
		if (sub1->age < MinAge)	{
			if (sub1->age == 0)	{
				w1 = (sran()<0) ? 1.0 : 0.0;
				}
			else	{
				w1 = (diff < 0) ? 1.0 : 0.0;
				}
			w2 = 1.0 - w1;
			}
		sub1->casewt = cls->casewt * w1;
		sub2->casewt = cls->casewt * w2;

	doneclass3:	;
		}

/*	We have now assigned caseweights to all Leafs and Subs.
	Collect weights from leaves into Dads, setting their casecosts  */
	if (rootcl->type == Leaf) goto collected;  /* root is only leaf */
	for (clc = numson-1; clc >= 0; clc--)	{
		cls = sons[clc];
		if (cls->type == Sub) goto doneclass4;
		if ((!seeall) && (cls->vv[thing] & 1)) goto doneclass4;
		if (cls->casewt < MinWt) cls->vv[thing] |= 1;
			else cls->vv[thing] &= -2;
		if (cls->idad >= 0)
			pop->classes[cls->idad]->casewt += cls->casewt;
		if (cls->type == Dad)	{
/*	casecost for the completed dad is root's cost - log dad's wt  */
	if (cls->casewt > 0.0)	cls->casencost = rootcost - log (cls->casewt);
	else  cls->casencost = rootcost + 200.0;
	cls->casecost = cls->casencost;
			}
	doneclass4:	;
		}

collected:
	rootcl->casewt = 1.0;
/*	Now all classes have casewt assigned, I hope. Can proceed to
collect statistics from this case  */
	if (! derivs) goto casedone1;
	for (clc = 0;  clc < numson;  clc++)   {
		cls = sons [clc];
		if (cls->casewt > 0.0)	{
			derivvarall (cls);
			}
		}

casedone1:
	return;
}
