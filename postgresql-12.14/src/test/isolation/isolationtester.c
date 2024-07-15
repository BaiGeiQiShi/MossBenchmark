   
                                        
   
                     
                                                     
   

#include "postgres_fe.h"

#include <sys/time.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "datatype/timestamp.h"
#include "libpq-fe.h"
#include "pqexpbuffer.h"
#include "pg_getopt.h"

#include "isolationtester.h"

#define PREP_WAITING "isolationtester_waiting"

   
                                                                                
                                                
   
typedef struct IsoConnInfo
{
                                                        
  PGconn *conn;
                                                       
  int backend_pid;
  const char *backend_pid_str;
                                       
  const char *sessionname;
                                                        
  PermutationStep *active_step;
                                                           
  int total_notices;
} IsoConnInfo;

static IsoConnInfo *conns = NULL;
static int nconns = 0;

                                                 
static bool any_new_notice = false;

                                                               
static int64 max_step_wait = 300 * USECS_PER_SEC;

static void
check_testspec(TestSpec *testspec);
static void
run_testspec(TestSpec *testspec);
static void
run_all_permutations(TestSpec *testspec);
static void
run_all_permutations_recurse(TestSpec *testspec, int *piles, int nsteps, PermutationStep **steps);
static void
run_named_permutations(TestSpec *testspec);
static void
run_permutation(TestSpec *testspec, int nsteps, PermutationStep **steps);

                                        
#define STEP_NONBLOCK 0x1                                             
#define STEP_RETRY 0x2                                                     

static int
try_complete_steps(TestSpec *testspec, PermutationStep **waiting, int nwaiting, int flags);
static bool
try_complete_step(TestSpec *testspec, PermutationStep *pstep, int flags);

static int
step_qsort_cmp(const void *a, const void *b);
static int
step_bsearch_cmp(const void *a, const void *b);

static bool
step_has_blocker(PermutationStep *pstep);
static void
printResultSet(PGresult *res);
static void
isotesterNoticeProcessor(void *arg, const char *message);
static void
blackholeNoticeProcessor(void *arg, const char *message);

static void
disconnect_atexit(void)
{
  int i;

  for (i = 0; i < nconns; i++)
  {
    if (conns[i].conn)
    {
      PQfinish(conns[i].conn);
    }
  }
}

