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

    samp = ctx.sample;
    svi = samp->variables + i;
    avi = vlist + i;
    vtp = avi->vtype;
    loc = (char *)samp->records;
    loc += n * samp->record_length;
    loc += svi->offset;
    /*	Test for missing  */
    if (*loc == 1) {
        printf("    =====");
        return;
    }
    loc += 1;
    (*vtp->print_datum)(loc);
    return;
}
/*	------------------------ readvset ------------------------------ */
/*	To read in a vset from a file. Returns index of vset  */
/*	Returns negative if fail*/
int read_vset() {

    int i, itype, indx;
    int kread;
    Buffer bufst, *buf;
    char *vaux;

    buf = &bufst;
    for (i = 0; i < MAX_VSETS; i++)
        if (!vsets[i])
            goto gotit;
nospce:
    printf("No space for VariableSet\n");
    i = -10;
    goto error;

gotit:
    indx = i;
    vst = vsets[i] = (VarSet *)malloc(sizeof(VarSet));
    if (!vst)
        goto nospce;
    vst->id = indx;
    ctx.vset = vst;
    vst->variables = 0;
    vst->blocks = 0;
    printf("Enter variable-set file name:\n");
    kread = read_str(vst->filename, 1);
    strcpy(buf->cname, vst->filename);
    ctx.buffer = buf;
    if (open_buffser()) {
        printf("Cant open variable-set file %s\n", vst->filename);
        i = -2;
        goto error;
    }

    /*	Begin to read variable-set file. First entry is its name   */
    new_line();
    kread = read_str(vst->name, 1);
    if (kread < 0) {
        printf("Error in name of variable-set\n");
        i = -9;
        goto error;
    }
    /*	Num of variables   */
    new_line();
    kread = read_int(&nv, 1);
    if (kread) {
        printf("Cant read number of variables\n");
        i = -4;
        goto error;
    }
    vst->length = nv;
    vst->num_active = vst->length;

    /*	Make a vec of nv VSetVar blocks  */
    vlist = (VSetVar *)alloc_blocks(3, nv * sizeof(VSetVar));
    if (!vlist) {
        printf("Cannot allocate memory for variables blocks\n");
        i = -3;
        goto error;
    }
    vst->variables = vlist;

    /*	Read in the info for each variable into vlist   */
    for (i = 0; i < nv; i++) {
        vlist[i].id = -1;
        vlist[i].vaux = 0;
    }
    for (i = 0; i < nv; i++) {
        avi = vlist + i;
        avi->id = i;

        /*	Read name  */
        new_line();
        kread = read_str(avi->name, 1);
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
        avi->inactive = 0;
        if (itype < 0) {
            avi->inactive = 1;
            itype = -itype;
        }
        if ((itype < 1) || (itype > Ntypes)) {
            printf("Bad type code %d for var %d\n", itype, i + 1);
            i = -5;
            goto error;
        }
        itype = itype - 1; /*  Convert types to start at 0  */
        vtp = avi->vtype = types + itype;
        avi->type = itype;

        /*	Make the vaux block  */
        vaux = (char *)alloc_blocks(3, vtp->attr_aux_size);
        if (!vaux) {
            printf("Cant make auxilliary var block\n");
            i = -6;
            goto error;
        }
        avi->vaux = vaux;

        /*	Read auxilliary information   */
        if ((*vtp->read_aux_attr)(vaux)) {
            printf("Error in reading auxilliary info var %d\n", i + 1);
            i = -7;
            goto error;
        }
        /*	Set sizes of stats and basic blocks for var in classes */
        (*vtp->set_sizes)(i);
    } /* End of variables loop */
    i = indx;

error:
    close_buffer();
    ctx.buffer = source;
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

    memcpy(&oldctx, &ctx, sizeof(Context));
    buf = &bufst;
    strcpy(buf->cname, fname);
    ctx.buffer = buf;
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
    vst = ctx.vset = vsets[kread];

    /*	Find a vacant sample slot  */
    for (i = 0; i < MAX_SAMPLES; i++) {
        if (samples[i] == 0)
            goto gotit;
    }
    goto nospace;

gotit:
    samp = samples[i] = (Sample *)malloc(sizeof(Sample));
    if (!samp)
        goto nospace;
    ctx.sample = samp;
    samp->blocks = 0;
    samp->id = i;
    strcpy(samp->filename, buf->cname);
    strcpy(samp->name, sampname);
    /*	Set variable-set name in sample  */
    strcpy(samp->vset_name, vst->name);
    /*	Num of variables   */
    nv = vst->length;
    vlist = vst->variables;

    /*	Make a vec of nv SampleVar blocks  */
    svars = (SampleVar *)alloc_blocks(0, nv * sizeof(SampleVar));
    if (!svars) {
        printf("Cannot allocate memory for variables blocks\n");
        i = -3;
        goto error;
    }
    samp->variables = svars;

    /*	Read in the info for each variable into svars   */
    for (i = 0; i < nv; i++) {
        svars[i].id = -1;
        svars[i].saux = 0;
        svars[i].offset = 0;
        svars[i].nval = 0;
    }
    reclen = 1 + sizeof(int); /* active flag and ident  */
    for (i = 0; i < nv; i++) {
        svi = svars + i;
        svi->id = i;
        avi = vlist + i;
        vtp = avi->vtype;

        /*	Make the saux block  */
        saux = (char *)alloc_blocks(0, vtp->smpl_aux_size);
        if (!saux) {
            printf("Cant make auxilliary var block\n");
            i = -6;
            goto error;
        }
        svi->saux = saux;

        /*	Read auxilliary information   */
        if ((*vtp->read_aux_smpl)(saux)) {
            printf("Error in reading auxilliary info var %d\n", i + 1);
            i = -7;
            goto error;
        }

        /*	Set the offset of the (missing, value) pair  */
        svi->offset = reclen;
        reclen += (1 + vtp->data_size); /* missing flag and value */
    }                                 /* End of variables loop */

    /*	Now attempt to read in the data. The first item is the number of cases*/
    new_line();
    kread = read_int(&nc, 1);
    if (kread) {
        printf("Cant read number of cases\n");
        i = -11;
        goto error;
    }
    samp->num_cases = nc;
    samp->num_active = 0;
    /*	Make a vector of nc records each of size reclen  */
    recs = samp->records = loc = (char *)alloc_blocks(0, nc * reclen);
    if (!loc) {
        printf("No space for data\n");
        i = -8;
        goto error;
    }
    samp->record_length = reclen;

    /*	Read in the data cases, each preceded by an active flag and ident   */
    for (n = 0; n < nc; n++) {
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
            *loc = 0;
        } else {
            *loc = 1;
            samp->num_active++;
        }
        loc++;
        memcpy(loc, &caseid, sizeof(int));
        loc += sizeof(int);
        /*	Posn now points to where the (missing, val) pair for the
        attribute should start.  */
        for (i = 0; i < nv; i++) {
            svi = svars + i;
            avi = vlist + i;
            vtp = avi->vtype;
            kread = (*vtp->read_datum)(loc + 1, i);
            if (kread < 0) {
                printf("Data error case %d var %d\n", n + 1, i + 1);
                swallow();
            }
            if (kread)
                *loc = 1; /* Data missing */
            else {
                *loc = 0;
                svi->nval++;
            }
            loc += (vtp->data_size + 1);
        }
    }
    printf("Number of active cases = %d\n", samp->num_active);
    close_buffer();
    ctx.buffer = source;
    if (sort_sample(samp)) {
        printf("Sort failure on sample\n");
        return (-1);
    }
    return (samp->id);

