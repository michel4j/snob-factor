#define NOTGLOB 1
#define POPLNS 1
#include "glob.c"
#include <string.h>

/*	-----------------------  setpop  --------------------------    */
void setpop ()
{
	pop = ctx.popln;   samp = ctx.sample;  vst = ctx.vset;
	nv = vst->nv;  vlist = vst->vlist;
	if (samp)	{
		nc = samp->nc;  svars = samp->svars;
		reclen = samp->reclen;  recs = samp->recs;
		}
	else		{
		nc = 0;  svars = 0;
		recs = 0;
		}
	pvars = pop->pvars;
	root = pop->root;
	rootcl = pop->classes [root];
	return;
}


/*	-----------------------  nextclass  ---------------------  */
/*	given a ptr to a ptr to a class in pop, changes the Class* to point
to the next class in a depth-first traversal, or 0 if there is none  */
void nextclass (ptr)
	Class **ptr;
{
	Class *clp;

	clp = *ptr;
	if (clp->ison >= 0) { *ptr = pop->classes[clp->ison]; goto done; }
loop:
	if (clp->isib >= 0) { *ptr = pop->classes[clp->isib]; goto done; }
	if (clp->idad < 0)  { *ptr = 0; goto done; }
	clp = pop->classes[clp->idad];  goto loop;

done:	return;
}


/*	-----------------------   makepop   ---------------------   */
/*      To make a population with the parameter set 'vst'  */
/*	Returns index of new popln in poplns array	*/
/*	The popln is given a root class appropriate for the variable-set.
	If fill = 0, root has only basics and stats, but no weight, cost or
or score vectors, and the popln is not connected to the current sample.
OTHERWIZE, the root class is fully configured for the current sample.
	*/
Sw makepop (fill)
	Sw fill;
{
	PVinst *pvars;
	Class *cls;

	Sw indx, i;

	samp = ctx.sample;  vst = ctx.vset;
	if (samp) nc = samp->nc;  else nc = 0;
	if ((! samp) && fill)	{
		printf ("Makepop cannot fill because no sample defined\n");
		return (-1);    }
/*	Find vacant popln slot    */
	for (indx = 0; indx < Maxpoplns; indx++)  {
		if (poplns[indx] == 0) goto gotit;
		}
	goto nospace;

gotit:
	pop = poplns[indx]= (Popln*) malloc (sizeof (Popln));
	if (! pop) goto nospace;
	pop->id = indx;
	pop->nc = 0;  strcpy (pop->samplename, "??");
	strcpy (pop->vstname, vst->name);
	pop->pblks = pop->jblks = 0;
	ctx.popln = pop;
	for (i = 0; i < 80; i++) pop->name[i] = 0;
	strcpy (pop->name, "newpop");
	pop->dfname[0] = 0;
	pop->ncl = 0;	/*  Initially no class  */

/*	Make vector of PVinsts    */
	pvars = pop->pvars = (PVinst*) gtsp(1, nv * sizeof (PVinst));
	if (! pvars) goto nospace;
/*	Copy from variable-set AVinsts to PVinsts  */
	for (i = 0; i < nv; i++)	{
		avi = vlist+i;  vtp = avi->vtp;    pvi = pvars+i;
		pvi->id = avi->id;
		pvi->paux = (Sc*) gtsp(1, vtp->pauxsize);
		if (! pvi->paux) goto nospace;
		}

/*	Make an empty vector of ptrs to class structures, initially
	allowing for MaxClasses classes  */
	pop->classes = (Class**) gtsp(1, MaxClasses * sizeof (Class*));
	if (! pop->classes) goto nospace;
	pop->mncl = MaxClasses;
	for (i = 0; i < pop->mncl; i++) pop->classes[i] = 0;

	if (fill)  {  strcpy (pop->samplename, samp->name);  pop->nc = nc; }
	root = pop->root = makeclass ();
	if (root < 0) goto nospace;
	cls = pop->classes [root];
	cls->serial = 4;
	pop->nextserial = 2;
	cls->idad = -2;  /* root has no dad */
	cls->relab = 1.0;
/*      Set type as leaf, no fac  */
	cls->type = Leaf;  cls->use = Plain;
	cls->cnt = 0.0;

	setpop();
	return (indx);

nospace:
	printf ("No space for another population\n");
	return (-1);
}


