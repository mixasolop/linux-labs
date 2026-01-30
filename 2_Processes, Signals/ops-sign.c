// ops-sign.c
// Solution for task "Oikoumene"
// low-level I/O only (except printf)

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>

#define ERR(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

static int n_docs;
static int n_children;
static pid_t *children = NULL;
static int delay_ms = 200;

static volatile sig_atomic_t got_usr1 = 0;
static volatile sig_atomic_t got_usr2 = 0;

void sigusr1_handler(int sig) {
    (void)sig;
    got_usr1 = 1;
}

void sigusr2_handler(int sig) {
    (void)sig;
    got_usr2 = 1;
}

void sigint_handler(int sig) {
    (void)sig;
    printf("[%d] Signing process interrupted!\n", getpid());
    kill(0, SIGINT);
}

void msleep(int ms) {
    usleep(ms * 1000);
}

void read_last_line_and_append(int page) {
    char fname[32];
    snprintf(fname, sizeof(fname), "page%d", page);

    int fd = open(fname, O_RDWR | O_CREAT, 0644);
    if (fd < 0) ERR("open");

    off_t size = lseek(fd, 0, SEEK_END);
    if (size == 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d\n", getpid());
        write(fd, buf, strlen(buf));
        close(fd);
        return;
    }

    off_t pos = size - 1;
    char c;
    while (pos > 0) {
        lseek(fd, pos, SEEK_SET);
        read(fd, &c, 1);
        if (c == '\n') {
            pos++;
            break;
        }
        pos--;
    }
    lseek(fd, pos, SEEK_SET);

    char line[256];
    int len = read(fd, line, sizeof(line) - 1);
    line[len] = 0;

    char name[128];
    int p;
    sscanf(line, "%s %d", name, &p);

    char out[256];
    snprintf(out, sizeof(out), "%s %d\n", name, abs(p - getpid()) + 1);
    write(fd, out, strlen(out));

    close(fd);
}

void child_process(int idx, const char *name) {
    struct sigaction sa1 = {0}, sa2 = {0}, sai = {0};
    sigemptyset(&sa1.sa_mask);
    sigemptyset(&sa2.sa_mask);
    sigemptyset(&sai.sa_mask);

    sa1.sa_handler = sigusr1_handler;
    sa2.sa_handler = sigusr2_handler;
    sai.sa_handler = sigint_handler;

    sigaction(SIGUSR1, &sa1, NULL);
    sigaction(SIGUSR2, &sa2, NULL);
    sigaction(SIGINT,  &sai, NULL);

    sigset_t mask;
    sigemptyset(&mask);

    int received = 0;

    while (1) {
        sigsuspend(&mask);

        if (got_usr1) {
            got_usr1 = 0;
            received++;
            printf("[%d] I, %s, sign the declaration\n", getpid(), name);
            read_last_line_and_append(received);
            msleep(delay_ms);

            if (idx > 0)
                kill(children[idx - 1], SIGUSR1);
            else
                kill(getppid(), SIGUSR2);
        }

        if (got_usr2) {
            got_usr2 = 0;
            printf("[%d] Done!\n", getpid());
            if (idx > 0)
                kill(children[idx - 1], SIGUSR2);
            exit(EXIT_SUCCESS);
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <n> <name1> ... <nameN>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    n_docs = atoi(argv[1]);
    if (n_docs < 1 || n_docs > 25) {
        fprintf(stderr, "Invalid number of documents\n");
        exit(EXIT_FAILURE);
    }

    n_children = argc - 2;
    children = calloc(n_children, sizeof(pid_t));
    if (!children) ERR("calloc");

    struct sigaction sai = {0};
    sigemptyset(&sai.sa_mask);
    sai.sa_handler = sigint_handler;
    sigaction(SIGINT, &sai, NULL);

    for (int i = 0; i < n_children; i++) {
        pid_t pid = fork();
        if (pid < 0) ERR("fork");
        if (pid == 0) {
            printf("[%d] My name is %s\n", getpid(), argv[i + 2]);
            child_process(i, argv[i + 2]);
            exit(EXIT_SUCCESS);
        }
        children[i] = pid;
    }

    struct sigaction sa2 = {0};
    sigemptyset(&sa2.sa_mask);
    sa2.sa_handler = sigusr2_handler;
    sigaction(SIGUSR2, &sa2, NULL);

    for (int i = 0; i < n_docs; i++) {
        kill(children[n_children - 1], SIGUSR1);
        msleep(delay_ms);
    }

    kill(children[n_children - 1], SIGUSR2);

    for (int i = 0; i < n_children; i++)
        waitpid(children[i], NULL, 0);

    free(children);
    return 0;
}