int
main(int argc, char **argv)
{
  const char *conninfo;
  const char *env_wait;
  TestSpec *testspec;
  PGresult *res;
  PQExpBufferData wait_query;
  int opt;
  int i;

  while ((opt = getopt(argc, argv, "V")) != -1)
  {
    switch (opt)
    {
    case 'V':
      puts("isolationtester (PostgreSQL) " PG_VERSION);
      exit(0);
    default:
      fprintf(stderr, "Usage: isolationtester [CONNINFO]\n");
      return EXIT_FAILURE;
    }
  }

     
                                                                             
                                                                             
     
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

     
                                                                             
                                                                          
                                                                          
                 
     
  if (argc > optind)
  {
    conninfo = argv[optind];
  }
  else
  {
    conninfo = "dbname = postgres";
  }

     
                                                                             
                                                                       
     
  env_wait = getenv("PGISOLATIONTIMEOUT");
  if (env_wait != NULL)
  {
    max_step_wait = ((int64)atoi(env_wait)) * USECS_PER_SEC;
  }

                                     
  spec_yyparse();
  testspec = &parseresult;

                                                               
  check_testspec(testspec);

  printf("Parsed test spec with %d sessions\n", testspec->nsessions);

     
                                                                        
                                                    
     
  nconns = 1 + testspec->nsessions;
  conns = (IsoConnInfo *)pg_malloc0(nconns * sizeof(IsoConnInfo));
  atexit(disconnect_atexit);

  for (i = 0; i < nconns; i++)
  {
    const char *sessionname;

    if (i == 0)
    {
      sessionname = "control connection";
    }
    else
    {
      sessionname = testspec->sessions[i - 1]->name;
    }

    conns[i].sessionname = sessionname;

    conns[i].conn = PQconnectdb(conninfo);
    if (PQstatus(conns[i].conn) != CONNECTION_OK)
    {
      fprintf(stderr, "Connection %d failed: %s", i, PQerrorMessage(conns[i].conn));
      exit(1);
    }

       
                                                                          
                                                                      
                                                                          
                  
       
    if (i != 0)
    {
      PQsetNoticeProcessor(conns[i].conn, isotesterNoticeProcessor, (void *)&conns[i]);
    }
    else
    {
      PQsetNoticeProcessor(conns[i].conn, blackholeNoticeProcessor, NULL);
    }

       
                                                                         
                                                          
                                                                          
                                                                          
       
    res = PQexecParams(conns[i].conn,
        "SELECT set_config('application_name',\n"
        "  current_setting('application_name') || '/' || $1,\n"
        "  false)",
        1, NULL, &sessionname, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
      fprintf(stderr, "setting of application name failed: %s", PQerrorMessage(conns[i].conn));
      exit(1);
    }

                                                                
    conns[i].backend_pid = PQbackendPID(conns[i].conn);
    conns[i].backend_pid_str = psprintf("%d", conns[i].backend_pid);
  }

     
                                                                           
                                                                       
                                                                           
                                                                             
                                                                            
                                 
     
  initPQExpBuffer(&wait_query);
  appendPQExpBufferStr(&wait_query, "SELECT pg_catalog.pg_isolation_test_session_is_blocked($1, '{");
                                                                        
  appendPQExpBufferStr(&wait_query, conns[1].backend_pid_str);
  for (i = 2; i < nconns; i++)
  {
    appendPQExpBuffer(&wait_query, ",%s", conns[i].backend_pid_str);
  }
  appendPQExpBufferStr(&wait_query, "}')");

  res = PQprepare(conns[0].conn, PREP_WAITING, wait_query.data, 0, NULL);
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "prepare of lock wait query failed: %s", PQerrorMessage(conns[0].conn));
    exit(1);
  }
  PQclear(res);
  termPQExpBuffer(&wait_query);

     
                                                                     
                           
     
  run_testspec(testspec);

  return 0;
}

   
                                                                       
   
static void
check_testspec(TestSpec *testspec)
{
  int nallsteps;
  Step **allsteps;
  int i, j, k;

                                                  
  nallsteps = 0;
  for (i = 0; i < testspec->nsessions; i++)
  {
    nallsteps += testspec->sessions[i]->nsteps;
  }

  allsteps = pg_malloc(nallsteps * sizeof(Step *));

  k = 0;
  for (i = 0; i < testspec->nsessions; i++)
  {
    for (j = 0; j < testspec->sessions[i]->nsteps; j++)
    {
      allsteps[k++] = testspec->sessions[i]->steps[j];
    }
  }

  qsort(allsteps, nallsteps, sizeof(Step *), step_qsort_cmp);

                                              
  for (i = 1; i < nallsteps; i++)
  {
    if (strcmp(allsteps[i - 1]->name, allsteps[i]->name) == 0)
    {
      fprintf(stderr, "duplicate step name: %s\n", allsteps[i]->name);
      exit(1);
    }
  }

                                              
  for (i = 0; i < testspec->nsessions; i++)
  {
    Session *session = testspec->sessions[i];

    for (j = 0; j < session->nsteps; j++)
    {
      session->steps[j]->session = i;
    }
  }

     
                                                                          
                                       
     
  for (i = 0; i < testspec->npermutations; i++)
  {
    Permutation *p = testspec->permutations[i];

    for (j = 0; j < p->nsteps; j++)
    {
      PermutationStep *pstep = p->steps[j];
      Step **this = (Step **)bsearch(pstep->name, allsteps, nallsteps, sizeof(Step *), step_bsearch_cmp);

      if (this == NULL)
      {
        fprintf(stderr, "undefined step \"%s\" specified in permutation\n", pstep->name);
        exit(1);
      }
      pstep->step = *this;

                                               
      pstep->step->used = true;
    }

       
                                                               
                                                                       
                                                                       
                                                 
       
    for (j = 0; j < p->nsteps; j++)
    {
      PermutationStep *pstep = p->steps[j];

      for (k = 0; k < pstep->nblockers; k++)
      {
        PermutationStepBlocker *blocker = pstep->blockers[k];
        int n;

        if (blocker->blocktype == PSB_ONCE)
        {
          continue;                         
        }

        blocker->step = NULL;
        for (n = 0; n < p->nsteps; n++)
        {
          PermutationStep *otherp = p->steps[n];

          if (strcmp(otherp->name, blocker->stepname) == 0)
          {
            blocker->step = otherp->step;
            break;
          }
        }
        if (blocker->step == NULL)
        {
          fprintf(stderr, "undefined blocking step \"%s\" referenced in permutation step \"%s\"\n", blocker->stepname, pstep->name);
          exit(1);
        }
                                                              
        if (blocker->step->session == pstep->step->session)
        {
          fprintf(stderr, "permutation step \"%s\" cannot block on its own session\n", pstep->name);
          exit(1);
        }
      }
    }
  }

     
                                                                            
                                                                          
                                                           
     
  if (testspec->permutations)
  {
    for (i = 0; i < nallsteps; i++)
    {
      if (!allsteps[i]->used)
      {
        fprintf(stderr, "unused step name: %s\n", allsteps[i]->name);
      }
    }
  }

  free(allsteps);
}

   
                                                                   
                         
   
