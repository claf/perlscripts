/*
 * c2x.c
 *
 * Part of c2x.
 *
 * Created by Christophe Laferriere on 22/06/10.
 * Copyright 2010 INRIA. All rights reserved.
 *
 */
#include "kaapi.h"
#include "c2x.h"

#ifdef C2X_USES_TIMING
# include "timing.h"
#endif

#define CONFIG_PAR_GRAIN 1

/* per thread identifier */
__thread int c2x_tid = -1;

/* Thief Entrypoint */
static void thief_entrypoint
(void* args, kaapi_thread_t* thread, kaapi_task_t* pc)
{
  /* input work */
  thief_work_t* const t_work = (thief_work_t*) args;

  int beg = t_work->beg;
  int end = t_work->end;

  c2x_assert_debug ((beg != -1) && (end != -1));
  PC2X ("Executing %d from [%d;%d]\n", beg, t_work->beg, t_work->end);

  while (1)
  {
    /* Actually call the component methode : */
    t_work->array[beg]->meth (t_work->array[beg]->args);

    /* Free */
    free (t_work->array[beg]->args);
    free (t_work->array[beg]);

    /* update */
    if (beg == end)
    {
      break;
    } else {
      beg = (beg + 1) % t_work->wq->size;
    }
  }

  //TODO mettre ces fonctions dans un fichier avec C2X
  //doState ("Xk");
}


int splitter(
  struct kaapi_task_t*                 victim_task,
  void*                                args,
  struct kaapi_listrequest_t*          lr, 
  struct kaapi_listrequest_iterator_t* lri
)
{
  int nreq = kaapi_listrequest_iterator_count(lri);
  int ret, real_nreq = nreq;

  if (unlikely (c2x_tid == -1))
    c2x_tid = kaapi_get_self_kid();
  
#ifdef C2X_USES_TIMING
  ++(wq_time_table[c2x_tid].nbsplit);
#endif

#ifdef C2X_USES_TIMING
  tick_t ts1,ts2,tp1,tp2;
  GET_TICK(ts1);
#endif

  /* victim workqueue : */
  int priority = 1;
  work_t* const vw = (work_t*) args;
  c2x_workqueue_t* wq;

  if (c2x_workqueue_size(&vw->wq_1) > 0) {
    priority = 1;
    wq = &vw->wq_1;
  } else {
    priority = 2;
    wq = &vw->wq_2;
  }

  /* stolen range */
  c2x_workqueue_index_t i, j;
  c2x_workqueue_index_t range_size;

  /* reply count */
  int nrep = 0;

  /* size per request */
  c2x_workqueue_index_t unit_size;

 redo_steal:
  /* do not steal if range size <= PAR_GRAIN */
  range_size = c2x_workqueue_size(wq);
  if (range_size == 1)
  {
    nreq = 1;
    unit_size = 1;
    goto split;
  }

  if (range_size < CONFIG_PAR_GRAIN)
  {
#ifdef C2X_USES_TIMING
    GET_TICK(ts2);
    wq_time_table[c2x_tid].tsplit += TICK_RAW_DIFF(ts1,ts2);
    wq_time_table[c2x_tid].tTotsplit += (TICK_RAW_DIFF(ts1,ts2) * real_nreq);
    //printf ("%d, %ld, %d\n", c2x_tid, wq_time_table[c2x_tid].tTotsplit, nreq);
#endif
    return 0;
  }

  /* how much per req */
  unit_size = range_size / (nreq + 1);
  if (unit_size == 0)
  {
    nreq = (range_size / CONFIG_PAR_GRAIN) - 1;
    unit_size = CONFIG_PAR_GRAIN;
  }

  /* perform the actual steal. if the range
     changed size in between, redo the steal
   */
split:
#ifdef C2X_USES_TIMING
  GET_TICK(tp1);
#endif
  ret = c2x_workqueue_steal(wq, &i, &j, nreq /** unit_size*/);
#ifdef C2X_USES_TIMING
  GET_TICK(tp2);
  wq_time_table[c2x_tid].tpop += TICK_RAW_DIFF(tp1,tp2);
#endif

  if (ret == -1)
    goto redo_steal;

  for ( /* void */; 
      ((nreq != 0) && !kaapi_listrequest_iterator_empty(lri));
      --nreq, ++nrep, i = (i + 1) % work.wq_2.size/*unit_size*/
      )  
  {
    kaapi_request_t* req = kaapi_listrequest_iterator_get(lr, lri);

    if (req == 0)
    {
      printf ("NULL REQUEST\tnreq %d; nrep %d; i %d; j %d\n", nreq, nrep, i, j);
      abort ();
    }

    thief_work_t* const tw = kaapi_request_pushdata(req, sizeof(thief_work_t) );

    if (tw == 0)
    {
      printf ("NULL DATA POINTER\n");
      abort ();
    }

    tw->beg = i /*- unit_size + 1*/;
    tw->end = i;
    if (priority == 1) {
      tw->array = work.array_1;
      tw->wq = &(work.wq_1);
    } else {
      tw->array = work.array_2;
      tw->wq = &(work.wq_2);
    }   

    kaapi_task_init_with_flag(
        kaapi_request_toptask(req),
        (kaapi_task_body_t)thief_entrypoint,
        tw,
        KAAPI_TASK_UNSTEALABLE && KAAPI_TASK_S_CONCURRENT && KAAPI_TASK_S_NOPREEMPTION
        );  

    int result = kaapi_request_pushtask(
              req,
              victim_task
              );

    if (result == EINVAL)
    {
      printf ("BAD STACK POINTER\n");
      abort ();
    }

    result = kaapi_request_committask(req);

    if (result == EINVAL)
    {
      printf ("BAD REQUEST POINTER\n");
      abort ();
    } 

    kaapi_listrequest_iterator_next(lr, lri);
  }

  //doState ("Xk");
#ifdef C2X_USES_TIMING
  GET_TICK(ts2);
  wq_time_table[c2x_tid].tsplit += TICK_RAW_DIFF(ts1,ts2);
  wq_time_table[c2x_tid].tTotsplit += (TICK_RAW_DIFF(ts1,ts2) * real_nreq);
  //printf ("%d, %ld, %llu\n", c2x_tid, wq_time_table[c2x_tid].tsplit, TICK_RAW_DIFF(ts1,ts2));
#endif
  return nrep;
}






