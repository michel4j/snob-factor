#define NOTGLOB 1
#define SAMPLES 1
#include "snob.h"

/**
 * @brief To print datum for variable i, case n, in sample
 * @param ctx Pointer to the Snob context.
 * @param i
 * @param n
 */
void print_var_datum(SnobContext *ctx, int i, int n) {
    Sample *samp;
    VSetVar *avi;
    SampleVar *svi;
    VarType *vtp;
    char *field;

    samp = ctx->state.sample;
    svi = &ctx->state.sample->variables[i];
    avi = &ctx->state.vset->variables[i];
    vtp = avi->vtype;
    field = (char *)samp->records;
    field += n * samp->record_length;
    field += svi->offset;
    //	Test for missing
    if (*field == 1) {
        printf("%9s ", "=====");
        return;
    }
    field += 1;
    (*vtp->print_datum)(ctx, field);
    return;
}

void peek_data(SnobContext *ctx) {
    int caseid;
    char *field;
    VSetVar *vset_var;

    // print header
    printf("%9s ", "id");
    for (int i = 0; i < ctx->state.vset->length; i++) {
        if ((ctx->state.vset->length > 10) && (i > 5) && (i < ctx->state.vset->length - 5))
            continue;
        vset_var = &ctx->state.vset->variables[i];
        if ((i == 5) && (ctx->state.vset->length > 10)) {
            printf("%9s ", "...");
        } else {
            printf("%9s ", vset_var->name);
        }
    }
    printf("\n");

    // print types
    printf("%9s ", "type");
    for (int i = 0; i < ctx->state.vset->length; i++) {
        if ((ctx->state.vset->length > 10) && (i > 5) && (i < ctx->state.vset->length - 5))
            continue;
        if ((i == 5) && (ctx->state.vset->length > 10)) {
            printf("%9s ", "...");
        } else {
            printf("%9d ", ctx->state.vset->variables[i].vtype->id + 1);
        }
    }
    printf("\n");

    // print units
    printf("%9s ", "units");
    for (int i = 0; i < ctx->state.vset->length; i++) {
        if ((ctx->state.vset->length > 10) && (i > 5) && (i < ctx->state.vset->length - 5))
            continue;
        vset_var = &ctx->state.vset->variables[i];
        if ((i == 5) && (ctx->state.vset->length > 10)) {
            printf("%9s ", "...");
        } else {
            printf("%9d ", vset_var->vtype->get_unit(ctx, i));
        }
    }
    printf("\n");

    for (int n = 0; n < ctx->state.sample->num_cases; n++) {
        if ((ctx->state.sample->num_cases > 20) && (n > 5) && (n < ctx->state.sample->num_cases - 5))
            continue;
        field = ctx->state.sample->records + n * ctx->state.sample->record_length + 1;
        caseid = *(int *)(field);
        if ((n == 5) && (ctx->state.sample->num_cases > 20)) {
            printf("%9s ", "...");
            for (int i = 0; i < ctx->state.vset->length; i++) {
                if ((ctx->state.vset->length > 10) && (i > 5) && (i < ctx->state.vset->length - 5))
                    continue;
                printf("%9s ", "...");
            }
        } else {
            printf("%9d ", caseid);
            for (int i = 0; i < ctx->state.vset->length; i++) {
                if ((ctx->state.vset->length > 10) && (i > 5) && (i < ctx->state.vset->length - 5))
                    continue;
                if ((i == 5) && (ctx->state.vset->length > 10)) {
                    printf("%9s ", "...");
                } else {
                    print_var_datum(ctx, i, n);
                }
            }
        }
        printf("\n");
    }

    printf("[%d items x %d attributes]\n\n", ctx->state.sample->num_cases, ctx->state.vset->length);
}
/**
 * @brief To read in a vset from a file. Returns index of vset
 * Returns negative if fail
 * @param ctx Pointer to the Snob context.
 */

int read_vset(SnobContext *ctx) {
    char filename[80];
    int kread;

    printf("Enter variable-set file name:\n");
    kread = read_str(ctx, filename, 1);
    if (kread < 0) {
        log_msg(ctx, 2, "Error in name of variable-set file");
        return (-2);
    } else {
        return load_vset(ctx, filename);
    }
}

