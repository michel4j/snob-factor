
/*    ---------------------  MAIN FILE -----------------  */
#include "snob.h"
#include <stdarg.h>
#include <time.h>

#define DO_ALL_STEPS 50
#define TRY_MOVE_STEPS 4

/*    ---------------------  main  ---------------------------  */
int main(int argc, char *argv[]) {
    int index, cycles = 25;
    SnobContext *ctx = initialize(0, 0, 0);

    clock_t cpu_start, cpu_end;
    struct timespec wall_start, wall_end;
    double cpu_time, wall_time;

    if (argc < 3) {
        log_msg(ctx, 1, "Usage: %s <vset.v> <smpl.s> <report.rep>", argv[1]);
        exit(2);
    }

    // Record start time 
    cpu_start = clock();
    timespec_get(&wall_start, TIME_UTC);

    log_msg(ctx, 1, "################################################################################");
    log_msg(ctx, 1, "Factor SNOB - Mixture Modelling by Minimum Message Length (MML) with Factors");
    log_msg(ctx, 1, "################################################################################");

    log_msg(ctx, 1, "Loading vset: %s", argv[1]);
    index = load_vset(ctx, argv[1]);
    if (index < 0) {
        log_msg(ctx, 2, "Error[ %d ] reading vset: %s", index, argv[1]);
        exit(2);
    }

    log_msg(ctx, 1, "Loading sample: %s", argv[2]);
    index = load_sample(ctx, argv[2]);
    if (index < 0) {
        log_msg(ctx, 2, "Error[ %d ] reading sample: %s", index, argv[2]);
        exit(2);
    }    
    peek_data(ctx);
    classify(ctx, cycles, DO_ALL_STEPS, TRY_MOVE_STEPS, 0.01);  // % tolerance of 0.01 % for convergence of cost
   
    // display tree and classes
    print_class(ctx, -2, 1);
    show_population(ctx);

    if (argc == 4) {
        item_list(ctx, argv[3]);
    }

    // Report time used
    cpu_end = clock();
    timespec_get(&wall_end, TIME_UTC);
    cpu_time = ((double) (cpu_end - cpu_start)) / CLOCKS_PER_SEC;
    wall_time = (wall_end.tv_sec - wall_start.tv_sec) +  (wall_end.tv_nsec - wall_start.tv_nsec) / 1E9;

    log_msg(ctx, 1, "CPU Time:     %10.3f s", cpu_time);
    log_msg(ctx, 1, "Elapsed Time: %10.3f s", wall_time);
}