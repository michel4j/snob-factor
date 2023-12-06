#define NOTGLOB 1
#define POPLNS 1
#include "glob.h"
#include <string.h>

/*	-----------------------  setpop  --------------------------    */
void setpop() {
    population = ctx.popln;
    samp = ctx.sample;
    vst = ctx.vset;
    nv = vst->length;
    vlist = vst->variables;
    if (samp) {
        nc = samp->num_cases;
        svars = samp->variables;
        reclen = samp->record_length;
        recs = samp->records;
    } else {
        nc = 0;
        svars = 0;
        recs = 0;
    }
    pvars = population->variables;
    root = population->root;
    rootcl = population->classes[root];
    return;
}

/*	-----------------------  nextclass  ---------------------  */
/*	given a ptr to a ptr to a class in pop, changes the Class* to point
to the next class in a depth-first traversal, or 0 if there is none  */
void nextclass(Class **ptr)
{
    Class *clp;

    clp = *ptr;
    if (clp->son_id >= 0) {
        *ptr = population->classes[clp->son_id];
        goto done;
    }
loop:
    if (clp->sib_id >= 0) {
        *ptr = population->classes[clp->sib_id];
        goto done;
    }
    if (clp->dad_id < 0) {
        *ptr = 0;
        goto done;
    }
    clp = population->classes[clp->dad_id];
    goto loop;

done:
    return;
}

/*	-----------------------   makepop   ---------------------   */
/*      To make a population with the parameter set 'vst'  */
/*	Returns index of new popln in poplns array	*/
/*	The popln is given a root class appropriate for the variable-set.
    If fill = 0, root has only basics and stats, but no weight, cost or
or score vectors, and the popln is not connected to the current sample.
OTHERWIZE, the root class is fully configured for the current sample.
    */
int makepop(int fill){
    PVinst *pvars;
    Class *cls;

    int indx, i;

    samp = ctx.sample;
    vst = ctx.vset;
    if (samp)
        nc = samp->num_cases;
    else
        nc = 0;
    if ((!samp) && fill) {
        printf("Makepop cannot fill because no sample defined\n");
        return (-1);
    }
    /*	Find vacant popln slot    */
    for (indx = 0; indx < MAX_POPULATIONS; indx++) {
        if (poplns[indx] == 0)
            goto gotit;
    }
    goto nospace;

gotit:
    population = poplns[indx] = (Population *)malloc(sizeof(Population));
    if (!population)
        goto nospace;
    population->id = indx;
    population->sample_size = 0;
    strcpy(population->sample_name, "??");
    strcpy(population->vst_name, vst->name);
    population->blocks = population->model_blocks = 0;
    ctx.popln = population;
    for (i = 0; i < 80; i++)
        population->name[i] = 0;
    strcpy(population->name, "newpop");
    population->filename[0] = 0;
    population->num_classes = 0; /*  Initially no class  */

    /*	Make vector of PVinsts    */
    pvars = population->variables = (PVinst *)gtsp(1, nv * sizeof(PVinst));
    if (!pvars)
        goto nospace;
    /*	Copy from variable-set AVinsts to PVinsts  */
    for (i = 0; i < nv; i++) {
        avi = vlist + i;
        vtp = avi->vtype;
        pvi = pvars + i;
        pvi->id = avi->id;
        pvi->paux = (char *)gtsp(1, vtp->pop_aux_size);
        if (!pvi->paux)
            goto nospace;
    }

    /*	Make an empty vector of ptrs to class structures, initially
        allowing for MAX_CLASSES classes  */
    population->classes = (Class **)gtsp(1, MAX_CLASSES * sizeof(Class *));
    if (!population->classes)
        goto nospace;
    population->cls_vec_len = MAX_CLASSES;
    for (i = 0; i < population->cls_vec_len; i++)
        population->classes[i] = 0;

    if (fill) {
        strcpy(population->sample_name, samp->name);
        population->sample_size = nc;
    }
    root = population->root = makeclass();
    if (root < 0)
        goto nospace;
    cls = population->classes[root];
    cls->serial = 4;
    population->next_serial = 2;
    cls->dad_id = -2; /* root has no dad */
    cls->relab = 1.0;
    /*      Set type as leaf, no fac  */
    cls->type = Leaf;
    cls->use = Plain;
    cls->weights_sum = 0.0;

    setpop();
    return (indx);

nospace:
    printf("No space for another population\n");
    return (-1);
}

