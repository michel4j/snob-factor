
#define NOTGLOB 1
#define POPLNS 1
#include "snob.h"
#include <string.h>

/**
 * @brief given a ptr to a ptr to a class in pop, changes the Class* to point
 * to the next class in a depth-first traversal, or 0 if there is none
 * @param ctx Pointer to the Snob context.
 * @param ptr
 */
void next_class(SnobContext *ctx, Class **ptr) {
    Class *cls;
    Population *popln = ctx->state.popln;
    cls = *ptr;
    if (cls->son_id >= 0) {
        *ptr = popln->classes[cls->son_id];
        return;
    }
    while (1) {
        if (cls->sib_id >= 0) {
            *ptr = popln->classes[cls->sib_id];
            break;
        }
        if (cls->dad_id < 0) {
            *ptr = 0;
            break;
        }
        cls = popln->classes[cls->dad_id];
    }
}

/**
 * @brief To make a population with the parameter set 'vst'
 * Returns index of new popln in poplns array
 * The popln is given a root class appropriate for the variable-set.
 * If fill = 0, root has only basics and stats, but no weight, cost or
 * or score vectors, and the popln is not connected to the current sample.
 * OTHERWIZE, the root class is fully configured for the current sample.
 *
 * @param ctx Pointer to the Snob context.
 * @param fill
 */

int make_population(SnobContext *ctx, int fill) {
    PopVar *pop_var;
    Class *cls;
    Population *popln;
    VSetVar *vset_var;
    VarType *vtype;
    int indx, i, found = -1;
    int num_cases = (ctx->state.sample) ? ctx->state.sample->num_cases : 0;

    if ((!ctx->state.sample) && fill) {
        return error_value(ctx, "Makepop cannot fill because no sample defined", -1);
    }
    //	Find vacant popln slot
    for (indx = 0; indx < MAX_POPULATIONS; indx++) {
        if (ctx->populations[indx] == 0) {
            found = indx;
            break;
        }
    }

    if (found < 0) {
        return error_value(ctx, "No space for another population", -1);
    }

    popln = ctx->populations[found] = (Population *)malloc(sizeof(Population));
    if (!popln) {
        return error_value(ctx, "No space for another population", -1);
    }

    popln->id = found;
    popln->sample_size = 0;
    strcpy(popln->sample_name, "??");
    strcpy(popln->vst_name, ctx->state.vset->name);
    popln->blocks = popln->model_blocks = 0;
    ctx->state.popln = popln;
    for (i = 0; i < 80; i++) {
        popln->name[i] = 0;
    }
    strcpy(popln->name, "newpop");
    popln->filename[0] = 0;
    popln->num_classes = 0; //  Initially no class

    //	Make vector of PVinsts
    popln->variables = (PopVar *)alloc_blocks(ctx, 1, ctx->state.vset->length * sizeof(PopVar));

    if (!popln->variables) {
        return error_value(ctx, "No space for another population", -1);
    }

    //	Copy from variable-set AVinsts to PVinsts
    for (i = 0; i < ctx->state.vset->length; i++) {
        vset_var = &ctx->state.vset->variables[i];
        vtype = vset_var->vtype;
        pop_var = &popln->variables[i];
        pop_var->id = vset_var->id;
        pop_var->paux = (char *)alloc_blocks(ctx, 1, vtype->pop_aux_size);
        if (!pop_var->paux) {
            return error_value(ctx, "No space for another population", -1);
        }
    }

    /*	Make an empty vector of ptrs to class structures, initially
        allowing for MAX_CLASSES classes  */
    popln->classes = (Class **)alloc_blocks(ctx, 1, MAX_CLASSES * sizeof(Class *));
    if (!popln->classes) {
        return error_value(ctx, "No space for another population", -1);
    }

    popln->cls_vec_len = MAX_CLASSES;
    for (i = 0; i < popln->cls_vec_len; i++)
        popln->classes[i] = 0;

    if (fill) {
        strcpy(popln->sample_name, ctx->state.sample->name);
        popln->sample_size = num_cases;
    }

    popln->root = make_class(ctx);
    if (popln->root < 0) {
        return error_value(ctx, "No space for another population", -1);
    }
    cls = popln->classes[popln->root];
    cls->serial = 4;
    popln->next_serial = 2;
    cls->dad_id = -2; // root has no dad
    cls->relab = 1.0;
    //      Set type as leaf, no fac
    cls->type = Leaf;
    cls->use = Plain;
    cls->weights_sum = 0.0;
    return (found);
}

