   
                                              
   
#include "postgres_fe.h"

#include <time.h>
#include <limits.h>
#include <math.h>

#ifdef __FAST_MATH__
#error -ffast-math is known to break this code
#endif

#include "pgtypeslib_extern.h"
#include "dt.h"
#include "pgtypes_timestamp.h"
#include "pgtypes_date.h"

static int64
time2t(const int hour, const int min, const int sec, const fsec_t fsec)
{
  return (((((hour * MINS_PER_HOUR) + min) * SECS_PER_MINUTE) + sec) * USECS_PER_SEC) + fsec;
}               

static timestamp
dt2local(timestamp dt, int tz)
{
  dt -= (tz * USECS_PER_SEC);
  return dt;
}                 

                  
                                                    
                                                                      
                                               
   
                                     
   
int
tm2timestamp(struct tm *tm, fsec_t fsec, int *tzp, timestamp *result)
{
  int dDate;
  int64 time;

                                               
  if (!IS_VALID_JULIAN(tm->tm_year, tm->tm_mon, tm->tm_mday))
  {
    return -1;
  }

  dDate = date2j(tm->tm_year, tm->tm_mon, tm->tm_mday) - date2j(2000, 1, 1);
  time = time2t(tm->tm_hour, tm->tm_min, tm->tm_sec, fsec);
  *result = (dDate * USECS_PER_DAY) + time;
                                
  if ((*result - time) / USECS_PER_DAY != dDate)
  {
    return -1;
  }
                                                                      
                                                     
  if ((*result < 0 && dDate > 0) || (*result > 0 && dDate < -1))
  {
    return -1;
  }
  if (tzp != NULL)
  {
    *result = dt2local(*result, -(*tzp));
  }

                                                              
  if (!IS_VALID_TIMESTAMP(*result))
  {
    return -1;
  }

  return 0;
}                     

static timestamp
SetEpochTimestamp(void)
{
  int64 noresult = 0;
  timestamp dt;
  struct tm tt, *tm = &tt;

  if (GetEpochTime(tm) < 0)
  {
    return noresult;
  }

  tm2timestamp(tm, 0, NULL, &dt);
  return dt;
}                          

                  
                                                        
                                                                      
                                               
            
                 
                      
   
                                                                      
                                                                       
   
static int
timestamp2tm(timestamp dt, int *tzp, struct tm *tm, fsec_t *fsec, const char **tzn)
{
  int64 dDate, date0;
  int64 time;
#if defined(HAVE_TM_ZONE) || defined(HAVE_INT_TIMEZONE)
  time_t utime;
  struct tm *tx;
#endif

  date0 = date2j(2000, 1, 1);

  time = dt;
  TMODULO(time, dDate, USECS_PER_DAY);

  if (time < INT64CONST(0))
  {
    time += USECS_PER_DAY;
    dDate -= 1;
  }

                                                                
  dDate += date0;

                                                                 
  if (dDate < 0 || dDate > (timestamp)INT_MAX)
  {
    return -1;
  }

  j2date((int)dDate, &tm->tm_year, &tm->tm_mon, &tm->tm_mday);
  dt2time(time, &tm->tm_hour, &tm->tm_min, &tm->tm_sec, fsec);

  if (tzp != NULL)
  {
       
                                                                 
                                                                  
       
    if (IS_VALID_UTIME(tm->tm_year, tm->tm_mon, tm->tm_mday))
    {
#if defined(HAVE_TM_ZONE) || defined(HAVE_INT_TIMEZONE)

      utime = dt / USECS_PER_SEC + ((date0 - date2j(1970, 1, 1)) * INT64CONST(86400));

      tx = localtime(&utime);
      tm->tm_year = tx->tm_year + 1900;
      tm->tm_mon = tx->tm_mon + 1;
      tm->tm_mday = tx->tm_mday;
      tm->tm_hour = tx->tm_hour;
      tm->tm_min = tx->tm_min;
      tm->tm_isdst = tx->tm_isdst;

#if defined(HAVE_TM_ZONE)
      tm->tm_gmtoff = tx->tm_gmtoff;
      tm->tm_zone = tx->tm_zone;

      *tzp = -tm->tm_gmtoff;                               
      if (tzn != NULL)
      {
        *tzn = tm->tm_zone;
      }
#elif defined(HAVE_INT_TIMEZONE)
      *tzp = (tm->tm_isdst > 0) ? TIMEZONE_GLOBAL - SECS_PER_HOUR : TIMEZONE_GLOBAL;
      if (tzn != NULL)
      {
        *tzn = TZNAME_GLOBAL[(tm->tm_isdst > 0)];
      }
#endif
#else                                              
      *tzp = 0;
                                                 
      tm->tm_isdst = -1;
      if (tzn != NULL)
      {
        *tzn = NULL;
      }
#endif
    }
    else
    {
      *tzp = 0;
                                                 
      tm->tm_isdst = -1;
      if (tzn != NULL)
      {
        *tzn = NULL;
      }
    }
  }
  else
  {
    tm->tm_isdst = -1;
    if (tzn != NULL)
    {
      *tzn = NULL;
    }
  }

  tm->tm_yday = dDate - date2j(tm->tm_year, 1, 1) + 1;

  return 0;
}                     

                            
                                                     
      
static void
EncodeSpecialTimestamp(timestamp dt, char *str)
{
  if (TIMESTAMP_IS_NOBEGIN(dt))
  {
    strcpy(str, EARLY);
  }
  else if (TIMESTAMP_IS_NOEND(dt))
  {
    strcpy(str, LATE);
  }
  else
  {
    abort();                       
  }
}

