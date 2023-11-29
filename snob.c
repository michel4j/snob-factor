/*    ---------------------  MAIN FILE -----------------  */
#include "glob.h"
#include <omp.h>
#include <stdarg.h>
#include <time.h>

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
    defaulttune();
    printf("SNOB-Factor: %d Threads Available\n\n", omp_get_max_threads());
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
                printf("Work already picked\n");
            else
                printf("Switching context to existing 'work'");
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
    show_population(CurPopln, CurSample);
}

extern char *malloc_options;
int main(int argc, char *argv[]) {
    int index, ret_code;

    if (argc < 3) {
        printf("Usage: %s <vset.v> <smpl.s> <report.rep>\n", argv[1]);
        exit(2);
    }

    Fix = DFix = Partial;
    DControl = Control = AdjAll;

    initialize();    // initialize menu
    clear_vectors(); //    Clear vectors of pointers to Poplns, Samples
    do_types();

    /*    Set source to commsbuf and initialize  */
    CurSource = &commsbuf;
    CurCtx.buffer = CurSource;
    CurSource->cfile = 0;
    CurSource->nch = 0;
    CurSource->inl[0] = '\n';
    UseLib = 1;
    SeeAll = 2;

    index = open_vset(argv[1]);
    if (index < 0) {
        printf("Error[ %d ] reading vset: %s\n", index, argv[1]);
        exit(2);
    }
    CurVSet = CurCtx.vset = VarSets[index];

    load_sample(argv[2]);
    init_population();
    print_class(CurRoot, 0);

    for (int j = 0; j < 6; j++) {
        cleanup_population();
        if (j % 2 == 0) {

            ret_code = do_all(50, 1);
            if (ret_code >= 0) {
                printf("\nDoall ends after %d cycles\n", ret_code);
            }
        } else {
            try_moves(4);
        }
    }
    cleanup_population();
    print_tree();    
    tidy(1);

    print_class(-2, 1);
    show_population(CurPopln, CurSample);
    if (argc == 4) {
        item_list(argv[3]);
    }
}
