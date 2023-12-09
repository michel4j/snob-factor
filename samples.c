#define NOTGLOB 1
#define SAMPLES 1
#include "glob.h"

int sort_sample();

/*	----------------------  printdatum  -------------------------  */
/*	To print datum for variable i, case n, in sample     */
void print_var_datam(int i, int n) {
    Sample *samp;
    VSetVar *avi;
    SampleVar *svi;
    VarType *vtp;

    samp = CurCtx.sample;
    svi = &CurCtx.sample->variables[i];
    avi = &CurCtx.vset->variables[i];
    vtp = avi->vtype;
    CurField = (char *)samp->records;
    CurField += n * samp->record_length;
    CurField += svi->offset;
    /*	Test for missing  */
    if (*CurField == 1) {
        printf("    =====");
        return;
    }
    CurField += 1;
    (*vtp->print_datum)(CurField);
    return;
}
/*	------------------------ readvset ------------------------------ */
/*	To read in a vset from a file. Returns index of vset  */
/*	Returns negative if fail*/

int read_vset() {
    char filename[80];
    int kread;

    printf("Enter variable-set file name:\n");
    kread = read_str(filename, 1);
    if (kread < 0) {
        log_msg(2, "Error in name of variable-set file");
        return (-2);
    } else {
        return load_vset(filename);
    }
}

int load_vset(const char *filename) {

    int i, itype, indx;
    int kread;
    Buffer bufst, *buf;
    char *vaux;
    VarType *vtype;
    VSetVar *vset_var, *vset_var_list;

    buf = &bufst;
    for (i = 0; i < MAX_VSETS; i++)
        if (!VarSets[i])
            goto gotit;
nospce:
    printf("No space for VariableSet\n");
    i = -10;
    goto error;

gotit:
    indx = i;
    CurCtx.vset = VarSets[i] = (VarSet *)malloc(sizeof(VarSet));
    if (!CurCtx.vset)
        goto nospce;
    CurCtx.vset->id = indx;
    CurCtx.vset->variables = 0;
    CurCtx.vset->blocks = 0;
    strcpy(CurCtx.vset->filename, filename);
    strcpy(buf->cname, CurCtx.vset->filename);
    CurCtx.buffer = buf;
    if (open_buffser()) {
        printf("Cant open variable-set file %s\n", CurCtx.vset->filename);
        i = -2;
        goto error;
    }

    /*	Begin to read variable-set file. First entry is its name   */
    new_line();
    kread = read_str(CurCtx.vset->name, 1);
    if (kread < 0) {
        printf("Error in name of variable-set\n");
        i = -9;
        goto error;
    }
    /*	Num of variables   */
    new_line();
    kread = read_int(&NumVars, 1);
    if (kread) {
        printf("Cant read number of variables\n");
        i = -4;
        goto error;
    }
    CurCtx.vset->length = NumVars;
    CurCtx.vset->num_active = CurCtx.vset->length;

    /*	Make a vec of nv VSetVar blocks  */
    vset_var_list = (VSetVar *)alloc_blocks(3, NumVars * sizeof(VSetVar));
    if (!vset_var_list) {
        printf("Cannot allocate memory for variables blocks\n");
        i = -3;
        goto error;
    }
    CurCtx.vset->variables = vset_var_list;

    /*	Read in the info for each variable into vlist   */
    for (i = 0; i < NumVars; i++) {
        CurCtx.vset->variables[i].id = -1;
        CurCtx.vset->variables[i].vaux = 0;
    }
    for (i = 0; i < NumVars; i++) {
        vset_var = &CurCtx.vset->variables[i];
        vset_var->id = i;

        /*	Read name  */
        new_line();
        kread = read_str(vset_var->name, 1);
        if (kread < 0) {
            printf("Error in name of variable %d\n", i + 1);
            i = -10;
            goto error;
        }

        /*	Read type code. Negative means idle.   */
        kread = read_int(&itype, 1);
        if (kread) {
            printf("Cant read type code for var %d\n", i + 1);
            i = -5;
            goto error;
        }
        vset_var->inactive = 0;
        if (itype < 0) {
            vset_var->inactive = 1;
            itype = -itype;
        }
        if ((itype < 1) || (itype > NTypes)) {
            printf("Bad type code %d for var %d\n", itype, i + 1);
            i = -5;
            goto error;
        }
        itype = itype - 1; /*  Convert types to start at 0  */
        vtype = vset_var->vtype = &Types[itype];
        vset_var->type = itype;

        /*	Make the vaux block  */
        vaux = (char *)alloc_blocks(3, vtype->attr_aux_size);
        if (!vaux) {
            printf("Cant make auxilliary var block\n");
            i = -6;
            goto error;
        }
        vset_var->vaux = vaux;

        /*	Read auxilliary information   */
        if ((*vtype->read_aux_attr)(vaux)) {
            printf("Error in reading auxilliary info var %d\n", i + 1);
            i = -7;
            goto error;
        }
        /*	Set sizes of stats and basic blocks for var in classes */
        (*vtype->set_sizes)(i);
    } /* End of variables loop */
    i = indx;

error:
    close_buffer();
    CurCtx.buffer = CurSource;
    return (i);
}

