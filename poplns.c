#define NOTGLOB 1
#define POPLNS 1
#include "glob.h"
#include <string.h>

/*	-----------------------  setpop  --------------------------    */
void set_population() {
    Population *popln = CurCtx.popln;
    CurSample = CurCtx.sample;
    CurVSet = CurCtx.vset;
    NumVars = CurVSet->length;
    VSetVarList = CurVSet->variables;
    if (CurSample) {
        NumCases = CurSample->num_cases;
        SmplVarList = CurSample->variables;
        RecLen = CurSample->record_length;
        Records = CurSample->records;
    } else {
        NumCases = 0;
        SmplVarList = 0;
        Records = 0;
    }
    PopVarList = popln->variables;
    CurRoot = popln->root;
}

/*	-----------------------  nextclass  ---------------------  */
/*	given a ptr to a ptr to a class in pop, changes the Class* to point
to the next class in a depth-first traversal, or 0 if there is none  */
void next_class(Class **ptr) {
    Class *clp;
    Population *popln = CurCtx.popln;
    clp = *ptr;
    if (clp->son_id >= 0) {
        *ptr = popln->classes[clp->son_id];
        goto done;
    }
loop:
    if (clp->sib_id >= 0) {
        *ptr = popln->classes[clp->sib_id];
        goto done;
    }
    if (clp->dad_id < 0) {
        *ptr = 0;
        goto done;
    }
    clp = popln->classes[clp->dad_id];
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
int make_population(int fill) {
    PopVar *pvars;
    Class *cls;
    Population *popln;
    VSetVar *vset_var;

    int indx, i;

    CurSample = CurCtx.sample;
    CurVSet = CurCtx.vset;
    if (CurSample)
        NumCases = CurSample->num_cases;
    else
        NumCases = 0;
    if ((!CurSample) && fill) {
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
    popln = Populations[indx] = (Population *)malloc(sizeof(Population));
    if (!popln)
        goto nospace;
    popln->id = indx;
    popln->sample_size = 0;
    strcpy(popln->sample_name, "??");
    strcpy(popln->vst_name, CurVSet->name);
    popln->blocks = popln->model_blocks = 0;
    CurCtx.popln = popln;
    for (i = 0; i < 80; i++)
        popln->name[i] = 0;
    strcpy(popln->name, "newpop");
    popln->filename[0] = 0;
    popln->num_classes = 0; /*  Initially no class  */

    /*	Make vector of PVinsts    */
    pvars = popln->variables = (PopVar *)alloc_blocks(1, NumVars * sizeof(PopVar));
    if (!pvars)
        goto nospace;
    /*	Copy from variable-set AVinsts to PVinsts  */
    for (i = 0; i < NumVars; i++) {
        vset_var = VSetVarList + i;
        CurVType = vset_var->vtype;
        CurPopVar = pvars + i;
        CurPopVar->id = vset_var->id;
        CurPopVar->paux = (char *)alloc_blocks(1, CurVType->pop_aux_size);
        if (!CurPopVar->paux)
            goto nospace;
    }

    /*	Make an empty vector of ptrs to class structures, initially
        allowing for MAX_CLASSES classes  */
    popln->classes = (Class **)alloc_blocks(1, MAX_CLASSES * sizeof(Class *));
    if (!popln->classes)
        goto nospace;
    popln->cls_vec_len = MAX_CLASSES;
    for (i = 0; i < popln->cls_vec_len; i++)
        popln->classes[i] = 0;

    if (fill) {
        strcpy(popln->sample_name, CurSample->name);
        popln->sample_size = NumCases;
    }
    CurRoot = popln->root = make_class();
    if (CurRoot < 0)
        goto nospace;
    cls = popln->classes[CurRoot];
    cls->serial = 4;
    popln->next_serial = 2;
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

/*	----------------------  firstpop  ---------------------  */
/*	To make an initial population given a vset and a samp.
    Should return a popln index for a popln with a root class set up
and non-fac estimates done  */
int init_population() {
    int ipop;

    clr_bad_move();
    CurSample = CurCtx.sample;
    CurVSet = CurCtx.vset;
    ipop = -1;
    if (!CurSample) {
        printf("No sample defined for firstpop\n");
        goto finish;
    }
    ipop = make_population(1); /* Makes a configured popln */
    if (ipop < 0)
        goto finish;
    strcpy(CurCtx.popln->name, "work");

    set_population();
    /*	Run doall on the root alone  */
    do_all(5, 0);

finish:
    return (ipop);
}
/*	----------------------  makesubs -----------------------    */
/*	Given a class index kk, makes two subclasses  */

void make_subclasses(int kk) {
    Class *cls, *clsa, *clsb;
    ClassVar *cvi, *scvi;
    double cntk;
    int i, kka, kkb, iv, nch;

    Population *popln = CurCtx.popln;
    if (NoSubs)
        return;
    cls = popln->classes[kk];

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
    adjust_class(cls, 0);
    Control = DControl;
    kka = make_class();
    if (kka < 0) {
        i = 2;
        goto finish;
    }
    clsa = popln->classes[kka];

    kkb = make_class();
    if (kkb < 0) {
        i = 2;
        goto finish;
    }
    clsb = popln->classes[kkb];
    clsa->serial = cls->serial + 1;
    clsb->serial = cls->serial + 2;

    /*	Fix hierarchy links.  */
    cls->num_sons = 2;
    cls->son_id = kka;
    clsa->dad_id = clsb->dad_id = kk;
    clsa->num_sons = clsb->num_sons = 0;
    clsa->sib_id = kkb;
    clsb->sib_id = -1;

    /*	Copy kk's Basics into both subs  */
    for (iv = 0; iv < NumVars; iv++) {
        cvi = cls->basics[iv];
        scvi = clsa->basics[iv];
        nch = VSetVarList[iv].basic_size;
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
void destroy_population(int px) {
    Population *popln;
    int prev;
    if (CurCtx.popln)
        prev = CurCtx.popln->id;
    else
        prev = -1;
    popln = Populations[px];
    if (!popln)
        return;

    CurCtx.popln = popln;
    set_population();
    free_blocks(2);
    free_blocks(1);
    free(popln);
    Populations[px] = 0;
    CurCtx.popln = 0;
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
sample shown in ctx, if compatible, and given small, silly factor scores.
    If <newname> exists, it is discarded.
    If fill = 0, only basic and stats info is copied.
    Returns index of new popln.
    If newname is "work", the context ctx is left addressing the new
popln, but otherwise is not disturbed.
    */
int copy_population(int p1, int fill, char *newname) {
    Population *fpop, *popln;
    Context oldctx;
    Class *cls, *fcls;
    ClassVar *cvi, *fcvi;
    ExplnVar *evi, *fevi;
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
    CurVSet = CurCtx.vset = VarSets[kk];
    sindx = -1;
    NumCases = 0;
    if (!fill)
        goto sampfound;
    if (fpop->sample_size) {
        sindx = find_sample(fpop->sample_name, 1);
        goto sampfound;
    }
    /*	Use current sample if compatible  */
    CurSample = CurCtx.sample;
    if (CurSample && (!strcmp(CurSample->vset_name, CurVSet->name))) {
        sindx = CurSample->id;
        goto sampfound;
    }
    /*	Fill is indicated but no sample is defined.  */
    printf("There is no defined sample to fill for.\n");
    indx = -103;
    goto finish;

sampfound:
    if (sindx >= 0) {
        CurSample = CurCtx.sample = Samples[sindx];
        NumCases = CurSample->num_cases;
    } else {
        CurSample = CurCtx.sample = 0;
        fill = NumCases = 0;
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

    popln = CurCtx.popln = Populations[indx];
    set_population();
    if (CurSample) {
        strcpy(popln->sample_name, CurSample->name);
        popln->sample_size = CurSample->num_cases;
    } else {
        strcpy(popln->sample_name, "??");
        popln->sample_size = 0;
    }
    strcpy(popln->name, newname);
    hiser = 0;

    /*	We must begin copying classes, begining with fpop's root  */
    fcls = fpop->classes[CurRoot];
    jdad = -1;
    /*	In case the pop is unattached (fpop->nc = 0), prepare a nominal
    count to insert in leaf classes  */
    if (fill & (!fpop->sample_size)) {
        nomcnt = CurSample->num_active;
        nomcnt = nomcnt / fpop->num_leaves;
    }

newclass:
    if (fcls->dad_id < 0)
        kk = popln->root;
    else
        kk = make_class();
    if (kk < 0) {
        indx = -105;
        goto finish;
    }
    cls = popln->classes[kk];
    nch = ((char *)&cls->id) - ((char *)cls);
    memcpy(cls, fcls, nch);
    cls->sib_id = cls->son_id = -1;
    cls->dad_id = jdad;
    cls->num_sons = 0;
    if (cls->serial > hiser)
        hiser = cls->serial;

    /*	Copy Basics. the structures should have been made.  */
    for (iv = 0; iv < NumVars; iv++) {
        fcvi = fcls->basics[iv];
        cvi = cls->basics[iv];
        nch = VSetVarList[iv].basic_size;
        memcpy(cvi, fcvi, nch);
    }

    /*	Copy stats  */
    for (iv = 0; iv < NumVars; iv++) {
        fevi = fcls->stats[iv];
        evi = cls->stats[iv];
        nch = VSetVarList[iv].stats_size;
        memcpy(evi, fevi, nch);
    }
    if (fill == 0)
        goto classdone;

    /*	If fpop is not attached, we cannot copy item-specific data */
    if (fpop->sample_size == 0)
        goto fakeit;
    /*	Copy scores  */
    for (n = 0; n < NumCases; n++)
        cls->factor_scores[n] = fcls->factor_scores[n];
    goto classdone;

fakeit: /*  initialize scorevectors  */
    for (n = 0; n < NumCases; n++) {
        CurRecord = Records + n * RecLen;
        cls->factor_scores[n] = 0;
    }
    cls->weights_sum = nomcnt;
    if (cls->dad_id < 0) /* Root class */
        cls->weights_sum = CurSample->num_active;

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
    cls = popln->classes[cls->dad_id];
    jdad = cls->dad_id;
    fcls = fpop->classes[fcls->dad_id];
    goto siborback;

alldone: /*  All classes copied. Tidy up */
    tidy(0);
    popln->next_serial = (hiser >> 2) + 1;

finish:
    /*	Unless newname = "work", revert to old context  */
    if (strcmp(newname, "work") || (indx < 0))
        memcpy(&CurCtx, &oldctx, sizeof(Context));
    set_population();
    return (indx);
}

/*	----------------------  printsubtree  ----------------------   */
static int pdeep;
void print_subtree(int kk) {
    Class *son, *cls;
    int kks;
    Population *popln = CurCtx.popln;

    for (kks = 0; kks < pdeep; kks++)
        printf("   ");
    cls = popln->classes[kk];
    printf("%s", serial_to_str(cls));
    if (cls->type == Dad)
        printf(" Dad");
    if (cls->type == Leaf)
        printf("Leaf");
    if (cls->type == Sub)
        printf(" Sub");
    if (cls->hold_type)
        printf(" H");
    else
        printf("  ");
    if (cls->use == Fac)
        printf(" Fac");
    if (cls->use == Tiny)
        printf(" Tny");
    if (cls->use == Plain)
        printf("    ");
    printf(" Scan%8d", cls->scancnt);
    printf(" RelAb%6.3f  Size%8.1f\n", cls->relab, cls->newcnt);

    pdeep++;
    for (kks = cls->son_id; kks >= 0; kks = son->sib_id) {
        son = popln->classes[kks];
        print_subtree(kks);
    }
    pdeep--;
    return;
}

/*	---------------------  printtree  ---------------------------  */
void print_tree() {

    Population *popln = CurCtx.popln;
    if (!popln) {
        printf("No current population.\n");
        return;
    }
    set_population();
    printf("Popln%3d on sample%3d,%4d classes,%6d things", popln->id + 1, CurSample->id + 1, popln->num_classes, CurSample->num_cases);
    printf("  Cost %10.2f\n", popln->classes[popln->root]->best_cost);
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
    print_subtree(CurRoot);
    return;
}

/*	--------------------  bestpopid  -----------------------------  */
/*	Finds the popln whose name is "BST_" followed by the name of
ctx.samp.  If no such popln exists, makes one by copying ctx.popln.
Returns id of popln, or -1 if failure  */

/*	The char BSTname[] is used to pass the popln name to trackbest */
char BSTname[80];

int get_best_pop() {
    int i;

    i = -1;
    if ((CurCtx.popln) && (CurCtx.sample)) {

        set_population();

        strcpy(BSTname, "BST_");
        strcpy(BSTname + 4, CurSample->name);
        i = find_population(BSTname);
        if (i < 0) {
            /*	No such popln exists.  Make one using copypop  */
            i = copy_population(CurCtx.popln->id, 0, BSTname);
        }
        if (i < 0) {
            /*	Set bestcost in samp from current cost  */
            CurSample->best_cost = CurCtx.popln->classes[CurRoot]->best_cost;
            /*	Set goodtime as current pop age  */
            CurSample->best_time = CurCtx.popln->classes[CurRoot]->age;
        }
    }
    return (i);
}

/*	-----------------------  trackbest  ----------------------------  */
/*	If current model is better than 'BST_...', updates BST_... */
void track_best(int verify) {
    Population *bstpop;
    int bstid, bstroot, kk;
    double bstcst;
    Population *popln = CurCtx.popln;

    if ((!CurCtx.popln) || (strcmp("work", CurCtx.popln->name)) || (Fix == Random)) {
        /*	Check current popln is 'work'  */
        /*	Don't believe cost if fix is random  */
        return;
    }
        
    /*	Compare current and best costs  */
    bstid = get_best_pop();
    if (bstid < 0) {
        printf("Cannot make BST_ model\n");
        return;
    }
    bstpop = Populations[bstid];
    bstroot = bstpop->root;
    bstcst = bstpop->classes[bstroot]->best_cost;
    if (popln->classes[CurRoot]->best_cost >= bstcst)
        return;
    /*	Current looks better, but do a doall to ensure cost is correct */
    /*	But only bother if 'verify'  */
    if (verify) {
        kk = Control;
        Control = 0;
        do_all(1, 1);
        Control = kk;
        if (popln->classes[CurRoot]->best_cost >= (bstcst))
            return;
    }

    /*	Seems better, so kill old 'bestmodel' and make a new one */
    set_population();
    kk = copy_population(popln->id, 0, BSTname);
    CurSample->best_cost = popln->classes[CurRoot]->best_cost;
    CurSample->best_time = popln->classes[CurRoot]->age;
    return;
}

/*	------------------  pname2id  -----------------------------   */
/*	To find a popln with given name and return its id, or -1 if fail */
int find_population(char *nam) {
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
    char *buffer = (char *)from;

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

int save_population(int p1, int fill, char *newname) {
    FILE *fl;
    char oldname[80], *jp;
    Context oldctx;
    Class *cls;
    ClassVar *cvi;
    ExplnVar *evi;
    int leng, nch, i, iv, nc, jcl;
    Population *popln;

    memcpy(&oldctx, &CurCtx, sizeof(Context));
    fl = 0;
    if (!Populations[p1]) {
        printf("No popln index %d\n", p1);
        leng = -106;
        goto finish;
    }
    /*	Begin by copying the popln to a clean TrialPop   */
    popln = Populations[p1];
    if (!strcmp(popln->name, "TrialPop")) {
        printf("Cannot save TrialPop\n");
        leng = -105;
        goto finish;
    }
    /*	Save old name  */
    strcpy(oldname, popln->name);
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
    nc = popln->sample_size;
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
    CurVSet = CurCtx.vset = VarSets[find_vset(popln->vst_name)];
    nc = popln->sample_size;
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
    for (jp = popln->vst_name; *jp; jp++) {
        fputc(*jp, fl);
    }
    fputc('\n', fl);
    for (jp = popln->sample_name; *jp; jp++) {
        fputc(*jp, fl);
    }
    fputc('\n', fl);
    fprintf(fl, "%4d%10d\n", popln->num_classes, nc);
    fputc('+', fl);
    fputc('\n', fl);

    /*	We must begin copying classes, begining with pop's root  */
    jcl = 0;
    /*	The classes should be lined up in pop->classes  */

newclass:
    cls = popln->classes[jcl];
    leng = 0;
    nch = ((char *)&cls->id) - ((char *)cls);
    recordit(fl, cls, nch);
    leng += nch;

    /*	Copy Basics..  */
    for (iv = 0; iv < NumVars; iv++) {
        cvi = cls->basics[iv];
        nch = VSetVarList[iv].basic_size;
        recordit(fl, cvi, nch);
        leng += nch;
    }

    /*	Copy stats  */
    for (iv = 0; iv < NumVars; iv++) {
        evi = cls->stats[iv];
        nch = VSetVarList[iv].stats_size;
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
    if (jcl < popln->num_classes)
        goto newclass;

finish:
    fclose(fl);
    printf("\nModel %s  Cost %10.2f  saved in file %s\n", oldname, popln->classes[0]->best_cost, newname);
    memcpy(&CurCtx, &oldctx, sizeof(Context));
    set_population();
    return (leng);
}

/*	-----------------  restorepop  -------------------------------  */
/*	To read a model saved by savepop  */
int load_population(char *nam) {
    char pname[80], name[80], *jp;
    Context oldctx;
    int i, j, k, indx, fncl, fnc, nch, iv;
    Class *cls;
    ClassVar *cvi;
    ExplnVar *evi;
    FILE *fl;
    Population *popln;

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
    CurVSet = CurCtx.vset = VarSets[j];
    NumVars = CurVSet->length;
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
            NumCases = 0;
            CurSample = CurCtx.sample = 0;
        } else {
            CurSample = CurCtx.sample = Samples[j];
            if (CurSample->num_cases != fnc) {
                printf("Size conflict Model%9d vs. Sample%9d\n", fnc, NumCases);
                goto error;
            }
            NumCases = fnc;
        }
    } else { /* file model unattached.  */
        CurSample = CurCtx.sample = 0;
        NumCases = 0;
    }

    /*	Build input model in TrialPop  */
    j = find_population("TrialPop");
    if (j >= 0)
        destroy_population(j);
    indx = make_population(NumCases);
    if (indx < 0)
        goto error;
    popln = CurCtx.popln = Populations[indx];
    popln->sample_size = NumCases;
    strcpy(popln->name, "TrialPop");
    if (!NumCases)
        strcpy(popln->sample_name, "??");
    set_population();
    popln->num_leaves = 0;
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
    cls = popln->classes[j];
    jp = (char *)cls;
    nch = ((char *)&cls->id) - jp;
    for (k = 0; k < nch; k++) {
        *jp = fgetc(fl);
        jp++;
    }
    for (iv = 0; iv < NumVars; iv++) {
        cvi = cls->basics[iv];
        nch = VSetVarList[iv].basic_size;
        jp = (char *)cvi;
        for (k = 0; k < nch; k++) {
            *jp = fgetc(fl);
            jp++;
        }
    }
    for (iv = 0; iv < NumVars; iv++) {
        evi = cls->stats[iv];
        nch = VSetVarList[iv].stats_size;
        jp = (char *)evi;
        for (k = 0; k < nch; k++) {
            *jp = fgetc(fl);
            jp++;
        }
    }
    if (cls->type == Leaf)
        popln->num_leaves++;
    if (!fnc)
        goto classdone;

    /*	Read scores but throw away if nc = 0  */
    if (!NumCases) {
        nch = fnc * sizeof(short);
        for (k = 0; k < nch; k++)
            fgetc(fl);
        goto classdone;
    }
    nch = NumCases * sizeof(short);
    jp = (char *)(cls->factor_scores);
    for (k = 0; k < nch; k++) {
        *jp = fgetc(fl);
        jp++;
    }

classdone:
    if (popln->num_classes < fncl)
        goto newclass;

    i = find_population(pname);
    if (i >= 0) {
        printf("Overwriting old model %s\n", pname);
        destroy_population(i);
    }
    if (!strcmp(pname, "work"))
        goto pickwork;
    strcpy(popln->name, pname);
error:
    memcpy(&CurCtx, &oldctx, sizeof(Context));
    goto finish;

pickwork:
    indx = set_work_population(popln->id);
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
int set_work_population(int pp) {
    int j, windx, fpopnc;
    Context oldctx;
    Population *fpop, *popln;

    windx = -1;
    memcpy(&oldctx, &CurCtx, sizeof(Context));
    fpop = 0;
    if (pp < 0)
        goto error;
    fpop = popln = Populations[pp];
    if (!popln)
        goto error;
    /*	Save the 'nc' of the popln to be loaded  */
    fpopnc = popln->sample_size;
    /*	Check popln vset  */
    j = find_vset(popln->vst_name);
    if (j < 0) {
        printf("Load cannot find variable set\n");
        goto error;
    }
    CurVSet = CurCtx.vset = VarSets[j];
    /*	Check VarSet  */
    if (strcmp(CurVSet->name, oldctx.sample->vset_name)) {
        printf("Picked popln has incompatible VariableSet\n");
        goto error;
    }
    /*	Check sample   */
    if (fpopnc && strcmp(popln->sample_name, oldctx.sample->name)) {
        printf("Picked popln attached to non-current sample.\n");
        /*	Make pop appear unattached  */
        popln->sample_size = 0;
    }

    CurCtx.sample = CurSample = oldctx.sample;
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
    /*	doall should leave a count of score changes in global  */
    flp();
    printf("%8d  score changes\n", ScoreChanges);
    if (Heard) {
        flp();
        printf("Score fixing stopped prematurely\n");
    } else if (ScoreChanges > 1)
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
void correlpops(int xid) {
    float table[MAX_CLASSES][MAX_CLASSES];
    Class *wsons[MAX_CLASSES], *xsons[MAX_CLASSES];
    Population *xpop, *wpop;
    double fnact, wwt;
    int wic, xic, n, pcw, wnl, xnl, num_son;
    Population *popln = CurCtx.popln;

    set_population();
    wpop = popln;
    xpop = Populations[xid];
    if (!xpop) {
        printf("No such model\n");
        goto finish;
    }
    if (!xpop->sample_size) {
        printf("Model is unattached\n");
        goto finish;
    }
    if (strcmp(popln->sample_name, xpop->sample_name)) {
        printf("Model not for same sample.\n");
        goto finish;
    }
    if (xpop->sample_size != NumCases) {
        printf("Models have unequal numbers of cases.\n");
        goto finish;
    }
    /*	Should be able to proceed  */
    fnact = CurSample->num_active;
    Control = AdjSc;
    SeeAll = 4;
    NoSubs++;
    do_all(3, 1);
    wpop = popln;
    wnl = find_all(Leaf);
    for (wic = 0; wic < wnl; wic++)
        wsons[wic] = Sons[wic];
    /*	Switch to xpop  */
    CurCtx.popln = xpop;
    set_population();
    SeeAll = 4;
    do_all(3, 1);
    /*	Find all leaves of xpop  */
    xnl = find_all(Leaf);
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
    for (n = 0; n < NumCases; n++) {
        CurCtx.popln = wpop;
        set_population();
        CurRecord = Records + n * RecLen;
        if (!*CurRecord)
            goto ndone;
        num_son = find_all(Leaf);
        do_case(n, Leaf, 0, num_son);
        CurCtx.popln = xpop;
        set_population();
        num_son = find_all(Leaf);
        do_case(n, Leaf, 0, num_son);
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
