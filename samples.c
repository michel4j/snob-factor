#define NOTGLOB 1
#define SAMPLES 1
#include "glob.h"

int sort_sample();

/*    ----------------------  print_var_datum  -------------------------  */
/*    To print datum for variable i, case n, in sample     */
void print_var_datum(int i, int n) {
    Sample *smpl;
    AVinst *avi;
    SVinst *svi;
    VarType *var_t;

    smpl = CurCtx.sample;
    svi = smpl->variables + i;
    avi = CurAttrList + i;
    var_t = avi->vtype;
    CurField = (char *)smpl->records;
    CurField += n * smpl->record_length;
    CurField += svi->offset;
    /*    Test for missing  */
    if (*CurField == 1) {
        printf("    =====");
        return;
    }
    CurField += 1;
    (*var_t->print_datum)(CurField);
    return;
}
/*    ------------------------ load_vset ------------------------------ */
/*    To read in a vset from a file. Returns index of vset  */
/*    Returns negative if fail*/
int load_vset() {

    int i, itype, indx;
    int kread;
    Buf bufst, *buf;
    char *vaux;

    buf = &bufst;
    for (i = 0; i < MAX_VSETS; i++)
        if (!VarSets[i])
            goto gotit;
nospce:
    printf("No space for VarSet\n");
    i = -10;
    goto error;

gotit:
    indx = i;
    CurVSet = VarSets[i] = (VarSet *)malloc(sizeof(VarSet));
    if (!CurVSet)
        goto nospce;
    CurVSet->id = indx;
    CurCtx.vset = CurVSet;
    CurVSet->attrs = 0;
    CurVSet->blocks = 0;
    printf("Enter variable-set file name:\n");
    kread = read_str(CurVSet->filename, 1);
    strcpy(buf->cname, CurVSet->filename);
    CurCtx.buffer = buf;
    if (open_buffer()) {
        printf("Cant open variable-set file %s\n", CurVSet->filename);
        i = -2;
        goto error;
    }

    /*    Begin to read variable-set file. First entry is its name   */
    new_line();
    kread = read_str(CurVSet->name, 1);
    if (kread < 0) {
        printf("Error in name of variable-set\n");
        i = -9;
        goto error;
    }
    /*    Num of variables   */
    new_line();
    kread = read_int(&NumVars, 1);
    if (kread) {
        printf("Cant read number of variables\n");
        i = -4;
        goto error;
    }
    CurVSet->num_vars = NumVars;
    CurVSet->num_active = CurVSet->num_vars;

    /*    Make a vec of nv AVinst blocks  */
    CurAttrList = (AVinst *)alloc_blocks(3, NumVars * sizeof(AVinst));
    if (!CurAttrList) {
        printf("Cannot allocate memory for variables blocks\n");
        i = -3;
        goto error;
    }
    CurVSet->attrs = CurAttrList;

    /*    Read in the info for each variable into vlist   */
    for (i = 0; i < NumVars; i++) {
        CurAttrList[i].id = -1;
        CurAttrList[i].vaux = 0;
    }
    for (i = 0; i < NumVars; i++) {
        CurAttr = CurAttrList + i;
        CurAttr->id = i;

        /*    Read name  */
        new_line();
        kread = read_str(CurAttr->name, 1);
        if (kread < 0) {
            printf("Error in name of variable %d\n", i + 1);
            i = -10;
            goto error;
        }

        /*    Read type code. Negative means idle.   */
        kread = read_int(&itype, 1);
        if (kread) {
            printf("Cant read type code for var %d\n", i + 1);
            i = -5;
            goto error;
        }
        CurAttr->inactive = 0;
        if (itype < 0) {
            CurAttr->inactive = 1;
            itype = -itype;
        }
        if ((itype < 1) || (itype > Ntypes)) {
            printf("Bad type code %d for var %d\n", itype, i + 1);
            i = -5;
            goto error;
        }
        itype = itype - 1; /*  Convert types to start at 0  */
        CurVType = CurAttr->vtype = Types + itype;
        CurAttr->type = itype;

        /*    Make the vaux block  */
        vaux = (char *)alloc_blocks(3, CurVType->attr_aux_size);
        if (!vaux) {
            printf("Cant make auxilliary var block\n");
            i = -6;
            goto error;
        }
        CurAttr->vaux = vaux;

        /*    Read auxilliary information   */
        if ((*CurVType->read_aux_attr)(vaux)) {
            printf("Error in reading auxilliary info var %d\n", i + 1);
            i = -7;
            goto error;
        }
        /*    Set sizes of stats and basic blocks for var in classes */
        (*CurVType->set_sizes)(i);
    } /* End of variables loop */
    i = indx;

error:
    close_buffer();
    CurCtx.buffer = CurSource;
    return (i);
}

