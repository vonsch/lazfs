/*
 * Copyright (C) 2013 Adam Tkac <vonsch@gmail.com>
 *
 * This program can be distributed under the terms of the GNU GPLv3.
 * See the file COPYING.
 */

#ifndef _WORKQ_H_
#define _WORKQ_H_
#include <pthread.h>
#include <sys/queue.h>

typedef struct lazfs_workq lazfs_workq_t;

typedef struct lazfs_workq_job {
	int (*routine)(int sfd, int dfd);
	int sfd;
	int dfd;
	int *ret;
	char *complete;
	pthread_cond_t *signal;
	STAILQ_ENTRY(lazfs_workq_job) link;
} lazfs_workq_job_t;

#define LAZFS_WORKQ_JOB_INIT { NULL, -1, -1, NULL, NULL, NULL }

int
lazfs_workq_create(lazfs_workq_t **workqp, int threads);

void
lazfs_workq_destroy(lazfs_workq_t **workqp);

void
lazfs_workq_run(lazfs_workq_t *workq, lazfs_workq_job_t *job);

#endif
