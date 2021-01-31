// SPDX-License-Identifier: GPL-2.0-or-later

#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "atomic.h"
#include "futextest.h"

/* The futex the main thread waits on. */
futex_t futex_main = FUTEX_INITIALIZER;
/* The futex the other thread wats on. */
futex_t futex_other = FUTEX_INITIALIZER;

/* The number of iterations to run (>1 => run benchmarks. */
static int cfg_iterations = 1;

/* If != 0, print diagnostic messages. */
static int cfg_verbose;

/* If == 0, do not use validation_counter. Useful for benchmarking. */
static int cfg_validate = 1;

/* How to swap threads. */
#define SWAP_WAKE_WAIT 1
#define SWAP_SWAP 2

/* Futex values. */
#define FUTEX_WAITING 0
#define FUTEX_WAKEUP 1

/* An atomic counter used to validate proper swapping. */
static atomic_t validation_counter;

void futex_swap_op(int mode, futex_t *futex_this, futex_t *futex_that)
{
	int ret;

	switch (mode) {
	case SWAP_WAKE_WAIT:
		futex_set(futex_this, FUTEX_WAITING);
		futex_set(futex_that, FUTEX_WAKEUP);
		futex_wake(futex_that, 1, FUTEX_PRIVATE_FLAG);
		futex_wait(futex_this, FUTEX_WAITING, NULL, FUTEX_PRIVATE_FLAG);
		if (*futex_this != FUTEX_WAKEUP) {
			fprintf(stderr, "unexpected futex_this value on wakeup\n");
			exit(1);
		}
		break;

	case SWAP_SWAP:
		futex_set(futex_this, FUTEX_WAITING);
		futex_set(futex_that, FUTEX_WAKEUP);
		ret = futex_swap(futex_this, FUTEX_WAITING, NULL,
				 futex_that, FUTEX_PRIVATE_FLAG);
		if (ret < 0 && errno == ENOSYS) {
			/* futex_swap not implemented */
			perror("futex_swap");
			exit(1);
		}
		if (*futex_this != FUTEX_WAKEUP) {
			fprintf(stderr, "unexpected futex_this value on wakeup\n");
			exit(1);
		}
		break;

	default:
		fprintf(stderr, "unknown mode in %s\n", __func__);
		exit(1);
	}
}

void *other_thread(void *arg)
{
	int mode = *((int *)arg);
	int counter;

	if (cfg_verbose)
		printf("%s started\n", __func__);

	futex_wait(&futex_other, 0, NULL, FUTEX_PRIVATE_FLAG);

	for (counter = 0; counter < cfg_iterations; ++counter) {
		if (cfg_validate) {
			int prev = 2 * counter + 1;

			if (prev != atomic_cmpxchg(&validation_counter, prev,
						   prev + 1)) {
				fprintf(stderr, "swap validation failed\n");
				exit(1);
			}
		}
		futex_swap_op(mode, &futex_other, &futex_main);
	}

	if (cfg_verbose)
		printf("%s finished: %d iteration(s)\n", __func__, counter);

	return NULL;
}

void run_test(int mode)
{
	struct timespec start, stop;
	int ret, counter;
	pthread_t thread;
	uint64_t duration;

	futex_set(&futex_other, FUTEX_WAITING);
	atomic_set(&validation_counter, 0);
	ret = pthread_create(&thread, NULL, &other_thread, &mode);
	if (ret) {
		perror("pthread_create");
		exit(1);
	}

	ret = clock_gettime(CLOCK_MONOTONIC, &start);
	if (ret) {
		perror("clock_gettime");
		exit(1);
	}

	for (counter = 0; counter < cfg_iterations; ++counter) {
		if (cfg_validate) {
			int prev = 2 * counter;

			if (prev != atomic_cmpxchg(&validation_counter, prev,
						   prev + 1)) {
				fprintf(stderr, "swap validation failed\n");
				exit(1);
			}
		}
		futex_swap_op(mode, &futex_main, &futex_other);
	}
	if (cfg_validate && validation_counter.val != 2 * cfg_iterations) {
		fprintf(stderr, "final swap validation failed\n");
		exit(1);
	}

	ret = clock_gettime(CLOCK_MONOTONIC, &stop);
	if (ret) {
		perror("clock_gettime");
		exit(1);
	}

	duration = (stop.tv_sec - start.tv_sec) * 1000000000LL +
	stop.tv_nsec - start.tv_nsec;
	if (cfg_verbose || cfg_iterations > 1) {
		printf("completed %d swap and back iterations in %lu ns: %lu ns per swap\n",
			cfg_iterations, duration,
			duration / (cfg_iterations * 2));
	}

	/* The remote thread is blocked; send it the final wake. */
	futex_set(&futex_other, FUTEX_WAKEUP);
	futex_wake(&futex_other, 1, FUTEX_PRIVATE_FLAG);
	if (pthread_join(thread, NULL)) {
		perror("pthread_join");
		exit(1);
	}
}

void usage(char *prog)
{
	printf("Usage: %s\n", prog);
	printf("  -h    Display this help message\n");
	printf("  -i N  Use N iterations to benchmark\n");
	printf("  -n    Do not validate swapping correctness\n");
	printf("  -v    Print diagnostic messages\n");
}

int main(int argc, char *argv[])
{
	int c;

	while ((c = getopt(argc, argv, "hi:nv")) != -1) {
		switch (c) {
		case 'h':
			usage(basename(argv[0]));
			exit(0);
		case 'i':
			cfg_iterations = atoi(optarg);
			break;
		case 'n':
			cfg_validate = 0;
			break;
		case 'v':
			cfg_verbose = 1;
			break;
		default:
			usage(basename(argv[0]));
			exit(1);
		}
	}

	printf("\n\n------- running SWAP_WAKE_WAIT -----------\n\n");
	run_test(SWAP_WAKE_WAIT);
	printf("PASS\n");

	printf("\n\n------- running SWAP_SWAP -----------\n\n");
	run_test(SWAP_SWAP);
	printf("PASS\n");

	return 0;
}