#if 0
// TODO : splitter_N doesn't have priority implementation yet
int splitter_N
(struct kaapi_stealcontext_t* sc, int nreq, kaapi_request_t* req, void* args)
{
  /* victim workqueue : */
  work_t* const vw = (work_t*) args;

  /* stolen range */
  c2x_workqueue_index_t i, j;
  c2x_workqueue_index_t range_size;

  /* reply count */
  int nrep = 0;

  /* size per request */
  c2x_workqueue_index_t unit_size;

 redo_steal:
  /* do not steal if range size <= PAR_GRAIN */
  range_size = c2x_workqueue_size(&vw->wq_2);
  if (range_size == 0)
  {
    return 0;
  } else if (range_size < 10*nreq)
  {
    nreq = 1;
  }

  /* how much per req */
  unit_size = range_size / nreq;

  /* perform the actual steal. if the range
     changed size in between, redo the steal
   */
  if (c2x_workqueue_steal(&vw->wq_2, &i, &j, nreq *  unit_size) == -1)
    goto redo_steal;

  for (; nreq; --nreq, ++req, ++nrep, i = (i + unit_size) % work.wq_2.size)
  {
    c2x_assert_debug (j != -1);

    /* for reduction, a result is needed. take care of initializing it */
    //kaapi_taskadaptive_result_t* const ktr =
    //  kaapi_allocate_thief_result(req, sizeof(thief_work_t), NULL);

    /* thief work: not adaptive result because no preemption is used here  */
    thief_work_t* const tw = kaapi_reply_init_adaptive_task
      ( sc, req, (kaapi_task_body_t)thief_entrypoint, sizeof(thief_work_t), NULL/*ktr*/ );
    tw->beg = i;
    tw->end = (i + unit_size - 1) % work.wq_2.size;

    //TODO : priority to add (wq and array pointers)

    /* initialize ktr task may be preempted before entrypoint */
    //((thief_work_t*)ktr->data)->beg = tw->beg;
    //((thief_work_t*)ktr->data)->end = tw->end;
    //((thief_work_t*)ktr->data)->res = 0.f;

    /* reply head, preempt head */
    PC2X ("Reply interval [%d;%d]\n", tw->beg, tw->end);
    kaapi_reply_pushhead_adaptive_task(sc, req);
  }

  return nrep;
}
#endif
