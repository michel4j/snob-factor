/*    ---------------------  MAIN FILE -----------------  */
#include "glob.h"
#include <omp.h>
#include <stdarg.h>
#include <time.h>

FILE *logfile;

char sparam[80];
int intparam;

/*    ---------------------  menu  ---------------------------  */
/*    To read a command word and return an integer code for it  */

#define NUM_COMMANDS 50
static char *commands[NUM_COMMANDS] = {"stop",          "doall",    "doleaves", "killclass", "killsons",   "prclass", "adjust",  "assign",
                                       "splitleaf",     "deldad",   "help",     "copypop",   "killpop",    "pickpop", "tree",    "sto",
                                       "file",          "readsamp", "flatten",  "dodads",    "samps",      "insdad",  "dogood",  "bestinsdad",
                                       "bestdeldad",    "rebuild",  "ranclass", "savepop",   "restorepop", "nosubs",  "binhier", "moveclass",
                                       "bestmoveclass", "pops",     "crosstab", "trymoves",  "thing",      "trep",    "select",  "zzzzzzzzzzzz"};

int cmd_index[NUM_COMMANDS], nkey, skk, nvkey;
char *spaces = "               ";

char *prmt[] = {"stop stops both sprompt and cnob",
                "doall <N> does N steps of top-down re-estimation and assignment",
                "doleaves <N> does N steps of re-estimation and assignment to leaf classes\n\
subclasses and tree structure usually unaffected. Bottom-up reassignment of\n\
weights to dad classes. Doleaves gives faster refinement than Doall, but no\n\
tree adjustment. Always followed by one Doall step.",

                "killclass <S> kills class serial S.  Descendants also die.",

                "killsons <S> kills all sons, grandsons etc. of class serial S. S becomes "
                "leaf",

                "prclass <S, N> prints properties of class S. If N > 0, prints parameters\n\
  If S = -1, prints all dads and leaves. If S = -2, includes subs.",

                "adjust controls which aspects of a population model will be changed.\n\
Follow it with one or more characters from:\n\
   'a'll, 's'cores, 't'ree, 'p'arams, each followed by '+' or '-'\n\
to turn adjustment on or off. Thus \"adjust t-p+\" disables tree structure\n\
adjustment and enables class parameter adjustment. 'p' does not include scores.",

                "assign <c> controls how things are assigned to classes. The character c\n\
should be one of 'p'artial, 'm'ost_likely or 'r'andom. Default is 'p'.",

                "splitleaf <S>, if class S is a leaf, makes S a dad and its subclasses "
                "leaves",

                "deldad <S> replaces dad class S by its sons",

                "help <command> gives some details of the command. help <\"help\"> lists "
                "all",

                "copypop <N, P> copies the \"work\" model to a new model. N selects the\n\
level of detail: N=0 copies no item weights or scores, leaving the new popln\n\
unattached to a sample. P is the new popln name.",

                "killpop <P> destroys a popln model. P must be the popln index or FULL "
                "name",

                "pickpop <P> copies popln P to \"work\".  P = popln index or FULL name",

                "tree  prints a summary of control settings and the hierarchic tree",

                "To stop, please use the full word \"stop\".",

                "file <F> switches command input to file name F. Command input will return\n\
to keyboard at end of F or error, unless F contains a \"file\" command.\n\
F may contain a file command with its own name, returning to the start of F.",

                "readsamp <F> reads in a new sample from file F. Sample must use a VarSet\n\
  already loaded.",

                "flatten makes all twigs (non-root classes with no sons) immediate leaves",

                "dodads <N> iterates adjustment of parameter costs and dad parameters\n\
   till stability or for N cycles",

                "samps lists the known samples.",

                "insdad <S1, S2> where classes S1,S2 are sibs inserts a new Dad with S1,S2\n\
as childen. The new Dad is son of original Dad od S1, S2.",

                "dogood <N> does N cycles of doleaves(3) and dodads(2) or until stable.",

                "bestinsdad makes a guess at the most profitable insdad and tries it.",

                "bestdeldad guesses the most profitable dad to delete and tries it.",

                "rebuild flattens the tree, then greedily rebuilds it.",

                "ranclass <N> destroys the current model and inserts N random leaves",

                "savepop <P, N, F> records model P on file named F, unattached if N=0",

                "restorepop <F> reads a model from file named F, as saved by 'savepop'.\n\
  If model unattached, or attached to an unknown sample, it is attached to\
  the current sample if any.",

                "nosubs <N> kills and prevents birth of subclasses if N > 0",

                "binhier <N> inserts dads to convert tree to a binary hierarchy, then\n\
  deletes dads to improve. If N > 0, first flattens tree.",

                "moveclass <S1 S2> moves class S1 (and any dependent subtree) to be a son of\n\
  class S2, which must be a dad. S1 may not be an ancestor of S2.",

                "bestmoveclass guesses the most profitable moveclass and tries it.",

                "pops lists the defined models.",

                "crosstab <P> shows the overlap between the leaves of work and the leaves of\n\
model P (which must be attached to the current sample.) The overlap is shown\n\
as a permillage of the number of active things, in a cross-tabulation table.\n\
A table entry for leaf serial int of work and leaf serial Sp of P shows the\n\
permillage of all active things which are in both leaves. Crosstab <work> is\n\
also meaningful. Here, an entry for leaves S1, S2 shows the permillage of\n\
item which are partially assigned to both classes.",

                "trymoves <N> attempts moveclasses until N successive failures.",

                "item <N> finds the sample item with identifier N.",

                "trep <F> writes a item report on file F. Assigns each item to smallest class\n\
    in which item has weight > 0.5,  and to most likely leaf.",

                "select <S> first copies the current work model to an unattached model called\
 OldWork, replaces the current sample by sample <S> where S is either name or\
 index, then picks OldWork to model it, getting item weights and scores.\
 The 'adjust' state is left as adjusting only scores, not params or tree.",

                " "};

