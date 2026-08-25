#define _GNU_SOURCE

#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>

static volatile sig_atomic_t g_stop = 0;

typedef struct {
    int cpu;
    int duration;
} worker_arg_t;

static void handle_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

static double elapsed_seconds(const struct timespec *start)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);

    return (double)(now.tv_sec - start->tv_sec) +
           (double)(now.tv_nsec - start->tv_nsec) / 1000000000.0;
}

static void *worker(void *arg)
{
    worker_arg_t *ctx = (worker_arg_t *)arg;

    /*
     * CPU affinity:
     * 每个线程绑定到指定 CPU。
     */
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(ctx->cpu, &cpuset);

    int ret = pthread_setaffinity_np(
        pthread_self(),
        sizeof(cpu_set_t),
        &cpuset
    );

    if (ret != 0) {
        fprintf(
            stderr,
            "pthread_setaffinity_np(cpu=%d) failed: %s\n",
            ctx->cpu,
            strerror(ret)
        );
    }

    /*
     * CPU busy loop。
     *
     * volatile 防止编译器把循环优化掉。
     */
    volatile unsigned long counter = 0;

    while (!g_stop) {
        counter++;

        /*
         * duration 在主线程统一控制。
         * worker 只负责检测 g_stop。
         */
    }

    return NULL;
}

static void print_usage(const char *program)
{
    printf(
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  --threads N       Number of worker threads\n"
        "  --duration SEC    Run duration in seconds, 0 = unlimited\n"
        "  --no-affinity     Do not set CPU affinity\n"
        "  --help            Show this help\n"
        "\n"
        "Examples:\n"
        "  %s --threads 4 --duration 10\n"
        "  %s --threads 8 --duration 60\n"
        "  %s --threads 8 --duration 60 --no-affinity\n",
        program,
        program,
        program,
        program
    );
}

int main(int argc, char *argv[])
{
    long threads_count = 0;
    long duration = 0;
    int use_affinity = 1;

    static struct option long_options[] = {
        {"threads",   required_argument, 0, 't'},
        {"duration",  required_argument, 0, 'd'},
        {"no-affinity", no_argument,     0, 'a'},
        {"help",      no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;

    while ((opt = getopt_long(
        argc,
        argv,
        "t:d:ah",
        long_options,
        NULL
    )) != -1) {

        switch (opt) {

        case 't': {
            char *end = NULL;

            errno = 0;
            threads_count = strtol(optarg, &end, 10);

            if (
                errno != 0 ||
                end == optarg ||
                *end != '\0' ||
                threads_count <= 0
            ) {
                fprintf(stderr, "Invalid --threads: %s\n", optarg);
                return EXIT_FAILURE;
            }

            break;
        }

        case 'd': {
            char *end = NULL;

            errno = 0;
            duration = strtol(optarg, &end, 10);

            if (
                errno != 0 ||
                end == optarg ||
                *end != '\0' ||
                duration < 0
            ) {
                fprintf(stderr, "Invalid --duration: %s\n", optarg);
                return EXIT_FAILURE;
            }

            break;
        }

        case 'a':
            use_affinity = 0;
            break;

        case 'h':
            print_usage(argv[0]);
            return EXIT_SUCCESS;

        default:
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (threads_count == 0) {
        threads_count = sysconf(_SC_NPROCESSORS_ONLN);

        if (threads_count <= 0) {
            threads_count = 1;
        }
    }

    long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);

    if (cpu_count <= 0) {
        fprintf(stderr, "Unable to determine CPU count\n");
        return EXIT_FAILURE;
    }

    /*
     * 防止创建过多线程。
     */
    if (threads_count > 4096) {
        fprintf(
            stderr,
            "--threads is too large: %ld (max 4096)\n",
            threads_count
        );
        return EXIT_FAILURE;
    }

    /*
     * 注册 SIGINT / SIGTERM。
     */
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) != 0) {
        perror("sigaction(SIGINT)");
        return EXIT_FAILURE;
    }

    if (sigaction(SIGTERM, &sa, NULL) != 0) {
        perror("sigaction(SIGTERM)");
        return EXIT_FAILURE;
    }

    pthread_t *threads =
        calloc((size_t)threads_count, sizeof(pthread_t));

    worker_arg_t *args =
        calloc((size_t)threads_count, sizeof(worker_arg_t));

    if (threads == NULL || args == NULL) {
        perror("calloc");
        free(threads);
        free(args);
        return EXIT_FAILURE;
    }

    printf(
        "CPU stress started\n"
        "  threads  : %ld\n"
        "  duration : %ld seconds%s\n"
        "  CPUs     : %ld\n"
        "  affinity : %s\n",
        threads_count,
        duration,
        duration == 0 ? " (unlimited)" : "",
        cpu_count,
        use_affinity ? "enabled" : "disabled"
    );

    /*
     * 创建 worker。
     */
    long created = 0;

    for (long i = 0; i < threads_count; i++) {

        args[i].cpu =
            use_affinity
            ? (int)(i % cpu_count)
            : -1;

        args[i].duration = (int)duration;

        int ret = pthread_create(
            &threads[i],
            NULL,
            worker,
            &args[i]
        );

        if (ret != 0) {
            fprintf(
                stderr,
                "pthread_create(%ld) failed: %s\n",
                i,
                strerror(ret)
            );

            g_stop = 1;
            break;
        }

        created++;
    }

    /*
     * 记录开始时间。
     */
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    /*
     * 主线程负责 duration。
     */
    while (!g_stop) {

        if (duration > 0) {
            if (elapsed_seconds(&start) >= duration) {
                g_stop = 1;
                break;
            }
        }

        /*
         * 每 100ms 检查一次。
         */
        struct timespec sleep_time;

        sleep_time.tv_sec = 0;
        sleep_time.tv_nsec = 100000000L;

        nanosleep(&sleep_time, NULL);
    }

    printf("\nStopping workers...\n");

    /*
     * 等待所有 worker 正常退出。
     */
    for (long i = 0; i < created; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("CPU stress stopped.\n");

    free(threads);
    free(args);

    return EXIT_SUCCESS;
}