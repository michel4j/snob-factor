
/*     ------------------  functions to allocate Popln and Sample space --*/
#define BLOCK 1
#include "glob.h"

static int allocated = 0; /*  Total block space allocated  */
#define SpUnit 16
/*	------------------------  gtsp  ----------------------------  */
/*	To allocate a block in a Popln chain  */
/*	Provides a space of 'size' chars on a chain selected by 'gr'
        gr = 3:  variable-set chain.
        gr = 0: sample chain. gr = 1: popln chain. gr = 2:  chain for
        (popln:sample) pair.  The actual space allocated is
        size + SpUnit, where SpUnit must be the smallest power of 2
        required for item alignment, or some larger power of 2.
        */
void *alloc_blocks(int gr, int size) {
    Block *blk;

    blk = (Block *)malloc(size + SpUnit);
    if (!blk) {
        printf("No more memory available\n");
        return (0);
    }
    blk->size = size;
    allocated += size;

    switch (gr) {
    case 0:
        blk->next = ctx.sample->blocks;
        ctx.sample->blocks = blk;
        break;
    case 1:
        blk->next = ctx.popln->blocks;
        ctx.popln->blocks = blk;
        break;
    case 2:
        blk->next = ctx.popln->model_blocks;
        ctx.popln->model_blocks = blk;
        break;
    case 3:
        blk->next = ctx.vset->blocks;
        ctx.vset->blocks = blk;
        break;
    } /* End of switch */
    return ((void *)(((char *)blk) + SpUnit));
}

/*	----------------------  freesp (gr)----------------- */
/*	To free all blocks on chain 'gr' (0=sample, 1=popln,
                        2 = popln:sample,  3 = variable-set)	*/
void free_blocks(int gr) {
    Block *blk, *nblk;
    switch (gr) {
    case 0:
        blk = ctx.sample->blocks;
        ctx.sample->blocks = 0;
        break;
    case 1:
        blk = ctx.popln->blocks;
        ctx.popln->blocks = 0;
        break;
    case 2:
        blk = ctx.popln->model_blocks;
        ctx.popln->model_blocks = 0;
        break;
    case 3:
        blk = ctx.vset->blocks;
        ctx.vset->blocks = 0;
        break;
    default:
        printf("False group value %d in freespace\n", gr);
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

/*	------------------ report_space ---------------------------  */
/*	To report allocated space  */
int report_space(pp)
int pp;
{
    if (pp)
        printf("Allocated space %8d chars\n", allocated);
    return (allocated);
}
