
#define GLOBALS 1
#include "glob.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*    To assist in printing class serials  */


char *serial_to_str(Class *cls) {
    static char str[8];
    int i, j, k;
    for (i = 0; i < 6; i++) {
        str[i] = ' ';
    }
    str[6] = 0;
    if (cls && cls->type != Vacant) {
        j = cls->serial;
        k = j & 3;
        if (k == 1) {
            str[5] = 'a';
        } else if (k == 2) {
            str[5] = 'b';
        }
        j = j >> 2;
        str[4] = '0';
        for (i = 4; i > 0; i--) {
            if (j <= 0)
                break;
            str[i] = j % 10 + '0';
            j = j / 10;
        }
        if (j) {
            strcpy(str + 1, "2BIG");
        }
    }
    return str;
}