/*
 * Copyright (C) 2013 Adam Tkac <vonsch@gmail.com>
 *
 * This program can be distributed under the terms of the GNU GPLv3.
 * See the file COPYING.
 */

#include "util.h"
#include "workq.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/queue.h>

struct lazfs_workq {
	pthread_mutex_t lock;
	pthread_t *workers;
	int nworkers;
	STAILQ_HEAD(jobs_t, lazfs_workq_job) jobs;
	pthread_cond_t cond;
};

static void*
worker_thread(void *arg)
{
	lazfs_workq_t *workq = (lazfs_workq_t *) arg;
	lazfs_workq_job_t *job;

	while (1) {
		LOCK(workq->lock);

		while (STAILQ_EMPTY(&workq->jobs)) {
			WAIT(workq->cond, workq->lock);
		}

		job = STAILQ_FIRST(&workq->jobs);
		STAILQ_REMOVE_HEAD(&workq->jobs, link);
		UNLOCK(workq->lock);

		*job->ret = job->routine(job->sfd, job->dfd);
		*job->complete = 1;
		pthread_cond_broadcast(job->signal);
		free(job);
		job = NULL;
	}

	return NULL;
}

int
lazfs_workq_create(lazfs_workq_t **workqp, int threads)
{
	lazfs_workq_t *workq = NULL;
	int err, i;

	assert(workqp != NULL && *workqp == NULL);

	workq = malloc(sizeof(*workq));
	if (workq == NULL)
		return -ENOMEM;

	memset(workq, 0, sizeof(*workq));

	workq->nworkers = threads;
	workq->workers = malloc(sizeof(*workq->workers) * threads);
	if (workq->workers == NULL) {
		err = -ENOMEM;
		goto cleanup;
	}

	err = pthread_mutex_init(&workq->lock, NULL);
	assert(err == 0); /* This shouldn't fail */
	err = pthread_cond_init(&workq->cond, NULL);
	assert(err == 0); /* This shouldn't fail */

	STAILQ_INIT(&workq->jobs);

	for (i = 0; i < threads; i++) {
		err = pthread_create(&workq->workers[i], NULL, &worker_thread, workq);
		assert(err == 0); /* This shouldn't fail */
	}

	*workqp = workq;
	return 0;

cleanup:
	if (workq->workers != NULL)
		free(workq->workers);
	if (workq != NULL)
		free(workq);
		
	return err;
}

void
lazfs_workq_destroy(lazfs_workq_t **workqp)
{
	lazfs_workq_t *workq;
	int i, err;

	assert(workqp != NULL && workqp != NULL);
	workq = *workqp;

	assert(STAILQ_EMPTY(&workq->jobs));

	for (i = 0; i < workq->nworkers; i++) {
		err = pthread_cancel(workq->workers[i]);
		assert(err == 0);
		err = pthread_join(workq->workers[i], NULL);
		assert(err == 0);
	}

	free(workq->workers);
	free(workq);

	*workqp = NULL;
}

void
lazfs_workq_run(lazfs_workq_t *workq, lazfs_workq_job_t *job)
{
	int err;

	assert(workq != NULL);
	assert(job != NULL);

	LOCK(workq->lock);
	STAILQ_INSERT_TAIL(&workq->jobs, job, link);

	err = pthread_cond_signal(&workq->cond);
	UNLOCK(workq->lock);
	assert(err == 0);
}

