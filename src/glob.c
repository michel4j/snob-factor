
#define GLOBALS 1
#include "glob.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Initialize some variables */
int Interactive = 0;
int Debug = 1;
int NumRepChars = 0;

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
    NumRepChars = 0;
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
    NumRepChars = 0;
}

/// @brief Select the sample by the given name
/// @param name name of sample to select
void select_sample(char *name) {
    int smpl_id, k;
    smpl_id = find_sample(name, 1);
    if (smpl_id >= 0) {
        log_msg(1, "Selecting sample [%d] %s", smpl_id, Samples[smpl_id]->name);
        k = copy_population(CurCtx.popln->id, 0, "OldWork");
        if (k >= 0) {
            if (CurCtx.popln) {
                destroy_population(CurCtx.popln->id);
            }
            CurCtx.popln = 0;
            CurCtx.sample = Samples[smpl_id];
            log_msg(0, "Preparing Initial Population for sample [%d] %s", smpl_id, Samples[smpl_id]->name);
            k = init_population();
            if (k < 0) {
                log_msg(1, "Cannot make first population for sample");
                return;
            }
        } else {
            log_msg(1, "Can't make OldWork copy of work");
        }
        select_population("OldWork");
        cleanup_population();
    }
}

/// @brief Select a population by name
/// @param name
void select_population(char *name) {
    int k, p = find_population(name);
    if (p >= 0) {
        if (!strcmp(Populations[p]->name, "work")) {
            if (CurCtx.popln && (CurCtx.popln->id == p)) {
                log_msg(0, "Work already picked");
            } else {
                log_msg(0, "Switching context to existing 'work'");
            }
            k = p;
        } else {
            k = set_work_population(p);
        }
        CurCtx.popln = Populations[k];
    } else {
        log_msg(0, "No existing population '%s'", name);
    }
    show_population();
}

void log_msg(int level, const char *format, ...) {

    if (level >= Debug) {
        if (NumRepChars > 0) {
            printf("\n");
        }
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\n");
        NumRepChars = 0;
    }
}

void print_buffer(MemBuffer *dest, const char *format, ...) {
    va_list args;
    va_start(args, format);
    if (dest->offset < dest->size) {
        dest->offset += vsnprintf(dest->buffer + dest->offset, dest->size - dest->offset, format, args);
    }
    va_end(args);
}

int error_value(const char *message, const int value) {
    log_msg(1, message);
    return value;
}

void handle_sigint(int sig) { Stop = 1; }

/// @brief Initialize SNOB parameters
/// @param interact integer specifying if running from a library or not 1 = interactive, 0 = non interactive
/// @param debug turn on verbose printing of progress
/// @param threads number of threads to use during parallel portions, 0 = use OpenMP environment variables instead

static int Initialized = 0;

void initialize(int interact, int debug, int seed) {
    int k;
    Interactive = interact;
    Debug = debug;
    Stop = 0;

    SeeAll = 2;
    Fix = DFix = Partial;
    DControl = Control = AdjAll;

    if (seed != 0) {
        RSeed = seed;
    } else {
        RSeed = time(NULL);
    }

    if (!Initialized) {
        signal(SIGINT, handle_sigint);
        default_tune();
        do_types();

        for (k = 0; k < MAX_POPULATIONS; k++)
            Populations[k] = 0;
        for (k = 0; k < MAX_SAMPLES; k++)
            Samples[k] = 0;
        for (k = 0; k < MAX_VSETS; k++)
            VarSets[k] = 0;
    } else {
        // Cleanup
        for (k = 0; k < MAX_POPULATIONS; k++)
            destroy_population(k);
        for (k = 0; k < MAX_SAMPLES; k++)
            destroy_sample(k);
        for (k = 0; k < MAX_VSETS; k++)
            destroy_vset(k);
    }
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
    tidy(1, NoSubs);
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
        log_msg(1, "Doing %d steps of costing, assignment and adjustments.", do_steps);
        do_all(do_steps, 1);
        cleanup_population();

        log_msg(1, "Attempting class moves until %d successive failures", move_steps);
        try_moves(move_steps);
        cleanup_population();

        log_msg(1, "Cost dropped by %8.3f%%", delta);
        show_population();
        root = CurCtx.popln->classes[CurCtx.popln->root];
        delta = 100.0 * (cost - root->best_cost) / cost;
        if ((CurCtx.popln->num_classes > 1)) {
            // test convergence if we are not at the beginning
            if ((prev_classes == CurCtx.popln->num_classes) && (prev_leaves == CurCtx.popln->num_leaves) && (delta < tol)) {
                no_change_count++;
            }
        }
        prev_classes = CurCtx.popln->num_classes;
        prev_leaves = CurCtx.popln->num_leaves;
        cost = root->best_cost;
        cycle++;
        if ((no_change_count > 2) || (Stop)) {
            break;
        }

    } while (cycle < max_cycles);

    if (cycle > max_cycles) {
        log_msg(1, "WARNING: Classification did not converge after %d cycles", max_cycles);
    } else if (Stop) {
        log_msg(1, "WARNING: Classification interrupted after %d cycles", cycle);
    } else {
        log_msg(1, "Classification converged after %d cycles", cycle);
    }

    //  Prepare return structure
    result.num_classes = CurCtx.popln->num_classes;
    result.num_leaves = CurCtx.popln->num_leaves;
    result.model_length = root->best_par_cost;
    result.data_length = root->best_case_cost;
    result.message_length = root->best_cost;
    result.num_attrs = CurCtx.vset->length;
    result.num_cases = CurCtx.sample->num_cases;

    return result;
}

/// @brief Save a Classification Model to file
/// @param filename model file
/// @return >= 0 if successful
int save_model(char *filename) {
    int best;
    best = copy_population(get_best_pop(), 0, "SavedModel");
    return save_population(best, 0, filename);
}

Result load_model(char *filename) {
    int pop;
    Result result;
    Class *root;

    pop = load_population(filename);
    set_work_population(pop);

    //  Prepare return structure
    root = CurCtx.popln->classes[CurCtx.popln->root];
    result.num_classes = CurCtx.popln->num_classes;
    result.num_leaves = CurCtx.popln->num_leaves;
    result.model_length = root->best_par_cost;
    result.data_length = root->best_case_cost;
    result.message_length = root->best_cost;
    result.num_attrs = CurCtx.vset->length;
    result.num_cases = CurCtx.sample->num_cases;

    return result;
}

void save_context() { memcpy(&BkpCtx, &CurCtx, sizeof(Context)); }
void restore_context() { memcpy(&CurCtx, &BkpCtx, sizeof(Context)); }
void set_control_flags(int flags) { Control = DControl = flags; }