/*	----------------------  firstpop  ---------------------  */
/*	To make an initial population given a vset and a samp.
	Should return a popln index for a popln with a root class set up
and non-fac estimates done  */
Sw firstpop ()
{
	Sw ipop;

	clearbadm ();
	samp = ctx.sample;  vst = ctx.vset;
	ipop = -1;
	if (! samp) { printf ("No sample defined for firstpop\n");
		goto finish;  }
	ipop = makepop (1);   /* Makes a configured popln */
	if (ipop < 0) goto finish;
	strcpy (pop->name, "work");


	setpop ();
/*	Run doall on the root alone  */
	doall (5, 0);

finish:
	return (ipop);
}
/*	----------------------  makesubs -----------------------    */
/*	Given a class index kk, makes two subclasses  */

void makesubs (kk)
	Sw kk;
{
	Class *clp, *clsa, *clsb;
	CVinst *cvi, *scvi;
	Sf cntk;
	Sw i, kka, kkb, iv, nch;

	if (nosubs) return;
	clp = pop->classes [kk];
	setclass1 (clp);
	clsa = clsb = 0;
/*	Check that class has no subs   */
	if (cls->nson > 0)  { i = 0;  goto finish; }
/*	And that it is big enough to support subs    */
	cntk = cls->cnt;
	if (cntk < (2 * MinSize + 2.0))	{ i = 1;  goto finish; }
/*	Ensure clp's as-dad params set to plain values  */
	control = 0;  adjustclass (clp, 0);  control = dcontrol;
	kka = makeclass ();
	if (kka < 0) {  i = 2;  goto finish;   }
	clsa = pop->classes[kka];

	kkb = makeclass ();
	if (kkb < 0) {  i = 2;  goto finish;   }
	clsb = pop->classes[kkb];
	clsa->serial = clp->serial + 1;
	clsb->serial = clp->serial + 2;

	setclass1 (clp);

/*	Fix hierarchy links.  */
	cls->nson = 2;
	cls->ison = kka;
	clsa->idad = clsb->idad = kk;
	clsa->nson = clsb->nson = 0;
	clsa->isib = kkb;  clsb->isib = -1;

/*	Copy kk's Basics into both subs  */
	for (iv = 0; iv < nv; iv++)	{
		cvi = cls->basics[iv];
		scvi = clsa->basics[iv];
		nch = vlist[iv].basicsize;
		cmcpy (scvi, cvi, nch);
		scvi = clsb->basics[iv];
		cmcpy (scvi, cvi, nch);
		}

	clsa->age = 0;   clsb->age = 0;
	clsa->type = clsb->type = Sub;
	clsa->use = clsb->use = Plain;
	clsa->relab = clsb->relab = 0.5 * cls->relab;
	clsa->mlogab = clsb->mlogab = - log (clsa->relab);
	clsa->cnt = clsb->cnt = 0.5 * cls->cnt;
	i = 3;

finish:
	if (i < 3)	{
		if (clsa) { clsa->type = Vacant; clsa->idad = Dead; }
		if (clsb) { clsb->type = Vacant; clsb->idad = Dead; }
		}
	return;
}


/*	-------------------- killpop --------------------  */
/*	To destroy popln index px    */
void killpop (px)
	Sw px;
{
	Sw prev;
	if (ctx.popln) prev = ctx.popln->id;  else prev = -1;
	pop = poplns [px];
	if (! pop) return;

	ctx.popln = pop;  setpop();
	freesp (2);
	freesp (1);
	free (pop);
	poplns[px] = 0;
	pop = ctx.popln = 0;
	if (px == prev) return;
	if (prev < 0) return;
	ctx.popln = poplns[prev];
	setpop();
	return;
}



/*	------------------- copypop ---------------------  */
/*	Copies poplns[p1] into a popln called <newname>.  If no
current popln has this name, it makes one.
	The new popln has the variable-set of p1, and if fill, is
attached to the sample given in p1's samplename, if p1 is attached.
If fill but p1 is not attached, the new popln is attached to the current
sample shown in ctx, if compatible, and given small, silly factor scores.
	If <newname> exists, it is discarded.
	If fill = 0, only basic and stats info is copied.
	Returns index of new popln.
	If newname is "work", the context ctx is left addressing the new
popln, but otherwise is not disturbed.
	*/