int load_vset(SnobContext *ctx, const char *filename) {

    int i, itype, indx;
    int kread;
    Buffer bufst, *buf;
    char *vaux;
    VarType *vtype;
    VSetVar *vset_var, *vset_var_list;
    int num_vars;
    int out = 0;

    buf = &bufst;
    indx = -1;
    for (i = 0; i < MAX_VSETS; i++) {
        if (!ctx->var_sets[i]) {
            indx = i;
            break;
        }
    }

    if (indx < 0) {
        printf("No space for VariableSet\n");
        return -10;
    }

    ctx->state.vset = ctx->var_sets[indx] = (VarSet *)malloc(sizeof(VarSet));
    if (!ctx->state.vset) {
        printf("No space for VariableSet\n");
        return -10;
    }

    ctx->state.vset->id = indx;
    ctx->state.vset->variables = 0;
    ctx->state.vset->blocks = 0;
    strcpy(ctx->state.vset->filename, filename);
    strcpy(buf->cname, ctx->state.vset->filename);
    ctx->state.buffer = buf;

    do {
        if (open_buffser(ctx)) {
            printf("Cant open variable-set file %s\n", ctx->state.vset->filename);
            out = -2;
            break;
        }

        //	Begin to read variable-set file. First entry is its name
        new_line(ctx);
        kread = read_str(ctx, ctx->state.vset->name, 1);
        if (kread < 0) {
            printf("Error in name of variable-set\n");
            out = -9;
            break;
        }
        //	Num of variables
        new_line(ctx);
        kread = read_int(ctx, &num_vars, 1);
        if (kread) {
            printf("Cant read number of variables\n");
            out = -4;
            break;
        }
        ctx->state.vset->length = num_vars;
        ctx->state.vset->num_active = ctx->state.vset->length;

        //	Make a vec of nv VSetVar blocks
        vset_var_list = (VSetVar *)alloc_blocks(ctx, 3, num_vars * sizeof(VSetVar));
        if (!vset_var_list) {
            printf("Cannot allocate memory for variables blocks\n");
            out = -3;
            break;
        }
        ctx->state.vset->variables = vset_var_list;

        //	Read in the info for each variable into vlist
        for (i = 0; i < ctx->state.vset->length; i++) {
            ctx->state.vset->variables[i].id = -1;
            ctx->state.vset->variables[i].vaux = 0;
        }
        for (i = 0; i < ctx->state.vset->length; i++) {
            vset_var = &ctx->state.vset->variables[i];
            vset_var->id = i;

            //	Read name
            new_line(ctx);
            kread = read_str(ctx, vset_var->name, 1);
            if (kread < 0) {
                printf("Error in name of variable %d\n", i + 1);
                out = -10;
                break; // Break the for loop
            }

            //	Read type code. Negative means idle.
            kread = read_int(ctx, &itype, 1);
            if (kread) {
                printf("Cant read type code for var %d\n", i + 1);
                out = -5;
                break; // Break the for loop
            }
            vset_var->inactive = 0;
            if (itype < 0) {
                vset_var->inactive = 1;
                itype = -itype;
            }
            if ((itype < 1) || (itype > ctx->num_types)) {
                printf("Bad type code %d for var %d\n", itype, i + 1);
                out = -5;
                break; // Break the for loop
            }
            itype = itype - 1; //  Convert types to start at 0
            vtype = vset_var->vtype = &ctx->types[itype];
            vset_var->type = itype;

            //	Make the vaux block
            vaux = (char *)alloc_blocks(ctx, 3, vtype->attr_aux_size);
            if (!vaux) {
                printf("Cant make auxilliary var block\n");
                out = -6;
                break; // Break the for loop
            }
            vset_var->vaux = vaux;

            //	Read auxilliary information
            if ((*vtype->read_aux_attr)(ctx, vaux)) {
                printf("Error in reading auxilliary info var %d\n", i + 1);
                out = -7;
                break; // Break the for loop
            }
            //	Set sizes of stats and basic blocks for var in classes
            (*vtype->set_sizes)(ctx, i);
        } // End of variables loop

        if (out < 0)
            break; // Break the do-while loop if inner loop errored
        out = indx;
    } while (0);

    close_buffer(ctx);
    ctx->state.buffer = ctx->current_source;
    return out;
}

