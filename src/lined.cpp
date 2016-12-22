/* lined.c -- line editing for embedded systems.
 *
 * lined is a fork of the linenoise line editing library originally written
 * by Salvatore Sanfilippo and Pieter Noordhuis. It is stripped down and
 * riddened from all OS specific dependencies to make it usable for small-
 * and/or embedded systems.
 *
 * Additionally the following changes have been made:
 *
 * converted camelcase to underscore syntax
 * added spaces after ',' in parameter lists
 * do not block, while editing the command line
 * allow multiple instances at the same time
 * removed multiline editing mode
 * fixed some inconsistancies in API usage
 * turned switch ... case into if ... else to save ram
 * added macros to put constant strings into flash memory
 * allow local echo to be switched on/off (e.g. for password)
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2016-2017, Clemens Kirchgatterer <clemens at 1541 dot org>
 *
 */

/* linenois.c -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 *
 * You can find the latest source code at:
 *
 *   http://github.com/antirez/linenoise
 *
 * Does a number of crazy assumptions that happen to be true in 99.9999% of
 * the 2010 UNIX computers around.
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2010-2016, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010-2013, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ------------------------------------------------------------------------
 *
 * References:
 * - http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
 * - http://www.3waylabs.com/nw/WWW/products/wizcon/vt220.html
 *
 * Todo list:
 * - Filter bogus Ctrl+<char> combinations.
 * - Win32 support
 *
 * Bloat:
 * - _history_ search like Ctrl+r in readline?
 *
 * List of escape sequences used by this program, we do everything just
 * with three sequences. In order to be so cheap we may have some
 * flickering effect with some slow terminal, but the lesser sequences
 * the more compatible.
 *
 * EL (Erase Line)
 *  Sequence: ESC [ n K
 *  Effect: if n is 0 or missing, clear from cursor to end of line
 *  Effect: if n is 1, clear from beginning of line to cursor
 *  Effect: if n is 2, clear entire line
 *
 * CUF (CUrsor Forward)
 *  Sequence: ESC [ n C
 *  Effect: moves cursor forward n chars
 *
 * CUB (CUrsor Backward)
 *  Sequence: ESC [ n D
 *  Effect: moves cursor backward n chars
 *
 * The following is used to get the terminal width if getting
 * the width with the TIOCGWINSZ ioctl fails
 *
 * DSR (Device Status Report)
 *  Sequence: ESC [ 6 n
 *  Effect: reports the current cusor position as ESC [ n ; m R
 *      where n is the row and m is the column
 *
 * When multi line mode is enabled, we also use an additional escape
 * sequence. However multi line editing is disabled by default.
 *
 * CUU (Cursor Up)
 *  Sequence: ESC [ n A
 *  Effect: moves cursor up of n chars.
 *
 * CUD (Cursor Down)
 *  Sequence: ESC [ n B
 *  Effect: moves cursor down of n chars.
 *
 * When linenoiseClearScreen() is called, two additional escape sequences
 * are used in order to clear the screen and position the cursor at home
 * position.
 *
 * CUP (Cursor position)
 *  Sequence: ESC [ H
 *  Effect: moves the cursor to upper left corner
 *
 * ED (Erase display)
 *  Sequence: ESC [ 2 J
 *  Effect: clear the whole screen
 *
 */

#include <Arduino.h>

#include "lined.h"

#define HISTORY_NEXT 0
#define HISTORY_PREV 1

#define DEFAULT_LINE_LENGTH 80

typedef struct completion_t {
  int len;
  int index;
  char **cvec;
} completion_t;

/* The lined_t structure represents the state during line editing.
 * We pass this state to functions implementing specific editing
 * functionalities. */
typedef struct lined_t {
  Terminal *term;         /* Reference to Terminal. */
  char *buf;              /* Edited line buffer. */
  int buflen;             /* Edited line buffer size. */
  int plen;               /* Prompt length. */
  int pos;                /* Current cursor position. */
  int len;                /* Current edited line length. */
  int cols;               /* Number of columns in terminal. */
  int rows;               /* Number of rows in terminal. */
  int echo;               /* Update current line while editing. */
  completion_t *lc;       /* Current TAB completion vector. */
  int history_index;      /* The history index we are currently editing. */
  const char *prompt;     /* Prompt to display. */
} lined_t;

static lined_completion_cb *completion_cb = NULL;
static lined_hint_cb *hint_cb = NULL;

static int  history_max_len = 10; // default history length
static int  history_len = 0;
static char **history = NULL;

static void refresh_line(lined_t *l);