/*    ---------------------  load_sample -------------------------  */
/*    To open a sample file and read in all about the sample.
    Returns index of new sample in samples array   */
int load_sample(char *fname) {

    int i, n;
    int kread;
    int caseid;
    Buf bufst, *buf;
    Context oldctx;
    char *saux, vstnam[80], sampname[80];

    memcpy(&oldctx, &CurCtx, sizeof(Context));
    buf = &bufst;
    strcpy(buf->cname, fname);
    CurCtx.buffer = buf;
    if (open_buffer()) {
        printf("Cannot open sample file %s\n", buf->cname);
        i = -2;
        goto error;
    }

    /*    Begin to read sample file. First entry is its name   */
    new_line();
    kread = read_str(sampname, 1);
    if (kread < 0) {
        printf("Error in name of sample\n");
        i = -9;
        goto error;
    }
    /*    See if sample already loaded  */
    if (find_sample(sampname, 0) >= 0) {
        printf("Sample %s already present\n", sampname);
        i = -8;
        goto error;
    }
    /*    Next line should be the vset name  */
    new_line();
    kread = read_str(vstnam, 1);
    if (kread < 0) {
        printf("Error in name of variableset\n");
        i = -9;
        goto error;
    }
    /*    Check vset known  */
    kread = find_vset(vstnam);
    if (kread < 0) {
        printf("Variableset %s unknown\n", vstnam);
        i = -8;
        goto error;
    }
    CurVSet = CurCtx.vset = VarSets[kread];

    /*    Find a vacant sample slot  */
    for (i = 0; i < MAX_SAMPLES; i++) {
        if (Samples[i] == 0)
            goto gotit;
    }
    goto nospace;

gotit:
    CurSample = Samples[i] = (Sample *)malloc(sizeof(Sample));
    if (!CurSample)
        goto nospace;
    CurCtx.sample = CurSample;
    CurSample->blocks = 0;
    CurSample->id = i;
    strcpy(CurSample->filename, buf->cname);
    strcpy(CurSample->name, sampname);
    /*    Set variable-set name in sample  */
    strcpy(CurSample->vset_name, CurVSet->name);
    /*    Num of variables   */
    NumVars = CurVSet->num_vars;
    CurAttrList = CurVSet->attrs;

    /*    Make a vec of nv SVinst blocks  */
    CurVarList = (SVinst *)alloc_blocks(0, NumVars * sizeof(SVinst));
    if (!CurVarList) {
        printf("Cannot allocate memory for variables blocks\n");
        i = -3;
        goto error;
    }
    CurSample->variables = CurVarList;

    /*    Read in the info for each variable into svars   */
    for (i = 0; i < NumVars; i++) {
        CurVarList[i].id = -1;
        CurVarList[i].saux = 0;
        CurVarList[i].offset = 0;
        CurVarList[i].nval = 0;
    }
    CurRecLen = 1 + sizeof(int); /* active flag and ident  */
    for (i = 0; i < NumVars; i++) {
        CurVar = CurVarList + i;
        CurVar->id = i;
        CurAttr = CurAttrList + i;
        CurVType = CurAttr->vtype;

        /*    Make the saux block  */
        saux = (char *)alloc_blocks(0, CurVType->smpl_aux_size);
        if (!saux) {
            printf("Cant make auxilliary var block\n");
            i = -6;
            goto error;
        }
        CurVar->saux = saux;

        /*    Read auxilliary information   */
        if ((*CurVType->read_aux_smpl)(saux)) {
            printf("Error in reading auxilliary info var %d\n", i + 1);
            i = -7;
            goto error;
        }

        /*    Set the offset of the (missing, value) pair  */
        CurVar->offset = CurRecLen;
        CurRecLen += (1 + CurVType->data_size); /* missing flag and value */
    }                                 /* End of variables loop */

    /*    Now attempt to read in the data. The first item is the number of cases*/
    new_line();
    kread = read_int(&NumCases, 1);
    if (kread) {
        printf("Cant read number of cases\n");
        i = -11;
        goto error;
    }
    CurSample->num_cases = NumCases;
    CurSample->num_active = 0;
    /*    Make a vector of nc records each of size reclen  */
    CurRecords = CurSample->records = CurField = (char *)alloc_blocks(0, NumCases * CurRecLen);
    if (!CurField) {
        printf("No space for data\n");
        i = -8;
        goto error;
    }
    CurSample->record_length = CurRecLen;

    /*    Read in the data cases, each preceded by an active flag and ident   */
    for (n = 0; n < NumCases; n++) {
        new_line();
        kread = read_int(&caseid, 1);
        if (kread) {
            printf("Cant read ident, case %d\n", n + 1);
            i = -12;
            goto error;
        }
        /*    If ident negative, so clear active */
        if (caseid < 0) {
            caseid = -caseid;
            *CurField = 0;
        } else {
            *CurField = 1;
            CurSample->num_active++;
        }
        CurField++;
        memcpy(CurField, &caseid, sizeof(int));
        CurField += sizeof(int);
        /*    Posn now points to where the (missing, val) pair for the
        attribute should start.  */
        for (i = 0; i < NumVars; i++) {
            CurVar = CurVarList + i;
            CurAttr = CurAttrList + i;
            CurVType = CurAttr->vtype;
            kread = (*CurVType->read_datum)(CurField + 1, i);
            if (kread < 0) {
                printf("Data error case %d var %d\n", n + 1, i + 1);
                swallow();
            }
            if (kread)
                *CurField = 1; /* Data missing */
            else {
                *CurField = 0;
                CurVar->nval++;
            }
            CurField += (CurVType->data_size + 1);
        }
    }
    printf("Number of active cases = %d\n", CurSample->num_active);
    close_buffer();
    CurCtx.buffer = CurSource;
    if (sort_sample(CurSample)) {
        printf("Sort failure on sample\n");
        return (-1);
    }
    return (CurSample->id);

nospace:
    printf("No space for another sample\n");
    i = -1;
error:
    close_buffer();
    memcpy(&CurCtx, &oldctx, sizeof(Context));
    return (i);
}