int poss[NUM_COMMANDS];

int menu(int cnl) {
    char inp[80];
    int i, j, k = 0, nposs;

    i = read_str(inp, cnl);
    if (i == 2) {
        printf("%s\n Enter command string:\n", prmt[10]);
        i = read_str(inp, 1);
    }
    if (i)
        return (-1);

    // Initialize possibility array
    int poss[NUM_COMMANDS] = {0};
    for (i = 0; i < nvkey; i++) {
        poss[i] = 1;
    }

    nposs = i;

    // Identify possible keywords
    for (j = 0; inp[j]; j++) {
        for (i = 0; i < nvkey; i++) {
            if (poss[i] && (strlen(commands[i]) <= j || inp[j] != commands[i][j])) {
                poss[i] = 0;
            } else {
                k = i; // Last matching keyword index
            }
        }
    }

    if (nposs <= 0) {
        return (-1);
    } else if (nposs > 1) {
        /*    Could have an exact match  */
        for (k = 0; k < nvkey; k++) {
            if (poss[k] && (!commands[k][j])) {
                skk = k;
                return (cmd_index[k]);
            };
        }
        printf("Ambiguous command\n");
        for (i = 0; i < nvkey; i++) {
            if (poss[i])
                printf(" %s?", commands[i]);
        }
        printf("\n");
        return (-1);
    }

    skk = k;
    return (cmd_index[k]);
}

// write to logfile and flush buffers
void log_cmd(const char *format, ...) __attribute__((format(printf, 1, 2)));

