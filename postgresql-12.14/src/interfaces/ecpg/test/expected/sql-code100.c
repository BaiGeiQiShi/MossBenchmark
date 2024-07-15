                                         
                                                       
#include <ecpglib.h>
#include <ecpgerrno.h>
#include <sqlca.h>
                                      
#define ECPGdebug(X, Y) ECPGdebug((X) + 100, (Y))

#line 1 "code100.pgc"

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

#line 1 "code100.pgc"

#include <stdio.h>

#line 1 "regression.h"

#line 4 "code100.pgc"

int
main()
{                                     

#line 9 "code100.pgc"
  int index;
                                  
#line 10 "code100.pgc"

  ECPGdebug(1, stderr);

  {
    ECPGconnect(__LINE__, 0, "ecpg1_regression", NULL, NULL, NULL, 0);
  }
#line 15 "code100.pgc"

  if (sqlca.sqlcode)
  {
    printf("%ld:%s\n", sqlca.sqlcode, sqlca.sqlerrm.sqlerrmc);
  }

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "create table test ( \"index\" numeric ( 3 ) primary key , \"payload\" int4 not null )", ECPGt_EOIT, ECPGt_EORT);
  }
#line 20 "code100.pgc"

  if (sqlca.sqlcode)
  {
    printf("%ld:%s\n", sqlca.sqlcode, sqlca.sqlerrm.sqlerrmc);
  }
  {
    ECPGtrans(__LINE__, NULL, "commit work");
  }
#line 22 "code100.pgc"

  if (sqlca.sqlcode)
  {
    printf("%ld:%s\n", sqlca.sqlcode, sqlca.sqlerrm.sqlerrmc);
  }

  for (index = 0; index < 10; ++index)
  {
    {
      ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "insert into test ( payload , index ) values ( 0 , $1  )", ECPGt_int, &(index), (long)1, (long)1, sizeof(int), ECPGt_NO_INDICATOR, NULL, 0L, 0L, 0L, ECPGt_EOIT, ECPGt_EORT);
    }
#line 28 "code100.pgc"

    if (sqlca.sqlcode)
    {
      printf("%ld:%s\n", sqlca.sqlcode, sqlca.sqlerrm.sqlerrmc);
    }
  }
  {
    ECPGtrans(__LINE__, NULL, "commit work");
  }
#line 31 "code100.pgc"

  if (sqlca.sqlcode)
  {
    printf("%ld:%s\n", sqlca.sqlcode, sqlca.sqlerrm.sqlerrmc);
  }

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "update test set payload = payload + 1 where index = - 1", ECPGt_EOIT, ECPGt_EORT);
  }
#line 35 "code100.pgc"

  if (sqlca.sqlcode != 100)
  {
    printf("%ld:%s\n", sqlca.sqlcode, sqlca.sqlerrm.sqlerrmc);
  }

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "delete from test where index = - 1", ECPGt_EOIT, ECPGt_EORT);
  }
#line 38 "code100.pgc"

  if (sqlca.sqlcode != 100)
  {
    printf("%ld:%s\n", sqlca.sqlcode, sqlca.sqlerrm.sqlerrmc);
  }

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "insert into test ( select * from test where index = - 1 )", ECPGt_EOIT, ECPGt_EORT);
  }
#line 41 "code100.pgc"

  if (sqlca.sqlcode != 100)
  {
    printf("%ld:%s\n", sqlca.sqlcode, sqlca.sqlerrm.sqlerrmc);
  }

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "drop table test", ECPGt_EOIT, ECPGt_EORT);
  }
#line 44 "code100.pgc"

  if (sqlca.sqlcode)
  {
    printf("%ld:%s\n", sqlca.sqlcode, sqlca.sqlerrm.sqlerrmc);
  }
  {
    ECPGtrans(__LINE__, NULL, "commit work");
  }
#line 46 "code100.pgc"

  if (sqlca.sqlcode)
  {
    printf("%ld:%s\n", sqlca.sqlcode, sqlca.sqlerrm.sqlerrmc);
  }

  {
    ECPGdisconnect(__LINE__, "CURRENT");
  }
#line 49 "code100.pgc"

  if (sqlca.sqlcode)
  {
    printf("%ld:%s\n", sqlca.sqlcode, sqlca.sqlerrm.sqlerrmc);
  }
  return 0;
}