Sw copypop (p1, fill, newname)
	Sw p1, fill;
	char *newname;
{
	Popln *fpop;
	Context oldctx;
	Class *cls, *fcls;
	CVinst *cvi, *fcvi;
	EVinst *evi, *fevi;
	Sf nomcnt;
	Sw indx, sindx, kk, jdad, nch, n, i, iv, hiser;

	nomcnt = 0.0;
	cmcpy (&oldctx, &ctx, sizeof (Context));
	indx = -1;
	fpop = poplns [p1];
	if (! fpop) {
		printf ("No popln index %d\n", p1);
		indx = -106;  goto finish;
		}
	kk = vname2id (fpop->vstname);
	if (kk < 0)	{
		printf ("No Variable-set %s\n", fpop->vstname);
		indx = -101;  goto finish;
		}
	vst = ctx.vset = vsets [kk];
	sindx = -1;  nc = 0;
	if (! fill) goto sampfound;
	if (fpop->nc) { sindx = sname2id (fpop->samplename, 1);
		goto sampfound;  }
/*	Use current sample if compatible  */
	samp = ctx.sample;
	if (samp && (! strcmp (samp->vstname, vst->name)))	{
		sindx = samp->id;  goto sampfound;  }
/*	Fill is indicated but no sample is defined.  */
	printf ("There is no defined sample to fill for.\n");
	indx = -103;  goto finish;

sampfound:
	if (sindx >= 0)	{
		samp = ctx.sample = samples [sindx];
		nc = samp->nc;
		}
	else { samp = ctx.sample = 0;  fill = nc = 0;  }

/*	See if destn popln already exists  */
	i = pname2id (newname);
/*	Check for copying into self  */
	if (i == p1)	{
		printf ("From copypop: attempt to copy model%3d into itself\n", p1+1);
		indx = -102;  goto finish;  }
	if (i >= 0) killpop (i);
/*	Make a new popln  */
	indx = makepop (fill);
	if (indx < 0)	{
		printf ("Cant make new popln from%4d\n", p1+1);
		indx = -104;  goto finish;
		}

	ctx.popln = poplns [indx];  setpop();
	if (samp) { strcpy (pop->samplename, samp->name);  pop->nc = samp->nc;}
	else { strcpy (pop->samplename, "??");  pop->nc = 0; }
	strcpy (pop->name, newname);
	hiser = 0;

/*	We must begin copying classes, begining with fpop's root  */
	fcls = fpop->classes[root];  jdad = -1;
/*	In case the pop is unattached (fpop->nc = 0), prepare a nominal
count to insert in leaf classes  */
	if (fill & (! fpop->nc))	{
		nomcnt = samp->nact;  nomcnt = nomcnt / fpop->nleaf;
		}

newclass:
	if (fcls->idad < 0)	kk = pop->root;
	else  kk = makeclass();
	if (kk < 0) { indx = -105;  goto finish; }
	cls = pop->classes[kk];
	nch = ((char*) & cls->id) - ((char*) cls);
	cmcpy (cls, fcls, nch);
	cls->isib = cls->ison = -1;
	cls->idad = jdad;  cls->nson = 0;
	if (cls->serial > hiser) hiser = cls->serial;

/*	Copy Basics. the structures should have been made.  */
	for (iv = 0; iv < nv; iv++)	{
		fcvi = fcls->basics[iv];
		cvi = cls->basics[iv];
		nch = vlist[iv].basicsize;
		cmcpy (cvi, fcvi, nch);
		}

/*	Copy stats  */
	for (iv = 0; iv < nv; iv++)        {
		fevi = fcls->stats[iv];
		evi = cls->stats[iv];
		nch = vlist[iv].statssize;
		cmcpy (evi, fevi, nch);
		}
	if (fill == 0) goto classdone;

/*	If fpop is not attached, we cannot copy thing-specific data */
	if (fpop->nc == 0) goto fakeit;
/*	Copy scores  */
	for (n = 0; n < nc; n++)
		cls->vv[n] = fcls->vv[n];
	goto classdone;

fakeit:		/*  initialize scorevectors  */
	for (n = 0; n < nc; n++)	{
		rec = recs + n * reclen;
		cls->vv[n] = 0;
		}
	cls->cnt = nomcnt;
	if (cls->idad < 0)   /* Root class */   cls->cnt = samp->nact;

classdone:
	if (fill) setbestparall (cls);
/*	Move on to a son class, if any, else a sib class, else back up */
	if ((fcls->ison >= 0) && (fpop->classes[fcls->ison]->type != Sub)) {
		jdad = cls->id;
		fcls = fpop->classes[fcls->ison];  goto newclass;
		}
siborback:
	if (fcls->isib >= 0) { fcls = fpop->classes[fcls->isib];
				goto newclass;  }
	if (fcls->idad < 0) goto alldone;
	cls = pop->classes[cls->idad];  jdad = cls->idad;
	fcls = fpop->classes[fcls->idad];  goto siborback;

alldone:	/*  All classes copied. Tidy up */
	tidy (0);
	pop->nextserial = (hiser >> 2) + 1;

finish:
/*	Unless newname = "work", revert to old context  */
	if (strcmp (newname, "work") || (indx < 0))
		cmcpy (&ctx, &oldctx, sizeof (Context));
	setpop ();
	return (indx);
}


