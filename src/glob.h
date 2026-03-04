#include "snob.h"

/*	--------------  Global variables declared here  ----------------  */

typedef struct SnobContextStruct {
  /* mathematical constants */
  double half_log_2pi, half_log_2, lattice, pi, bit, twobit, two_on_pi, half_pi;
  double zero_vec[MAX_ZERO];
  double fac_log[MAX_CLASSES + 1];
  volatile sig_atomic_t stop;

  /* general */
  int num_types;
  VarType *types;
  State state, state_backup;
  VarSet *var_sets[MAX_VSETS];
  Sample *samples[MAX_SAMPLES];
  Population *populations[MAX_POPULATIONS];

  /* re inputs for main */
  Buffer *current_source;

  /* re hark */
  int heard;
  int use_stdin;
  int interactive;
  int debug;
  int control, d_control;
  int d_fix, fix;
  int num_rep_chars;

  Score scores;

  /* re Doall */
  int random_seed;
  int no_subs;
  int new_subs;
  Class *sons[MAX_CLASSES];
  int next_ic[MAX_CLASSES];

  /* re Tuning */
  int min_age, min_fac_age, min_sub_age, max_sub_age;
  int hold_time, forever;
  double min_size, min_weight, min_sub_weight;
  int sig_score_change;
  int see_all;
  int dont_ignore;
  int score_changes;
  int new_subs_time;
  double initial_adj, max_adj, min_gain, m_beta, b_beta;
  int root_age, give_up;

  /* re Badmoves */
  int bad_key[BadSize];
} SnobContext;
