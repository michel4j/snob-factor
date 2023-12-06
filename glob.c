/*	This file contains type definitions for generic and global structures,
and declarations of cetain global variables. The file is intended to be
included in many other files, as well as being compiled and loaded itself.
    For variables, arrays etc defined herein, the instantiation is
suppressed when included in other files by #define NOTGLOB 1 in those other
files. The declarations herein then become converted to " declarations.
    */

#define GLOBALS 1
#include "glob.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*	To assist in printing class serials  */
char decsers[8];

char *serial_to_str(Class *cll) {
    int i, j, k;
    for (i = 0; i < 6; i++) decsers[i] = ' ';
    decsers[6] = 0;
    if (!cll)
        goto done;
    if (cll->type == Vacant)
        goto done;
    j = cll->serial;
    k = j & 3;
    if (k == 1)
        decsers[5] = 'a';
    else if (k == 2)
        decsers[5] = 'b';
    j = j >> 2;
    decsers[4] = '0';
    for (i = 4; i > 0; i--) {
        if (j <= 0)
            goto done;
        decsers[i] = j % 10 + '0';
        j = j / 10;
    }
    if (j) {
        strcpy(decsers + 1, "2BIG");
    }
done:
    return (decsers);
}