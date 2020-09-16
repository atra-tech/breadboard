/*
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  Library for manipulation of strings and characters
 *  in UTF-8 format.
 *
 *  */

#ifndef ATRA_UTF8_H
#define ATRA_UTF8_H

#include "util.h"

char *utf8create(uint size);
char *utf8strdup(const char *str);
int utf8strlen(const char *str);
int utf8chrsize(const char *chr);
int utf8chrcpy(char *buf, const char *src, int bufsize);
const char *utf8next(const char *str);
int utf8strinsert(char *str, int pos, const char *src, int maxbytes);
void utf8strdel(char *str, int pos);
char utf8toascii(char *str);
char *utf8tounicode(uchar chr);
char *utf8toasciistr(char *str, uint length);
char *utf8tounicodestr(char *str, uint length);
void utf8delete(char *str);

#endif
