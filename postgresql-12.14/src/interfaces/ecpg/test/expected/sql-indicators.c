                                         
                                                       
#include <ecpglib.h>
#include <ecpgerrno.h>
#include <sqlca.h>
                                      
#define ECPGdebug(X, Y) ECPGdebug((X) + 100, (Y))

#line 1 "indicators.pgc"
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

#line 3 "indicators.pgc"

#line 1 "regression.h"

#line 4 "indicators.pgc"

int
main()
{
                                      

#line 9 "indicators.pgc"
  int intvar = 5;

#line 10 "indicators.pgc"
  int nullind = -1;
                                  
#line 11 "indicators.pgc"

  ECPGdebug(1, stderr);

  {
    ECPGconnect(__LINE__, 0, "ecpg1_regression", NULL, NULL, NULL, 0);
  }
#line 15 "indicators.pgc"

  {
    ECPGsetcommit(__LINE__, "off", NULL);
  }
#line 16 "indicators.pgc"

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "create table indicator_test ( \"id\" int primary key , \"str\" text not null , val int null )", ECPGt_EOIT, ECPGt_EORT);
  }
#line 21 "indicators.pgc"

  {
    ECPGtrans(__LINE__, NULL, "commit work");
  }
#line 22 "indicators.pgc"

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "insert into indicator_test ( id , str , val ) values ( 1 , 'Hello' , 0 )", ECPGt_EOIT, ECPGt_EORT);
  }
#line 24 "indicators.pgc"

                               
  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "insert into indicator_test ( id , str , val ) values ( 2 , 'Hi there' , $1  )", ECPGt_int, &(intvar), (long)1, (long)1, sizeof(int), ECPGt_int, &(nullind), (long)1, (long)1, sizeof(int), ECPGt_EOIT, ECPGt_EORT);
  }
#line 27 "indicators.pgc"

  nullind = 0;
  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "insert into indicator_test ( id , str , val ) values ( 3 , 'Good evening' , $1  )", ECPGt_int, &(intvar), (long)1, (long)1, sizeof(int), ECPGt_int, &(nullind), (long)1, (long)1, sizeof(int), ECPGt_EOIT, ECPGt_EORT);
  }
#line 29 "indicators.pgc"

  {
    ECPGtrans(__LINE__, NULL, "commit work");
  }
#line 30 "indicators.pgc"

                                                       
  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select val from indicator_test where id = 1", ECPGt_EOIT, ECPGt_int, &(intvar), (long)1, (long)1, sizeof(int), ECPGt_NO_INDICATOR, NULL, 0L, 0L, 0L, ECPGt_EORT);
  }
#line 33 "indicators.pgc"

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select val from indicator_test where id = 2", ECPGt_EOIT, ECPGt_int, &(intvar), (long)1, (long)1, sizeof(int), ECPGt_int, &(nullind), (long)1, (long)1, sizeof(int), ECPGt_EORT);
  }
#line 34 "indicators.pgc"

  printf("intvar: %d, nullind: %d\n", intvar, nullind);
  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select val from indicator_test where id = 3", ECPGt_EOIT, ECPGt_int, &(intvar), (long)1, (long)1, sizeof(int), ECPGt_int, &(nullind), (long)1, (long)1, sizeof(int), ECPGt_EORT);
  }
#line 36 "indicators.pgc"

  printf("intvar: %d, nullind: %d\n", intvar, nullind);

                                 
  intvar = 5;
  nullind = -1;
  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "update indicator_test set val = $1  where id = 1", ECPGt_int, &(intvar), (long)1, (long)1, sizeof(int), ECPGt_int, &(nullind), (long)1, (long)1, sizeof(int), ECPGt_EOIT, ECPGt_EORT);
  }
#line 41 "indicators.pgc"

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select val from indicator_test where id = 1", ECPGt_EOIT, ECPGt_int, &(intvar), (long)1, (long)1, sizeof(int), ECPGt_int, &(nullind), (long)1, (long)1, sizeof(int), ECPGt_EORT);
  }
#line 42 "indicators.pgc"

  printf("intvar: %d, nullind: %d\n", intvar, nullind);

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "drop table indicator_test", ECPGt_EOIT, ECPGt_EORT);
  }
#line 45 "indicators.pgc"

  {
    ECPGtrans(__LINE__, NULL, "commit work");
  }
#line 46 "indicators.pgc"

  {
    ECPGdisconnect(__LINE__, "CURRENT");
  }
#line 48 "indicators.pgc"

  return 0;
}
