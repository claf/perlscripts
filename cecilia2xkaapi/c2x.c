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

#define CONFIG_PAR_GRAIN 1

/* entrypoint */
static void thief_entrypoint
(void* args, kaapi_thread_t* thread, kaapi_stealcontext_t* sc)
{
  /* input work */
  thief_work_t* const t_work = (thief_work_t*) args;

  PC2X ("Executing [%d;%d]\n", t_work->beg, t_work->end);

  while (1)
  {
    /* Actually call the component methode : */
    work.array[t_work->beg]->meth (work.array[t_work->beg]->args);

    /* Free */
    free (work.array[t_work->beg]);

    /* update */
    if (t_work->beg == t_work->end)
    {
      break;
    } else {
      t_work->beg = (t_work->beg + 1) % work.wq.size;
    }
  }
}


int splitter
(kaapi_stealcontext_t* sc, int nreq, kaapi_request_t* req, void* args)
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
  range_size = c2x_workqueue_size(&vw->wq);
  if (range_size <= CONFIG_PAR_GRAIN)
    return 0;

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
  if (c2x_workqueue_steal(&vw->wq, &i, &j, nreq /** unit_size*/))
    goto redo_steal;

  for (; nreq; --nreq, ++req, ++nrep, j = (j - 1) % work.wq.size/*unit_size*/)
  {
    /* for reduction, a result is needed. take care of initializing it */
    //kaapi_taskadaptive_result_t* const ktr =
    //  kaapi_allocate_thief_result(req, sizeof(thief_work_t), NULL);

    /* thief work: not adaptive result because no preemption is used here  */
    thief_work_t* const tw = kaapi_reply_init_adaptive_task
      ( sc, req, (kaapi_task_body_t)thief_entrypoint, sizeof(thief_work_t), NULL/*ktr*/ );
    tw->beg = j /*- unit_size + 1*/;
    tw->end = j;

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