/*	---------------------  readsample -------------------------  */
/*	To open a sample file and read in all about the sample.
    Returns index of new sample in samples array   */
int load_sample(const char *fname) {

    int i, n;
    int kread;
    int caseid;
    Buffer bufst, *buf;
    Context oldctx;
    char *saux, vstnam[80], sampname[80];
    VSetVar *vset_var;
    VarType *vtype;
    SampleVar *smpl_var, *smpl_var_list;

    memcpy(&oldctx, &CurCtx, sizeof(Context));
    buf = &bufst;
    strcpy(buf->cname, fname);
    CurCtx.buffer = buf;
    if (open_buffser()) {
        printf("Cannot open sample file %s\n", buf->cname);
        i = -2;
        goto error;
    }

    /*	Begin to read sample file. First entry is its name   */
    new_line();
    kread = read_str(sampname, 1);
    if (kread < 0) {
        printf("Error in name of sample\n");
        i = -9;
        goto error;
    }
    /*	See if sample already loaded  */
    if (find_sample(sampname, 0) >= 0) {
        printf("Sample %s already present\n", sampname);
        i = -8;
        goto error;
    }
    /*	Next line should be the vset name  */
    new_line();
    kread = read_str(vstnam, 1);
    if (kread < 0) {
        printf("Error in name of variableset\n");
        i = -9;
        goto error;
    }
    /*	Check vset known  */
    kread = find_vset(vstnam);
    if (kread < 0) {
        printf("Variableset %s unknown\n", vstnam);
        i = -8;
        goto error;
    }
    CurCtx.vset = VarSets[kread];

    /*	Find a vacant sample slot  */
    for (i = 0; i < MAX_SAMPLES; i++) {
        if (Samples[i] == 0)
            goto gotit;
    }
    goto nospace;

gotit:
    CurCtx.sample = Samples[i] = (Sample *)malloc(sizeof(Sample));
    if (!CurCtx.sample)
        goto nospace;

    CurCtx.sample->blocks = 0;
    CurCtx.sample->id = i;
    strcpy(CurCtx.sample->filename, buf->cname);
    strcpy(CurCtx.sample->name, sampname);
    /*	Set variable-set name in sample  */
    strcpy(CurCtx.sample->vset_name, CurCtx.vset->name);
    /*	Num of variables   */
    NumVars = CurCtx.vset->length;

    /*	Make a vec of nv SampleVar blocks  */
    smpl_var_list = (SampleVar *)alloc_blocks(0, NumVars * sizeof(SampleVar));
    if (!smpl_var_list) {
        printf("Cannot allocate memory for variables blocks\n");
        i = -3;
        goto error;
    }
    CurCtx.sample->variables = smpl_var_list;

    /*	Read in the info for each variable into svars   */
    for (i = 0; i < NumVars; i++) {
        smpl_var_list[i].id = -1;
        smpl_var_list[i].saux = 0;
        smpl_var_list[i].offset = 0;
        smpl_var_list[i].nval = 0;
    }
    RecLen = 1 + sizeof(int); /* active flag and ident  */
    for (i = 0; i < NumVars; i++) {
        smpl_var = &CurCtx.sample->variables[i];
        vset_var = &CurCtx.vset->variables[i];
        smpl_var->id = i;
        vtype = vset_var->vtype;

        /*	Make the saux block  */
        saux = (char *)alloc_blocks(0, vtype->smpl_aux_size);
        if (!saux) {
            printf("Cant make auxilliary var block\n");
            i = -6;
            goto error;
        }
        smpl_var->saux = saux;

        /*	Read auxilliary information   */
        if ((*vtype->read_aux_smpl)(saux)) {
            printf("Error in reading auxilliary info var %d\n", i + 1);
            i = -7;
            goto error;
        }

        /*	Set the offset of the (missing, value) pair  */
        smpl_var->offset = RecLen;
        RecLen += (1 + vtype->data_size); /* missing flag and value */
    }                                 /* End of variables loop */

    /*	Now attempt to read in the data. The first item is the number of cases*/
    new_line();
    kread = read_int(&NumCases, 1);
    if (kread) {
        printf("Cant read number of cases\n");
        i = -11;
        goto error;
    }
    CurCtx.sample->num_cases = NumCases;
    CurCtx.sample->num_active = 0;
    /*	Make a vector of nc records each of size reclen  */
    Records = CurCtx.sample->records = CurField = (char *)alloc_blocks(0, NumCases * RecLen);
    if (!CurField) {
        printf("No space for data\n");
        i = -8;
        goto error;
    }
    CurCtx.sample->record_length = RecLen;

    /*	Read in the data cases, each preceded by an active flag and ident   */
    for (n = 0; n < NumCases; n++) {
        new_line();
        kread = read_int(&caseid, 1);
        if (kread) {
            printf("Cant read ident, case %d\n", n + 1);
            i = -12;
            goto error;
        }
        /*	If ident negative, so clear active */
        if (caseid < 0) {
            caseid = -caseid;
            *CurField = 0;
        } else {
            *CurField = 1;
            CurCtx.sample->num_active++;
        }
        CurField++;
        memcpy(CurField, &caseid, sizeof(int));
        CurField += sizeof(int);
        /*	Posn now points to where the (missing, val) pair for the
        attribute should start.  */
        for (i = 0; i < NumVars; i++) {
            smpl_var = &CurCtx.sample->variables[i];
            vset_var = &CurCtx.vset->variables[i];
            vtype = vset_var->vtype;
            kread = (*vtype->read_datum)(CurField + 1, i);
            if (kread < 0) {
                printf("Data error case %d var %d\n", n + 1, i + 1);
                swallow();
            }
            if (kread)
                *CurField = 1; /* Data missing */
            else {
                *CurField = 0;
                smpl_var->nval++;
            }
            CurField += (vtype->data_size + 1);
        }
    }
    printf("Number of active cases = %d\n", CurCtx.sample->num_active);
    close_buffer();
    CurCtx.buffer = CurSource;
    if (sort_sample(CurCtx.sample)) {
        printf("Sort failure on sample\n");
        return (-1);
    }
    return (CurCtx.sample->id);

nospace:
    printf("No space for another sample\n");
    i = -1;
error:
    close_buffer();
    memcpy(&CurCtx, &oldctx, sizeof(Context));
    return (i);
}

