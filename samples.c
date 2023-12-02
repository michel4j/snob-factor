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
    char *field;

    smpl = CurCtx.sample;
    svi = smpl->variables + i;
    avi = CurAttrList + i;
    var_t = avi->vtype;
    field = (char *)smpl->records;
    field += n * smpl->record_length;
    field += svi->offset;
    /*    Test for missing  */
    if (*field == 1) {
        printf("    =====");
        return;
    }
    field += 1;
    (*var_t->print_datum)(field);
    return;
}
/*    ------------------------ load_vset ------------------------------ */
/*    To read in a vset from a file. Returns index of vset  */
/*    Returns negative if fail*/
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
    int i, itype, indx = -10;
    int kread;
    Buf bufst, *buf;
    char *vaux;
    VarSet *vset;

    buf = &bufst;
    for (i = 0; i < MAX_VSETS; i++) {
        if (!VarSets[i]) {
            indx = i;
            break;
        }
    }
    if (indx < 0) {
        log_msg(2, "No space for VarSet");
    }

    while (indx >= 0) {
        vset = VarSets[i] = (VarSet *)malloc(sizeof(VarSet));
        if (!vset) {
            indx = -10;
            break;
        }
        vset->id = indx;
        CurCtx.vset = vset;
        vset->attrs = 0;
        vset->blocks = 0;

        strcpy(vset->filename, filename);
        strcpy(buf->cname, vset->filename);
        CurCtx.buffer = buf;
        if (open_buffer()) {
            log_msg(2, "Cant open variable-set file %s", vset->filename);
            indx = -2;
            break;
        }

        /*    Begin to read variable-set file. First entry is its name   */
        new_line();
        kread = read_str(vset->name, 1);
        if (kread < 0) {
            log_msg(2, "Error in name of variable-set");
            indx = -9;
            break;
        }
        /*    Num of variables   */
        new_line();
        kread = read_int(&NumVars, 1);
        if (kread) {
            log_msg(2, "Cant read number of variables");
            indx = -4;
            break;
        }
        vset->num_vars = NumVars;
        vset->num_active = vset->num_vars;

        /*    Make a vec of nv AVinst blocks  */
        CurAttrList = (AVinst *)alloc_blocks(3, NumVars * sizeof(AVinst));
        if (!CurAttrList) {
            log_msg(2, "Cannot allocate memory for variables blocks");
            indx = -3;
            break;
        }
        vset->attrs = CurAttrList;

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
                log_msg(2, "Error in name of variable %d", i + 1);
                indx = -10;
                break;
            }

            /*    Read type code. Negative means idle.   */
            kread = read_int(&itype, 1);
            if (kread) {
                log_msg(2, "Cant read type code for var %d", i + 1);
                indx = -5;
                break;
            }
            CurAttr->inactive = 0;
            if (itype < 0) {
                CurAttr->inactive = 1;
                itype = -itype;
            }
            if ((itype < 1) || (itype > Ntypes)) {
                log_msg(2, "Bad type code %d for var %d", itype, i + 1);
                indx = -5;
                break;
            }
            itype = itype - 1; /*  Convert types to start at 0  */
            CurVType = CurAttr->vtype = Types + itype;
            CurAttr->type = itype;

            /*    Make the vaux block  */
            vaux = (char *)alloc_blocks(3, CurVType->attr_aux_size);
            if (!vaux) {
                log_msg(2, "Cant make auxilliary var block");
                indx = -6;
                break;
            }
            CurAttr->vaux = vaux;

            /*    Read auxilliary information   */
            if ((*CurVType->read_aux_attr)(vaux)) {
                log_msg(2, "Error in reading auxilliary info var %d", i + 1);
                indx = -7;
                break;
            }
            /*    Set sizes of stats and basic blocks for var in classes */
            (*CurVType->set_sizes)(i);
        } /* End of variables loop */
        i = indx;
        break;
    }
    close_buffer();
    CurCtx.buffer = CurSource;
    return (indx);
}

/*    ---------------------  load_sample -------------------------  */
/*    To open a sample file and read in all about the sample.
    Returns index of new sample in samples array   */
