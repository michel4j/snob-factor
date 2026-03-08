
#include "glob.h"

// Some routines to read numbers

/*	  The routines will accept "missing" values
shown by the character  '=' or by a string of consecutive '='s, e.g.
"==========" is read as a single missing value.    */

/*	Input is line-sensitive.  The routine "newline" advances to the
next line.
The "cfile" field of the buf is a file
pointer of the file to be read, the "cname" field is the file's character
name.
    */
/*	Routines to read an item dont consume the character which terminates
the item   */

Buffer CFileBuffer, CommsBuffer; // Buffers for command input
int Terminator;

/** @brief Given a Buffer with a name in it, sets up and initializes the named
 * file
 *  @param ctx Pointer to the Snob context.
 *  @return 0 if successful, 1 if file not found, -1 if error
 */
int open_buffser(SnobContext *ctx) {
    Buffer *buf;

    buf = ctx->state.buffer;
    buf->cfile = fopen(buf->cname, "r");
    if (!buf->cfile) {
        buf->nch = -1;
        return (1);
    }
    buf->nch = 0;
    buf->line = 0;
    buf->inl[0] = '\n';
    return (0);
    //	Leaves the buffer at "end of line 0"
}

/** @brief Skip to next line
 *  @param ctx Pointer to the Snob context.
 *  @return 0 if successful, -1 if error
 */
int new_line(SnobContext *ctx) {
    Buffer *buf;
    //	Discard anything in inl and read in a new line, to '\n'
    int i, j;

    buf = ctx->state.buffer;
    if (buf->cfile == 0) { // Input via comms
        j = 0;             // To count tries at opening comms
        while (!ctx->use_stdin) {
            i = hark(ctx, buf->inl);
            /*	i = 0 means comms OK but no input yet. 1 means input present
                -1 means no comms file or bad format  */
            if (i == 1) {
                ctx->heard = buf->nch = 0;
                return (0);
            }
            if (i < 0) {
                j++;
                if (j > 3) {
                    ctx->use_stdin = 1;
                    printf("There being no comms file, input will be taken from StdInput\n");
                    break;
                }
            } else {
                sleep(1);
            }
        }

        // Take input from standard input
        i = 0;
        buf->nch = -1;
        do {
            j = getchar();
        } while (j == '\n');

        if (j == EOF) {
            printf("EOF in StdInput\n");
            exit(0);
        }
        buf->inl[0] = j;
        while (1) {
            i++;
            if (i >= INPUT_BUFFER_SIZE) {
                printf("Line too long\n");
                return (-1);
            }
            j = getchar();
            buf->inl[i] = j;
            if (j == '\n')
                break;
        }
        buf->inl[i + 1] = 0;
        ctx->heard = buf->nch = 0;
        return (0);
    }

    //	Input from control file
    i = 0;
    buf->nch = -1;
    while (1) {
        j = fgetc(buf->cfile);
        if (j == '\n') {
            buf->line++;
            continue;
        }
        if ((j == ' ') || (j == '\t')) {
            continue;
        }
        break;
    }

    if (j == EOF) {
        printf("Unexpected end of file after line %d\n", buf->line);
        return (-2);
    }

    buf->inl[i] = j;
    buf->line++;

    while (1) {
        i++;
        if (i >= INPUT_BUFFER_SIZE) {
            printf("Line %5d too long\n", buf->line);
            return (-1);
        }
        j = fgetc(buf->cfile);
        if ((j == EOF) || (j == '\n')) {
            buf->inl[i] = '\n';
            buf->inl[i + 1] = 0;
            buf->nch = 0;
            //	Copy out line of control file
            if (buf == &CFileBuffer)
                printf("=== %s\n", buf->inl);
            return (0);
        }
        buf->inl[i] = j;
    }
}

/** @brief Report a format error
 *  @param ctx Pointer to the Snob context.
 */
