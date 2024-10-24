                                         
                                                       
#include <ecpglib.h>
#include <ecpgerrno.h>
#include <sqlca.h>
                                      
#define ECPGdebug(X, Y) ECPGdebug((X) + 100, (Y))

#line 1 "define.pgc"

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

#line 1 "define.pgc"

#line 1 "regression.h"

#line 2 "define.pgc"

int
main(void)
{
                                      

#line 10 "define.pgc"
  int i;

#line 11 "define.pgc"
  char s[200];
                                  
#line 12 "define.pgc"

  ECPGdebug(1, stderr);

                                                     
#line 16 "define.pgc"

  {
    ECPGconnect(__LINE__, 0, "ecpg1_regression", NULL, NULL, NULL, 0);
#line 17 "define.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 17 "define.pgc"

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "create table test ( a int , b text )", ECPGt_EOIT, ECPGt_EORT);
#line 19 "define.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 19 "define.pgc"

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "insert into test values ( 29 , 'abcdef' )", ECPGt_EOIT, ECPGt_EORT);
#line 20 "define.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 20 "define.pgc"

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "insert into test values ( null , 'defined' )", ECPGt_EOIT, ECPGt_EORT);
#line 23 "define.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 23 "define.pgc"

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "insert into test values ( null , 'someothervar not defined' )", ECPGt_EOIT, ECPGt_EORT);
#line 31 "define.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 31 "define.pgc"

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select 1 , 29 :: text || '-' || 'abcdef'", ECPGt_EOIT, ECPGt_int, &(i), (long)1, (long)1, sizeof(int), ECPGt_NO_INDICATOR, NULL, 0L, 0L, 0L, ECPGt_char, (s), (long)200, (long)1, (200) * sizeof(char), ECPGt_NO_INDICATOR, NULL, 0L, 0L, 0L, ECPGt_EORT);
#line 36 "define.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 36 "define.pgc"

  printf("i: %d, s: %s\n", i, s);

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "insert into test values ( 29 , 'no string' )", ECPGt_EOIT, ECPGt_EORT);
#line 42 "define.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 42 "define.pgc"

                

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "set TIMEZONE to 'UTC'", ECPGt_EOIT, ECPGt_EORT);
#line 53 "define.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 53 "define.pgc"

  {
    ECPGdisconnect(__LINE__, "CURRENT");
#line 56 "define.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 56 "define.pgc"

  return 0;
}
