#define NOTGLOB 1
#define POPLNS 1
#include "glob.h"
#include <string.h>

/*	-----------------------  set_population  --------------------------    */
void set_population() {
    Popln = CurCtx.popln;
    Smpl = CurCtx.sample;
    VSet = CurCtx.vset;
    nv = VSet->length;
    vlist = VSet->variables;
    if (Smpl) {
        nc = Smpl->num_cases;
        svars = Smpl->variables;
        reclen = Smpl->record_length;
        recs = Smpl->records;
    } else {
        nc = 0;
        svars = 0;
        recs = 0;
    }
    pvars = Popln->variables;
    root = Popln->root;
    rootcl = Popln->classes[root];
    return;
}

/*	-----------------------  next_class  ---------------------  */
/*	given a ptr to a ptr to a class in pop, changes the Class* to point
to the next class in a depth-first traversal, or 0 if there is none  */
void next_class(Class **ptr)
{
    Class *clp;

    clp = *ptr;
    if (clp->son_id >= 0) {
        *ptr = Popln->classes[clp->son_id];
        goto done;
    }
loop:
    if (clp->sib_id >= 0) {
        *ptr = Popln->classes[clp->sib_id];
        goto done;
    }
    if (clp->dad_id < 0) {
        *ptr = 0;
        goto done;
    }
    clp = Popln->classes[clp->dad_id];
    goto loop;

done:
    return;
}

/*	-----------------------   make_population   ---------------------   */
/*      To make a population with the parameter set 'vst'  */
/*	Returns index of new popln in poplns array	*/
/*	The popln is given a root class appropriate for the variable-set.
    If fill = 0, root has only basics and stats, but no weight, cost or
or score vectors, and the popln is not connected to the current sample.
OTHERWIZE, the root class is fully configured for the current sample.
    */
int make_population(int fill){
    PVinst *pvars;
    Class *cls;

    int indx, i;

    Smpl = CurCtx.sample;
    VSet = CurCtx.vset;
    if (Smpl)
        nc = Smpl->num_cases;
    else
        nc = 0;
    if ((!Smpl) && fill) {
        printf("Makepop cannot fill because no sample defined\n");
        return (-1);
    }
    /*	Find vacant popln slot    */
    for (indx = 0; indx < MAX_POPULATIONS; indx++) {
        if (Populations[indx] == 0)
            goto gotit;
    }
    goto nospace;

gotit:
    Popln = Populations[indx] = (Population *)malloc(sizeof(Population));
    if (!Popln)
        goto nospace;
    Popln->id = indx;
    Popln->sample_size = 0;
    strcpy(Popln->sample_name, "??");
    strcpy(Popln->vst_name, VSet->name);
    Popln->blocks = Popln->model_blocks = 0;
    CurCtx.popln = Popln;
    for (i = 0; i < 80; i++)
        Popln->name[i] = 0;
    strcpy(Popln->name, "newpop");
    Popln->filename[0] = 0;
    Popln->num_classes = 0; /*  Initially no class  */

    /*	Make vector of PVinsts    */
    pvars = Popln->variables = (PVinst *)alloc_blocks(1, nv * sizeof(PVinst));
    if (!pvars)
        goto nospace;
    /*	Copy from variable-set AVinsts to PVinsts  */
    for (i = 0; i < nv; i++) {
        avi = vlist + i;
        vtp = avi->vtype;
        pvi = pvars + i;
        pvi->id = avi->id;
        pvi->paux = (char *)alloc_blocks(1, vtp->pop_aux_size);
        if (!pvi->paux)
            goto nospace;
    }

    /*	Make an empty vector of ptrs to class structures, initially
        allowing for MAX_CLASSES classes  */
    Popln->classes = (Class **)alloc_blocks(1, MAX_CLASSES * sizeof(Class *));
    if (!Popln->classes)
        goto nospace;
    Popln->cls_vec_len = MAX_CLASSES;
    for (i = 0; i < Popln->cls_vec_len; i++)
        Popln->classes[i] = 0;

    if (fill) {
        strcpy(Popln->sample_name, Smpl->name);
        Popln->sample_size = nc;
    }
    root = Popln->root = make_class();
    if (root < 0)
        goto nospace;
    cls = Popln->classes[root];
    cls->serial = 4;
    Popln->next_serial = 2;
    cls->dad_id = -2; /* root has no dad */
    cls->relab = 1.0;
    /*      Set type as leaf, no fac  */
    cls->type = Leaf;
    cls->use = Plain;
    cls->weights_sum = 0.0;

    set_population();
    return (indx);

nospace:
    printf("No space for another population\n");
    return (-1);
}