void reperror(SnobContext *ctx) {
    int i, j;
    char k;

    ctx->state.buffer->nch--;
    printf("Format error line %6d  character %3d\n", ctx->state.buffer->line, ctx->state.buffer->nch + 1);
    /*	Print some context of the error from ctx.buffer->inl
     *	Print up to 70 chars max   */
    i = 0;
    if (ctx->state.buffer->nch > 60)
        i = ctx->state.buffer->nch - 60;
    for (j = 0; j < 70; j++) {
        k = ctx->state.buffer->inl[i + j];
        if (k == '\n')
            break;
        printf("%c", k);
    }
    printf("\n");
    for (j = 0; j < (ctx->state.buffer->nch - i); j++)
        printf("%c", '-');
    printf("%s", "^\n");
    return;
}

/** @brief Read an integer from the input buffer
 *  @param ctx Pointer to the Snob context.
 *  @param x Pointer to the integer to read
 *  @param cnl Flag to indicate whether to advance to the next line
 *  @return 0 if successful, 2 if end of line reached before read, -1 if error
 */
int read_int(SnobContext *ctx, int *x, int cnl) {
    Buffer *buf;
    int sign, i, v;

    buf = ctx->state.buffer;
    v = sign = Terminator = 0;

    while (1) {
        i = buf->inl[buf->nch++];
        if (i == '\n') {
            if (!cnl) {
                buf->nch--;
                return (2);
            }
            if (new_line(ctx))
                return (-1);
            continue;
        }
        if (i == ' ' || i == '\t')
            continue;
        break;
    }

    if (i == '=') {
        while (1) {
            i = buf->inl[buf->nch++];
            if (i != '=')
                break;
        }
        buf->nch--;
        *x = 0;
        return (1);
    }

    if (i == '-') {
        sign = -1;
        i = buf->inl[buf->nch++];
    } else if (i == '+') {
        i = buf->inl[buf->nch++];
    }

    if ((i >= '0') && (i <= '9')) {
        v = i - '0';
        while (1) {
            i = buf->inl[buf->nch++];
            if ((i >= '0') && (i <= '9')) {
                v = 10 * v + i - '0';
            } else {
                break;
            }
        }
    } else {
        reperror(ctx);
        return (-1);
    }

    if (sign)
        v = -v;
    *x = v;
    Terminator = i;
    buf->nch--;
    return (0);
}

/** @brief Read a double from the input buffer
 *  @param ctx Pointer to the Snob context.
 *  @param x Pointer to the double to read
 *  @param cnl Flag to indicate whether to advance to the next line
 *  @return 0 if successful, 2 if end of line reached before read, -1 if error
 */
int read_double(SnobContext *ctx, double *x, int cnl) {
    Buffer *buf;
    int sign, i;
    double v, pow;
    buf = ctx->state.buffer;
    sign = 0;
    v = 0.0;
    pow = 1.0;

    while (1) {
        i = buf->inl[buf->nch++];
        if (i == '\n') {
            if (!cnl) {
                buf->nch--;
                return (2);
            }
            if (new_line(ctx))
                return (-1);
            continue;
        }
        if (i == ' ' || i == '\t')
            continue;
        break;
    }

    if (i == '=') {
        while (1) {
            i = buf->inl[buf->nch++];
            if (i != '=')
                break;
        }
        *x = 0;
        buf->nch--;
        return (1);
    }

    if (i == '-') {
        sign = -1;
        i = buf->inl[buf->nch++];
    } else if (i == '+') {
        i = buf->inl[buf->nch++];
    }

    if ((i >= '0') && (i <= '9')) {
        v = i - '0';
        while (1) {
            i = buf->inl[buf->nch++];
            if ((i >= '0') && (i <= '9')) {
                v = 10.0 * v + i - '0';
            } else {
                break;
            }
        }
    } else if (i != '.') {
        reperror(ctx);
        return (-1);
    }

    if (i == '.') {
        while (1) {
            i = buf->inl[buf->nch++];
            if ((i >= '0') && (i <= '9')) {
                v = 10.0 * v + i - '0';
                pow *= 0.1;
            } else {
                break;
            }
        }
    }

    if (sign)
        v = -v;
    *x = v * pow;
    buf->nch--;
    return (0);
}