/* ======================== Small helper functions ========================== */

/* We define a very simple "append buffer" structure, that is a string we
 * can append to. This is useful in order to write all the escape sequences
 * in a buffer and flush them to the standard output in a single call, to
 * avoid flickering effects. The buffer is NOT checked for overruns! */
struct abuf {
  char *b;
  int len;
};

static void ab_init(struct abuf *ab, char *buf) {
  ab->b = buf;
  ab->len = 0;
}

static void ab_append(struct abuf *ab, const char *s, int len) {
  memcpy(ab->b + ab->len, s, len);
  ab->len += len;
}

/* Beep, used for completion when there is nothing to complete or when all
 * the choices were already shown. */
static void make_beep(lined_t *l) {
  l->term->Print(F("\x7"));
}

/* ============================== Completion ================================ */

/* Free a list of completion option populated by linedAddCompletion(). */
static void reset_completion(lined_t *l) {
  if (l->lc) {
    for (int i=0; i<l->lc->len; i++) free(l->lc->cvec[i]);

    free(l->lc->cvec);
  }

  free(l->lc);
  l->lc = NULL;
}

/* Show completion or original buffer */
static void show_completion(lined_t *l) {
  int i = l->lc->index;

  if (i < l->lc->len) {
    lined_t saved = *l;

    l->len = l->pos = strlen(l->lc->cvec[i]);
    l->buf = l->lc->cvec[i];
    refresh_line(l);

    l->len = saved.len;
    l->pos = saved.pos;
    l->buf = saved.buf;
  } else {
    refresh_line(l);
  }
}

/* This is an helper function for edit() and is called when the
 * user hits <tab> in order to complete the string currently in the
 * input buffer.
 *
 * The state of the editing is encapsulated into the pointed lined_t
 * structure as described in the structure definition. */
static void complete_line(lined_t *l, char &c) {
  if (!l->lc && (c == TERM_KEY_TAB)) {
    c = TERM_KEY_NONE;

    // fill completion vector
    completion_cb(l, l->buf);

    if (l->lc) {
      show_completion(l);
    } else {
      make_beep(l);
    }

    return;
  }

  if (!l->lc) return;
 
  int i = l->lc->index;

  if (c == TERM_KEY_TAB) {
    c = TERM_KEY_NONE;

    // increment completion index
    i = (i+1) % (l->lc->len+1);
    if (i == l->lc->len) make_beep(l);

    l->lc->index = i;

    show_completion(l);
  } else if (c == TERM_KEY_ESC) {
    c = TERM_KEY_NONE;

    /* Re-show original buffer */
    if (i < l->lc->len) refresh_line(l);

    reset_completion(l);
  } else {
    /* Update buffer and return */
    if (i < l->lc->len) {
      int n = snprintf_P(l->buf, l->buflen, PSTR("%s"), l->lc->cvec[i]);
      l->len = l->pos = n;
    }

    reset_completion(l);
  }
}

/* =========================== Line editing ================================= */

/* Helper of refresh_line() to show hints
 * to the right of the prompt. */
static void show_hint(struct abuf *ab, lined_t *l) {
  if (hint_cb && (l->plen + l->len < l->cols)) {
    int color = -1, bold = 0;
    char *hint = hint_cb(l->buf, &color, &bold);

    if (hint) {
      int hintlen = strlen(hint);
      int hintmaxlen = l->cols - (l->plen + l->len);
      char seq[64];
      int ln = 0;

      if (hintlen > hintmaxlen) hintlen = hintmaxlen;
      if (bold == 1 && color == -1) color = 37;
      if (color != -1 || bold != 0) {
        ln = snprintf_P(seq, 64, PSTR("\033[%i;%i;49m"), bold, color);
      }

      ab_append(ab, seq, ln);
      ab_append(ab, hint, hintlen);

      if (color != -1 || bold != 0) {
        ln = snprintf_P(seq, 64, PSTR("\033[0m"));
        ab_append(ab, seq, ln);
      }
    }
  }
}

/* Rewrite the currently edited line accordingly to the buffer content,
 * cursor position, and number of columns of the terminal. */
