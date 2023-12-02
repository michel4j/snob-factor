
/*    ---------------------  MAIN FILE -----------------  */
#include "snob.h"
#include <stdarg.h>
#include <time.h>

#define DO_ALL_STEPS 50
#define TRY_MOVE_STEPS 2

/*    ---------------------  main  ---------------------------  */
int main(int argc, char *argv[]) {
    int index, cycles = 20;

    if (argc < 3) {
        log_msg(1, "Usage: %s <vset.v> <smpl.s> <report.rep>", argv[1]);
        exit(2);
    }

    initialize(1, 0, 8);   

    index = load_vset(argv[1]);
    if (index < 0) {
        log_msg(2, "Error[ %d ] reading vset: %s", index, argv[1]);
        exit(2);
    }

    index = load_sample(argv[2]);
    if (index < 0) {
        log_msg(2, "Error[ %d ] reading sample: %s", index, argv[2]);
        exit(2);
    }
    classify(cycles, DO_ALL_STEPS, TRY_MOVE_STEPS, 0.01);  // % tolerance of 0.01 % for convergence of cost
   
    // display tree and classes
    print_tree();
    print_class(-2, 1);
    show_population();

    if (argc == 4) {
        item_list(argv[3]);
    }
}
