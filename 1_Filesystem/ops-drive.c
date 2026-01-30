#define _XOPEN_SOURCE 700
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include "utils.h"

void set(char* user, char* path, char* value)
{
    printf("%s SET %s to %s\n", user, path, value);
    checked_mkdir("drive");
    checked_chdir("drive");
    checked_mkdir(user);

    checked_chdir(user);
    make_path(path);
    struct stat s;
    stat(path, &s);
    if (S_ISDIR(s.st_mode))
    {
        fprintf(stderr, "SET: Cannot create dir: %s\n", path);
        checked_chdir("..");
        checked_chdir("..");
        return;
    }
    int fd;
    if ((fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0755)) == -1)
    {
        ERR("open");
    }
    int len = strlen(value);
    if (write(fd, value, len) != len)
    {
        ERR("write");
    }
    if (close(fd))
    {
        ERR("close");
    }

    checked_chdir("..");
    checked_chdir("..");
}

void get(char* user, char* path)
{
    path[strcspn(path, "\n")] = '\0';
    checked_chdir("drive");
    checked_chdir(user);
    struct stat s;
    stat(path, &s);
    if (checked_stat(path, &s, false, false) == -1)
    {
        fprintf(stderr, "GET: Cannot read file: %s", path);
    }
    if (S_ISDIR(s.st_mode))
    {
        fprintf(stderr, "GET: Cannot read dir: %s\n", path);
        checked_chdir("..");
        checked_chdir("..");
        return;
    }
    FILE* fp = fopen(path, "r");
    if (fp == NULL)
    {
        ERR("fopen");
    }
    size_t len = 0;
    char* line = NULL;
    ssize_t n;

    while ((n = getline(&line, &len, fp)) != -1)
    {
        line[len - 1] = '\0';
        printf("%s %s: %s\n", user, path, line);
    }

    if (fclose(fp))
    {
        ERR("fclose");
    }
    free(line);

    checked_chdir("..");
    checked_chdir("..");
}

void share(char* user, char* path)
{
    path[strcspn(path, "\n")] = '\0';
    printf("%s SHARE %s\n", user, path);
    checked_mkdir("shared");
    checked_chdir("shared");

    char* link_name = NULL;
    char* file_path = NULL;

    link_name = get_link_name(user, path);
    file_path = concat(5, "..", "/drive/", user, "/", path);

    if (symlink(file_path, link_name) == -1)
    {
        ERR("symlink");
    }
    free(link_name);
    free(file_path);
    checked_chdir("..");
}

int walk(const char* fpath, const struct stat* sb, int typeflag, struct FTW* ftwbuf)
{
    if (typeflag != FTW_D)
    {
        return remove(fpath);
    }
    return 0;
}

void erase(char* user, char* path)
{
    printf("%s ERASE %s\n", user, path);
    path[strcspn(path, "\n")] = '\0';
    checked_chdir("drive");
    checked_chdir(user);
    struct stat s;
    stat(path, &s);
    if (S_ISREG(s.st_mode))
    {
        if (remove(path))
        {
            ERR("remove");
        }
        char* path_to_link = NULL;
        char* name_link = get_link_name(user, path);
        fprintf(stderr, "%s", name_link);
        path_to_link = concat(3, "../../", "shared/", name_link);
        if (checked_stat(path_to_link, &s, true, false) == -1)
        {
            fprintf(stderr, "TESTESTYES");
            if (unlink(path_to_link) == -1)
            {
                ERR("unlink");
            }
        }
        free(path_to_link);
        free(name_link);
    }
    else
    {
        // https://stackoverflow.com/questions/70695049/nftw-remove-the-directory-content-without-removing-the-top-dir-itself
        nftw(path, walk, 100, FTW_DEPTH);
    }
    checked_chdir("..");
    checked_chdir("..");
}

int main(int argc, char** argv)
{
    char* line = NULL;
    size_t len = 0;
    ssize_t n;
    int words_num = 0;
    char** words;
    if (argc == 1)
    {
        while ((n = getline(&line, &len, stdin)) != -1)
        {
            words = split_string(line, &words_num);
            if (strcmp(words[0], "SET") == 0 && words_num == 4)
            {
                set(words[1], words[2], words[3]);
            }
            else if (strcmp(words[0], "GET") == 0 && words_num == 3)
            {
                get(words[1], words[2]);
            }
            else if (strcmp(words[0], "SHARE") == 0 && words_num == 3)
            {
                share(words[1], words[2]);
            }
            else if (strcmp(words[0], "ERASE") == 0 && words_num == 3)
            {
                erase(words[1], words[2]);
            }
            free_strings(words, words_num);
        }
        free(line);
    }
    if (argc == 2)
    {
        FILE* fp;
        fp = fopen(argv[1], "r");
        if (fp == NULL)
        {
            ERR("fopen");
        }
        while ((n = getline(&line, &len, fp)) != -1)
        {
            words = split_string(line, &words_num);
            if (strcmp(words[0], "SET") == 0 && words_num == 4)
            {
                set(words[1], words[2], words[3]);
            }
            else if (strcmp(words[0], "GET") == 0 && words_num == 3)
            {
                get(words[1], words[2]);
            }
            else if (strcmp(words[0], "SHARE") == 0 && words_num == 3)
            {
                share(words[1], words[2]);
            }
            else if (strcmp(words[0], "ERASE") == 0 && words_num == 3)
            {
                erase(words[1], words[2]);
            }
            free_strings(words, words_num);
        }
        free(line);
        if (fclose(fp))
        {
            ERR("fclose");
        }
    }
}