/*	----------------------  printsubtree  ----------------------   */
static Sw pdeep;
void printsubtree(kk)
	Sw kk;
{
	Class *son, *clp;
	Sw kks;

	for (kks = 0; kks < pdeep; kks++) printf("   ");
	clp = pop->classes [kk];
	printf ("%s", sers (clp));
	if (clp->type == Dad) printf(" Dad");
	if (clp->type == Leaf) printf("Leaf");
	if (clp->type ==  Sub) printf(" Sub");
	if (clp->holdtype) printf (" H");  else printf ("  ");
	if (clp->use == Fac) printf (" Fac");
	if (clp->use == Tiny) printf (" Tny");
	if (clp->use == Plain) printf ("    ");
	printf(" Scan%8d", clp->scancnt);
	printf(" RelAb%6.3f  Size%8.1f\n", clp->relab, clp->newcnt);

	pdeep++;
	for (kks = clp->ison;  kks >= 0;  kks = son->isib)	{
		son = pop->classes[kks];
		printsubtree (kks);
		}
	pdeep --;
	return;
}

/*	---------------------  printtree  ---------------------------  */
void printtree()
{

	pop = ctx.popln;
	if (! pop)	{
		printf ("No current population.\n");  return;
		}
	setpop();
	printf ("Popln%3d on sample%3d,%4d classes,%6d things",
		pop->id+1, samp->id+1, pop->ncl, samp->nc);
	printf ("  Cost %10.2f\n", pop->classes[pop->root]->cbcost);
	printf("\n  Assign mode ");
	if (fix == Partial) printf ("Partial    ");
	if (fix == Most_likely) printf ("Most_likely");
	if (fix == Random) printf ("Random     ");
	printf ("--- Adjust:");
	if (control&AdjPr) printf(" Params");
	if (control & AdjSc) printf(" Scores");
	if (control & AdjTr) printf(" Tree");
	printf ("\n");
	pdeep = 0;
	printsubtree (root);
	return;
}


/*	--------------------  bestpopid  -----------------------------  */
/*	Finds the popln whose name is "BST_" followed by the name of
ctx.samp.  If no such popln exists, makes one by copying ctx.popln.
Returns id of popln, or -1 if failure  */

/*	The char BSTname[] is used to pass the popln name to trackbest */
	char BSTname[80];

Sw bestpopid ()
{
	Sw i;

	i = -1;
	if (! ctx.popln) goto gotit;  if (! ctx.sample) goto gotit;
	setpop ();

	strcpy (BSTname, "BST_");  strcpy (BSTname+4, samp->name);
	i = pname2id (BSTname);
	if (i >= 0) goto gotit;

/*	No such popln exists.  Make one using copypop  */
	i = copypop (pop->id, 0, BSTname);
	if (i < 0) goto gotit;
/*	Set bestcost in samp from current cost  */
	samp->bestcost = pop->classes[root]->cbcost;
/*	Set goodtime as current pop age  */
	samp->goodtime = pop->classes[root]->age;

gotit:
	return (i);
}