/*	-----------------------  sname2id  ------------------------  */
/*	To find sample id given its name. Returns -1 if unknown  */
/*	'expect' shows if sample expected to be present  */
int find_sample(char *nam, int expect)
{
    int i;

    for (i = 0; i < MAX_SAMPLES; i++) {
        if (Samples[i]) {
            if (!strcmp(nam, Samples[i]->name))
                goto searched;
        }
    }
    i = -1;

searched:
    if ((i < 0) && expect)
        printf("Cannot find sample %s\n", nam);
    if ((i >= 0) && (!expect))
        printf("Sample %s already loaded\n", nam);
    return (i);
}

/*	-----------------------  vname2id  ------------------------  */
/*	To find vset id given its name. Returns -1 if unknown  */
int find_vset(char *nam)
{
    int i, ii;

    ii = -1;
    for (i = 0; i < MAX_VSETS; i++) {
        if (VarSets[i]) {
            if (!strcmp(nam, VarSets[i]->name))
                ii = i;
        }
    }
    if (ii < 0)
        printf("Cannot find variable set %s\n", nam);
    return (ii);
}

/*	------------------  qssamp  ----------------------------  */
/*	To quicksort a sample into increasing ident order   */

/*	Record swapper  */
void swaprec(char *p1, char *p2, int ll)
{
    int tt;
    if (p1 == p2)
        return;
    while (ll) {
        tt = *p1;
        *p1 = *p2;
        *p2 = tt;
        p1++;
        p2++;
        ll--;
    }
    return;
}