void log_cmd(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(logfile, format, args);
    fprintf(logfile, "\n");
    va_end(args);
    fflush(logfile);
}
/* ----------------- readserial ------------------------- */
/* To get a class serial parameter for an action */
/* Returns the class ID or -3 for failure. Prints string if needed */
/* If input serial is -1 or -2, returns -1 or -2 */
int readserial(int prm) {
    int n, nn;

    n = read_int(&nn, 0);
    if (n == 2) {
        printf("The serial of a subclass is the serial of its dad\n");
        printf("followed without space by 'a' or 'b', e.g. 12a\n");
        printf("%s\n Enter class serial or 0 to abort\\n", prmt[prm]);
        n = read_int(&nn, 1);
    }

    if (n)
        return -3;
    if (nn == -1 || nn == -2)
        return nn;

    nn *= 4;
    if (terminator == 'a')
        nn += 1;
    else if (terminator == 'b')
        nn += 2;

    // Advance buffer if needed
    if (terminator == 'a' || terminator == 'b') {
        CurCtx.buffer->nch++;
    }

    return serial_to_id(nn);
}

/*    ----------------------  readanint -----------------------------  */
/*    To read an integer action parameter. Returns -1 if error  */
int readanint(int kk) {
    int n, nn;

    n = read_int(&nn, 0);
    if (n == 2) {
        printf("%s\n Enter integer parameter N\n", prmt[kk]);
        n = read_int(&nn, 1);
    }
    if (n)
        return (-1);
    return (nn);
}

/*    -------------------- readareal ----------------------------  */
/*    To read a real action parameter. Returns -1 if error, else 0  */
int readareal(int kk, double *rea) {
    double newv;
    int n;

    n = read_double(&newv, 0);
    if (n == 2) {
        printf("%s\n Enter real parameter V or 'a' to abort.\n", prmt[kk]);
        n = read_double(&newv, 1);
    }
    if (n)
        return (-1);
    *rea = newv;
    return (0);
}

/*    ------------------- readsparam ---------------------------  */
/*    To read a string parameter. -1 if error  */
int readsparam(int kk) {
    int n;
    n = read_str(sparam, 0);
    if (n == 2) {
        printf("%s\n Enter string of characters\n", prmt[kk]);
        n = read_str(sparam, 1);
    }
    if (n)
        n = -1;
    return (n);
}

/* ------------------ readachar ------------------------ */
/* To read one non-blank character */
int readachar(int kk) {
    int ch, nl = 0;

    while (1) {
        ch = read_char(nl);
        if (ch != ' ' && ch != '\t') {
            if (ch == 2) {
                printf("%s\n Enter character:\n", prmt[kk]);
                continue; // Read next character
            }
            break; // Non-blank character found
        }
    }
    return ch;
}

/* -------------------- readintname ----------------------- */
/* Reads a pos integer to intparam or name to sparam. Returns -1 if error.
   Leaves sparam = 0 if integer found */
int readintname(int kk, int jj) {
    int n, i, nn = 0;
    intparam = -1;

    n = read_str(sparam, 0);
    if (n == 2) {
        printf("%s\n Enter name string or integer index\n", prmt[kk]);
        switch (jj) {
        case 1:
            show_pop_names();
            break;
        case 2:
            show_smpl_names();
            break;
        }
        n = read_str(sparam, 1);
    }

    if (n)
        return -1;

    // Scan sparam for an integer
    for (i = 0; sparam[i]; i++) {
        if (sparam[i] >= '0' && sparam[i] <= '9') {
            nn = 10 * nn + sparam[i] - '0';
        } else if (sparam[i] == ' ' || sparam[i] == '\t' || sparam[i] == '\0') {
            sparam[0] = 0;
            intparam = nn;
            return 0; // Number found
        } else {
            break; // Not a number
        }
    }

    // If no number is found, return 0
    return 0;
}

