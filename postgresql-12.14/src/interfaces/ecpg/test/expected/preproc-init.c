                                         
                                                       
#include <ecpglib.h>
#include <ecpgerrno.h>
#include <sqlca.h>
                                      
#define ECPGdebug(X, Y) ECPGdebug((X) + 100, (Y))

#line 1 "init.pgc"

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

#line 1 "init.pgc"

enum e
{
  ENUM0,
  ENUM1
};
struct sa
{
  int member;
};

static int
fa(void)
{
  printf("in fa\n");
  return 2;
}

static int
fb(int x)
{
  printf("in fb (%d)\n", x);
  return x;
}

static int
fc(const char *x)
{
  printf("in fc (%s)\n", x);
  return *x;
}

static int
fd(const char *x, int i)
{
  printf("in fd (%s, %d)\n", x, i);
  return (*x) * i;
}

static int
fe(enum e x)
{
  printf("in fe (%d)\n", (int)x);
  return (int)x;
}

static void
sqlnotice(const char *notice, short trans)
{
  if (!notice)
  {
    notice = "-empty-";
  }
  printf("in sqlnotice (%s, %d)\n", notice, trans);
}

#define YES 1

#ifdef _cplusplus
namespace N
{
static const int i = 2;
};
#endif

int
main(void)
{
  struct sa x = {14}, *y = &x;
                                      

            

#line 60 "init.pgc"
  int a = (int)2;

#line 61 "init.pgc"
  int b = 2 + 2;

#line 62 "init.pgc"
  int b2 = (14 * 7);

#line 63 "init.pgc"
  int d = x.member;

#line 64 "init.pgc"
  int g = fb(2);

#line 65 "init.pgc"
  int i = 3 ^ 1;

#line 66 "init.pgc"
  int j = 1 ? 1 : 2;

#line 68 "init.pgc"
  int e = y->member;

#line 69 "init.pgc"
  int c = 10 >> 2;

#line 70 "init.pgc"
  bool h = 2 || 1;

#line 71 "init.pgc"
  long iay;
                                  
#line 72 "init.pgc"

  int f = fa();

#ifdef _cplusplus
                                      
                     

#line 78 "init.pgc"
  int k = N : : i;
                                  
#line 79 "init.pgc"

#endif

  ECPGdebug(1, stderr);

  printf("%d %d %d %d %d %d %d %d %d %d %d\n", a, b, b2, c, d, e, f, g, h, i, j);
  iay = 0;
  printf("%ld\n", iay);
                                               
#line 87 "init.pgc"

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select now ( )", ECPGt_EOIT, ECPGt_EORT);
#line 88 "init.pgc"

    if (sqlca.sqlcode < 0)
    {
      fa();
    }
  }
#line 88 "init.pgc"

                                                  
#line 89 "init.pgc"

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select now ( )", ECPGt_EOIT, ECPGt_EORT);
#line 90 "init.pgc"

    if (sqlca.sqlcode < 0)
    {
      fb(20);
    }
  }
#line 90 "init.pgc"

                                                      
#line 91 "init.pgc"

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select now ( )", ECPGt_EOIT, ECPGt_EORT);
#line 92 "init.pgc"

    if (sqlca.sqlcode < 0)
    {
      fc("50");
    }
  }
#line 92 "init.pgc"

                                                          
#line 93 "init.pgc"

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select now ( )", ECPGt_EOIT, ECPGt_EORT);
#line 94 "init.pgc"

    if (sqlca.sqlcode < 0)
    {
      fd("50", 1);
    }
  }
#line 94 "init.pgc"

                                                     
#line 95 "init.pgc"

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select now ( )", ECPGt_EOIT, ECPGt_EORT);
#line 96 "init.pgc"

    if (sqlca.sqlcode < 0)
    {
      fe(ENUM0);
    }
  }
#line 96 "init.pgc"

                                                               
#line 97 "init.pgc"

  {
    ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select now ( )", ECPGt_EOIT, ECPGt_EORT);
#line 98 "init.pgc"

    if (sqlca.sqlcode < 0)
    {
      sqlnotice(NULL, 0);
    }
  }
#line 98 "init.pgc"

  return 0;
}
