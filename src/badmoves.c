
#include "glob.h"

static char *movestr[] = {" ", "Insert", "Delete", "Move"};

/**
 * @brief Clear the badmoves table
 * @param ctx Pointer to the Snob context.
 */
void clr_bad_move(SnobContext *ctx) {
    for (int i = 0; i < BadSize; i++)
        ctx->bad_key[i] = 0;
    return;
}

/**
 * @brief check if a specific structural modification to the model's classification tree
 * has recently been attempted and rejected using a fast, direct-mapped hash table lookup
 * with an eviction-based caching strategy with no collision chaining
 * @param ctx Pointer to the Snob context.
 * @param code Operation code. 1 = insert, 2 = delete, 3 = move
 * @param cls_a First class
 * @param cls_b Second class
 */
int chk_bad_move(SnobContext *ctx, int code, int cls_a, int cls_b) {
    int hi, key, bad, s1, s2;

    s1 = cls_a;
    s2 = cls_b;
    if ((code == 1) || (code == 3)) {
        if (cls_a > cls_b) {
            s1 = cls_b;
            s2 = cls_a;
        }
    } else
        s1 = 0;
    key = hi = (((code << 13) + s1) << 13) + s2;
    if (hi < 0)
        hi = -1 - hi;
    hi = hi % BadSize;
    bad = 0;
    if (ctx->bad_key[hi] == key) {
        bad = 1;
        log_msg(ctx, 0, "Badmove rejects %s %6d %6d", movestr[code], cls_a >> 2, cls_b >> 2);
    }
    return (bad);
}

/**
 * @brief Log a bad move
 * @param ctx Pointer to the Snob context.
 * @param code Operation code. 1 = insert, 2 = delete, 3 = move
 * @param cls_a First class
 * @param cls_b Second class
 */
void set_bad_move(SnobContext *ctx, int code, int cls_a, int cls_b) {
    int hi, key;

    if ((code == 1) || (code == 3)) {
        if (cls_a > cls_b) {
            hi = cls_b;
            cls_b = cls_a;
            cls_a = hi;
        }
    } else
        cls_a = 0;
    key = hi = (((code << 13) + cls_a) << 13) + cls_b;

    if (hi < 0)
        hi = -1 - hi;
    hi = hi % BadSize;
    ctx->bad_key[hi] = key;
    return;
}
