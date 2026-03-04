/*	A routine to initialize the VarType list "types"
    It must be added to for every new type added, by adding a call
on XXXdefine() where XXX is the prefix of the fun XXXdefine in the file
of functions for the new type.
    The routine also sets some global geometric constants.
    */

#include "glob.h"

void reals_define(SnobContext *ctx, int typindx);
void expmults_define(SnobContext *ctx, int typindx);
void expbinary_define(SnobContext *ctx, int typindx);
void vonm_define(SnobContext *ctx, int typindx);

void do_types(SnobContext *ctx) {
    int i;

    /*	Set the number of attribute types  */
    ctx->num_types = 4;

    /*	Set constants  */
    ctx->pi = 4.0 * atan(1.0);
    ctx->half_log_2pi = 0.5 * log(2.0 * ctx->pi);
    ctx->two_on_pi = 2.0 / ctx->pi;
    ctx->half_pi = 0.5 * ctx->pi;
    ctx->bit = log(2.0);
    ctx->twobit = 2.0 * ctx->bit;
    ctx->half_log_2 = 0.5 * log(2.0);
    ctx->lattice = -0.5 * log(12.0);
    for (i = 0; i < MAX_ZERO; i++)
        ctx->zero_vec[i] = 0.0;

    /*	Make the 'types' vector  */
    ctx->types = (VarType *)malloc(ctx->num_types * sizeof(VarType));

    reals_define(ctx, 0);
    expmults_define(ctx, 1);
    expbinary_define(ctx, 2);
    vonm_define(ctx, 3);

    return;
}