/*	Recursive quicksort  */
void qssamp1(char *bot, int nn, int len)
{
    char *top, *rp1, *rp2, *cen;
    int av, bv, cv, nb, nt;

again:
    if (nn < 2)
        return;
    if (nn >= 6)
        goto recurse;
    /*	Do a short block by bubble  */
    rp1 = bot;
    for (nt = 0; nt < nn - 1; nt++) {
        memcpy(&bv, rp1 + 1, sizeof(int));
        rp2 = cen = rp1;
        for (nb = nt + 1; nb < nn; nb++) {
            rp2 += len;
            memcpy(&av, rp2 + 1, sizeof(int));
            if (av < bv) {
                bv = av;
                cen = rp2;
            }
        }
        if (cen != rp1)
            swaprec(cen, rp1, len);
        rp1 += len;
    }
    return;

recurse:
    /*	Pick a random central value  */
    nt = nn * rand_float();
    if (nt == nn)
        nt = nn / 2;
    cen = bot + nt * len;
    memcpy(&cv, cen + 1, sizeof(int));
    top = bot + (nn - 1) * len;
    rp1 = bot;
    rp2 = top;
    nt = nb = 0;
loop1:
    memcpy(&av, rp2 + 1, sizeof(int));
    if (av >= cv) {
        nt++;
        rp2 -= len;
        if (rp2 >= rp1)
            goto loop1;
        else
            goto done;
    }
loop2:
    memcpy(&bv, rp1 + 1, sizeof(int));
    if (bv < cv) {
        nb++;
        rp1 += len;
        if (rp2 >= rp1)
            goto loop2;
        else
            goto done;
    }
    /*	Have av < cv, bv >= cv  */
    swaprec(rp1, rp2, len);
    nt++;
    rp2 -= len;
    nb++;
    rp1 += len;
    if (rp2 >= rp1)
        goto loop1;

done:
    /*	Check that something has been placed in lower block.  */
    if (nb)
        goto doboth;
    /*	Nothing was less than cv, the value at cen, so swap it to bot */
    swaprec(cen, bot, len);
    nn--;
    bot += len;
    goto again;

doboth:
    qssamp1(bot, nb, len);
    qssamp1(bot + nb * len, nt, len);
    return;
}

int sort_sample(Sample *samp)
{
    int nc, len;

    nc = samp->num_cases;
    printf("Begin sort of %d cases\n", nc);
    if (nc < 1) {
        printf("From qssamp: sample unattached.\n");
        return (-1);
    }
    len = samp->record_length;

    qssamp1(samp->records, nc, len);
    printf("Finished sort\n");
    return (0);
}

