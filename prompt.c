#include <stdio.h>
#include <string.h>
#include "glob.h"

#define LL 200
FILE *co;
int i, j, k, m, seq, num;
int rk1, rk2, rseq1, rseq2;
char sline[INPUT_BUFFER_SIZE], rline[INPUT_BUFFER_SIZE];
char *stopp = "stop\n";

/*	Format of message is:
    char sequence_number
    char action_code
    char [INPUT_BUFFER_SIZE] string
    char action_code
    char sequence_number
    CR/LF
    */
/*	Code values are:
    'I': valid input line
    'A': acknowlege by hark
    */
int main() {
    int nnon;

    seq = '1';
    m = 0;
    for (i = 0; i < INPUT_BUFFER_SIZE; i++)
        sline[i] = '.';
    sline[0] = '\n';

send:
    nnon = 0;
open1:
    co = fopen("comms", "w");
    if (!co) {
        sleep(1);
        goto open1;
    }
    putc(seq, co);
    putc('I', co);
    for (i = 0; i < INPUT_BUFFER_SIZE; i++)
        putc(sline[i], co);
    putc('I', co);
    putc(seq, co);
    putc('\n', co);
    fclose(co);

receive:
    sleep(m);
open2:
    co = fopen("comms", "r");
    if (!co) {
        sleep(1);
        goto open2;
    }
    rseq1 = fgetc(co);
    rk1 = fgetc(co);
    for (i = 0; i < INPUT_BUFFER_SIZE; i++)
        rline[i] = fgetc(co);
    rk2 = fgetc(co);
    rseq2 = fgetc(co);
    fclose(co);

    /*	Check format	*/
    if ((rseq1 == rseq2) && (rk1 == rk2) && (rseq2 >= '0') && (rseq2 <= '9'))
        goto formok;
    m = 1;
    nnon++;
    if (nnon < 3)
        goto receive;
    goto send;

formok:
    /*	Have valid format.  See if just our previous message  */
    if ((rseq1 == seq) && (rk1 == 'I')) {
        m = 1;
        goto receive;
    }

    /*	See if an acknowlegement of our previous message  */
    if ((rseq1 == seq) && (rk1 == 'A'))
        goto next;
    /*	Cant make sense of it  */
    m = 1;
    goto send;

next:
    /*	If last line was 'stop', stop  */
    for (j = 0; j < 4; j++)
        if (rline[j] != stopp[j])
            goto firstch;
open3:
    co = fopen("comms", "w");
    if (!co) {
        sleep(1);
        goto open3;
    }
    fprintf(co, "Dead\n");
    fclose(co);
    exit(1);

firstch:
    for (i = 0; i < INPUT_BUFFER_SIZE - 1; i++)
        sline[i] = '.';
    sline[i] = '\n';
    i = 0;
    j = getchar();
    if ((j == ' ') || (j == '\t'))
        goto firstch;

gotchar:
    sline[i] = j;
    i++;
    if (j == '\n')
        goto gotline;
    if (i < INPUT_BUFFER_SIZE) {
        j = getchar();
        goto gotchar;
    }
    printf("From prompt: Input line is too long\n");
    while (getchar() != '\n')
        ;
    goto firstch;

gotline:
    seq++;
    if (seq > '9')
        seq = '1';
    m = 0;
    goto send;
}
