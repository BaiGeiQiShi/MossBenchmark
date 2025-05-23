                                         
                                                       
#include <ecpglib.h>
#include <ecpgerrno.h>
#include <sqlca.h>
                                       
#include <ecpg_informix.h>
                                      
#define ECPGdebug(X, Y) ECPGdebug((X) + 100, (Y))

#line 1 "dec_test.pgc"
#include <stdio.h>
#include <stdlib.h>
#include <pgtypes_numeric.h>
#include <pgtypes_error.h>
#include <decimal.h>
#include <sqltypes.h>

#line 1 "regression.h"

#line 8 "dec_test.pgc"

#line 1 "printf_hack.h"
   
                                                                           
                                                        
   
static void
print_double(double x)
{
#ifdef WIN32
                                                                      
  char convert[128];
  int vallen;

  sprintf(convert, "%g", x);
  vallen = strlen(convert);

  if (vallen >= 6 && convert[vallen - 5] == 'e' && convert[vallen - 3] == '0')
  {
    convert[vallen - 3] = convert[vallen - 2];
    convert[vallen - 2] = convert[vallen - 1];
    convert[vallen - 1] = '\0';
  }

  printf("%s", convert);
#else
  printf("%g", x);
#endif
}

#line 10 "dec_test.pgc"

   
      
                             
                                    
                                     
  

char *decs[] = {"2E394", "-2", ".794", "3.44", "592.49E21", "-32.84e4", "2E-394", ".1E-2", "+.0", "-592.49E-07", "+32.84e-4", ".500001", "-.5000001", "1234567890123456789012345678.91",                         
                                                                                                                                                                                                           
    "1234567890123456789012345678.921",                                                                                                                                                                          
                                                                                                                                                                                                               
    "not a number", NULL};

static void
check_errno(void);

#define BUFSIZE 200