timestamp
PGTYPEStimestamp_from_asc(char *str, char **endptr)
{
  timestamp result;
  int64 noresult = 0;
  fsec_t fsec;
  struct tm tt, *tm = &tt;
  int dtype;
  int nf;
  char *field[MAXDATEFIELDS];
  int ftype[MAXDATEFIELDS];
  char lowstr[MAXDATELEN + MAXDATEFIELDS];
  char *realptr;
  char **ptr = (endptr != NULL) ? endptr : &realptr;

  if (strlen(str) > MAXDATELEN)
  {
    errno = PGTYPES_TS_BAD_TIMESTAMP;
    return noresult;
  }

  if (ParseDateTime(str, lowstr, field, ftype, &nf, ptr) != 0 || DecodeDateTime(field, ftype, nf, &dtype, tm, &fsec, 0) != 0)
  {
    errno = PGTYPES_TS_BAD_TIMESTAMP;
    return noresult;
  }

  switch (dtype)
  {
  case DTK_DATE:
    if (tm2timestamp(tm, fsec, NULL, &result) != 0)
    {
      errno = PGTYPES_TS_BAD_TIMESTAMP;
      return noresult;
    }
    break;

  case DTK_EPOCH:
    result = SetEpochTimestamp();
    break;

  case DTK_LATE:
    TIMESTAMP_NOEND(result);
    break;

  case DTK_EARLY:
    TIMESTAMP_NOBEGIN(result);
    break;

  default:
    errno = PGTYPES_TS_BAD_TIMESTAMP;
    return noresult;
  }

                                                  

     
                                                                           
                     
     
  errno = 0;
  return result;
}

char *
PGTYPEStimestamp_to_asc(timestamp tstamp)
{
  struct tm tt, *tm = &tt;
  char buf[MAXDATELEN + 1];
  fsec_t fsec;
  int DateStyle = 1;                                              
                                        

  if (TIMESTAMP_NOT_FINITE(tstamp))
  {
    EncodeSpecialTimestamp(tstamp, buf);
  }
  else if (timestamp2tm(tstamp, NULL, tm, &fsec, NULL) == 0)
  {
    EncodeDateTime(tm, fsec, false, 0, NULL, DateStyle, buf, 0);
  }
  else
  {
    errno = PGTYPES_TS_BAD_TIMESTAMP;
    return NULL;
  }
  return pgtypes_strdup(buf);
}

void
PGTYPEStimestamp_current(timestamp *ts)
{
  struct tm tm;

  GetCurrentDateTime(&tm);
  if (errno == 0)
  {
    tm2timestamp(&tm, 0, NULL, ts);
  }
  return;
}