/*    -----------------------  find_sample  ------------------------  */
/*    To find sample id given its name. Returns -1 if unknown  */
/*    'expect' shows if sample expected to be present  */
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

/*    -----------------------  find_vset  ------------------------  */
/*    To find vset id given its name. Returns -1 if unknown  */
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

/*    Record swapper  */
void swap_records(char *p1, char *p2, int ll)
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


/*    Recursive quicksort  */
void quick_sort_sample(char *bot, int nn, int len)
{
    char *top, *rp1, *rp2, *cen;
    int av, bv, cv, nb, nt;

again:
    if (nn < 2)
        return;
    if (nn >= 6)
        goto recurse;
    /*    Do a short block by bubble  */
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
            swap_records(cen, rp1, len);
        rp1 += len;
    }
    return;

recurse:
    /*    Pick a random central value  */
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
    /*    Have av < cv, bv >= cv  */
    swap_records(rp1, rp2, len);
    nt++;
    rp2 -= len;
    nb++;
    rp1 += len;
    if (rp2 >= rp1)
        goto loop1;

done:
    /*    Check that something has been placed in lower block.  */
    if (nb)
        goto doboth;
    /*    Nothing was less than cv, the value at cen, so swap it to bot */
    swap_records(cen, bot, len);
    nn--;
    bot += len;
    goto again;

