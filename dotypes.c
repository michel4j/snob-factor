/*	A routine to initialize the VarType list "types"
    It must be added to for every new type added, by adding a call
on XXXdefine() where XXX is the prefix of the fun XXXdefine in the file
of functions for the new type.
    The routine also sets some global geometric constants.
    */

#include "glob.h"

void reals_define(int typindx);
void expmults_define(int typindx);
void expbinary_define(int typindx);
void vonm_define(int typindx);

void do_types() {
    int i;

    /*	Set the number of attribute types  */
    Ntypes = 4;

    /*	Set constants  */
    pi = 4.0 * atan(1.0);
    hlg2pi = 0.5 * log(2.0 * pi);
    twoonpi = 2.0 / pi;
    pion2 = 0.5 * pi;
    bit = log(2.0);
    bit2 = 2.0 * bit;
    hlg2 = 0.5 * log(2.0);
    lattice = -0.5 * log(12.0);
    for (i = 0; i < MAX_ZERO; i++)
        zerov[i] = 0.0;

    /*	Make the 'types' vector  */
    Types = (VarType *)malloc(Ntypes * sizeof(VarType));

    reals_define(0);
    expmults_define(1);
    expbinary_define(2);
    vonm_define(3);

    return;
}