/**
 * @brief To make an initial population given a vset and a samp.
 * Should return a popln index for a popln with a root class set up
 * and non-fac estimates done
 * @param ctx Pointer to the Snob context.
 */
int init_population(SnobContext *ctx) {
    int ipop;
    clr_bad_move(ctx);

    ipop = -1;
    if (!ctx->state.sample) {
        return error_value(ctx, "No sample defined for firstpop", ipop);
    }

    ipop = make_population(ctx, 1); // Makes a configured popln
    if (ipop < 0) {
        return ipop;
    }

    strcpy(ctx->state.popln->name, "work");
    do_all(ctx, 5, 0); //	Run doall on the root alone

    return (ipop);
}
/**
 * @brief Given a class index kk, makes two subclasses
 * @param clsa
 * @param clsb
 */
void cleanup_subclasses(Class *clsa, Class *clsb) {
    if (clsa) {
        clsa->type = Vacant;
        clsa->dad_id = Dead;
    }
    if (clsb) {
        clsb->type = Vacant;
        clsb->dad_id = Dead;
    }
}

void make_subclasses(SnobContext *ctx, int kk) {
    Class *cls, *clsa, *clsb;
    ClassVar *cls_var, *clsa_var;
    double cntk;
    int kka, kkb, iv, nch;
    Population *popln = ctx->state.popln;
    if (ctx->no_subs)
        return;
    cls = popln->classes[kk];

    clsa = clsb = 0;
    //	Check that class has no subs
    if (cls->num_sons > 0) {
        cleanup_subclasses(clsa, clsb);
        return;
    }
    //	And that it is big enough to support subs
    cntk = cls->weights_sum;
    if (cntk < (2 * ctx->min_size + 2.0)) {
        cleanup_subclasses(clsa, clsb);
        return;
    }
    //	Ensure clp's as-dad params set to plain values
    ctx->control = 0;
    adjust_class(ctx, cls, 0);
    ctx->control = ctx->d_control;
    kka = make_class(ctx);
    if (kka < 0) {
        cleanup_subclasses(clsa, clsb);
        return;
    }
    clsa = popln->classes[kka];

    kkb = make_class(ctx);
    if (kkb < 0) {
        cleanup_subclasses(clsa, clsb);
        return;
    }
    clsb = popln->classes[kkb];
    clsa->serial = cls->serial + 1;
    clsb->serial = cls->serial + 2;

    //	ctx->fix hierarchy links.
    cls->num_sons = 2;
    cls->son_id = kka;
    clsa->dad_id = clsb->dad_id = kk;
    clsa->num_sons = clsb->num_sons = 0;
    clsa->sib_id = kkb;
    clsb->sib_id = -1;

    //	Copy kk's Basics into both subs
    for (iv = 0; iv < ctx->state.vset->length; iv++) {
        cls_var = cls->basics[iv];
        clsa_var = clsa->basics[iv];
        nch = ctx->state.vset->variables[iv].basic_size;
        memcpy(clsa_var, cls_var, nch);
        clsa_var = clsb->basics[iv];
        memcpy(clsa_var, cls_var, nch);
    }

    clsa->age = 0;
    clsb->age = 0;
    clsa->type = clsb->type = Sub;
    clsa->use = clsb->use = Plain;
    clsa->relab = clsb->relab = 0.5 * cls->relab;
    clsa->mlogab = clsb->mlogab = -log(clsa->relab);
    clsa->weights_sum = clsb->weights_sum = 0.5 * cls->weights_sum;
}

/**
 * @brief To destroy popln index px
 * @param ctx Pointer to the Snob context.
 * @param px
 */
void destroy_population(SnobContext *ctx, int px) {
    int prev = (ctx->state.popln) ? ctx->state.popln->id : -1;
    Population *popln = ctx->populations[px];

    if (popln) {
        ctx->state.popln = popln;
        free_blocks(ctx, 2);
        free_blocks(ctx, 1);
        free(popln);
        ctx->populations[px] = 0;
        ctx->state.popln = 0;
        if ((px != prev) && (prev >= 0)) {
            ctx->state.popln = ctx->populations[prev];
        }
    }
}

/**
 * @brief Copies poplns[p1] into a popln called <newname>.  If no
 * current popln has this name, it makes one.
 * The new popln has the variable-set of p1, and if fill, is
 * attached to the sample given in p1's samplename, if p1 is attached.
 * If fill but p1 is not attached, the new popln is attached to the current
 * sample shown in ctx, if compatible, and given small, silly factor scores.
 * If <newname> exists, it is discarded.
 * If fill = 0, only basic and stats info is copied.
 * Returns index of new popln.
 * If newname is "work", the context ctx is left addressing the new
 * popln, but otherwise is not disturbed.
 *
 * @param ctx Pointer to the Snob context.
 * @param p1
 * @param fill
 * @param newname Name or filename string.
 */
