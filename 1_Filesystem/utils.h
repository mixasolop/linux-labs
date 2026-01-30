#define _XOPEN_SOURCE 700

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

// DO NOT MODIFY THIS FILE!!!

/**
 * @brief ensure that there is a directory at given path - create if necessary, exit program if it cannot be done
 */
void checked_mkdir(char* path)
{
    if (mkdir(path, 0755) != 0)
    {
        if (errno != EEXIST)
            ERR("mkdir");
    }
    struct stat s;
    if (stat(path, &s))
        ERR("stat");
    if (!S_ISDIR(s.st_mode))
    {
        printf("%s is not a valid dir, exiting\n", path);
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief create all necessary dirs on given path (it excludes file name, ie. the last component)
 */
void make_path(char* path)
{
    char* temp = strdup(path);
    if (!temp)
        ERR("strdup");
    for (char* p = temp + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = '\0';
            checked_mkdir(temp);
            *p = '/';
        }
    }
    free(temp);
}

/**
 * @brief Splits input string by spaces.
 * @param count is set to number of output strings
 * @return array of splitted strings - it's allocated here and needs to be free using free_strings()
 */
char** split_string(const char* const input_string, int* count)
{
    char* input_copy = strdup(input_string);
    if (!input_copy)
        ERR("strdup");
    char* temp_output[256];
    *count = 0;
    char* token = strtok(input_copy, " ");
    while (token != NULL)
    {
        temp_output[(*count)++] = strdup(token);
        if (*count >= 256)
        {
            fprintf(stderr, "Too many components of the path (>256)! Aborting\n");
            exit(EXIT_FAILURE);
        }
        token = strtok(NULL, " ");
    }

    char** output_strings = malloc(sizeof(char*) * (*count));
    if (!output_strings)
        ERR("malloc");
    for (int i = 0; i < *count; i++)
    {
        output_strings[i] = temp_output[i];
    }

    free(input_copy);
    return output_strings;
}

/**
 * @brief free strings created by split_string()
 */
void free_strings(char** strings, int count)
{
    for (int i = 0; i < count; i++)
    {
        free(strings[i]);
    }
    free(strings);
}

/**
 * @brief try chdir and fail whole program on error
 */
void checked_chdir(char* path)
{
    if (chdir(path))
        ERR("chdir");
}

/**
 * @param path to a file
 * @param output parameter - stat results
 * @param should stat follow links (stat vs lstat)
 * @param set to true if program should fail on missing file
 * @return 0 on correct call. -1 if exit_on_missing_file=false and file is missing.
 */
int checked_stat(char* path, struct stat* s, bool follow_links, bool exit_on_missing_file)
{
    int status = follow_links ? stat(path, s) : lstat(path, s);
    if (status)
    {
        if (exit_on_missing_file == false && errno == ENOENT)
            return -1;
        ERR("stat");
    }
    return 0;
}

/**
 * @brief concatenate input string
 * @param number of input strings
 * @return newly allocated concatenation
 */
char* concat(int n, ...)
{
    int total_lenght = 0;
    va_list args;
    va_start(args, n);
    for (int i = 0; i < n; i++)
    {
        total_lenght += strlen(va_arg(args, char*));
    }
    va_end(args);

    char* re = malloc(total_lenght + 1);
    if (!re)
        ERR("malloc");
    re[0] = 0;

    va_start(args, n);
    for (int i = 0; i < n; i++)
    {
        strcat(re, va_arg(args, char*));
    }
    va_end(args);
    return re;
}

/**
 * @brief get the link name for shared file in stage 3
 * @param user requesting sharing
 * @param path to file in user drive
 * @return newly allocated link name
 */
char* get_link_name(char* user, char* path)
{
    unsigned long long hash = 0;
    int N = strlen(path);
    for (int i = 0; i < N; i++)
    {
        hash += hash * 127 + path[i];
    }
    char buff[18];
    sprintf(buff + 1, "%llx", hash);
    buff[0] = '-';
    return concat(2, user, buff);
}
