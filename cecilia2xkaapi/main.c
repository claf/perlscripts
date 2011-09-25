/*
 * main.c
 *
 * Generated by c2x version 0.9 adapt.
 *
 * Created by Christophe Laferriere on 22/11/10.
 * Copyright 2010 INRIA. All rights reserved.
 *
 */
#include <kaapi.h>
#include <sys/time.h>
#include "main.h"
#include "c2x.h"

#ifdef C2X_USES_PAPI
# include <papi.h>
# define NUM_EVENTS 2
# define ERROR_RETURN(retval) { fprintf(stderr, "Error %d %s:line %d: \n",retval,__FILE__,__LINE__);  exit(retval); }
#endif

#ifdef C2X_USES_TIMING
# include "timing.h"
#endif

#define MAX_COMP_CALL 20000

work_t work;
uint64_t epoc = 0;

#ifdef C2X_USES_TIMING
// TODO : xkaapi bug ?
//  time_wq_t* wq_time_table;
  time_wq_t wq_time_table[48];
#endif

int main (int argc, char** argv)
{
  int UNUSED (nb_threads);
  main_arg_t* args;
  kaapi_thread_t* thread;
  kaapi_stealcontext_t* sc;

  /* WorkQueue initialization : */
  work.array = (component_call_t **) malloc (sizeof (component_call_t*) * MAX_COMP_CALL * 2);
  c2x_workqueue_init (&work.wq, MAX_COMP_CALL * 2);

#ifdef C2X_USES_TIMING
  /* Timing lib. initialisation : */
  timing_init();
#endif //C2X_USES_TIMING

  /* Library initialization : */
  kaapi_init (&argc, &argv);
  nb_threads = kaapi_getconcurrency();
  thread = kaapi_self_thread ();

#ifdef C2X_USES_TIMING
// TODO : xkaapi bug ?
//  wq_time_table = (time_wq_t*) malloc (sizeof (time_wq_t) * nb_threads);
  for (int i = 0; i < nb_threads; i++)
  {
    wq_time_table[i].tpop    = 0;
    wq_time_table[i].tpush   = 0;
    wq_time_table[i].tsplit  = 0;
    wq_time_table[i].nbsplit = 0;
  }
#endif

#ifdef C2X_USES_PAPI
  /*must be initialized to PAPI_NULL before calling PAPI_create_event*/
  int EventSet = PAPI_NULL;
  
  /*This is where we store the values we read from the eventset */
  long long values[NUM_EVENTS];

  /* We use number to keep track of the number of events in the EventSet */
  int retval,number=NUM_EVENTS,Events[NUM_EVENTS];
  
  /*************************************************************************** 
   *  This part initializes the library and compares the version number of the*
   * header file, to the version of the library, if these don't match then it *
   * is likely that PAPI won't work correctly.If there is an error, retval    *
   * keeps track of the version number.                                       *
   ***************************************************************************/
  if ( (retval = PAPI_library_init(PAPI_VER_CURRENT)) != PAPI_VER_CURRENT )
    ERROR_RETURN(retval);
  
  /* Creating the eventset */
  if ( (retval = PAPI_create_eventset(&EventSet)) != PAPI_OK)
    ERROR_RETURN(retval);
  
  /* Add Total Instructions Executed to the EventSet */
  if ( (retval = PAPI_add_event(EventSet, PAPI_TOT_INS)) != PAPI_OK)
    ERROR_RETURN(retval);
  
  /* Add Total Cycles event to the EventSet */
  if ( (retval = PAPI_add_event(EventSet, PAPI_TOT_CYC)) != PAPI_OK)
    ERROR_RETURN(retval);
  
  /* Add Total Cycles event to the EventSet */
  if ( (retval = PAPI_add_event(EventSet, PAPI_RES_STL)) != PAPI_OK)
    ERROR_RETURN(retval);

  /* Get the number of events in the event set */
  if ( (retval = PAPI_list_events(EventSet, Events, &number)) != PAPI_OK)
    ERROR_RETURN(retval);
  
  if ( number != NUM_EVENTS )
    printf("There are %d events in the event set\n", number);
  
  /* Start counting */
  if ( (retval = PAPI_start(EventSet)) != PAPI_OK)
    ERROR_RETURN(retval);
#endif

#ifdef C2X_USES_GTG
  /* GTG init for PAJE traces : */
  setTraceType (PAJE);
  initTrace ("trace", 0, GTG_FLAG_NONE);

  addContType ("T", "0", "Thread");
  addStateType ("S", "T", "State");
  
  addEntityValue ("Te", "S", "Thief_Entrypoint", GTG_DARKGREY);
  addEntityValue ("St", "S", "Steal", GTG_LIGHTGREY);
  addEntityValue ("Xk", "S", "XKaapi", GTG_WHITE);
  addEntityValue ("Sp", "S", "Splitter", GTG_DARKBLUE);
  addContainer (0, "P", "T", "0", "machine", "0");
  addVarType ("W", "WorkQueue Size", "T");

  /* Add a container for every kaapi_threads : */
  for (int i = 1; i <= nb_threads; i++)
  {
    char buf[3];
    sprintf(buf, "%d", i); 
    addContainer (0, buf, "T", "P", buf, "0");
  }
#endif //C2X_USES_GTG

  struct timeval start;
  gettimeofday (&start, NULL);
  epoc = start.tv_sec * 1000000 + start.tv_usec;

  /* Push an adaptive task : */
  sc = kaapi_task_begin_adaptive (thread, KAAPI_SC_CONCURRENT, $splitter, &work);

  /* Now let's do the work! */
  args = (main_arg_t*) malloc (sizeof (main_arg_t));
  args->argc = argc;
  args->argv = argv;
  main_body (args);

  /* Wait for thieves : */
  kaapi_task_end_adaptive (sc);

#ifdef C2X_USES_GTG
  /* Trace writing */
  endTrace();
#endif //C2X_USES_GTG

#ifdef C2X_USES_TIMING
  printf ("\n*** C2X TIMING INFOS ***\n\n");
  long tpop = 0;
  long tpush = 0;
  long tsplit = 0;
  long tTotsplit = 0;
  for (int i = 0; i < nb_threads; i++)
  {
    printf ("Time for thread %d :\\t pop :%ld\n",i,(long)tick2usec(wq_time_table[i].tpop));
    tpop += (long)tick2usec(wq_time_table[i].tpop);

    printf ("Time for thread %d :\\t push :%ld\n",i,(long)tick2usec(wq_time_table[i].tpush));
    tpush += (long)tick2usec(wq_time_table[i].tpush);

    printf ("Time for thread %d :\\t split :%ld\n",i,(long)tick2usec(wq_time_table[i].tsplit));
    tsplit += (long)tick2usec(wq_time_table[i].tsplit);

    printf ("Time for thread %d :\\t Totsplit :%ld\n",i,(long)tick2usec(wq_time_table[i].tTotsplit));
    tTotsplit += (long)tick2usec(wq_time_table[i].tTotsplit);

    printf ("Time for thread %d :\\t nb_split :%ld\n",i, wq_time_table[i].nbsplit);
    printf ("--------------------------------\n");
  }
  printf ("Total pop : %ld\n", tpop);
  printf ("Total push : %ld\n", tpush);
  printf ("Total splitter : %ld\n", tsplit);
  printf ("Total real splitter : %ld\n", tTotsplit);

  printf ("\n\n*** END ***\n");
#endif

  /* Library termination : */
  kaapi_finalize();

#ifdef C2X_USES_PAPI
  /* Stop counting and store the values into the array */
  if ( (retval = PAPI_stop(EventSet, values)) != PAPI_OK)
    ERROR_RETURN(retval);

  printf("Total instructions executed are\t%lld \n", values[0] );
  printf("Total cycles executed are\t%lld \n",values[1]);
  printf("Total stalled cycles are\t%lld \n",values[2]);

  /* free the resources used by PAPI */
  PAPI_shutdown();
#endif

  /* Free some stuff : */
  // TODO : destroy or not?
  //c2x_workqueue_destroy (&work.wq);
  free (work.array);
  free (args);

#ifdef C2X_USES_TIMING
// TODO : xkaapi bug ?
//  free (wq_time_table);
#endif

  return 0;
}