/// @brief Create an empty VSet and set it as the active one ready for adding
/// attributes
/// @param name name of VSet
/// @param num_vars Number of attributes
/// @return index of new vset or negative error code
int create_vset(SnobContext *ctx, const char *name, int num_vars) {
    int found = -1;
    VSetVar *vset_var_list;

    for (int i = 0; i < MAX_VSETS; i++) {
        if (!ctx->var_sets[i]) {
            found = i;
            break;
        }
    }
    if (found >= 0) {
        ctx->state.vset = ctx->var_sets[found] = (VarSet *)malloc(sizeof(VarSet));
        if (!ctx->state.vset) {
            return error_value(ctx, "Cannot allocate memory for VariableSet", -1);
        }
        ctx->state.vset->id = found;
        ctx->state.vset->variables = 0;
        ctx->state.vset->blocks = 0;
        strcpy(ctx->state.vset->name, name);

        ctx->state.vset->length = num_vars;
        ctx->state.vset->num_active = ctx->state.vset->length;

        //	Make a vec of nv VSetVar blocks
        vset_var_list = (VSetVar *)alloc_blocks(ctx, 3, num_vars * sizeof(VSetVar));
        if (!vset_var_list) {
            return error_value(ctx, "Cannot allocate memory for variables blocks", -3);
        }
        ctx->state.vset->variables = vset_var_list;

        //	initialize the variables
        for (int i = 0; i < ctx->state.vset->length; i++) {
            ctx->state.vset->variables[i].id = -1;
            ctx->state.vset->variables[i].vaux = 0;
        }
        return found;
    } else {
        return error_value(ctx, "No space for VariableSet", -1);
    }
}

/// @brief Add a new attribute to current newly created VSet
/// @param index attribute index
/// @param name Name of attribute
/// @param itype Type of attribute 1 = real, 2 = Multi-State, 3 = Binary, 4 =
/// Von Mises
/// @param aux Auxillary information, ignored by types 1, 3, and 4. Number of
/// states for type 2.
/// @return value of index if successful otherwise a negative error code.
int add_attribute(SnobContext *ctx, int index, const char *name, int itype, int aux) {
    char *vaux;
    VarType *vtype;
    VSetVar *vset_var;

    if ((index < ctx->state.vset->length) && (itype > 0) && (itype <= ctx->num_types)) {
        vset_var = &ctx->state.vset->variables[index];
        vset_var->id = index;
        strcpy(vset_var->name, name);
        vset_var->vtype = &ctx->types[itype];
        vset_var->inactive = 0;

        itype = itype - 1; //  Convert types to start at 0
        vtype = vset_var->vtype = &ctx->types[itype];
        vset_var->type = itype;

        //	Make the vaux block
        vaux = (char *)alloc_blocks(ctx, 3, vtype->attr_aux_size);
        if (!vaux) {
            return error_value(ctx, "Cant make auxilliary var block", -6);
        }
        vset_var->vaux = vaux;

        //	Set auxilliary information
        if ((*vtype->set_aux_attr)(ctx, vaux, aux)) {
            return error_value(ctx, "Error in setting auxilliary info", -7);
        }
        //	Set sizes of stats and basic blocks for var in classes
        (*vtype->set_sizes)(ctx, index);
    }
    return index;
}

/**
 * @brief To open a sample file and read in all about the sample.
 * Returns index of new sample in samples array
 * @param ctx Pointer to the Snob context.
 * @param fname Name or filename string.
 */