/*	-----------------------  trackbest  ----------------------------  */
/*	If current model is better than 'BST_...', updates BST_... */
void trackbest (verify)
	Sw verify;
{
	Popln *bstpop;
	Sw bstid, bstroot, kk;
	Sf bstcst;

	if (! ctx.popln) return;
/*	Check current popln is 'work'  */
	if (strcmp ("work", ctx.popln->name)) return;
/*	Don't believe cost if fix is random  */
	if (fix == Random) return;
/*	Compare current and best costs  */
	bstid = bestpopid ();
	if (bstid < 0)	{
		printf ("Cannot make BST_ model\n");
		return;	}
	bstpop = poplns [bstid];
	bstroot = bstpop->root;
	bstcst = bstpop->classes[bstroot]->cbcost;
	if (pop->classes[root]->cbcost >= bstcst) return;
/*	Current looks better, but do a doall to ensure cost is correct */
/*	But only bother if 'verify'  */
	if (verify)	{
		kk = control;  control = 0;  doall (1,1);  control = kk;
		if (pop->classes[root]->cbcost >= (bstcst)) return;
		}

/*	Seems better, so kill old 'bestmodel' and make a new one */
	setpop ();
	kk = copypop (pop->id, 0, BSTname);
	samp->bestcost = pop->classes[root]->cbcost;
	samp->goodtime = pop->classes[root]->age;
	return;
}


/*	------------------  pname2id  -----------------------------   */
/*	To find a popln with given name and return its id, or -1 if fail */
Sw pname2id (nam)
	Sc *nam;
{
	Sw i;
	char lname [80];

	strcpy (lname, "BST_");
	if (strcmp (nam, lname)) strcpy (lname, nam);
	else {
		if (ctx.sample)	strcpy (lname+4, ctx.sample->name);
		else return (-1);
		}
	for (i = 0; i < Maxpoplns; i++)	{
		if (poplns[i] && (! strcmp (lname, poplns[i]->name)))
			return (i);
		}
	return (-1);
}


/*	--------------  recordit  -----------------------  */
/*	To write characters to a file  */
void recordit (fll, from, nn)
	FILE *fll;  char *from;  Sw nn;
{
	Sw i;
	for (i = 0; i < nn; i++)	{
		fputc (*from, fll);  from++;
		}
	return;
}


/*	------------------- savepop ---------------------  */
/*	Copies poplns[p1] into a file called <newname>.
	If fill, thing weights and scores are recorded. If not, just
Basics and Stats. If fill but pop->nc = 0, behaves as for fill = 0.
	Returns length of recorded file.
	*/

char *saveheading = "Scnob-Model-Save-File";