static int
dttofmtasc_replace(timestamp *ts, date dDate, int dow, struct tm *tm, char *output, int *pstr_len, const char *fmtstr)
{
  union un_fmt_comb replace_val;
  int replace_type;
  int i;
  const char *p = fmtstr;
  char *q = output;

  while (*p)
  {
    if (*p == '%')
    {
      p++;
                                
      replace_type = PGTYPES_TYPE_NOTHING;
      switch (*p)
      {
                                                         
                                        
      case 'a':
        replace_val.str_val = pgtypes_date_weekdays_short[dow];
        replace_type = PGTYPES_TYPE_STRING_CONSTANT;
        break;
                                                  
                                        
      case 'A':
        replace_val.str_val = days[dow];
        replace_type = PGTYPES_TYPE_STRING_CONSTANT;
        break;
                                               
                                        
      case 'b':
      case 'h':
        replace_val.str_val = months[tm->tm_mon - 1];
        replace_type = PGTYPES_TYPE_STRING_CONSTANT;
        break;
                                        
                                        
      case 'B':
        replace_val.str_val = pgtypes_date_months[tm->tm_mon - 1];
        replace_type = PGTYPES_TYPE_STRING_CONSTANT;
        break;

           
                                                             
                               
           
      case 'c':
                 
        break;
                                                    
      case 'C':
        replace_val.uint_val = tm->tm_year / 100;
        replace_type = PGTYPES_TYPE_UINT_2_LZ;
        break;
                                               
      case 'd':
        replace_val.uint_val = tm->tm_mday;
        replace_type = PGTYPES_TYPE_UINT_2_LZ;
        break;
                                             
      case 'D':

           
                                                                 
           
                                                       
           
                                                                  
                              
           
        i = dttofmtasc_replace(ts, dDate, dow, tm, q, pstr_len, "%m/%d/%y");
        if (i)
        {
          return i;
        }
        break;
                                               
      case 'e':
        replace_val.uint_val = tm->tm_mday;
        replace_type = PGTYPES_TYPE_UINT_2_LS;
        break;

           
                                       
           
      case 'E':
      {
        char tmp[4] = "%Ex";

        p++;
        if (*p == '\0')
        {
          return -1;
        }
        tmp[2] = *p;

           
                                                        
           
        tm->tm_mon -= 1;
        i = strftime(q, *pstr_len, tmp, tm);
        if (i == 0)
        {
          return -1;
        }
        while (*q)
        {
          q++;
          (*pstr_len)--;
        }
        tm->tm_mon += 1;
        replace_type = PGTYPES_TYPE_NOTHING;
        break;
      }

           
                                                                   
                                                              
           
      case 'G':
      {
                                                              
        const char *fmt = "%G";

        tm->tm_mon -= 1;
        i = strftime(q, *pstr_len, fmt, tm);
        if (i == 0)
        {
          return -1;
        }
        while (*q)
        {
          q++;
          (*pstr_len)--;
        }
        tm->tm_mon += 1;
        replace_type = PGTYPES_TYPE_NOTHING;
      }
      break;

           
                                                                   
                    
           
      case 'g':
      {
        const char *fmt = "%g";                              
                                                  

        tm->tm_mon -= 1;
        i = strftime(q, *pstr_len, fmt, tm);
        if (i == 0)
        {
          return -1;
        }
        while (*q)
        {
          q++;
          (*pstr_len)--;
        }
        tm->tm_mon += 1;
        replace_type = PGTYPES_TYPE_NOTHING;
      }
      break;
                                                      
      case 'H':
        replace_val.uint_val = tm->tm_hour;
        replace_type = PGTYPES_TYPE_UINT_2_LZ;
        break;
                                                      
      case 'I':
        replace_val.uint_val = tm->tm_hour % 12;
        replace_type = PGTYPES_TYPE_UINT_2_LZ;
        break;

           
                                                                
                                              
           
      case 'j':
        replace_val.uint_val = tm->tm_yday;
        replace_type = PGTYPES_TYPE_UINT_3_LZ;
        break;

           
                                                                   
                        
           
      case 'k':
        replace_val.uint_val = tm->tm_hour;
        replace_type = PGTYPES_TYPE_UINT_2_LS;
        break;

           
                                                                   
                        
           
      case 'l':
        replace_val.uint_val = tm->tm_hour % 12;
        replace_type = PGTYPES_TYPE_UINT_2_LS;
        break;
                                                               
      case 'm':
        replace_val.uint_val = tm->tm_mon;
        replace_type = PGTYPES_TYPE_UINT_2_LZ;
        break;
                                                                
      case 'M':
        replace_val.uint_val = tm->tm_min;
        replace_type = PGTYPES_TYPE_UINT_2_LZ;
        break;
                                 
      case 'n':
        replace_val.char_val = '\n';
        replace_type = PGTYPES_TYPE_CHAR;
        break;
                                             
                                        
      case 'p':
        if (tm->tm_hour < 12)
        {
          replace_val.str_val = "AM";
        }
        else
        {
          replace_val.str_val = "PM";
        }
        replace_type = PGTYPES_TYPE_STRING_CONSTANT;
        break;
                                             
                                        
      case 'P':
        if (tm->tm_hour < 12)
        {
          replace_val.str_val = "am";
        }
        else
        {
          replace_val.str_val = "pm";
        }
        replace_type = PGTYPES_TYPE_STRING_CONSTANT;
        break;
                                                
                                        
      case 'r':
        i = dttofmtasc_replace(ts, dDate, dow, tm, q, pstr_len, "%I:%M:%S %p");
        if (i)
        {
          return i;
        }
        break;
                                                  
      case 'R':
        i = dttofmtasc_replace(ts, dDate, dow, tm, q, pstr_len, "%H:%M");
        if (i)
        {
          return i;
        }
        break;
                                                                
      case 's':
        replace_val.int64_val = (*ts - SetEpochTimestamp()) / 1000000.0;
        replace_type = PGTYPES_TYPE_INT64;
        break;
                                                             
      case 'S':
        replace_val.uint_val = tm->tm_sec;
        replace_type = PGTYPES_TYPE_UINT_2_LZ;
        break;
                         
      case 't':
        replace_val.char_val = '\t';
        replace_type = PGTYPES_TYPE_CHAR;
        break;
                                                     
      case 'T':
        i = dttofmtasc_replace(ts, dDate, dow, tm, q, pstr_len, "%H:%M:%S");
        if (i)
        {
          return i;
        }
        break;

           
                                                                  
             
           
      case 'u':
        replace_val.uint_val = dow;
        if (replace_val.uint_val == 0)
        {
          replace_val.uint_val = 7;
        }
        replace_type = PGTYPES_TYPE_UINT;
        break;
                                                             
      case 'U':
        tm->tm_mon -= 1;
        i = strftime(q, *pstr_len, "%U", tm);
        if (i == 0)
        {
          return -1;
        }
        while (*q)
        {
          q++;
          (*pstr_len)--;
        }
        tm->tm_mon += 1;
        replace_type = PGTYPES_TYPE_NOTHING;
        break;

           
                                                                  
                           
           
      case 'V':
      {
                                                              
        const char *fmt = "%V";

        i = strftime(q, *pstr_len, fmt, tm);
        if (i == 0)
        {
          return -1;
        }
        while (*q)
        {
          q++;
          (*pstr_len)--;
        }
        replace_type = PGTYPES_TYPE_NOTHING;
      }
      break;

           
                                                                
                     
           
      case 'w':
        replace_val.uint_val = dow;
        replace_type = PGTYPES_TYPE_UINT;
        break;
                                                              
      case 'W':
        tm->tm_mon -= 1;
        i = strftime(q, *pstr_len, "%U", tm);
        if (i == 0)
        {
          return -1;
        }
        while (*q)
        {
          q++;
          (*pstr_len)--;
        }
        tm->tm_mon += 1;
        replace_type = PGTYPES_TYPE_NOTHING;
        break;

           
                                                             
                                    
           
      case 'x':
      {
        const char *fmt = "%x";                              
                                                  

        tm->tm_mon -= 1;
        i = strftime(q, *pstr_len, fmt, tm);
        if (i == 0)
        {
          return -1;
        }
        while (*q)
        {
          q++;
          (*pstr_len)--;
        }
        tm->tm_mon += 1;
        replace_type = PGTYPES_TYPE_NOTHING;
      }
      break;

           
                                                             
                                    
           
      case 'X':
        tm->tm_mon -= 1;
        i = strftime(q, *pstr_len, "%X", tm);
        if (i == 0)
        {
          return -1;
        }
        while (*q)
        {
          q++;
          (*pstr_len)--;
        }
        tm->tm_mon += 1;
        replace_type = PGTYPES_TYPE_NOTHING;
        break;
                                                                     
      case 'y':
        replace_val.uint_val = tm->tm_year % 100;
        replace_type = PGTYPES_TYPE_UINT_2_LZ;
        break;
                                                  
      case 'Y':
        replace_val.uint_val = tm->tm_year;
        replace_type = PGTYPES_TYPE_UINT;
        break;
                                           
      case 'z':
        tm->tm_mon -= 1;
        i = strftime(q, *pstr_len, "%z", tm);
        if (i == 0)
        {
          return -1;
        }
        while (*q)
        {
          q++;
          (*pstr_len)--;
        }
        tm->tm_mon += 1;
        replace_type = PGTYPES_TYPE_NOTHING;
        break;
                                                       
      case 'Z':
        tm->tm_mon -= 1;
        i = strftime(q, *pstr_len, "%Z", tm);
        if (i == 0)
        {
          return -1;
        }
        while (*q)
        {
          q++;
          (*pstr_len)--;
        }
        tm->tm_mon += 1;
        replace_type = PGTYPES_TYPE_NOTHING;
        break;
                      
      case '%':
        replace_val.char_val = '%';
        replace_type = PGTYPES_TYPE_CHAR;
        break;
      case '\0':
                                                           

           
                                                      
           
        return -1;
      default:

           
                                                         
           
        if (*pstr_len > 1)
        {
          *q = '%';
          q++;
          (*pstr_len)--;
          if (*pstr_len > 1)
          {
            *q = *p;
            q++;
            (*pstr_len)--;
          }
          else
          {
            *q = '\0';
            return -1;
          }
          *q = '\0';
        }
        else
        {
          return -1;
        }
        break;
      }
      i = pgtypes_fmt_replace(replace_val, replace_type, &q, pstr_len);
      if (i)
      {
        return i;
      }
    }
    else
    {
      if (*pstr_len > 1)
      {
        *q = *p;
        (*pstr_len)--;
        q++;
        *q = '\0';
      }
      else
      {
        return -1;
      }
    }
    p++;
  }
  return 0;
}