static void
run_testspec(TestSpec *testspec)
{
  if (testspec->permutations)
  {
    run_named_permutations(testspec);
  }
  else
  {
    run_all_permutations(testspec);
  }
}

   
                                                   
   
static void
run_all_permutations(TestSpec *testspec)
{
  int nsteps;
  int i;
  PermutationStep *steps;
  PermutationStep **stepptrs;
  int *piles;

                                                       
  nsteps = 0;
  for (i = 0; i < testspec->nsessions; i++)
  {
    nsteps += testspec->sessions[i]->nsteps;
  }

                                              
  steps = (PermutationStep *)pg_malloc0(sizeof(PermutationStep) * nsteps);
  stepptrs = (PermutationStep **)pg_malloc(sizeof(PermutationStep *) * nsteps);
  for (i = 0; i < nsteps; i++)
  {
    stepptrs[i] = steps + i;
  }

     
                                                                         
                                                                          
                                                                     
                                                     
     
                                                                         
                                    
     
  piles = pg_malloc(sizeof(int) * testspec->nsessions);
  for (i = 0; i < testspec->nsessions; i++)
  {
    piles[i] = 0;
  }

  run_all_permutations_recurse(testspec, piles, 0, stepptrs);

  free(steps);
  free(stepptrs);
  free(piles);
}

static void
run_all_permutations_recurse(TestSpec *testspec, int *piles, int nsteps, PermutationStep **steps)
{
  int i;
  bool found = false;

  for (i = 0; i < testspec->nsessions; i++)
  {
                                                                     
    if (piles[i] < testspec->sessions[i]->nsteps)
    {
      Step *newstep = testspec->sessions[i]->steps[piles[i]];

         
                                                                   
                                                                         
                                                            
         
      steps[nsteps]->name = newstep->name;
      steps[nsteps]->step = newstep;

      piles[i]++;

      run_all_permutations_recurse(testspec, piles, nsteps + 1, steps);

      piles[i]--;

      found = true;
    }
  }

                                                                          
  if (!found)
  {
    run_permutation(testspec, nsteps, steps);
  }
}

   
                                           
   
static void
run_named_permutations(TestSpec *testspec)
{
  int i;

  for (i = 0; i < testspec->npermutations; i++)
  {
    Permutation *p = testspec->permutations[i];

    run_permutation(testspec, p->nsteps, p->steps);
  }
}

