/*
 * c2x.h
 *
 * Part of c2x.
 *
 * Created by Christophe Laferriere on 22/06/11.
 * Copyright 2011 INRIA. All rights reserved.
 *
 */

#ifndef _HAVE_C2X_H
#define _HAVE_C2X_H

#include <pthread.h>
#include <stdlib.h>

#ifdef C2X_USES_GTG
# include <trace_gtg.h>
#endif

/* Usefull likely and unlikely for branch prediction */
#ifndef likely
#  define likely(x)       __builtin_expect((x),1)
#  define unlikely(x)     __builtin_expect((x),0)
#endif

/* Usefull unused to get rid of warnings sometimes 
 * usage : int UNUSED (toto); */
#ifndef UNUSED
# if defined(__GNUC__)
# define UNUSED(x) x __attribute__((unused))
# elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
# endif
#endif

#if defined(NDEBUG)
#  define c2x_assert_debug( cond )
#else
#  include <errno.h>
#  include <stdio.h>
#  define c2x_assert_debug( cond ) if (!(cond)) { printf("Bad assertion, line:%i, file:'%s'\n", __LINE__, __FILE__ ); abort(); }
#endif

#ifdef _C2X_DEBUG
#define PC2X(format, ...) printf ("[c2x::%s] " format, __FUNCTION__, ## __VA_ARGS__)
#else
#define PC2X(format, ...)
#endif







extern __thread int c2x_tid;

#ifdef C2X_USES_TIMING
// per worker thread structure :
typedef struct time_wq {
  long tpop;
  long tpush;
  long tsplit;
  long tTotsplit;
  long nbsplit;
} __attribute__((aligned (64))) time_wq_t;

// global time table :
// TODO : xkaapi bug?
//extern time_wq_t* wq_time_table;
extern time_wq_t wq_time_table[48];
#endif

typedef int c2x_workqueue_index_t;

typedef struct {
  volatile c2x_workqueue_index_t beg __attribute__((aligned(64)));
  volatile c2x_workqueue_index_t end __attribute__((aligned(64)));
  c2x_workqueue_index_t size;
  volatile c2x_workqueue_index_t a_size;
  pthread_mutex_t lock;
} c2x_workqueue_t;

/* Function definition : */
typedef int (*methode_t)(void* args);

/* Struct with function pointer and arguments pointer : */
typedef struct component_call
{
  methode_t meth;
  void* args;
} component_call_t;

/* Work struct that contains the workqueue and an array of component_call_t : */
typedef struct work
{
  c2x_workqueue_t wq_1;
  component_call_t** array_1;
  c2x_workqueue_t wq_2;
  component_call_t** array_2;
} work_t;

extern work_t work;

/* Struct containing work for thief : */
typedef struct thief_work_t {
  c2x_workqueue_index_t beg;
  c2x_workqueue_index_t end;
  component_call_t** array;
  c2x_workqueue_t* wq;
} thief_work_t;

/* Splitters : */
int splitter (
    struct kaapi_task_t*                 victim_task,
    void*                                args,
    struct kaapi_listrequest_t*          lr,
    struct kaapi_listrequest_iterator_t* lri
    );

//int splitter_N (struct kaapi_stealcontext_t* sc, int nreq, kaapi_request_t* req, void* args);

/* Memory fence : should be both read & write barrier */
static inline void c2x_mem_barrier()  
{
#  ifdef __ppc__
    OSMemoryBarrier();
#  elif defined(__x86_64) || defined(__i386__)
      /* not need lfence on X86 archi: read are ordered */
      __asm__ __volatile__ ("mfence":::"memory");
#  else
#    error "bad configuration"
#  endif
}

/* Workqueue stuffs : */

static inline int c2x_workqueue_init (c2x_workqueue_t* kwq, c2x_workqueue_index_t size)
{
  c2x_mem_barrier();
  pthread_mutex_init (&kwq->lock, NULL);
#if defined(__i386__)||defined(__x86_64)||defined(__powerpc64__)||defined(__powerpc__)||defined(__ppc__)
  c2x_assert_debug( (((unsigned long)&kwq->beg) & (sizeof(c2x_workqueue_index_t)-1)) == 0 );
  c2x_assert_debug( (((unsigned long)&kwq->end) & (sizeof(c2x_workqueue_index_t)-1)) == 0 );
#else
#  error "May be alignment constraints exit to garantee atomic read write"
#endif
  kwq->beg    = 0;
  kwq->end    = 0;
  kwq->size   = size + 1;
  kwq->a_size = 0;
  return 0;
}


/* the push operation increments kwq->end. */
static inline int c2x_push(work_t *work, component_call_t* value, int priority)
{
  c2x_workqueue_t *kwq;

  if (priority == 1)
    kwq = &(work->wq_1);
  else
    kwq = &(work->wq_2);

  pthread_mutex_lock (&kwq->lock);

  PC2X("PUSH IN beg : %d\tend : %d\tsize : %d\n", kwq->beg, kwq->end, kwq->a_size);

  /* The queue is full : */
  if (kwq->a_size == kwq->size - 1)
  {
    PC2X("QUEUE FULL!!! %d\n", kwq->a_size);
    pthread_mutex_unlock (&kwq->lock);
    return -1;
  }

  if (priority == 1)
    work->array_1[kwq->beg] = value;
  else
    work->array_2[kwq->beg] = value;

  c2x_mem_barrier ();
  kwq->beg = (kwq->beg + 1) % (kwq->size - 1);
  __sync_add_and_fetch (&kwq->a_size, 1);

  PC2X("PUSH OUT beg : %d\tend : %d\tsize : %d\n", kwq->beg, kwq->end, kwq->a_size);
  pthread_mutex_unlock (&kwq->lock);

  return 1;
}

static inline int c2x_workqueue_size (c2x_workqueue_t* kwq)
{
  return kwq->a_size;
}

/** This function should only be called into a splitter to ensure correctness
    the lock of the victim kprocessor is assumed to be locked to handle conflict.
    Return 0 in case of success 
    Return ERANGE if the queue is empty or less than requested size.

    The steal operation decrement beg.
 */
static inline int c2x_workqueue_steal(
  c2x_workqueue_t* kwq,
  c2x_workqueue_index_t *beg,
  c2x_workqueue_index_t *end,
  c2x_workqueue_index_t size
)
{
  /* Not enought : */
  if (size > c2x_workqueue_size (kwq))
  {
    return -1;
  }


  *beg = kwq->end;
  kwq->end = (kwq->end + size - 1) % (kwq->size - 1);
  *end = kwq->end;
  kwq->end = (kwq->end + 1) % (kwq->size - 1);
  __sync_fetch_and_sub (&kwq->a_size, size);
  PC2X("STEAL reply beg : %d\tend : %d\n", *beg, *end);
  PC2X("STEAL queue beg : %d\tend : %d\tsize : %d\n", kwq->beg, kwq->end, kwq->a_size);

  return 1;
}

#endif //_HAVE_C2X_H