doboth:
    quick_sort_sample(bot, nb, len);
    quick_sort_sample(bot + nb * len, nt, len);
    return;
}

/*    ------------------  sort_sample  ----------------------------  */
/*    To quicksort a sample into increasing ident order   */
int sort_sample(Sample *smpl)
{
    int nc, len;

    nc = smpl->num_cases;
    printf("Begin sort of %d cases\n", nc);
    if (nc < 1) {
        printf("From sort_sample: sample unattached.\n");
        return (-1);
    }
    len = smpl->record_length;

    quick_sort_sample(smpl->records, nc, len);
    printf("Finished sort\n");
    return (0);
}

/*    --------------------  find_sample_index  ------------------------------  */
/*    Given a item ident, returns index in sample, or -1 if not found  */
int find_sample_index(int id)
{
    int iu, il, ic, cid, len;
    char *recs;

    if ((!CurSample) || (CurSample->num_cases == 0)) {
        printf("No defined sample\n");
        return (-1);
    }
    recs = CurSample->records + 1;
    len = CurSample->record_length;
    iu = CurSample->num_cases;
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

/*    ------------------------  item_list  -------------------   */
/*    Records best class and score for all items in a sample.
 */
int item_list(char *tlstname)
{
    FILE *tlst;
    int nn, dadser, i, bc, bl, tid;
    double bw, bs;
    Class *clp;
    char * record;

    /*    Check we have an attched sample and model  */
    if (!CurCtx.popln)
        return (-1);
    if (!CurCtx.sample)
        return (-2);
    set_population();
    if (!CurSample->num_cases)
        return (-3);

    /*    Open a file  */
    tlst = fopen(tlstname, "w");
    if (!tlst)
        return (-4);
    /*    Output a tree list in a primitive form  */
    clp = CurRootClass;

treeloop:
    if (clp->type == Sub)
        goto nextcl1;
    fprintf(tlst, "%8d", clp->serial >> 2);
    if (clp->dad_id >= 0)
        dadser = CurPopln->classes[clp->dad_id]->serial;
    else
        dadser = -4;
    fprintf(tlst, "%8d\n", dadser >> 2);
nextcl1:
    next_class(&clp);
    if (clp)
        goto treeloop;
    fprintf(tlst, "0 0\n");

    find_all(Dad + Leaf);

    for (nn = 0; nn < CurSample->num_cases; nn++) {
        do_case(nn, Leaf + Dad, 0);
        bl = bc = -1;
        bw = 0.0;
        bs = CurSample->num_cases + 1;
        for (i = 0; i < NumSon; i++) {
            clp = Sons[i];
            if ((clp->case_weight > 0.5) && (clp->weights_sum < bs)) {
                bc = i;
                bs = clp->weights_sum;
            }
            if ((clp->type == Leaf) && (clp->case_weight > bw)) {
                bl = i;
                bw = clp->case_weight;
            }
        }
        
        record = CurRecords + nn * CurRecLen; 
        memcpy(&tid, record + 1, sizeof(int));
        fprintf(tlst, "%8d %6d %6d  %6.3f\n", tid, Sons[bc]->serial >> 2,
                Sons[bl]->serial >> 2, ScoreRscale * Sons[bl]->vv[nn]);
        
    }

    fclose(tlst);
    return (0);
}
