// stuff to tune actions
#define NOTGLOB 1
#define TUNE
#include "glob.h"

/**
 */
void default_tune(SnobContext *ctx) {
    int i;

    ctx->min_age = 4;
    ctx->min_fac_age = 11;
    ctx->min_sub_age = 6;
    ctx->max_sub_age = 50;
    ctx->hold_time = 15;
    ctx->forever = 100;
    ctx->min_size = 4.0;
    ctx->new_subs_time = 10;
    ctx->initial_adj = 0.3;
    ctx->max_adj = 1.3;
    ctx->root_age = 25;
    ctx->min_gain = 0.01;
    ctx->give_up = 6;
    ctx->min_weight = 0.005;
    ctx->min_sub_weight = 0.01;
    ctx->sig_score_change = 5;
    ctx->m_beta = 0.00;

    //	Set table of log factorials
    ctx->fac_log[0] = ctx->fac_log[1] = 0.0;
    for (i = 2; i <= MAX_CLASSES; i++)
        ctx->fac_log[i] = ctx->fac_log[i - 1] + log((double)i);
    return;
}