/*	----------------------  init_population  ---------------------  */
/*	To make an initial population given a vset and a samp.
    Should return a popln index for a popln with a root class set up
and non-fac estimates done  */
int init_population() {
    int ipop;

    clr_bad_move();
    Smpl = CurCtx.sample;
    VSet = CurCtx.vset;
    ipop = -1;
    if (!Smpl) {
        printf("No sample defined for init_population\n");
        goto finish;
    }
    ipop = make_population(1); /* Makes a configured popln */
    if (ipop < 0)
        goto finish;
    strcpy(Popln->name, "work");

    set_population();
    /*	Run do_all on the root alone  */
    do_all(5, 0);

finish:
    return (ipop);
}
/*	----------------------  make_subclasses -----------------------    */
/*	Given a class index kk, makes two subclasses  */

void make_subclasses(int kk){
    Class *clp, *clsa, *clsb;
    CVinst *cvi, *scvi;
    double cntk;
    int i, kka, kkb, iv, nch;

    if (NoSubs)
        return;
    clp = Popln->classes[kk];
    set_class(clp);
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
    Control = 0;
    adjust_class(clp, 0);
    Control = DControl;
    kka = make_class();
    if (kka < 0) {
        i = 2;
        goto finish;
    }
    clsa = Popln->classes[kka];

    kkb = make_class();
    if (kkb < 0) {
        i = 2;
        goto finish;
    }
    clsb = Popln->classes[kkb];
    clsa->serial = clp->serial + 1;
    clsb->serial = clp->serial + 2;

    set_class(clp);

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
void destroy_population(int px){
    int prev;
    if (CurCtx.popln)
        prev = CurCtx.popln->id;
    else
        prev = -1;
    Popln = Populations[px];
    if (!Popln)
        return;

    CurCtx.popln = Popln;
    set_population();
    free_blocks(2);
    free_blocks(1);
    free(Popln);
    Populations[px] = 0;
    Popln = CurCtx.popln = 0;
    if (px == prev)
        return;
    if (prev < 0)
        return;
    CurCtx.popln = Populations[prev];
    set_population();
    return;
}

/*	------------------- copypop ---------------------  */
/*	Copies poplns[p1] into a popln called <newname>.  If no
current popln has this name, it makes one.
    The new popln has the variable-set of p1, and if fill, is
attached to the sample given in p1's samplename, if p1 is attached.
If fill but p1 is not attached, the new popln is attached to the current
sample shown in CurCtx, if compatible, and given small, silly factor scores.
    If <newname> exists, it is discarded.
    If fill = 0, only basic and stats info is copied.
    Returns index of new popln.
    If newname is "work", the context CurCtx is left addressing the new
popln, but otherwise is not disturbed.
    */
int copy_population(int p1, int fill, char *newname){
    Population *fpop;
    Context oldctx;
    Class *cls, *fcls;
    CVinst *cvi, *fcvi;
    EVinst *evi, *fevi;
    double nomcnt;
    int indx, sindx, kk, jdad, nch, n, i, iv, hiser;

    nomcnt = 0.0;
    memcpy(&oldctx, &CurCtx, sizeof(Context));
    indx = -1;
    fpop = Populations[p1];
    if (!fpop) {
        printf("No popln index %d\n", p1);
        indx = -106;
        goto finish;
    }
    kk = find_vset(fpop->vst_name);
    if (kk < 0) {
        printf("No Variable-set %s\n", fpop->vst_name);
        indx = -101;
        goto finish;
    }
    VSet = CurCtx.vset = VarSets[kk];
    sindx = -1;
    nc = 0;
    if (!fill)
        goto sampfound;
    if (fpop->sample_size) {
        sindx = find_sample(fpop->sample_name, 1);
        goto sampfound;
    }
    /*	Use current sample if compatible  */
    Smpl = CurCtx.sample;
    if (Smpl && (!strcmp(Smpl->vset_name, VSet->name))) {
        sindx = Smpl->id;
        goto sampfound;
    }
    /*	Fill is indicated but no sample is defined.  */
    printf("There is no defined sample to fill for.\n");
    indx = -103;
    goto finish;

sampfound:
    if (sindx >= 0) {
        Smpl = CurCtx.sample = Samples[sindx];
        nc = Smpl->num_cases;
    } else {
        Smpl = CurCtx.sample = 0;
        fill = nc = 0;
    }

    /*	See if destn popln already exists  */
    i = find_population(newname);
    /*	Check for copying into self  */
    if (i == p1) {
        printf("From copypop: attempt to copy model%3d into itself\n", p1 + 1);
        indx = -102;
        goto finish;
    }
    if (i >= 0)
        destroy_population(i);
    /*	Make a new popln  */
    indx = make_population(fill);
    if (indx < 0) {
        printf("Cant make new popln from%4d\n", p1 + 1);
        indx = -104;
        goto finish;
    }

    CurCtx.popln = Populations[indx];
    set_population();
    if (Smpl) {
        strcpy(Popln->sample_name, Smpl->name);
        Popln->sample_size = Smpl->num_cases;
    } else {
        strcpy(Popln->sample_name, "??");
        Popln->sample_size = 0;
    }
    strcpy(Popln->name, newname);
    hiser = 0;

    /*	We must begin copying classes, begining with fpop's root  */
    fcls = fpop->classes[root];
    jdad = -1;
    /*	In case the pop is unattached (fpop->nc = 0), prepare a nominal
    count to insert in leaf classes  */
    if (fill & (!fpop->sample_size)) {
        nomcnt = Smpl->num_active;
        nomcnt = nomcnt / fpop->num_leaves;
    }

newclass:
    if (fcls->dad_id < 0)
        kk = Popln->root;
    else
        kk = make_class();
    if (kk < 0) {
        indx = -105;
        goto finish;
    }
    cls = Popln->classes[kk];
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
        cls->weights_sum = Smpl->num_active;

classdone:
    if (fill)
        set_best_costs(cls);
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
    cls = Popln->classes[cls->dad_id];
    jdad = cls->dad_id;
    fcls = fpop->classes[fcls->dad_id];
    goto siborback;

alldone: /*  All classes copied. Tidy up */
    tidy(0);
    Popln->next_serial = (hiser >> 2) + 1;

finish:
    /*	Unless newname = "work", revert to old context  */
    if (strcmp(newname, "work") || (indx < 0))
        memcpy(&CurCtx, &oldctx, sizeof(Context));
    set_population();
    return (indx);
}

/*	----------------------  printsubtree  ----------------------   */
static int pdeep;
void printsubtree(int kk){
    Class *son, *clp;
    int kks;

    for (kks = 0; kks < pdeep; kks++)
        printf("   ");
    clp = Popln->classes[kk];
    printf("%s", serial_to_str(clp));
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
        son = Popln->classes[kks];
        printsubtree(kks);
    }
    pdeep--;
    return;
}