static int
step_qsort_cmp(const void *a, const void *b)
{
  Step *stepa = *((Step **)a);
  Step *stepb = *((Step **)b);

  return strcmp(stepa->name, stepb->name);
}

static int
step_bsearch_cmp(const void *a, const void *b)
{
  char *stepname = (char *)a;
  Step *step = *((Step **)b);

  return strcmp(stepname, step->name);
}

   
                       
   
static void
run_permutation(TestSpec *testspec, int nsteps, PermutationStep **steps)
{
  PGresult *res;
  int i;
  int nwaiting = 0;
  PermutationStep **waiting;

  waiting = pg_malloc(sizeof(PermutationStep *) * testspec->nsessions);

  printf("\nstarting permutation:");
  for (i = 0; i < nsteps; i++)
  {
    printf(" %s", steps[i]->name);
  }
  printf("\n");

                     
  for (i = 0; i < testspec->nsetupsqls; i++)
  {
    res = PQexec(conns[0].conn, testspec->setupsqls[i]);
    if (PQresultStatus(res) == PGRES_TUPLES_OK)
    {
      printResultSet(res);
    }
    else if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
      fprintf(stderr, "setup failed: %s", PQerrorMessage(conns[0].conn));
      exit(1);
    }
    PQclear(res);
  }

                                 
  for (i = 0; i < testspec->nsessions; i++)
  {
    if (testspec->sessions[i]->setupsql)
    {
      res = PQexec(conns[i + 1].conn, testspec->sessions[i]->setupsql);
      if (PQresultStatus(res) == PGRES_TUPLES_OK)
      {
        printResultSet(res);
      }
      else if (PQresultStatus(res) != PGRES_COMMAND_OK)
      {
        fprintf(stderr, "setup of session %s failed: %s", conns[i + 1].sessionname, PQerrorMessage(conns[i + 1].conn));
        exit(1);
      }
      PQclear(res);
    }
  }

                     
  for (i = 0; i < nsteps; i++)
  {
    PermutationStep *pstep = steps[i];
    Step *step = pstep->step;
    IsoConnInfo *iconn = &conns[1 + step->session];
    PGconn *conn = iconn->conn;
    bool mustwait;
    int j;

       
                                                                        
                                                                        
       
    if (iconn->active_step != NULL)
    {
      struct timeval start_time;

      gettimeofday(&start_time, NULL);

      while (iconn->active_step != NULL)
      {
        PermutationStep *oldstep = iconn->active_step;

           
                                                           
                                                                   
                       
           
        if (!try_complete_step(testspec, oldstep, STEP_RETRY))
        {
                                                                 
          int w;

          for (w = 0; w < nwaiting; w++)
          {
            if (oldstep == waiting[w])
            {
              break;
            }
          }
          if (w >= nwaiting)
          {
            abort();                   
          }
          if (w + 1 < nwaiting)
          {
            memmove(&waiting[w], &waiting[w + 1], (nwaiting - (w + 1)) * sizeof(PermutationStep *));
          }
          nwaiting--;
        }

           
                                                                   
                                                                 
                                                                       
                                                                    
                                               
           
        nwaiting = try_complete_steps(testspec, waiting, nwaiting, STEP_NONBLOCK | STEP_RETRY);

           
                                                                   
                                                                   
                                                            
                                                                      
                                                                     
                                                            
           
        if (iconn->active_step != NULL)
        {
          struct timeval current_time;
          int64 td;

          gettimeofday(&current_time, NULL);
          td = (int64)current_time.tv_sec - (int64)start_time.tv_sec;
          td *= USECS_PER_SEC;
          td += (int64)current_time.tv_usec - (int64)start_time.tv_usec;
          if (td > 2 * max_step_wait)
          {
            fprintf(stderr, "step %s timed out after %d seconds\n", iconn->active_step->name, (int)(td / USECS_PER_SEC));
            fprintf(stderr, "active steps are:");
            for (j = 1; j < nconns; j++)
            {
              IsoConnInfo *oconn = &conns[j];

              if (oconn->active_step != NULL)
              {
                fprintf(stderr, " %s", oconn->active_step->name);
              }
            }
            fprintf(stderr, "\n");
            exit(1);
          }
        }
      }
    }

                                       
    if (!PQsendQuery(conn, step->sql))
    {
      fprintf(stdout, "failed to send query for step %s: %s\n", step->name, PQerrorMessage(conn));
      exit(1);
    }

                                      
    iconn->active_step = pstep;

                                                                       
    for (j = 0; j < pstep->nblockers; j++)
    {
      PermutationStepBlocker *blocker = pstep->blockers[j];

      if (blocker->blocktype == PSB_NUM_NOTICES)
      {
        blocker->target_notices = blocker->num_notices + conns[blocker->step->session + 1].total_notices;
      }
    }

                                                      
    mustwait = try_complete_step(testspec, pstep, STEP_NONBLOCK);

                                                                         
    nwaiting = try_complete_steps(testspec, waiting, nwaiting, STEP_NONBLOCK | STEP_RETRY);

                                                                  
    if (mustwait)
    {
      waiting[nwaiting++] = pstep;
    }
  }

                                       
  nwaiting = try_complete_steps(testspec, waiting, nwaiting, STEP_RETRY);
  if (nwaiting != 0)
  {
    fprintf(stderr, "failed to complete permutation due to mutually-blocking steps\n");
    exit(1);
  }

                                    
  for (i = 0; i < testspec->nsessions; i++)
  {
    if (testspec->sessions[i]->teardownsql)
    {
      res = PQexec(conns[i + 1].conn, testspec->sessions[i]->teardownsql);
      if (PQresultStatus(res) == PGRES_TUPLES_OK)
      {
        printResultSet(res);
      }
      else if (PQresultStatus(res) != PGRES_COMMAND_OK)
      {
        fprintf(stderr, "teardown of session %s failed: %s", conns[i + 1].sessionname, PQerrorMessage(conns[i + 1].conn));
                                            
      }
      PQclear(res);
    }
  }

                        
  if (testspec->teardownsql)
  {
    res = PQexec(conns[0].conn, testspec->teardownsql);
    if (PQresultStatus(res) == PGRES_TUPLES_OK)
    {
      printResultSet(res);
    }
    else if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
      fprintf(stderr, "teardown failed: %s", PQerrorMessage(conns[0].conn));
                                          
    }
    PQclear(res);
  }

  free(waiting);
}

   
                                                
                                                   
                                         
                                                       
   