int load_sample(const char *fname) {

    int i, n;
    int kread;
    int caseid;
    Buf bufst, *buf;
    Context oldctx;
    char *saux, *field, vstnam[80], sampname[80];
    size_t rec_len;
    Sample *sample;
    SVinst *var_list;

    memcpy(&oldctx, &CurCtx, sizeof(Context));
    buf = &bufst;
    strcpy(buf->cname, fname);
    CurCtx.buffer = buf;
    if (open_buffer()) {
        log_msg(2, "Cannot open sample file %s", buf->cname);
        i = -2;
        goto error;
    }

    /*    Begin to read sample file. First entry is its name   */
    new_line();
    kread = read_str(sampname, 1);
    if (kread < 0) {
        log_msg(2, "Error in name of sample");
        i = -9;
        goto error;
    }
    /*    See if sample already loaded  */
    if (find_sample(sampname, 0) >= 0) {
        log_msg(2, "Sample %s already present", sampname);
        i = -8;
        goto error;
    }
    /*    Next line should be the vset name  */
    new_line();
    kread = read_str(vstnam, 1);
    if (kread < 0) {
        log_msg(2, "Error in name of variableset");
        i = -9;
        goto error;
    }
    /*    Check vset known  */
    kread = find_vset(vstnam);
    if (kread < 0) {
        log_msg(2, "Variableset %s unknown", vstnam);
        i = -8;
        goto error;
    }
    CurCtx.vset = VarSets[kread];

    /*    Find a vacant sample slot  */
    for (i = 0; i < MAX_SAMPLES; i++) {
        if (Samples[i] == 0)
            goto gotit;
    }
    goto nospace;

gotit:
    sample = Samples[i] = (Sample *)malloc(sizeof(Sample));
    if (!sample)
        goto nospace;
    CurCtx.sample = sample;
    CurCtx.sample->blocks = 0;
    CurCtx.sample->id = i;
    strcpy(CurCtx.sample->filename, buf->cname);
    strcpy(CurCtx.sample->name, sampname);
    /*    Set variable-set name in sample  */
    strcpy(CurCtx.sample->vset_name, CurCtx.vset->name);
    /*    Num of variables   */
    NumVars = CurCtx.vset->num_vars;
    CurAttrList = CurCtx.vset->attrs;

    /*    Make a vec of nv SVinst blocks  */
    var_list = (SVinst *)alloc_blocks(0, NumVars * sizeof(SVinst));
    if (!var_list) {
        log_msg(0, "Cannot allocate memory for variables blocks");
        i = -3;
        goto error;
    }
    CurCtx.sample->variables = var_list;

    /*    Read in the info for each variable into svars   */
    for (i = 0; i < NumVars; i++) {
        var_list[i].id = -1;
        var_list[i].saux = 0;
        var_list[i].offset = 0;
        var_list[i].nval = 0;
    }
    rec_len = 1 + sizeof(int); /* active flag and ident  */
    for (i = 0; i < NumVars; i++) {
        CurVar = var_list + i;
        CurVar->id = i;
        CurAttr = CurAttrList + i;
        CurVType = CurAttr->vtype;

        /*    Make the saux block  */
        saux = (char *)alloc_blocks(0, CurVType->smpl_aux_size);
        if (!saux) {
            log_msg(0, "Cant make auxilliary var block");
            i = -6;
            goto error;
        }
        CurVar->saux = saux;

        /*    Read auxilliary information   */
        if ((*CurVType->read_aux_smpl)(saux)) {
            log_msg(0, "Error in reading auxilliary info var %d", i + 1);
            i = -7;
            goto error;
        }

        /*    Set the offset of the (missing, value) pair  */
        CurVar->offset = rec_len;
        rec_len += (1 + CurVType->data_size); /* missing flag and value */
    }                                           /* End of variables loop */

    /*    Now attempt to read in the data. The first item is the number of cases*/
    new_line();
    kread = read_int(&NumCases, 1);
    if (kread) {
        log_msg(0, "Cant read number of cases");
        i = -11;
        goto error;
    }
    CurCtx.sample->num_cases = NumCases;
    CurCtx.sample->num_active = 0;
    /*    Make a vector of nc records each of size reclen  */
    CurRecords = CurCtx.sample->records = field = (char *)alloc_blocks(0, NumCases * rec_len);
    if (!field) {
        log_msg(0, "No space for data");
        i = -8;
        goto error;
    }
    CurCtx.sample->record_length = rec_len;

    /*    Read in the data cases, each preceded by an active flag and ident   */
    for (n = 0; n < NumCases; n++) {
        new_line();
        kread = read_int(&caseid, 1);
        if (kread) {
            log_msg(0, "Cant read ident, case %d", n + 1);
            i = -12;
            goto error;
        }
        /*    If ident negative, so clear active */
        if (caseid < 0) {
            caseid = -caseid;
            *field = 0;
        } else {
            *field = 1;
            CurCtx.sample->num_active++;
        }
        field++;
        memcpy(field, &caseid, sizeof(int));
        field += sizeof(int);
        /*    Posn now points to where the (missing, val) pair for the
        attribute should start.  */
        for (i = 0; i < NumVars; i++) {
            CurVar = var_list + i;
            CurAttr = CurAttrList + i;
            CurVType = CurAttr->vtype;
            kread = (*CurVType->read_datum)(field + 1, i);
            if (kread < 0) {
                log_msg(0, "Data error case %d var %d", n + 1, i + 1);
                swallow();
            }
            if (kread)
                *field = 1; /* Data missing */
            else {
                *field = 0;
                CurVar->nval++;
            }
            field += (CurVType->data_size + 1);
        }
    }
    log_msg(0, "Number of active cases = %d", CurCtx.sample->num_active);
    close_buffer();
    CurCtx.buffer = CurSource;
    if (sort_sample(CurCtx.sample)) {
        log_msg(0, "Sort failure on sample");
        return (-1);
    }
    return (CurCtx.sample->id);

nospace:
    log_msg(0, "No space for another sample");
    i = -1;
error:
    close_buffer();
    memcpy(&CurCtx, &oldctx, sizeof(Context));
    return (i);
}

