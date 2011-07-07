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
#include "define_common.h"

#ifndef likely
#  define likely(x)       __builtin_expect((x),1)
#  define unlikely(x)     __builtin_expect((x),0)
#endif

#if defined(NDEBUG)
#  define c2x_assert_debug( cond )
#else
#  include <errno.h>
#  include <stdio.h>
#  define c2x_assert_debug( cond ) if (!(cond)) { printf("Bad assertion, line:%i, file:'%s'\n", __LINE__, __FILE__ ); abort(); }
#endif

#ifdef _C2X_DEBUG
#define PC2X(format, ...) printf ("%c[%d;%d;%dm[c2x::%s]%c[%d;%d;%dm " format, 0x1B, 0,CYAN,40,__FUNCTION__, 0x1B, 0, WHITE, 40, ## __VA_ARGS__)
#else
#define PC2X(format, ...)
#endif


typedef long c2x_workqueue_index_t;

typedef struct {
  volatile c2x_workqueue_index_t beg __attribute__((aligned(64)));
  volatile c2x_workqueue_index_t end __attribute__((aligned(64)));
  c2x_workqueue_index_t size;
  c2x_workqueue_index_t a_size;
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
  c2x_workqueue_t wq;
  component_call_t** array;
} work_t;

extern work_t work;

/* Struct containing work for thief : */
typedef struct thief_work_t {
  c2x_workqueue_index_t beg;
  c2x_workqueue_index_t end;
} thief_work_t;

/* Splitters : */
int splitter (kaapi_stealcontext_t* sc, int nreq, kaapi_request_t* req, void* args);

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
  kwq->beg    = -1;
  kwq->end    = -1;
  kwq->size   = size;
  kwq->a_size = 0;
  return 0;
}


/* the push operation increments kwq->end. */
static inline int c2x_workqueue_push (c2x_workqueue_t* kwq)
{   
  PC2X("beg : %d\tend : %d\tsize : %d\n", kwq->beg, kwq->end, kwq->a_size);

  /* The queue is full : */
  if( (kwq->beg == 0 && kwq->end == kwq->size - 1) || (kwq->beg == kwq->end + 1) )
  {
    PC2X("QUEUE FULL!!! %d\n", kwq->a_size);
    return -1;
  }

  /* The queue is empty : */
  if (kwq->beg == -1)
  {
    //mfence?
    kwq->beg = 0;
    kwq->end = 0;
    kwq->a_size += 1;
    PC2X("return\tbeg : %d\tend : %d\tsize : %d\n", kwq->beg, kwq->end, kwq->a_size);
    return 0;
  }

  /* Last position of the queue : */
  if(kwq->end == kwq->size - 1)
  {
    //mfence?
    kwq->end = 0;
    kwq->a_size += 1;
    PC2X("return\tbeg : %d\tend : %d\tsize : %d\n", kwq->beg, kwq->end, kwq->a_size);
    return 0;
  } else {
    //mfence?
    kwq->end += 1;
    kwq->a_size += 1;
    PC2X("return\tbeg : %d\tend : %d\tsize : %d\n", kwq->beg, kwq->end, kwq->a_size);
    return kwq->end;
  }

  return -1;
}

static inline int c2x_push(work_t *work, component_call_t* value)
{
  c2x_workqueue_t *kwq = &(work->wq);
  int ret = 0;

  pthread_mutex_lock (&kwq->lock); 
  ret = c2x_workqueue_push (kwq);
  if (ret == -1)
  {
    pthread_mutex_unlock (&kwq->lock);
    return -1;
  } else {
    work->array[ret] = value;
    pthread_mutex_unlock (&kwq->lock);
    return 0;
  }
}

static inline int c2x_workqueue_size (c2x_workqueue_t* kwq)
{
  return kwq->a_size;

  /* workqueue is empty : 
  int ret = 0;
  if (unlikely (kwq->beg == -1))
  {
    return 0;
  }

  if (kwq->beg < kwq->end)
  {
    ret = kwq->end - kwq->beg + 1;
    printf("Size : %d\n", kwq->end - kwq->beg + 1);
    pthread_mutex_unlock (&kwq->lock);
    return ret;
  } else {
    // TODO : is it the right value ?
    pthread_mutex_lock (&kwq->lock);
    ret = kwq->end + (kwq->size - kwq->beg) + 1;
    printf("Size : %d\n", ret);
    pthread_mutex_unlock (&kwq->lock);
    return ret;
  }*/
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

  pthread_mutex_lock (&kwq->lock);

  PC2X("beg : %d\tend : %d\tsize : %d\n", kwq->beg, kwq->end, kwq->a_size);

  /* Now the workqueue is empty : */
  if (size == c2x_workqueue_size (kwq))
  {
    if (kwq->beg > kwq->end)
    {
      *end = kwq->beg;
      *beg = kwq->end;
    } else {
      *end = kwq->end;
      *beg = kwq->beg;
    }

    kwq->beg = kwq->end = -1;
    kwq->a_size = 0;
    PC2X("return\tbeg : %d\tend : %d\tb : %d\te : %d\n", kwq->beg, kwq->end, *beg, *end);
    pthread_mutex_unlock (&kwq->lock);

    return 0;
  }

  if (kwq->beg < kwq->end)
  {
    *beg = kwq->beg;
    *end = kwq->beg + size - 1;
    kwq->beg = *end + 1;

    kwq->a_size -= size;

    PC2X("return\tbeg : %d\tend : %d\tb : %d\te : %d\n", kwq->beg, kwq->end, *beg, *end);
    pthread_mutex_unlock (&kwq->lock);

    return 0;
  } else {
    *beg = kwq->beg;
    *end = ( kwq->beg + size - 1) % kwq->size;
    kwq->beg = (*end + 1) % kwq->size;

    kwq->a_size -= size;

    PC2X("return\tbeg : %d\tend : %d\tb : %d\te : %d\n", kwq->beg, kwq->end, *beg, *end);
    pthread_mutex_unlock (&kwq->lock);

    return 0;
  }

  return -1;
}  

#endif //_HAVE_C2X_H