/* -------------------- readpopid ------------------------ */
/* To read a popln index (+1) or name and return index */
int readpopid(int kk) {
    int n, nn;

    n = readintname(kk, 1);
    if (n)
        return n;

    if (intparam >= 0) {
        nn = intparam - 1;
        if (nn >= 0 && nn < MAX_POPULATIONS && Populations[nn] && Populations[nn]->id == nn) {
            return nn;
        }
        printf("%d is not a valid popln index\n", intparam);
        return -1;
    } else {
        nn = find_population(sparam);
        if (nn < 0) {
            printf("%s is not a model name\n", sparam);
        }
        return nn;
    }
}

/* -------------------- readsampid ------------------------ */
/* To read a sample index (+1) or name and return index */
int readsampid(int kk) {
    int n, nn;

    n = readintname(kk, 2);
    if (n)
        return n;

    if (intparam >= 0) {
        nn = intparam - 1;
        if (nn >= 0 && nn < MAX_SAMPLES && Samples[nn] && Samples[nn]->id == nn) {
            return nn;
        }
        printf("%d is not a valid sample index\n", intparam);
        return -1;
    } else {
        nn = find_sample(sparam, 1);
        if (nn < 0) {
            printf("%s is not a sample name\n", sparam);
        }
        return nn;
    }
}

/*    ---------------------  main  ---------------------------  */
void setup_menu() {
    int i, n;
        char *chp;
/*    A section to sort the keywords into alphabetic order, and set
    cmd_index[] to hold their original order indexes   */

    for (i = 0; strcmp(commands[i], "zzzzzzzzzzzz"); i++) {
        cmd_index[i] = i;
    }
    /*    fill out commands[] with z-strings   */
    nkey = i;
    for (; i < NUM_COMMANDS; i++) {
        commands[i] = "zzzzzzzzzzzz";
        cmd_index[i] = 999;
    }
    /*    Now bubble-sort the first nkey entries   */
    for (i = (nkey - 1); i > 0; i--) {
        for (j = 0; j < i; j++) {
            if (strcmp(commands[j], commands[j + 1]) > 0) {
                chp = commands[j];
                commands[j] = commands[j + 1];
                commands[j + 1] = chp;
                n = cmd_index[j];
                cmd_index[j] = cmd_index[j + 1];
                cmd_index[j + 1] = n;
            }
        }
    }
    /*    Count valid ones (not 10 z-s)    */
    nvkey = 0;
    for (i = 0; i < nkey; i++) {
        if (strcmp(commands[i], "zzzzzzzzzz"))
            nvkey++;
    }
}


void reset_input() {
    printf("??? line %6d\n", CurSource->line);
    new_line();
    if (CurSource->cfile)
        revert(0);
}