static int
try_complete_steps(TestSpec *testspec, PermutationStep **waiting, int nwaiting, int flags)
{
  int old_nwaiting;
  bool have_blocker;

  do
  {
    int w = 0;

                                                                       
    any_new_notice = false;

                                                         
    old_nwaiting = nwaiting;
    have_blocker = false;

                                                
    while (w < nwaiting)
    {
      if (try_complete_step(testspec, waiting[w], flags))
      {
                                            
        if (waiting[w]->nblockers > 0)
        {
          have_blocker = true;
        }
        w++;
      }
      else
      {
                                         
        if (w + 1 < nwaiting)
        {
          memmove(&waiting[w], &waiting[w + 1], (nwaiting - (w + 1)) * sizeof(PermutationStep *));
        }
        nwaiting--;
      }
    }

       
                                                                           
                                                                      
                                                                         
                                                                       
                                                                          
                                                                      
       
  } while (have_blocker && (nwaiting < old_nwaiting || any_new_notice));
  return nwaiting;
}

   
                                                                             
                                                    
   
                                                                              
                                                                      
   
                                                                            
                                                                           
                                                                      
                                                                          
                                                                            
   
static bool
try_complete_step(TestSpec *testspec, PermutationStep *pstep, int flags)
{
  Step *step = pstep->step;
  IsoConnInfo *iconn = &conns[1 + step->session];
  PGconn *conn = iconn->conn;
  fd_set read_set;
  struct timeval start_time;
  struct timeval timeout;
  int sock = PQsocket(conn);
  int ret;
  PGresult *res;
  PGnotify *notify;
  bool canceled = false;

     
                                                                            
                                                                        
                                                                           
     
  if (!(flags & STEP_RETRY))
  {
    int i;

    for (i = 0; i < pstep->nblockers; i++)
    {
      PermutationStepBlocker *blocker = pstep->blockers[i];

      if (blocker->blocktype == PSB_ONCE)
      {
        printf("step %s: %s <waiting ...>\n", step->name, step->sql);
        return true;
      }
    }
  }

  if (sock < 0)
  {
    fprintf(stderr, "invalid socket: %s", PQerrorMessage(conn));
    exit(1);
  }

  gettimeofday(&start_time, NULL);
  FD_ZERO(&read_set);

  while (PQisBusy(conn))
  {
    FD_SET(sock, &read_set);
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000;                                       

    ret = select(sock + 1, &read_set, NULL, NULL, &timeout);
    if (ret < 0)                        
    {
      if (errno == EINTR)
      {
        continue;
      }
      fprintf(stderr, "select failed: %s\n", strerror(errno));
      exit(1);
    }
    else if (ret == 0)                                            
    {
      struct timeval current_time;
      int64 td;

                                                                   
      if (flags & STEP_NONBLOCK)
      {
        bool waiting;

        res = PQexecPrepared(conns[0].conn, PREP_WAITING, 1, &conns[step->session + 1].backend_pid_str, NULL, NULL, 0);
        if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) != 1)
        {
          fprintf(stderr, "lock wait query failed: %s", PQerrorMessage(conns[0].conn));
          exit(1);
        }
        waiting = ((PQgetvalue(res, 0, 0))[0] == 't');
        PQclear(res);

        if (waiting)                                
        {
             
                                                                  
                                                                   
                                                                   
                                                               
                                                                  
                                                            
                               
             
          if (!PQconsumeInput(conn))
          {
            fprintf(stderr, "PQconsumeInput failed: %s\n", PQerrorMessage(conn));
            exit(1);
          }
          if (!PQisBusy(conn))
          {
            break;
          }

             
                                                                     
                      
             
          if (!(flags & STEP_RETRY))
          {
            printf("step %s: %s <waiting ...>\n", step->name, step->sql);
          }
          return true;
        }
                               
      }

                                                                 
      gettimeofday(&current_time, NULL);
      td = (int64)current_time.tv_sec - (int64)start_time.tv_sec;
      td *= USECS_PER_SEC;
      td += (int64)current_time.tv_usec - (int64)start_time.tv_usec;

         
                                                                    
         
                                                                         
                                                                 
                                                                   
                                                                       
             
         
      if (td > max_step_wait && !canceled)
      {
        PGcancel *cancel = PQgetCancel(conn);

        if (cancel != NULL)
        {
          char buf[256];

          if (PQcancel(cancel, buf, sizeof(buf)))
          {
               
                                                                 
                                          
               
            printf("isolationtester: canceling step %s after %d seconds\n", step->name, (int)(td / USECS_PER_SEC));
            canceled = true;
          }
          else
          {
            fprintf(stderr, "PQcancel failed: %s\n", buf);
          }
          PQfreeCancel(cancel);
        }
      }

         
                                                          
         
                                                                       
                                                                         
                                                          
         
      if (td > 2 * max_step_wait)
      {
        fprintf(stderr, "step %s timed out after %d seconds\n", step->name, (int)(td / USECS_PER_SEC));
        exit(1);
      }
    }
    else if (!PQconsumeInput(conn))                               
    {
      fprintf(stderr, "PQconsumeInput failed: %s\n", PQerrorMessage(conn));
      exit(1);
    }
  }

     
                                                                           
                   
     
  if (step_has_blocker(pstep))
  {
    if (!(flags & STEP_RETRY))
    {
      printf("step %s: %s <waiting ...>\n", step->name, step->sql);
    }
    return true;
  }

                                            
  if (flags & STEP_RETRY)
  {
    printf("step %s: <... completed>\n", step->name);
  }
  else
  {
    printf("step %s: %s\n", step->name, step->sql);
  }

  while ((res = PQgetResult(conn)))
  {
    switch (PQresultStatus(res))
    {
    case PGRES_COMMAND_OK:
    case PGRES_EMPTY_QUERY:
      break;
    case PGRES_TUPLES_OK:
      printResultSet(res);
      break;
    case PGRES_FATAL_ERROR:

         
                                                                
                                                                     
                                                               
         
      {
        const char *sev = PQresultErrorField(res, PG_DIAG_SEVERITY);
        const char *msg = PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY);

        if (sev && msg)
        {
          printf("%s:  %s\n", sev, msg);
        }
        else
        {
          printf("%s\n", PQresultErrorMessage(res));
        }
      }
      break;
    default:
      printf("unexpected result status: %s\n", PQresStatus(PQresultStatus(res)));
    }
    PQclear(res);
  }

                                                 
  PQconsumeInput(conn);
  while ((notify = PQnotifies(conn)) != NULL)
  {
                                                    
    const char *sendername = NULL;
    char pidstring[32];
    int i;

    for (i = 0; i < testspec->nsessions; i++)
    {
      if (notify->be_pid == conns[i + 1].backend_pid)
      {
        sendername = conns[i + 1].sessionname;
        break;
      }
    }
    if (sendername == NULL)
    {
                                                                     
      snprintf(pidstring, sizeof(pidstring), "PID %d", notify->be_pid);
      sendername = pidstring;
    }
    printf("%s: NOTIFY \"%s\" with payload \"%s\" from %s\n", testspec->sessions[step->session]->name, notify->relname, notify->extra, sendername);
    PQfreemem(notify);
    PQconsumeInput(conn);
  }

                               
  iconn->active_step = NULL;

  return false;
}

                                                                  
static bool
step_has_blocker(PermutationStep *pstep)
{
  int i;

  for (i = 0; i < pstep->nblockers; i++)
  {
    PermutationStepBlocker *blocker = pstep->blockers[i];
    IsoConnInfo *iconn;

    switch (blocker->blocktype)
    {
    case PSB_ONCE:
                                                            
      break;
    case PSB_OTHER_STEP:
                                              
      iconn = &conns[1 + blocker->step->session];
      if (iconn->active_step && iconn->active_step->step == blocker->step)
      {
        return true;
      }
      break;
    case PSB_NUM_NOTICES:
                                                    
      iconn = &conns[1 + blocker->step->session];
      if (iconn->total_notices < blocker->target_notices)
      {
        return true;
      }
      break;
    }
  }
  return false;
}

static void
printResultSet(PGresult *res)
{
  PQprintOpt popt;

  memset(&popt, 0, sizeof(popt));
  popt.header = true;
  popt.align = true;
  popt.fieldSep = "|";
  PQprint(stdout, res, &popt);
}

                                                
static void
isotesterNoticeProcessor(void *arg, const char *message)
{
  IsoConnInfo *myconn = (IsoConnInfo *)arg;

                                                           
  printf("%s: %s", myconn->sessionname, message);
                                                                           
  myconn->total_notices++;
  any_new_notice = true;
}

                                         
static void
blackholeNoticeProcessor(void *arg, const char *message)
{
                  
}
