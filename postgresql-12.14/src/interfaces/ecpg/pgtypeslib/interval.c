                                               

#include "postgres_fe.h"
#include <time.h>
#include <math.h>
#include <limits.h>

#ifdef __FAST_MATH__
#error -ffast-math is known to break this code
#endif

#include "common/string.h"

#include "pgtypeslib_extern.h"
#include "dt.h"
#include "pgtypes_error.h"
#include "pgtypes_interval.h"

                                                         
                                          
   
static void
AdjustFractSeconds(double frac, struct           tm *tm, fsec_t *fsec, int scale)
{
  int sec;

  if (frac == 0)
  {
    return;
  }
  frac *= scale;
  sec = (int)frac;
  tm->tm_sec += sec;
  frac -= sec;
  *fsec += rint(frac * 1000000);
}

                                                         
                                          
   
static void
AdjustFractDays(double frac, struct           tm *tm, fsec_t *fsec, int scale)
{
  int extra_days;

  if (frac == 0)
  {
    return;
  }
  frac *= scale;
  extra_days = (int)frac;
  tm->tm_mday += extra_days;
  frac -= extra_days;
  AdjustFractSeconds(frac, tm, fsec, SECS_PER_DAY);
}

                                                           
static int
ParseISO8601Number(const char *str, char **endptr, int *ipart, double *fpart)
{
  double val;

  if (!(isdigit((unsigned char)*str) || *str == '-' || *str == '.'))
  {
    return DTERR_BAD_FORMAT;
  }
  errno = 0;
  val = strtod(str, endptr);
                                                         
  if (*endptr == str || errno != 0)
  {
    return DTERR_BAD_FORMAT;
  }
                              
  if (val < INT_MIN || val > INT_MAX)
  {
    return DTERR_FIELD_OVERFLOW;
  }
                                                           
  if (val >= 0)
  {
    *ipart = (int)floor(val);
  }
  else
  {
    *ipart = (int)-floor(-val);
  }
  *fpart = val - *ipart;
  return 0;
}

                                                           
static int
ISO8601IntegerWidth(const char *fieldstart)
{
                                       
  if (*fieldstart == '-')
  {
    fieldstart++;
  }
  return strspn(fieldstart, "0123456789");
}

                                                         
                                          
   
static inline void
ClearPgTm(struct           tm *tm, fsec_t *fsec)
{
  tm->tm_year = 0;
  tm->tm_mon = 0;
  tm->tm_mday = 0;
  tm->tm_hour = 0;
  tm->tm_min = 0;
  tm->tm_sec = 0;
  *fsec = 0;
}

                                                         
   
                                        
   
                              
   
