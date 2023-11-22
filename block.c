#define NOTGLOB 1
/*     ------------------  functions to allocate Popln and Sample space --*/
#define BLOCK 1
#include "glob.c"

static Sw allocated = 0;	/*  Total block space allocated  */
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
void *gtsp (gr, size)
	Sw gr, size;
{
	Block *blk;

	blk = (Block*) malloc (size + SpUnit);
	if (! blk)	{
		printf ("No more memory available\n");
		return (0);
		}
	blk->size = size;
	allocated += size;

	switch (gr)	{
case 0:
	blk->nblock = ctx.sample->sblks;
	ctx.sample->sblks = blk;
	break;
case 1:
	blk->nblock = ctx.popln->pblks;
	ctx.popln->pblks = blk;
	break;
case 2:
	blk->nblock = ctx.popln->jblks;
	ctx.popln->jblks = blk;
	break;
case 3:
	blk->nblock = ctx.vset->vblks;
	ctx.vset->vblks = blk;
	break;
	}	/* End of switch */
	return ((void*) (((char*) blk) + SpUnit));
}


/*	----------------------  freesp (gr)----------------- */
/*	To free all blocks on chain 'gr' (0=sample, 1=popln,
			2 = popln:sample,  3 = variable-set)	*/
void freesp (gr)
	Sw gr;
{
	Block *blk, *nblk;
	switch (gr)	{
case 0:	blk = ctx.sample->sblks;  ctx.sample->sblks = 0;  break;
case 1: blk = ctx.popln->pblks;  ctx.popln->pblks = 0;  break;
case 2:	blk = ctx.popln->jblks;  ctx.popln->jblks = 0;  break;
case 3:	blk = ctx.vset->vblks;  ctx.vset->vblks = 0;  break;
default:  printf ("False group value %d in freespace\n", gr);
	exit (10);
	}
	while (blk)	{
		nblk = blk->nblock;
		allocated -= blk->size;
		free (blk);
		blk = nblk;
		}
	return;
}


/*	------------------ repspace ---------------------------  */
/*	To report allocated space  */
Sw repspace (pp)
	Sw pp;
{
	if (pp) printf("Allocated space %8d chars\n", allocated);
	return (allocated);
}
