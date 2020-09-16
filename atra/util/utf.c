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

#include <string.h>

#include "utf.h"

char *
utf8create(uint size)
{
    char* result;
    if(size == 0)
        return NULL;

    result = (char*)malloc(size);
    result[0] = '\0';

    return result;
}

char *
utf8strdup(const char *str)
{
    char* result;
    if(str == NULL)
        return NULL;

    result = (char*)malloc(strlen(str)+1);
    strcpy(result, str);

    return result;
}

int
utf8strlen(const char *str)
{
    int length = 0;
    if(str == NULL)
        return 0;

    while(*str != '\0')
    {
        str = utf8next(str);
        ++length;
    }

    return length;
}

int
utf8chrsize(const char *chr)
{
    if(chr == NULL)
        return 0;

    if((uchar)*chr <= 0x7F)
        return 1;
    else if((uchar)*chr < 0xE0)
        return 2;
    else if((uchar)*chr < 0xF0)
        return 3;
    else
        return 4;
    return 1;
}

int
utf8chrcpy(char *buf, const char *src, int bufsize)
{
    int charsize;
    if(buf == NULL || src == NULL || bufsize < 1)
        return 0;

    charsize = utf8chrsize(src);
    if(charsize > bufsize)
        return 0;

    memcpy(buf, src, charsize);
    return charsize;
}

const char *
utf8next(const char *str)
{
    return str + utf8chrsize(str);
}

int
utf8strinsert(char *str, int pos, const char *src, int maxbytes)
{
    int poschar;
    int len;
    int addlen;
    int ulen;
    const char* strstart = str;

    if(str == NULL || src == NULL)
        return 0;

    len = strlen(str);
    addlen = strlen(src);
    ulen = utf8strlen(str);

    if(pos == -1)
        pos = ulen;

    if(pos < 0 || pos > ulen || len + addlen + 1 > maxbytes)
        return 0;

    // Move string pointer to the proper position
    poschar = 0;
    while(*str != '\0' && poschar < pos)
    {
        str = (char*)utf8next(str);
        ++poschar;
    }

    // Move the rest of the string out of the way
    memmove(str + addlen, str, len - (str - strstart) + 1);

    // Copy in the new characters
    memcpy(str, src, addlen);

    return 1;
}

void
utf8strdel(char *str, int pos)
{
    if(str == NULL || pos < 0)
        return;

    while(*str != '\0')
    {
        if(pos == 0)
        {
            int chrstoerase = utf8chrsize(str);
            int remainbytes = strlen(str) + 1;
            memmove(str, str + chrstoerase, remainbytes);
            break;
        }

        str = (char*)utf8next(str);
        --pos;
    }
}

char
utf8toascii(char *str)
{
    if ((uchar)str[0] < 0x80) {
        return str[0];
    } else if ((uchar)str[0] == 0xC2 || (uchar)str[0] == 0xC3) {
        return str[1] - (char)0x20;
    } else {
        return '\0';
    }
}

char *
utf8tounicode(uchar chr)
{
    uchar *str = calloc(2, sizeof(uchar));

    if (chr < 0x80) {
        str[0] = chr;
    } else if (chr < 0xC0) {
        str[0] = 0xC2;
        str[1] = chr + 0x20;
    } else {
        str[0] = 0xC3;
        str[1] = chr + 0x20;
    }

    return (char *)str;
}

char *
utf8toasciistr(char *str, uint length)
{
    uint i, index;
    char *result = calloc(length + 1, sizeof(char));

    for (i = 0, index = 0; i < length; i++, index++) {
        result[i] = utf8toascii(str + index);

        if ((uchar)result[i] >= 0x80) {
            index++;
        }
    }

    return result;
}

char *
utf8tounicodestr(char *str, uint length)
{
    uint i, index;
    char *out;
    char *result = calloc(length * 2 + 1, sizeof(char));
    
    for (i = 0, index = 0; i < length; i++) {
        out = utf8tounicode(str[i]);
        result[index] = out[0];

        if (out[1] > 0) {
            result[index + 1] = out[1];
            index++;
        }
    }

    return result;
}

void
utf8delete(char *str)
{
    free(str);
}