Sw savepop (p1, fill, newname)
	Sw p1, fill;
	char *newname;
{
	FILE *fl;
	char oldname [80], *jp;
	Context oldctx;
	Class *cls;
	CVinst *cvi;
	EVinst *evi;
	Sw leng, nch, i, iv, nc, jcl;

	cmcpy (&oldctx, &ctx, sizeof(Context));
	fl = 0;
	if (! poplns[p1])  {
		printf ("No popln index %d\n", p1);
		leng = -106;  goto finish;
		}
/*	Begin by copying the popln to a clean TrialPop   */
	pop = poplns[p1];
	if (! strcmp (pop->name, "TrialPop"))	{
		printf ("Cannot save TrialPop\n");
		leng = -105;  goto finish;
		}
/*	Save old name  */
	strcpy (oldname, pop->name);
/*	If name begins "BST_", change it to "BSTP" to avoid overwriting
a perhaps better BST_ model existing at time of restore.  */
	for (i = 0; i < 4; i++)	if (oldname[i] != "BST_"[i]) goto namefixed;
	oldname [3] = 'P';
	printf ("This model will be saved with name BSTP... not BST_...\n");
namefixed:
	i = pname2id ("TrialPop");
	if (i >= 0) killpop (i);
	nc = pop->nc;  if (! nc) fill = 0;  if (! fill) nc = 0;
	i = copypop (p1, fill, "TrialPop");
	if (i < 0) { leng = i; goto finish;  }
/*	We can now be sure that, in TrialPop, all subs are gone and classes
	have id-s from 0 up, starting with root  */
/*	switch context to TrialPop  */
	ctx.popln = poplns[i];   setpop ();
	vst = ctx.vset = vsets [vname2id (pop->vstname)];
	nc = pop->nc;  if (! fill) nc = 0;

	fl = fopen (newname, "w");
	if (! fl)	{
		printf("Cannot open %s\n", newname);  leng = -102;
		goto finish;
		}
/*	Output some heading material  */
	for (jp = saveheading; *jp; jp++) { fputc(*jp, fl);}
	fputc ('\n', fl); 
	for (jp = oldname; *jp; jp++) { fputc(*jp, fl);}
	fputc ('\n', fl); 
	for (jp = pop->vstname; *jp; jp++) { fputc(*jp, fl);}
	fputc ('\n', fl); 
	for (jp = pop->samplename; *jp; jp++) { fputc(*jp, fl);}
	fputc ('\n', fl); 
	fprintf (fl, "%4d%10d\n", pop->ncl, nc);
	fputc ('+', fl);  fputc ('\n', fl);

/*	We must begin copying classes, begining with pop's root  */
	jcl = 0;
/*	The classes should be lined up in pop->classes  */

newclass:
	cls = pop->classes [jcl];
	leng = 0;
	nch = ((char*) & cls->id) - ((char*) cls);
	recordit (fl, cls, nch);  leng += nch;

/*	Copy Basics..  */
	for (iv = 0; iv < nv; iv++)	{
		cvi = cls->basics[iv];
		nch = vlist[iv].basicsize;
		recordit (fl, cvi, nch);  leng += nch;
		}

/*	Copy stats  */
	for (iv = 0; iv < nv; iv++)        {
		evi = cls->stats[iv];
		nch = vlist[iv].statssize;
		recordit (fl, evi, nch);  leng += nch;
		}
	if (nc == 0) goto classdone;

/*	Copy scores  */
	nch = nc * sizeof (Sh);
	recordit (fl, cls->vv, nch);  leng += nch;
classdone:
	jcl++;  if (jcl < pop->ncl) goto newclass;

finish:
	fclose (fl);
	printf ("\nModel %s  Cost %10.2f  saved in file %s\n",
		oldname, pop->classes[0]->cbcost, newname);
	cmcpy (&ctx, &oldctx, sizeof (Context));
	setpop();
	return (leng);
}