static int
DecodeISO8601Interval(char *str, int *dtype, struct           tm *tm, fsec_t *fsec)
{
  bool datepart = true;
  bool havefield = false;

  *dtype = DTK_DELTA;
  ClearPgTm(tm, fsec);

  if (strlen(str) < 2 || str[0] != 'P')
  {
    return DTERR_BAD_FORMAT;
  }

  str++;
  while (*str)
  {
    char *fieldstart;
    int val;
    double fval;
    char unit;
    int dterr;

    if (*str == 'T')                                                 
    {
      datepart = false;
      havefield = false;
      str++;
      continue;
    }

    fieldstart = str;
    dterr = ParseISO8601Number(str, &str, &val, &fval);
    if (dterr)
    {
      return dterr;
    }

       
                                                                       
                                             
       
    unit = *str++;

    if (datepart)
    {
      switch (unit)                        
      {
      case 'Y':
        tm->tm_year += val;
        tm->tm_mon += (fval * MONTHS_PER_YEAR);
        break;
      case 'M':
        tm->tm_mon += val;
        AdjustFractDays(fval, tm, fsec, DAYS_PER_MONTH);
        break;
      case 'W':
        tm->tm_mday += val * 7;
        AdjustFractDays(fval, tm, fsec, 7);
        break;
      case 'D':
        tm->tm_mday += val;
        AdjustFractSeconds(fval, tm, fsec, SECS_PER_DAY);
        break;
      case 'T':                                                  
      case '\0':
        if (ISO8601IntegerWidth(fieldstart) == 8 && !havefield)
        {
          tm->tm_year += val / 10000;
          tm->tm_mon += (val / 100) % 100;
          tm->tm_mday += val % 100;
          AdjustFractSeconds(fval, tm, fsec, SECS_PER_DAY);
          if (unit == '\0')
          {
            return 0;
          }
          datepart = false;
          havefield = false;
          continue;
        }
                                                              
                         
      case '-':                                         
                              
        if (havefield)
        {
          return DTERR_BAD_FORMAT;
        }

        tm->tm_year += val;
        tm->tm_mon += (fval * MONTHS_PER_YEAR);
        if (unit == '\0')
        {
          return 0;
        }
        if (unit == 'T')
        {
          datepart = false;
          havefield = false;
          continue;
        }

        dterr = ParseISO8601Number(str, &str, &val, &fval);
        if (dterr)
        {
          return dterr;
        }
        tm->tm_mon += val;
        AdjustFractDays(fval, tm, fsec, DAYS_PER_MONTH);
        if (*str == '\0')
        {
          return 0;
        }
        if (*str == 'T')
        {
          datepart = false;
          havefield = false;
          continue;
        }
        if (*str != '-')
        {
          return DTERR_BAD_FORMAT;
        }
        str++;

        dterr = ParseISO8601Number(str, &str, &val, &fval);
        if (dterr)
        {
          return dterr;
        }
        tm->tm_mday += val;
        AdjustFractSeconds(fval, tm, fsec, SECS_PER_DAY);
        if (*str == '\0')
        {
          return 0;
        }
        if (*str == 'T')
        {
          datepart = false;
          havefield = false;
          continue;
        }
        return DTERR_BAD_FORMAT;
      default:
                                          
        return DTERR_BAD_FORMAT;
      }
    }
    else
    {
      switch (unit)                     
      {
      case 'H':
        tm->tm_hour += val;
        AdjustFractSeconds(fval, tm, fsec, SECS_PER_HOUR);
        break;
      case 'M':
        tm->tm_min += val;
        AdjustFractSeconds(fval, tm, fsec, SECS_PER_MINUTE);
        break;
      case 'S':
        tm->tm_sec += val;
        AdjustFractSeconds(fval, tm, fsec, 1);
        break;
      case '\0':                                          
        if (ISO8601IntegerWidth(fieldstart) == 6 && !havefield)
        {
          tm->tm_hour += val / 10000;
          tm->tm_min += (val / 100) % 100;
          tm->tm_sec += val % 100;
          AdjustFractSeconds(fval, tm, fsec, 1);
          return 0;
        }
                                                              
                         
      case ':':                                         
                              
        if (havefield)
        {
          return DTERR_BAD_FORMAT;
        }

        tm->tm_hour += val;
        AdjustFractSeconds(fval, tm, fsec, SECS_PER_HOUR);
        if (unit == '\0')
        {
          return 0;
        }

        dterr = ParseISO8601Number(str, &str, &val, &fval);
        if (dterr)
        {
          return dterr;
        }
        tm->tm_min += val;
        AdjustFractSeconds(fval, tm, fsec, SECS_PER_MINUTE);
        if (*str == '\0')
        {
          return 0;
        }
        if (*str != ':')
        {
          return DTERR_BAD_FORMAT;
        }
        str++;

        dterr = ParseISO8601Number(str, &str, &val, &fval);
        if (dterr)
        {
          return dterr;
        }
        tm->tm_sec += val;
        AdjustFractSeconds(fval, tm, fsec, 1);
        if (*str == '\0')
        {
          return 0;
        }
        return DTERR_BAD_FORMAT;

      default:
                                          
        return DTERR_BAD_FORMAT;
      }
    }

    havefield = true;
  }

  return 0;
}

                                                         
                     
   
                                        
   
                                                       
                                                    
                                                  
                                      
   
                                                   
              
                                           
   