static void refresh_line(lined_t *l) {
  int ln, len = l->len;
  char *buf = l->buf;
  char scratch[128];
  int pos = l->pos;
  struct abuf ab;
  char seq[64];

  if (l->echo == 0) return;

  while ((l->plen + pos) >= l->cols) {
    buf++;
    len--;
    pos--;
  }
  while (l->plen + len > l->cols) {
    len--;
  }

  ab_init(&ab, scratch);

  /* Cursor to left edge */
  ab_append(&ab, "\r", 1);

  /* Write the prompt and the current buffer content */
  ab_append(&ab, l->prompt, l->plen);
  ab_append(&ab, buf, len);

  /* Show hint if any. */
  show_hint(&ab, l);

  /* Erase to right */
  ln = snprintf_P(seq, 64, PSTR("\x1b[0K"));
  ab_append(&ab, seq, ln);

  /* Move cursor to original position. */
  ln = snprintf_P(seq, 64, PSTR("\r\x1b[%iC"), (int)(pos + l->plen));
  ab_append(&ab, seq, ln);

  if (l->term->tty.write(ab.b, ab.len) == -1) {
    /* Can't recover from write error. */
  }
}

/* Insert the character 'c' at cursor current position. */
static void edit_insert(lined_t *l, char c) {
  if ((c != TERM_KEY_ESC) && (l->len < l->buflen)) {
    if (l->len != l->pos) {
      memmove(l->buf + l->pos + 1, l->buf + l->pos, l->len - l->pos);
    }

    l->buf[l->pos] = c;
    l->pos++;
    l->len++;
    l->buf[l->len] = '\0';

    refresh_line(l);
  }
}

/* Move cursor to the left. */
static void edit_move_left(lined_t *l) {
  if (l->pos > 0) {
    l->pos--;
  }

  refresh_line(l);
}

/* Move cursor to the right. */
static void edit_move_right(lined_t *l) {
  if (l->pos != l->len) {
    l->pos++;
  }

  refresh_line(l);
}

/* Move cursor to the start of the line. */
static void edit_move_home(lined_t *l) {
  if (l->pos != 0) {
    l->pos = 0;
  }

  refresh_line(l);
}

/* Move cursor to the end of the line. */
static void edit_move_end(lined_t *l) {
  if (l->pos != l->len) {
    l->pos = l->len;
  }

  refresh_line(l);
}

/* Substitute the currently edited line with the next or previous history
 * entry as specified by 'dir'. */
static void edit_history_next(lined_t *l, int dir) {
  if (history_len > 1) {
    /* Update the current history entry before to
     * overwrite it with the next one. */
    free(history[history_len - 1 - l->history_index]);
    history[history_len - 1 - l->history_index] = strdup(l->buf);

    /* Show the new entry */
    l->history_index += (dir == HISTORY_PREV) ? 1 : -1;

    if (l->history_index < 0) {
      l->history_index = 0;
      return;
    } else if (l->history_index >= history_len) {
      l->history_index = history_len-1;
      return;
    }

    strncpy(l->buf, history[history_len - 1 - l->history_index], l->buflen);
    l->buf[l->buflen - 1] = '\0';
    l->len = l->pos = strlen(l->buf);

    refresh_line(l);
  }
}

/* Delete the character at the right of the cursor without altering the
 * cursor position. Basically this is what the DEL keyboard key does. */
static void edit_delete(lined_t *l) {
  if (l->len > 0 && l->pos < l->len) {
    memmove(l->buf + l->pos, l->buf + l->pos + 1, l->len - l->pos - 1);

    l->len--;
    l->buf[l->len] = '\0';
  }

  refresh_line(l);
}

/* Backspace implementation. */
static void edit_backspace(lined_t *l) {
  if (l->pos > 0 && l->len > 0) {
    memmove(l->buf + l->pos-1, l->buf + l->pos, l->len - l->pos);

    l->pos--;
    l->len--;
    l->buf[l->len] = '\0';
  }

  refresh_line(l);
}

/* Delete the previous word, maintaining the cursor at the start of the
 * current word. */
static void edit_delete_prev_word(lined_t *l) {
  int old_pos = l->pos;
  int diff;

  while (l->pos > 0 && l->buf[l->pos - 1] == ' ') l->pos--;
  while (l->pos > 0 && l->buf[l->pos - 1] != ' ') l->pos--;

  diff = old_pos - l->pos;
  memmove(l->buf + l->pos, l->buf + old_pos, l->len - old_pos + 1);
  l->len -= diff;

  refresh_line(l);
}

/* This function is the core of the line editing capability of lined.
 * It expects the GetKey() function to return every key pressed ASAP
 * or return TERM_KEY_NONE. GetKey() shall never block.
 *
 * The string (in buf) is constantly updated even when using TAB
 * completion.
 *
 * The function returns the code of the last pressed key. */