int
main(void)
{
  decimal *dec, *din;
  char buf[BUFSIZE];
  long l;
  int i, j, k, q, r, count = 0;
  double dbl;
  decimal **decarr = (decimal **)calloc(1, sizeof(decimal));

  ECPGdebug(1, stderr);

  for (i = 0; decs[i]; i++)
  {
    dec = PGTYPESdecimal_new();
    r = deccvasc(decs[i], strlen(decs[i]), dec);
    if (r)
    {
      check_errno();
      printf("dec[%d,0]: r: %d\n", i, r);
      PGTYPESdecimal_free(dec);
      continue;
    }
    decarr = realloc(decarr, sizeof(decimal *) * (count + 1));
    decarr[count++] = dec;

    r = dectoasc(dec, buf, BUFSIZE - 1, -1);
    if (r < 0)
    {
      check_errno();
    }
    printf("dec[%d,1]: r: %d, %s\n", i, r, buf);

    r = dectoasc(dec, buf, BUFSIZE - 1, 0);
    if (r < 0)
    {
      check_errno();
    }
    printf("dec[%d,2]: r: %d, %s\n", i, r, buf);
    r = dectoasc(dec, buf, BUFSIZE - 1, 1);
    if (r < 0)
    {
      check_errno();
    }
    printf("dec[%d,3]: r: %d, %s\n", i, r, buf);
    r = dectoasc(dec, buf, BUFSIZE - 1, 2);
    if (r < 0)
    {
      check_errno();
    }
    printf("dec[%d,4]: r: %d, %s\n", i, r, buf);

    din = PGTYPESdecimal_new();
    r = dectoasc(din, buf, BUFSIZE - 1, 2);
    if (r < 0)
    {
      check_errno();
    }
    printf("dec[%d,5]: r: %d, %s\n", i, r, buf);

    r = dectolong(dec, &l);
    if (r)
    {
      check_errno();
    }
    printf("dec[%d,6]: %ld (r: %d)\n", i, r ? 0L : l, r);
    if (r == 0)
    {
      r = deccvlong(l, din);
      if (r)
      {
        check_errno();
      }
      dectoasc(din, buf, BUFSIZE - 1, 2);
      q = deccmp(dec, din);
      printf("dec[%d,7]: %s (r: %d - cmp: %d)\n", i, buf, r, q);
    }

    r = dectoint(dec, &k);
    if (r)
    {
      check_errno();
    }
    printf("dec[%d,8]: %d (r: %d)\n", i, r ? 0 : k, r);
    if (r == 0)
    {
      r = deccvint(k, din);
      if (r)
      {
        check_errno();
      }
      dectoasc(din, buf, BUFSIZE - 1, 2);
      q = deccmp(dec, din);
      printf("dec[%d,9]: %s (r: %d - cmp: %d)\n", i, buf, r, q);
    }

    if (i != 6)
    {
                                                                                     
                                                              
      r = dectodbl(dec, &dbl);
      if (r)
      {
        check_errno();
      }
      printf("dec[%d,10]: ", i);
      print_double(r ? 0.0 : dbl);
      printf(" (r: %d)\n", r);
    }

    PGTYPESdecimal_free(din);
    printf("\n");
  }

                        
  dec = PGTYPESdecimal_new();
  decarr = realloc(decarr, sizeof(decimal *) * (count + 1));
  decarr[count++] = dec;

  rsetnull(CDECIMALTYPE, (char *)decarr[count - 1]);
  printf("dec[%d]: %sNULL\n", count - 1, risnull(CDECIMALTYPE, (char *)decarr[count - 1]) ? "" : "NOT ");
  printf("dec[0]: %sNULL\n", risnull(CDECIMALTYPE, (char *)decarr[0]) ? "" : "NOT ");

  r = dectoasc(decarr[3], buf, -1, -1);
  check_errno();
  printf("dectoasc with len == -1: r: %d\n", r);
  r = dectoasc(decarr[3], buf, 0, -1);
  check_errno();
  printf("dectoasc with len == 0: r: %d\n", r);

  for (i = 0; i < count; i++)
  {
    for (j = 0; j < count; j++)
    {
      decimal a, s, m, d;
      int c;
      c = deccmp(decarr[i], decarr[j]);
      printf("dec[c,%d,%d]: %d\n", i, j, c);

      r = decadd(decarr[i], decarr[j], &a);
      if (r)
      {
        check_errno();
        printf("r: %d\n", r);
      }
      else
      {
        dectoasc(&a, buf, BUFSIZE - 1, -1);
        printf("dec[a,%d,%d]: %s\n", i, j, buf);
      }

      r = decsub(decarr[i], decarr[j], &s);
      if (r)
      {
        check_errno();
        printf("r: %d\n", r);
      }
      else
      {
        dectoasc(&s, buf, BUFSIZE - 1, -1);
        printf("dec[s,%d,%d]: %s\n", i, j, buf);
      }

      r = decmul(decarr[i], decarr[j], &m);
      if (r)
      {
        check_errno();
        printf("r: %d\n", r);
      }
      else
      {
        dectoasc(&m, buf, BUFSIZE - 1, -1);
        printf("dec[m,%d,%d]: %s\n", i, j, buf);
      }

      r = decdiv(decarr[i], decarr[j], &d);
      if (r)
      {
        check_errno();
        printf("r: %d\n", r);
      }
      else
      {
        dectoasc(&d, buf, BUFSIZE - 1, -1);
        printf("dec[d,%d,%d]: %s\n", i, j, buf);
      }
    }
  }

  for (i = 0; i < count; i++)
  {
    dectoasc(decarr[i], buf, BUFSIZE - 1, -1);
    printf("%d: %s\n", i, buf);

    PGTYPESdecimal_free(decarr[i]);
  }
  free(decarr);

  return 0;
}

static void
check_errno(void)
{
  switch (errno)
  {
  case 0:
    printf("(no errno set) - ");
    break;
  case ECPG_INFORMIX_NUM_OVERFLOW:
    printf("(errno == ECPG_INFORMIX_NUM_OVERFLOW) - ");
    break;
  case ECPG_INFORMIX_NUM_UNDERFLOW:
    printf("(errno == ECPG_INFORMIX_NUM_UNDERFLOW) - ");
    break;
  case PGTYPES_NUM_OVERFLOW:
    printf("(errno == PGTYPES_NUM_OVERFLOW) - ");
    break;
  case PGTYPES_NUM_UNDERFLOW:
    printf("(errno == PGTYPES_NUM_UNDERFLOW) - ");
    break;
  case PGTYPES_NUM_BAD_NUMERIC:
    printf("(errno == PGTYPES_NUM_BAD_NUMERIC) - ");
    break;
  case PGTYPES_NUM_DIVIDE_ZERO:
    printf("(errno == PGTYPES_NUM_DIVIDE_ZERO) - ");
    break;
  default:
    printf("(unknown errno (%d))\n", errno);
    printf("(libc: (%s)) ", strerror(errno));
    break;
  }
}
