/*    ---------------------  MAIN FILE -----------------  */
#include "glob.h"
#include <omp.h>
#include <stdarg.h>
#include <time.h>

#define DO_ALL_STEPS 50
#define TRY_MOVE_STEPS 4

/*    ---------------------  main  ---------------------------  */

/**
    Clear vectors of pointers to Poplns, Samples
*/
void clear_vectors() {
    int k;

    for (k = 0; k < MAX_POPULATIONS; k++)
        Populations[k] = 0;
    for (k = 0; k < MAX_SAMPLES; k++)
        Samples[k] = 0;
    for (k = 0; k < MAX_VSETS; k++)
        VarSets[k] = 0;
}
/**
    Initialize SNOB
*/
void initialize() {
    RSeed = 1234567;
    SeeAll = 2;
    UseLib = 1;
    defaulttune();
    log_msg(1, "SNOB-Factor: %d Threads Available\n\n", omp_get_max_threads());
    clear_vectors();
}

void show_summary() {
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
    show_summary();
    Fix = DFix;
    Control = DControl;
    tidy(1);
    track_best(1);
}


int main(int argc, char *argv[]) {
    int index, cycles = 4;

    if (argc < 3) {
        log_msg(1, "Usage: %s <vset.v> <smpl.s> <report.rep>", argv[1]);
        exit(2);
    }

    Fix = DFix = Partial;
    DControl = Control = AdjAll;

    initialize();    // initialize menu
    clear_vectors(); //    Clear vectors of pointers to Poplns, Samples
    do_types();

    index = open_vset(argv[1]);
    if (index < 0) {
        log_msg(2, "Error[ %d ] reading vset: %s", index, argv[1]);
        exit(2);
    }
    CurVSet = CurCtx.vset = VarSets[index];

    load_sample(argv[2]);
    init_population();
    print_class(CurRoot, 0);

    for (int j = 0; j < cycles * 2; j++) {
        cleanup_population();
        if (j % 2 == 0) {
            log_msg(1, "Cycle %d", 1+j/2);
            log_msg(1, "DOALL:   Doing %d steps of re-estimation and assignment.", DO_ALL_STEPS);
            do_all(DO_ALL_STEPS, 1);
        } else {
            log_msg(1, "TRYMOVE: Attempting class moves until %d successive failures", TRY_MOVE_STEPS);
            try_moves(TRY_MOVE_STEPS);
            show_population(CurPopln, CurSample);
        }
    }
    cleanup_population();

    // display tree and classes
    print_tree();
    print_class(-2, 1);
    show_population(CurPopln, CurSample);
    if (argc == 4) {
        item_list(argv[3]);
    }
}
