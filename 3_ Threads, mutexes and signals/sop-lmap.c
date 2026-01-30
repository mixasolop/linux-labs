// sop-lmap.c
// L3: The Logistic Map
// POSIX threads + signals
// low-level style, human-written

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <math.h>
#include <time.h>

typedef struct {
    int iterations;
    int bucket_count;
    double r;
    int *buckets;
    volatile sig_atomic_t *cancelled;
} worker_args_t;

static volatile sig_atomic_t sigint_received = 0;
static volatile sig_atomic_t sigusr1_received = 0;

void *signal_thread(void *arg) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGUSR1);

    int sig;
    while (1) {
        sigwait(&set, &sig);
        if (sig == SIGINT)
            sigint_received = 1;
        else if (sig == SIGUSR1)
            sigusr1_received = 1;
    }
    return NULL;
}

void *simulate(void *arg) {
    worker_args_t *a = arg;
    double x = (double)rand() / RAND_MAX;
    int remaining = a->iterations;

    for (int i = 0; i < a->iterations; i++) {
        if (*a->cancelled) {
            printf("Simulation cancelled with %d iterations remaining.\n", remaining);
            return NULL;
        }

        x = a->r * x * (1.0 - x);
        int bucket = (int)(x * a->bucket_count);
        if (bucket == a->bucket_count)
            bucket--;

        a->buckets[bucket]++;
        remaining--;
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 6) {
        fprintf(stderr,
                "Usage: %s <iterations> <bucket_count> <test_count> <worker_count> <output_file>\n",
                argv[0]);
        return 1;
    }

    int iterations   = atoi(argv[1]);
    int bucket_count = atoi(argv[2]);
    int test_count   = atoi(argv[3]);
    int worker_count = atoi(argv[4]);
    const char *outf = argv[5];

    if (iterations <= 0 || bucket_count <= 0 || test_count <= 0 || worker_count <= 0) {
        fprintf(stderr, "All numeric arguments must be > 0\n");
        return 1;
    }

    srand(time(NULL));

    sigset_t block;
    sigemptyset(&block);
    sigaddset(&block, SIGINT);
    sigaddset(&block, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &block, NULL);

    pthread_t sigthr;
    pthread_create(&sigthr, NULL, signal_thread, NULL);

    FILE *f = fopen(outf, "w");
    if (!f) {
        perror("fopen");
        return 1;
    }

    fprintf(f, "P2\n%d %d\n255\n", bucket_count, test_count);

    for (int t = 0; t < test_count; t++) {
        double r = 2.5 + (4.0 - 2.5) * t / (test_count - 1);

        int *buckets = calloc(bucket_count, sizeof(int));
        if (!buckets) {
            perror("calloc");
            return 1;
        }

        pthread_t workers[worker_count];
        worker_args_t args[worker_count];
        volatile sig_atomic_t cancelled = 0;

        for (int i = 0; i < worker_count; i++) {
            args[i].iterations = iterations;
            args[i].bucket_count = bucket_count;
            args[i].r = r;
            args[i].buckets = buckets;
            args[i].cancelled = &cancelled;
            pthread_create(&workers[i], NULL, simulate, &args[i]);
        }

        for (int i = 0; i < worker_count; i++)
            pthread_join(workers[i], NULL);

        if (sigint_received) {
            for (int i = 0; i < bucket_count; i++)
                fprintf(f, "127 ");
            fprintf(f, "\n");
            free(buckets);
            break;
        }

        if (sigusr1_received) {
            freopen(outf, "w", f);
            fprintf(f, "P2\n%d %d\n255\n", bucket_count, test_count);
            t = -1;
            sigusr1_received = 0;
            free(buckets);
            continue;
        }

        int max = iterations * worker_count;
        for (int i = 0; i < bucket_count; i++) {
            int scaled = (int)(255.0 * buckets[i] / max);
            if (scaled > 255) scaled = 255;
            fprintf(f, "%d ", scaled);
        }
        fprintf(f, "\n");

        free(buckets);
    }

    fclose(f);
    return 0;
}