static int edit_line(lined_t *l) {
  char c, seq[3];
  int nread;

  c = l->term->GetKey();

  if (c == TERM_KEY_NONE) return (TERM_KEY_NONE);

  /* Only autocomplete when the callback is set. */
  if (completion_cb) complete_line(l, c);

  if (c == TERM_KEY_ENTER) {
    if (history_len > 0) {
      history_len--;
      free(history[history_len]);
    }

    if (hint_cb) {
      /* Force a refresh without hints to leave the previous
       * line as the user typed it after a newline. */
      lined_hint_cb *hc = hint_cb;
      hint_cb = NULL;
      refresh_line(l);
      hint_cb = hc;
    }

    return (TERM_KEY_ENTER);
  } else if (c == TERM_KEY_CTRL_C) {
    return (TERM_KEY_CTRL_C);
  } else if (c == TERM_KEY_BACKSPACE) {
    edit_backspace(l);
  } else if (c == TERM_KEY_CTRL_D) {
    /* remove char at right of cursor, or if the
     * line is empty, act as end-of-file. */
    if (l->len > 0) {
      edit_delete(l);
    } else {
      if (history_len > 0) {
        history_len--;
        free(history[history_len]);
      }

      return (TERM_KEY_CTRL_D);
    }
  } else if (c == TERM_KEY_CTRL_T) {
    /* swaps current character with previous. */
    if (l->pos > 0 && l->pos < l->len) {
      int aux = l->buf[l->pos-1];

      l->buf[l->pos-1] = l->buf[l->pos];
      l->buf[l->pos] = aux;
      if (l->pos != l->len-1) l->pos++;

      refresh_line(l);
    }
  } else if (c == TERM_KEY_CTRL_B) {
    edit_move_left(l);
  } else if (c == TERM_KEY_CTRL_F) {
    edit_move_right(l);
  } else if (c == TERM_KEY_CTRL_P) {
    edit_history_next(l, HISTORY_PREV);
  } else if (c == TERM_KEY_CTRL_N) {
    edit_history_next(l, HISTORY_NEXT);
  } else if (c == TERM_KEY_UP) {
    edit_history_next(l, HISTORY_PREV);
  } else if (c == TERM_KEY_DOWN) {
    edit_history_next(l, HISTORY_NEXT);
  } else if (c == TERM_KEY_LEFT) {
    edit_move_left(l);
  } else if (c == TERM_KEY_RIGHT) {
    edit_move_right(l);
  } else if (c == TERM_KEY_HOME) {
    edit_move_home(l);
  } else if (c == TERM_KEY_END) {
    edit_move_end(l);
  } else if (c == TERM_KEY_CTRL_U) {
    /* delete the whole line. */
    l->buf[0] = '\0';
    l->pos = l->len = 0;
    refresh_line(l);
  } else if (c == TERM_KEY_CTRL_K) {
    /* delete from current to end of line. */
    l->buf[l->pos] = '\0';
    l->len = l->pos;
    refresh_line(l);
  } else if (c == TERM_KEY_CTRL_A) {
    /* go to the start of the line */
    edit_move_home(l);
  } else if (c == TERM_KEY_CTRL_E) {
    /* go to the end of the line */
    edit_move_end(l);
  } else if (c == TERM_KEY_CTRL_L) {
    /* clear screen */
    l->term->ScreenClear();
    refresh_line(l);
  } else if (c == TERM_KEY_CTRL_W) {
    /* delete previous word */
    edit_delete_prev_word(l);
  } else if ((c == '\0') || (c == '\r')) {
    /* ignore '\0' and '\r' */
  } else {
    edit_insert(l, c);
  }

  return (TERM_KEY_NONE);
}

/* The high level function that creates a new lined context. */
lined_t *lined_begin(Terminal *term) {
  lined_t *l = (lined_t *)malloc(sizeof (lined_t));

  if (l) {
    /* Populate the lined state that we pass to functions implementing
     * specific editing functionalities. */
    l->term    = term;
    l->buf     = (char *)malloc(DEFAULT_LINE_LENGTH);
    l->buflen  = DEFAULT_LINE_LENGTH - 1; /* reserve space for '\0' */
    l->prompt  = NULL;
    l->plen    = 0;
    l->cols    = 0;
    l->rows    = 0;
    l->pos     = 0;
    l->len     = 0;
    l->echo    = 1;
    l->buf[0]  = '\0';
    l->lc      = NULL;
    l->history_index = 0;
  }

  return (l);
}

