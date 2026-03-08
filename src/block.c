
// functions to allocate Popln and Sample space
#define BLOCK 1
#include "snob.h"
#define SpUnit 16

static int allocated = 0; //  Total block space allocated

/**
 * @brief To allocate a block in a Popln chain
 * Provides a space of 'size' chars on a chain selected by 'chain'. The actual space allocated is
 * size + SpUnit, where SpUnit must be the smallest power of 2
 * required for item alignment, or some larger power of 2.
 *
 * @param ctx Pointer to the Snob context.
 * @param chain Chain number, 0 = sample, 1 = popln, 2 = model, 3 = variable-set
 * @param size Size or length.
 */
void *alloc_blocks(SnobContext *ctx, int chain, int size) {
    Block *blk;

    blk = (Block *)malloc(size + SpUnit);
    if (!blk) {
        printf("No more memory available\n");
        return (0);
    }
    blk->size = size;
    allocated += size;

    switch (chain) {
    case 0:
        blk->next = ctx->state.sample->blocks;
        ctx->state.sample->blocks = blk;
        break;
    case 1:
        blk->next = ctx->state.popln->blocks;
        ctx->state.popln->blocks = blk;
        break;
    case 2:
        blk->next = ctx->state.popln->model_blocks;
        ctx->state.popln->model_blocks = blk;
        break;
    case 3:
        blk->next = ctx->state.vset->blocks;
        ctx->state.vset->blocks = blk;
        break;
    } // End of switch
    return ((void *)(((char *)blk) + SpUnit));
}

/**
 * @brief To free all blocks on chain 'chain'
 * @param ctx Pointer to the Snob context.
 * @param chain Chain number, 0 = sample, 1 = popln, 2 = model, 3 = variable-set
 */
void free_blocks(SnobContext *ctx, int chain) {
    Block *blk, *nblk;
    switch (chain) {
    case 0:
        blk = ctx->state.sample->blocks;
        ctx->state.sample->blocks = 0;
        break;
    case 1:
        blk = ctx->state.popln->blocks;
        ctx->state.popln->blocks = 0;
        break;
    case 2:
        blk = ctx->state.popln->model_blocks;
        ctx->state.popln->model_blocks = 0;
        break;
    case 3:
        blk = ctx->state.vset->blocks;
        ctx->state.vset->blocks = 0;
        break;
    default:
        printf("False group value %d in freespace\n", chain);
        exit(10);
    }
    while (blk) {
        nblk = blk->next;
        allocated -= blk->size;
        free(blk);
        blk = nblk;
    }
    return;
}
