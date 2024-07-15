                                         
                                                       
#include <ecpglib.h>
#include <ecpgerrno.h>
#include <sqlca.h>
                                       
#include <ecpg_informix.h>
                                      
#define ECPGdebug(X, Y) ECPGdebug((X) + 100, (Y))

#line 1 "rfmtlong.pgc"
#include <stdio.h>
#include <stdlib.h>
#include <pgtypes_error.h>
#include <sqltypes.h>

   
                                                                  
            
   

static void
check_return(int ret);

static void
fmtlong(long lng, const char *fmt)
{
  static int i;
  int r;
  char buf[30];

  r = rfmtlong(lng, fmt, buf);
  printf("r: %d ", r);
  if (r == 0)
  {
    printf("%d: %s (fmt was: %s)\n", i++, buf, fmt);
  }
  else
  {
    check_return(r);
  }
}

int
main(void)
{
  ECPGdebug(1, stderr);

  fmtlong(-8494493, "-<<<<,<<<,<<<,<<<");
  fmtlong(-8494493, "################");
  fmtlong(-8494493, "+++$$$$$$$$$$$$$.##");
  fmtlong(-8494493, "(&,&&&,&&&,&&&.)");
  fmtlong(-8494493, "<<<<,<<<,<<<,<<<");
  fmtlong(-8494493, "$************.**");
  fmtlong(-8494493, "---$************.**");
  fmtlong(-8494493, "+-+################");
  fmtlong(-8494493, "abc: ################+-+");
  fmtlong(-8494493, "+<<<<,<<<,<<<,<<<");

  return 0;
}

static void
check_return(int ret)
{
  switch (ret)
  {
  case ECPG_INFORMIX_ENOTDMY:
    printf("(ECPG_INFORMIX_ENOTDMY)");
    break;
  case ECPG_INFORMIX_ENOSHORTDATE:
    printf("(ECPG_INFORMIX_ENOSHORTDATE)");
    break;
  case ECPG_INFORMIX_BAD_DAY:
    printf("(ECPG_INFORMIX_BAD_DAY)");
    break;
  case ECPG_INFORMIX_BAD_MONTH:
    printf("(ECPG_INFORMIX_BAD_MONTH)");
    break;
  default:
    printf("(unknown ret: %d)", ret);
    break;
  }
  printf("\n");
}
