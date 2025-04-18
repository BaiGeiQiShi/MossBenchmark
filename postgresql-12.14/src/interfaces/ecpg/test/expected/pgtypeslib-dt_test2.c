                                         
                                                       
#include <ecpglib.h>
#include <ecpgerrno.h>
#include <sqlca.h>
                                      
#define ECPGdebug(X, Y) ECPGdebug((X) + 100, (Y))

#line 1 "dt_test2.pgc"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <pgtypes_date.h>
#include <pgtypes_timestamp.h>

#line 1 "regression.h"

#line 8 "dt_test2.pgc"

char *dates[] = {"19990108foobar", "19990108 foobar", "1999-01-08 foobar", "January 8, 1999", "1999-01-08", "1/8/1999", "1/18/1999", "01/02/03", "1999-Jan-08", "Jan-08-1999", "08-Jan-1999", "99-Jan-08", "08-Jan-99", "08-Jan-06", "Jan-08-99", "19990108", "990108", "1999.008", "J2451187", "January 8, 99 BC",
       
                                                       
                                                                 
       
    "........................Xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                   
    ".........................aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    NULL};

                                              
static char *times[] = {"0:04", "1:59 PDT", "13:24:40 -8:00", "13:24:40.495+3", "13:24:40.123456123+3", NULL};

char *intervals[] = {"1 minute", "1 12:59:10", "2 day 12 hour 59 minute 10 second", "1 days 12 hrs 59 mins 10 secs", "1 days 1 hours 1 minutes 1 seconds", "1 year 59 mins", "1 year 59 mins foobar", NULL};

int
main(void)
{
                                      

#line 62 "dt_test2.pgc"
  date date1;

#line 63 "dt_test2.pgc"
  timestamp ts1, ts2;

#line 64 "dt_test2.pgc"
  char *text;

#line 65 "dt_test2.pgc"
  interval *i1;

#line 66 "dt_test2.pgc"
  date *dc;
                                  
#line 67 "dt_test2.pgc"

  int i, j;
  char *endptr;

  ECPGdebug(1, stderr);

  ts1 = PGTYPEStimestamp_from_asc("2003-12-04 17:34:29", NULL);
  text = PGTYPEStimestamp_to_asc(ts1);

  printf("timestamp: %s\n", text);
  PGTYPESchar_free(text);

  date1 = PGTYPESdate_from_timestamp(ts1);
  dc = PGTYPESdate_new();
  *dc = date1;
  text = PGTYPESdate_to_asc(*dc);
  printf("Date of timestamp: %s\n", text);
  PGTYPESchar_free(text);
  PGTYPESdate_free(dc);

  for (i = 0; dates[i]; i++)
  {
    bool err = false;
    date1 = PGTYPESdate_from_asc(dates[i], &endptr);
    if (date1 == INT_MIN)
    {
      err = true;
    }
    text = PGTYPESdate_to_asc(date1);
    printf("Date[%d]: %s (%c - %c)\n", i, err ? "-" : text, endptr ? 'N' : 'Y', err ? 'T' : 'F');
    PGTYPESchar_free(text);
    if (!err)
    {
      for (j = 0; times[j]; j++)
      {
        int length = strlen(dates[i]) + 1 + strlen(times[j]) + 1;
        char *t = malloc(length);
        sprintf(t, "%s %s", dates[i], times[j]);
        ts1 = PGTYPEStimestamp_from_asc(t, NULL);
        text = PGTYPEStimestamp_to_asc(ts1);
        printf("TS[%d,%d]: %s\n", i, j, errno ? "-" : text);
        PGTYPESchar_free(text);
        free(t);
      }
    }
  }

  ts1 = PGTYPEStimestamp_from_asc("2004-04-04 23:23:23", NULL);

  for (i = 0; intervals[i]; i++)
  {
    interval *ic;
    i1 = PGTYPESinterval_from_asc(intervals[i], &endptr);
    if (*endptr)
    {
      printf("endptr set to %s\n", endptr);
    }
    if (!i1)
    {
      printf("Error parsing interval %d\n", i);
      continue;
    }
    j = PGTYPEStimestamp_add_interval(&ts1, i1, &ts2);
    if (j < 0)
    {
      continue;
    }
    text = PGTYPESinterval_to_asc(i1);
    printf("interval[%d]: %s\n", i, text ? text : "-");
    PGTYPESchar_free(text);

    ic = PGTYPESinterval_new();
    PGTYPESinterval_copy(i1, ic);
    text = PGTYPESinterval_to_asc(i1);
    printf("interval_copy[%d]: %s\n", i, text ? text : "-");
    PGTYPESchar_free(text);
    PGTYPESinterval_free(ic);
    PGTYPESinterval_free(i1);
  }

  return 0;
}
