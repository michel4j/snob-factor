
#include "glob.h"

/*	------------------------  Some routines to read numbers  -------- */

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

Buffer CFileBuffer, CommsBuffer; /* Buffers for command input */
int Terminator;

/*	--------------------------- bufopen() --------------------------  */
/*	Given a Buffer with a name in it, sets up and initializes the named
 * file*/
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
  /*	Leaves the buffer at "end of line 0"   */
}

/*	------------------------  newline () ------------------   */
/*	To skip to next line  */
int new_line(SnobContext *ctx) {
  Buffer *buf;
  /*	Discard anything in inl and read in a new line, to '\n'  */
  int i, j;

  buf = ctx->state.buffer;
  if (buf->cfile == 0) { /* Input via comms */
    j = 0;               /* To count tries at opening comms */
  retry:
    if (UseStdIn)
      goto usestd;
    i = hark(ctx, buf->inl);
    /*	i = 0 means comms OK but no input yet. 1 means input present
        -1 means no comms file or bad format  */
    if (i == 1) {
      Heard = buf->nch = 0;
      return (0);
    }
    if (i < 0) {
      j++;
      if (j > 3) {
        UseStdIn = 1;
        printf(
            "There being no comms file, input will be taken from StdInput\n");
        goto retry;
      }
    }
    sleep(1);
    goto retry;

  usestd: /* Take input from standard input */
    i = 0;
    buf->nch = -1;
    j = getchar();
    if (j == '\n')
      goto usestd;
    if (j == EOF) {
      printf("EOF in StdInput\n");
      exit(0);
    }
    buf->inl[0] = j;
  nextchar:
    i++;
    if (i >= INPUT_BUFFER_SIZE) {
      printf("Line too long\n");
      return (-1);
    }
    j = getchar();
    buf->inl[i] = j;
    if (j != '\n')
      goto nextchar;
    buf->inl[i + 1] = 0;
    Heard = buf->nch = 0;
    return (0);
  }

  /*	Input from control file  */
  i = 0;
  buf->nch = -1;
firstch:
  j = fgetc(buf->cfile);
  if (j == '\n') {
    buf->line++;
    goto firstch;
  }
  if (j == EOF) {
    printf("Unexpected end of file after line %d\n", buf->line);
    return (-2);
  }
  if ((j == ' ') || (j == '\t'))
    goto firstch;
  buf->inl[i] = j;
  buf->line++;

nextch:
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
    /*	Copy out line of control file  */
    if (buf == &CFileBuffer)
      printf("=== %s\n", buf->inl);
    return (0);
  }
  buf->inl[i] = j;
  goto nextch;
}

/*	-------------------------  reperror (ctx) ---------------------    */
void reperror(SnobContext *ctx) {
  int i, j;
  char k;

  ctx->state.buffer->nch--;
  printf("Format error line %6d  character %3d\n", ctx->state.buffer->line,
         ctx->state.buffer->nch + 1);
  /*	Print some context of the error from ctx.buffer->inl
   *	Print up to 70 chars max   */
  i = 0;
  if (ctx->state.buffer->nch > 60)
    i = ctx->state.buffer->nch - 60;
  for (j = 0; j < 70; j++) {
    k = ctx->state.buffer->inl[i + j];
    if (k == '\n')
      goto done;
    printf("%c", k);
  }
done:
  printf("\n");
  for (j = 0; j < (ctx->state.buffer->nch - i); j++)
    printf("%c", '-');
  printf("%s", "^\n");
  return;
}

/*	-------------------------- readint () ----------------------  */
/*	Readint, readdf, readalf will automatically advance to the next
line if cnl not zero, but will return 2 if cnl = 0 and EOL is reached before
the read is satisfied  */
/*	To read an integer into x   */
int read_int(SnobContext *ctx, int *x, int cnl) {
  Buffer *buf;
  int sign, i, v;

  buf = ctx->state.buffer;
  v = sign = Terminator = 0;

skip:
  i = buf->inl[buf->nch++];
  switch (i) {
  case '\n':
    if (!cnl) {
      buf->nch--;
      return (2);
    }
    if (new_line(ctx))
      return (-1);
  case ' ':
  case '\t':
    goto skip;
  case '=':
    goto miss;
  case '-':
    sign = -1;
  case '+':
    goto begun;
  }
  if ((i >= '0') && (i <= '9')) {
    v = i - '0';
    goto begun;
  }
  reperror(ctx);
  return (-1);

begun:
  i = buf->inl[buf->nch++];
  if ((i >= '0') && (i <= '9')) {
    v = 10 * v + i - '0';
    goto begun;
  }
  if (sign)
    v = -v;
  *x = v;
  Terminator = i;
  buf->nch--;
  return (0);

miss: /* An '=' signifies missing value  */
  /*	Consume all = chars  */
  i = buf->inl[buf->nch++];
  if (i == '=')
    goto miss;
  buf->nch--;
  *x = 0;
  return (1);
}

