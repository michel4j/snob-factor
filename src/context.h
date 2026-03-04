typedef struct SnobContextStruct {
  extern
#else
#define EXT
#endif

      /*	mathematical constants   */
      EXT double ctx->half_log_2pi,
      ctx->half_log_2, ctx->lattice, ctx->pi, ctx->bit, ctx->twobit, ctx->two_on_pi, ctx->half_pi;
  double ctx->zero_vec[MAX_ZERO];
  double ctx->fac_log[MAX_CLASSES + 1];
  volatile sig_atomic_t ctx->stop;
  int ctx->num_types;
  VarType *ctx->types;
  Context state, state_backup;
  VarSet *ctx->var_sets[MAX_VSETS];
  Sample *ctx->samples[MAX_SAMPLES];
  Population *ctx->populations[MAX_POPULATIONS];
  Buffer *ctx->current_source;
  int ctx->heard;
  int ctx->use_stdin;
  int ctx->interactive;
  int ctx->debug;
  int ctx->control, ctx->d_control;
  int ctx->d_fix, ctx->fix;
  int ctx->num_rep_chars;
  Score ctx->scores;
  int ctx->random_seed;
  int ctx->no_subs;
  int ctx->new_subs;
  Class *ctx->sons[MAX_CLASSES];
  int ctx->next_ic[MAX_CLASSES];
  int ctx->min_age;
  int ctx->min_fac_age;
  int ctx->min_sub_age;
  int ctx->max_sub_age;
  int ctx->hold_time;
  int ctx->forever;
  double ctx->min_size;
  double ctx->min_weight;
  double ctx->min_sub_weight;
  int ctx->sig_score_change;
  int ctx->see_all;
  int ctx->dont_ignore;
  int ctx->score_changes;
  int ctx->new_subs_time;
  double ctx->initial_adj;
  double ctx->max_adj;
  double ctx->min_gain;
  double ctx->m_beta;
  double ctx->b_beta;
  int ctx->root_age;
  int ctx->give_up;
  int ctx->bad_key[BadSize];
} SnobContext;

#ifdef USE_SNOB_CONTEXT_MACROS
#define ctx->half_log_2pi (ctx->half_log_2pi)
#define ctx->half_log_2 (ctx->half_log_2)
#define ctx->lattice (ctx->lattice)
#define ctx->pi (ctx->pi)
#define ctx->bit (ctx->bit)
#define ctx->twobit (ctx->twobit)
#define ctx->two_on_pi (ctx->two_on_pi)
#define ctx->half_pi (ctx->half_pi)
#define ctx->zero_vec (ctx->zero_vec)
#define ctx->fac_log (ctx->fac_log)
#define ctx->stop (ctx->stop)
#define ctx->num_types (ctx->num_types)
#define ctx->types (ctx->types)
#define CurCtx (ctx->CurCtx)
#define BkpCtx (ctx->BkpCtx)
#define ctx->var_sets (ctx->var_sets)
#define ctx->samples (ctx->samples)
#define ctx->populations (ctx->populations)
#define ctx->current_source (ctx->current_source)
#define ctx->heard (ctx->heard)
#define ctx->use_stdin (ctx->use_stdin)
#define ctx->interactive (ctx->interactive)
#define ctx->debug (ctx->debug)
#define ctx->control (ctx->control)
#define ctx->d_control (ctx->d_control)
#define ctx->d_fix (ctx->d_fix)
#define ctx->fix (ctx->fix)
#define ctx->num_rep_chars (ctx->num_rep_chars)
#define ctx->scores (ctx->scores)
#define ctx->random_seed (ctx->random_seed)
#define ctx->no_subs (ctx->no_subs)
#define ctx->new_subs (ctx->new_subs)
#define ctx->sons (ctx->sons)
#define ctx->next_ic (ctx->next_ic)
#define ctx->min_age (ctx->min_age)
#define ctx->min_fac_age (ctx->min_fac_age)
#define ctx->min_sub_age (ctx->min_sub_age)
#define ctx->max_sub_age (ctx->max_sub_age)
#define ctx->hold_time (ctx->hold_time)
#define ctx->forever (ctx->forever)
#define ctx->min_size (ctx->min_size)
#define ctx->min_weight (ctx->min_weight)
#define ctx->min_sub_weight (ctx->min_sub_weight)
#define ctx->sig_score_change (ctx->sig_score_change)
#define ctx->see_all (ctx->see_all)
#define ctx->dont_ignore (ctx->dont_ignore)
#define ctx->score_changes (ctx->score_changes)
#define ctx->new_subs_time (ctx->new_subs_time)
#define ctx->initial_adj (ctx->initial_adj)
#define ctx->max_adj (ctx->max_adj)
#define ctx->min_gain (ctx->min_gain)
#define ctx->m_beta (ctx->m_beta)
#define ctx->b_beta (ctx->b_beta)
#define ctx->root_age (ctx->root_age)
#define ctx->give_up (ctx->give_up)
#define ctx->bad_key (ctx->bad_key)
#endif