//extern char *malloc_options;
int main() {
    int k, kk, k1, k2;
    int i, j, n, nn, cspace = 0;
    double drop;
    Context oldctx;

    Fix = DFix = Partial;
    DControl = Control = AdjAll;

    initialize();    // initialize menu
    setup_menu(); //    Clear vectors of pointers to Poplns, Samples


    /*    Set source to commsbuf and initialize  */
    CurSource = &commsbuf;
    CurCtx.buffer = CurSource;
    CurSource->cfile = 0;
    CurSource->nch = 0;
    CurSource->inl[0] = '\n';
    Heard = UseStdIn = 0;
    SeeAll = 2;
    Debug = 1;

    /*ddd
    printf("Enter var and state to watch (kiv, kz)\n");
    ddd*/

    /*    Open a log file   */
    logfile = fopen("run.log", "w");

    kk = load_vset();

    printf("Readvset returns %d\n", kk);
    if (kk < 0) {
        exit(2);
    }
    CurVSet = CurCtx.vset = VarSets[kk];

    printf("Enter sample file name:\n");
    if (read_str(sparam, 1) < 0) {
        printf("Cannot read sample file name\n");
        goto error;
    }

    k = load_sample(sparam);
    printf("Readsample returns %d\n", k);
    cspace = report_space(1);
    kk = init_population();
    flp();
    printf("Firstpop returns %d\n", kk);
    cspace = report_space(1);

    trapcnt = 0;
#ifdef TRAP
    printf("Enter trap class, age; or -1 for notrap: ");
    scanf("%d", &trapkk);
    trapcnt = 0;
    if (trapkk >= 0)
        scanf("%d", &trapage);
#endif
    print_class(CurRoot, 0);
error:
    reset_input();
loop:
    k = find_population("TrialPop");
    if (k >= 0)
        destroy_population(k);
    flp();
    kk = report_space(0);
    if (kk != cspace)
        cspace = report_space(1);
    if (Heard) {
        revert(1);
        Heard = 0;
    }
    if (NoSubs || (Control != AdjAll)) {
        if (NoSubs)
            printf("NOSUBS  ");
        if (Control != AdjAll) {
            printf("FREEZE");
            if (!(Control & AdjPr))
                printf(" PARAMS");
            if (!(Control & AdjSc))
                printf(" SCORES");
            if (!(Control & AdjTr))
                printf(" TREE");
        }
        printf("  Cost%8.1f\n", CurRootClass->best_cost);
    }
    if (!CurCtx.popln) {
        kk = 13;
        goto pickapop;
    }
    set_population();
    Fix = DFix;
    Control = DControl;
    tidy(1);
    track_best(1);
    if (!CurSource->cfile) {
        flp();
        CurPopln = CurCtx.popln;
        CurClass = CurPopln->classes[CurPopln->root];
        printf("P%1d  %4d classes, %4d leaves,  Pcost%8.1f", CurPopln->id + 1, CurPopln->num_classes, CurPopln->num_leaves, CurClass->best_par_cost);
        if (CurPopln->sample_size)
            printf("  Tcost%10.1f,  Cost%10.1f", CurClass->best_case_cost, CurClass->best_cost);
        printf("\n");
        printf("Sample %2d %s\n", (CurCtx.sample) ? CurCtx.sample->id + 1 : 0, (CurCtx.sample) ? CurCtx.sample->name : "NULL");
    }
    if ((CurSource->nch) || (CurSource->inl[0] == '\n')) {
        if (new_line())
            goto error;
    }

    kk = menu(1);
    if (kk < 0)
        goto error;
    switch (kk) {
    case 0:
        fprintf(logfile, "\n");
        fclose(logfile);
        exit(2);
    case 1: /* doall */
        nn = readanint(kk);
        if (nn < 0)
            goto error;
        log_cmd("%s %d", commands[skk], nn);
        k = do_all(nn, 1);
        if (k >= 0) {
            flp();
            printf("Doall ends after %d cycles\n", k);
        }
        goto loop;
    case 2: /* doleaves */
        nn = readanint(kk);
        if (nn < 0)
            goto error;
        log_cmd("%s %d", commands[skk], nn);
        k = do_all(nn, 0);
        if (k >= 0) {
            flp();
            printf("Doleaves ends after %d cycles\n", k);
        }
        goto loop;
    case 3: /* killclass */
        k = readserial(kk);
        if (k == CurRoot) {
            printf("Won't kill Root\n");
            goto loop;
        }
        if (k < 0)
            goto error;
        log_cmd("%s %s", commands[skk], serial_to_str(CurPopln->classes[k]));
        CurPopln->classes[k]->weights_sum = 0.0;
        goto loop;
    case 4: /* delete_sons */
        k = readserial(kk);
        if (k < 0)
            goto error;
        log_cmd("%s %s", commands[skk], serial_to_str(CurPopln->classes[k]));
        delete_sons(k);
        CurPopln->classes[k]->hold_type = HoldTime;
        goto loop;
    case 5: /* prclass */
        k = readserial(kk);
        if (k < -2)
            goto error;
        i = readanint(kk);
        if (i < 0)
            goto error;
        log_cmd("%s %s %d", commands[skk], serial_to_str(CurPopln->classes[k]), i);
        print_class(k, i);
        goto loop;
    case 6: /*    adjust  */
        k = readsparam(kk);
        if (k < 0)
            goto error;
        i = 0;
        n = nn = 0;
    adjchar1:
        k = sparam[i++];
        switch (k) {
        case 0:
            goto adjflagsin;
        case 'a':
            j = AdjAll;
            goto adjchar2;
        case 's':
            j = AdjSc;
            goto adjchar2;
        case 't':
            j = AdjTr;
            goto adjchar2;
        case 'p':
            j = AdjPr;
            goto adjchar2;
        }
        goto error;

    adjchar2:
        k = sparam[i++];
        switch (k) {
        case '+':
            n |= j;
            goto adjchar1;
        case '-':
            nn |= j;
            goto adjchar1;
        }
        goto error;
    adjflagsin:
        log_cmd("%s %s", commands[skk], sparam);
        nn = AdjAll - nn;
        Control = DControl = (DControl & nn) | n;
        goto loop;
    case 7: /*  assign  */
        k = readachar(kk);
        switch (k) {
        case 'p':
            DFix = Partial;
            goto assignok;
            ;
        case 'r':
            DFix = Random;
            goto assignok;
            ;
        case 'm':
            DFix = Most_likely;
            goto assignok;
            ;
        }
        goto error;
    assignok:
        log_cmd("%s %c", commands[skk], k);
        goto loop;
    case 8: /* splitleaf */
        k = readserial(kk);
        if (k < 0)
            goto error;
        if (CurPopln->classes[k]->type != Leaf) {
            printf("Class %s is not a leaf\n", serial_to_str(CurPopln->classes[k]));
            goto loop;
        }

        log_cmd("%s %s", commands[skk], serial_to_str(CurPopln->classes[k]));
        if (split_leaf(k))
            printf("Cannot split class%s\n", serial_to_str(CurPopln->classes[k]));
        goto loop;
    case 9: /* splice_dad */
        k = readserial(kk);
        if (k < 0)
            goto error;
        log_cmd("%s %s", commands[skk], serial_to_str(CurPopln->classes[k]));
        k = CurPopln->classes[k]->serial;
        drop = splice_dad(k);
        goto dropprint;

    case 10: /* help */
        k = menu(0);
        if (k < 0)
            goto error;
        if (k == kk)
            goto listall;
        printf("%s\n", prmt[k]);
        goto loop;

    listall:
        /*    This bit lists all the command words, 5 per line    */
        /*    Keywords of 10 z-s are omitted, so first I count valid ones  */
        n = (nvkey + 4) / 5;
        for (k = 0; k < n; k++) {
            for (j = 0; j < 5; j++) {
                i = j * n + k;
                if (i >= nvkey)
                    goto endof5;
                printf("%s%s", commands[i], spaces + strlen(commands[i]));
            endof5:;
            }
            printf("\n");
        }
        goto loop;
    case 11: /* copy_population */
        i = readanint(kk);
        if (i < 0)
            goto error;
        j = readsparam(kk);
        if (j < 0)
            goto error;
        if (!strcmp(sparam, "work")) {
            printf("Donot copy work into work!\n");
            goto error;
        }
        for (j = 0; j < 4; j++) {
            if (sparam[j] != "BST_"[j])
                goto ok11;
        }
        printf("Please donot use a popln name beginning BST_\n");
        goto error;
    ok11:
        log_cmd("%s %d %s", commands[skk], i, sparam);
        i = copy_population(CurPopln->id, i, sparam);
        if (i < 0)
            goto error;
        printf("Index%3d = popln %s\n", i + 1, sparam);
        Control = DControl;
        Fix = DFix;
        goto loop;
    case 12: /*  destroy_population */
        k = readpopid(kk);
        if (k < 0)
            goto error;
        log_cmd("%s %s", commands[skk], Populations[k]->name);
        destroy_population(k);
        goto loop;
    case 13: /* pickpop */
        goto pickapop;
    case 14: /* tree */
        print_tree();
        goto loop;
    case 15: /* sto */
        printf("%s\n", prmt[kk]);
        goto loop;
    case 16: /* file */
        k = readsparam(kk);
        if (k < 0)
            goto error;
        if (CurSource->cfile)
            close_buffer();
        strcpy(cfilebuf.cname, sparam);
        CurSource = &cfilebuf;
        CurCtx.buffer = CurSource;
        if (open_buffer()) {
            printf("Cant open file %s\n", sparam);
            goto error;
        }
        goto loop;
    case 17: /* readsamp  */
        if (readsparam(kk) < 0)
            goto error;
        memcpy(&oldctx, &CurCtx, sizeof(Context));
        k = load_sample(sparam);
        memcpy(&CurCtx, &oldctx, sizeof(Context));
        if (k < 0)
            goto error;
        printf("Sample %s index%2d from file %s\n", Samples[k]->name, k + 1, sparam);
        log_cmd("%s %s", commands[skk], sparam);
        goto loop;
    case 18: /*  flatten  */
        log_cmd("%s", commands[skk]);
        log_cmd("%s", commands[skk]);
        flatten(1);
        goto loop;

    case 19: /*  dodads  */
        nn = readanint(kk);
        if (nn < 0)
            goto error;
        log_cmd("%s %d", commands[skk], nn);
        do_dads(nn);
        goto loop;

    case 20: /*  samps  */
        show_smpl_names();
        goto loop;

    case 21: /* insert_dad */
        k1 = readserial(kk);
        if (k1 < 0)
            goto error;
        k2 = readserial(kk);
        if (k2 < 0)
            goto error;
        k1 = CurPopln->classes[k1]->serial;
        k2 = CurPopln->classes[k2]->serial;
        log_cmd("%s %s %s", commands[skk], serial_to_str(CurPopln->classes[k1]), serial_to_str(CurPopln->classes[k2]));
        drop = insert_dad(k1, k2, &k);
        flp();
        if (k >= 0)
            printf("New Dad's serial%s\n", serial_to_str(CurPopln->classes[k]));
    dropprint:
        if (drop < -1000000.0) {
            printf("Cannot be done\n");
            goto loop;
        }
        printf("Tree parameter cost changes by%12.1f\n", -drop);
        print_tree();
        goto loop;

    case 22: /*  dogood  */
        nn = readanint(kk);
        if (nn < 0)
            goto error;
        log_cmd("%s %d", commands[skk], nn);
        k = do_good(nn, ((double)0.0));
        if (k >= 0) {
            flp();
            printf("Dogood ends after %d cycles\n", k);
        }
        goto loop;

    case 23: /*  best_insert_dad  */
        clr_bad_move();
        log_cmd("%s", commands[skk]);
        k = best_insert_dad(0);
        if (k < 0)
            printf("No good dad insertion found\n");
        else
            printf("Inserting new dad %5d\n", k >> 2);
        goto loop;

    case 24: /*  best_remove_dad  */
        clr_bad_move();
        log_cmd("%s", commands[skk]);
        k = best_remove_dad();
        if (k < 0)
            printf("No good dad deletion found\n");
        else
            printf("Deleting dad%6d\n", k >> 2);
        goto loop;

    case 25: /*  rebuild  */
        log_cmd("%s", commands[skk]);
        rebuild();
        goto loop;

    case 26: /*  ranclass  */
        nn = readanint(kk);
        if (nn < 2)
            goto error;
        log_cmd("%s %d", commands[skk], nn);
        ranclass(nn);
        goto loop;

    case 27: /*  save_population */
        k = readpopid(kk);
        if (k < 0)
            goto error;
        i = readanint(kk);
        if (i < 0)
            goto error;
        j = readsparam(kk);
        if (j < 0)
            goto error;

        log_cmd("%s %s %d %s", commands[skk], Populations[k]->name, i, sparam);
        i = save_population(k, i, sparam);
        if (i < 0) {
            printf("Savepop failed\n");
            goto error;
        }
        goto loop;

    case 28: /*  load_population */
        if (readsparam(kk) < 0)
            goto error;
        log_cmd("%s %s", commands[skk], sparam);
        k = load_population(sparam);
        if (k < 0) {
            printf("Restorepop failed\n");
            goto error;
        }
        printf("Model %s Index%3d\n", Populations[k]->name, k + 1);
        goto loop;

    case 29: /* nosubs */
        nn = readanint(kk);
        if (nn < 0)
            goto error;
        if (nn)
            NoSubs = 1;
        else
            NoSubs = 0;
        log_cmd("%s %d", commands[skk], NoSubs);
        goto loop;

    case 30: /* binary_hierarchy */
        nn = readanint(kk);
        if (nn < 0)
            goto error;
        log_cmd("%s %d", commands[skk], nn);
        binary_hierarchy(nn);
        goto loop;

    case 31: /* move_class */
        k1 = readserial(kk);
        if (k1 < 0)
            goto error;
        k2 = readserial(kk);
        if (k2 < 0)
            goto error;
        k1 = CurPopln->classes[k1]->serial;
        k2 = CurPopln->classes[k2]->serial;
        log_cmd("%s %s %s", commands[skk], serial_to_str(CurPopln->classes[k1]), serial_to_str(CurPopln->classes[k2]));
        drop = move_class(k1, k2);
        printf("Move changes cost by %12.1f\n", -drop);
        goto loop;

    case 32: /*  best_move_class  */
        clr_bad_move();
        log_cmd("%s", commands[skk]);
        best_move_class(0);
        goto loop;

    case 33: /*  pops  */
        show_pop_names();
        goto loop;

    case 34: /*  crosstab  */
        k = readpopid(kk);
        if (k < 0)
            goto error;
        correlpops(k);
        goto loop;

    case 35: /*  trymoves  */
        nn = readanint(kk);
        if (nn < 0)
            goto error;
        log_cmd("%s %d", commands[skk], nn);
        try_moves(nn);
        goto loop;

    case 36: /* item */
        nn = readanint(kk);
        if (nn < 0)
            goto error;
        log_cmd("%s %d", commands[skk], nn);
        kk = find_sample_index(nn);
        printf("Thing%8d  at index%8d\n", nn, kk);
        goto loop;

    case 37: /* trep */
        if (readsparam(kk) < 0)
            goto error;
        item_list(sparam);
        goto loop;

    case 38: /* select */
        k = readsampid(kk);
        if (k < 0)
            goto error;
        printf("Selecting sample %s\n", Samples[k]->name);
        log_cmd("%s %s", commands[skk], Samples[k]->name);
        i = copy_population(CurPopln->id, 0, "OldWork");
        if (i < 0) {
            printf("Can't make OldWork copy of work\n");
            goto error;
        }
        if (CurCtx.popln)
            destroy_population(CurCtx.popln->id);
        CurPopln = CurCtx.popln = 0;
        CurSample = CurCtx.sample = Samples[k];
        i = init_population();
        if (i < 0) {
            printf("Cannot make first population for sample\n");
            goto error;
        }
        goto loop;
    }; /* End of kk switch  */

pickapop:
    i = readpopid(kk);
    if (i < 0)
        goto error;
    if (!strcmp(Populations[i]->name, "work")) {
        if (CurCtx.popln && (CurCtx.popln->id == i))
            printf("Work already picked\n");
        else
            printf("Switching context to existing 'work'");
        k = i;
        goto picked;
    }
    k = set_work_population(i);
picked:
    CurCtx.popln = CurPopln = Populations[k];
    log_cmd("%s %s", commands[skk], Populations[i]->name);
    goto loop;
#ifdef ZZ
#endif
}