int
DecodeInterval(char **field, int *ftype, int nf,                 
    int *dtype, struct           tm *tm, fsec_t *fsec)
{
  int IntervalStyle = INTSTYLE_POSTGRES_VERBOSE;
  int range = INTERVAL_FULL_RANGE;
  bool is_before = false;
  char *cp;
  int fmask = 0, tmask, type;
  int i;
  int dterr;
  int val;
  double fval;

  *dtype = DTK_DELTA;
  type = IGNORE_DTF;
  ClearPgTm(tm, fsec);

                                                                  
  for (i = nf - 1; i >= 0; i--)
  {
    switch (ftype[i])
    {
    case DTK_TIME:
      dterr = DecodeTime(field[i],             
          &tmask, tm, fsec);
      if (dterr)
      {
        return dterr;
      }
      type = DTK_DAY;
      break;

    case DTK_TZ:

         
                                                                  
                                                                   
                     
         
      Assert(*field[i] == '-' || *field[i] == '+');

         
                                                             
                                                                    
                                   
         
      if (strchr(field[i] + 1, ':') != NULL && DecodeTime(field[i] + 1,                           
                                                   &tmask, tm, fsec) == 0)
      {
        if (*field[i] == '-')
        {
                                           
          tm->tm_hour = -tm->tm_hour;
          tm->tm_min = -tm->tm_min;
          tm->tm_sec = -tm->tm_sec;
          *fsec = -(*fsec);
        }

           
                                                           
                                                                   
                                      
           
        type = DTK_DAY;
        tmask = DTK_M(TZ);
        break;
      }
                        

    case DTK_DATE:
    case DTK_NUMBER:
      if (type == IGNORE_DTF)
      {
                                                          
        switch (range)
        {
        case INTERVAL_MASK(YEAR):
          type = DTK_YEAR;
          break;
        case INTERVAL_MASK(MONTH):
        case INTERVAL_MASK(YEAR) | INTERVAL_MASK(MONTH):
          type = DTK_MONTH;
          break;
        case INTERVAL_MASK(DAY):
          type = DTK_DAY;
          break;
        case INTERVAL_MASK(HOUR):
        case INTERVAL_MASK(DAY) | INTERVAL_MASK(HOUR):
        case INTERVAL_MASK(DAY) | INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE):
        case INTERVAL_MASK(DAY) | INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE) | INTERVAL_MASK(SECOND):
          type = DTK_HOUR;
          break;
        case INTERVAL_MASK(MINUTE):
        case INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE):
          type = DTK_MINUTE;
          break;
        case INTERVAL_MASK(SECOND):
        case INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE) | INTERVAL_MASK(SECOND):
        case INTERVAL_MASK(MINUTE) | INTERVAL_MASK(SECOND):
          type = DTK_SECOND;
          break;
        default:
          type = DTK_SECOND;
          break;
        }
      }

      errno = 0;
      val = strtoint(field[i], &cp, 10);
      if (errno == ERANGE)
      {
        return DTERR_FIELD_OVERFLOW;
      }

      if (*cp == '-')
      {
                                       
        int val2;

        val2 = strtoint(cp + 1, &cp, 10);
        if (errno == ERANGE || val2 < 0 || val2 >= MONTHS_PER_YEAR)
        {
          return DTERR_FIELD_OVERFLOW;
        }
        if (*cp != '\0')
        {
          return DTERR_BAD_FORMAT;
        }
        type = DTK_MONTH;
        if (*field[i] == '-')
        {
          val2 = -val2;
        }
        val = val * MONTHS_PER_YEAR + val2;
        fval = 0;
      }
      else if (*cp == '.')
      {
        errno = 0;
        fval = strtod(cp, &cp);
        if (*cp != '\0' || errno != 0)
        {
          return DTERR_BAD_FORMAT;
        }

        if (*field[i] == '-')
        {
          fval = -fval;
        }
      }
      else if (*cp == '\0')
      {
        fval = 0;
      }
      else
      {
        return DTERR_BAD_FORMAT;
      }

      tmask = 0;                   

      switch (type)
      {
      case DTK_MICROSEC:
        *fsec += rint(val + fval);
        tmask = DTK_M(MICROSECOND);
        break;

      case DTK_MILLISEC:
        *fsec += rint((val + fval) * 1000);
        tmask = DTK_M(MILLISECOND);
        break;

      case DTK_SECOND:
        tm->tm_sec += val;
        *fsec += rint(fval * 1000000);

           
                                                           
                                                      
           
        if (fval == 0)
        {
          tmask = DTK_M(SECOND);
        }
        else
        {
          tmask = DTK_ALL_SECS_M;
        }
        break;

      case DTK_MINUTE:
        tm->tm_min += val;
        AdjustFractSeconds(fval, tm, fsec, SECS_PER_MINUTE);
        tmask = DTK_M(MINUTE);
        break;

      case DTK_HOUR:
        tm->tm_hour += val;
        AdjustFractSeconds(fval, tm, fsec, SECS_PER_HOUR);
        tmask = DTK_M(HOUR);
        type = DTK_DAY;
        break;

      case DTK_DAY:
        tm->tm_mday += val;
        AdjustFractSeconds(fval, tm, fsec, SECS_PER_DAY);
        tmask = (fmask & DTK_M(DAY)) ? 0 : DTK_M(DAY);
        break;

      case DTK_WEEK:
        tm->tm_mday += val * 7;
        AdjustFractDays(fval, tm, fsec, 7);
        tmask = (fmask & DTK_M(DAY)) ? 0 : DTK_M(DAY);
        break;

      case DTK_MONTH:
        tm->tm_mon += val;
        AdjustFractDays(fval, tm, fsec, DAYS_PER_MONTH);
        tmask = DTK_M(MONTH);
        break;

      case DTK_YEAR:
        tm->tm_year += val;
        if (fval != 0)
        {
          tm->tm_mon += fval * MONTHS_PER_YEAR;
        }
        tmask = (fmask & DTK_M(YEAR)) ? 0 : DTK_M(YEAR);
        break;

      case DTK_DECADE:
        tm->tm_year += val * 10;
        if (fval != 0)
        {
          tm->tm_mon += fval * MONTHS_PER_YEAR * 10;
        }
        tmask = (fmask & DTK_M(YEAR)) ? 0 : DTK_M(YEAR);
        break;

      case DTK_CENTURY:
        tm->tm_year += val * 100;
        if (fval != 0)
        {
          tm->tm_mon += fval * MONTHS_PER_YEAR * 100;
        }
        tmask = (fmask & DTK_M(YEAR)) ? 0 : DTK_M(YEAR);
        break;

      case DTK_MILLENNIUM:
        tm->tm_year += val * 1000;
        if (fval != 0)
        {
          tm->tm_mon += fval * MONTHS_PER_YEAR * 1000;
        }
        tmask = (fmask & DTK_M(YEAR)) ? 0 : DTK_M(YEAR);
        break;

      default:
        return DTERR_BAD_FORMAT;
      }
      break;

    case DTK_STRING:
    case DTK_SPECIAL:
      type = DecodeUnits(i, field[i], &val);
      if (type == IGNORE_DTF)
      {
        continue;
      }

      tmask = 0;                   
      switch (type)
      {
      case UNITS:
        type = val;
        break;

      case AGO:
        is_before = true;
        type = val;
        break;

      case RESERV:
        tmask = (DTK_DATE_M | DTK_TIME_M);
        *dtype = val;
        break;

      default:
        return DTERR_BAD_FORMAT;
      }
      break;

    default:
      return DTERR_BAD_FORMAT;
    }

    if (tmask & fmask)
    {
      return DTERR_BAD_FORMAT;
    }
    fmask |= tmask;
  }

                                                          
  if (fmask == 0)
  {
    return DTERR_BAD_FORMAT;
  }

                                                
  if (*fsec != 0)
  {
    int sec;

    sec = *fsec / USECS_PER_SEC;
    *fsec -= sec * USECS_PER_SEC;
    tm->tm_sec += sec;
  }

               
                                                   
                   
                                                                    
                                                                        
                                                                         
                                                         
     
                                                                      
                                                                          
                                                                             
                                                                           
                                                                            
                                                
               
     
  if (IntervalStyle == INTSTYLE_SQL_STANDARD && *field[0] == '-')
  {
                                             
    bool more_signs = false;

    for (i = 1; i < nf; i++)
    {
      if (*field[i] == '-' || *field[i] == '+')
      {
        more_signs = true;
        break;
      }
    }

    if (!more_signs)
    {
         
                                                                         
                           
         
      if (*fsec > 0)
      {
        *fsec = -(*fsec);
      }
      if (tm->tm_sec > 0)
      {
        tm->tm_sec = -tm->tm_sec;
      }
      if (tm->tm_min > 0)
      {
        tm->tm_min = -tm->tm_min;
      }
      if (tm->tm_hour > 0)
      {
        tm->tm_hour = -tm->tm_hour;
      }
      if (tm->tm_mday > 0)
      {
        tm->tm_mday = -tm->tm_mday;
      }
      if (tm->tm_mon > 0)
      {
        tm->tm_mon = -tm->tm_mon;
      }
      if (tm->tm_year > 0)
      {
        tm->tm_year = -tm->tm_year;
      }
    }
  }

                                       
  if (is_before)
  {
    *fsec = -(*fsec);
    tm->tm_sec = -tm->tm_sec;
    tm->tm_min = -tm->tm_min;
    tm->tm_hour = -tm->tm_hour;
    tm->tm_mday = -tm->tm_mday;
    tm->tm_mon = -tm->tm_mon;
    tm->tm_year = -tm->tm_year;
  }

  return 0;
}

                                                           
static char *
AddVerboseIntPart(char *cp, int value, const char *units, bool *is_zero, bool *is_before)
{
  if (value == 0)
  {
    return cp;
  }
                                          
  if (*is_zero)
  {
    *is_before = (value < 0);
    value = abs(value);
  }
  else if (*is_before)
  {
    value = -value;
  }
  sprintf(cp, " %d %s%s", value, units, (value == 1) ? "" : "s");
  *is_zero = false;
  return cp + strlen(cp);
}

                                                           
static char *
AddPostgresIntPart(char *cp, int value, const char *units, bool *is_zero, bool *is_before)
{
  if (value == 0)
  {
    return cp;
  }
  sprintf(cp, "%s%s%d %s%s", (!*is_zero) ? " " : "", (*is_before && value > 0) ? "+" : "", value, units, (value != 1) ? "s" : "");

     
                                                                           
                                                  
     
  *is_before = (value < 0);
  *is_zero = false;
  return cp + strlen(cp);
}

                                                           
static char *
AddISO8601IntPart(char *cp, int value, char units)
{
  if (value == 0)
  {
    return cp;
  }
  sprintf(cp, "%d%c", value, units);
  return cp + strlen(cp);
}

                                                           
static void
AppendSeconds(char *cp, int sec, fsec_t fsec, int precision, bool fillzeros)
{
  if (fsec == 0)
  {
    if (fillzeros)
    {
      sprintf(cp, "%02d", abs(sec));
    }
    else
    {
      sprintf(cp, "%d", abs(sec));
    }
  }
  else
  {
    if (fillzeros)
    {
      sprintf(cp, "%02d.%0*d", abs(sec), precision, (int)Abs(fsec));
    }
    else
    {
      sprintf(cp, "%d.%0*d", abs(sec), precision, (int)Abs(fsec));
    }
    TrimTrailingZeros(cp);
  }
}

                                                         
   
                      
   

