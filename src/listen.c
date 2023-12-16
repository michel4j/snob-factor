/*	---------------------  hark  --------------------------------  */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "glob.h"

#define LI 200
/*	Listens for instruction via file "comms"    */
static int seq = '0';

int hark(char *lline) {
    FILE *co;
    int i, k, m;
    int rk1, rk2, rseq1, rseq2;
    int nnon;

    /*	Format of message is:
        char sequence_number
        char action_code
        char [LI] string
        char action_code
        char sequence_number
        CR/LF
        */
    /*	Code values are:
        'I': valid input line
        'A': acknowlege by hark
        */

    nnon = k = 0;
    m = 0;
    goto receive;

send:
    nnon = 0;
open1:
    co = fopen("comms", "w");
    if (!co) {
        sleep(1);
        goto open1;
    }
    putc(seq, co);
    putc('A', co);
    for (i = 0; i < LI; i++)
        putc(lline[i], co);
    putc('A', co);
    putc(seq, co);
    putc('\n', co);
    fclose(co);
finish:
    Heard = k;
    return (k);

receive:
    sleep(m);
    co = fopen("comms", "r");
    if (!co)
        return (-1);
    rseq1 = fgetc(co);
    rk1 = fgetc(co);
    for (i = 0; i < LI; i++)
        lline[i] = fgetc(co);
    rk2 = fgetc(co);
    rseq2 = fgetc(co);
    fclose(co);

    /*	Check format	*/
    if ((rseq1 == rseq2) && (rk1 == rk2) && (rseq2 >= '0') && (rseq2 <= '9'))
        goto formok;
    if (rseq1 == 'D')
        return (-1);
    m = 1;
    nnon++;
    if (nnon < 3)
        goto receive;
    goto send;

formok:
    /*	Have valid format.  See if just our previous message  */
    if ((rseq1 == seq) && (rk1 == 'A')) {
        k = 0;
        goto finish;
    }

    if (rk1 == 'I') {
        /*	See if a new message  */
        if (rseq1 != seq) {
            seq = rseq1;
            k = 1;
        }
    }
    goto send;
}

/*	----------------------- testmain ---------------------------  */
#ifdef TEST
main() {
    int i, j, kk;
    char gl[LI];

loop:
    kk = hark(gl);
    if (!kk) {
        sleep(1);
        goto loop;
    }
    i = 0;
outl:
    j = gl[i];
    i++;
    putchar(j);
    if (j != '\n')
        goto outl;
    goto loop;
}
#endif
