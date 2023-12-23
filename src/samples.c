#define NOTGLOB 1
#define SAMPLES 1
#include "glob.h"

/*	----------------------  printdatum  -------------------------  */
/*	To print datum for variable i, case n, in sample     */
void print_var_datum(int i, int n) {
    Sample *samp;
    VSetVar *avi;
    SampleVar *svi;
    VarType *vtp;
    char *field;

    samp = CurCtx.sample;
    svi = &CurCtx.sample->variables[i];
    avi = &CurCtx.vset->variables[i];
    vtp = avi->vtype;
    field = (char *)samp->records;
    field += n * samp->record_length;
    field += svi->offset;
    /*	Test for missing  */
    if (*field == 1) {
        printf("%9s ", "=====");
        return;
    }
    field += 1;
    (*vtp->print_datum)(field);
    return;
}

void peek_data() {
    int caseid;
    char *field;
    VSetVar *vset_var;

    // print header
    printf("%9s ", "id");
    for (int i = 0; i < CurCtx.vset->length; i++) {
        if ((CurCtx.vset->length > 10) && (i > 5) && (i < CurCtx.vset->length - 5))
            continue;
        vset_var = &CurCtx.vset->variables[i];
        if ((i == 5) && (CurCtx.vset->length > 10)) {
            printf("%9s ", "...");
        } else {
            printf("%9s ", vset_var->name);
        }
    }
    printf("\n");

    for (int n = 0; n < CurCtx.sample->num_cases; n++) {
        if ((CurCtx.sample->num_cases > 20) && (n > 5) && (n < CurCtx.sample->num_cases - 5))
            continue;
        field = CurCtx.sample->records + n * CurCtx.sample->record_length + 1;
        caseid = *(int *)(field);
        if ((n == 5) && (CurCtx.sample->num_cases > 20)) {
            printf("%9s ", "...");
            for (int i = 0; i < CurCtx.vset->length; i++) {
                if ((CurCtx.vset->length > 10) && (i > 5) && (i < CurCtx.vset->length - 5))
                    continue;
                printf("%9s ", "...");
            }
        } else {
            printf("%9d ", caseid);
            for (int i = 0; i < CurCtx.vset->length; i++) {
                if ((CurCtx.vset->length > 10) && (i > 5) && (i < CurCtx.vset->length - 5))
                    continue;
                if ((i == 5) && (CurCtx.vset->length > 10)) {
                    printf("%9s ", "...");
                } else {
                    print_var_datum(i, n);
                }
            }
        }
        printf("\n");
    }

    printf("[%d items x %d attributes]\n\n", CurCtx.sample->num_cases, CurCtx.vset->length);
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
    int num_vars;

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
    kread = read_int(&num_vars, 1);
    if (kread) {
        printf("Cant read number of variables\n");
        i = -4;
        goto error;
    }
    CurCtx.vset->length = num_vars;
    CurCtx.vset->num_active = CurCtx.vset->length;

    /*	Make a vec of nv VSetVar blocks  */
    vset_var_list = (VSetVar *)alloc_blocks(3, num_vars * sizeof(VSetVar));
    if (!vset_var_list) {
        printf("Cannot allocate memory for variables blocks\n");
        i = -3;
        goto error;
    }
    CurCtx.vset->variables = vset_var_list;

    /*	Read in the info for each variable into vlist   */
    for (i = 0; i < CurCtx.vset->length; i++) {
        CurCtx.vset->variables[i].id = -1;
        CurCtx.vset->variables[i].vaux = 0;
    }
    for (i = 0; i < CurCtx.vset->length; i++) {
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

/// @brief Create an empty VSet and set it as the active one ready for adding attributes
/// @param name name of VSet
/// @param num_vars Number of attributes
/// @return index of new vset or negative error code
int create_vset(const char *name, int num_vars) {
    int found = -1;
    VSetVar *vset_var_list;

    for (int i = 0; i < MAX_VSETS; i++) {
        if (!VarSets[i]) {
            found = i;
            break;
        }
    }
    if (found >= 0) {
        CurCtx.vset = VarSets[found] = (VarSet *)malloc(sizeof(VarSet));
        if (!CurCtx.vset) {
            return error_value("Cannot allocate memory for VariableSet", -1);
        }
        CurCtx.vset->id = found;
        CurCtx.vset->variables = 0;
        CurCtx.vset->blocks = 0;
        strcpy(CurCtx.vset->name, name);

        CurCtx.vset->length = num_vars;
        CurCtx.vset->num_active = CurCtx.vset->length;

        /*	Make a vec of nv VSetVar blocks  */
        vset_var_list = (VSetVar *)alloc_blocks(3, num_vars * sizeof(VSetVar));
        if (!vset_var_list) {
            return error_value("Cannot allocate memory for variables blocks", -3);
        }
        CurCtx.vset->variables = vset_var_list;

        /*	initialize the variables   */
        for (int i = 0; i < CurCtx.vset->length; i++) {
            CurCtx.vset->variables[i].id = -1;
            CurCtx.vset->variables[i].vaux = 0;
        }
        return found;
    } else {
        return error_value("No space for VariableSet", -1);
    }
}

/// @brief Add a new attribute to current newly created VSet
/// @param index attribute index
/// @param name Name of attribute
/// @param itype Type of attribute 1 = real, 2 = Multi-State, 3 = Binary, 4 = Von Mises
/// @param aux Auxillary information, ignored by types 1, 3, and 4. Number of states for type 2.
/// @return value of index if successful otherwise a negative error code.
int add_attribute(int index, const char *name, int itype, int aux) {
    char *vaux;
    VarType *vtype;
    VSetVar *vset_var;

    if ((index < CurCtx.vset->length) && (itype > 0) && (itype <= NTypes)) {
        vset_var = &CurCtx.vset->variables[index];
        vset_var->id = index;
        strcpy(vset_var->name, name);
        vset_var->vtype = &Types[itype];
        vset_var->inactive = 0;

        itype = itype - 1; /*  Convert types to start at 0  */
        vtype = vset_var->vtype = &Types[itype];
        vset_var->type = itype;

        /*	Make the vaux block  */
        vaux = (char *)alloc_blocks(3, vtype->attr_aux_size);
        if (!vaux) {
            return error_value("Cant make auxilliary var block", -6);
        }
        vset_var->vaux = vaux;

        /*	Set auxilliary information   */
        if ((*vtype->set_aux_attr)(vaux, aux)) {
            return error_value("Error in setting auxilliary info", -7);
        }
        /*	Set sizes of stats and basic blocks for var in classes */
        (*vtype->set_sizes)(index);
    }
    return index;
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
    int num_cases, record_length;
    char *field;

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

    /*	Make a vec of nv SampleVar blocks  */
    smpl_var_list = (SampleVar *)alloc_blocks(0, CurCtx.vset->length * sizeof(SampleVar));
    if (!smpl_var_list) {
        printf("Cannot allocate memory for variables blocks\n");
        i = -3;
        goto error;
    }
    CurCtx.sample->variables = smpl_var_list;

    /*	Read in the info for each variable into svars   */
    for (i = 0; i < CurCtx.vset->length; i++) {
        smpl_var_list[i].id = -1;
        smpl_var_list[i].saux = 0;
        smpl_var_list[i].offset = 0;
        smpl_var_list[i].nval = 0;
    }
    record_length = 1 + sizeof(int); /* active flag and ident  */
    for (i = 0; i < CurCtx.vset->length; i++) {
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
        smpl_var->offset = record_length;
        record_length += (1 + vtype->data_size); /* missing flag and value */
    }                                            /* End of variables loop */

    /*	Now attempt to read in the data. The first item is the number of cases*/
    new_line();
    kread = read_int(&num_cases, 1);
    if (kread) {
        printf("Cant read number of cases\n");
        i = -11;
        goto error;
    }
    CurCtx.sample->num_cases = num_cases;
    CurCtx.sample->num_active = 0;
    /*	Make a vector of nc records each of size reclen  */
    CurCtx.sample->records = field = (char *)alloc_blocks(0, num_cases * record_length);
    if (!field) {
        printf("No space for data\n");
        i = -8;
        goto error;
    }
    CurCtx.sample->record_length = record_length;

    /*	Read in the data cases, each preceded by an active flag and ident   */
    for (n = 0; n < num_cases; n++) {
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
            *field = 0;
        } else {
            *field = 1;
            CurCtx.sample->num_active++;
        }
        field++;
        memcpy(field, &caseid, sizeof(int));
        field += sizeof(int);
        /*	Posn now points to where the (missing, val) pair for the
        attribute should start.  */
        for (i = 0; i < CurCtx.vset->length; i++) {
            smpl_var = &CurCtx.sample->variables[i];
            vset_var = &CurCtx.vset->variables[i];
            vtype = vset_var->vtype;
            kread = (*vtype->read_datum)(field + 1, i);
            if (kread < 0) {
                printf("Data error case %d var %d\n", n + 1, i + 1);
                swallow();
            }
            if (kread)
                *field = 1; /* Data missing */
            else {
                *field = 0;
                smpl_var->nval++;
            }
            field += (vtype->data_size + 1);
        }
        CurCtx.sample->num_added++;
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

/// @brief Create an empty sample ready for loading data
/// @param name Name of sample
/// @param size Number of items in sample
/// @param units Array of integers one for each attribute 0 = radians, 1 = degrees, ignored by other types
/// @param precision Array of doubles, one for each attribute, used by reals and von-mises only
/// @return index of sample or negative error code
int create_sample(char *name, int size, int *units, double *precision) {
    int found = -1, out = 0;
    Context oldctx;
    char *saux;
    VSetVar *vset_var;
    VarType *vtype;
    SampleVar *smpl_var, *smpl_var_list;
    int record_length;
    char *field;

    // backup context
    memcpy(&oldctx, &CurCtx, sizeof(Context));

    if (find_sample(name, 0) >= 0) {
        log_msg(2, "Sample with name '%s' already present", name);
        out = -8;
    } else {
        //	Find a vacant sample slot
        for (int i = 0; i < MAX_SAMPLES; i++) {
            if (Samples[i] == 0) {
                found = i;
                break;
            }
        }
        do {
            if (found < 0) {
                log_msg(2, "No space for data");
                out = -1;
                break;
            }
            CurCtx.sample = Samples[found] = (Sample *)malloc(sizeof(Sample));
            if (!CurCtx.sample) {
                log_msg(2, "No space for data");
                out = -1;
                break;
            }
            CurCtx.sample->blocks = 0;
            CurCtx.sample->id = found;
            strcpy(CurCtx.sample->filename, "???");
            strcpy(CurCtx.sample->name, name);
            strcpy(CurCtx.sample->vset_name, CurCtx.vset->name); /*	Set variable-set name in sample  */
            smpl_var_list = (SampleVar *)alloc_blocks(0, CurCtx.vset->length * sizeof(SampleVar));
            if (!smpl_var_list) {
                log_msg(2, "Cannot allocate memory for variables blocks");
                out = -3;
                break;
            }
            CurCtx.sample->variables = smpl_var_list;

            // initialize sample_var_list
            record_length = 1 + sizeof(int); /* active flag and ident  */
            for (int i = 0; i < CurCtx.vset->length; i++) {
                smpl_var_list[i].id = -1;
                smpl_var_list[i].saux = 0;
                smpl_var_list[i].offset = 0;
                smpl_var_list[i].nval = 0;

                smpl_var = &CurCtx.sample->variables[i];
                vset_var = &CurCtx.vset->variables[i];
                smpl_var->id = i;
                vtype = vset_var->vtype;

                saux = (char *)alloc_blocks(0, vtype->smpl_aux_size); /*	Make the saux block  */
                if (!saux) {
                    log_msg(2, "Cant make auxilliary var block");
                    out = -6;
                    break;
                }
                smpl_var->saux = saux;

                if ((*vtype->set_aux_smpl)(saux, units[i], precision[i])) {
                    log_msg(2, "Error setting auxilliary info var %d\n", i + 1);
                    out = -7;
                    break;
                }
                smpl_var->offset = record_length;
                record_length += (1 + vtype->data_size); /* missing flag and value */
            }

            if (out < 0) {
                break;
            }
            CurCtx.sample->num_cases = size;
            CurCtx.sample->num_added = 0;
            CurCtx.sample->num_active = 0;
            CurCtx.sample->records = field = (char *)alloc_blocks(0, size * record_length);
            if (!field) {
                log_msg(2, "No space for data");
                out = -8;
                break;
            }
            CurCtx.sample->record_length = record_length;
        } while (0);
    }
    if (out < 0) {
        log_msg(2, "Creating Sample Failed!");
        memcpy(&CurCtx, &oldctx, sizeof(Context));
        return (out);
    }
    return found;
}

/// @brief Add a record to the current newly created sample
/// @param bytes
/// @return
int add_record(int index, char *bytes) {
    int caseid = index, kread;
    VSetVar *vset_var;
    VarType *vtype;
    SampleVar *smpl_var;
    int offset = 0;

    char *field = (char *)CurCtx.sample->records + CurCtx.sample->num_added * CurCtx.sample->record_length;
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
    /*	Posn now points to where the (missing, val) pair for the attribute should start.  */
    for (int i = 0; i < CurCtx.vset->length; i++) {
        smpl_var = &CurCtx.sample->variables[i];
        vset_var = &CurCtx.vset->variables[i];
        vtype = vset_var->vtype;
        kread = (*vtype->set_datum)(field + 1, i, bytes + offset);
        offset += abs(kread);
        if (kread < 0) {
            *field = 1;
        } else {
            smpl_var->nval++;
            *field = 0;
        }
        //(*vtype->print_datum)(field + 1);
        field += (vtype->data_size + 1);
    }
    CurCtx.sample->num_added++;
    return CurCtx.sample->num_added;
}

int sort_current_sample() {
    if (sort_sample(CurCtx.sample)) {
        printf("Sort failure on sample\n");
        return (-1);
    }
    return 0;
}

/*	-----------------------  vname2id  ------------------------  */
/*	To find vset id given its name. Returns -1 if unknown  */
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

/*	------------------  qssamp  ----------------------------  */
/*	To quicksort a sample into increasing ident order   */

/*	Record swapper  */
void swaprec(char *p1, char *p2, int ll) {
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
void qssamp1(char *bot, int nn, int len) {
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

int sort_sample(Sample *samp) {
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

/*	------------------------  thinglist  -------------------   */
/*	Records best class and score for all things in a sample.
 */
int item_list(char *tlstname) {
    FILE *tlst;
    int nn, dadser, i, bc, tid, bl, num_son;
    double bw, bs;
    char *record;
    Class *cls;
    Population *popln = CurCtx.popln;
    Class *root = CurCtx.popln->classes[CurCtx.popln->root];

    /*	Check we have an attched sample and model  */
    if (!CurCtx.popln)
        return (-1);
    if (!CurCtx.sample)
        return (-2);
    if (!CurCtx.sample->num_cases)
        return (-3);

    /*	Open a file  */
    tlst = fopen(tlstname, "w");
    if (!tlst)
        return (-4);

    /*	Output a tree list in a primitive form  */
    cls = root;
    while (cls) {
        if (cls->type != Sub) {
            fprintf(tlst, "%8d", cls->serial >> 2);
            if (cls->dad_id >= 0)
                dadser = popln->classes[cls->dad_id]->serial;
            else
                dadser = -4;
            fprintf(tlst, "%8d\n", dadser >> 2);
        }
        next_class(&cls);
    }

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
        record = CurCtx.sample->records + nn * CurCtx.sample->record_length;
        memcpy(&tid, record + 1, sizeof(int));
        fprintf(tlst, "%8d %6d %6d  %6.3f\n", tid, Sons[bc]->serial >> 2, Sons[bl]->serial >> 2, ScoreRScale * Sons[bl]->factor_scores[nn]);
    }

    fclose(tlst);
    return (0);
}

int get_assignments(int *ids, int *prim_cls, double *prim_probs, int *sec_cls, double *sec_probs) {
    int nn, i, best_cls, best_leaf, next_leaf, num_son;
    double best_weight, next_weight;
    char *record;
    Class *cls;

    /*	Check we have an attched sample and model  */
    if (!CurCtx.popln)
        return (-1);
    if (!CurCtx.sample)
        return (-2);
    if (!CurCtx.sample->num_cases)
        return (-3);

    num_son = find_all(Dad + Leaf);
    for (nn = 0; nn < CurCtx.sample->num_cases; nn++) {
        do_case(nn, Leaf + Dad, 0, num_son);
        best_leaf = next_leaf = best_cls = -1;
        best_weight = next_weight = 0.0;

        record = CurCtx.sample->records + nn * CurCtx.sample->record_length;
        memcpy(&ids[nn], record + 1, sizeof(int));
        for (i = 0; i < num_son; i++) {
            cls = Sons[i];
            if ((cls->type == Leaf) && (cls->case_weight > best_weight)) {
                next_leaf = best_leaf;
                best_leaf = i;
                best_weight = cls->case_weight;
            }
        }
        if ((next_leaf >= 0) && (Sons[next_leaf]->case_weight > 1e-3)) {
            prim_cls[nn] = Sons[best_leaf]->serial;
            prim_probs[nn] = Sons[best_leaf]->case_weight;
            sec_cls[nn] = Sons[next_leaf]->serial;
            sec_probs[nn] = Sons[next_leaf]->case_weight;
        } else {
            prim_cls[nn] = Sons[best_leaf]->serial;
            prim_probs[nn] = Sons[best_leaf]->case_weight;
            sec_cls[nn] = -1;
            sec_probs[nn] = 0.0;
        }
    }
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