/*	-----------------  restorepop  -------------------------------  */
/*	To read a model saved by savepop  */
Sw restorepop (nam)
	char *nam;
{
	char pname[80], name[80], *jp;
	Context oldctx;
	Sw i, j, k, indx, fncl, fnc, nch, iv;
	Class *cls;
	CVinst *cvi;  EVinst *evi;
	FILE *fl;

	indx = -999;
	cmcpy (&oldctx, &ctx, sizeof (Context));
	fl = fopen (nam, "r");
	if (! fl) { printf("Cannot open %s\n", nam);  goto error; }
	fscanf (fl, "%s", name);
	if (strcmp (name, saveheading))	{
		printf ("File is not a Scnob save-file\n"); goto error; }
	fscanf (fl, "%s", pname);
	printf ("Model %s\n", pname);
	fscanf (fl, "%s", name);   /* Reading v-set name */
	j = vname2id (name);
	if (j < 0)	{
		printf("Model needs variableset %s\n", name);  goto error; }
	vst = ctx.vset = vsets [j];  nv = vst->nv;
	fscanf (fl, "%s", name);   /* Reading sample name */
	fscanf (fl, "%d%d", &fncl, &fnc);  /* num of classes, cases */
/*	Advance to real data */
	while (fgetc (fl) != '+');  fgetc (fl);
	if (fnc)	{
		j = sname2id (name, 1);
		if (j < 0)	{
			printf("Sample %s unknown.\n", name);
			nc = 0;  samp = ctx.sample = 0;
			}
		else	{
			samp = ctx.sample = samples[j];
			if (samp->nc != fnc)	{
				printf("Size conflict Model%9d vs. Sample%9d\n",
					fnc, nc);  goto error;  }
			nc = fnc;
			}
		}
	else	{	/* file model unattached.  */
		samp = ctx.sample = 0;  nc = 0;
		}

/*	Build input model in TrialPop  */
	j = pname2id ("TrialPop");
	if (j >= 0) killpop (j);
	indx = makepop (nc);
	if (indx < 0) goto error;
	pop = ctx.popln = poplns[indx];  pop->nc = nc;
	strcpy (pop->name, "TrialPop");
	if (! nc) strcpy (pop->samplename, "??");
	setpop ();  pop->nleaf = 0;
/*	Popln has a root class set up. We read into it.  */
	j = 0;  goto haveclass;

newclass:
	j = makeclass ();
	if (j < 0) { printf("RestoreClass fails in Makeclass\n"); goto error; }
haveclass:
	cls = pop->classes[j];
	jp = (char*) cls;  nch = ((char*) &cls->id) - jp;
	for (k = 0; k < nch; k++) {*jp = fgetc(fl); jp++; }
	for (iv = 0; iv < nv; iv++)	{
		cvi = cls->basics[iv];  nch = vlist[iv].basicsize;
		jp = (char*) cvi;
		for (k = 0; k < nch; k++) {*jp = fgetc(fl); jp++; }
		}
	for (iv = 0; iv < nv; iv++)     {
		evi = cls->stats[iv];  nch = vlist[iv].statssize;
		jp = (char*) evi;
		for (k = 0; k < nch; k++) {*jp = fgetc(fl); jp++; }
		}
	if (cls->type == Leaf) pop->nleaf++;
	if ( ! fnc) goto classdone;

/*	Read scores but throw away if nc = 0  */
	if (! nc)	{
		nch = fnc * sizeof (Sh);
		for (k = 0; k < nch; k++) fgetc (fl);
		goto classdone;
		}
	nch = nc * sizeof (Sh);
	jp = (char*) (cls->vv);
	for (k = 0; k < nch; k++) {*jp = fgetc(fl); jp++; }

classdone:
	if (pop->ncl < fncl) goto newclass;

	i = pname2id (pname);
	if (i >= 0)	{
		printf ("Overwriting old model %s\n", pname);
		killpop (i);
		}
	if ( ! strcmp (pname, "work"))  goto pickwork;
	strcpy (pop->name, pname);
error:
	cmcpy (&ctx, &oldctx, sizeof (Context));  goto finish;

pickwork:
	indx = loadpop (pop->id);
	j = pname2id ("Trialpop");  if (j >= 0) killpop (j);
finish:
	setpop (); return (indx);
}


/*	----------------------  loadpop  -----------------------------  */
/*	To load popln pp into work, attaching sample as needed.
	Returns index of work, and sets ctx, or returns neg if fail  */
Sw loadpop (pp)
	Sw pp;
{
	Sw j, windx, fpopnc;
	Context oldctx;
	Popln *fpop;

	windx = -1;
	cmcpy (& oldctx, &ctx, sizeof (Context));
	fpop = 0;
	if (pp < 0) goto error;
	fpop = pop = poplns[pp];  if (! pop) goto error;
/*	Save the 'nc' of the popln to be loaded  */
	fpopnc = pop->nc;
/*	Check popln vset  */
	j = vname2id (pop->vstname);
	if (j < 0) {
		printf ("Load cannot find variable set\n");  goto error; }
	vst = ctx.vset = vsets[j];
/*	Check Vset  */
	if (strcmp (vst->name, oldctx.sample->vstname))	{
		printf ("Picked popln has incompatible VariableSet\n");
		goto error;
		}
/*	Check sample   */
	if (fpopnc && strcmp (pop->samplename, oldctx.sample->name))  {
		printf ("Picked popln attached to non-current sample.\n");
	/*	Make pop appear unattached  */
		pop->nc = 0;
		}

	ctx.sample = samp = oldctx.sample;
	windx = copypop (pp, 1, "work");  if (windx < 0) goto error;
	if (poplns[pp]->nc) goto finish;

/*	The popln was copied as if unattached, so scores, weights must be
	fixed  */
	printf ("Model will have weights, scores adjusted to sample.\n");
	fix = Partial;  control = AdjSc;
fixscores:
	seeall = 16;
	doall (15, 0);
/*	doall should leave a count of score changes in global  */
flp(); printf ("%8d  score changes\n", scorechanges);
	if (heard)      {
		flp();  printf("Score fixing stopped prematurely\n");
		}
	else if (scorechanges > 1) goto fixscores;
	fix = dfix;  control = dcontrol;
	goto finish;

error:
	cmcpy (&ctx, &oldctx, sizeof (Context));
finish:
/*	Restore 'nc' of copied popln  */
	if (fpop) fpop->nc = fpopnc;
	setpop();
	if (windx >= 0) printtree();
	return (windx);
}