int copy_population(SnobContext *ctx, int p1, int fill, const char *newname) {
    Population *fpop, *popln;
    State oldctx;
    Class *cls, *fcls;
    ClassVar *cls_var, *fcls_var;
    ExplnVar *exp_var, *fexp_var;
    double nomcnt;
    int indx, sindx, kk, jdad, nch, n, i, iv, hiser;
    int num_cases;

    nomcnt = 0.0;
    memcpy(&oldctx, &ctx->state, sizeof(State));
    indx = -1;
    fpop = ctx->populations[p1];
    if (!fpop) {
        log_msg(ctx, 1, "No popln index %d", p1);
        indx = -106;
        goto finish;
    }
    kk = find_vset(ctx, fpop->vst_name);
    if (kk < 0) {
        log_msg(ctx, 1, "No Variable-set %s", fpop->vst_name);
        indx = -101;
        goto finish;
    }
    ctx->state.vset = ctx->var_sets[kk];
    sindx = -1;
    num_cases = 0;
    if (!fill)
        goto sampfound;
    if (fpop->sample_size) {
        sindx = find_sample(ctx, fpop->sample_name, 1);
        goto sampfound;
    }
    //	Use current sample if compatible
    if (ctx->state.sample && (!strcmp(ctx->state.sample->vset_name, ctx->state.vset->name))) {
        sindx = ctx->state.sample->id;
        goto sampfound;
    }
    //	Fill is indicated but no sample is defined.
    log_msg(ctx, 1, "There is no defined sample to fill for.");
    indx = -103;
    goto finish;

sampfound:
    if (sindx >= 0) {
        ctx->state.sample = ctx->samples[sindx];
        num_cases = ctx->state.sample->num_cases;
    } else {
        ctx->state.sample = 0;
        fill = num_cases = 0;
    }

    //	See if destn popln already exists
    i = find_population(ctx, newname);
    //	Check for copying into self
    if (i == p1) {
        log_msg(ctx, 1, "From copypop: attempt to copy model%3d into itself", p1 + 1);
        indx = -102;
        goto finish;
    }
    if (i >= 0)
        destroy_population(ctx, i);
    //	Make a new popln
    indx = make_population(ctx, fill);
    if (indx < 0) {
        log_msg(ctx, 1, "Cant make new popln from%4d", p1 + 1);
        indx = -104;
        goto finish;
    }

    popln = ctx->state.popln = ctx->populations[indx];
    if (ctx->state.sample) {
        strcpy(popln->sample_name, ctx->state.sample->name);
        popln->sample_size = ctx->state.sample->num_cases;
    } else {
        strcpy(popln->sample_name, "??");
        popln->sample_size = 0;
    }
    strcpy(popln->name, newname);
    hiser = 0;

    //	We must begin copying classes, begining with fpop's root
    fcls = fpop->classes[popln->root];
    jdad = -1;
    /*	In case the pop is unattached (fpop->nc = 0), prepare a nominal
    count to insert in leaf classes  */
    if (fill & (!fpop->sample_size)) {
        nomcnt = ctx->state.sample->num_active;
        nomcnt = nomcnt / fpop->num_leaves;
    }

newclass:
    if (fcls->dad_id < 0)
        kk = popln->root;
    else
        kk = make_class(ctx);
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

    //	Copy Basics. the structures should have been made.
    for (iv = 0; iv < ctx->state.vset->length; iv++) {
        fcls_var = fcls->basics[iv];
        cls_var = cls->basics[iv];
        nch = ctx->state.vset->variables[iv].basic_size;
        memcpy(cls_var, fcls_var, nch);
    }

    //	Copy stats
    for (iv = 0; iv < ctx->state.vset->length; iv++) {
        fexp_var = fcls->stats[iv];
        exp_var = cls->stats[iv];
        nch = ctx->state.vset->variables[iv].stats_size;
        memcpy(exp_var, fexp_var, nch);
    }
    if (fill == 0)
        goto classdone;

    //	If fpop is not attached, we cannot copy item-specific data
    if (fpop->sample_size == 0)
        goto fakeit;
    //	Copy scores
    for (n = 0; n < num_cases; n++)
        cls->factor_scores[n] = fcls->factor_scores[n];
    goto classdone;

fakeit: //  initialize scorevectors
    for (n = 0; n < num_cases; n++) {
        // record = Records + n * RecLen;
        cls->factor_scores[n] = 0;
    }
    cls->weights_sum = nomcnt;
    if (cls->dad_id < 0) // Root class
        cls->weights_sum = ctx->state.sample->num_active;

classdone:
    if (fill)
        set_best_costs(ctx, cls);
    //	Move on to a son class, if any, else a sib class, else back up
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

alldone: //  All classes copied. Tidy up
    tidy(ctx, 0, ctx->no_subs);
    popln->next_serial = (hiser >> 2) + 1;

finish:
    //	Unless newname = "work", revert to old context
    if (strcmp(newname, "work") || (indx < 0))
        memcpy(&ctx->state, &oldctx, sizeof(State));
    return (indx);
}