/*	---------------------  printtree  ---------------------------  */
void print_tree() {

    Popln = CurCtx.popln;
    if (!Popln) {
        printf("No current population.\n");
        return;
    }
    set_population();
    printf("Popln%3d on sample%3d,%4d classes,%6d things", Popln->id + 1,
           Smpl->id + 1, Popln->num_classes, Smpl->num_cases);
    printf("  Cost %10.2f\n", Popln->classes[Popln->root]->best_cost);
    printf("\n  Assign mode ");
    if (Fix == Partial)
        printf("Partial    ");
    if (Fix == Most_likely)
        printf("Most_likely");
    if (Fix == Random)
        printf("Random     ");
    printf("--- Adjust:");
    if (Control & AdjPr)
        printf(" Params");
    if (Control & AdjSc)
        printf(" Scores");
    if (Control & AdjTr)
        printf(" Tree");
    printf("\n");
    pdeep = 0;
    printsubtree(root);
    return;
}

/*	--------------------  get_best_pop  -----------------------------  */
/*	Finds the popln whose name is "BST_" followed by the name of
CurCtx.samp.  If no such popln exists, makes one by copying CurCtx.popln.
Returns id of popln, or -1 if failure  */

/*	The char BSTname[] is used to pass the popln name to track_best */
char BSTname[80];