/*	----------------------  firstpop  ---------------------  */
/*	To make an initial population given a vset and a samp.
    Should return a popln index for a popln with a root class set up
and non-fac estimates done  */
int firstpop() {
    int ipop;

    clearbadm();
    samp = ctx.sample;
    vst = ctx.vset;
    ipop = -1;
    if (!samp) {
        printf("No sample defined for firstpop\n");
        goto finish;
    }
    ipop = makepop(1); /* Makes a configured popln */
    if (ipop < 0)
        goto finish;
    strcpy(population->name, "work");

    setpop();
    /*	Run doall on the root alone  */
    doall(5, 0);

finish:
    return (ipop);
}
/*	----------------------  makesubs -----------------------    */
/*	Given a class index kk, makes two subclasses  */

void makesubs(int kk){
    Class *clp, *clsa, *clsb;
    CVinst *cvi, *scvi;
    double cntk;
    int i, kka, kkb, iv, nch;

    if (nosubs)
        return;
    clp = population->classes[kk];
    setclass1(clp);
    clsa = clsb = 0;
    /*	Check that class has no subs   */
    if (cls->num_sons > 0) {
        i = 0;
        goto finish;
    }
    /*	And that it is big enough to support subs    */
    cntk = cls->weights_sum;
    if (cntk < (2 * MinSize + 2.0)) {
        i = 1;
        goto finish;
    }
    /*	Ensure clp's as-dad params set to plain values  */
    control = 0;
    adjustclass(clp, 0);
    control = dcontrol;
    kka = makeclass();
    if (kka < 0) {
        i = 2;
        goto finish;
    }
    clsa = population->classes[kka];

    kkb = makeclass();
    if (kkb < 0) {
        i = 2;
        goto finish;
    }
    clsb = population->classes[kkb];
    clsa->serial = clp->serial + 1;
    clsb->serial = clp->serial + 2;

    setclass1(clp);

    /*	Fix hierarchy links.  */
    cls->num_sons = 2;
    cls->son_id = kka;
    clsa->dad_id = clsb->dad_id = kk;
    clsa->num_sons = clsb->num_sons = 0;
    clsa->sib_id = kkb;
    clsb->sib_id = -1;

    /*	Copy kk's Basics into both subs  */
    for (iv = 0; iv < nv; iv++) {
        cvi = cls->basics[iv];
        scvi = clsa->basics[iv];
        nch = vlist[iv].basic_size;
        memcpy(scvi, cvi, nch);
        scvi = clsb->basics[iv];
        memcpy(scvi, cvi, nch);
    }

    clsa->age = 0;
    clsb->age = 0;
    clsa->type = clsb->type = Sub;
    clsa->use = clsb->use = Plain;
    clsa->relab = clsb->relab = 0.5 * cls->relab;
    clsa->mlogab = clsb->mlogab = -log(clsa->relab);
    clsa->weights_sum = clsb->weights_sum = 0.5 * cls->weights_sum;
    i = 3;

finish:
    if (i < 3) {
        if (clsa) {
            clsa->type = Vacant;
            clsa->dad_id = Dead;
        }
        if (clsb) {
            clsb->type = Vacant;
            clsb->dad_id = Dead;
        }
    }
    return;
}