int load_sample(SnobContext *ctx, const char *fname) {

    int i, n;
    int kread;
    int caseid;
    Buffer bufst, *buf;
    State oldctx;
    char *saux, vstnam[80], sampname[80];
    VSetVar *vset_var;
    VarType *vtype;
    SampleVar *smpl_var, *smpl_var_list;
    int num_cases, record_length;
    char *field;
    int out = 0;
    int indx = -1;

    memcpy(&oldctx, &ctx->state, sizeof(State));
    buf = &bufst;
    strcpy(buf->cname, fname);
    ctx->state.buffer = buf;

    do {
        if (open_buffser(ctx)) {
            printf("Cannot open sample file %s\n", buf->cname);
            out = -2;
            break;
        }

        //	Begin to read sample file. First entry is its name
        new_line(ctx);
        kread = read_str(ctx, sampname, 1);
        if (kread < 0) {
            printf("Error in name of sample\n");
            out = -9;
            break;
        }
        //	See if sample already loaded
        if (find_sample(ctx, sampname, 0) >= 0) {
            printf("Sample %s already present\n", sampname);
            out = -8;
            break;
        }
        //	Next line should be the vset name
        new_line(ctx);
        kread = read_str(ctx, vstnam, 1);
        if (kread < 0) {
            printf("Error in name of variableset\n");
            out = -9;
            break;
        }
        //	Check vset known
        kread = find_vset(ctx, vstnam);
        if (kread < 0) {
            printf("Variableset %s unknown\n", vstnam);
            out = -8;
            break;
        }
        ctx->state.vset = ctx->var_sets[kread];

        //	Find a vacant sample slot
        for (i = 0; i < MAX_SAMPLES; i++) {
            if (ctx->samples[i] == 0) {
                indx = i;
                break;
            }
        }
        if (indx < 0) {
            printf("No space for another sample\n");
            out = -1;
            break;
        }

        ctx->state.sample = ctx->samples[indx] = (Sample *)malloc(sizeof(Sample));
        if (!ctx->state.sample) {
            printf("No space for another sample\n");
            out = -1;
            break;
        }

        ctx->state.sample->blocks = 0;
        ctx->state.sample->id = indx;
        strcpy(ctx->state.sample->filename, buf->cname);
        strcpy(ctx->state.sample->name, sampname);
        //	Set variable-set name in sample
        strcpy(ctx->state.sample->vset_name, ctx->state.vset->name);

        //	Make a vec of nv SampleVar blocks
        smpl_var_list = (SampleVar *)alloc_blocks(ctx, 0, ctx->state.vset->length * sizeof(SampleVar));
        if (!smpl_var_list) {
            printf("Cannot allocate memory for variables blocks\n");
            out = -3;
            break;
        }
        ctx->state.sample->variables = smpl_var_list;

        //	Read in the info for each variable into svars
        for (i = 0; i < ctx->state.vset->length; i++) {
            smpl_var_list[i].id = -1;
            smpl_var_list[i].saux = 0;
            smpl_var_list[i].offset = 0;
            smpl_var_list[i].nval = 0;
        }
        record_length = 1 + sizeof(int); // active flag and ident
        for (i = 0; i < ctx->state.vset->length; i++) {
            smpl_var = &ctx->state.sample->variables[i];
            vset_var = &ctx->state.vset->variables[i];
            smpl_var->id = i;
            vtype = vset_var->vtype;

            //	Make the saux block
            saux = (char *)alloc_blocks(ctx, 0, vtype->smpl_aux_size);
            if (!saux) {
                printf("Cant make auxilliary var block\n");
                out = -6;
                break;
            }
            smpl_var->saux = saux;

            //	Read auxilliary information
            if ((*vtype->read_aux_smpl)(ctx, saux)) {
                printf("Error in reading auxilliary info var %d\n", i + 1);
                out = -7;
                break;
            }

            //	Set the offset of the (missing, value) pair
            smpl_var->offset = record_length;
            record_length += (1 + vtype->data_size); // missing flag and value
        } // End of variables loop
        if (out < 0)
            break;

        //	Now attempt to read in the data. The first item is the number of cases
        new_line(ctx);
        kread = read_int(ctx, &num_cases, 1);
        if (kread) {
            printf("Cant read number of cases\n");
            out = -11;
            break;
        }
        ctx->state.sample->num_cases = num_cases;
        ctx->state.sample->num_active = 0;
        //	Make a vector of nc records each of size reclen
        ctx->state.sample->records = field = (char *)alloc_blocks(ctx, 0, num_cases * record_length);
        if (!field) {
            printf("No space for data\n");
            out = -8;
            break;
        }
        ctx->state.sample->record_length = record_length;

        //	Read in the data cases, each preceded by an active flag and ident
        for (n = 0; n < num_cases; n++) {
            new_line(ctx);
            kread = read_int(ctx, &caseid, 1);
            if (kread) {
                printf("Cant read ident, case %d\n", n + 1);
                out = -12;
                break;
            }
            //	If ident negative, so clear active
            if (caseid < 0) {
                caseid = -caseid;
                *field = 0;
            } else {
                *field = 1;
                ctx->state.sample->num_active++;
            }
            field++;
            memcpy(field, &caseid, sizeof(int));
            field += sizeof(int);
            /*	Posn now points to where the (missing, val) pair for the
            attribute should start.  */
            for (i = 0; i < ctx->state.vset->length; i++) {
                smpl_var = &ctx->state.sample->variables[i];
                vset_var = &ctx->state.vset->variables[i];
                vtype = vset_var->vtype;
                kread = (*vtype->read_datum)(ctx, field + 1, i);
                if (kread < 0) {
                    printf("Data error case %d var %d\n", n + 1, i + 1);
                    swallow(ctx);
                }
                if (kread)
                    *field = 1; // Data missing
                else {
                    *field = 0;
                    smpl_var->nval++;
                }
                field += (vtype->data_size + 1);
            }
            ctx->state.sample->num_added++;
        }
        if (out < 0)
            break;

        printf("Number of active cases = %d\n", ctx->state.sample->num_active);
        close_buffer(ctx);
        ctx->state.buffer = ctx->current_source;
        if (sort_sample(ctx, ctx->state.sample)) {
            printf("Sort failure on sample\n");
            return (-1);
        }
        return (ctx->state.sample->id);

    } while (0);

    close_buffer(ctx);
    memcpy(&ctx->state, &oldctx, sizeof(State));
    return (out);
}

