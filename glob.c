
#define GLOBALS 1
#include "glob.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

/* Initialize some variables */
int UseLib = 0;
int Debug = 0;

/*    To assist in printing class serials  */
char *serial_to_str(Class *cls) {
    static char str[8];
    int i, j, k;
    for (i = 0; i < 6; i++) {
        str[i] = ' ';
    }
    str[6] = 0;
    if (cls && cls->type != Vacant) {
        j = cls->serial;
        k = j & 3;
        if (k == 1) {
            str[5] = 'a';
        } else if (k == 2) {
            str[5] = 'b';
        }
        j = j >> 2;
        str[4] = '0';
        for (i = 4; i > 0; i--) {
            if (j <= 0)
                break;
            str[i] = j % 10 + '0';
            j = j / 10;
        }
        if (j) {
            strcpy(str + 1, "2BIG");
        }
    }
    return str;
}

/*    --------------------  show_pop_names  ---------------------  */
void show_pop_names() {
    int i;
    printf("The defined models are:\n");
    for (i = 0; i < MAX_POPULATIONS; i++) {
        if (Populations[i]) {
            printf("%2d %s", i + 1, Populations[i]->name);
            if (Populations[i]->sample_size)
                printf(" Sample %s", Populations[i]->sample_name);
            else
                printf(" (unattached)");
            printf("\n");
        }
    }
    printf("Also, the pseudonym \"BST_\" can be used for the best\n");
    printf("model for the current sample\n");
    return;
}

/*      ----------------------  show_smpl_names  -----------------  */
void show_smpl_names() {
    int k;
    printf("Loaded samples:\n");
    for (k = 0; k < MAX_SAMPLES; k++) {
        if (Samples[k]) {
            printf("%2d:  %s\n", k + 1, Samples[k]->name);
        }
    }
    return;
}


void log_msg(int level, const char *format, ...) {
    if (level || Debug) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\n");
    }
}


/**
    Initialize SNOB
*/
void initialize(int lib) {
    int k;
    RSeed = 1234567;
    SeeAll = 2;
    UseLib = lib;
    Fix = DFix = Partial;
    DControl = Control = AdjAll;

    defaulttune();
    for (k = 0; k < MAX_POPULATIONS; k++)
        Populations[k] = 0;
    for (k = 0; k < MAX_SAMPLES; k++)
        Samples[k] = 0;
    for (k = 0; k < MAX_VSETS; k++)
        VarSets[k] = 0;
    do_types();
}

void show_population(Population *popln, Sample *sample) {
    Class *root;
    root = popln->classes[popln->root];
    printf("\n--------------------------------------------------------------------------------\n");
    printf("P%1d  %4d classes, %4d leaves,  Pcost%8.1f", popln->id + 1, popln->num_classes, popln->num_leaves, root->best_par_cost);
    if (popln->sample_size) {
        printf("  Tcost%10.1f,  Cost%10.1f", root->best_case_cost, root->best_cost);
    }
    printf("\n");
    printf("Sample %2d %s", (sample) ? sample->id + 1 : 0, (sample) ? sample->name : "NULL");
    printf("\n--------------------------------------------------------------------------------\n");
}

void pick_population(int index) {
    if (index >= 0 && index < MAX_POPULATIONS && Populations[index] && Populations[index]->id == index) {
        if (!strcmp(Populations[index]->name, "work")) {
            if (CurCtx.popln && (CurCtx.popln->id == index))
                log_msg(1, "Work already picked");
            else
                log_msg(1, "Switching context to existing 'work'");
            set_work_population(index);
        }
    }
    CurCtx.popln = CurPopln = Populations[index];
}

void cleanup_population() {
    int index = find_population("TrialPop");
    if (index >= 0) {
        destroy_population(index);
    }
    set_population();
    Fix = DFix;
    Control = DControl;
    tidy(1);
    track_best(1);
}

void print_progress(size_t count, size_t max) {
    const int bar_width = 70;

    float progress = (float) count / max;
    int bar_length = progress * bar_width;

    printf("\r[");
    for (int i = 0; i < bar_length; ++i) {
        printf("━");
    }
    for (int i = bar_length; i < bar_width; ++i) {
        printf(" ");
    }
    printf("] %.2f%%", progress * 100);
    fflush(stdout);
}

int classify(unsigned int cycles, unsigned int do_steps, unsigned int move_steps) {
    init_population();
    cleanup_population();
    print_class(CurRoot, 0);

    for (int j = 0; j < cycles * 2; j++) {
        if (j % 2 == 0) {
            log_msg(1, "Cycle %d", 1 + j / 2);
            log_msg(1, "DOALL:   Doing %d steps of re-estimation and assignment.", do_steps);
            do_all(do_steps, 1);
        } else {
            log_msg(1, "TRYMOVE: Attempting class moves until %d successive failures", move_steps);
            try_moves(move_steps);
            show_population(CurPopln, CurSample);
        }
        cleanup_population();
    }
    return CurPopln->num_leaves;
}