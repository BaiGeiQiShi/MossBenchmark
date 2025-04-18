                                         
                                                       
#include <ecpglib.h>
#include <ecpgerrno.h>
#include <sqlca.h>
                                      
#define ECPGdebug(X, Y) ECPGdebug((X) + 100, (Y))

#line 1 "num_test2.pgc"
#include <stdio.h>
#include <stdlib.h>
#include <pgtypes_numeric.h>
#include <pgtypes_error.h>
#include <decimal.h>

#line 1 "regression.h"

#line 7 "num_test2.pgc"

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

#line 9 "num_test2.pgc"

char *nums[] = {"2E394", "-2", ".794", "3.44", "592.49E21", "-32.84e4", "2E-394", ".1E-2", "+.0", "-592.49E-07", "+32.84e-4", ".500001", "-.5000001", "1234567890123456789012345678.91",                         
                                                                                                                                                                                                           
    "1234567890123456789012345678.921",                                                                                                                                                                          
                                                                                                                                                                                                               
    "not a number", NULL};

static void
check_errno(void);

int
main(void)
{
  char *text = "error\n";
  char *endptr;
  numeric *num, *nin;
  decimal *dec;
  long l;
  int i, j, k, q, r, count = 0;
  double d;
  numeric **numarr = (numeric **)calloc(1, sizeof(numeric));

  ECPGdebug(1, stderr);

  for (i = 0; nums[i]; i++)
  {
    num = PGTYPESnumeric_from_asc(nums[i], &endptr);
    if (!num)
    {
      check_errno();
    }
    if (endptr != NULL)
    {
      printf("endptr of %d is not NULL\n", i);
      if (*endptr != '\0')
      {
        printf("*endptr of %d is not \\0\n", i);
      }
    }
    if (!num)
    {
      continue;
    }

    numarr = realloc(numarr, sizeof(numeric *) * (count + 1));
    numarr[count++] = num;

    text = PGTYPESnumeric_to_asc(num, -1);
    if (!text)
    {
      check_errno();
    }
    printf("num[%d,1]: %s\n", i, text);
    PGTYPESchar_free(text);
    text = PGTYPESnumeric_to_asc(num, 0);
    if (!text)
    {
      check_errno();
    }
    printf("num[%d,2]: %s\n", i, text);
    PGTYPESchar_free(text);
    text = PGTYPESnumeric_to_asc(num, 1);
    if (!text)
    {
      check_errno();
    }
    printf("num[%d,3]: %s\n", i, text);
    PGTYPESchar_free(text);
    text = PGTYPESnumeric_to_asc(num, 2);
    if (!text)
    {
      check_errno();
    }
    printf("num[%d,4]: %s\n", i, text);
    PGTYPESchar_free(text);

    nin = PGTYPESnumeric_new();
    text = PGTYPESnumeric_to_asc(nin, 2);
    if (!text)
    {
      check_errno();
    }
    printf("num[%d,5]: %s\n", i, text);
    PGTYPESchar_free(text);

    r = PGTYPESnumeric_to_long(num, &l);
    if (r)
    {
      check_errno();
    }
    printf("num[%d,6]: %ld (r: %d)\n", i, r ? 0L : l, r);
    if (r == 0)
    {
      r = PGTYPESnumeric_from_long(l, nin);
      if (r)
      {
        check_errno();
      }
      text = PGTYPESnumeric_to_asc(nin, 2);
      q = PGTYPESnumeric_cmp(num, nin);
      printf("num[%d,7]: %s (r: %d - cmp: %d)\n", i, text, r, q);
      PGTYPESchar_free(text);
    }

    r = PGTYPESnumeric_to_int(num, &k);
    if (r)
    {
      check_errno();
    }
    printf("num[%d,8]: %d (r: %d)\n", i, r ? 0 : k, r);
    if (r == 0)
    {
      r = PGTYPESnumeric_from_int(k, nin);
      if (r)
      {
        check_errno();
      }
      text = PGTYPESnumeric_to_asc(nin, 2);
      q = PGTYPESnumeric_cmp(num, nin);
      printf("num[%d,9]: %s (r: %d - cmp: %d)\n", i, text, r, q);
      PGTYPESchar_free(text);
    }

    if (i != 6)
    {
                                                                                     
                                                              

      r = PGTYPESnumeric_to_double(num, &d);
      if (r)
      {
        check_errno();
      }
      printf("num[%d,10]: ", i);
      print_double(r ? 0.0 : d);
      printf(" (r: %d)\n", r);
    }

                                             
                                                               
                                                                                    
       

    dec = PGTYPESdecimal_new();
    r = PGTYPESnumeric_to_decimal(num, dec);
    if (r)
    {
      check_errno();
    }
                                                                   
                                     
    printf("num[%d,11]: - (r: %d)\n", i, r);
    if (r == 0)
    {
      r = PGTYPESnumeric_from_decimal(dec, nin);
      if (r)
      {
        check_errno();
      }
      text = PGTYPESnumeric_to_asc(nin, 2);
      q = PGTYPESnumeric_cmp(num, nin);
      printf("num[%d,12]: %s (r: %d - cmp: %d)\n", i, text, r, q);
      PGTYPESchar_free(text);
    }

    PGTYPESdecimal_free(dec);
    PGTYPESnumeric_free(nin);
    printf("\n");
  }

  for (i = 0; i < count; i++)
  {
    for (j = 0; j < count; j++)
    {
      numeric *a = PGTYPESnumeric_new();
      numeric *s = PGTYPESnumeric_new();
      numeric *m = PGTYPESnumeric_new();
      numeric *d = PGTYPESnumeric_new();
      r = PGTYPESnumeric_add(numarr[i], numarr[j], a);
      if (r)
      {
        check_errno();
        printf("r: %d\n", r);
      }
      else
      {
        text = PGTYPESnumeric_to_asc(a, 10);
        printf("num[a,%d,%d]: %s\n", i, j, text);
        PGTYPESchar_free(text);
      }
      r = PGTYPESnumeric_sub(numarr[i], numarr[j], s);
      if (r)
      {
        check_errno();
        printf("r: %d\n", r);
      }
      else
      {
        text = PGTYPESnumeric_to_asc(s, 10);
        printf("num[s,%d,%d]: %s\n", i, j, text);
        PGTYPESchar_free(text);
      }
      r = PGTYPESnumeric_mul(numarr[i], numarr[j], m);
      if (r)
      {
        check_errno();
        printf("r: %d\n", r);
      }
      else
      {
        text = PGTYPESnumeric_to_asc(m, 10);
        printf("num[m,%d,%d]: %s\n", i, j, text);
        PGTYPESchar_free(text);
      }
      r = PGTYPESnumeric_div(numarr[i], numarr[j], d);
      if (r)
      {
        check_errno();
        printf("r: %d\n", r);
      }
      else
      {
        text = PGTYPESnumeric_to_asc(d, 10);
        printf("num[d,%d,%d]: %s\n", i, j, text);
        PGTYPESchar_free(text);
      }

      PGTYPESnumeric_free(a);
      PGTYPESnumeric_free(s);
      PGTYPESnumeric_free(m);
      PGTYPESnumeric_free(d);
    }
  }

  for (i = 0; i < count; i++)
  {
    text = PGTYPESnumeric_to_asc(numarr[i], -1);
    printf("%d: %s\n", i, text);
    PGTYPESchar_free(text);
    PGTYPESnumeric_free(numarr[i]);
  }
  free(numarr);

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