/**
 * @brief To find sample id given its name. Returns -1 if unknown
 * 'expect' shows if sample expected to be present
 * @param ctx Pointer to the Snob context.
 * @param nam Name or filename string.
 * @param expect
 */
int find_sample(SnobContext *ctx, char *nam, int expect) {
    int i;
    int found = -1;

    for (i = 0; i < MAX_SAMPLES; i++) {
        if (ctx->samples[i]) {
            if (!strcmp(nam, ctx->samples[i]->name)) {
                found = i;
                break;
            }
        }
    }

    if ((found < 0) && expect)
        printf("Cannot find sample %s\n", nam);
    if ((found >= 0) && (!expect))
        printf("Sample %s already loaded\n", nam);
    return (found);
}

/// @brief Create an empty sample ready for loading data
/// @param name Name of sample
/// @param size Number of items in sample
/// @param units Array of integers one for each attribute 0 = radians, 1 =
/// degrees, ignored by other types
/// @param precision Array of doubles, one for each attribute, used by reals and
/// von-mises only
/// @return index of sample or negative error code
int create_sample(SnobContext *ctx, char *name, int size, int *units, double *precision) {
    int found = -1, out = 0;
    State oldctx;
    char *saux;
    VSetVar *vset_var;
    VarType *vtype;
    SampleVar *smpl_var, *smpl_var_list;
    int record_length;
    char *field;

    // backup context
    memcpy(&oldctx, &ctx->state, sizeof(State));

    if (find_sample(ctx, name, 0) >= 0) {
        log_msg(ctx, 2, "Sample with name '%s' already present", name);
        out = -8;
    } else {
        //	Find a vacant sample slot
        for (int i = 0; i < MAX_SAMPLES; i++) {
            if (ctx->samples[i] == 0) {
                found = i;
                break;
            }
        }
        do {
            if (found < 0) {
                log_msg(ctx, 2, "No space for data");
                out = -1;
                break;
            }
            ctx->state.sample = ctx->samples[found] = (Sample *)malloc(sizeof(Sample));
            if (!ctx->state.sample) {
                log_msg(ctx, 2, "No space for data");
                out = -1;
                break;
            }
            ctx->state.sample->blocks = 0;
            ctx->state.sample->id = found;
            strcpy(ctx->state.sample->filename, "???");
            strcpy(ctx->state.sample->name, name);
            strcpy(ctx->state.sample->vset_name, ctx->state.vset->name); //	Set variable-set name in sample
            smpl_var_list = (SampleVar *)alloc_blocks(ctx, 0, ctx->state.vset->length * sizeof(SampleVar));
            if (!smpl_var_list) {
                log_msg(ctx, 2, "Cannot allocate memory for variables blocks");
                out = -3;
                break;
            }
            ctx->state.sample->variables = smpl_var_list;

            // initialize sample_var_list
            record_length = 1 + sizeof(int); // active flag and ident
            for (int i = 0; i < ctx->state.vset->length; i++) {
                smpl_var_list[i].id = -1;
                smpl_var_list[i].saux = 0;
                smpl_var_list[i].offset = 0;
                smpl_var_list[i].nval = 0;

                smpl_var = &ctx->state.sample->variables[i];
                vset_var = &ctx->state.vset->variables[i];
                smpl_var->id = i;
                vtype = vset_var->vtype;

                saux = (char *)alloc_blocks(ctx, 0, vtype->smpl_aux_size); //	Make the saux block
                if (!saux) {
                    log_msg(ctx, 2, "Cant make auxilliary var block");
                    out = -6;
                    break;
                }
                smpl_var->saux = saux;

                if ((*vtype->set_aux_smpl)(ctx, saux, units[i], precision[i])) {
                    log_msg(ctx, 2, "Error setting auxilliary info var %d\n", i + 1);
                    out = -7;
                    break;
                }
                smpl_var->offset = record_length;
                record_length += (1 + vtype->data_size); // missing flag and value
            }

            if (out < 0) {
                break;
            }
            ctx->state.sample->num_cases = size;
            ctx->state.sample->num_added = 0;
            ctx->state.sample->num_active = 0;
            ctx->state.sample->records = field = (char *)alloc_blocks(ctx, 0, size * record_length);
            if (!field) {
                log_msg(ctx, 2, "No space for data");
                out = -8;
                break;
            }
            ctx->state.sample->record_length = record_length;
        } while (0);
    }
    if (out < 0) {
        log_msg(ctx, 2, "Creating Sample Failed!");
        memcpy(&ctx->state, &oldctx, sizeof(State));
        return (out);
    }
    return found;
}