/*    -----------------------  find_sample  ------------------------  */
/*    To find sample id given its name. Returns -1 if unknown  */
/*    'expect' shows if sample expected to be present  */
int find_sample(char *nam, int expect) {
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
int find_vset(char *nam) {
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
void swap_records(char *p1, char *p2, int ll) {
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
void quick_sort_sample(char *bot, int nn, int len) {
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
int sort_sample(Sample *smpl) {
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
int find_sample_index(int id) {
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

/*    ------------------------  item_list  -------------------   */
/*    Records best class and score for all items in a sample.
 */
int item_list(char *tlstname) {
    FILE *tlst;
    int nn, dadser, i, bc, bl, tid;
    double bw, bs;
    Class *clp, *root;
    char *record;

    /*    Check we have an attched sample and model  */
    if (!CurCtx.popln)
        return (-1);
    if (!CurCtx.sample)
        return (-2);
    set_population();
    if (!CurCtx.sample->num_cases)
        return (-3);

    root = CurCtx.popln->classes[CurCtx.popln->root];

    /*    Open a file  */
    tlst = fopen(tlstname, "w");
    if (!tlst)
        return (-4);
    /*    Output a tree list in a primitive form  */
    clp = root;

treeloop:
    if (clp->type == Sub)
        goto nextcl1;
    fprintf(tlst, "%8d", clp->serial >> 2);
    if (clp->dad_id >= 0)
        dadser = CurCtx.popln->classes[clp->dad_id]->serial;
    else
        dadser = -4;
    fprintf(tlst, "%8d\n", dadser >> 2);
nextcl1:
    next_class(&clp);
    if (clp)
        goto treeloop;
    fprintf(tlst, "0 0\n");

    find_all(Dad + Leaf);

    for (nn = 0; nn < CurCtx.sample->num_cases; nn++) {
        do_case(nn, Leaf + Dad, 0);
        bl = bc = -1;
        bw = 0.0;
        bs = CurCtx.sample->num_cases + 1;
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

        record = CurRecords + nn * CurCtx.sample->record_length;
        memcpy(&tid, record + 1, sizeof(int));
        fprintf(tlst, "%8d %6d %6d  %6.3f\n", tid, Sons[bc]->serial >> 2, Sons[bl]->serial >> 2, ScoreRscale * Sons[bl]->factor_scores[nn]);
    }

    fclose(tlst);
    return (0);
}