void lined_reset(lined_t *l) {
  if (l) {
    /* The latest history entry is always our current buffer, that
     * initially is just an empty string. */
    lined_history_add("");
    l->history_index = 0;

    reset_completion(l);

    l->pos    = 0;
    l->len    = 0;
    l->buf[0] = '\0';
    l->plen   = strlen(l->prompt);

    if ((l->cols == 0) || (l->rows == 0)) {
      l->term->GetSize(l->cols, l->rows);
    }

    refresh_line(l);
  }
}

void lined_echo(lined_t *l, int echo) {
  if (l) {
    l->echo = echo;
  }
}

void lined_prompt(lined_t *l, const char *prompt) {
  if (l) {
    l->prompt = prompt;
  }
}

void lined_resize(lined_t *l, int w, int h) {
  if (l) {
    l->cols = w;
    l->rows = h;
  }
}

char *lined_line(lined_t *l) {
  if (l) {
    return (l->buf);
  }

  return (NULL);
}

int lined_poll(lined_t *l) {
  if (l) {
    return (edit_line(l));
  }

  return (TERM_KEY_NONE);
}

void lined_end(lined_t *l) {
  if (l) {
    reset_completion(l);

    free(l->buf);
    free(l);
  }
}

/* Register a callback function to be called for tab-completion. */
void lined_set_completion_cb(lined_completion_cb *fn) {
  completion_cb = fn;
}

/* Register a function to be called to show hints to the user at the
 * right of the prompt. */
void lined_set_hint_cb(lined_hint_cb *fn) {
  hint_cb = fn;
}

/* This function is used by the callback function registered by the user
 * in order to add completion options given the input string when the
 * user hit <tab>. */
void lined_completion_add(lined_t *l, const char *str) {
  int len = strlen(str);
  char *copy, **cvec;

  if (!l->lc) {
    l->lc = (completion_t *)malloc(sizeof (completion_t));

    l->lc->len   = 0;
    l->lc->index = 0;
    l->lc->cvec  = NULL;
  }

  copy = (char *)malloc(len + 1);

  if (copy == NULL) return;

  memcpy(copy, str, len + 1);
  cvec = (char **)realloc(l->lc->cvec, sizeof (char *) * (l->lc->len + 1));

  if (cvec == NULL) {
    free(copy);
    return;
  }

  l->lc->cvec = cvec;
  l->lc->cvec[l->lc->len++] = copy;
}

/* ================================ History ================================= */

/* Free the global history vector. */
void lined_history_free(void) {
  if (history) {
    for (int j=0; j<history_len; j++) {
      free(history[j]);
      history[j] = NULL;
    }

    free(history);
    history = NULL;
  }
}

/* This is the API call to add a new entry in the lined history.
 * It uses a fixed array of char pointers that are shifted (memmoved)
 * when the history max length is reached in order to remove the older
 * entry and make room for the new one, so it is not exactly suitable
 * for huge histories, but will work well for a few hundred of entries. */
int lined_history_add(const char *line) {
  char *linecopy;

  if (history_max_len == 0) return (0);

  /* Initialization on first call. */
  if (history == NULL) {
    history = (char **)malloc(sizeof (char *) * history_max_len);
    if (history == NULL) return (0);
    memset(history, 0, (sizeof (char *) * history_max_len));
  }

  /* Don't add duplicates. */
  if (history_len && !strcmp(history[history_len-1], line)) return (0);

  /* Add an heap allocated copy of the line in the history.
   * If we reached the max length, remove the older line. */
  linecopy = strdup(line);
  if (!linecopy) return (0);
  if (history_len == history_max_len) {
    free(history[0]);
    memmove(history, history+1, sizeof (char *) * (history_max_len - 1));
    history_len--;
  }
  history[history_len] = linecopy;
  history_len++;

  return (1);
}

/* Set the maximum length for the history. This function can be called even
 * if there is already some history, the function will make sure to retain
 * just the latest 'len' elements if the new history length value is smaller
 * than the amount of items already inside the history. */
int lined_history_set(int len) {
  char **n;

  if (len < 1) return (0);

  if (history) {
    int tocopy = history_len;

    n = (char **)malloc(sizeof (char *) * len);
    if (n == NULL) return (0);

    /* If we can't copy everything, free the elements we'll not use. */
    if (len < tocopy) {
      int j;

      for (j = 0; j < tocopy-len; j++) free(history[j]);
      tocopy = len;
    }

    memset(n, 0, sizeof (char *) * len);
    memcpy(n, history + (history_len - tocopy), sizeof (char *) * tocopy);
    free(history);
    history = n;
  }

  history_max_len = len;
  if (history_len > history_max_len) {
    history_len = history_max_len;
  }

  return (1);
}