int get_best_pop() {
    int i;

    i = -1;
    if (!CurCtx.popln)
        goto gotit;
    if (!CurCtx.sample)
        goto gotit;
    set_population();

    strcpy(BSTname, "BST_");
    strcpy(BSTname + 4, Smpl->name);
    i = find_population(BSTname);
    if (i >= 0)
        goto gotit;

    /*	No such popln exists.  Make one using copypop  */
    i = copy_population(Popln->id, 0, BSTname);
    if (i < 0)
        goto gotit;
    /*	Set bestcost in samp from current cost  */
    Smpl->best_cost = Popln->classes[root]->best_cost;
    /*	Set goodtime as current pop age  */
    Smpl->best_time = Popln->classes[root]->age;

gotit:
    return (i);
}

/*	-----------------------  track_best  ----------------------------  */
/*	If current model is better than 'BST_...', updates BST_... */
void track_best(int verify) {
    Population *bstpop;
    int bstid, bstroot, kk;
    double bstcst;

    if (!CurCtx.popln)
        return;
    /*	Check current popln is 'work'  */
    if (strcmp("work", CurCtx.popln->name))
        return;
    /*	Don't believe cost if fix is random  */
    if (Fix == Random)
        return;
    /*	Compare current and best costs  */
    bstid = get_best_pop();
    if (bstid < 0) {
        printf("Cannot make BST_ model\n");
        return;
    }
    bstpop = Populations[bstid];
    bstroot = bstpop->root;
    bstcst = bstpop->classes[bstroot]->best_cost;
    if (Popln->classes[root]->best_cost >= bstcst)
        return;
    /*	Current looks better, but do a do_all to ensure cost is correct */
    /*	But only bother if 'verify'  */
    if (verify) {
        kk = Control;
        Control = 0;
        do_all(1, 1);
        Control = kk;
        if (Popln->classes[root]->best_cost >= (bstcst))
            return;
    }

    /*	Seems better, so kill old 'bestmodel' and make a new one */
    set_population();
    kk = copy_population(Popln->id, 0, BSTname);
    Smpl->best_cost = Popln->classes[root]->best_cost;
    Smpl->best_time = Popln->classes[root]->age;
    return;
}