/// @brief Add a record to the current newly created sample
/// @param bytes
/// @return
int add_record(SnobContext *ctx, int index, char *bytes) {
    int caseid = index, kread;
    VSetVar *vset_var;
    VarType *vtype;
    SampleVar *smpl_var;
    int offset = 0;

    char *field = (char *)ctx->state.sample->records + ctx->state.sample->num_added * ctx->state.sample->record_length;
    if (caseid < 0) {
        caseid = -caseid;
        *field = 0;
    } else {
        *field = 1;
        ctx->state.sample->num_active++;
    }
    field++;
    memcpy(field, &caseid, sizeof(int));
    field += sizeof(int);
    /*	Posn now points to where the (missing, val) pair for the attribute
     * should start.  */
    for (int i = 0; i < ctx->state.vset->length; i++) {
        smpl_var = &ctx->state.sample->variables[i];
        vset_var = &ctx->state.vset->variables[i];
        vtype = vset_var->vtype;
        kread = (*vtype->set_datum)(ctx, field + 1, i, bytes + offset);
        offset += abs(kread);
        if (kread < 0) {
            *field = 1;
        } else {
            smpl_var->nval++;
            *field = 0;
        }
        //(*vtype->print_datum)(ctx, field + 1);
        field += (vtype->data_size + 1);
    }
    ctx->state.sample->num_added++;
    return ctx->state.sample->num_added;
}

int sort_current_sample(SnobContext *ctx) {
    if (sort_sample(ctx, ctx->state.sample)) {
        printf("Sort failure on sample\n");
        return (-1);
    }
    return 0;
}

/**
 * @brief To find vset id given its name. Returns -1 if unknown
 * @param ctx Pointer to the Snob context.
 * @param nam Name or filename string.
 */
int find_vset(SnobContext *ctx, char *nam) {
    int i, ii;

    ii = -1;
    for (i = 0; i < MAX_VSETS; i++) {
        if (ctx->var_sets[i]) {
            if (!strcmp(nam, ctx->var_sets[i]->name))
                ii = i;
        }
    }
    if (ii < 0)
        printf("Cannot find variable set %s\n", nam);
    return (ii);
}

/**
 * @brief To quicksort a sample into increasing ident order
 * @param p1
 * @param p2
 * @param ll
 */

//	Record swapper
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

