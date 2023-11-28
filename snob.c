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

extern char *malloc_options;
int main(int argc, char *argv[]) {
    int k, i, res, kk;
    int cspace = 0;

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

    res = open_vset(argv[1]);
    if (res < 0) {
        printf("Error[ %d ] reading vset: %s\n", res, argv[1]);
        exit(2);
    }
    CurVSet = CurCtx.vset = VarSets[res];
    SeeAll = 2;
    k = load_sample(argv[2]);
    cspace = report_space(1);
    kk = init_population();
    cspace = report_space(1);

    trapcnt = 0;
    print_class(CurRoot, 0);

    k = find_population("TrialPop");
    if (k >= 0)
        destroy_population(k);
    kk = report_space(0);
    if (kk != cspace)
        cspace = report_space(1);

    show_summary();
    set_population();
    Fix = DFix;
    Control = DControl;
    tidy(1);
    track_best(1);
    if (!CurSource->cfile) {
        CurPopln = CurCtx.popln;
        CurClass = CurPopln->classes[CurPopln->root];
        printf("\n");
        printf("P%1d  %4d classes, %4d leaves,  Pcost%8.1f", CurPopln->id + 1, CurPopln->num_classes, CurPopln->num_leaves, CurClass->best_par_cost);
        if (CurPopln->sample_size)
            printf("  Tcost%10.1f,  Cost%10.1f", CurClass->best_case_cost, CurClass->best_cost);
        printf("\n");
        printf("Sample %2d %s\n", (CurCtx.sample) ? CurCtx.sample->id + 1 : 0, (CurCtx.sample) ? CurCtx.sample->name : "NULL");
    }

    for (int j = 0; j < 3; j++) {
        k = do_all(50, 1);
        printf("Doall ends after %d cycles\n", k);
        try_moves(2);
        show_summary();
    }
    print_tree();
    print_class(-2, 1);

    if (argc == 4) {
        item_list(argv[3]);
    }
}
