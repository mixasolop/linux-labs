#define _XOPEN_SOURCE 700

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

typedef struct shop
{
    sem_t shop_sem;
    sem_t terminal_sem;
    int delivery_num;
    pthread_mutex_t del_mx;
    pthread_barrier_t* barr;
    pthread_cond_t cond;
    int lidlomix_count;
    pthread_mutex_t lidlomix_mx;
    int done;
    sigset_t* mask;
    pthread_t sig_pid;
} shop_t;

typedef struct customer
{
    int id;
    pthread_t pid;
    shop_t* shop;
} customer_t;

typedef struct restoker
{
    int id;
    pthread_t pid;
    shop_t* shop;
} restoker_t;

void ms_sleep(unsigned int milli)
{
    struct timespec ts = {milli / 1000, (milli % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

void usage(int argc, char* argv[])
{
    printf("%s M N K\n", argv[0]);
    printf("\t20 <= M <= 100 - number of customers\n");
    printf("\t3 <= N <= 12, N %% 3 = 0 - number of restock workers\n");
    printf("\t2 <= K <= 5 - number of payment terminals\n");
    exit(EXIT_FAILURE);
}

void* sig_vait(void* arg)
{
    shop_t* args = arg;
    int sig;
    while (1)
    {
        sigwait(args->mask, &sig);
        if (sig == SIGINT)
        {
            fprintf(stderr, "STOPPED\n");
            args->done = 1;
            break;
        }
    }
    return NULL;
}

void* shoper_work(void* arg)
{
    customer_t* args = arg;
    sem_wait(&args->shop->shop_sem);
    if (args->shop->done == 1)
    {
        sem_post(&args->shop->shop_sem);
        fprintf(stderr, "CUSTOMER <%d>: FORCE QUIT\n", args->id);
        return NULL;
    }
    fprintf(stderr, "CUSTOMER <%d>: Entered the shop\n", args->id);
    ms_sleep(200);

    pthread_mutex_lock(&args->shop->lidlomix_mx);
    while (args->shop->lidlomix_count == 0)
    {
        if (args->shop->done == 1)
        {
            pthread_mutex_unlock(&args->shop->lidlomix_mx);
            sem_post(&args->shop->terminal_sem);
            sem_post(&args->shop->shop_sem);
            fprintf(stderr, "CUSTOMER <%d>: FORCE QUIT\n", args->id);
            return NULL;
        }
        pthread_cond_wait(&args->shop->cond, &args->shop->lidlomix_mx);
    }
    args->shop->lidlomix_count--;
    fprintf(stderr, "CUSTOMER <%d>: Picked up Lidlomix\n", args->id);
    pthread_mutex_unlock(&args->shop->lidlomix_mx);

    sem_wait(&args->shop->terminal_sem);
    if (args->shop->done == 1)
    {
        sem_post(&args->shop->shop_sem);
        sem_post(&args->shop->terminal_sem);
        fprintf(stderr, "CUSTOMER <%d>: FORCE QUIT\n", args->id);
        return NULL;
    }
    fprintf(stderr, "CUSTOMER <%d>: Using payment terminal\n", args->id);
    ms_sleep(300);
    fprintf(stderr, "CUSTOMER <%d>: Paid and leaving\n", args->id);
    sem_post(&args->shop->terminal_sem);
    sem_post(&args->shop->shop_sem);

    return NULL;
}

void* restoker_work(void* arg)
{
    restoker_t* args = arg;
    int result;
    while (1)
    {
        if (args->shop->done == 1)
        {
            fprintf(stderr, "TEAM <%d>: FORCE QUIT\n", args->id);
            return NULL;
        }
        pthread_barrier_wait(&args->shop->barr[args->id]);
        pthread_mutex_lock(&args->shop->del_mx);
        if (args->shop->delivery_num == 0)
        {
            pthread_mutex_unlock(&args->shop->del_mx);
            break;
        }
        pthread_mutex_unlock(&args->shop->del_mx);
        result = pthread_barrier_wait(&args->shop->barr[args->id]);

        if (result == PTHREAD_BARRIER_SERIAL_THREAD)
        {
            pthread_mutex_lock(&args->shop->del_mx);
            fprintf(stderr, "TEAM <%d>: Delivering stock\n", args->id);
            args->shop->delivery_num--;
            pthread_mutex_unlock(&args->shop->del_mx);
        }
        pthread_barrier_wait(&args->shop->barr[args->id]);
        if (args->shop->done == 1)
        {
            fprintf(stderr, "TEAM <%d>: FORCE QUIT\n", args->id);
            return NULL;
        }
        ms_sleep(400);

        result = pthread_barrier_wait(&args->shop->barr[args->id]);
        if (result == PTHREAD_BARRIER_SERIAL_THREAD)
        {
            pthread_mutex_lock(&args->shop->lidlomix_mx);
            args->shop->lidlomix_count++;
            pthread_mutex_unlock(&args->shop->lidlomix_mx);
            fprintf(stderr, "TEAM <%d>: Lidlomix delivered\n", args->id);

            pthread_cond_broadcast(&args->shop->cond);
        }
        pthread_barrier_wait(&args->shop->barr[args->id]);
        if (args->shop->done == 1)
        {
            fprintf(stderr, "TEAM <%d>: FORCE QUIT\n", args->id);
            return NULL;
        }
    }
    return NULL;
}

int main(int argc, char* argv[])
{
    sigset_t oldMask, newMask;
    sigemptyset(&newMask);
    sigaddset(&newMask, SIGINT);
    if (pthread_sigmask(SIG_BLOCK, &newMask, &oldMask))
        ERR("SIG_BLOCK");
    if (argc != 4)
    {
        usage(argc, argv);
    }
    int M = atoi(argv[1]);
    int N = atoi(argv[2]);
    int K = atoi(argv[3]);
    if (M < 20 || M > 100 || N < 3 || N > 12 || K < 2 || K > 6 || N % 3 != 0)
    {
        usage(argc, argv);
    }

    shop_t shop;
    sem_init(&shop.shop_sem, 0, 8);
    sem_init(&shop.terminal_sem, 0, K);
    shop.delivery_num = M;
    pthread_mutex_init(&shop.del_mx, NULL);
    shop.barr = malloc(sizeof(pthread_barrier_t) * N / 3);
    shop.lidlomix_count = 0;
    pthread_cond_init(&shop.cond, NULL);
    pthread_mutex_init(&shop.lidlomix_mx, NULL);
    shop.done = 0;
    shop.mask = &newMask;
    for (int i = 0; i < N / 3; i++)
    {
        pthread_barrier_init(&shop.barr[i], 0, 3);
    }
    customer_t* customers = malloc(sizeof(customer_t) * M);

    for (int i = 0; i < M; i++)
    {
        customers[i].id = i + 1;
        customers[i].shop = &shop;
        pthread_create(&customers[i].pid, NULL, shoper_work, &customers[i]);
    }

    restoker_t* restokers = malloc(sizeof(restoker_t) * N);
    int r = 0;
    for (int i = 0; i < N; i++)
    {
        r = i / 3;
        restokers[i].shop = &shop;
        restokers[i].id = r;
        pthread_create(&restokers[i].pid, NULL, restoker_work, &restokers[i]);
    }
    pthread_create(&shop.sig_pid, NULL, sig_vait, &shop);

    for (int i = 0; i < M; i++)
    {
        pthread_join(customers[i].pid, NULL);
    }
    for (int i = 0; i < N; i++)
    {
        pthread_join(restokers[i].pid, NULL);
    }
    free(customers);
    sem_destroy(&shop.shop_sem);
    sem_destroy(&shop.terminal_sem);
    for (int i = 0; i < N / 3; i++)
    {
        pthread_barrier_destroy(&shop.barr[i]);
    }
    pthread_cond_destroy(&shop.cond);
    free(shop.barr);
    free(restokers);
}
