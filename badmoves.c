#define BADMOVES 1
#include "snob.h"

static char *movestr[] = {" ", "Insert", "Delete", "Move"};

/*    ---------------  clear_bad_move  --------------------------------  */
/*    To clear the badmoves table  */
void clear_bad_move() {
    for (int i = 0; i < BadSize; i++)
        BadKey[i] = 0;
    return;
}

/*    -----------------  check_bad_move  -----------------------------  */
/*    To test a move for known recent fail  */
/*    code 1 means insert ,code 2 means delete  */
/*    code 3 means move */
int check_bad_move(int code, int w1, int w2) {
    int hi, key, bad, s1, s2;

    set_population();
    s1 = w1;
    s2 = w2;
    if ((code == 1) || (code == 3)) {
        if (w1 > w2) {
            s1 = w2;
            s2 = w1;
        }
    } else
        s1 = 0;
    key = hi = (((code << 13) + s1) << 13) + s2;
    if (hi < 0)
        hi = -1 - hi;
    hi = hi % BadSize;
    bad = 0;
    if (BadKey[hi] == key) {
        flp();
        bad = 1;
        printf("Badmove rejects %s", movestr[code]);
        if (w1)
            printf("%6d", w1 >> 2);
        if (w2)
            printf("%6d", w2 >> 2);
        printf("\n");
    }
    return (bad);
}

/*    ------------------  set_bad_move  -----------------------------   */
/*    To log a bad move  */
void set_bad_move(int code, int s1, int s2) {
    int hi, key;

    if ((code == 1) || (code == 3)) {
        if (s1 > s2) {
            hi = s2;
            s2 = s1;
            s1 = hi;
        }
    } else
        s1 = 0;
    key = hi = (((code << 13) + s1) << 13) + s2;
    set_population();
    if (hi < 0)
        hi = -1 - hi;
    hi = hi % BadSize;
    BadKey[hi] = key;
    return;
}