//	Recursive quicksort
void qssamp1(SnobContext *ctx, char *bot, int nn, int len) {
    char *top, *rp1, *rp2, *cen;
    int av, bv, cv, nb, nt;

    while (1) {
        if (nn < 2)
            return;

        if (nn < 6) {
            //	Do a short block by bubble
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
        }

        //	Pick a random central value
        nt = nn * rand_float(ctx);
        if (nt == nn)
            nt = nn / 2;
        cen = bot + nt * len;
        memcpy(&cv, cen + 1, sizeof(int));
        top = bot + (nn - 1) * len;
        rp1 = bot;
        rp2 = top;
        nt = nb = 0;

        while (rp2 >= rp1) {
            // Advance rp2 until we find a value < cv
            while (rp2 >= rp1) {
                memcpy(&av, rp2 + 1, sizeof(int));
                if (av >= cv) {
                    nt++;
                    rp2 -= len;
                } else {
                    break;
                }
            }

            if (rp2 < rp1)
                break;

            // Advance rp1 until we find a value >= cv
            while (rp2 >= rp1) {
                memcpy(&bv, rp1 + 1, sizeof(int));
                if (bv < cv) {
                    nb++;
                    rp1 += len;
                } else {
                    break;
                }
            }

            if (rp2 < rp1)
                break;

            //	Have av < cv, bv >= cv
            swaprec(rp1, rp2, len);
            nt++;
            rp2 -= len;
            nb++;
            rp1 += len;
        }

        //	Check that something has been placed in lower block.
        if (nb) {
            qssamp1(ctx, bot, nb, len);
            qssamp1(ctx, bot + nb * len, nt, len);
            return;
        }

        //	Nothing was less than cv, the value at cen, so swap it to bot
        swaprec(cen, bot, len);
        nn--;
        bot += len;
    }
}

int sort_sample(SnobContext *ctx, Sample *samp) {
    int nc, len;

    nc = samp->num_cases;
    printf("Begin sort of %d cases\n", nc);
    if (nc < 1) {
        printf("From qssamp: sample unattached.\n");
        return (-1);
    }
    len = samp->record_length;

    qssamp1(ctx, samp->records, nc, len);
    printf("Finished sort\n");
    return (0);
}

/**
 * @brief Given a item ident, returns index in sample, or -1 if not found
 * @param ctx Pointer to the Snob context.
 * @param id Identifier or serial number.
 */
int find_sample_index(SnobContext *ctx, int id) {
    int iu, il, ic, cid, len;
    char *recs;

    if ((!ctx->state.sample) || (ctx->state.sample->num_cases == 0)) {
        printf("No defined sample\n");
        return (-1);
    }
    recs = ctx->state.sample->records + 1;
    len = ctx->state.sample->record_length;
    iu = ctx->state.sample->num_cases;
    il = 0;

    while (1) {
        ic = (iu + il) >> 1;
        memcpy(&cid, recs + ic * len, sizeof(int));
        if (ic == il)
            break;
        if (cid > id) {
            iu = ic;
        } else if (cid < id) {
            il = ic;
        } else {
            break;
        }
    }

    return ((cid == id) ? ic : -1);
}

/**
 * @brief Records best class and score for all things in a sample.
 *
 * @param ctx Pointer to the Snob context.
 * @param tlstname Name or filename string.
 */
