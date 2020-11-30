#include <stdio.h>

#include "util.h"
#include "log.h"

int 
checkmalloc(void *ptr) {
    if(!ptr) {
        fprintf(stderr, "%s: %d: malloc error", __func__, __LINE__);
        return -1;
    }
    return 0;
}

char *
loadfile(const char *path)
{
    FILE *file = fopen(path, "rb");

    if (!file) {
        logprint(LOG_ERROR, "util: Failed to load file \"$s\": " \
                            "Unable to open file", path);
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    int length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *data = malloc((length + 2) * sizeof(char));
    fread(data, 1, length, file);
    data[length + 0] = 0;
    data[length + 1] = EOF;
    fclose(file);

    return data;
}

void
die(char *msg) {
    if (msg) {
        logprint(LOG_FATAL, msg);
    }
    exit(EXIT_FAILURE);
}