nospace:
    printf("No space for another sample\n");
    i = -1;
error:
    close_buffer();
    memcpy(&ctx, &oldctx, sizeof(Context));
    return (i);
}

/*	-----------------------  sname2id  ------------------------  */
/*	To find sample id given its name. Returns -1 if unknown  */
/*	'expect' shows if sample expected to be present  */
int find_sample(char *nam, int expect)
{
    int i;

    for (i = 0; i < MAX_SAMPLES; i++) {
        if (samples[i]) {
            if (!strcmp(nam, samples[i]->name))
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
        if (vsets[i]) {
            if (!strcmp(nam, vsets[i]->name))
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

    if ((!samp) || (samp->num_cases == 0)) {
        printf("No defined sample\n");
        return (-1);
    }
    recs = samp->records + 1;
    len = samp->record_length;
    iu = samp->num_cases;
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
    int nn, dadser, i, bc, tid, bl;
    double bw, bs;
    Class *clp;

    /*	Check we have an attched sample and model  */
    if (!ctx.popln)
        return (-1);
    if (!ctx.sample)
        return (-2);
    set_population();
    if (!samp->num_cases)
        return (-3);

    /*	Open a file  */
    tlst = fopen(tlstname, "w");
    if (!tlst)
        return (-4);
    /*	Output a tree list in a primitive form  */
    clp = rootcl;

treeloop:
    if (clp->type == Sub)
        goto nextcl1;
    fprintf(tlst, "%8d", clp->serial >> 2);
    if (clp->dad_id >= 0)
        dadser = population->classes[clp->dad_id]->serial;
    else
        dadser = -4;
    fprintf(tlst, "%8d\n", dadser >> 2);
nextcl1:
    next_class(&clp);
    if (clp)
        goto treeloop;
    fprintf(tlst, "0 0\n");

    find_all(Dad + Leaf);

    for (nn = 0; nn < samp->num_cases; nn++) {
        do_case(nn, Leaf + Dad, 0);
        bl = bc = -1;
        bw = 0.0;
        bs = samp->num_cases + 1;
        for (i = 0; i < numson; i++) {
            clp = sons[i];
            if ((clp->case_weight > 0.5) && (clp->weights_sum < bs)) {
                bc = i;
                bs = clp->weights_sum;
            }
            if ((clp->type == Leaf) && (clp->case_weight > bw)) {
                bl = i;
                bw = clp->case_weight;
            }
        }

        memcpy(&tid, record + 1, sizeof(int));
        fprintf(tlst, "%8d %6d %6d  %6.3f\n", tid, sons[bc]->serial >> 2,
                sons[bl]->serial >> 2, ScoreRscale * sons[bl]->factor_scores[nn]);
    }

    fclose(tlst);
    return (0);
}