int
PGTYPEStimestamp_fmt_asc(timestamp *ts, char *output, int str_len, const char *fmtstr)
{
  struct tm tm;
  fsec_t fsec;
  date dDate;
  int dow;

  dDate = PGTYPESdate_from_timestamp(*ts);
  dow = PGTYPESdate_dayofweek(dDate);
  timestamp2tm(*ts, NULL, &tm, &fsec, NULL);

  return dttofmtasc_replace(ts, dDate, dow, &tm, output, &str_len, fmtstr);
}

int
PGTYPEStimestamp_sub(timestamp *ts1, timestamp *ts2, interval *iv)
{
  if (TIMESTAMP_NOT_FINITE(*ts1) || TIMESTAMP_NOT_FINITE(*ts2))
  {
    return PGTYPES_TS_ERR_EINFTIME;
  }
  else
  {
    iv->time = (*ts1 - *ts2);
  }

  iv->month = 0;

  return 0;
}

int
PGTYPEStimestamp_defmt_asc(const char *str, const char *fmt, timestamp *d)
{
  int year, month, day;
  int hour, minute, second;
  int tz;

  int i;
  char *mstr;
  char *mfmt;

  if (!fmt)
  {
    fmt = "%Y-%m-%d %H:%M:%S";
  }
  if (!fmt[0])
  {
    return 1;
  }

  mstr = pgtypes_strdup(str);
  mfmt = pgtypes_strdup(fmt);

     
                                                                        
                            
     
                                         
  year = -1;
  month = -1;
  day = -1;
  hour = 0;
  minute = -1;
  second = -1;
  tz = 0;

  i = PGTYPEStimestamp_defmt_scan(&mstr, mfmt, d, &year, &month, &day, &hour, &minute, &second, &tz);
  free(mstr);
  free(mfmt);
  return i;
}

   
                                   
   
                      
   
                            
                           
   
   

