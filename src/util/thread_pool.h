#ifndef UTIL_THREAD_POOL_H
#define UTIL_THREAD_POOL_H

#include "dynamic_array.h"

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

typedef void *(*tp_JobCallback)(void *input);
typedef uint64_t tp_JobHandle;

struct tp_Job {
  tp_JobHandle handle;

  tp_JobCallback job;

  void *in;
  void *out;
};

struct tp_ThreadPool {
  pthread_t *pool;
  uint32_t count;

  struct {
    DA_TYPE(struct tp_Job) job_queue;
    pthread_mutex_t job_mutex;
  };

  struct {
    uint64_t handle_counter;
    pthread_mutex_t handle_counter_mutex;
  };

  struct {
    DA_TYPE(struct tp_Job) completed;
    pthread_mutex_t completed_mutex;
  };

  struct {
    bool should_exit;
    pthread_mutex_t should_exit_mutex;
  };
};

// Returns a pointer to a struct tp_ThreadPool allocated on the heap.
struct tp_ThreadPool *tp_create_pool(uint32_t num_threads);

tp_JobHandle tp_add_job(struct tp_ThreadPool *pool, tp_JobCallback job,
                        void *input);

void *tp_wait_job(struct tp_ThreadPool *pool, tp_JobHandle handle);

// Takes ownership of pool and frees it.
// if pool == NULL, tp_free_pool does nothing.
void tp_free_pool(struct tp_ThreadPool *pool);

//#define UTIL_THREAD_POOL_IMPLEMENTATION
#ifdef UTIL_THREAD_POOL_IMPLEMENTATION

void *tp_worker(void *p) {
  struct tp_ThreadPool *pool = p;

  bool should_exit;

  do {
    pthread_mutex_lock(&pool->should_exit_mutex);
    should_exit = pool->should_exit;
    pthread_mutex_unlock(&pool->should_exit_mutex);

    if (should_exit) {
      break;
    }

    struct tp_Job job = { 0 };

    pthread_mutex_lock(&pool->job_mutex);

    bool has_job = false;
    if (pool->job_queue.count != 0) {
      job = DA_POP(&pool->job_queue, 0);
      has_job = true;
    }

    pthread_mutex_unlock(&pool->job_mutex);

    if (has_job) {
      job.out = job.job(job.in);

      pthread_mutex_lock(&pool->completed_mutex);
      DA_APPEND(&pool->completed, job);
      pthread_mutex_unlock(&pool->completed_mutex);
    }

  } while (1);

  return NULL;
}

struct tp_ThreadPool *tp_create_pool(uint32_t num_threads) {
  struct tp_ThreadPool *pool = calloc(1, sizeof(*pool));
  assert(pool != NULL && "Failed to allocate memory");

  pool->count = num_threads;
  pool->pool = malloc(pool->count * sizeof(*pool->pool));
  assert(pool->pool != NULL && "Failed to allocate memory");

  pthread_mutex_init(&pool->job_mutex, NULL);
  pthread_mutex_init(&pool->completed_mutex, NULL);

  pthread_mutex_init(&pool->handle_counter_mutex, NULL);

  pool->should_exit = false;
  pthread_mutex_init(&pool->should_exit_mutex, NULL);

  for (uint32_t i = 0; i < pool->count; i++) {
    errno = pthread_create(&pool->pool[i], NULL, tp_worker, pool);
    if (errno != 0) {
      fprintf(stderr, "Failed to create thread: %s\n", strerror(errno));

      pthread_mutex_lock(&pool->should_exit_mutex);
      pool->should_exit = true;
      pthread_mutex_unlock(&pool->should_exit_mutex);

      free(pool->pool);
      free(pool);
      return NULL;
    }
  }

  return pool;
}

tp_JobHandle tp_add_job(struct tp_ThreadPool *pool, tp_JobCallback job,
                        void *input) {
  pthread_mutex_lock(&pool->handle_counter_mutex);
  struct tp_Job j = {
    .handle = pool->handle_counter++,
    .job = job,
    .in = input,
    .out = NULL,
  };
  pthread_mutex_unlock(&pool->handle_counter_mutex);

  pthread_mutex_lock(&pool->job_mutex);
  DA_APPEND(&pool->job_queue, j);
  pthread_mutex_unlock(&pool->job_mutex);

  return j.handle;
}

void *tp_wait_job(struct tp_ThreadPool *pool, tp_JobHandle handle) {
  for (;;) {
    pthread_mutex_lock(&pool->completed_mutex);

    for (uint64_t i = 0; i < pool->completed.count; i++) {
      struct tp_Job *j = &DA_AT(pool->completed, i);

      if (j->handle == handle) {
        void *output = j->out;

        DA_POP(&pool->completed, i);

        pthread_mutex_unlock(&pool->completed_mutex);
        return output;
      }
    }

    pthread_mutex_unlock(&pool->completed_mutex);
  }
}

void tp_free_pool(struct tp_ThreadPool *pool) {
  if (pool == NULL) {
    return;
  }

  pthread_mutex_lock(&pool->should_exit_mutex);
  pool->should_exit = true;
  pthread_mutex_unlock(&pool->should_exit_mutex);

  for (uint32_t i = 0; i < pool->count; i++) {
    errno = pthread_join(pool->pool[i], NULL);
  }

  free(pool->pool);
  DA_FREE(&pool->job_queue);
  DA_FREE(&pool->completed);

  pthread_mutex_destroy(&pool->job_mutex);
  pthread_mutex_destroy(&pool->completed_mutex);
  pthread_mutex_destroy(&pool->should_exit_mutex);
  pthread_mutex_destroy(&pool->handle_counter_mutex);

  free(pool);
}

#endif // UTIL_THREAD_POOL_IMPLEMENTATION

#endif // UTIL_THREAD_POOL_H
