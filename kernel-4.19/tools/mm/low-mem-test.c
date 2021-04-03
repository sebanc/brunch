/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * This program is free software, released under the GPL.
 * Based on code by Minchan Kim
 *
 * User program that tests low-memory notifications.
 *
 * Compile with -lpthread
 * for instance
 * i686-pc-linux-gnu-gcc low-mem-test.c -o low-mem-test -lpthread
 *
 * Run as: low-mem-test <allocation size> <allocation interval (microseconds)>
 *
 * This program runs in two threads.  One thread continuously allocates memory
 * in the given chunk size, waiting for the specified microsecond interval
 * between allocations.  The other runs in a loop that waits for a low-memory
 * notification, then frees some of the memory that the first thread has
 * allocated.
 */

#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

int memory_chunk_size = 10000000;
int wait_time_us = 10000;
int autotesting;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct node {
	void *memory;
	struct node *prev;
	struct node *next;
};

struct node head, tail;

void work(void)
{
	int i;

	while (1) {
		struct node *new = malloc(sizeof(struct node));
		if (new == NULL) {
			perror("allocating node");
			exit(1);
		}
		new->memory = malloc(memory_chunk_size);
		if (new->memory == NULL) {
			perror("allocating chunk");
			exit(1);
		}

		pthread_mutex_lock(&mutex);
		new->next = &head;
		new->prev = head.prev;
		new->prev->next = new;
		new->next->prev = new;
		for (i = 0; i < memory_chunk_size / 4096; i++) {
			/* touch page */
			((unsigned char *) new->memory)[i * 4096] = 1;
		}

		pthread_mutex_unlock(&mutex);

		if (!autotesting) {
			printf("+");
			fflush(stdout);
		}

		usleep(wait_time_us);
	}
}

void free_memory(void)
{
	struct node *old;
	pthread_mutex_lock(&mutex);
	old = tail.next;
	if (old == &head) {
		fprintf(stderr, "no memory left to free\n");
		exit(1);
	}
	old->prev->next = old->next;
	old->next->prev = old->prev;
	free(old->memory);
	free(old);
	pthread_mutex_unlock(&mutex);
	if (!autotesting) {
		printf("-");
		fflush(stdout);
	}
}

void *poll_thread(void *dummy)
{
	struct pollfd pfd;
	int fd = open("/dev/chromeos-low-mem", O_RDONLY);
	if (fd == -1) {
		perror("/dev/chromeos-low-mem");
		exit(1);
	}

	pfd.fd = fd;
	pfd.events = POLLIN;

	if (autotesting) {
		/* Check that there is no memory shortage yet. */
		poll(&pfd, 1, 0);
		if (pfd.revents != 0) {
			exit(0);
		} else {
			fprintf(stderr, "expected no events but "
				"poll() returned 0x%x\n", pfd.revents);
			exit(1);
		}
	}

	while (1) {
		poll(&pfd, 1, -1);
		if (autotesting) {
			/* Free several chunks and check that the notification
			 * is gone. */
			free_memory();
			free_memory();
			free_memory();
			free_memory();
			free_memory();
			poll(&pfd, 1, 0);
			if (pfd.revents == 0) {
				exit(0);
			} else {
				fprintf(stderr, "expected no events but "
					"poll() returned 0x%x\n", pfd.revents);
				exit(1);
			}
		}
		free_memory();
	}
}

int main(int argc, char **argv)
{
	pthread_t threadid;

	head.next = NULL;
	head.prev = &tail;
	tail.next = &head;
	tail.prev = NULL;

	if (argc != 3 && (argc != 2 || strcmp(argv[1], "autotesting"))) {
		fprintf(stderr,
			"usage: low-mem-test <alloc size in bytes> "
			"<alloc interval in microseconds>\n"
			"or:    low-mem-test autotesting\n");
		exit(1);
	}

	if (argc == 2) {
		autotesting = 1;
	} else {
		memory_chunk_size = atoi(argv[1]);
		wait_time_us = atoi(argv[2]);
	}

	if (pthread_create(&threadid, NULL, poll_thread, NULL)) {
		perror("pthread");
		return 1;
	}

	work();
	return 0;
}
