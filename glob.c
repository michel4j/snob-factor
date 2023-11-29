
#define GLOBALS 1
#include "glob.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Initialize some variables */
int UseLib = 0;

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

/*    --------------------  show_pop_names  ---------------------  */
void show_pop_names() {
    int i;
    printf("The defined models are:\n");
    for (i = 0; i < MAX_POPULATIONS; i++) {
        if (Populations[i]) {
            printf("%2d %s", i + 1, Populations[i]->name);
            if (Populations[i]->sample_size)
                printf(" Sample %s", Populations[i]->sample_name);
            else
                printf(" (unattached)");
            printf("\n");
        }
    }
    printf("Also, the pseudonym \"BST_\" can be used for the best\n");
    printf("model for the current sample\n");
    return;
}

/*      ----------------------  show_smpl_names  -----------------  */
void show_smpl_names() {
    int k;
    printf("Loaded samples:\n");
    for (k = 0; k < MAX_SAMPLES; k++) {
        if (Samples[k]) {
            printf("%2d:  %s\n", k + 1, Samples[k]->name);
        }
    }
    return;
}
