
// MAIN FILE
#include "snob.h"
#include <getopt.h>
#include <stdarg.h>
#include <time.h>

#define DO_ALL_STEPS 50
#define TRY_MOVE_STEPS 4
#define FIT_CYCLES 25

/**
 * @param argc
 * @param argv
 */
int main(int argc, char *argv[]) {
    int index, cycles = FIT_CYCLES, steps = DO_ALL_STEPS, moves = TRY_MOVE_STEPS;
    int log_level = 1;
    int opt;

    while ((opt = getopt(argc, argv, "hvc:s:m:")) != -1) {
        switch (opt) {
        case 'h':
            printf("Usage: %s [OPTIONS] <vset.v> <smpl.s> [report.rep]\n", argv[0]);
            printf("Options:\n");
            printf("  -h          Print this help message\n");
            printf("  -v          Verbose Logging (default: off)\n");
            printf("  -c CYCLES   Set number of fit cycles (default: %d)\n", FIT_CYCLES);
            printf("  -s STEPS    Set number of EM steps per cycle (default: %d)\n", DO_ALL_STEPS);
            printf("  -m MOVES    Set number of try moves per cycle (default: %d)\n", TRY_MOVE_STEPS);
            exit(0);
        case 'v':
            log_level = 0;
            break;
        case 'c':
            cycles = atoi(optarg);
            break;
        case 's':
            steps = atoi(optarg);
            break;
        case 'm':
            moves = atoi(optarg);
            break;
        default:
            fprintf(stderr,
                    "Usage: %s [-h -v -c CYCLES -s STEPS -m MOVES] <vset.v> "
                    "<smpl.s> [report.rep]\n",
                    argv[0]);
            exit(2);
        }
    }

    if (optind + 2 > argc) {
        fprintf(stderr,
                "Usage: %s [-h -v -c CYCLES -s STEPS -m MOVES] <vset.v> "
                "<smpl.s> [report.rep]\n",
                argv[0]);
        exit(2);
    }

    char *vset_file = argv[optind];
    char *sample_file = argv[optind + 1];
    char *report_file = (optind + 2 < argc) ? argv[optind + 2] : NULL;

    SnobContext *ctx = initialize(0, log_level, 0);

    clock_t cpu_start, cpu_end;
    struct timespec wall_start, wall_end;
    double cpu_time, wall_time;

    // Record start time
    cpu_start = clock();
    timespec_get(&wall_start, TIME_UTC);

    log_msg(ctx, 1,
            "####################################################################"
            "############");
    log_msg(ctx, 1,
            "Factor SNOB - Mixture Modelling by Minimum Message Length (MML) "
            "with Factors");
    log_msg(ctx, 1,
            "####################################################################"
            "############");

    log_msg(ctx, 1, "Loading vset: %s", vset_file);
    index = load_vset(ctx, vset_file);
    if (index < 0) {
        log_msg(ctx, 2, "Error[ %d ] reading vset: %s", index, vset_file);
        exit(2);
    }

    log_msg(ctx, 1, "Loading sample: %s", sample_file);
    index = load_sample(ctx, sample_file);
    if (index < 0) {
        log_msg(ctx, 2, "Error[ %d ] reading sample: %s", index, sample_file);
        exit(2);
    }
    peek_data(ctx);
    classify(ctx, cycles, steps, moves,
             0.01); // % tolerance of 0.01 % for convergence of cost

    // display tree and classes
    print_class(ctx, -2, 1);
    show_population(ctx);

    if (report_file) {
        item_list(ctx, report_file);
    }

    // Report time used
    cpu_end = clock();
    timespec_get(&wall_end, TIME_UTC);
    cpu_time = ((double)(cpu_end - cpu_start)) / CLOCKS_PER_SEC;
    wall_time = (wall_end.tv_sec - wall_start.tv_sec) + (wall_end.tv_nsec - wall_start.tv_nsec) / 1E9;

    log_msg(ctx, 1, "CPU Time:     %10.3f s", cpu_time);
    log_msg(ctx, 1, "Elapsed Time: %10.3f s", wall_time);

    // Free context
    destroy_context(ctx);

    return 0;
}