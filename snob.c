
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


    initialize(1);   

    index = open_vset(argv[1]);
    if (index < 0) {
        log_msg(2, "Error[ %d ] reading vset: %s", index, argv[1]);
        exit(2);
    }
    CurVSet = CurCtx.vset = VarSets[index];

    load_sample(argv[2]);
    classify(cycles, DO_ALL_STEPS, TRY_MOVE_STEPS);
   
    // display tree and classes
    print_tree();
    print_class(-2, 1);
    show_population(CurPopln, CurSample);

    if (argc == 4) {
        item_list(argv[3]);
    }
}