int item_list(SnobContext *ctx, char *tlstname) {
    FILE *tlst;
    int nn, dadser, i, bc, tid, bl, num_son;
    double bw, bs;
    char *record;
    Class *cls;
    Population *popln = ctx->state.popln;
    Class *root = ctx->state.popln->classes[ctx->state.popln->root];

    //	Check we have an attched sample and model
    if (!ctx->state.popln)
        return (-1);
    if (!ctx->state.sample)
        return (-2);
    if (!ctx->state.sample->num_cases)
        return (-3);

    //	Open a file
    tlst = fopen(tlstname, "w");
    if (!tlst)
        return (-4);

    //	Output a tree list in a primitive form
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
        next_class(ctx, &cls);
    }

    fprintf(tlst, "0 0\n");

    num_son = find_all(ctx, Dad + Leaf);

    for (nn = 0; nn < ctx->state.sample->num_cases; nn++) {
        do_case(ctx, nn, Leaf + Dad, 0, num_son);
        bl = bc = -1;
        bw = 0.0;
        bs = ctx->state.sample->num_cases + 1;
        for (i = 0; i < num_son; i++) {
            cls = ctx->sons[i];
            if ((cls->case_weight > 0.5) && (cls->weights_sum < bs)) {
                bc = i;
                bs = cls->weights_sum;
            }
            if ((cls->type == Leaf) && (cls->case_weight > bw)) {
                bl = i;
                bw = cls->case_weight;
            }
        }
        record = ctx->state.sample->records + nn * ctx->state.sample->record_length;
        memcpy(&tid, record + 1, sizeof(int));
        fprintf(tlst, "%8d %6d %6d  %6.3f\n", tid, ctx->sons[bc]->serial >> 2, ctx->sons[bl]->serial >> 2,
                ScoreRScale * ctx->sons[bl]->factor_scores[nn]);
    }

    fclose(tlst);
    return (0);
}

int get_assignments(SnobContext *ctx, int *ids, int *prim_cls, double *prim_probs, int *sec_cls, double *sec_probs) {
    int nn, i, best_cls, best_leaf, next_leaf, num_son;
    double best_weight, next_weight;
    char *record;
    Class *cls;

    //	Check we have an attched sample and model
    if (!ctx->state.popln)
        return (-1);
    if (!ctx->state.sample)
        return (-2);
    if (!ctx->state.sample->num_cases)
        return (-3);

    num_son = find_all(ctx, Dad + Leaf);
    for (nn = 0; nn < ctx->state.sample->num_cases; nn++) {
        do_case(ctx, nn, Leaf + Dad, 0, num_son);
        best_leaf = next_leaf = best_cls = -1;
        best_weight = next_weight = 0.0;

        record = ctx->state.sample->records + nn * ctx->state.sample->record_length;
        memcpy(&ids[nn], record + 1, sizeof(int));
        for (i = 0; i < num_son; i++) {
            cls = ctx->sons[i];
            if ((cls->type == Leaf) && (cls->case_weight > best_weight)) {
                next_leaf = best_leaf;
                best_leaf = i;
                best_weight = cls->case_weight;
            }
        }
        if ((next_leaf >= 0) && (ctx->sons[next_leaf]->case_weight > 1e-3)) {
            prim_cls[nn] = ctx->sons[best_leaf]->serial;
            prim_probs[nn] = ctx->sons[best_leaf]->case_weight;
            sec_cls[nn] = ctx->sons[next_leaf]->serial;
            sec_probs[nn] = ctx->sons[next_leaf]->case_weight;
        } else {
            prim_cls[nn] = ctx->sons[best_leaf]->serial;
            prim_probs[nn] = ctx->sons[best_leaf]->case_weight;
            sec_cls[nn] = -1;
            sec_probs[nn] = 0.0;
        }
    }
    return (0);
}

/**
 * @brief To destroy sample index sx
 * @param ctx Pointer to the Snob context.
 * @param sx
 */
void destroy_sample(SnobContext *ctx, int sx) {
    int prev;
    if (ctx->state.sample)
        prev = ctx->state.sample->id;
    else
        prev = -1;
    ctx->state.sample = ctx->samples[sx];
    if (!ctx->state.sample)
        return;

    free_blocks(ctx, 0);
    free(ctx->state.sample);
    ctx->samples[sx] = 0;
    ctx->state.sample = 0;
    if (sx == prev)
        return;
    if (prev < 0)
        return;
    ctx->state.sample = ctx->samples[prev];
}

/**
 * @brief To destroy vset index vx
 * @param ctx Pointer to the Snob context.
 * @param vx
 */
void destroy_vset(SnobContext *ctx, int vx) {
    int prev;
    if (ctx->state.vset)
        prev = ctx->state.vset->id;
    else
        prev = -1;
    ctx->state.vset = ctx->var_sets[vx];
    if (!ctx->state.vset)
        return;

    free_blocks(ctx, 3);
    free(ctx->state.vset);
    ctx->var_sets[vx] = 0;
    ctx->state.vset = 0;
    if (vx == prev)
        return;
    if (prev < 0)
        return;
    ctx->state.vset = ctx->var_sets[prev];
}