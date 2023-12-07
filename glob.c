
#define GLOBALS 1
#include "glob.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
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

/// @brief Initialize SNOB parameters
/// @param lib integer specifying if running from a library or not 1 = library, 0 = interactive
/// @param debug turn on verbose printing of progress
/// @param threads number of threads to use during parallel portions, 0 = use OpenMP environment variables instead

static int Initialized = 0;

void initialize(int lib, int debug, int threads) {
    int k;
    UseLib = lib;
    Debug = debug;
    UseBin = 0;

    RSeed = 1234567;
    SeeAll = 2;
    Fix = DFix = Partial;
    DControl = Control = AdjAll;

    if (threads > 0) {
        // omp_set_num_threads(4);
    }

    default_tune();
    do_types();
    
    for (k = 0; k < MAX_POPULATIONS; k++)
        Populations[k] = 0;
    for (k = 0; k < MAX_SAMPLES; k++)
        Samples[k] = 0;
    for (k = 0; k < MAX_VSETS; k++)
        VarSets[k] = 0;

    Initialized = 1;
}

void reset() {  
    int k;
    RSeed = 1234567;
    SeeAll = 2;
    Fix = DFix = Partial;
    DControl = Control = AdjAll;

    if (Initialized) {
        for (k = 0; k < MAX_POPULATIONS; k++)
            destroy_population(k);
        for (k = 0; k < MAX_SAMPLES; k++)
            destroy_sample(k);
        for (k = 0; k < MAX_VSETS; k++)
            destroy_vset(k);
    }

}

/// @brief Print the details about the number of classes, leaves, and the associated costs for the current population
void show_population() {
    Class *root;
    Population *popln = CurCtx.popln;
    Sample *sample = CurCtx.sample;

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

void cleanup_population() {
    int index = find_population("TrialPop");
    if (index >= 0) {
        destroy_population(index);
    }

    Fix = DFix;
    Control = DControl;
    tidy(1);
    track_best(1);
}

void print_progress(size_t count, size_t max) {
    const int bar_width = 70;

    float progress = (float)count / max;
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

/// @brief Run f full classification sequence
/// @param max_cycles Maximum number of full cycles of doall followed by trymoves
/// @param do_steps Number of doall steps of assignment and estimation
/// @param move_steps Number of trymoves steps to fix the classification tree
/// @param tol  Convergence tolerance as a percentage. Classification stops if the cost improvement is less than the tolerance
/// @return Classification Result Structure containing number of classes and cost
Result classify(const int max_cycles, const int do_steps, const int move_steps, const double tol) {
    Result result;
    Class *root;
    int cycle = 0, prev_classes = 0, prev_leaves = 0, no_change_count = 0;

    init_population();
    cleanup_population();
    print_class(CurCtx.popln->root, 0);
    root = CurCtx.popln->classes[CurCtx.popln->root];

    double cost = root->best_cost, delta = 0.0;
    do {
        log_msg(1, "Cycle %d", 1 + cycle);
        log_msg(1, "DOALL:   Doing %d steps of re-estimation and assignment.", do_steps);
        cleanup_population();
        do_all(do_steps, 1);
        
        cleanup_population();
        log_msg(1, "TRYMOVE: Attempting class moves until %d successive failures", move_steps);
        try_moves(move_steps);

        show_population();
        root = CurCtx.popln->classes[CurCtx.popln->root];
        delta = 100.0 * (cost - root->best_cost) / cost;
        if ((prev_classes == CurCtx.popln->num_classes) && (prev_leaves == CurCtx.popln->num_leaves) && (delta < tol)) {
            no_change_count++;
        }
        prev_classes = CurCtx.popln->num_classes;
        prev_leaves = CurCtx.popln->num_leaves;
        cost = root->best_cost;
        cycle++;
        if (no_change_count > 2) {
            break;
        }
    } while (cycle < max_cycles);

    if (cycle > max_cycles) {
        log_msg(1, "WARNING: Classification did not converge after %d cycles", max_cycles);
    } else {
        log_msg(1, "Classification converged after %d cycles", cycle);
        log_msg(1, "%4d classes,  %4d leaves,  Cost %8.1f", prev_classes, prev_leaves, cost);
    }

    //  Prepare return structure
    result.num_classes = CurCtx.popln->num_classes;
    result.num_leaves = CurCtx.popln->num_leaves;
    result.model_length = root->best_par_cost;
    result.data_length = root->best_case_cost;
    result.message_length = root->best_cost;

    return result;
}