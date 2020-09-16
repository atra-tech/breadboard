/*
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  Cross-platform thread pool system using SDL threads.
 *  Defines types and functions for easily creating and
 *  managing a flexible thread worker pool.
 *
 *  */

#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>

#include "thread.h"

typedef struct TPoolJob TPoolJob;

struct TPoolJob {
    ThreadFunc func;
    void *arg;
    TPoolJob *next;
};

struct TPool {
    TPoolJob *first;
    TPoolJob *last;
    SDL_mutex *queuemutex;
    SDL_mutex *countmutex;
    SDL_mutex *jobmutex;
    SDL_cond *idlecond;
    SDL_cond *jobcond;
    volatile uint alivecount;
    volatile uint workingcount;
    uint jobcount;
    uint hasjobs;
    uint stop;
};

static TPoolJob * tpooljobcreate(ThreadFunc func, void *arg);
static void tpooljobdelete(TPoolJob *job);
static TPoolJob * tpooljobget(TPool *pool);
static int tpoolworker(void *arg);

TPool *
tpoolcreate(uint tcount)
{
    TPool *pool;
    SDL_Thread *thread;
    char tname[16];
    size_t i;

    if (tcount == 0) {
        tcount = 2;
    }
    pool = calloc(1, sizeof(TPool));
    pool->queuemutex = SDL_CreateMutex();
    pool->countmutex = SDL_CreateMutex();
    pool->jobmutex = SDL_CreateMutex();
    pool->idlecond = SDL_CreateCond();
    pool->jobcond = SDL_CreateCond();

    for (i = 0; i < tcount; i++) {
        sprintf(tname, "atra_thread_%d", (int)i);
        thread = SDL_CreateThread(tpoolworker, tname, pool);
        SDL_DetachThread(thread);
    }

    while (pool->alivecount < tcount) {}

    return pool;
}

void
tpooldelete(TPool *pool)
{
    TPoolJob *job;
    TPoolJob *nextjob;

    pool->stop = 1;

    SDL_LockMutex(pool->queuemutex);
    job = pool->first;

    while (job) {
        nextjob = job->next;
        tpooljobdelete(job);
        job = nextjob;
    }

    SDL_CondBroadcast(pool->jobcond);
    SDL_UnlockMutex(pool->queuemutex);

    tpoolwait(pool);

    SDL_DestroyMutex(pool->queuemutex);
    SDL_DestroyMutex(pool->countmutex);
    SDL_DestroyMutex(pool->jobmutex);
    SDL_DestroyCond(pool->idlecond);
    SDL_DestroyCond(pool->jobcond);

    free(pool);
}

int
tpooladdjob(TPool *pool, ThreadFunc func, void *arg)
{
    TPoolJob *job;

    job = tpooljobcreate(func, arg);
    
    if (!job) {
        return 0;
    }
    SDL_LockMutex(pool->queuemutex);

    if (!pool->first) {
        pool->first = job;
        pool->last = pool->first;
    } else {
        pool->last->next = job;
        pool->last = job;
    }
    pool->jobcount++;

    SDL_LockMutex(pool->jobmutex);
    pool->hasjobs = 1;
    SDL_CondSignal(pool->jobcond);
    SDL_UnlockMutex(pool->jobmutex);

    SDL_UnlockMutex(pool->queuemutex);

    return 1;
}

/* Halt calling thread until all pool threads have finished processing */
void
tpoolwait(TPool *pool)
{
    SDL_LockMutex(pool->countmutex);

    while (pool->jobcount || pool->workingcount) {
        SDL_CondWait(pool->idlecond, pool->countmutex);
    }
    SDL_UnlockMutex(pool->countmutex);
}

TPoolJob *
tpooljobcreate(ThreadFunc func, void *arg)
{
    TPoolJob *job;

    job = malloc(sizeof(TPoolJob));
    job->func = func;
    job->arg = arg;
    job->next = NULL;

    return job;
}

void
tpooljobdelete(TPoolJob *job)
{
    free(job);
}

TPoolJob *
tpooljobget(TPool *pool)
{
    TPoolJob *job;

    SDL_LockMutex(pool->queuemutex);
    job = pool->first;

    if (!job) {
        SDL_UnlockMutex(pool->queuemutex);
        return NULL;
    }
    if (!job->next) {
        pool->first = NULL;
        pool->last = NULL;
        pool->jobcount = 0;
    } else {
        pool->first = job->next;
        pool->jobcount--;
        SDL_LockMutex(pool->jobmutex);
        pool->hasjobs = 1;
        SDL_CondSignal(pool->jobcond);
        SDL_UnlockMutex(pool->jobmutex);
    }
    SDL_UnlockMutex(pool->queuemutex);

    return job;
}

int
tpoolworker(void *arg)
{
    TPool *pool = arg;
    TPoolJob *job;

    SDL_LockMutex(pool->countmutex);
    pool->alivecount++;
    SDL_UnlockMutex(pool->countmutex);

    while (1) {
        SDL_LockMutex(pool->jobmutex);

        while (!pool->hasjobs) {
            SDL_CondWait(pool->jobcond, pool->jobmutex);
        }
        pool->hasjobs = 0;
        SDL_UnlockMutex(pool->jobmutex);

        if (pool->stop) {
            break;
        }
        SDL_LockMutex(pool->countmutex);
        job = tpooljobget(pool);
        pool->workingcount++;
        SDL_UnlockMutex(pool->countmutex);

        if (job) {
            job->func(job->arg);
            tpooljobdelete(job);
        }
        SDL_LockMutex(pool->countmutex);
        pool->workingcount--;
        
        if (!pool->workingcount) {
            SDL_CondSignal(pool->idlecond);
        }
        SDL_UnlockMutex(pool->countmutex);
    }
    SDL_LockMutex(pool->countmutex);
    pool->alivecount--;
    SDL_UnlockMutex(pool->countmutex);

    return 0;
}