/*	--------------------  readdf ()  --------------------------- */
/*	To read a float into (double) x   */
int read_double(SnobContext *ctx, double *x, int cnl) {
  Buffer *buf;
  int sign, i;
  double v, pow;
  buf = ctx->state.buffer;
  sign = 0;
  v = 0.0;
  pow = 1.0;

skip:
  i = buf->inl[buf->nch++];
  switch (i) {
  case '\n':
    if (!cnl) {
      buf->nch--;
      return (2);
    }
    if (new_line(ctx))
      return (-1);
  case ' ':
  case '\t':
    goto skip;
  case '=':
    goto miss;
  case '-':
    sign = -1;
  case '+':
    goto begun;
  }
  if ((i >= '0') && (i <= '9')) {
    v = i - '0';
    goto begun;
  }
  if (i == '.')
    goto part;
  reperror(ctx);
  return (-1);

begun:
  i = buf->inl[buf->nch++];
  if ((i >= '0') && (i <= '9')) {
    v = 10 * v + i - '0';
    goto begun;
  }
  if (i == '.')
    goto part;
  goto endnum;

part:
  i = buf->inl[buf->nch++];
  if ((i >= '0') && (i <= '9')) {
    v = 10.0 * v + i - '0';
    pow *= 0.1;
    goto part;
  }

endnum:
  if (sign)
    v = -v;
  *x = v * pow;
  buf->nch--;
  return (0);

miss: /* An '=' signifies missing value  */
  /*	Consume all = chars  */
  i = buf->inl[buf->nch++];
  if (i == '=')
    goto miss;
  *x = 0;
  buf->nch--;
  return (1);
}

/*	--------------------  readalf () ------------------------   */
/*	To read a string of characters  */
int read_str(SnobContext *ctx, char *str, int cnl) {
  Buffer *buf;
  int i, n;

  buf = ctx->state.buffer;
  n = 0;
skip:
  i = buf->inl[buf->nch++];
  switch (i) {
  case '\n':
    if (!cnl) {
      buf->nch--;
      return (2);
    }
    if (new_line(ctx))
      return (-1);
  case ' ':
  case '\t':
    goto skip;
  case '=':
    goto miss;
  }

begun:
  str[n] = i;
  n++;
  if (n >= 80)
    goto err; /* Too long */
  i = buf->inl[buf->nch++];
  switch (i) {
  case ' ':
  case '\t':
  case '\n':
    goto done;
  }
  goto begun;

done:
  str[n] = 0;
  buf->nch--;
  return (0);

miss: /* An '=' signifies missing value  */
  /*	Consume all = chars  */
  i = buf->inl[buf->nch++];
  if (i == '=')
    goto miss;
  *str = 0;
  buf->nch--;
  return (1);

err:
  *str = 0;
  reperror(ctx);
  return (-1);
}

/*	---------------------- readch () -----------------------  */
/*	Returns next char, or -1 if error, or 2 if EOL and not cnl  */
int read_char(SnobContext *ctx, int cnl) {
  Buffer *buf;
  int i;

  buf = ctx->state.buffer;
skip:
  i = buf->inl[buf->nch++];
  if (i == '\n') {
    if (!cnl) {
      buf->nch--;
      return (2);
    }
    if (new_line(ctx))
      return (-1);
    goto skip;
  }
  return (i);
}

/*	--------------------------  swallow (ctx)	--------------------- */
/*	To swallow an erroneus field, stopping at blank, newline or tab */
void swallow(SnobContext *ctx) {
  Buffer *buf;
  int i;

  buf = ctx->state.buffer;
gulp:
  i = buf->inl[buf->nch];
  switch (i) {
  case ' ':
  case '\t':
  case '\n':
    return;
  }
  buf->nch++;
  goto gulp;
}

/*	--------------------------  bufclose () ------------------  */
/*	To close the open input file   */
void close_buffer(SnobContext *ctx) {
  if (!ctx->state.buffer)
    return;
  if (!(ctx->state.buffer->cfile))
    return;
  fclose(ctx->state.buffer->cfile);
  ctx->state.buffer = 0;
  return;
}

/*	------------------------  revert (ctx) ---------------------  */
/*	To revert to comms-file input  */
/*	If flag, revert due to an interrupt via hark, so use existing
CommsBuffer line. Otherwise, get a new line  */
void revert(SnobContext *ctx, int flag) {
  if (CurSource->cfile)
    printf("Command file %s\n terminated at line %d\n", CurSource->cname,
           CurSource->line);
  close_buffer(ctx);
  CurSource = &CommsBuffer;
  ctx->state.buffer = CurSource;
  if (flag)
    CurSource->nch = 0;
  else
    new_line(ctx);
  return;
}

/*	----------------------------  rep(ctx) --------------------  */
/*	rep(ctx, ch) prints char ch and flushes stdout. If end of line, does
a new line.  flp(ctx) does a new line.  */

void rep(SnobContext *ctx, int ch) {
  if (Debug < 1) {
    putchar(ch);
    NumRepChars++;
    if (NumRepChars == 80) {
      putchar('\n');
      NumRepChars = 0;
    }
    fflush(stdout);
  }
}

void flp(SnobContext *ctx) {
  if (NumRepChars && (Debug < 1)) {
    putchar('\n');
    NumRepChars = 0;
  }
  return;
}
