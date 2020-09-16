/*
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  Formatted output and logging system, complete with colors.
 *
 *  */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "log.h"
#include "util.h"

typedef struct {
    char **ents;
    int *modes;
    int size;
    int init;
} Log;

static Log log;

int
loginit()
{
    int i;

    log.ents = (char**)malloc(LOG_DEFAULT_SIZE * sizeof(char*));
    log.modes = (int*)malloc(LOG_DEFAULT_SIZE * sizeof(int));

    if(checkmalloc(log.ents) < 0)
        return -1;

    if(checkmalloc(log.modes) < 0)
        return -1;

    for (i = 0; i < LOG_DEFAULT_SIZE; i++) {
        log.ents[i] = (char*)malloc(LOG_MAX_ENTRY_SIZE * sizeof(char));
        if (checkmalloc(log.ents[i]) < 0)
            return -1;
        memset(log.ents[i], 0, LOG_MAX_ENTRY_SIZE * sizeof(char));
    }
    log.size = 0;
    memset(log.modes, 0, LOG_DEFAULT_SIZE * sizeof(int));
    log.init = 1;

    return 0;
}

void
logprint(int mode, char *str, ...)
{
    char *arg, *prevptr;
    int i, found, prevpos;
    va_list list;

    if (!log.init) {
        loginit();
    }

    /* TODO: Replace with resize operation */
    if (log.size == LOG_DEFAULT_SIZE)
    {
        logsave();
        log.size = 0;
    }

    arg = (char *)malloc(LOG_MAX_ENTRY_SIZE * sizeof(char));
    if(checkmalloc(arg) < 0)
        return;

    LOG_ATTR(ATTR_BOLDON);
    switch(mode)
    {
    case LOG_INFO:
        LOG_ATTR(FGCOLOR_WHITE);
        strcpy(log.ents[log.size], "[INFO]  ");
        break;
    case LOG_WARN:
        LOG_ATTR(FGCOLOR_YELLOW);
        strcpy(log.ents[log.size], "[WARN]  ");
        break;
    case LOG_ERROR:
        LOG_ATTR(FGCOLOR_RED);
        strcpy(log.ents[log.size], "[ERROR] ");
        break;
    case LOG_FATAL:
        LOG_ATTR(FGCOLOR_MAGENTA);
        strcpy(log.ents[log.size], "[FATAL] ");
        break;
    default:
        logprint(LOG_ERROR, "log: Received invalid mode value");
        return;
    }
    log.modes[log.size] = mode;

    va_start(list, str);

    /* Replace $* instances with arguments */
    found = 0;
    prevpos = 0;
    prevptr = str;
    if (str != NULL) {
        for (i = 0; str[i] != '\0'; i++) {
            if (str[i] == '$') {
                switch (str[i+1]) {
                case ARG_DECIMAL:
                    sprintf(arg, "%d", va_arg(list, int));
                    found = 1;
                    break;
                case ARG_HEX:
                    sprintf(arg, "0x%x", va_arg(list, int));
                    found = 1;
                    break;
                case ARG_FLOAT:
                    sprintf(arg, "%f", (float)va_arg(list, double));
                    found = 1;
                    break;
                case ARG_DOUBLE:
                    sprintf(arg, "%f", va_arg(list, double));
                    found = 1;
                    break;
                case ARG_STRING:
                    sprintf(arg, "%s", va_arg(list, char *));
                    found = 1;
                    break;
                default:
                    break;
                }
                if (found) {
                    /* Add arg to log entry and move pointer past $* */
                    strncat(log.ents[log.size], prevptr, i - prevpos);
                    strcat(log.ents[log.size], arg);
                    prevptr += (i - prevpos) + 2;
                    prevpos += (i - prevpos) + 2;
                    found = 0;
                    i++;
                }
            }
        }
        strcat(log.ents[log.size], prevptr);
        switch (log.modes[log.size]) {
        case LOG_INFO:
            fprintf(stdout, "%s\n", log.ents[log.size]);
            break;
        case LOG_WARN:  /* FALLTHROUGH */
        case LOG_ERROR:
        case LOG_FATAL:
            fprintf(stdout, "%s\n", log.ents[log.size]);
            break;
        default:
            break;
        }
        log.size++;
    }
    LOG_ATTR(ATTR_NONE);
    va_end(list);
    free(arg);
    arg = NULL;
}

void
logclear()
{
    fprintf(stdout, "\033[2J\033[;H");
}

int
logsave()
{
    int i;
    FILE* logfile;

    logfile = fopen("log.dat", "a");
    if (logfile == NULL) {
        logprint(LOG_ERROR, "log: Error writing to log file: " \
                            "Unable to open file");
        return -1;
    } else {
        for (i = 0; i < LOG_DEFAULT_SIZE; i++) {
            if(log.ents[i][0] == '\0')
                break;
            fprintf(logfile, "%s\n", log.ents[i]);
        }
        fprintf(logfile, "\n");
    }
    fclose(logfile);

    return 0;
}

void
logstop()
{
    int i;

    if (log.init) {
        for (i = 0; i < LOG_DEFAULT_SIZE; i++) {
            free(log.ents[i]);
            log.ents[i] = NULL;
        }
        free(log.ents);
        free(log.modes);
        log.ents = NULL;
        log.modes = NULL;
    } else {
       logprint(LOG_ERROR, "log: Attempted to terminate uninitialised log");
       logstop();
    }
}
