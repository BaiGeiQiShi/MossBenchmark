                                         
                                                       
#include <ecpglib.h>
#include <ecpgerrno.h>
#include <sqlca.h>
                                       
#include <ecpg_informix.h>
                                      
#define ECPGdebug(X, Y) ECPGdebug((X) + 100, (Y))

#line 1 "test_informix2.pgc"
#include <stdio.h>
#include <stdlib.h>
#include "sqltypes.h"

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

#line 5 "test_informix2.pgc"

#line 1 "regression.h"

#line 6 "test_informix2.pgc"

                                                                  
static void
sql_check(const char *fn, const char *caller, int ignore)
{
  char errorstring[255];

  if (SQLCODE == ignore)
  {
    return;
  }
  else
  {
    if (SQLCODE != 0)
    {

      sprintf(errorstring, "**SQL error %ld doing '%s' in function '%s'. [%s]", SQLCODE, caller, fn, sqlca.sqlerrm.sqlerrmc);
      fprintf(stderr, "%s", errorstring);
      printf("%s\n", errorstring);

                              
      {
        ECPGtrans(__LINE__, NULL, "rollback");
      }
#line 27 "test_informix2.pgc"

      if (SQLCODE == 0)
      {
        sprintf(errorstring, "Rollback successful.\n");
      }
      else
      {
        sprintf(errorstring, "Rollback failed with code %ld.\n", SQLCODE);
      }

      fprintf(stderr, "%s", errorstring);
      printf("%s\n", errorstring);

      exit(1);
    }
  }
}

int
main(void)
{
                                      

#line 47 "test_informix2.pgc"
  int c;

#line 48 "test_informix2.pgc"
  timestamp d;

#line 49 "test_informix2.pgc"
  timestamp e;

#line 50 "test_informix2.pgc"
  timestamp maxd;

#line 51 "test_informix2.pgc"
  char dbname[30];
                                  
#line 52 "test_informix2.pgc"

  interval *intvl;

                                          
#line 56 "test_informix2.pgc"

  ECPGdebug(1, stderr);

  strcpy(dbname, "ecpg1_regression");
  {
    ECPGconnect(__LINE__, 1, dbname, NULL, NULL, NULL, 0);
#line 61 "test_informix2.pgc"

    if (sqlca.sqlcode < 0)
    {
      exit(1);
    }
  }
#line 61 "test_informix2.pgc"

  sql_check("main", "connect", 0);

  {
    ECPGdo(__LINE__, 1, 1, NULL, 0, ECPGst_normal, "set DateStyle to 'DMY'", ECPGt_EOIT, ECPGt_EORT);
#line 64 "test_informix2.pgc"

    if (sqlca.sqlcode < 0)
    {
      exit(1);
    }
  }
#line 64 "test_informix2.pgc"

  {
    ECPGdo(__LINE__, 1, 1, NULL, 0, ECPGst_normal, "create table history ( customerid integer , timestamp timestamp without time zone , action_taken char ( 5 ) , narrative varchar ( 100 ) )", ECPGt_EOIT, ECPGt_EORT);
#line 66 "test_informix2.pgc"

    if (sqlca.sqlcode < 0)
    {
      exit(1);
    }
  }
#line 66 "test_informix2.pgc"

  sql_check("main", "create", 0);

  {
    ECPGdo(__LINE__, 1, 1, NULL, 0, ECPGst_normal, "insert into history ( customerid , timestamp , action_taken , narrative ) values ( 1 , '2003-05-07 13:28:34 CEST' , 'test' , 'test' )", ECPGt_EOIT, ECPGt_EORT);
#line 71 "test_informix2.pgc"

    if (sqlca.sqlcode < 0)
    {
      exit(1);
    }
  }
#line 71 "test_informix2.pgc"

  sql_check("main", "insert", 0);

  {
    ECPGdo(__LINE__, 1, 1, NULL, 0, ECPGst_normal, "select max ( timestamp ) from history", ECPGt_EOIT, ECPGt_timestamp, &(maxd), (long)1, (long)1, sizeof(timestamp), ECPGt_NO_INDICATOR, NULL, 0L, 0L, 0L, ECPGt_EORT);
#line 76 "test_informix2.pgc"

    if (sqlca.sqlcode < 0)
    {
      exit(1);
    }
  }
#line 76 "test_informix2.pgc"

  sql_check("main", "select max", 100);

  {
    ECPGdo(__LINE__, 1, 1, NULL, 0, ECPGst_normal, "select customerid , timestamp from history where timestamp = $1  limit 1", ECPGt_timestamp, &(maxd), (long)1, (long)1, sizeof(timestamp), ECPGt_NO_INDICATOR, NULL, 0L, 0L, 0L, ECPGt_EOIT, ECPGt_int, &(c), (long)1, (long)1, sizeof(int), ECPGt_NO_INDICATOR, NULL, 0L, 0L, 0L, ECPGt_timestamp, &(d), (long)1, (long)1, sizeof(timestamp), ECPGt_NO_INDICATOR, NULL, 0L, 0L, 0L, ECPGt_EORT);
#line 83 "test_informix2.pgc"

    if (sqlca.sqlcode < 0)
    {
      exit(1);
    }
  }
#line 83 "test_informix2.pgc"

  sql_check("main", "select", 0);

  printf("Read in customer %d\n", c);

  intvl = PGTYPESinterval_from_asc("1 day 2 hours 24 minutes 65 seconds", NULL);
  PGTYPEStimestamp_add_interval(&d, intvl, &e);
  free(intvl);
  c++;

  {
    ECPGdo(__LINE__, 1, 1, NULL, 0, ECPGst_normal, "insert into history ( customerid , timestamp , action_taken , narrative ) values ( $1  , $2  , 'test' , 'test' )", ECPGt_int, &(c), (long)1, (long)1, sizeof(int), ECPGt_NO_INDICATOR, NULL, 0L, 0L, 0L, ECPGt_timestamp, &(e), (long)1, (long)1, sizeof(timestamp), ECPGt_NO_INDICATOR, NULL, 0L, 0L, 0L, ECPGt_EOIT, ECPGt_EORT);
#line 95 "test_informix2.pgc"

    if (sqlca.sqlcode < 0)
    {
      exit(1);
    }
  }
#line 95 "test_informix2.pgc"

  sql_check("main", "update", 0);

  {
    ECPGtrans(__LINE__, NULL, "commit");
#line 98 "test_informix2.pgc"

    if (sqlca.sqlcode < 0)
    {
      exit(1);
    }
  }
#line 98 "test_informix2.pgc"

  {
    ECPGdo(__LINE__, 1, 1, NULL, 0, ECPGst_normal, "drop table history", ECPGt_EOIT, ECPGt_EORT);
#line 100 "test_informix2.pgc"

    if (sqlca.sqlcode < 0)
    {
      exit(1);
    }
  }
#line 100 "test_informix2.pgc"

  sql_check("main", "drop", 0);

  {
    ECPGtrans(__LINE__, NULL, "commit");
#line 103 "test_informix2.pgc"

    if (sqlca.sqlcode < 0)
    {
      exit(1);
    }
  }
#line 103 "test_informix2.pgc"

  {
    ECPGdisconnect(__LINE__, "CURRENT");
#line 105 "test_informix2.pgc"

    if (sqlca.sqlcode < 0)
    {
      exit(1);
    }
  }
#line 105 "test_informix2.pgc"

  sql_check("main", "disconnect", 0);

  printf("All OK!\n");

  exit(0);

     
                                          
                                                         
                                                          
                                                         
                                                         
                                                         
                                                
    
}