/*	--------------------  id2ind  ------------------------------  */
/*	Given a item ident, returns index in sample, or -1 if not found  */
int find_sample_index(int id)
{
    int iu, il, ic, cid, len;
    char *recs;

    if ((!CurCtx.sample) || (CurCtx.sample->num_cases == 0)) {
        printf("No defined sample\n");
        return (-1);
    }
    recs = CurCtx.sample->records + 1;
    len = CurCtx.sample->record_length;
    iu = CurCtx.sample->num_cases;
    il = 0;
chop:
    ic = (iu + il) >> 1;
    memcpy(&cid, recs + ic * len, sizeof(int));
    if (ic == il)
        goto chopped;
    if (cid > id) {
        iu = ic;
        goto chop;
    }
    if (cid < id) {
        il = ic;
        goto chop;
    }
chopped:
    return ((cid == id) ? ic : -1);
}

/*	------------------------  thinglist  -------------------   */
/*	Records best class and score for all things in a sample.
 */
int item_list(char *tlstname)
{
    FILE *tlst;
    int nn, dadser, i, bc, tid, bl, num_son;
    double bw, bs;
    Class *cls;
    Population *popln = CurCtx.popln;
    Class *root = CurCtx.popln->classes[CurCtx.popln->root];
    /*	Check we have an attched sample and model  */
    if (!CurCtx.popln)
        return (-1);
    if (!CurCtx.sample)
        return (-2);
    set_population();
    if (!CurCtx.sample->num_cases)
        return (-3);

    /*	Open a file  */
    tlst = fopen(tlstname, "w");
    if (!tlst)
        return (-4);
    /*	Output a tree list in a primitive form  */
    cls = root;

treeloop:
    if (cls->type == Sub)
        goto nextcl1;
    fprintf(tlst, "%8d", cls->serial >> 2);
    if (cls->dad_id >= 0)
        dadser = popln->classes[cls->dad_id]->serial;
    else
        dadser = -4;
    fprintf(tlst, "%8d\n", dadser >> 2);
nextcl1:
    next_class(&cls);
    if (cls)
        goto treeloop;
    fprintf(tlst, "0 0\n");

    num_son = find_all(Dad + Leaf);

    for (nn = 0; nn < CurCtx.sample->num_cases; nn++) {
        do_case(nn, Leaf + Dad, 0, num_son);
        bl = bc = -1;
        bw = 0.0;
        bs = CurCtx.sample->num_cases + 1;
        for (i = 0; i < num_son; i++) {
            cls = Sons[i];
            if ((cls->case_weight > 0.5) && (cls->weights_sum < bs)) {
                bc = i;
                bs = cls->weights_sum;
            }
            if ((cls->type == Leaf) && (cls->case_weight > bw)) {
                bl = i;
                bw = cls->case_weight;
            }
        }

        memcpy(&tid, CurRecord + 1, sizeof(int));
        fprintf(tlst, "%8d %6d %6d  %6.3f\n", tid, Sons[bc]->serial >> 2,
                Sons[bl]->serial >> 2, ScoreRscale * Sons[bl]->factor_scores[nn]);
    }

    fclose(tlst);
    return (0);
}


/*    -------------------- destroy_sample --------------------  */
/*    To destroy sample index sx    */
void destroy_sample(int sx) {
    int prev;
    if (CurCtx.sample)
        prev = CurCtx.sample->id;
    else
        prev = -1;
    CurCtx.sample = Samples[sx];
    if (!CurCtx.sample)
        return;

    free_blocks(0);
    free(CurCtx.sample);
    Samples[sx] = 0;
    CurCtx.sample = 0;
    if (sx == prev)
        return;
    if (prev < 0)
        return;
    CurCtx.sample = Samples[prev];
}


/*    -------------------- destroy_vset --------------------  */
/*    To destroy vset index vx    */
void destroy_vset(int vx) {
    int prev;
    if (CurCtx.vset)
        prev = CurCtx.vset->id;
    else
        prev = -1;
    CurCtx.vset = VarSets[vx];
    if (!CurCtx.vset)
        return;

    free_blocks(3);
    free(CurCtx.vset);
    VarSets[vx] = 0;
    CurCtx.vset = 0;
    if (vx == prev)
        return;
    if (prev < 0)
        return;
    CurCtx.vset = VarSets[prev];

}