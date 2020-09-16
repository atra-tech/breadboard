/*
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  Formatted output and logging system, complete with colors.
 *
 *  */

#ifndef ATRA_LOG_H
#define ATRA_LOG_H

#define LOG_ATTR(x) printf("\033[%dm", x)
#define LOG_DEFAULT_SIZE   64
#define LOG_MAX_ENTRY_SIZE 512

enum log_mode {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
};

enum log_args {
    ARG_DECIMAL = 'i',
    ARG_HEX     = 'x',
    ARG_LONG    = 'l',
    ARG_FLOAT   = 'f',
    ARG_DOUBLE  = 'd',
    ARG_STRING  = 's'
};

enum log_attr {
    ATTR_NONE     = 0,
    ATTR_BOLDON   = 1,
    ATTR_BLINKON  = 5,
    ATTR_INVERT   = 7,
    ATTR_BOLDOFF  = 22,
    ATTR_BLINKOFF = 25
};

enum log_bgcolor {
    BGCOLOR_RED     = 41,
    BGCOLOR_GREEN   = 42,
    BGCOLOR_YELLOW  = 43,
    BGCOLOR_BLUE    = 44,
    BGCOLOR_MAGENTA = 45,
    BGCOLOR_CYAN    = 46,
    BGCOLOR_WHITE   = 47,
    BGCOLOR_DEFAULT = 48
};

enum log_fgcolor {
    FGCOLOR_RED     = 91,
    FGCOLOR_GREEN   = 92,
    FGCOLOR_YELLOW  = 93,
    FGCOLOR_BLUE    = 94,
    FGCOLOR_MAGENTA = 95,
    FGCOLOR_CYAN    = 96,
    FGCOLOR_WHITE   = 97,
    FGCOLOR_DEFAULT = 98
};

int loginit();
void logprint(int type, char *msg, ...);
void logclear();
int logsave();
void logstop();

#endif