int
PGTYPEStimestamp_add_interval(timestamp *tin, interval *span, timestamp *tout)
{
  if (TIMESTAMP_NOT_FINITE(*tin))
  {
    *tout = *tin;
  }

  else
  {
    if (span->month != 0)
    {
      struct tm tt, *tm = &tt;
      fsec_t fsec;

      if (timestamp2tm(*tin, NULL, tm, &fsec, NULL) != 0)
      {
        return -1;
      }
      tm->tm_mon += span->month;
      if (tm->tm_mon > MONTHS_PER_YEAR)
      {
        tm->tm_year += (tm->tm_mon - 1) / MONTHS_PER_YEAR;
        tm->tm_mon = (tm->tm_mon - 1) % MONTHS_PER_YEAR + 1;
      }
      else if (tm->tm_mon < 1)
      {
        tm->tm_year += tm->tm_mon / MONTHS_PER_YEAR - 1;
        tm->tm_mon = tm->tm_mon % MONTHS_PER_YEAR + MONTHS_PER_YEAR;
      }

                                                        
      if (tm->tm_mday > day_tab[isleap(tm->tm_year)][tm->tm_mon - 1])
      {
        tm->tm_mday = (day_tab[isleap(tm->tm_year)][tm->tm_mon - 1]);
      }

      if (tm2timestamp(tm, fsec, NULL, tin) != 0)
      {
        return -1;
      }
    }

    *tin += span->time;
    *tout = *tin;
  }
  return 0;
}

   
                                          
   
                      
   
                            
                           
   
   

int
PGTYPEStimestamp_sub_interval(timestamp *tin, interval *span, timestamp *tout)
{
  interval tspan;

  tspan.month = -span->month;
  tspan.time = -span->time;

  return PGTYPEStimestamp_add_interval(tin, &tspan, tout);
}