void
EncodeInterval(struct           tm *tm, fsec_t fsec, int style, char *str)
{
  char *cp = str;
  int year = tm->tm_year;
  int mon = tm->tm_mon;
  int mday = tm->tm_mday;
  int hour = tm->tm_hour;
  int min = tm->tm_min;
  int sec = tm->tm_sec;
  bool is_before = false;
  bool is_zero = true;

     
                                                                        
                                                                             
                                                                       
             
     
  switch (style)
  {
                                      
  case INTSTYLE_SQL_STANDARD:
  {
    bool has_negative = year < 0 || mon < 0 || mday < 0 || hour < 0 || min < 0 || sec < 0 || fsec < 0;
    bool has_positive = year > 0 || mon > 0 || mday > 0 || hour > 0 || min > 0 || sec > 0 || fsec > 0;
    bool has_year_month = year != 0 || mon != 0;
    bool has_day_time = mday != 0 || hour != 0 || min != 0 || sec != 0 || fsec != 0;
    bool has_day = mday != 0;
    bool sql_standard_value = !(has_negative && has_positive) && !(has_year_month && has_day_time);

       
                                                              
                                                      
       
    if (has_negative && sql_standard_value)
    {
      *cp++ = '-';
      year = -year;
      mon = -mon;
      mday = -mday;
      hour = -hour;
      min = -min;
      sec = -sec;
      fsec = -fsec;
    }

    if (!has_negative && !has_positive)
    {
      sprintf(cp, "0");
    }
    else if (!sql_standard_value)
    {
         
                                                                
                                                            
                                
         
      char year_sign = (year < 0 || mon < 0) ? '-' : '+';
      char day_sign = (mday < 0) ? '-' : '+';
      char sec_sign = (hour < 0 || min < 0 || sec < 0 || fsec < 0) ? '-' : '+';

      sprintf(cp, "%c%d-%d %c%d %c%d:%02d:", year_sign, abs(year), abs(mon), day_sign, abs(mday), sec_sign, abs(hour), abs(min));
      cp += strlen(cp);
      AppendSeconds(cp, sec, fsec, MAX_INTERVAL_PRECISION, true);
    }
    else if (has_year_month)
    {
      sprintf(cp, "%d-%d", year, mon);
    }
    else if (has_day)
    {
      sprintf(cp, "%d %d:%02d:", mday, hour, min);
      cp += strlen(cp);
      AppendSeconds(cp, sec, fsec, MAX_INTERVAL_PRECISION, true);
    }
    else
    {
      sprintf(cp, "%d:%02d:", hour, min);
      cp += strlen(cp);
      AppendSeconds(cp, sec, fsec, MAX_INTERVAL_PRECISION, true);
    }
  }
  break;

                                                    
  case INTSTYLE_ISO_8601:
                                                     
    if (year == 0 && mon == 0 && mday == 0 && hour == 0 && min == 0 && sec == 0 && fsec == 0)
    {
      sprintf(cp, "PT0S");
      break;
    }
    *cp++ = 'P';
    cp = AddISO8601IntPart(cp, year, 'Y');
    cp = AddISO8601IntPart(cp, mon, 'M');
    cp = AddISO8601IntPart(cp, mday, 'D');
    if (hour != 0 || min != 0 || sec != 0 || fsec != 0)
    {
      *cp++ = 'T';
    }
    cp = AddISO8601IntPart(cp, hour, 'H');
    cp = AddISO8601IntPart(cp, min, 'M');
    if (sec != 0 || fsec != 0)
    {
      if (sec < 0 || fsec < 0)
      {
        *cp++ = '-';
      }
      AppendSeconds(cp, sec, fsec, MAX_INTERVAL_PRECISION, false);
      cp += strlen(cp);
      *cp++ = 'S';
      *cp = '\0';
    }
    break;

                                                                 
  case INTSTYLE_POSTGRES:
    cp = AddPostgresIntPart(cp, year, "year", &is_zero, &is_before);
    cp = AddPostgresIntPart(cp, mon, "mon", &is_zero, &is_before);
    cp = AddPostgresIntPart(cp, mday, "day", &is_zero, &is_before);
    if (is_zero || hour != 0 || min != 0 || sec != 0 || fsec != 0)
    {
      bool minus = (hour < 0 || min < 0 || sec < 0 || fsec < 0);

      sprintf(cp, "%s%s%02d:%02d:", is_zero ? "" : " ", (minus ? "-" : (is_before ? "+" : "")), abs(hour), abs(min));
      cp += strlen(cp);
      AppendSeconds(cp, sec, fsec, MAX_INTERVAL_PRECISION, true);
    }
    break;

                                                                  
  case INTSTYLE_POSTGRES_VERBOSE:
  default:
    strcpy(cp, "@");
    cp++;
    cp = AddVerboseIntPart(cp, year, "year", &is_zero, &is_before);
    cp = AddVerboseIntPart(cp, mon, "mon", &is_zero, &is_before);
    cp = AddVerboseIntPart(cp, mday, "day", &is_zero, &is_before);
    cp = AddVerboseIntPart(cp, hour, "hour", &is_zero, &is_before);
    cp = AddVerboseIntPart(cp, min, "min", &is_zero, &is_before);
    if (sec != 0 || fsec != 0)
    {
      *cp++ = ' ';
      if (sec < 0 || (sec == 0 && fsec < 0))
      {
        if (is_zero)
        {
          is_before = true;
        }
        else if (!is_before)
        {
          *cp++ = '-';
        }
      }
      else if (is_before)
      {
        *cp++ = '-';
      }
      AppendSeconds(cp, sec, fsec, MAX_INTERVAL_PRECISION, false);
      cp += strlen(cp);
      sprintf(cp, " sec%s", (abs(sec) != 1 || fsec != 0) ? "s" : "");
      is_zero = false;
    }
                                                          
    if (is_zero)
    {
      strcat(cp, " 0");
    }
    if (is_before)
    {
      strcat(cp, " ago");
    }
    break;
  }
}

                 
                                                    
   
static int
interval2tm(interval span, struct tm *tm, fsec_t *fsec)
{
  int64 time;

  if (span.month != 0)
  {
    tm->tm_year = span.month / MONTHS_PER_YEAR;
    tm->tm_mon = span.month % MONTHS_PER_YEAR;
  }
  else
  {
    tm->tm_year = 0;
    tm->tm_mon = 0;
  }

  time = span.time;

  tm->tm_mday = time / USECS_PER_DAY;
  time -= tm->tm_mday * USECS_PER_DAY;
  tm->tm_hour = time / USECS_PER_HOUR;
  time -= tm->tm_hour * USECS_PER_HOUR;
  tm->tm_min = time / USECS_PER_MINUTE;
  time -= tm->tm_min * USECS_PER_MINUTE;
  tm->tm_sec = time / USECS_PER_SEC;
  *fsec = time - (tm->tm_sec * USECS_PER_SEC);

  return 0;
}                    

