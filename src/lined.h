/* lined.h -- VERSION 1.0
 *
 * A linenoise based line editing library for embedded systems.
 *
 * See lined.cpp for more information.
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2016-2017, Clemens Kirchgatterer <clemens at 1541 dot org>
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
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
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
 */

#ifndef _LINED_H_
#define _LINED_H_

#include "terminal.h"

typedef struct lined_t lined_t;

typedef struct lined_completion_t {
  int len;
  char **cvec;
} lined_completion_t;

typedef void (lined_completion_cb)(const char *, lined_completion_t *);
typedef char *(lined_hints_cb)(const char *, int *color, int *bold);
typedef void (lined_free_hints_cb)(void *);

void lined_set_completion_cb(lined_completion_cb *);
void lined_set_hints_cb(lined_hints_cb *);
void lined_set_free_hints_cb(lined_free_hints_cb *);

void lined_add_completion(lined_completion_t *, const char *);

lined_t *lined_begin(Terminal *term);
char    *lined_line(lined_t *);
int      lined_poll(lined_t *);
void     lined_resize(lined_t *, int w, int h);
void     lined_prompt(lined_t *l, const char *prompt);
void     lined_echo(lined_t *, int echo);
void     lined_reset(lined_t *);
void     lined_end(lined_t *);

void lined_history_free(void);
int  lined_history_add(const char *line);
int  lined_history_len(int len);
//int lined_history_save(const char *filename);
//int lined_history_load(const char *filename);

#endif // _LINED_H_