/*	-------------------- killpop --------------------  */
/*	To destroy popln index px    */
void killpop(int px){
    int prev;
    if (ctx.popln)
        prev = ctx.popln->id;
    else
        prev = -1;
    population = poplns[px];
    if (!population)
        return;

    ctx.popln = population;
    setpop();
    freesp(2);
    freesp(1);
    free(population);
    poplns[px] = 0;
    population = ctx.popln = 0;
    if (px == prev)
        return;
    if (prev < 0)
        return;
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
int copypop(int p1, int fill, char *newname){
    Population *fpop;
    Context oldctx;
    Class *cls, *fcls;
    CVinst *cvi, *fcvi;
    EVinst *evi, *fevi;
    double nomcnt;
    int indx, sindx, kk, jdad, nch, n, i, iv, hiser;

    nomcnt = 0.0;
    memcpy(&oldctx, &ctx, sizeof(Context));
    indx = -1;
    fpop = poplns[p1];
    if (!fpop) {
        printf("No popln index %d\n", p1);
        indx = -106;
        goto finish;
    }
    kk = vname2id(fpop->vst_name);
    if (kk < 0) {
        printf("No Variable-set %s\n", fpop->vst_name);
        indx = -101;
        goto finish;
    }
    vst = ctx.vset = vsets[kk];
    sindx = -1;
    nc = 0;
    if (!fill)
        goto sampfound;
    if (fpop->sample_size) {
        sindx = sname2id(fpop->sample_name, 1);
        goto sampfound;
    }
    /*	Use current sample if compatible  */
    samp = ctx.sample;
    if (samp && (!strcmp(samp->vset_name, vst->name))) {
        sindx = samp->id;
        goto sampfound;
    }
    /*	Fill is indicated but no sample is defined.  */
    printf("There is no defined sample to fill for.\n");
    indx = -103;
    goto finish;

sampfound:
    if (sindx >= 0) {
        samp = ctx.sample = samples[sindx];
        nc = samp->num_cases;
    } else {
        samp = ctx.sample = 0;
        fill = nc = 0;
    }

    /*	See if destn popln already exists  */
    i = pname2id(newname);
    /*	Check for copying into self  */
    if (i == p1) {
        printf("From copypop: attempt to copy model%3d into itself\n", p1 + 1);
        indx = -102;
        goto finish;
    }
    if (i >= 0)
        killpop(i);
    /*	Make a new popln  */
    indx = makepop(fill);
    if (indx < 0) {
        printf("Cant make new popln from%4d\n", p1 + 1);
        indx = -104;
        goto finish;
    }

    ctx.popln = poplns[indx];
    setpop();
    if (samp) {
        strcpy(population->sample_name, samp->name);
        population->sample_size = samp->num_cases;
    } else {
        strcpy(population->sample_name, "??");
        population->sample_size = 0;
    }
    strcpy(population->name, newname);
    hiser = 0;

    /*	We must begin copying classes, begining with fpop's root  */
    fcls = fpop->classes[root];
    jdad = -1;
    /*	In case the pop is unattached (fpop->nc = 0), prepare a nominal
    count to insert in leaf classes  */
    if (fill & (!fpop->sample_size)) {
        nomcnt = samp->num_active;
        nomcnt = nomcnt / fpop->num_leaves;
    }

newclass:
    if (fcls->dad_id < 0)
        kk = population->root;
    else
        kk = makeclass();
    if (kk < 0) {
        indx = -105;
        goto finish;
    }
    cls = population->classes[kk];
    nch = ((char *)&cls->id) - ((char *)cls);
    memcpy(cls, fcls, nch);
    cls->sib_id = cls->son_id = -1;
    cls->dad_id = jdad;
    cls->num_sons = 0;
    if (cls->serial > hiser)
        hiser = cls->serial;

    /*	Copy Basics. the structures should have been made.  */
    for (iv = 0; iv < nv; iv++) {
        fcvi = fcls->basics[iv];
        cvi = cls->basics[iv];
        nch = vlist[iv].basic_size;
        memcpy(cvi, fcvi, nch);
    }

    /*	Copy stats  */
    for (iv = 0; iv < nv; iv++) {
        fevi = fcls->stats[iv];
        evi = cls->stats[iv];
        nch = vlist[iv].stats_size;
        memcpy(evi, fevi, nch);
    }
    if (fill == 0)
        goto classdone;

    /*	If fpop is not attached, we cannot copy item-specific data */
    if (fpop->sample_size == 0)
        goto fakeit;
    /*	Copy scores  */
    for (n = 0; n < nc; n++)
        cls->factor_scores[n] = fcls->factor_scores[n];
    goto classdone;

fakeit: /*  initialize scorevectors  */
    for (n = 0; n < nc; n++) {
        record = recs + n * reclen;
        cls->factor_scores[n] = 0;
    }
    cls->weights_sum = nomcnt;
    if (cls->dad_id < 0) /* Root class */
        cls->weights_sum = samp->num_active;

classdone:
    if (fill)
        setbestparall(cls);
    /*	Move on to a son class, if any, else a sib class, else back up */
    if ((fcls->son_id >= 0) && (fpop->classes[fcls->son_id]->type != Sub)) {
        jdad = cls->id;
        fcls = fpop->classes[fcls->son_id];
        goto newclass;
    }
siborback:
    if (fcls->sib_id >= 0) {
        fcls = fpop->classes[fcls->sib_id];
        goto newclass;
    }
    if (fcls->dad_id < 0)
        goto alldone;
    cls = population->classes[cls->dad_id];
    jdad = cls->dad_id;
    fcls = fpop->classes[fcls->dad_id];
    goto siborback;

alldone: /*  All classes copied. Tidy up */
    tidy(0);
    population->next_serial = (hiser >> 2) + 1;

finish:
    /*	Unless newname = "work", revert to old context  */
    if (strcmp(newname, "work") || (indx < 0))
        memcpy(&ctx, &oldctx, sizeof(Context));
    setpop();
    return (indx);
}

/*	----------------------  printsubtree  ----------------------   */
static int pdeep;
void printsubtree(int kk){
    Class *son, *clp;
    int kks;

    for (kks = 0; kks < pdeep; kks++)
        printf("   ");
    clp = population->classes[kk];
    printf("%s", sers(clp));
    if (clp->type == Dad)
        printf(" Dad");
    if (clp->type == Leaf)
        printf("Leaf");
    if (clp->type == Sub)
        printf(" Sub");
    if (clp->hold_type)
        printf(" H");
    else
        printf("  ");
    if (clp->use == Fac)
        printf(" Fac");
    if (clp->use == Tiny)
        printf(" Tny");
    if (clp->use == Plain)
        printf("    ");
    printf(" Scan%8d", clp->scancnt);
    printf(" RelAb%6.3f  Size%8.1f\n", clp->relab, clp->newcnt);

    pdeep++;
    for (kks = clp->son_id; kks >= 0; kks = son->sib_id) {
        son = population->classes[kks];
        printsubtree(kks);
    }
    pdeep--;
    return;
}

/*	---------------------  printtree  ---------------------------  */
void printtree() {

    population = ctx.popln;
    if (!population) {
        printf("No current population.\n");
        return;
    }
    setpop();
    printf("Popln%3d on sample%3d,%4d classes,%6d things", population->id + 1,
           samp->id + 1, population->num_classes, samp->num_cases);
    printf("  Cost %10.2f\n", population->classes[population->root]->best_cost);
    printf("\n  Assign mode ");
    if (fix == Partial)
        printf("Partial    ");
    if (fix == Most_likely)
        printf("Most_likely");
    if (fix == Random)
        printf("Random     ");
    printf("--- Adjust:");
    if (control & AdjPr)
        printf(" Params");
    if (control & AdjSc)
        printf(" Scores");
    if (control & AdjTr)
        printf(" Tree");
    printf("\n");
    pdeep = 0;
    printsubtree(root);
    return;
}

/*	--------------------  bestpopid  -----------------------------  */
/*	Finds the popln whose name is "BST_" followed by the name of
ctx.samp.  If no such popln exists, makes one by copying ctx.popln.
Returns id of popln, or -1 if failure  */

/*	The char BSTname[] is used to pass the popln name to trackbest */
char BSTname[80];

int bestpopid() {
    int i;

    i = -1;
    if (!ctx.popln)
        goto gotit;
    if (!ctx.sample)
        goto gotit;
    setpop();

    strcpy(BSTname, "BST_");
    strcpy(BSTname + 4, samp->name);
    i = pname2id(BSTname);
    if (i >= 0)
        goto gotit;

    /*	No such popln exists.  Make one using copypop  */
    i = copypop(population->id, 0, BSTname);
    if (i < 0)
        goto gotit;
    /*	Set bestcost in samp from current cost  */
    samp->best_cost = population->classes[root]->best_cost;
    /*	Set goodtime as current pop age  */
    samp->best_time = population->classes[root]->age;

gotit:
    return (i);
}

/*	-----------------------  trackbest  ----------------------------  */
/*	If current model is better than 'BST_...', updates BST_... */
void trackbest(int verify) {
    Population *bstpop;
    int bstid, bstroot, kk;
    double bstcst;

    if (!ctx.popln)
        return;
    /*	Check current popln is 'work'  */
    if (strcmp("work", ctx.popln->name))
        return;
    /*	Don't believe cost if fix is random  */
    if (fix == Random)
        return;
    /*	Compare current and best costs  */
    bstid = bestpopid();
    if (bstid < 0) {
        printf("Cannot make BST_ model\n");
        return;
    }
    bstpop = poplns[bstid];
    bstroot = bstpop->root;
    bstcst = bstpop->classes[bstroot]->best_cost;
    if (population->classes[root]->best_cost >= bstcst)
        return;
    /*	Current looks better, but do a doall to ensure cost is correct */
    /*	But only bother if 'verify'  */
    if (verify) {
        kk = control;
        control = 0;
        doall(1, 1);
        control = kk;
        if (population->classes[root]->best_cost >= (bstcst))
            return;
    }

    /*	Seems better, so kill old 'bestmodel' and make a new one */
    setpop();
    kk = copypop(population->id, 0, BSTname);
    samp->best_cost = population->classes[root]->best_cost;
    samp->best_time = population->classes[root]->age;
    return;
}

/*	------------------  pname2id  -----------------------------   */
/*	To find a popln with given name and return its id, or -1 if fail */
int pname2id(char *nam)
{
    int i;
    char lname[80];

    strcpy(lname, "BST_");
    if (strcmp(nam, lname))
        strcpy(lname, nam);
    else {
        if (ctx.sample)
            strcpy(lname + 4, ctx.sample->name);
        else
            return (-1);
    }
    for (i = 0; i < MAX_POPULATIONS; i++) {
        if (poplns[i] && (!strcmp(lname, poplns[i]->name)))
            return (i);
    }
    return (-1);
}

/**
 * Writes a sequence of characters to a file.
 * 
 * @param fll A pointer to the file to write to.
 * @param from A pointer to the sequence of characters to be written.
 * @param nn The number of characters to write.
 */
void recordit(FILE *fll, void *from, int nn) {
    char *buffer = (char*) from;

    for (int i = 0; i < nn; i++) {
        fputc(buffer[i], fll);
    }
}


/*	------------------- savepop ---------------------  */
/*	Copies poplns[p1] into a file called <newname>.
    If fill, item weights and scores are recorded. If not, just
Basics and Stats. If fill but pop->nc = 0, behaves as for fill = 0.
    Returns length of recorded file.
    */

char *saveheading = "Scnob-Model-Save-File";

int savepop(int p1, int fill, char *newname){
    FILE *fl;
    char oldname[80], *jp;
    Context oldctx;
    Class *cls;
    CVinst *cvi;
    EVinst *evi;
    int leng, nch, i, iv, nc, jcl;

    memcpy(&oldctx, &ctx, sizeof(Context));
    fl = 0;
    if (!poplns[p1]) {
        printf("No popln index %d\n", p1);
        leng = -106;
        goto finish;
    }
    /*	Begin by copying the popln to a clean TrialPop   */
    population = poplns[p1];
    if (!strcmp(population->name, "TrialPop")) {
        printf("Cannot save TrialPop\n");
        leng = -105;
        goto finish;
    }
    /*	Save old name  */
    strcpy(oldname, population->name);
    /*	If name begins "BST_", change it to "BSTP" to avoid overwriting
    a perhaps better BST_ model existing at time of restore.  */
    for (i = 0; i < 4; i++)
        if (oldname[i] != "BST_"[i])
            goto namefixed;
    oldname[3] = 'P';
    printf("This model will be saved with name BSTP... not BST_...\n");
namefixed:
    i = pname2id("TrialPop");
    if (i >= 0)
        killpop(i);
    nc = population->sample_size;
    if (!nc)
        fill = 0;
    if (!fill)
        nc = 0;
    i = copypop(p1, fill, "TrialPop");
    if (i < 0) {
        leng = i;
        goto finish;
    }
    /*	We can now be sure that, in TrialPop, all subs are gone and classes
        have id-s from 0 up, starting with root  */
    /*	switch context to TrialPop  */
    ctx.popln = poplns[i];
    setpop();
    vst = ctx.vset = vsets[vname2id(population->vst_name)];
    nc = population->sample_size;
    if (!fill)
        nc = 0;

    fl = fopen(newname, "w");
    if (!fl) {
        printf("Cannot open %s\n", newname);
        leng = -102;
        goto finish;
    }
    /*	Output some heading material  */
    for (jp = saveheading; *jp; jp++) {
        fputc(*jp, fl);
    }
    fputc('\n', fl);
    for (jp = oldname; *jp; jp++) {
        fputc(*jp, fl);
    }
    fputc('\n', fl);
    for (jp = population->vst_name; *jp; jp++) {
        fputc(*jp, fl);
    }
    fputc('\n', fl);
    for (jp = population->sample_name; *jp; jp++) {
        fputc(*jp, fl);
    }
    fputc('\n', fl);
    fprintf(fl, "%4d%10d\n", population->num_classes, nc);
    fputc('+', fl);
    fputc('\n', fl);

    /*	We must begin copying classes, begining with pop's root  */
    jcl = 0;
    /*	The classes should be lined up in pop->classes  */

newclass:
    cls = population->classes[jcl];
    leng = 0;
    nch = ((char *)&cls->id) - ((char *)cls);
    recordit(fl, cls, nch);
    leng += nch;

    /*	Copy Basics..  */
    for (iv = 0; iv < nv; iv++) {
        cvi = cls->basics[iv];
        nch = vlist[iv].basic_size;
        recordit(fl, cvi, nch);
        leng += nch;
    }

    /*	Copy stats  */
    for (iv = 0; iv < nv; iv++) {
        evi = cls->stats[iv];
        nch = vlist[iv].stats_size;
        recordit(fl, evi, nch);
        leng += nch;
    }
    if (nc == 0)
        goto classdone;

    /*	Copy scores  */
    nch = nc * sizeof(short);
    recordit(fl, cls->factor_scores, nch);
    leng += nch;
classdone:
    jcl++;
    if (jcl < population->num_classes)
        goto newclass;

finish:
    fclose(fl);
    printf("\nModel %s  Cost %10.2f  saved in file %s\n", oldname,
           population->classes[0]->best_cost, newname);
    memcpy(&ctx, &oldctx, sizeof(Context));
    setpop();
    return (leng);
}

/*	-----------------  restorepop  -------------------------------  */
/*	To read a model saved by savepop  */
int restorepop(char *nam){
    char pname[80], name[80], *jp;
    Context oldctx;
    int i, j, k, indx, fncl, fnc, nch, iv;
    Class *cls;
    CVinst *cvi;
    EVinst *evi;
    FILE *fl;

    indx = -999;
    memcpy(&oldctx, &ctx, sizeof(Context));
    fl = fopen(nam, "r");
    if (!fl) {
        printf("Cannot open %s\n", nam);
        goto error;
    }
    fscanf(fl, "%s", name);
    if (strcmp(name, saveheading)) {
        printf("File is not a Scnob save-file\n");
        goto error;
    }
    fscanf(fl, "%s", pname);
    printf("Model %s\n", pname);
    fscanf(fl, "%s", name); /* Reading v-set name */
    j = vname2id(name);
    if (j < 0) {
        printf("Model needs variableset %s\n", name);
        goto error;
    }
    vst = ctx.vset = vsets[j];
    nv = vst->length;
    fscanf(fl, "%s", name);          /* Reading sample name */
    fscanf(fl, "%d%d", &fncl, &fnc); /* num of classes, cases */
                                     /*	Advance to real data */
    while (fgetc(fl) != '+')
        ;
    fgetc(fl);
    if (fnc) {
        j = sname2id(name, 1);
        if (j < 0) {
            printf("Sample %s unknown.\n", name);
            nc = 0;
            samp = ctx.sample = 0;
        } else {
            samp = ctx.sample = samples[j];
            if (samp->num_cases != fnc) {
                printf("Size conflict Model%9d vs. Sample%9d\n", fnc, nc);
                goto error;
            }
            nc = fnc;
        }
    } else { /* file model unattached.  */
        samp = ctx.sample = 0;
        nc = 0;
    }

    /*	Build input model in TrialPop  */
    j = pname2id("TrialPop");
    if (j >= 0)
        killpop(j);
    indx = makepop(nc);
    if (indx < 0)
        goto error;
    population = ctx.popln = poplns[indx];
    population->sample_size = nc;
    strcpy(population->name, "TrialPop");
    if (!nc)
        strcpy(population->sample_name, "??");
    setpop();
    population->num_leaves = 0;
    /*	Popln has a root class set up. We read into it.  */
    j = 0;
    goto haveclass;

newclass:
    j = makeclass();
    if (j < 0) {
        printf("RestoreClass fails in Makeclass\n");
        goto error;
    }
haveclass:
    cls = population->classes[j];
    jp = (char *)cls;
    nch = ((char *)&cls->id) - jp;
    for (k = 0; k < nch; k++) {
        *jp = fgetc(fl);
        jp++;
    }
    for (iv = 0; iv < nv; iv++) {
        cvi = cls->basics[iv];
        nch = vlist[iv].basic_size;
        jp = (char *)cvi;
        for (k = 0; k < nch; k++) {
            *jp = fgetc(fl);
            jp++;
        }
    }
    for (iv = 0; iv < nv; iv++) {
        evi = cls->stats[iv];
        nch = vlist[iv].stats_size;
        jp = (char *)evi;
        for (k = 0; k < nch; k++) {
            *jp = fgetc(fl);
            jp++;
        }
    }
    if (cls->type == Leaf)
        population->num_leaves++;
    if (!fnc)
        goto classdone;

    /*	Read scores but throw away if nc = 0  */
    if (!nc) {
        nch = fnc * sizeof(short);
        for (k = 0; k < nch; k++)
            fgetc(fl);
        goto classdone;
    }
    nch = nc * sizeof(short);
    jp = (char *)(cls->factor_scores);
    for (k = 0; k < nch; k++) {
        *jp = fgetc(fl);
        jp++;
    }

classdone:
    if (population->num_classes < fncl)
        goto newclass;

    i = pname2id(pname);
    if (i >= 0) {
        printf("Overwriting old model %s\n", pname);
        killpop(i);
    }
    if (!strcmp(pname, "work"))
        goto pickwork;
    strcpy(population->name, pname);
error:
    memcpy(&ctx, &oldctx, sizeof(Context));
    goto finish;

pickwork:
    indx = loadpop(population->id);
    j = pname2id("Trialpop");
    if (j >= 0)
        killpop(j);
finish:
    setpop();
    return (indx);
}

/*	----------------------  loadpop  -----------------------------  */
/*	To load popln pp into work, attaching sample as needed.
    Returns index of work, and sets ctx, or returns neg if fail  */
int loadpop(int pp){
    int j, windx, fpopnc;
    Context oldctx;
    Population *fpop;

    windx = -1;
    memcpy(&oldctx, &ctx, sizeof(Context));
    fpop = 0;
    if (pp < 0)
        goto error;
    fpop = population = poplns[pp];
    if (!population)
        goto error;
    /*	Save the 'nc' of the popln to be loaded  */
    fpopnc = population->sample_size;
    /*	Check popln vset  */
    j = vname2id(population->vst_name);
    if (j < 0) {
        printf("Load cannot find variable set\n");
        goto error;
    }
    vst = ctx.vset = vsets[j];
    /*	Check VarSet  */
    if (strcmp(vst->name, oldctx.sample->vset_name)) {
        printf("Picked popln has incompatible VariableSet\n");
        goto error;
    }
    /*	Check sample   */
    if (fpopnc && strcmp(population->sample_name, oldctx.sample->name)) {
        printf("Picked popln attached to non-current sample.\n");
        /*	Make pop appear unattached  */
        population->sample_size = 0;
    }

    ctx.sample = samp = oldctx.sample;
    windx = copypop(pp, 1, "work");
    if (windx < 0)
        goto error;
    if (poplns[pp]->sample_size)
        goto finish;

    /*	The popln was copied as if unattached, so scores, weights must be
        fixed  */
    printf("Model will have weights, scores adjusted to sample.\n");
    fix = Partial;
    control = AdjSc;
fixscores:
    seeall = 16;
    doall(15, 0);
    /*	doall should leave a count of score changes in global  */
    flp();
    printf("%8d  score changes\n", scorechanges);
    if (heard) {
        flp();
        printf("Score fixing stopped prematurely\n");
    } else if (scorechanges > 1)
        goto fixscores;
    fix = dfix;
    control = dcontrol;
    goto finish;

error:
    memcpy(&ctx, &oldctx, sizeof(Context));
finish:
    /*	Restore 'nc' of copied popln  */
    if (fpop)
        fpop->sample_size = fpopnc;
    setpop();
    if (windx >= 0)
        printtree();
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
void correlpops(int xid){
    float table[MAX_CLASSES][MAX_CLASSES];
    Class *wsons[MAX_CLASSES], *xsons[MAX_CLASSES];
    Population *xpop, *wpop;
    double fnact, wwt;
    int wic, xic, n, pcw, wnl, xnl;

    setpop();
    wpop = population;
    xpop = poplns[xid];
    if (!xpop) {
        printf("No such model\n");
        goto finish;
    }
    if (!xpop->sample_size) {
        printf("Model is unattached\n");
        goto finish;
    }
    if (strcmp(population->sample_name, xpop->sample_name)) {
        printf("Model not for same sample.\n");
        goto finish;
    }
    if (xpop->sample_size != nc) {
        printf("Models have unequal numbers of cases.\n");
        goto finish;
    }
    /*	Should be able to proceed  */
    fnact = samp->num_active;
    control = AdjSc;
    seeall = 4;
    nosubs++;
    doall(3, 1);
    wpop = population;
    findall(Leaf);
    wnl = numson;
    for (wic = 0; wic < wnl; wic++)
        wsons[wic] = sons[wic];
    /*	Switch to xpop  */
    ctx.popln = xpop;
    setpop();
    seeall = 4;
    doall(3, 1);
    /*	Find all leaves of xpop  */
    findall(Leaf);
    xnl = numson;
    if ((wnl < 2) || (xnl < 2)) {
        printf("Need at least 2 classes in each model.\n");
        goto finish;
    }
    for (xic = 0; xic < xnl; xic++)
        xsons[xic] = sons[xic];
    /*	Clear table   */
    for (wic = 0; wic < wnl; wic++) {
        for (xic = 0; xic < xnl; xic++)
            table[wic][xic] = 0.0;
    }

    /*	Now accumulate cross products of weights for each active item */
    seeall = 2;
    for (n = 0; n < nc; n++) {
        ctx.popln = wpop;
        setpop();
        record = recs + n * reclen;
        if (!*record)
            goto ndone;
        findall(Leaf);
        docase(n, Leaf, 0);
        ctx.popln = xpop;
        setpop();
        findall(Leaf);
        docase(n, Leaf, 0);
        /*	Should now have caseweights set in leaves of both poplns  */
        for (wic = 0; wic < wnl; wic++) {
            wwt = wsons[wic]->case_weight;
            for (xic = 0; xic < xnl; xic++) {
                table[wic][xic] += wwt * xsons[xic]->case_weight;
            }
        }
    ndone:;
    }

    /*	Print results	*/
    flp();
    printf("Cross tabulation of popln classes as permillages.\n");
    printf("Rows show 'work' classes, Columns show 'other' classes.\n");
    printf("\nSer:");
    for (xic = 0; xic < xnl; xic++)
        printf("%3d:", xsons[xic]->serial / 4);
    printf("\n");
    fnact = 1000.0 / fnact;
    for (wic = 0; wic < wnl; wic++) {
        printf("%3d:", wsons[wic]->serial / 4);
        for (xic = 0; xic < xnl; xic++) {
            pcw = table[wic][xic] * fnact + 0.5;
            if (pcw > 999)
                pcw = 999;
            printf("%4d", pcw);
        }
        printf("\n");
    }

finish:
    if (nosubs > 0)
        nosubs--;
    ctx.popln = wpop;
    setpop();
    control = dcontrol;
    return;
}