/**
 */
static int pdeep;
void print_subtree(SnobContext *ctx, int kk) {
    Class *son, *cls;
    int kks;
    Population *popln = ctx->state.popln;

    for (kks = 0; kks < pdeep; kks++)
        printf("   ");
    cls = popln->classes[kk];
    printf("%s", serial_to_str(ctx, cls));
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
        print_subtree(ctx, kks);
    }
    pdeep--;
    return;
}

/**
 * @param ctx Pointer to the Snob context.
 */
void print_tree(SnobContext *ctx) {

    Population *popln = ctx->state.popln;
    if (!popln) {
        printf("No current population.\n");
        return;
    }

    printf("Popln%3d on sample%3d,%4d classes,%6d things", popln->id + 1, ctx->state.sample->id + 1, popln->num_classes,
           ctx->state.sample->num_cases);
    printf("  Cost %10.2f\n", popln->classes[popln->root]->best_cost);
    printf("\n  Assign mode ");
    if (ctx->fix == Partial)
        printf("Partial    ");
    if (ctx->fix == Most_likely)
        printf("Most_likely");
    if (ctx->fix == Random)
        printf("Random     ");
    printf("--- Adjust:");
    if (ctx->control & AdjPr)
        printf(" Params");
    if (ctx->control & AdjSc)
        printf(" ctx->scores");
    if (ctx->control & AdjTr)
        printf(" Tree");
    printf("\n");
    pdeep = 0;
    print_subtree(ctx, popln->root);
    return;
}

/**
 * @brief Finds the popln whose name is "BST_" followed by the name of
 * ctx.samp.  If no such popln exists, makes one by copying ctx.popln.
 * Returns id of popln, or -1 if failure
 */

//	The char BSTname[] is used to pass the popln name to trackbest
char BSTname[80];

int get_best_pop(SnobContext *ctx) {
    int i;

    i = -1;
    if ((ctx->state.popln) && (ctx->state.sample)) {

        strcpy(BSTname, "BST_");
        strcpy(BSTname + 4, ctx->state.sample->name);
        i = find_population(ctx, BSTname);
        if (i < 0) {
            //	No such popln exists.  Make one using copypop
            i = copy_population(ctx, ctx->state.popln->id, 0, BSTname);
        }
        if (i < 0) {
            //	Set bestcost in samp from current cost
            ctx->state.sample->best_cost = ctx->state.popln->classes[ctx->state.popln->root]->best_cost;
            //	Set goodtime as current pop age
            ctx->state.sample->best_time = ctx->state.popln->classes[ctx->state.popln->root]->age;
        }
    }
    return (i);
}

/**
 * @brief If current model is better than 'BST_...', updates BST_...
 * @param ctx Pointer to the Snob context.
 * @param verify
 */
void track_best(SnobContext *ctx, int verify) {
    Population *bstpop;
    int bstid, bstroot, kk;
    double bstcst;
    Population *popln = ctx->state.popln;

    if ((!ctx->state.popln) || (strcmp("work", ctx->state.popln->name)) || (ctx->fix == Random)) {
        //	Check current popln is 'work'
        //	Don't believe cost if fix is random
        return;
    }

    //	Compare current and best costs
    bstid = get_best_pop(ctx);
    if (bstid < 0) {
        log_msg(ctx, 1, "Cannot make BST_ model");
        return;
    }
    bstpop = ctx->populations[bstid];
    bstroot = bstpop->root;
    bstcst = bstpop->classes[bstroot]->best_cost;
    if (popln->classes[popln->root]->best_cost >= bstcst)
        return;
    //	Current looks better, but do a doall to ensure cost is correct
    //	But only bother if 'verify'
    if (verify) {
        kk = ctx->control;
        ctx->control = 0;
        do_all(ctx, 1, 1);
        ctx->control = kk;
        if (popln->classes[popln->root]->best_cost >= (bstcst))
            return;
    }

    //	Seems better, so kill old 'bestmodel' and make a new one
    kk = copy_population(ctx, popln->id, 0, BSTname);
    ctx->state.sample->best_cost = popln->classes[popln->root]->best_cost;
    ctx->state.sample->best_time = popln->classes[popln->root]->age;
    return;
}

