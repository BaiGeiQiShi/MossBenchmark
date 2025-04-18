                                         
                                                       
#include <ecpglib.h>
#include <ecpgerrno.h>
#include <sqlca.h>
                                      
#define ECPGdebug(X, Y) ECPGdebug((X) + 100, (Y))

#line 1 "prep.pgc"
#include <stdlib.h>
#include "ecpg_config.h"

#ifndef ENABLE_THREAD_SAFETY
int
main(void)
{
  printf("No threading enabled.\n");
  return 0;
}
#else
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#include <locale.h>
#else
#include <pthread.h>
#endif
#include <stdio.h>

#define THREADS 16
#define REPEATS 50

#line 1 "sqlca.h"
#ifndef POSTGRES_SQLCA_H
#define POSTGRES_SQLCA_H

#ifndef PGDLLIMPORT
#if defined(WIN32) || defined(__CYGWIN__)
#define PGDLLIMPORT __declspec(dllimport)
#else
#define PGDLLIMPORT
#endif                 
#endif                  

#define SQLERRMC_LEN 150

#ifdef __cplusplus
extern "C"
{
#endif

  struct sqlca_t
  {
    char sqlcaid[8];
    long sqlabc;
    long sqlcode;
    struct
    {
      int sqlerrml;
      char sqlerrmc[SQLERRMC_LEN];
    } sqlerrm;
    char sqlerrp[8];
    long sqlerrd[6];
                               
                                                   
                                        
                                       
                              
                       
                       
                       
    char sqlwarn[8];
                                                            
                                                  
                                           
                                                  

       
                                                         
       	              
                       
                       
                       
                       

    char sqlstate[5];
  };

  struct sqlca_t *
  ECPGget_sqlca(void);

#ifndef POSTGRES_ECPG_INTERNAL
#define sqlca (*ECPGget_sqlca())
#endif

#ifdef __cplusplus
}
#endif

#endif

#line 25 "prep.pgc"

#line 1 "regression.h"

#line 26 "prep.pgc"

                                            
#line 28 "prep.pgc"

                                             
#line 29 "prep.pgc"

#ifdef WIN32
static unsigned __stdcall fn(void *arg)
#else
static void *
fn(void *arg)
#endif
{
  int i;

                                      

#line 40 "prep.pgc"
  int value;

#line 41 "prep.pgc"
  char name[100];

#line 42 "prep.pgc"
  char query[256] = "INSERT INTO T VALUES ( ? )";
                                    
#line 43 "prep.pgc"

  value = (long)arg;
  sprintf(name, "Connection: %d", value);

  {
    ECPGconnect(__LINE__, 0, "ecpg1_regression", NULL, NULL, name, 0);
#line 48 "prep.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 48 "prep.pgc"

  {
    ECPGsetcommit(__LINE__, "on", NULL);
#line 49 "prep.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 49 "prep.pgc"

  for (i = 1; i <= REPEATS; ++i)
  {
    {
      ECPGprepare(__LINE__, NULL, 0, "i", query);
#line 52 "prep.pgc"

      if (sqlca.sqlcode < 0)
      {
        sqlprint();
      }
    }
#line 52 "prep.pgc"

    {
      ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_execute, "i", ECPGt_int, &(value), (long)1, (long)1, sizeof(int), ECPGt_NO_INDICATOR, NULL, 0L, 0L, 0L, ECPGt_EOIT, ECPGt_EORT);
#line 53 "prep.pgc"

      if (sqlca.sqlcode == ECPG_NOT_FOUND)
      {
        sqlprint();
      }
#line 53 "prep.pgc"

      if (sqlca.sqlcode < 0)
      {
        sqlprint();
      }
    }
#line 53 "prep.pgc"
  }
  {
    ECPGdeallocate(__LINE__, 0, NULL, "i");
#line 55 "prep.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 55 "prep.pgc"

  {
    ECPGdisconnect(__LINE__, name);
#line 56 "prep.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 56 "prep.pgc"

  return 0;
}

int
main()
{
  int i;
#ifdef WIN32
  HANDLE threads[THREADS];
#else
  pthread_t threads[THREADS];
#endif

  {
    ECPGconnect(__LINE__, 0, "ecpg1_regression", NULL, NULL, NULL, 0);
#line 70 "prep.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 70 "prep.pgc"

  {
    ECPGsetcommit(__LINE__, "on", NULL);
#line 71 "prep.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 71 "prep.pgc"

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "drop table if exists T", ECPGt_EOIT, ECPGt_EORT);
#line 72 "prep.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 72 "prep.pgc"

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "create table T ( i int )", ECPGt_EOIT, ECPGt_EORT);
#line 73 "prep.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 73 "prep.pgc"

  {
    ECPGdisconnect(__LINE__, "CURRENT");
#line 74 "prep.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 74 "prep.pgc"

#ifdef WIN32
  for (i = 0; i < THREADS; ++i)
  {
    unsigned id;
    threads[i] = (HANDLE)_beginthreadex(NULL, 0, fn, (void *)i, 0, &id);
  }

  WaitForMultipleObjects(THREADS, threads, TRUE, INFINITE);
  for (i = 0; i < THREADS; ++i)
  {
    CloseHandle(threads[i]);
  }
#else
  for (i = 0; i < THREADS; ++i)
  {
    pthread_create(&threads[i], NULL, fn, (void *)(long)i);
  }
  for (i = 0; i < THREADS; ++i)
  {
    pthread_join(threads[i], NULL);
  }
#endif

  return 0;
}
#endif
