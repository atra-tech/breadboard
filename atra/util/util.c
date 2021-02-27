#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dirent.h>
#endif

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
    fread(data, sizeof(char), length, file);
    data[length + 0] = 0;
    data[length + 1] = EOF;
    fclose(file);

    return data;
}

void
savefile(const char *path, const char *data, size_t nitems)
{
    FILE *file = fopen(path, "wb");

    if (!file) {
        logprint(LOG_ERROR, "util: Failed to save file \"$s\": " \
                            "Unable to open file", path);
        return;
    }
    fwrite(data, sizeof(char), nitems, file);
    fclose(file);
}

void
die(char *msg) {
    if (msg) {
        logprint(LOG_FATAL, msg);
    }
    exit(EXIT_FAILURE);
}

#ifdef _WIN32

List *
listfilesat(char *path, char *ext)
{
    WIN32_FIND_DATA fd_file;
    HANDLE find = NULL;
    List *result;
    char *entry;
    char fname[2048];

    sprintf(fname, "%s\\*.%s", path, ext);

    if ((find = FindFirstFile(fname, &fd_file)) == INVALID_HANDLE_VALUE) {
        logprint(LOG_ERROR, "util: Path not found: \"$s\"", path);
        return NULL;
    }
    result = listcreate();

    do {
        if (strcmp(fd_file.cFileName, ".") &&
                strcmp(fd_file.cFileName, "..")) {
            entry = malloc((strlen(fd_file.cFileName) + 1) * sizeof(char));
            strcpy(entry, fd_file.cFileName);
            listpushback(result, entry);
        }
    } while (FindNextFile(find, &fd_file));

    FindClose(find);

    return result;
}

#else

List *
listfilesat(char *path, char *ext)
{
    struct dirent *files;
    DIR *dir = opendir(path);
    List *result;
    char *entry;

    if (!dir) {
        logprint(LOG_ERROR, "util: Unable to open directory: $s", path);
        return NULL;
    }
    result = listcreate();

    while ((files = readdir(dir))) {
        entry = strrchr(files->d_name, '.');
        
        if (entry && !strcmp(++entry, ext)) {
            entry = malloc((strlen(files->d_name) + 1) * sizeof(char));
            strcpy(entry, files->d_name);
            listpushback(result, entry);
        }
    }
    closedir(dir);

    return result;
}

#endif

void
removefile(const char *path)
{
    remove(path);
}

char *
strdup(const char *str)
{
    size_t len;
    char *result;

    len = strlen(str) + 1;
    result = malloc(len * sizeof(char));
    memcpy(result, str, len);

    return result;
}
