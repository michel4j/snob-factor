
/*    ---------------------  MAIN FILE -----------------  */
#include "glob.h"
#include <stdarg.h>
#include <time.h>

#define DO_ALL_STEPS 50
#define TRY_MOVE_STEPS 4

/*    ---------------------  main  ---------------------------  */
int main(int argc, char *argv[]) {
    int index, cycles = 3;

    if (argc < 3) {
        log_msg(1, "Usage: %s <vset.v> <smpl.s> <report.rep>", argv[1]);
        exit(2);
    }

    Fix = DFix = Partial;
    DControl = Control = AdjAll;

    initialize(1);    // initialize menu

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
            log_msg(1, "Cycle %d", 1 + j / 2);
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
