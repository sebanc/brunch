/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef IPTS_THREAD_H
#define IPTS_THREAD_H

#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/sched.h>

/**
 * struct ipts_thread - Slim wrapper around kthread for MEI purposes.
 *
 * Calling kthread_stop puts the thread into a interrupt state, causes the MEI API functions
 * to always return -EINTR. This makes it impossible to issue MEI commands from a thread that
 * is shutting itself down.
 *
 * By using a custom boolean variable we can instruct the thread to shut down independently of
 * kthread_stop. Then we wait and only call kthread_stop once the thread has singnaled that
 * it has successfully shut itself down.
 *
 * @thread:
 *     The thread that is being wrapped.
 *
 * @data:
 *     Data pointer for passing data to the function running inside the thread.
 *
 * @threadfn:
 *     The function that will be executed inside of the thread.
 *
 * @should_stop:
 *     Whether the thread should start shutting itself down.
 *
 * @done:
 *     Completion object that will be triggered by the actual thread runner function once
 *     threadfn has exited.
 */
struct ipts_thread {
	struct task_struct *thread;

	void *data;
	int (*threadfn)(struct ipts_thread *thread);

	bool should_stop;
	struct completion done;
};

bool ipts_thread_should_stop(struct ipts_thread *thread);
int ipts_thread_stop(struct ipts_thread *thread);
int ipts_thread_start(struct ipts_thread *thread, int (*threadfn)(struct ipts_thread *thread),
		      void *data, const char *name);

#endif /* IPTS_THREAD_H */