/*	------------------  find_population  -----------------------------   */
/*	To find a popln with given name and return its id, or -1 if fail */
int find_population(char *nam)
{
    int i;
    char lname[80];

    strcpy(lname, "BST_");
    if (strcmp(nam, lname))
        strcpy(lname, nam);
    else {
        if (CurCtx.sample)
            strcpy(lname + 4, CurCtx.sample->name);
        else
            return (-1);
    }
    for (i = 0; i < MAX_POPULATIONS; i++) {
        if (Populations[i] && (!strcmp(lname, Populations[i]->name)))
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

int save_population(int p1, int fill, char *newname){
    FILE *fl;
    char oldname[80], *jp;
    Context oldctx;
    Class *cls;
    CVinst *cvi;
    EVinst *evi;
    int leng, nch, i, iv, nc, jcl;

    memcpy(&oldctx, &CurCtx, sizeof(Context));
    fl = 0;
    if (!Populations[p1]) {
        printf("No popln index %d\n", p1);
        leng = -106;
        goto finish;
    }
    /*	Begin by copying the popln to a clean TrialPop   */
    Popln = Populations[p1];
    if (!strcmp(Popln->name, "TrialPop")) {
        printf("Cannot save TrialPop\n");
        leng = -105;
        goto finish;
    }
    /*	Save old name  */
    strcpy(oldname, Popln->name);
    /*	If name begins "BST_", change it to "BSTP" to avoid overwriting
    a perhaps better BST_ model existing at time of restore.  */
    for (i = 0; i < 4; i++)
        if (oldname[i] != "BST_"[i])
            goto namefixed;
    oldname[3] = 'P';
    printf("This model will be saved with name BSTP... not BST_...\n");
namefixed:
    i = find_population("TrialPop");
    if (i >= 0)
        destroy_population(i);
    nc = Popln->sample_size;
    if (!nc)
        fill = 0;
    if (!fill)
        nc = 0;
    i = copy_population(p1, fill, "TrialPop");
    if (i < 0) {
        leng = i;
        goto finish;
    }
    /*	We can now be sure that, in TrialPop, all subs are gone and classes
        have id-s from 0 up, starting with root  */
    /*	switch context to TrialPop  */
    CurCtx.popln = Populations[i];
    set_population();
    VSet = CurCtx.vset = VarSets[find_vset(Popln->vst_name)];
    nc = Popln->sample_size;
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
    for (jp = Popln->vst_name; *jp; jp++) {
        fputc(*jp, fl);
    }
    fputc('\n', fl);
    for (jp = Popln->sample_name; *jp; jp++) {
        fputc(*jp, fl);
    }
    fputc('\n', fl);
    fprintf(fl, "%4d%10d\n", Popln->num_classes, nc);
    fputc('+', fl);
    fputc('\n', fl);

    /*	We must begin copying classes, begining with pop's root  */
    jcl = 0;
    /*	The classes should be lined up in pop->classes  */

newclass:
    cls = Popln->classes[jcl];
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
    if (jcl < Popln->num_classes)
        goto newclass;

finish:
    fclose(fl);
    printf("\nModel %s  Cost %10.2f  saved in file %s\n", oldname,
           Popln->classes[0]->best_cost, newname);
    memcpy(&CurCtx, &oldctx, sizeof(Context));
    set_population();
    return (leng);
}

/*	-----------------  load_population  -------------------------------  */
/*	To read a model saved by savepop  */
int load_population(char *nam){
    char pname[80], name[80], *jp;
    Context oldctx;
    int i, j, k, indx, fncl, fnc, nch, iv;
    Class *cls;
    CVinst *cvi;
    EVinst *evi;
    FILE *fl;

    indx = -999;
    memcpy(&oldctx, &CurCtx, sizeof(Context));
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
    j = find_vset(name);
    if (j < 0) {
        printf("Model needs variableset %s\n", name);
        goto error;
    }
    VSet = CurCtx.vset = VarSets[j];
    nv = VSet->length;
    fscanf(fl, "%s", name);          /* Reading sample name */
    fscanf(fl, "%d%d", &fncl, &fnc); /* num of classes, cases */
                                     /*	Advance to real data */
    while (fgetc(fl) != '+')
        ;
    fgetc(fl);
    if (fnc) {
        j = find_sample(name, 1);
        if (j < 0) {
            printf("Sample %s unknown.\n", name);
            nc = 0;
            Smpl = CurCtx.sample = 0;
        } else {
            Smpl = CurCtx.sample = Samples[j];
            if (Smpl->num_cases != fnc) {
                printf("Size conflict Model%9d vs. Sample%9d\n", fnc, nc);
                goto error;
            }
            nc = fnc;
        }
    } else { /* file model unattached.  */
        Smpl = CurCtx.sample = 0;
        nc = 0;
    }

    /*	Build input model in TrialPop  */
    j = find_population("TrialPop");
    if (j >= 0)
        destroy_population(j);
    indx = make_population(nc);
    if (indx < 0)
        goto error;
    Popln = CurCtx.popln = Populations[indx];
    Popln->sample_size = nc;
    strcpy(Popln->name, "TrialPop");
    if (!nc)
        strcpy(Popln->sample_name, "??");
    set_population();
    Popln->num_leaves = 0;
    /*	Popln has a root class set up. We read into it.  */
    j = 0;
    goto haveclass;

newclass:
    j = make_class();
    if (j < 0) {
        printf("RestoreClass fails in Makeclass\n");
        goto error;
    }
haveclass:
    cls = Popln->classes[j];
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
        Popln->num_leaves++;
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
    if (Popln->num_classes < fncl)
        goto newclass;

    i = find_population(pname);
    if (i >= 0) {
        printf("Overwriting old model %s\n", pname);
        destroy_population(i);
    }
    if (!strcmp(pname, "work"))
        goto pickwork;
    strcpy(Popln->name, pname);
error:
    memcpy(&CurCtx, &oldctx, sizeof(Context));
    goto finish;

pickwork:
    indx = set_work_population(Popln->id);
    j = find_population("Trialpop");
    if (j >= 0)
        destroy_population(j);
finish:
    set_population();
    return (indx);
}

/*	----------------------  loadpop  -----------------------------  */
/*	To load popln pp into work, attaching sample as needed.
    Returns index of work, and sets ctx, or returns neg if fail  */
int set_work_population(int pp){
    int j, windx, fpopnc;
    Context oldctx;
    Population *fpop;

    windx = -1;
    memcpy(&oldctx, &CurCtx, sizeof(Context));
    fpop = 0;
    if (pp < 0)
        goto error;
    fpop = Popln = Populations[pp];
    if (!Popln)
        goto error;
    /*	Save the 'nc' of the popln to be loaded  */
    fpopnc = Popln->sample_size;
    /*	Check popln vset  */
    j = find_vset(Popln->vst_name);
    if (j < 0) {
        printf("Load cannot find variable set\n");
        goto error;
    }
    VSet = CurCtx.vset = VarSets[j];
    /*	Check Vset  */
    if (strcmp(VSet->name, oldctx.sample->vset_name)) {
        printf("Picked popln has incompatible VariableSet\n");
        goto error;
    }
    /*	Check sample   */
    if (fpopnc && strcmp(Popln->sample_name, oldctx.sample->name)) {
        printf("Picked popln attached to non-current sample.\n");
        /*	Make pop appear unattached  */
        Popln->sample_size = 0;
    }

    CurCtx.sample = Smpl = oldctx.sample;
    windx = copy_population(pp, 1, "work");
    if (windx < 0)
        goto error;
    if (Populations[pp]->sample_size)
        goto finish;

    /*	The popln was copied as if unattached, so scores, weights must be
        fixed  */
    printf("Model will have weights, scores adjusted to sample.\n");
    Fix = Partial;
    Control = AdjSc;
fixscores:
    SeeAll = 16;
    do_all(15, 0);
    /*	do_all should leave a count of score changes in global  */
    flp();
    printf("%8d  score changes\n", scorechanges);
    if (Heard) {
        flp();
        printf("Score fixing stopped prematurely\n");
    } else if (scorechanges > 1)
        goto fixscores;
    Fix = DFix;
    Control = DControl;
    goto finish;

error:
    memcpy(&CurCtx, &oldctx, sizeof(Context));
finish:
    /*	Restore 'nc' of copied popln  */
    if (fpop)
        fpop->sample_size = fpopnc;
    set_population();
    if (windx >= 0)
        print_tree();
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

    set_population();
    wpop = Popln;
    xpop = Populations[xid];
    if (!xpop) {
        printf("No such model\n");
        goto finish;
    }
    if (!xpop->sample_size) {
        printf("Model is unattached\n");
        goto finish;
    }
    if (strcmp(Popln->sample_name, xpop->sample_name)) {
        printf("Model not for same sample.\n");
        goto finish;
    }
    if (xpop->sample_size != nc) {
        printf("Models have unequal numbers of cases.\n");
        goto finish;
    }
    /*	Should be able to proceed  */
    fnact = Smpl->num_active;
    Control = AdjSc;
    SeeAll = 4;
    NoSubs++;
    do_all(3, 1);
    wpop = Popln;
    find_all(Leaf);
    wnl = NumSon;
    for (wic = 0; wic < wnl; wic++)
        wsons[wic] = Sons[wic];
    /*	Switch to xpop  */
    CurCtx.popln = xpop;
    set_population();
    SeeAll = 4;
    do_all(3, 1);
    /*	Find all leaves of xpop  */
    find_all(Leaf);
    xnl = NumSon;
    if ((wnl < 2) || (xnl < 2)) {
        printf("Need at least 2 classes in each model.\n");
        goto finish;
    }
    for (xic = 0; xic < xnl; xic++)
        xsons[xic] = Sons[xic];
    /*	Clear table   */
    for (wic = 0; wic < wnl; wic++) {
        for (xic = 0; xic < xnl; xic++)
            table[wic][xic] = 0.0;
    }

    /*	Now accumulate cross products of weights for each active item */
    SeeAll = 2;
    for (n = 0; n < nc; n++) {
        CurCtx.popln = wpop;
        set_population();
        record = recs + n * reclen;
        if (!*record)
            goto ndone;
        find_all(Leaf);
        do_case(n, Leaf, 0);
        CurCtx.popln = xpop;
        set_population();
        find_all(Leaf);
        do_case(n, Leaf, 0);
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
    if (NoSubs > 0)
        NoSubs--;
    CurCtx.popln = wpop;
    set_population();
    Control = DControl;
    return;
}
