
/*     ------------------  functions to allocate Popln and Sample space --*/
#define BLOCK 1
#include "glob.h"

static int AllocatedBlockSpace = 0; /*  Total block space AllocatedBlockSpace  */
#define SpUnit 16
/*    ------------------------  alloc_blocks  ----------------------------  */
/*    To allocate a block in a Popln chain  */
/*    Provides a space of 'size' chars on a chain selected by 'gr'
        gr = 3:  variable-set chain.
        gr = 0: sample chain. gr = 1: popln chain. gr = 2:  chain for
        (popln:sample) pair.  The actual space AllocatedBlockSpace is
        size + SpUnit, where SpUnit must be the smallest power of 2
        required for item alignment, or some larger power of 2.
        */
void *alloc_blocks(int gr, int size) {
    Block *blk;

    blk = (Block *)malloc(size + SpUnit);
    if (!blk) {
        log_msg(2, "No more memory available\n");
        return (0);
    }
    blk->size = size;
    AllocatedBlockSpace += size;

    switch (gr) {
    case 0:
        blk->next = CurCtx.sample->blocks;
        CurCtx.sample->blocks = blk;
        break;
    case 1:
        blk->next = CurCtx.popln->blocks;
        CurCtx.popln->blocks = blk;
        break;
    case 2:
        blk->next = CurCtx.popln->model_blocks;
        CurCtx.popln->model_blocks = blk;
        break;
    case 3:
        blk->next = CurCtx.vset->blocks;
        CurCtx.vset->blocks = blk;
        break;
    } /* End of switch */
    return ((void *)(((char *)blk) + SpUnit));
}

/*    ----------------------  free_blocks (gr)----------------- */
/*    To free all blocks on chain 'gr' (0=sample, 1=popln,
    2 = popln:sample,  3 = variable-set)    */
void free_blocks(int gr) {
    Block *blk, *nblk;
    switch (gr) {
    case 0:
        blk = CurCtx.sample->blocks;
        CurCtx.sample->blocks = 0;
        break;
    case 1:
        blk = CurCtx.popln->blocks;
        CurCtx.popln->blocks = 0;
        break;
    case 2:
        blk = CurCtx.popln->model_blocks;
        CurCtx.popln->model_blocks = 0;
        break;
    case 3:
        blk = CurCtx.vset->blocks;
        CurCtx.vset->blocks = 0;
        break;
    default:
        log_msg(0, "False group value %d in freespace", gr);
        exit(10);
    }
    while (blk) {
        nblk = blk->next;
        AllocatedBlockSpace -= blk->size;
        free(blk);
        blk = nblk;
    }
}

/*    ------------------ report_space ---------------------------  */
/*    To report allocated space  */
int report_space(int pp)
{
    if (pp)
        log_msg(1, "Allocated space %8d chars", AllocatedBlockSpace);
    return (AllocatedBlockSpace);
}