/**
 * @brief To find a popln with given name and return its id, or -1 if fail
 * @param ctx Pointer to the Snob context.
 * @param nam Name or filename string.
 */
int find_population(SnobContext *ctx, const char *nam) {
    int i;
    char lname[80];

    strcpy(lname, "BST_");
    if (strcmp(nam, lname))
        strcpy(lname, nam);
    else {
        if (ctx->state.sample)
            strcpy(lname + 4, ctx->state.sample->name);
        else
            return (-1);
    }
    for (i = 0; i < MAX_POPULATIONS; i++) {
        if (ctx->populations[i] && (!strcmp(lname, ctx->populations[i]->name)))
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

char *const saveheading = "Scnob-Model-Save-File";
/**
 * @brief Copies poplns[p1] into a file called <newname>.
 *
 * If fill, item weights and scores are recorded. If not, just
 * Basics and Stats. If fill but pop->nc = 0, behaves as for fill = 0.
 * Returns length of recorded file.
 *
 * @param ctx Pointer to the Snob context.
 * @param p1
 * @param fill
 * @param newname Name or filename string.
 */
int save_population(SnobContext *ctx, int p1, int fill, const char *newname) {

    FILE *file_ptr;
    char oldname[80], *jp;
    State oldctx;
    Class *cls;
    ClassVar *cls_var;
    ExplnVar *exp_var;
    int leng = 0, nch, i, iv, sample_size, jcl;
    Population *popln = 0;

    file_ptr = 0;
    if (!ctx->populations[p1]) {
        log_msg(ctx, 1, "No popln index %d", p1);
        return -106;
    }
    //	Begin by copying the popln to a clean TrialPop
    popln = ctx->populations[p1];
    if (!strcmp(popln->name, "TrialPop")) {
        log_msg(ctx, 1, "Cannot save TrialPop");
        return -105;
    }

    memcpy(&oldctx, &ctx->state, sizeof(State));

    //	Save old name
    strcpy(oldname, popln->name);
    /*	If name begins "BST_", change it to "BSTP" to avoid overwriting
    a perhaps better BST_ model existing at time of restore.  */
    if (strncmp(oldname, "BST_", 4) == 0) {
        oldname[3] = 'P';
        log_msg(ctx, 1, "This model will be saved with name BSTP... not BST_...");
    }

    i = find_population(ctx, "TrialPop");
    if (i >= 0) {
        destroy_population(ctx, i);
    }
    sample_size = popln->sample_size;
    if (sample_size == 0) {
        fill = 0;
    }
    if (!fill) {
        sample_size = 0;
    }
    i = copy_population(ctx, p1, fill, "TrialPop");
    if (i < 0) {
        memcpy(&ctx->state, &oldctx, sizeof(State));
        return i;
    }

    /*	We can now be sure that, in TrialPop, all subs are gone and classes
        have id-s from 0 up, starting with root  */
    //	switch context to TrialPop
    ctx->state.popln = ctx->populations[i];
    ctx->state.vset = ctx->var_sets[find_vset(ctx, popln->vst_name)];
    sample_size = popln->sample_size;
    if (!fill)
        sample_size = 0;

    file_ptr = fopen(newname, "w");
    if (!file_ptr) {
        log_msg(ctx, 1, "Cannot open %s", newname);
        memcpy(&ctx->state, &oldctx, sizeof(State));
        return -102;
    }
    //	Output some heading material
    for (jp = saveheading; *jp; jp++) {
        fputc(*jp, file_ptr);
    }
    fputc('\n', file_ptr);
    for (jp = oldname; *jp; jp++) {
        fputc(*jp, file_ptr);
    }
    fputc('\n', file_ptr);
    for (jp = popln->vst_name; *jp; jp++) {
        fputc(*jp, file_ptr);
    }
    fputc('\n', file_ptr);
    for (jp = popln->sample_name; *jp; jp++) {
        fputc(*jp, file_ptr);
    }
    fputc('\n', file_ptr);
    fprintf(file_ptr, "%4d%10d\n", popln->num_classes, sample_size);
    fputc('+', file_ptr);
    fputc('\n', file_ptr);

    //	We must begin copying classes, begining with pop's root
    //	The classes should be lined up in pop->classes

    for (jcl = 0; jcl < popln->num_classes; jcl++) {
        cls = popln->classes[jcl];
        leng = 0;
        nch = ((char *)&cls->id) - ((char *)cls);
        recordit(file_ptr, cls, nch);
        leng += nch;

        //	Copy Basics..
        for (iv = 0; iv < ctx->state.vset->length; iv++) {
            cls_var = cls->basics[iv];
            nch = ctx->state.vset->variables[iv].basic_size;
            recordit(file_ptr, cls_var, nch);
            leng += nch;
        }

        //	Copy stats
        for (iv = 0; iv < ctx->state.vset->length; iv++) {
            exp_var = cls->stats[iv];
            nch = ctx->state.vset->variables[iv].stats_size;
            recordit(file_ptr, exp_var, nch);
            leng += nch;
        }
        if (sample_size != 0) {
            //	Copy scores
            nch = sample_size * sizeof(short);
            recordit(file_ptr, cls->factor_scores, nch);
            leng += nch;
        }
    }

    fclose(file_ptr);
    log_msg(ctx, 1, "\nModel %s  Cost %10.2f  saved in file %s", oldname, popln->classes[0]->best_cost, newname);
    memcpy(&ctx->state, &oldctx, sizeof(State));
    return (leng);
}

/**
 * @brief To read a model saved by savepop
 * @param ctx Pointer to the Snob context.
 * @param filename Name or filename string.
 */
int load_population(SnobContext *ctx, const char *filename) {
    char pname[80], name[80], *jp;
    State oldctx;
    int i, j, k, indx, fncl, fnc, nch, iv;
    Class *cls;
    ClassVar *cls_var;
    ExplnVar *exp_var;
    FILE *file_ptr;
    Population *popln = 0;

    int num_cases = 0;

    indx = -999;
    log_msg(ctx, 1, "Loading model from %s", filename);
    memcpy(&oldctx, &ctx->state, sizeof(State));
    file_ptr = fopen(filename, "r");
    if (!file_ptr) {
        log_msg(ctx, 1, "Cannot open %s", filename);
        memcpy(&ctx->state, &oldctx, sizeof(State));
        return indx;
    }
    fscanf(file_ptr, "%s", name);
    if (strcmp(name, saveheading)) {
        log_msg(ctx, 1, "File is not a Scnob save-file");
        fclose(file_ptr);
        memcpy(&ctx->state, &oldctx, sizeof(State));
        return indx;
    }
    fscanf(file_ptr, "%s", pname);
    log_msg(ctx, 1, "Model %s", pname);
    fscanf(file_ptr, "%s", name); // Reading v-set name
    j = find_vset(ctx, name);
    if (j < 0) {
        log_msg(ctx, 1, "Model needs variableset %s", name);
        fclose(file_ptr);
        memcpy(&ctx->state, &oldctx, sizeof(State));
        return indx;
    }
    ctx->state.vset = ctx->var_sets[j];
    fscanf(file_ptr, "%s", name);          // Reading sample name
    fscanf(file_ptr, "%d%d", &fncl, &fnc); // num of classes, cases
                                           //	Advance to real data
    while (fgetc(file_ptr) != '+') {
        // do nothing
    }
    fgetc(file_ptr);
    if (fnc) {
        j = find_sample(ctx, name, 1);
        if (j < 0) {
            log_msg(ctx, 1, "Sample %s unknown.", name);
            num_cases = 0;
            ctx->state.sample = 0;
        } else {
            ctx->state.sample = ctx->samples[j];
            if (ctx->state.sample->num_cases != fnc) {
                log_msg(ctx, 1, "Size conflict Model%9d vs. Sample%9d", fnc, num_cases);
                fclose(file_ptr);
                memcpy(&ctx->state, &oldctx, sizeof(State));
                return indx;
            }
            num_cases = fnc;
        }
    } else { // file model unattached.
        log_msg(ctx, 0, "Model unattached!");
        ctx->state.sample = 0;
        num_cases = 0;
    }

    //	Build input model in TrialPop
    j = find_population(ctx, "TrialPop");
    if (j >= 0)
        destroy_population(ctx, j);
    indx = make_population(ctx, num_cases);
    if (indx < 0) {
        fclose(file_ptr);
        memcpy(&ctx->state, &oldctx, sizeof(State));
        return indx;
    }
    popln = ctx->state.popln = ctx->populations[indx];
    popln->sample_size = num_cases;
    strcpy(popln->name, "TrialPop");
    if (!num_cases)
        strcpy(popln->sample_name, "??");
    popln->num_leaves = 0;

    //	Read all classes: root slot (cls_idx == 0) already exists;
    //	additional classes are allocated via make_class.
    for (int cls_idx = 0; cls_idx < fncl; cls_idx++) {
        if (cls_idx == 0) {
            j = 0; // root class slot already exists
        } else {
            j = make_class(ctx);
            if (j < 0) {
                log_msg(ctx, 1, "RestoreClass fails in Makeclass");
                fclose(file_ptr);
                memcpy(&ctx->state, &oldctx, sizeof(State));
                return j;
            }
        }

        cls = popln->classes[j];
        jp = (char *)cls;
        nch = ((char *)&cls->id) - jp;
        for (k = 0; k < nch; k++) {
            *jp = fgetc(file_ptr);
            jp++;
        }
        for (iv = 0; iv < ctx->state.vset->length; iv++) {
            cls_var = cls->basics[iv];
            nch = ctx->state.vset->variables[iv].basic_size;
            jp = (char *)cls_var;
            for (k = 0; k < nch; k++) {
                *jp = fgetc(file_ptr);
                jp++;
            }
        }
        for (iv = 0; iv < ctx->state.vset->length; iv++) {
            exp_var = cls->stats[iv];
            nch = ctx->state.vset->variables[iv].stats_size;
            jp = (char *)exp_var;
            for (k = 0; k < nch; k++) {
                *jp = fgetc(file_ptr);
                jp++;
            }
        }
        if (cls->type == Leaf)
            popln->num_leaves++;

        if (fnc) {
            if (!num_cases) {
                //	Read scores but discard — sample not available
                nch = fnc * sizeof(short);
                for (k = 0; k < nch; k++)
                    fgetc(file_ptr);
            } else {
                nch = num_cases * sizeof(short);
                jp = (char *)(cls->factor_scores);
                for (k = 0; k < nch; k++) {
                    *jp = fgetc(file_ptr);
                    jp++;
                }
            }
        }
    }

    fclose(file_ptr);

    i = find_population(ctx, pname);
    if (i >= 0) {
        log_msg(ctx, 1, "Overwriting old model %s", pname);
        destroy_population(ctx, i);
    }

    if (!strcmp(pname, "work")) {
        indx = set_work_population(ctx, popln->id);
        j = find_population(ctx, "Trialpop");
        if (j >= 0)
            destroy_population(ctx, j);
    } else {
        strcpy(popln->name, pname);
        memcpy(&ctx->state, &oldctx, sizeof(State));
    }

    return (indx);
}

/**
 * @brief To load popln pp into work, attaching sample as needed.
 * Returns index of work, and sets ctx, or returns neg if fail
 * @param ctx Pointer to the Snob context.
 * @param pp
 */
int set_work_population(SnobContext *ctx, int pp) {
    int j, windx, fpopnc;
    State oldctx;
    Population *fpop, *popln;

    windx = -1;
    memcpy(&oldctx, &ctx->state, sizeof(State));
    fpop = 0;
    if (pp < 0)
        goto error;
    fpop = popln = ctx->populations[pp];
    if (!popln)
        goto error;
    //	Save the 'nc' of the popln to be loaded
    fpopnc = popln->sample_size;
    //	Check popln vset
    j = find_vset(ctx, popln->vst_name);
    if (j < 0) {
        log_msg(ctx, 1, "Load cannot find variable set");
        goto error;
    }
    ctx->state.vset = ctx->var_sets[j];
    //	Check VarSet
    if (ctx->state.sample && strcmp(ctx->state.vset->name, oldctx.sample->vset_name)) {
        log_msg(ctx, 1, "Picked popln has incompatible VariableSet");
        goto error;
    }
    //	Check sample
    if (fpopnc && strcmp(popln->sample_name, oldctx.sample->name)) {
        log_msg(ctx, 1, "Picked popln attached to non-current sample.");
        //	Make pop appear unattached
        popln->sample_size = 0;
    }

    ctx->state.sample = oldctx.sample;
    windx = copy_population(ctx, pp, 1, "work");
    if (windx < 0)
        goto error;
    if (ctx->populations[pp]->sample_size)
        goto finish;

    /*	The popln was copied as if unattached, so scores, weights must be
        fixed  */
    log_msg(ctx, 1, "Model will have weights, scores adjusted to sample.");
    ctx->fix = Partial;
    ctx->control = AdjSc;
fixscores:
    ctx->see_all = 16;
    do_all(ctx, 15, 0);
    //	doall should leave a count of score changes in global
    log_msg(ctx, 1, "%8d  score changes", ctx->score_changes);
    if (ctx->heard) {
        log_msg(ctx, 1, "Score fixing stopped prematurely");
    } else if (ctx->score_changes > 1)
        goto fixscores;
    ctx->fix = ctx->d_fix;
    ctx->control = ctx->d_control;
    goto finish;

error:
    memcpy(&ctx->state, &oldctx, sizeof(State));
finish:
    //	Restore 'nc' of copied popln
    if (fpop)
        fpop->sample_size = fpopnc;
    if ((windx >= 0) && (ctx->debug < 1))
        print_tree(ctx);
    return (windx);
}

/**
 * @brief Given a popln id attached to same sample as 'work' popln,
 * prints the overlap between the leaves of one popln and leaves of other.
 * The output is a cross-tabulation table of the leaves of 'work' against the
 * leaves of the chosen popln, which must be attached to the current sample.
 * Table entries show the weight assigned to both one leaf of 'work' and one
 * leaf of the other popln, as a permillage of active things.
 *
 * @param ctx Pointer to the Snob context.
 * @param xid Identifier or serial number.
 */
void correlpops(SnobContext *ctx, int xid) {
    float table[MAX_CLASSES][MAX_CLASSES];
    Class *wsons[MAX_CLASSES], *xsons[MAX_CLASSES];
    Population *xpop, *wpop;
    double fnact, wwt;
    char *record;
    int wic, xic, n, pcw, wnl, xnl, num_son;
    Population *popln = ctx->state.popln;
    int num_cases = (ctx->state.sample) ? ctx->state.sample->num_cases : 0;

    wpop = popln;
    xpop = ctx->populations[xid];
    if (!xpop) {
        log_msg(ctx, 1, "No such model");
        goto finish;
    }
    if (!xpop->sample_size) {
        log_msg(ctx, 1, "Model is unattached");
        goto finish;
    }
    if (strcmp(popln->sample_name, xpop->sample_name)) {
        log_msg(ctx, 1, "Model not for same sample.");
        goto finish;
    }
    if (xpop->sample_size != num_cases) {
        log_msg(ctx, 1, "Models have unequal numbers of cases.");
        goto finish;
    }
    //	Should be able to proceed
    fnact = ctx->state.sample->num_active;
    ctx->control = AdjSc;
    ctx->see_all = 4;
    ctx->no_subs++;
    do_all(ctx, 3, 1);
    wpop = popln;
    wnl = find_all(ctx, Leaf);
    for (wic = 0; wic < wnl; wic++)
        wsons[wic] = ctx->sons[wic];
    //	Switch to xpop
    ctx->state.popln = xpop;

    ctx->see_all = 4;
    do_all(ctx, 3, 1);
    //	Find all leaves of xpop
    xnl = find_all(ctx, Leaf);
    if ((wnl < 2) || (xnl < 2)) {
        log_msg(ctx, 1, "Need at least 2 classes in each model.");
        goto finish;
    }
    for (xic = 0; xic < xnl; xic++)
        xsons[xic] = ctx->sons[xic];
    //	Clear table
    for (wic = 0; wic < wnl; wic++) {
        for (xic = 0; xic < xnl; xic++)
            table[wic][xic] = 0.0;
    }

    //	Now accumulate cross products of weights for each active item
    ctx->see_all = 2;
    for (n = 0; n < num_cases; n++) {
        ctx->state.popln = wpop;

        record = ctx->state.sample->records + n * ctx->state.sample->record_length;
        if (!*record)
            goto ndone;
        num_son = find_all(ctx, Leaf);
        do_case(ctx, n, Leaf, 0, num_son);
        ctx->state.popln = xpop;
        num_son = find_all(ctx, Leaf);
        do_case(ctx, n, Leaf, 0, num_son);
        //	Should now have caseweights set in leaves of both poplns
        for (wic = 0; wic < wnl; wic++) {
            wwt = wsons[wic]->case_weight;
            for (xic = 0; xic < xnl; xic++) {
                table[wic][xic] += wwt * xsons[xic]->case_weight;
            }
        }
    ndone:;
    }

    //	Print results
    log_msg(ctx, 1, "Cross tabulation of popln classes as permillages.");
    log_msg(ctx, 1, "Rows show 'work' classes, Columns show 'other' classes.");
    log_msg(ctx, 1, "Ser:");
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
    if (ctx->no_subs > 0)
        ctx->no_subs--;
    ctx->state.popln = wpop;
    ctx->control = ctx->d_control;
    return;
}
