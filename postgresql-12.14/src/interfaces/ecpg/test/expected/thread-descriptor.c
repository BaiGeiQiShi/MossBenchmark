                                         
                                                       
#include <ecpglib.h>
#include <ecpgerrno.h>
#include <sqlca.h>
                                      
#define ECPGdebug(X, Y) ECPGdebug((X) + 100, (Y))

#line 1 "descriptor.pgc"
#ifdef ENABLE_THREAD_SAFETY
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#include <locale.h>
#else
#include <pthread.h>
#endif
#endif
#include <stdio.h>

#define THREADS 16
#define REPEATS 50000

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

#line 16 "descriptor.pgc"

                                            
#line 17 "descriptor.pgc"

                                             
#line 18 "descriptor.pgc"

#if defined(ENABLE_THREAD_SAFETY) && defined(WIN32)
static unsigned __stdcall fn(void *arg)
#else
static void *
fn(void *arg)
#endif
{
  int i;

  for (i = 1; i <= REPEATS; ++i)
  {
    ECPGallocate_desc(__LINE__, "mydesc");
#line 30 "descriptor.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
#line 30 "descriptor.pgc"

    ECPGdeallocate_desc(__LINE__, "mydesc");
#line 31 "descriptor.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
#line 31 "descriptor.pgc"
  }

  return 0;
}

int
main()
{
#ifdef ENABLE_THREAD_SAFETY
  int i;
#ifdef WIN32
  HANDLE threads[THREADS];
#else
  pthread_t threads[THREADS];
#endif

#ifdef WIN32
  for (i = 0; i < THREADS; ++i)
  {
    unsigned id;
    threads[i] = (HANDLE)_beginthreadex(NULL, 0, fn, NULL, 0, &id);
  }

  WaitForMultipleObjects(THREADS, threads, TRUE, INFINITE);
  for (i = 0; i < THREADS; ++i)
  {
    CloseHandle(threads[i]);
  }
#else
  for (i = 0; i < THREADS; ++i)
  {
    pthread_create(&threads[i], NULL, fn, NULL);
  }
  for (i = 0; i < THREADS; ++i)
  {
    pthread_join(threads[i], NULL);
  }
#endif
#else
  fn(NULL);
#endif

  return 0;
}
