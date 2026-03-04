
#define GLOBALS 1
#include "glob.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Global variables now in SnobContext (current_ctx->...) */

/*    To assist in printing class serials  */
char *serial_to_str(SnobContext *ctx, Class *cls) {
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
void show_pop_names(SnobContext *ctx) {
  int i;
  printf("The defined models are:\n");
  for (i = 0; i < MAX_POPULATIONS; i++) {
    if (ctx->populations[i]) {
      printf("%2d %s", i + 1, ctx->populations[i]->name);
      if (ctx->populations[i]->sample_size)
        printf(" Sample %s", ctx->populations[i]->sample_name);
      else
        printf(" (unattached)");
      printf("\n");
    }
  }
  printf("Also, the pseudonym \"BST_\" can be used for the best\n");
  printf("model for the current sample\n");
  ctx->num_rep_chars = 0;
}

/*      ----------------------  show_smpl_names  -----------------  */
void show_smpl_names(SnobContext *ctx) {
  int k;
  printf("Loaded samples:\n");
  for (k = 0; k < MAX_SAMPLES; k++) {
    if (ctx->samples[k]) {
      printf("%2d:  %s\n", k + 1, ctx->samples[k]->name);
    }
  }
  ctx->num_rep_chars = 0;
}

/// @brief Select the sample by the given name
/// @param name name of sample to select
void select_sample(SnobContext *ctx, char *name) {
  int smpl_id, k;
  smpl_id = find_sample(ctx, name, 1);
  if (smpl_id >= 0) {
    log_msg(ctx, 1, "Selecting sample [%d] %s", smpl_id,
            ctx->samples[smpl_id]->name);
    k = copy_population(ctx, ctx->state.popln->id, 0, "OldWork");
    if (k >= 0) {
      if (ctx->state.popln) {
        destroy_population(ctx, ctx->state.popln->id);
      }
      ctx->state.popln = 0;
      ctx->state.sample = ctx->samples[smpl_id];
      log_msg(ctx, 0, "Preparing Initial Population for sample [%d] %s",
              smpl_id, ctx->samples[smpl_id]->name);
      k = init_population(ctx);
      if (k < 0) {
        log_msg(ctx, 1, "Cannot make first population for sample");
        return;
      }
    } else {
      log_msg(ctx, 1, "Can't make OldWork copy of work");
    }
    select_population(ctx, "OldWork");
    cleanup_population(ctx);
  }
}

/// @brief Select a population by name
/// @param name
void select_population(SnobContext *ctx, char *name) {
  int k, p = find_population(ctx, name);
  if (p >= 0) {
    if (!strcmp(ctx->populations[p]->name, "work")) {
      if (ctx->state.popln && (ctx->state.popln->id == p)) {
        log_msg(ctx, 0, "Work already picked");
      } else {
        log_msg(ctx, 0, "Switching context to existing 'work'");
      }
      k = p;
    } else {
      k = set_work_population(ctx, p);
    }
    ctx->state.popln = ctx->populations[k];
  } else {
    log_msg(ctx, 0, "No existing population '%s'", name);
  }
  show_population(ctx);
}

void log_msg(SnobContext *ctx, int level, const char *format, ...) {

  if (level >= ctx->debug) {
    if (ctx->num_rep_chars > 0) {
      printf("\n");
    }
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    ctx->num_rep_chars = 0;
  }
}

void print_buffer(SnobContext *ctx, MemBuffer *dest, const char *format, ...) {
  va_list args;
  va_start(args, format);
  if (dest->offset < dest->size) {
    dest->offset += vsnprintf(dest->buffer + dest->offset,
                              dest->size - dest->offset, format, args);
  }
  va_end(args);
}

int error_value(SnobContext *ctx, const char *message, const int value) {
  log_msg(ctx, 1, message);
  return value;
}

#undef STOP
__thread SnobContext *signal_ctx = NULL;
void handle_sigint(int sig) {
  if (signal_ctx)
    signal_ctx->stop = 1;
}
#define STOP (ctx->stop)

/// @brief Initialize SNOB parameters
/// @param interact integer specifying if running from a library or not 1 =
/// interactive, 0 = non interactive
/// @param debug turn on verbose printing of progress
/// @param threads number of threads to use during parallel portions, 0 = use
/// OpenMP environment variables instead

static int Initialized = 0;

SnobContext *initialize(int interact, int debug, int seed) {
  int k;
  SnobContext *ctx = 0;
  if (!ctx) {
    ctx = (SnobContext *)calloc(1, sizeof(SnobContext));
  }
  signal_ctx = ctx;
  ctx->interactive = interact;
  ctx->debug = debug;
  ctx->stop = 0;

  ctx->see_all = 2;
  ctx->fix = ctx->d_fix = Partial;
  ctx->d_control = ctx->control = AdjAll;

  if (seed != 0) {
    ctx->random_seed = seed;
  } else {
    ctx->random_seed = time(NULL);
  }

  if (!Initialized) {
    signal(SIGINT, handle_sigint);
    default_tune(ctx);
    do_types(ctx);

    for (k = 0; k < MAX_POPULATIONS; k++)
      ctx->populations[k] = 0;
    for (k = 0; k < MAX_SAMPLES; k++)
      ctx->samples[k] = 0;
    for (k = 0; k < MAX_VSETS; k++)
      ctx->var_sets[k] = 0;
  } else {
    // Cleanup
    for (k = 0; k < MAX_POPULATIONS; k++)
      destroy_population(ctx, k);
    for (k = 0; k < MAX_SAMPLES; k++)
      destroy_sample(ctx, k);
    for (k = 0; k < MAX_VSETS; k++)
      destroy_vset(ctx, k);
  }
  Initialized = 1;
  return ctx;
}

void reset(SnobContext *ctx) {
  int k;
  if (!ctx) {
    ctx = (SnobContext *)calloc(1, sizeof(SnobContext));
  }
  ctx->random_seed = 1234567;
  ctx->see_all = 2;
  ctx->fix = ctx->d_fix = Partial;
  ctx->d_control = ctx->control = AdjAll;

  if (Initialized) {
    for (k = 0; k < MAX_POPULATIONS; k++)
      destroy_population(ctx, k);
    for (k = 0; k < MAX_SAMPLES; k++)
      destroy_sample(ctx, k);
    for (k = 0; k < MAX_VSETS; k++)
      destroy_vset(ctx, k);
  }
}

/// @brief Print the details about the number of classes, leaves, and the
/// associated costs for the current population
void show_population(SnobContext *ctx) {
  Class *root;
  Population *popln = ctx->state.popln;
  Sample *sample = ctx->state.sample;

  root = popln->classes[popln->root];
  log_msg(ctx, 2,
          "--------------------------------------------------------------------"
          "------------");
  if (popln->sample_size) {
    log_msg(
        ctx, 2,
        "P%1d  %4d classes, %4d leaves,  Pcost%8.1f  Tcost%10.1f,  Cost%10.1f",
        popln->id + 1, popln->num_classes, popln->num_leaves,
        root->best_par_cost, root->best_case_cost, root->best_cost);
  } else {
    log_msg(ctx, 2, "P%1d  %4d classes, %4d leaves,  Pcost%8.1f", popln->id + 1,
            popln->num_classes, popln->num_leaves, root->best_par_cost);
  }
  log_msg(ctx, 2, "Sample %2d %s", (sample) ? sample->id + 1 : 0,
          (sample) ? sample->name : "NULL");
  log_msg(ctx, 2,
          "--------------------------------------------------------------------"
          "------------");
}

void cleanup_population(SnobContext *ctx) {
  int index = find_population(ctx, "TrialPop");
  if (index >= 0) {
    destroy_population(ctx, index);
  }
  ctx->fix = ctx->d_fix;
  ctx->control = ctx->d_control;
  tidy(ctx, 1, ctx->no_subs);
  track_best(ctx, 1);
}

void print_progress(SnobContext *ctx, size_t count, size_t max) {
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
/// @param max_cycles Maximum number of full cycles of doall followed by
/// trymoves
/// @param do_steps Number of doall steps of assignment and estimation
/// @param move_steps Number of trymoves steps to fix the classification tree
/// @param tol  Convergence tolerance as a percentage. Classification stops if
/// the cost improvement is less than the tolerance
/// @return Classification Result Structure containing number of classes and
/// cost
Result classify(SnobContext *ctx, const int max_cycles, const int do_steps,
                const int move_steps, const double tol) {
  Result result;
  Class *root;
  int cycle = 0, prev_classes = 0, prev_leaves = 0, no_change_count = 0;

  init_population(ctx);
  cleanup_population(ctx);
  print_class(ctx, ctx->state.popln->root, 0);

  root = ctx->state.popln->classes[ctx->state.popln->root];
  double cost = root->best_cost, delta = 0.0;
  do {
    log_msg(ctx, 1, "\n");
    log_msg(ctx, 1,
            "=================================================================="
            "==============");
    log_msg(ctx, 1,
            "Cycle %d | %d steps of costing, assignment and adjustments",
            1 + cycle, do_steps);
    log_msg(ctx, 1,
            "=================================================================="
            "==============");
    do_all(ctx, do_steps, 1);
    cleanup_population(ctx);

    log_msg(ctx, 1, "Attempting class moves until %d successive failures",
            move_steps);
    try_moves(ctx, move_steps);
    cleanup_population(ctx);

    log_msg(ctx, 1, "Cost dropped by %8.3f%%", delta);
    show_population(ctx);
    root = ctx->state.popln->classes[ctx->state.popln->root];
    delta = fabs(100.0 * (cost - root->best_cost) / cost);
    if ((ctx->state.popln->num_classes > 1)) {
      // test convergence if we are not at the beginning
      if ((prev_classes == ctx->state.popln->num_classes) &&
          (prev_leaves == ctx->state.popln->num_leaves) && (delta < tol)) {
        no_change_count++;
      } else {
        no_change_count = 0;
      }
    }
    prev_classes = ctx->state.popln->num_classes;
    prev_leaves = ctx->state.popln->num_leaves;
    cost = root->best_cost;
    cycle++;
    if ((no_change_count > 2) || (ctx->stop)) {
      break;
    }

  } while (cycle < max_cycles);

  if (cycle >= max_cycles) {
    log_msg(ctx, 1, "WARNING: Classification did not converge after %d cycles",
            max_cycles);
  } else if (ctx->stop) {
    log_msg(ctx, 1, "WARNING: Classification interrupted after %d cycles",
            cycle);
  } else {
    log_msg(ctx, 1, "Classification converged after %d cycles", cycle);
  }

  //  Prepare return structure
  result.num_classes = ctx->state.popln->num_classes;
  result.num_leaves = ctx->state.popln->num_leaves;
  result.model_length = root->best_par_cost;
  result.data_length = root->best_case_cost;
  result.message_length = root->best_cost;
  result.num_attrs = ctx->state.vset->length;
  result.num_cases = ctx->state.sample->num_cases;

  return result;
}

/// @brief Save a Classification Model to file
/// @param filename model file
/// @return >= 0 if successful
int save_model(SnobContext *ctx, char *filename) {
  int best;
  best = copy_population(ctx, get_best_pop(ctx), 0, "SavedModel");
  return save_population(ctx, best, 0, filename);
}

int load_model(SnobContext *ctx, char *filename) {
  int result;
  result = load_population(ctx, filename);
  return set_work_population(ctx, result);
}

void save_context(SnobContext *ctx) {
  memcpy(&ctx->state_backup, &ctx->state, sizeof(State));
}
void restore_context(SnobContext *ctx) {
  memcpy(&ctx->state, &ctx->state_backup, sizeof(State));
}
void set_control_flags(SnobContext *ctx, int flags) {
  ctx->control = ctx->d_control = flags;
}