static int
tm2interval(struct tm *tm, fsec_t fsec, interval *span)
{
  if ((double)tm->tm_year * MONTHS_PER_YEAR + tm->tm_mon > INT_MAX || (double)tm->tm_year * MONTHS_PER_YEAR + tm->tm_mon < INT_MIN)
  {
    return -1;
  }
  span->month = tm->tm_year * MONTHS_PER_YEAR + tm->tm_mon;
  span->time = (((((((tm->tm_mday * INT64CONST(24)) + tm->tm_hour) * INT64CONST(60)) + tm->tm_min) * INT64CONST(60)) + tm->tm_sec) * USECS_PER_SEC) + fsec;

  return 0;
}                    

interval *
PGTYPESinterval_new(void)
{
  interval *result;

  result = (interval *)pgtypes_alloc(sizeof(interval));
                                                  
  return result;
}

void
PGTYPESinterval_free(interval *intvl)
{
  free(intvl);
}

interval *
PGTYPESinterval_from_asc(char *str, char **endptr)
{
  interval *result = NULL;
  fsec_t fsec;
  struct tm tt, *tm = &tt;
  int dtype;
  int nf;
  char *field[MAXDATEFIELDS];
  int ftype[MAXDATEFIELDS];
  char lowstr[MAXDATELEN + MAXDATEFIELDS];
  char *realptr;
  char **ptr = (endptr != NULL) ? endptr : &realptr;

  tm->tm_year = 0;
  tm->tm_mon = 0;
  tm->tm_mday = 0;
  tm->tm_hour = 0;
  tm->tm_min = 0;
  tm->tm_sec = 0;
  fsec = 0;

  if (strlen(str) > MAXDATELEN)
  {
    errno = PGTYPES_INTVL_BAD_INTERVAL;
    return NULL;
  }

  if (ParseDateTime(str, lowstr, field, ftype, &nf, ptr) != 0 || (DecodeInterval(field, ftype, nf, &dtype, tm, &fsec) != 0 && DecodeISO8601Interval(str, &dtype, tm, &fsec) != 0))
  {
    errno = PGTYPES_INTVL_BAD_INTERVAL;
    return NULL;
  }

  result = (interval *)pgtypes_alloc(sizeof(interval));
  if (!result)
  {
    return NULL;
  }

  if (dtype != DTK_DELTA)
  {
    errno = PGTYPES_INTVL_BAD_INTERVAL;
    free(result);
    return NULL;
  }

  if (tm2interval(tm, fsec, result) != 0)
  {
    errno = PGTYPES_INTVL_BAD_INTERVAL;
    free(result);
    return NULL;
  }

  errno = 0;
  return result;
}

char *
PGTYPESinterval_to_asc(interval *span)
{
  struct tm tt, *tm = &tt;
  fsec_t fsec;
  char buf[MAXDATELEN + 1];
  int IntervalStyle = INTSTYLE_POSTGRES_VERBOSE;

  if (interval2tm(*span, tm, &fsec) != 0)
  {
    errno = PGTYPES_INTVL_BAD_INTERVAL;
    return NULL;
  }

  EncodeInterval(tm, fsec, IntervalStyle, buf);

  return pgtypes_strdup(buf);
}

int
PGTYPESinterval_copy(interval *intvlsrc, interval *intvldest)
{
  intvldest->time = intvlsrc->time;
  intvldest->month = intvlsrc->month;

  return 0;
}