/*	-------------  correlpops  ----------------------  */
/*	Given a popln id attached to same sample as 'work' popln,
prints the overlap between the leaves of one popln and leaves of other.
The output is a cross-tabulation table of the leaves of 'work' against the
leaves of the chosen popln, which must be attached to the current sample.
Table entries show the weight assigned to both one leaf of 'work' and one
leaf of the other popln, as a permillage of active things.
	*/
void correlpops (xid)
	Sw xid;
{
	float table [MaxClasses][MaxClasses];
	Class *wsons[MaxClasses], *xsons[MaxClasses];
	Popln *xpop, *wpop;
	Sf fnact, wwt;
	Sw wic, xic, n, pcw, wnl, xnl;


	setpop ();
	wpop = pop;
	xpop = poplns[xid];
	if (! xpop)	{
		printf ("No such model\n");  goto finish;  }
	if (! xpop->nc)	{
		printf ("Model is unattached\n");  goto finish; }
	if (strcmp (pop->samplename, xpop->samplename))	{
		printf ("Model not for same sample.\n");  goto finish; }
	if (xpop->nc != nc)	{
		printf ("Models have unequal numbers of cases.\n");
		goto finish;	}
/*	Should be able to proceed  */
	fnact = samp->nact;
	control = AdjSc;
	seeall = 4;  nosubs ++;  doall (3,1);
	wpop = pop;
	findall (Leaf);
	wnl = numson;
	for (wic = 0; wic < wnl; wic++) wsons[wic] = sons[wic];
/*	Switch to xpop  */
	ctx.popln = xpop;  setpop();
	seeall = 4;  doall (3,1);
/*	Find all leaves of xpop  */
	findall (Leaf);  xnl = numson;
	if ((wnl < 2) || (xnl < 2))	{
		printf ("Need at least 2 classes in each model.\n");
		goto finish;
		}
	for (xic = 0; xic < xnl; xic++) xsons[xic] = sons[xic];
/*	Clear table   */
	for (wic = 0; wic < wnl; wic++)	{
		for (xic = 0; xic < xnl; xic++) table [wic][xic] = 0.0;
		}

/*	Now accumulate cross products of weights for each active thing */
	seeall = 2;
	for (n = 0; n < nc; n++)	{
		ctx.popln = wpop;  setpop ();
		rec = recs + n * reclen;
		if (! *rec) goto ndone;
		findall (Leaf);
		docase (n, Leaf, 0);
		ctx.popln = xpop;  setpop();
		findall (Leaf);
		docase (n, Leaf, 0);
	/*	Should now have caseweights set in leaves of both poplns  */
		for (wic = 0; wic < wnl; wic++) {
			wwt = wsons[wic]->casewt;
			for (xic = 0; xic < xnl; xic++) {
				table[wic][xic] += wwt * xsons[xic]->casewt;
				}
			}
	ndone:	;
		}

/*	Print results	*/
	flp();
	printf ("Cross tabulation of popln classes as permillages.\n");
	printf ("Rows show 'work' classes, Columns show 'other' classes.\n");
	printf ("\nSer:");
	for (xic = 0; xic < xnl; xic++)
		printf ("%3d:", xsons[xic]->serial / 4);
	printf ("\n");
	fnact = 1000.0 / fnact;
	for (wic = 0; wic < wnl; wic++) {
		printf ("%3d:", wsons[wic]->serial / 4);
		for (xic = 0; xic < xnl; xic++) {
			pcw = table[wic][xic] * fnact + 0.5;
			if (pcw > 999) pcw = 999;
			printf ("%4d", pcw);
			}
		printf ("\n");
		}

finish:
	if (nosubs > 0) nosubs--;
	ctx.popln = wpop;  setpop();
	control = dcontrol;
	return;
}
