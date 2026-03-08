/*	A routine to initialize the VarType list "types"
    It must be added to for every new type added, by adding a call
on XXXdefine() where XXX is the prefix of the fun XXXdefine in the file
of functions for the new type.
    The routine also sets some global geometric constants.
*/

#include "snob.h"

void reals_define(SnobContext *ctx, int typindx);
void expmults_define(SnobContext *ctx, int typindx);
void expbinary_define(SnobContext *ctx, int typindx);
void vonm_define(SnobContext *ctx, int typindx);

void do_types(SnobContext *ctx) {
    ctx->num_types = 4;

    for (int i = 0; i < MAX_ZERO; i++) {
        ctx->zero_vec[i] = 0.0;
    }

    ctx->types = (VarType *)malloc(ctx->num_types * sizeof(VarType));

    reals_define(ctx, 0);
    expmults_define(ctx, 1);
    expbinary_define(ctx, 2);
    vonm_define(ctx, 3);

    return;
}