/** @brief Read a string from the input buffer
 *  @param ctx Pointer to the Snob context.
 *  @param str Pointer to the string to read
 *  @param cnl Flag to indicate whether to advance to the next line
 *  @return 0 if successful, 2 if end of line reached before read, -1 if error
 */
int read_str(SnobContext *ctx, char *str, int cnl) {
    Buffer *buf;
    int i, n;

    buf = ctx->state.buffer;
    n = 0;

    while (1) {
        i = buf->inl[buf->nch++];
        if (i == '\n') {
            if (!cnl) {
                buf->nch--;
                return (2);
            }
            if (new_line(ctx))
                return (-1);
            continue;
        }
        if (i == ' ' || i == '\t')
            continue;
        break;
    }

    if (i == '=') {
        while (1) {
            i = buf->inl[buf->nch++];
            if (i != '=')
                break;
        }
        *str = 0;
        buf->nch--;
        return (1);
    }

    while (1) {
        str[n] = i;
        n++;
        if (n >= 80) {
            *str = 0;
            reperror(ctx);
            return (-1); // Too long
        }
        i = buf->inl[buf->nch++];
        if (i == ' ' || i == '\t' || i == '\n') {
            break;
        }
    }

    str[n] = 0;
    buf->nch--;
    return (0);
}

/** @brief Read a character from the input buffer
 *  @param ctx Pointer to the Snob context.
 *  @param cnl Flag to indicate whether to advance to the next line
 *  @return The character read, -1 if error, 2 if end of line reached before read
 */
int read_char(SnobContext *ctx, int cnl) {
    Buffer *buf;
    int i;

    buf = ctx->state.buffer;
    while (1) {
        i = buf->inl[buf->nch++];
        if (i == '\n') {
            if (!cnl) {
                buf->nch--;
                return (2);
            }
            if (new_line(ctx))
                return (-1);
            continue;
        }
        return (i);
    }
}

/** @brief Swallow an erroneous field, stopping at blank, newline or tab
 *  @param ctx Pointer to the Snob context.
 */
void swallow(SnobContext *ctx) {
    Buffer *buf;
    int i;

    buf = ctx->state.buffer;
    while (1) {
        i = buf->inl[buf->nch];
        if (i == ' ' || i == '\t' || i == '\n') {
            return;
        }
        buf->nch++;
    }
}

/** @brief Close the open input file
 *  @param ctx Pointer to the Snob context.
 */
void close_buffer(SnobContext *ctx) {
    if (!ctx->state.buffer)
        return;
    if (!(ctx->state.buffer->cfile))
        return;
    fclose(ctx->state.buffer->cfile);
    ctx->state.buffer = 0;
    return;
}

/** @brief Revert to comms-file input
 *  @param ctx Pointer to the Snob context.
 *  @param flag Flag to indicate whether to use existing CommsBuffer line
 */
void revert(SnobContext *ctx, int flag) {
    if (ctx->current_source->cfile)
        printf("Command file %s\n terminated at line %d\n", ctx->current_source->cname, ctx->current_source->line);
    close_buffer(ctx);
    ctx->current_source = &CommsBuffer;
    ctx->state.buffer = ctx->current_source;
    if (flag)
        ctx->current_source->nch = 0;
    else
        new_line(ctx);
    return;
}

/** @brief Print a character to the output buffer
 *  @param ctx Pointer to the Snob context.
 *  @param ch The character to print
 */
void rep(SnobContext *ctx, int ch) {
    if (ctx->debug < 1) {
        putchar(ch);
        ctx->num_rep_chars++;
        if (ctx->num_rep_chars == 80) {
            putchar('\n');
            ctx->num_rep_chars = 0;
        }
        fflush(stdout);
    }
}

/** @brief Flush the output buffer
 *  @param ctx Pointer to the Snob context.
 */
void flp(SnobContext *ctx) {
    if (ctx->num_rep_chars && (ctx->debug < 1)) {
        putchar('\n');
        ctx->num_rep_chars = 0;
    }
    return;
}
