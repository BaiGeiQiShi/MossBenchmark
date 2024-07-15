                                         
                                                       
#include <ecpglib.h>
#include <ecpgerrno.h>
#include <sqlca.h>
                                      
#define ECPGdebug(X, Y) ECPGdebug((X) + 100, (Y))

#line 1 "copystdout.pgc"
#include <stdio.h>

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

#line 3 "copystdout.pgc"

#line 1 "regression.h"

#line 4 "copystdout.pgc"

                                            
#line 6 "copystdout.pgc"

int
main()
{
  ECPGdebug(1, stderr);

  {
    ECPGconnect(__LINE__, 0, "ecpg1_regression", NULL, NULL, NULL, 0);
#line 13 "copystdout.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 13 "copystdout.pgc"

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "create table foo ( a int , b varchar )", ECPGt_EOIT, ECPGt_EORT);
#line 14 "copystdout.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 14 "copystdout.pgc"

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "insert into foo values ( 5 , 'abc' )", ECPGt_EOIT, ECPGt_EORT);
#line 15 "copystdout.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 15 "copystdout.pgc"

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "insert into foo values ( 6 , 'def' )", ECPGt_EOIT, ECPGt_EORT);
#line 16 "copystdout.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 16 "copystdout.pgc"

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "insert into foo values ( 7 , 'ghi' )", ECPGt_EOIT, ECPGt_EORT);
#line 17 "copystdout.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 17 "copystdout.pgc"

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "copy foo to stdout with delimiter ','", ECPGt_EOIT, ECPGt_EORT);
#line 19 "copystdout.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 19 "copystdout.pgc"

  printf("copy to STDOUT : sqlca.sqlcode = %ld\n", sqlca.sqlcode);

  {
    ECPGdisconnect(__LINE__, "CURRENT");
#line 22 "copystdout.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlprint();
    }
  }
#line 22 "copystdout.pgc"

  return 0;
}
