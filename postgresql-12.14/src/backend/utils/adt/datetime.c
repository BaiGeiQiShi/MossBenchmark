                                                                            
   
              
                                            
   
                                                                         
                                                                        
   
   
                  
                                      
   
                                                                            
   
#include "postgres.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>

#include "access/htup_details.h"
#include "access/xact.h"
#include "catalog/pg_type.h"
#include "common/string.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "utils/builtins.h"
#include "utils/date.h"
#include "utils/datetime.h"
#include "utils/memutils.h"
#include "utils/tzparser.h"

static int
DecodeNumber(int flen, char *field, bool haveTextMonth, int fmask, int *tmask, struct pg_tm *tm, fsec_t *fsec, bool *is2digits);
static int
DecodeNumberField(int len, char *str, int fmask, int *tmask, struct pg_tm *tm, fsec_t *fsec, bool *is2digits);
static int
DecodeTime(char *str, int fmask, int range, int *tmask, struct pg_tm *tm, fsec_t *fsec);
static const datetkn *
datebsearch(const char *key, const datetkn *base, int nel);
static int
DecodeDate(char *str, int fmask, int *tmask, bool *is2digits, struct pg_tm *tm);
static char *
AppendSeconds(char *cp, int sec, fsec_t fsec, int precision, bool fillzeros);
static void
AdjustFractSeconds(double frac, struct pg_tm *tm, fsec_t *fsec, int scale);
static void
AdjustFractDays(double frac, struct pg_tm *tm, fsec_t *fsec, int scale);
static int
DetermineTimeZoneOffsetInternal(struct pg_tm *tm, pg_tz *tzp, pg_time_t *tp);
static bool
DetermineTimeZoneAbbrevOffsetInternal(pg_time_t t, const char *abbr, pg_tz *tzp, int *offset, int *isdst);
static pg_tz *
FetchDynamicTimeZone(TimeZoneAbbrevTable *tbl, const datetkn *tp);

const int day_tab[2][13] = {{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 0}, {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 0}};

const char *const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL};

const char *const days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", NULL};

                                                                               
                                     
                                                                               

   
                                       
   
                                                                            
                                         
   
                                                                            
                      
   
                                                                        
                                                                          
                                                                
   
static const datetkn datetktbl[] = {
                            
    {EARLY, RESERV, DTK_EARLY},                                                                                                                                                                                                                                                     
    {DA_D, ADBC, AD},                                                                                                                                                                                                                                            
    {"allballs", RESERV, DTK_ZULU},                                                                                                                                                                                                                    
    {"am", AMPM, AM}, {"apr", MONTH, 4}, {"april", MONTH, 4}, {"at", IGNORE_DTF, 0},                                                                                                                                                                           
    {"aug", MONTH, 8}, {"august", MONTH, 8}, {DB_C, ADBC, BC},                                                                                                                                                                                                    
    {"d", UNITS, DTK_DAY},                                                                                                                                                                                                                                                 
    {"dec", MONTH, 12}, {"december", MONTH, 12}, {"dow", UNITS, DTK_DOW},                                                                                                                                                                                 
    {"doy", UNITS, DTK_DOY},                                                                                                                                                                                                                              
    {"dst", DTZMOD, SECS_PER_HOUR}, {EPOCH, RESERV, DTK_EPOCH},                                                                                                                                                                                                                      
    {"feb", MONTH, 2}, {"february", MONTH, 2}, {"fri", DOW, 5}, {"friday", DOW, 5}, {"h", UNITS, DTK_HOUR},                                                                                                                                          
    {LATE, RESERV, DTK_LATE},                                                                                                                                                                                                                                                     
    {"isodow", UNITS, DTK_ISODOW},                                                                                                                                                                                                                                         
    {"isoyear", UNITS, DTK_ISOYEAR},                                                                                                                                                                                                                                             
    {"j", UNITS, DTK_JULIAN}, {"jan", MONTH, 1}, {"january", MONTH, 1}, {"jd", UNITS, DTK_JULIAN}, {"jul", MONTH, 7}, {"julian", UNITS, DTK_JULIAN}, {"july", MONTH, 7}, {"jun", MONTH, 6}, {"june", MONTH, 6}, {"m", UNITS, DTK_MONTH},                            
    {"mar", MONTH, 3}, {"march", MONTH, 3}, {"may", MONTH, 5}, {"mm", UNITS, DTK_MINUTE},                                                                                                                                                                            
    {"mon", DOW, 1}, {"monday", DOW, 1}, {"nov", MONTH, 11}, {"november", MONTH, 11}, {NOW, RESERV, DTK_NOW},                                                                                                                                                          
    {"oct", MONTH, 10}, {"october", MONTH, 10}, {"on", IGNORE_DTF, 0},                                                                                                                                                                                         
    {"pm", AMPM, PM}, {"s", UNITS, DTK_SECOND},                                                                                                                                                                                                                       
    {"sat", DOW, 6}, {"saturday", DOW, 6}, {"sep", MONTH, 9}, {"sept", MONTH, 9}, {"september", MONTH, 9}, {"sun", DOW, 0}, {"sunday", DOW, 0}, {"t", ISOTIME, DTK_TIME},                                                                                                
    {"thu", DOW, 4}, {"thur", DOW, 4}, {"thurs", DOW, 4}, {"thursday", DOW, 4}, {TODAY, RESERV, DTK_TODAY},                                                                                                                                            
    {TOMORROW, RESERV, DTK_TOMORROW},                                                                                                                                                                                                                           
    {"tue", DOW, 2}, {"tues", DOW, 2}, {"tuesday", DOW, 2}, {"wed", DOW, 3}, {"wednesday", DOW, 3}, {"weds", DOW, 3}, {"y", UNITS, DTK_YEAR},                                                                                                                      
    {YESTERDAY, RESERV, DTK_YESTERDAY}                                                                                                                                                                                                                           
};

static const int szdatetktbl = sizeof datetktbl / sizeof datetktbl[0];

   
                                                                              
                                                    
   
static const datetkn deltatktbl[] = {
                            
    {"@", IGNORE_DTF, 0},                                                                                                                                                                                                                                     
    {DAGO, AGO, 0},                                                                                                                                                                                                                                                       
    {"c", UNITS, DTK_CENTURY},                                                                                                                                                                                                                          
    {"cent", UNITS, DTK_CENTURY},                                                                                                                                                                                                                       
    {"centuries", UNITS, DTK_CENTURY},                                                                                                                                                                                                                    
    {DCENTURY, UNITS, DTK_CENTURY},                                                                                                                                                                                                                     
    {"d", UNITS, DTK_DAY},                                                                                                                                                                                                                          
    {DDAY, UNITS, DTK_DAY},                                                                                                                                                                                                                         
    {"days", UNITS, DTK_DAY},                                                                                                                                                                                                                        
    {"dec", UNITS, DTK_DECADE},                                                                                                                                                                                                                        
    {DDECADE, UNITS, DTK_DECADE},                                                                                                                                                                                                                      
    {"decades", UNITS, DTK_DECADE},                                                                                                                                                                                                                     
    {"decs", UNITS, DTK_DECADE},                                                                                                                                                                                                                        
    {"h", UNITS, DTK_HOUR},                                                                                                                                                                                                                          
    {DHOUR, UNITS, DTK_HOUR},                                                                                                                                                                                                                        
    {"hours", UNITS, DTK_HOUR},                                                                                                                                                                                                                       
    {"hr", UNITS, DTK_HOUR},                                                                                                                                                                                                                         
    {"hrs", UNITS, DTK_HOUR},                                                                                                                                                                                                                         
    {"m", UNITS, DTK_MINUTE},                                                                                                                                                                                                                          
    {"microsecon", UNITS, DTK_MICROSEC},                                                                                                                                                                                                                    
    {"mil", UNITS, DTK_MILLENNIUM},                                                                                                                                                                                                                        
    {"millennia", UNITS, DTK_MILLENNIUM},                                                                                                                                                                                                                 
    {DMILLENNIUM, UNITS, DTK_MILLENNIUM},                                                                                                                                                                                                                  
    {"millisecon", UNITS, DTK_MILLISEC},                                                                                                                                                                                                      
    {"mils", UNITS, DTK_MILLENNIUM},                                                                                                                                                                                                                      
    {"min", UNITS, DTK_MINUTE},                                                                                                                                                                                                                        
    {"mins", UNITS, DTK_MINUTE},                                                                                                                                                                                                                        
    {DMINUTE, UNITS, DTK_MINUTE},                                                                                                                                                                                                                      
    {"minutes", UNITS, DTK_MINUTE},                                                                                                                                                                                                                     
    {"mon", UNITS, DTK_MONTH},                                                                                                                                                                                                                         
    {"mons", UNITS, DTK_MONTH},                                                                                                                                                                                                                        
    {DMONTH, UNITS, DTK_MONTH},                                                                                                                                                                                                                       
    {"months", UNITS, DTK_MONTH}, {"ms", UNITS, DTK_MILLISEC}, {"msec", UNITS, DTK_MILLISEC}, {DMILLISEC, UNITS, DTK_MILLISEC}, {"mseconds", UNITS, DTK_MILLISEC}, {"msecs", UNITS, DTK_MILLISEC}, {"qtr", UNITS, DTK_QUARTER},                         
    {DQUARTER, UNITS, DTK_QUARTER},                                                                                                                                                                                                                     
    {"s", UNITS, DTK_SECOND}, {"sec", UNITS, DTK_SECOND}, {DSECOND, UNITS, DTK_SECOND}, {"seconds", UNITS, DTK_SECOND}, {"secs", UNITS, DTK_SECOND}, {DTIMEZONE, UNITS, DTK_TZ},                                                                            
    {"timezone_h", UNITS, DTK_TZ_HOUR},                                                                                                                                                                                                                  
    {"timezone_m", UNITS, DTK_TZ_MINUTE},                                                                                                                                                                                                                   
    {"us", UNITS, DTK_MICROSEC},                                                                                                                                                                                                                            
    {"usec", UNITS, DTK_MICROSEC},                                                                                                                                                                                                                          
    {DMICROSEC, UNITS, DTK_MICROSEC},                                                                                                                                                                                                                       
    {"useconds", UNITS, DTK_MICROSEC},                                                                                                                                                                                                                       
    {"usecs", UNITS, DTK_MICROSEC},                                                                                                                                                                                                                          
    {"w", UNITS, DTK_WEEK},                                                                                                                                                                                                                          
    {DWEEK, UNITS, DTK_WEEK},                                                                                                                                                                                                                        
    {"weeks", UNITS, DTK_WEEK},                                                                                                                                                                                                                       
    {"y", UNITS, DTK_YEAR},                                                                                                                                                                                                                          
    {DYEAR, UNITS, DTK_YEAR},                                                                                                                                                                                                                        
    {"years", UNITS, DTK_YEAR},                                                                                                                                                                                                                       
    {"yr", UNITS, DTK_YEAR},                                                                                                                                                                                                                         
    {"yrs", UNITS, DTK_YEAR}                                                                                                                                                                                                                          
};

static const int szdeltatktbl = sizeof deltatktbl / sizeof deltatktbl[0];

static TimeZoneAbbrevTable *zoneabbrevtbl = NULL;

                                                         

static const datetkn *datecache[MAXDATEFIELDS] = {NULL};

static const datetkn *deltacache[MAXDATEFIELDS] = {NULL};

static const datetkn *abbrevcache[MAXDATEFIELDS] = {NULL};

   
                                             
                                                              
                                                                
                                                                  
                                                      
                                 
   
                                                                 
                                                             
                                                              
                                                                 
                       
   
                                                                  
                                                                    
                                                                      
                                             
   

int
date2j(int y, int m, int d)
{
  int julian;
  int century;

  if (m > 2)
  {
    m += 1;
    y += 4800;
  }
  else
  {
    m += 13;
    y += 4799;
  }

  century = y / 100;
  julian = y * 365 - 32167;
  julian += y / 4 - century + century / 4;
  julian += 7834 * m / 256 + d;

  return julian;
}               

void
j2date(int jd, int *year, int *month, int *day)
{
  unsigned int julian;
  unsigned int quad;
  unsigned int extra;
  int y;

  julian = jd;
  julian += 32044;
  quad = julian / 146097;
  extra = (julian - quad * 146097) * 4 + 3;
  julian += 60 + quad * 3 + extra / 146097;
  quad = julian / 1461;
  julian -= quad * 1461;
  y = julian * 4 / 1461;
  julian = ((y != 0) ? ((julian + 305) % 365) : ((julian + 306) % 366)) + 123;
  y += quad * 4;
  *year = y - 4800;
  quad = julian * 2141 / 65536;
  *day = julian - 7834 * quad / 256;
  *month = (quad + 10) % MONTHS_PER_YEAR + 1;

  return;
}               

   
                                                                 
   
                                                                      
                                                                         
                                                                            
   
int
j2day(int date)
{
  date += 1;
  date %= 7;
                                                                    
  if (date < 0)
  {
    date += 7;
  }

  return date;
}              

   
                        
   
                                                                           
   
void
GetCurrentDateTime(struct pg_tm *tm)
{
  int tz;
  fsec_t fsec;

  timestamp2tm(GetCurrentTransactionStartTimestamp(), &tz, tm, &fsec, NULL, NULL);
                                                                   
}

   
                        
   
                                                                           
                                                     
   
void
GetCurrentTimeUsec(struct pg_tm *tm, fsec_t *fsec, int *tzp)
{
  int tz;

  timestamp2tm(GetCurrentTransactionStartTimestamp(), &tz, tm, fsec, NULL, NULL);
                                                                   
  if (tzp != NULL)
  {
    *tzp = tz;
  }
}

   
                                                          
   
                                                                     
                                       
   
                                                                         
                                                                      
   
                                                                 
   
static char *
AppendSeconds(char *cp, int sec, fsec_t fsec, int precision, bool fillzeros)
{
  Assert(precision >= 0);

  if (fillzeros)
  {
    cp = pg_ltostr_zeropad(cp, Abs(sec), 2);
  }
  else
  {
    cp = pg_ltostr(cp, Abs(sec));
  }

                               
  if (fsec != 0)
  {
    int32 value = Abs(fsec);
    char *end = &cp[precision + 1];
    bool gotnonzero = false;

    *cp++ = '.';

       
                                                                        
                                                                          
                                                                       
       
    while (precision--)
    {
      int32 oldval = value;
      int32 remainder;

      value /= 10;
      remainder = oldval - value * 10;

                                      
      if (remainder)
      {
        gotnonzero = true;
      }

      if (gotnonzero)
      {
        cp[precision] = '0' + remainder;
      }
      else
      {
        end = &cp[precision];
      }
    }

       
                                                                           
                                                                        
                                                                        
       
    if (value)
    {
      return pg_ltostr(cp, Abs(fsec));
    }

    return end;
  }
  else
  {
    return cp;
  }
}

   
                                                          
   
                                                                         
                                                                      
   
static char *
AppendTimestampSeconds(char *cp, struct pg_tm *tm, fsec_t fsec)
{
  return AppendSeconds(cp, tm->tm_sec, fsec, MAX_TIMESTAMP_PRECISION, true);
}

   
                                                                       
                                                                        
   
static void
AdjustFractSeconds(double frac, struct pg_tm *tm, fsec_t *fsec, int scale)
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
AdjustFractDays(double frac, struct pg_tm *tm, fsec_t *fsec, int scale)
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
ParseFractionalSecond(char *cp, fsec_t *fsec)
{
  double frac;

                                                                
  Assert(*cp == '.');
  errno = 0;
  frac = strtod(cp, &cp);
                               
  if (*cp != '\0' || errno != 0)
  {
    return DTERR_BAD_FORMAT;
  }
  *fsec = rint(frac * 1000000);
  return 0;
}

                   
                                                          
                                                                
   
                              
                                                              
                                                                  
                                                                  
                                
                                                                  
                                                              
                                                  
                                                            
   
                                                               
                                                                    
                            
   
                                     
                                                      
                                                            
                                                                     
                                                
                                                     
                                                                            
   
                                                         
                                            
                                                             
                                                               
   
int
ParseDateTime(const char *timestr, char *workbuf, size_t buflen, char **field, int *ftype, int maxfields, int *numfields)
{
  int nf = 0;
  const char *cp = timestr;
  char *bufp = workbuf;
  const char *bufend = workbuf + buflen;

     
                                                                          
                                                                          
                                                                           
                                  
     
#define APPEND_CHAR(bufptr, end, newchar)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (((bufptr) + 1) >= (end))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      return DTERR_BAD_FORMAT;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    *(bufptr)++ = newchar;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  } while (0)

                                 
  while (*cp != '\0')
  {
                                      
    if (isspace((unsigned char)*cp))
    {
      cp++;
      continue;
    }

                                       
    if (nf >= maxfields)
    {
      return DTERR_BAD_FORMAT;
    }
    field[nf] = bufp;

                                          
    if (isdigit((unsigned char)*cp))
    {
      APPEND_CHAR(bufp, bufend, *cp++);
      while (isdigit((unsigned char)*cp))
      {
        APPEND_CHAR(bufp, bufend, *cp++);
      }

                       
      if (*cp == ':')
      {
        ftype[nf] = DTK_TIME;
        APPEND_CHAR(bufp, bufend, *cp++);
        while (isdigit((unsigned char)*cp) || (*cp == ':') || (*cp == '.'))
        {
          APPEND_CHAR(bufp, bufend, *cp++);
        }
      }
                                                 
      else if (*cp == '-' || *cp == '/' || *cp == '.')
      {
                                                    
        char delim = *cp;

        APPEND_CHAR(bufp, bufend, *cp++);
                                                                     
        if (isdigit((unsigned char)*cp))
        {
          ftype[nf] = ((delim == '.') ? DTK_NUMBER : DTK_DATE);
          while (isdigit((unsigned char)*cp))
          {
            APPEND_CHAR(bufp, bufend, *cp++);
          }

             
                                                                   
                   
             
          if (*cp == delim)
          {
            ftype[nf] = DTK_DATE;
            APPEND_CHAR(bufp, bufend, *cp++);
            while (isdigit((unsigned char)*cp) || *cp == delim)
            {
              APPEND_CHAR(bufp, bufend, *cp++);
            }
          }
        }
        else
        {
          ftype[nf] = DTK_DATE;
          while (isalnum((unsigned char)*cp) || *cp == delim)
          {
            APPEND_CHAR(bufp, bufend, pg_tolower((unsigned char)*cp++));
          }
        }
      }

         
                                                                        
                                      
         
      else
      {
        ftype[nf] = DTK_NUMBER;
      }
    }
                                                           
    else if (*cp == '.')
    {
      APPEND_CHAR(bufp, bufend, *cp++);
      while (isdigit((unsigned char)*cp))
      {
        APPEND_CHAR(bufp, bufend, *cp++);
      }

      ftype[nf] = DTK_NUMBER;
    }

       
                                                                        
       
    else if (isalpha((unsigned char)*cp))
    {
      bool is_date;

      ftype[nf] = DTK_STRING;
      APPEND_CHAR(bufp, bufend, pg_tolower((unsigned char)*cp++));
      while (isalpha((unsigned char)*cp))
      {
        APPEND_CHAR(bufp, bufend, pg_tolower((unsigned char)*cp++));
      }

         
                                                                        
                                                                         
                                                                        
                                                                         
                                                                        
                                                             
         
      is_date = false;
      if (*cp == '-' || *cp == '/' || *cp == '.')
      {
        is_date = true;
      }
      else if (*cp == '+' || isdigit((unsigned char)*cp))
      {
        *bufp = '\0';                                         
                                                                    
        if (datebsearch(field[nf], datetktbl, szdatetktbl) == NULL)
        {
          is_date = true;
        }
      }
      if (is_date)
      {
        ftype[nf] = DTK_DATE;
        do
        {
          APPEND_CHAR(bufp, bufend, pg_tolower((unsigned char)*cp++));
        } while (*cp == '+' || *cp == '-' || *cp == '/' || *cp == '_' || *cp == '.' || *cp == ':' || isalnum((unsigned char)*cp));
      }
    }
                                                
    else if (*cp == '+' || *cp == '-')
    {
      APPEND_CHAR(bufp, bufend, *cp++);
                                      
      while (isspace((unsigned char)*cp))
      {
        cp++;
      }
                             
                                                                      
      if (isdigit((unsigned char)*cp))
      {
        ftype[nf] = DTK_TZ;
        APPEND_CHAR(bufp, bufend, *cp++);
        while (isdigit((unsigned char)*cp) || *cp == ':' || *cp == '.' || *cp == '-')
        {
          APPEND_CHAR(bufp, bufend, *cp++);
        }
      }
                    
      else if (isalpha((unsigned char)*cp))
      {
        ftype[nf] = DTK_SPECIAL;
        APPEND_CHAR(bufp, bufend, pg_tolower((unsigned char)*cp++));
        while (isalpha((unsigned char)*cp))
        {
          APPEND_CHAR(bufp, bufend, pg_tolower((unsigned char)*cp++));
        }
      }
                                        
      else
      {
        return DTERR_BAD_FORMAT;
      }
    }
                                                       
    else if (ispunct((unsigned char)*cp))
    {
      cp++;
      continue;
    }
                                              
    else
    {
      return DTERR_BAD_FORMAT;
    }

                                               
    *bufp++ = '\0';
    nf++;
  }

  *numfields = nf;

  return 0;
}

                    
                                                                 
                                                                               
                                                            
   
                        
                                                                
                                
                            
                          
                          
                                               
                                         
                      
                     
                               
   
                                                                  
                                         
   
                                                                              
                                                                            
              
   
int
DecodeDateTime(char **field, int *ftype, int nf, int *dtype, struct pg_tm *tm, fsec_t *fsec, int *tzp)
{
  int fmask = 0, tmask, type;
  int ptype = 0;                                               
  int i;
  int val;
  int dterr;
  int mer = HR24;
  bool haveTextMonth = false;
  bool isjulian = false;
  bool is2digits = false;
  bool bc = false;
  pg_tz *namedTz = NULL;
  pg_tz *abbrevTz = NULL;
  pg_tz *valtz;
  char *abbrev = NULL;
  struct pg_tm cur_tm;

     
                                                                         
                                                        
     
  *dtype = DTK_DATE;
  tm->tm_hour = 0;
  tm->tm_min = 0;
  tm->tm_sec = 0;
  *fsec = 0;
                                                       
  tm->tm_isdst = -1;
  if (tzp != NULL)
  {
    *tzp = 0;
  }

  for (i = 0; i < nf; i++)
  {
    switch (ftype[i])
    {
    case DTK_DATE:

         
                                                                
                                                                     
                                     
         
      if (ptype == DTK_JULIAN)
      {
        char *cp;
        int val;

        if (tzp == NULL)
        {
          return DTERR_BAD_FORMAT;
        }

        errno = 0;
        val = strtoint(field[i], &cp, 10);
        if (errno == ERANGE || val < 0)
        {
          return DTERR_FIELD_OVERFLOW;
        }

        j2date(val, &tm->tm_year, &tm->tm_mon, &tm->tm_mday);
        isjulian = true;

                                                          
        dterr = DecodeTimezone(cp, tzp);
        if (dterr)
        {
          return dterr;
        }

        tmask = DTK_DATE_M | DTK_TIME_M | DTK_M(TZ);
        ptype = 0;
        break;
      }

         
                                                                  
                                                                  
                                                                     
                             
         
                                                                    
                                                                  
                                      
         
      else if (ptype != 0 || ((fmask & (DTK_M(MONTH) | DTK_M(DAY))) == (DTK_M(MONTH) | DTK_M(DAY))))
      {
                                                 
        if (tzp == NULL)
        {
          return DTERR_BAD_FORMAT;
        }

        if (isdigit((unsigned char)*field[i]) || ptype != 0)
        {
          char *cp;

          if (ptype != 0)
          {
                                                         
            if (ptype != DTK_TIME)
            {
              return DTERR_BAD_FORMAT;
            }
            ptype = 0;
          }

             
                                                            
                                                                
                        
             
          if ((fmask & DTK_TIME_M) == DTK_TIME_M)
          {
            return DTERR_BAD_FORMAT;
          }

          if ((cp = strchr(field[i], '-')) == NULL)
          {
            return DTERR_BAD_FORMAT;
          }

                                                            
          dterr = DecodeTimezone(cp, tzp);
          if (dterr)
          {
            return dterr;
          }
          *cp = '\0';

             
                                                               
                  
             
          dterr = DecodeNumberField(strlen(field[i]), field[i], fmask, &tmask, tm, fsec, &is2digits);
          if (dterr < 0)
          {
            return dterr;
          }

             
                                               
                                 
             
          tmask |= DTK_M(TZ);
        }
        else
        {
          namedTz = pg_tzset(field[i]);
          if (!namedTz)
          {
               
                                                         
                                                              
                                                 
               
            ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("time zone \"%s\" not recognized", field[i])));
          }
                                                  
          tmask = DTK_M(TZ);
        }
      }
      else
      {
        dterr = DecodeDate(field[i], fmask, &tmask, &is2digits, tm);
        if (dterr)
        {
          return dterr;
        }
      }
      break;

    case DTK_TIME:

         
                                                          
         
      if (ptype != 0)
      {
                                                     
        if (ptype != DTK_TIME)
        {
          return DTERR_BAD_FORMAT;
        }
        ptype = 0;
      }
      dterr = DecodeTime(field[i], fmask, INTERVAL_FULL_RANGE, &tmask, tm, fsec);
      if (dterr)
      {
        return dterr;
      }

                                   
      if (time_overflows(tm->tm_hour, tm->tm_min, tm->tm_sec, *fsec))
      {
        return DTERR_FIELD_OVERFLOW;
      }
      break;

    case DTK_TZ:
    {
      int tz;

      if (tzp == NULL)
      {
        return DTERR_BAD_FORMAT;
      }

      dterr = DecodeTimezone(field[i], &tz);
      if (dterr)
      {
        return dterr;
      }
      *tzp = tz;
      tmask = DTK_M(TZ);
    }
    break;

    case DTK_NUMBER:

         
                                                               
                                                      
         
      if (ptype != 0)
      {
        char *cp;
        int val;

        errno = 0;
        val = strtoint(field[i], &cp, 10);
        if (errno == ERANGE)
        {
          return DTERR_FIELD_OVERFLOW;
        }

           
                                                            
                   
           
        if (*cp == '.')
        {
          switch (ptype)
          {
          case DTK_JULIAN:
          case DTK_TIME:
          case DTK_SECOND:
            break;
          default:
            return DTERR_BAD_FORMAT;
            break;
          }
        }
        else if (*cp != '\0')
        {
          return DTERR_BAD_FORMAT;
        }

        switch (ptype)
        {
        case DTK_YEAR:
          tm->tm_year = val;
          tmask = DTK_M(YEAR);
          break;

        case DTK_MONTH:

             
                                                        
                     
             
          if ((fmask & DTK_M(MONTH)) != 0 && (fmask & DTK_M(HOUR)) != 0)
          {
            tm->tm_min = val;
            tmask = DTK_M(MINUTE);
          }
          else
          {
            tm->tm_mon = val;
            tmask = DTK_M(MONTH);
          }
          break;

        case DTK_DAY:
          tm->tm_mday = val;
          tmask = DTK_M(DAY);
          break;

        case DTK_HOUR:
          tm->tm_hour = val;
          tmask = DTK_M(HOUR);
          break;

        case DTK_MINUTE:
          tm->tm_min = val;
          tmask = DTK_M(MINUTE);
          break;

        case DTK_SECOND:
          tm->tm_sec = val;
          tmask = DTK_M(SECOND);
          if (*cp == '.')
          {
            dterr = ParseFractionalSecond(cp, fsec);
            if (dterr)
            {
              return dterr;
            }
            tmask = DTK_ALL_SECS_M;
          }
          break;

        case DTK_TZ:
          tmask = DTK_M(TZ);
          dterr = DecodeTimezone(field[i], tzp);
          if (dterr)
          {
            return dterr;
          }
          break;

        case DTK_JULIAN:
                                                            
          if (val < 0)
          {
            return DTERR_FIELD_OVERFLOW;
          }
          tmask = DTK_DATE_M;
          j2date(val, &tm->tm_year, &tm->tm_mon, &tm->tm_mday);
          isjulian = true;

                                      
          if (*cp == '.')
          {
            double time;

            errno = 0;
            time = strtod(cp, &cp);
            if (*cp != '\0' || errno != 0)
            {
              return DTERR_BAD_FORMAT;
            }
            time *= USECS_PER_DAY;
            dt2time(time, &tm->tm_hour, &tm->tm_min, &tm->tm_sec, fsec);
            tmask |= DTK_TIME_M;
          }
          break;

        case DTK_TIME:
                                                   
          dterr = DecodeNumberField(strlen(field[i]), field[i], (fmask | DTK_DATE_M), &tmask, tm, fsec, &is2digits);
          if (dterr < 0)
          {
            return dterr;
          }
          if (tmask != DTK_TIME_M)
          {
            return DTERR_BAD_FORMAT;
          }
          break;

        default:
          return DTERR_BAD_FORMAT;
          break;
        }

        ptype = 0;
        *dtype = DTK_DATE;
      }
      else
      {
        char *cp;
        int flen;

        flen = strlen(field[i]);
        cp = strchr(field[i], '.');

                                               
        if (cp != NULL && !(fmask & DTK_DATE_M))
        {
          dterr = DecodeDate(field[i], fmask, &tmask, &is2digits, tm);
          if (dterr)
          {
            return dterr;
          }
        }
                                                         
        else if (cp != NULL && flen - strlen(cp) > 2)
        {
             
                                                              
                                                              
                                         
             
          dterr = DecodeNumberField(flen, field[i], fmask, &tmask, tm, fsec, &is2digits);
          if (dterr < 0)
          {
            return dterr;
          }
        }

           
                                                                 
                                                                 
                                                                  
                                                                 
                                                               
                                                                 
                                       
           
        else if (flen >= 6 && (!(fmask & DTK_DATE_M) || !(fmask & DTK_TIME_M)))
        {
          dterr = DecodeNumberField(flen, field[i], fmask, &tmask, tm, fsec, &is2digits);
          if (dterr < 0)
          {
            return dterr;
          }
        }
                                                         
        else
        {
          dterr = DecodeNumber(flen, field[i], haveTextMonth, fmask, &tmask, tm, fsec, &is2digits);
          if (dterr)
          {
            return dterr;
          }
        }
      }
      break;

    case DTK_STRING:
    case DTK_SPECIAL:
                                                                 
      type = DecodeTimezoneAbbrev(i, field[i], &val, &valtz);
      if (type == UNKNOWN_FIELD)
      {
        type = DecodeSpecial(i, field[i], &val);
      }
      if (type == IGNORE_DTF)
      {
        continue;
      }

      tmask = DTK_M(type);
      switch (type)
      {
      case RESERV:
        switch (val)
        {
        case DTK_NOW:
          tmask = (DTK_DATE_M | DTK_TIME_M | DTK_M(TZ));
          *dtype = DTK_DATE;
          GetCurrentTimeUsec(tm, fsec, tzp);
          break;

        case DTK_YESTERDAY:
          tmask = DTK_DATE_M;
          *dtype = DTK_DATE;
          GetCurrentDateTime(&cur_tm);
          j2date(date2j(cur_tm.tm_year, cur_tm.tm_mon, cur_tm.tm_mday) - 1, &tm->tm_year, &tm->tm_mon, &tm->tm_mday);
          break;

        case DTK_TODAY:
          tmask = DTK_DATE_M;
          *dtype = DTK_DATE;
          GetCurrentDateTime(&cur_tm);
          tm->tm_year = cur_tm.tm_year;
          tm->tm_mon = cur_tm.tm_mon;
          tm->tm_mday = cur_tm.tm_mday;
          break;

        case DTK_TOMORROW:
          tmask = DTK_DATE_M;
          *dtype = DTK_DATE;
          GetCurrentDateTime(&cur_tm);
          j2date(date2j(cur_tm.tm_year, cur_tm.tm_mon, cur_tm.tm_mday) + 1, &tm->tm_year, &tm->tm_mon, &tm->tm_mday);
          break;

        case DTK_ZULU:
          tmask = (DTK_TIME_M | DTK_M(TZ));
          *dtype = DTK_DATE;
          tm->tm_hour = 0;
          tm->tm_min = 0;
          tm->tm_sec = 0;
          if (tzp != NULL)
          {
            *tzp = 0;
          }
          break;

        default:
          *dtype = val;
        }

        break;

      case MONTH:

           
                                                              
                         
           
        if ((fmask & DTK_M(MONTH)) && !haveTextMonth && !(fmask & DTK_M(DAY)) && tm->tm_mon >= 1 && tm->tm_mon <= 31)
        {
          tm->tm_mday = tm->tm_mon;
          tmask = DTK_M(DAY);
        }
        haveTextMonth = true;
        tm->tm_mon = val;
        break;

      case DTZMOD:

           
                                                            
                   
           
        tmask |= DTK_M(DTZ);
        tm->tm_isdst = 1;
        if (tzp == NULL)
        {
          return DTERR_BAD_FORMAT;
        }
        *tzp -= val;
        break;

      case DTZ:

           
                                                              
                                    
           
        tmask |= DTK_M(TZ);
        tm->tm_isdst = 1;
        if (tzp == NULL)
        {
          return DTERR_BAD_FORMAT;
        }
        *tzp = -val;
        break;

      case TZ:
        tm->tm_isdst = 0;
        if (tzp == NULL)
        {
          return DTERR_BAD_FORMAT;
        }
        *tzp = -val;
        break;

      case DYNTZ:
        tmask |= DTK_M(TZ);
        if (tzp == NULL)
        {
          return DTERR_BAD_FORMAT;
        }
                                                     
        abbrevTz = valtz;
        abbrev = field[i];
        break;

      case AMPM:
        mer = val;
        break;

      case ADBC:
        bc = (val == BC);
        break;

      case DOW:
        tm->tm_wday = val;
        break;

      case UNITS:
        tmask = 0;
        ptype = val;
        break;

      case ISOTIME:

           
                                                               
                                                               
           
        tmask = 0;

                                             
        if ((fmask & DTK_DATE_M) != DTK_DATE_M)
        {
          return DTERR_BAD_FORMAT;
        }

             
                                                     
                                           
                                           
                                        
             
        if (i >= nf - 1 || (ftype[i + 1] != DTK_NUMBER && ftype[i + 1] != DTK_TIME && ftype[i + 1] != DTK_DATE))
        {
          return DTERR_BAD_FORMAT;
        }

        ptype = val;
        break;

      case UNKNOWN_FIELD:

           
                                                              
                                                
           
        namedTz = pg_tzset(field[i]);
        if (!namedTz)
        {
          return DTERR_BAD_FORMAT;
        }
                                                
        tmask = DTK_M(TZ);
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

                                                    
  dterr = ValidateDate(fmask, isjulian, is2digits, bc, tm);
  if (dterr)
  {
    return dterr;
  }

                    
  if (mer != HR24 && tm->tm_hour > HOURS_PER_DAY / 2)
  {
    return DTERR_FIELD_OVERFLOW;
  }
  if (mer == AM && tm->tm_hour == HOURS_PER_DAY / 2)
  {
    tm->tm_hour = 0;
  }
  else if (mer == PM && tm->tm_hour != HOURS_PER_DAY / 2)
  {
    tm->tm_hour += HOURS_PER_DAY / 2;
  }

                                                     
  if (*dtype == DTK_DATE)
  {
    if ((fmask & DTK_DATE_M) != DTK_DATE_M)
    {
      if ((fmask & DTK_TIME_M) == DTK_TIME_M)
      {
        return 1;
      }
      return DTERR_BAD_FORMAT;
    }

       
                                                                           
                                                                   
       
    if (namedTz != NULL)
    {
                                                                  
      if (fmask & DTK_M(DTZMOD))
      {
        return DTERR_BAD_FORMAT;
      }

      *tzp = DetermineTimeZoneOffset(tm, namedTz);
    }

       
                                                                       
            
       
    if (abbrevTz != NULL)
    {
                                                                     
      if (fmask & DTK_M(DTZMOD))
      {
        return DTERR_BAD_FORMAT;
      }

      *tzp = DetermineTimeZoneAbbrevOffset(tm, abbrev, abbrevTz);
    }

                                                           
    if (tzp != NULL && !(fmask & DTK_M(TZ)))
    {
         
                                                                       
               
         
      if (fmask & DTK_M(DTZMOD))
      {
        return DTERR_BAD_FORMAT;
      }

      *tzp = DetermineTimeZoneOffset(tm, session_timezone);
    }
  }

  return 0;
}

                             
   
                                                                            
                                                                              
                                                                       
                                                                         
                                  
   
                                                                          
                                                                          
                                                
   
int
DetermineTimeZoneOffset(struct pg_tm *tm, pg_tz *tzp)
{
  pg_time_t t;

  return DetermineTimeZoneOffsetInternal(tm, tzp, &t);
}

                                     
   
                                                                          
             
   
                                                                         
                                                                       
                                           
   
                                                                        
                                                                              
                        
   
static int
DetermineTimeZoneOffsetInternal(struct pg_tm *tm, pg_tz *tzp, pg_time_t *tp)
{
  int date, sec;
  pg_time_t day, mytime, prevtime, boundary, beforetime, aftertime;
  long int before_gmtoff, after_gmtoff;
  int before_isdst, after_isdst;
  int res;

     
                                                                    
                                                                            
                                                                            
                                                                    
     
  if (!IS_VALID_JULIAN(tm->tm_year, tm->tm_mon, tm->tm_mday))
  {
    goto overflow;
  }
  date = date2j(tm->tm_year, tm->tm_mon, tm->tm_mday) - UNIX_EPOCH_JDATE;

  day = ((pg_time_t)date) * SECS_PER_DAY;
  if (day / SECS_PER_DAY != date)
  {
    goto overflow;
  }
  sec = tm->tm_sec + (tm->tm_min + tm->tm_hour * MINS_PER_HOUR) * SECS_PER_MINUTE;
  mytime = day + sec;
                                                                   
  if (mytime < 0 && day > 0)
  {
    goto overflow;
  }

     
                                                                             
                                                                             
                                                                         
                                                      
     
  prevtime = mytime - SECS_PER_DAY;
  if (mytime < 0 && prevtime > 0)
  {
    goto overflow;
  }

  res = pg_next_dst_boundary(&prevtime, &before_gmtoff, &before_isdst, &boundary, &after_gmtoff, &after_isdst, tzp);
  if (res < 0)
  {
    goto overflow;               
  }

  if (res == 0)
  {
                                      
    tm->tm_isdst = before_isdst;
    *tp = mytime - before_gmtoff;
    return -(int)before_gmtoff;
  }

     
                                                                    
     
  beforetime = mytime - before_gmtoff;
  if ((before_gmtoff > 0 && mytime < 0 && beforetime > 0) || (before_gmtoff <= 0 && mytime > 0 && beforetime < 0))
  {
    goto overflow;
  }
  aftertime = mytime - after_gmtoff;
  if ((after_gmtoff > 0 && mytime < 0 && aftertime > 0) || (after_gmtoff <= 0 && mytime > 0 && aftertime < 0))
  {
    goto overflow;
  }

     
                                                                             
                                                                          
                                                                   
     
  if (beforetime < boundary && aftertime < boundary)
  {
    tm->tm_isdst = before_isdst;
    *tp = beforetime;
    return -(int)before_gmtoff;
  }
  if (beforetime > boundary && aftertime >= boundary)
  {
    tm->tm_isdst = after_isdst;
    *tp = aftertime;
    return -(int)after_gmtoff;
  }

     
                                                                         
                                                                         
                                                                             
                                                                           
                                                                           
                                                                            
                                                                        
                                                                            
                                       
     
  if (beforetime > aftertime)
  {
    tm->tm_isdst = before_isdst;
    *tp = beforetime;
    return -(int)before_gmtoff;
  }
  tm->tm_isdst = after_isdst;
  *tp = aftertime;
  return -(int)after_gmtoff;

overflow:
                                                 
  tm->tm_isdst = 0;
  *tp = 0;
  return 0;
}

                                   
   
                                                                       
                                                                            
                                                                          
                                           
   
                                                                         
                                                                               
                                                                               
                                                                              
                                                                            
                                             
   
int
DetermineTimeZoneAbbrevOffset(struct pg_tm *tm, const char *abbr, pg_tz *tzp)
{
  pg_time_t t;
  int zone_offset;
  int abbr_offset;
  int abbr_isdst;

     
                                                                             
                                                                             
     
  zone_offset = DetermineTimeZoneOffsetInternal(tm, tzp, &t);

     
                                                                        
     
  if (DetermineTimeZoneAbbrevOffsetInternal(t, abbr, tzp, &abbr_offset, &abbr_isdst))
  {
                                                      
    tm->tm_isdst = abbr_isdst;
    return abbr_offset;
  }

     
                                                      
                                      
     
  return zone_offset;
}

                                     
   
                                                                                
                                                                         
   
int
DetermineTimeZoneAbbrevOffsetTS(TimestampTz ts, const char *abbr, pg_tz *tzp, int *isdst)
{
  pg_time_t t = timestamptz_to_time_t(ts);
  int zone_offset;
  int abbr_offset;
  int tz;
  struct pg_tm tm;
  fsec_t fsec;

     
                                                                           
     
  if (DetermineTimeZoneAbbrevOffsetInternal(t, abbr, tzp, &abbr_offset, isdst))
  {
    return abbr_offset;
  }

     
                                                                           
     
  if (timestamp2tm(ts, &tz, &tm, &fsec, NULL, tzp) != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
  }

  zone_offset = DetermineTimeZoneOffset(&tm, tzp);
  *isdst = tm.tm_isdst;
  return zone_offset;
}

                                           
   
                                                                           
                                                                         
   
static bool
DetermineTimeZoneAbbrevOffsetInternal(pg_time_t t, const char *abbr, pg_tz *tzp, int *offset, int *isdst)
{
  char upabbr[TZ_STRLEN_MAX + 1];
  unsigned char *p;
  long int gmtoff;

                                                 
  strlcpy(upabbr, abbr, sizeof(upabbr));
  for (p = (unsigned char *)upabbr; *p; p++)
  {
    *p = pg_toupper(*p);
  }

                                                              
  if (pg_interpret_timezone_abbrev(upabbr, &t, &gmtoff, isdst, tzp))
  {
                                                             
    *offset = (int)-gmtoff;
    return true;
  }
  return false;
}

                    
                                                
                                                                
   
                                               
                                           
                                                
                                                
                       
                                                    
                                                  
   
int
DecodeTimeOnly(char **field, int *ftype, int nf, int *dtype, struct pg_tm *tm, fsec_t *fsec, int *tzp)
{
  int fmask = 0, tmask, type;
  int ptype = 0;                                              
  int i;
  int val;
  int dterr;
  bool isjulian = false;
  bool is2digits = false;
  bool bc = false;
  int mer = HR24;
  pg_tz *namedTz = NULL;
  pg_tz *abbrevTz = NULL;
  char *abbrev = NULL;
  pg_tz *valtz;

  *dtype = DTK_TIME;
  tm->tm_hour = 0;
  tm->tm_min = 0;
  tm->tm_sec = 0;
  *fsec = 0;
                                                       
  tm->tm_isdst = -1;

  if (tzp != NULL)
  {
    *tzp = 0;
  }

  for (i = 0; i < nf; i++)
  {
    switch (ftype[i])
    {
    case DTK_DATE:

         
                                                                     
                                    
         
      if (tzp == NULL)
      {
        return DTERR_BAD_FORMAT;
      }

                                                                 
      if (i == 0 && nf >= 2 && (ftype[nf - 1] == DTK_DATE || ftype[1] == DTK_TIME))
      {
        dterr = DecodeDate(field[i], fmask, &tmask, &is2digits, tm);
        if (dterr)
        {
          return dterr;
        }
      }
                                                      
      else
      {
        if (isdigit((unsigned char)*field[i]))
        {
          char *cp;

             
                                                            
                                                                
             
          if ((fmask & DTK_TIME_M) == DTK_TIME_M)
          {
            return DTERR_BAD_FORMAT;
          }

             
                                                                
             
          if ((cp = strchr(field[i], '-')) == NULL)
          {
            return DTERR_BAD_FORMAT;
          }

                                                            
          dterr = DecodeTimezone(cp, tzp);
          if (dterr)
          {
            return dterr;
          }
          *cp = '\0';

             
                                                               
                  
             
          dterr = DecodeNumberField(strlen(field[i]), field[i], (fmask | DTK_DATE_M), &tmask, tm, fsec, &is2digits);
          if (dterr < 0)
          {
            return dterr;
          }
          ftype[i] = dterr;

          tmask |= DTK_M(TZ);
        }
        else
        {
          namedTz = pg_tzset(field[i]);
          if (!namedTz)
          {
               
                                                         
                                                              
                                                 
               
            ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("time zone \"%s\" not recognized", field[i])));
          }
                                                  
          ftype[i] = DTK_TZ;
          tmask = DTK_M(TZ);
        }
      }
      break;

    case DTK_TIME:
      dterr = DecodeTime(field[i], (fmask | DTK_DATE_M), INTERVAL_FULL_RANGE, &tmask, tm, fsec);
      if (dterr)
      {
        return dterr;
      }
      break;

    case DTK_TZ:
    {
      int tz;

      if (tzp == NULL)
      {
        return DTERR_BAD_FORMAT;
      }

      dterr = DecodeTimezone(field[i], &tz);
      if (dterr)
      {
        return dterr;
      }
      *tzp = tz;
      tmask = DTK_M(TZ);
    }
    break;

    case DTK_NUMBER:

         
                                                               
                                                    
         
      if (ptype != 0)
      {
        char *cp;
        int val;

                                                            
        switch (ptype)
        {
        case DTK_JULIAN:
        case DTK_YEAR:
        case DTK_MONTH:
        case DTK_DAY:
          if (tzp == NULL)
          {
            return DTERR_BAD_FORMAT;
          }
        default:
          break;
        }

        errno = 0;
        val = strtoint(field[i], &cp, 10);
        if (errno == ERANGE)
        {
          return DTERR_FIELD_OVERFLOW;
        }

           
                                                            
                   
           
        if (*cp == '.')
        {
          switch (ptype)
          {
          case DTK_JULIAN:
          case DTK_TIME:
          case DTK_SECOND:
            break;
          default:
            return DTERR_BAD_FORMAT;
            break;
          }
        }
        else if (*cp != '\0')
        {
          return DTERR_BAD_FORMAT;
        }

        switch (ptype)
        {
        case DTK_YEAR:
          tm->tm_year = val;
          tmask = DTK_M(YEAR);
          break;

        case DTK_MONTH:

             
                                                        
                     
             
          if ((fmask & DTK_M(MONTH)) != 0 && (fmask & DTK_M(HOUR)) != 0)
          {
            tm->tm_min = val;
            tmask = DTK_M(MINUTE);
          }
          else
          {
            tm->tm_mon = val;
            tmask = DTK_M(MONTH);
          }
          break;

        case DTK_DAY:
          tm->tm_mday = val;
          tmask = DTK_M(DAY);
          break;

        case DTK_HOUR:
          tm->tm_hour = val;
          tmask = DTK_M(HOUR);
          break;

        case DTK_MINUTE:
          tm->tm_min = val;
          tmask = DTK_M(MINUTE);
          break;

        case DTK_SECOND:
          tm->tm_sec = val;
          tmask = DTK_M(SECOND);
          if (*cp == '.')
          {
            dterr = ParseFractionalSecond(cp, fsec);
            if (dterr)
            {
              return dterr;
            }
            tmask = DTK_ALL_SECS_M;
          }
          break;

        case DTK_TZ:
          tmask = DTK_M(TZ);
          dterr = DecodeTimezone(field[i], tzp);
          if (dterr)
          {
            return dterr;
          }
          break;

        case DTK_JULIAN:
                                                            
          if (val < 0)
          {
            return DTERR_FIELD_OVERFLOW;
          }
          tmask = DTK_DATE_M;
          j2date(val, &tm->tm_year, &tm->tm_mon, &tm->tm_mday);
          isjulian = true;

          if (*cp == '.')
          {
            double time;

            errno = 0;
            time = strtod(cp, &cp);
            if (*cp != '\0' || errno != 0)
            {
              return DTERR_BAD_FORMAT;
            }
            time *= USECS_PER_DAY;
            dt2time(time, &tm->tm_hour, &tm->tm_min, &tm->tm_sec, fsec);
            tmask |= DTK_TIME_M;
          }
          break;

        case DTK_TIME:
                                                   
          dterr = DecodeNumberField(strlen(field[i]), field[i], (fmask | DTK_DATE_M), &tmask, tm, fsec, &is2digits);
          if (dterr < 0)
          {
            return dterr;
          }
          ftype[i] = dterr;

          if (tmask != DTK_TIME_M)
          {
            return DTERR_BAD_FORMAT;
          }
          break;

        default:
          return DTERR_BAD_FORMAT;
          break;
        }

        ptype = 0;
        *dtype = DTK_DATE;
      }
      else
      {
        char *cp;
        int flen;

        flen = strlen(field[i]);
        cp = strchr(field[i], '.');

                               
        if (cp != NULL)
        {
             
                                                           
                     
             
          if (i == 0 && nf >= 2 && ftype[nf - 1] == DTK_DATE)
          {
            dterr = DecodeDate(field[i], fmask, &tmask, &is2digits, tm);
            if (dterr)
            {
              return dterr;
            }
          }
                                                           
          else if (flen - strlen(cp) > 2)
          {
               
                                                            
                                                             
                                                  
               
            dterr = DecodeNumberField(flen, field[i], (fmask | DTK_DATE_M), &tmask, tm, fsec, &is2digits);
            if (dterr < 0)
            {
              return dterr;
            }
            ftype[i] = dterr;
          }
          else
          {
            return DTERR_BAD_FORMAT;
          }
        }
        else if (flen > 4)
        {
          dterr = DecodeNumberField(flen, field[i], (fmask | DTK_DATE_M), &tmask, tm, fsec, &is2digits);
          if (dterr < 0)
          {
            return dterr;
          }
          ftype[i] = dterr;
        }
                                                         
        else
        {
          dterr = DecodeNumber(flen, field[i], false, (fmask | DTK_DATE_M), &tmask, tm, fsec, &is2digits);
          if (dterr)
          {
            return dterr;
          }
        }
      }
      break;

    case DTK_STRING:
    case DTK_SPECIAL:
                                                                 
      type = DecodeTimezoneAbbrev(i, field[i], &val, &valtz);
      if (type == UNKNOWN_FIELD)
      {
        type = DecodeSpecial(i, field[i], &val);
      }
      if (type == IGNORE_DTF)
      {
        continue;
      }

      tmask = DTK_M(type);
      switch (type)
      {
      case RESERV:
        switch (val)
        {
        case DTK_NOW:
          tmask = DTK_TIME_M;
          *dtype = DTK_TIME;
          GetCurrentTimeUsec(tm, fsec, NULL);
          break;

        case DTK_ZULU:
          tmask = (DTK_TIME_M | DTK_M(TZ));
          *dtype = DTK_TIME;
          tm->tm_hour = 0;
          tm->tm_min = 0;
          tm->tm_sec = 0;
          tm->tm_isdst = 0;
          break;

        default:
          return DTERR_BAD_FORMAT;
        }

        break;

      case DTZMOD:

           
                                                            
                   
           
        tmask |= DTK_M(DTZ);
        tm->tm_isdst = 1;
        if (tzp == NULL)
        {
          return DTERR_BAD_FORMAT;
        }
        *tzp -= val;
        break;

      case DTZ:

           
                                                              
                                    
           
        tmask |= DTK_M(TZ);
        tm->tm_isdst = 1;
        if (tzp == NULL)
        {
          return DTERR_BAD_FORMAT;
        }
        *tzp = -val;
        ftype[i] = DTK_TZ;
        break;

      case TZ:
        tm->tm_isdst = 0;
        if (tzp == NULL)
        {
          return DTERR_BAD_FORMAT;
        }
        *tzp = -val;
        ftype[i] = DTK_TZ;
        break;

      case DYNTZ:
        tmask |= DTK_M(TZ);
        if (tzp == NULL)
        {
          return DTERR_BAD_FORMAT;
        }
                                                     
        abbrevTz = valtz;
        abbrev = field[i];
        ftype[i] = DTK_TZ;
        break;

      case AMPM:
        mer = val;
        break;

      case ADBC:
        bc = (val == BC);
        break;

      case UNITS:
        tmask = 0;
        ptype = val;
        break;

      case ISOTIME:
        tmask = 0;

             
                                                     
                                           
                                           
                                        
             
        if (i >= nf - 1 || (ftype[i + 1] != DTK_NUMBER && ftype[i + 1] != DTK_TIME && ftype[i + 1] != DTK_DATE))
        {
          return DTERR_BAD_FORMAT;
        }

        ptype = val;
        break;

      case UNKNOWN_FIELD:

           
                                                              
                                                
           
        namedTz = pg_tzset(field[i]);
        if (!namedTz)
        {
          return DTERR_BAD_FORMAT;
        }
                                                
        tmask = DTK_M(TZ);
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

                                                    
  dterr = ValidateDate(fmask, isjulian, is2digits, bc, tm);
  if (dterr)
  {
    return dterr;
  }

                    
  if (mer != HR24 && tm->tm_hour > HOURS_PER_DAY / 2)
  {
    return DTERR_FIELD_OVERFLOW;
  }
  if (mer == AM && tm->tm_hour == HOURS_PER_DAY / 2)
  {
    tm->tm_hour = 0;
  }
  else if (mer == PM && tm->tm_hour != HOURS_PER_DAY / 2)
  {
    tm->tm_hour += HOURS_PER_DAY / 2;
  }

                               
  if (time_overflows(tm->tm_hour, tm->tm_min, tm->tm_sec, *fsec))
  {
    return DTERR_FIELD_OVERFLOW;
  }

  if ((fmask & DTK_TIME_M) != DTK_TIME_M)
  {
    return DTERR_BAD_FORMAT;
  }

     
                                                                            
                                                                  
     
  if (namedTz != NULL)
  {
    long int gmtoff;

                                                                
    if (fmask & DTK_M(DTZMOD))
    {
      return DTERR_BAD_FORMAT;
    }

                                                          
    if (pg_get_timezone_offset(namedTz, &gmtoff))
    {
      *tzp = -(int)gmtoff;
    }
    else
    {
                                      
      if ((fmask & DTK_DATE_M) != DTK_DATE_M)
      {
        return DTERR_BAD_FORMAT;
      }
      *tzp = DetermineTimeZoneOffset(tm, namedTz);
    }
  }

     
                                                                          
     
  if (abbrevTz != NULL)
  {
    struct pg_tm tt, *tmp = &tt;

       
                                                                           
       
    if (fmask & DTK_M(DTZMOD))
    {
      return DTERR_BAD_FORMAT;
    }

    if ((fmask & DTK_DATE_M) == 0)
    {
      GetCurrentDateTime(tmp);
    }
    else
    {
                                      
      if ((fmask & DTK_DATE_M) != DTK_DATE_M)
      {
        return DTERR_BAD_FORMAT;
      }
      tmp->tm_year = tm->tm_year;
      tmp->tm_mon = tm->tm_mon;
      tmp->tm_mday = tm->tm_mday;
    }
    tmp->tm_hour = tm->tm_hour;
    tmp->tm_min = tm->tm_min;
    tmp->tm_sec = tm->tm_sec;
    *tzp = DetermineTimeZoneAbbrevOffset(tmp, abbrev, abbrevTz);
    tm->tm_isdst = tmp->tm_isdst;
  }

                                                         
  if (tzp != NULL && !(fmask & DTK_M(TZ)))
  {
    struct pg_tm tt, *tmp = &tt;

       
                                                                           
       
    if (fmask & DTK_M(DTZMOD))
    {
      return DTERR_BAD_FORMAT;
    }

    if ((fmask & DTK_DATE_M) == 0)
    {
      GetCurrentDateTime(tmp);
    }
    else
    {
                                      
      if ((fmask & DTK_DATE_M) != DTK_DATE_M)
      {
        return DTERR_BAD_FORMAT;
      }
      tmp->tm_year = tm->tm_year;
      tmp->tm_mon = tm->tm_mon;
      tmp->tm_mday = tm->tm_mday;
    }
    tmp->tm_hour = tm->tm_hour;
    tmp->tm_min = tm->tm_min;
    tmp->tm_sec = tm->tm_sec;
    *tzp = DetermineTimeZoneOffset(tmp, session_timezone);
    tm->tm_isdst = tmp->tm_isdst;
  }

  return 0;
}

                
                                                 
                                          
   
                           
                                               
                                                  
                                                   
                                                                        
   
static int
DecodeDate(char *str, int fmask, int *tmask, bool *is2digits, struct pg_tm *tm)
{
  fsec_t fsec;
  int nf = 0;
  int i, len;
  int dterr;
  bool haveTextMonth = false;
  int type, val, dmask = 0;
  char *field[MAXDATEFIELDS];

  *tmask = 0;

                            
  while (*str != '\0' && nf < MAXDATEFIELDS)
  {
                               
    while (*str != '\0' && !isalnum((unsigned char)*str))
    {
      str++;
    }

    if (*str == '\0')
    {
      return DTERR_BAD_FORMAT;                                    
    }

    field[nf] = str;
    if (isdigit((unsigned char)*str))
    {
      while (isdigit((unsigned char)*str))
      {
        str++;
      }
    }
    else if (isalpha((unsigned char)*str))
    {
      while (isalpha((unsigned char)*str))
      {
        str++;
      }
    }

                                                                
    if (*str != '\0')
    {
      *str++ = '\0';
    }
    nf++;
  }

                                                                        
  for (i = 0; i < nf; i++)
  {
    if (isalpha((unsigned char)*field[i]))
    {
      type = DecodeSpecial(i, field[i], &val);
      if (type == IGNORE_DTF)
      {
        continue;
      }

      dmask = DTK_M(type);
      switch (type)
      {
      case MONTH:
        tm->tm_mon = val;
        haveTextMonth = true;
        break;

      default:
        return DTERR_BAD_FORMAT;
      }
      if (fmask & dmask)
      {
        return DTERR_BAD_FORMAT;
      }

      fmask |= dmask;
      *tmask |= dmask;

                                              
      field[i] = NULL;
    }
  }

                                            
  for (i = 0; i < nf; i++)
  {
    if (field[i] == NULL)
    {
      continue;
    }

    if ((len = strlen(field[i])) <= 0)
    {
      return DTERR_BAD_FORMAT;
    }

    dterr = DecodeNumber(len, field[i], haveTextMonth, fmask, &dmask, tm, &fsec, is2digits);
    if (dterr)
    {
      return dterr;
    }

    if (fmask & dmask)
    {
      return DTERR_BAD_FORMAT;
    }

    fmask |= dmask;
    *tmask |= dmask;
  }

  if ((fmask & ~(DTK_M(DOY) | DTK_M(TZ))) != DTK_DATE_M)
  {
    return DTERR_BAD_FORMAT;
  }

                                                                     

  return 0;
}

                  
                                                              
                                          
   
int
ValidateDate(int fmask, bool isjulian, bool is2digits, bool bc, struct pg_tm *tm)
{
  if (fmask & DTK_M(YEAR))
  {
    if (isjulian)
    {
                                                        
    }
    else if (bc)
    {
                                                   
      if (tm->tm_year <= 0)
      {
        return DTERR_FIELD_OVERFLOW;
      }
                                                                       
      tm->tm_year = -(tm->tm_year - 1);
    }
    else if (is2digits)
    {
                                                                          
      if (tm->tm_year < 0)                    
      {
        return DTERR_FIELD_OVERFLOW;
      }
      if (tm->tm_year < 70)
      {
        tm->tm_year += 2000;
      }
      else if (tm->tm_year < 100)
      {
        tm->tm_year += 1900;
      }
    }
    else
    {
                                                   
      if (tm->tm_year <= 0)
      {
        return DTERR_FIELD_OVERFLOW;
      }
    }
  }

                                                 
  if (fmask & DTK_M(DOY))
  {
    j2date(date2j(tm->tm_year, 1, 1) + tm->tm_yday - 1, &tm->tm_year, &tm->tm_mon, &tm->tm_mday);
  }

                             
  if (fmask & DTK_M(MONTH))
  {
    if (tm->tm_mon < 1 || tm->tm_mon > MONTHS_PER_YEAR)
    {
      return DTERR_MD_FIELD_OVERFLOW;
    }
  }

                                   
  if (fmask & DTK_M(DAY))
  {
    if (tm->tm_mday < 1 || tm->tm_mday > 31)
    {
      return DTERR_MD_FIELD_OVERFLOW;
    }
  }

  if ((fmask & DTK_DATE_M) == DTK_DATE_M)
  {
       
                                                                         
                                                                           
                                                    
       
    if (tm->tm_mday > day_tab[isleap(tm->tm_year)][tm->tm_mon - 1])
    {
      return DTERR_FIELD_OVERFLOW;
    }
  }

  return 0;
}

                
                                                 
                                          
   
                                                                    
                                 
   
static int
DecodeTime(char *str, int fmask, int range, int *tmask, struct pg_tm *tm, fsec_t *fsec)
{
  char *cp;
  int dterr;

  *tmask = DTK_TIME_M;

  errno = 0;
  tm->tm_hour = strtoint(str, &cp, 10);
  if (errno == ERANGE)
  {
    return DTERR_FIELD_OVERFLOW;
  }
  if (*cp != ':')
  {
    return DTERR_BAD_FORMAT;
  }
  errno = 0;
  tm->tm_min = strtoint(cp + 1, &cp, 10);
  if (errno == ERANGE)
  {
    return DTERR_FIELD_OVERFLOW;
  }
  if (*cp == '\0')
  {
    tm->tm_sec = 0;
    *fsec = 0;
                                                                           
    if (range == (INTERVAL_MASK(MINUTE) | INTERVAL_MASK(SECOND)))
    {
      tm->tm_sec = tm->tm_min;
      tm->tm_min = tm->tm_hour;
      tm->tm_hour = 0;
    }
  }
  else if (*cp == '.')
  {
                                                     
    dterr = ParseFractionalSecond(cp, fsec);
    if (dterr)
    {
      return dterr;
    }
    tm->tm_sec = tm->tm_min;
    tm->tm_min = tm->tm_hour;
    tm->tm_hour = 0;
  }
  else if (*cp == ':')
  {
    errno = 0;
    tm->tm_sec = strtoint(cp + 1, &cp, 10);
    if (errno == ERANGE)
    {
      return DTERR_FIELD_OVERFLOW;
    }
    if (*cp == '\0')
    {
      *fsec = 0;
    }
    else if (*cp == '.')
    {
      dterr = ParseFractionalSecond(cp, fsec);
      if (dterr)
      {
        return dterr;
      }
    }
    else
    {
      return DTERR_BAD_FORMAT;
    }
  }
  else
  {
    return DTERR_BAD_FORMAT;
  }

                         
  if (tm->tm_hour < 0 || tm->tm_min < 0 || tm->tm_min > MINS_PER_HOUR - 1 || tm->tm_sec < 0 || tm->tm_sec > SECS_PER_MINUTE || *fsec < INT64CONST(0) || *fsec > USECS_PER_SEC)
  {
    return DTERR_FIELD_OVERFLOW;
  }

  return 0;
}

                  
                                                             
                                          
   
static int
DecodeNumber(int flen, char *str, bool haveTextMonth, int fmask, int *tmask, struct pg_tm *tm, fsec_t *fsec, bool *is2digits)
{
  int val;
  char *cp;
  int dterr;

  *tmask = 0;

  errno = 0;
  val = strtoint(str, &cp, 10);
  if (errno == ERANGE)
  {
    return DTERR_FIELD_OVERFLOW;
  }
  if (cp == str)
  {
    return DTERR_BAD_FORMAT;
  }

  if (*cp == '.')
  {
       
                                                                          
                                                         
       
    if (cp - str > 2)
    {
      dterr = DecodeNumberField(flen, str, (fmask | DTK_DATE_M), tmask, tm, fsec, is2digits);
      if (dterr < 0)
      {
        return dterr;
      }
      return 0;
    }

    dterr = ParseFractionalSecond(cp, fsec);
    if (dterr)
    {
      return dterr;
    }
  }
  else if (*cp != '\0')
  {
    return DTERR_BAD_FORMAT;
  }

                                    
  if (flen == 3 && (fmask & DTK_DATE_M) == DTK_M(YEAR) && val >= 1 && val <= 366)
  {
    *tmask = (DTK_M(DOY) | DTK_M(MONTH) | DTK_M(DAY));
    tm->tm_yday = val;
                                                          
    return 0;
  }

                                           
  switch (fmask & DTK_DATE_M)
  {
  case 0:

       
                                                                     
                                                              
                                                             
                                                                  
                                         
       
    if (flen >= 3 || DateOrder == DATEORDER_YMD)
    {
      *tmask = DTK_M(YEAR);
      tm->tm_year = val;
    }
    else if (DateOrder == DATEORDER_DMY)
    {
      *tmask = DTK_M(DAY);
      tm->tm_mday = val;
    }
    else
    {
      *tmask = DTK_M(MONTH);
      tm->tm_mon = val;
    }
    break;

  case (DTK_M(YEAR)):
                                             
    *tmask = DTK_M(MONTH);
    tm->tm_mon = val;
    break;

  case (DTK_M(MONTH)):
    if (haveTextMonth)
    {
         
                                                                     
                                                              
                                                                  
                                                                
                                                                    
         
      if (flen >= 3 || DateOrder == DATEORDER_YMD)
      {
        *tmask = DTK_M(YEAR);
        tm->tm_year = val;
      }
      else
      {
        *tmask = DTK_M(DAY);
        tm->tm_mday = val;
      }
    }
    else
    {
                                               
      *tmask = DTK_M(DAY);
      tm->tm_mday = val;
    }
    break;

  case (DTK_M(YEAR) | DTK_M(MONTH)):
    if (haveTextMonth)
    {
                                                       
      if (flen >= 3 && *is2digits)
      {
                                                             
        *tmask = DTK_M(DAY);                          
        tm->tm_mday = tm->tm_year;
        tm->tm_year = val;
        *is2digits = false;
      }
      else
      {
        *tmask = DTK_M(DAY);
        tm->tm_mday = val;
      }
    }
    else
    {
                                              
      *tmask = DTK_M(DAY);
      tm->tm_mday = val;
    }
    break;

  case (DTK_M(DAY)):
                                             
    *tmask = DTK_M(MONTH);
    tm->tm_mon = val;
    break;

  case (DTK_M(MONTH) | DTK_M(DAY)):
                                                        
    *tmask = DTK_M(YEAR);
    tm->tm_year = val;
    break;

  case (DTK_M(YEAR) | DTK_M(MONTH) | DTK_M(DAY)):
                                                          
    dterr = DecodeNumberField(flen, str, fmask, tmask, tm, fsec, is2digits);
    if (dterr < 0)
    {
      return dterr;
    }
    return 0;

  default:
                                      
    return DTERR_BAD_FORMAT;
  }

     
                                                                           
                    
     
  if (*tmask == DTK_M(YEAR))
  {
    *is2digits = (flen <= 2);
  }

  return 0;
}

                       
                                                                  
                                                                       
   
                                                             
                       
   
static int
DecodeNumberField(int len, char *str, int fmask, int *tmask, struct pg_tm *tm, fsec_t *fsec, bool *is2digits)
{
  char *cp;

     
                                                                           
              
     
  if ((cp = strchr(str, '.')) != NULL)
  {
       
                                                                          
                                   
       
    double frac;

    errno = 0;
    frac = strtod(cp, NULL);
    if (errno != 0)
    {
      return DTERR_BAD_FORMAT;
    }
    *fsec = rint(frac * 1000000);
                                                              
    *cp = '\0';
    len = strlen(str);
  }
                                                  
  else if ((fmask & DTK_DATE_M) != DTK_DATE_M)
  {
    if (len >= 6)
    {
      *tmask = DTK_DATE_M;

         
                                                                      
                               
         
      tm->tm_mday = atoi(str + (len - 2));
      *(str + (len - 2)) = '\0';
      tm->tm_mon = atoi(str + (len - 4));
      *(str + (len - 4)) = '\0';
      tm->tm_year = atoi(str);
      if ((len - 4) == 2)
      {
        *is2digits = true;
      }

      return DTK_DATE;
    }
  }

                                          
  if ((fmask & DTK_TIME_M) != DTK_TIME_M)
  {
                
    if (len == 6)
    {
      *tmask = DTK_TIME_M;
      tm->tm_sec = atoi(str + 4);
      *(str + 4) = '\0';
      tm->tm_min = atoi(str + 2);
      *(str + 2) = '\0';
      tm->tm_hour = atoi(str);

      return DTK_TIME;
    }
               
    else if (len == 4)
    {
      *tmask = DTK_TIME_M;
      tm->tm_sec = 0;
      tm->tm_min = atoi(str + 2);
      *(str + 2) = '\0';
      tm->tm_hour = atoi(str);

      return DTK_TIME;
    }
  }

  return DTERR_BAD_FORMAT;
}

                    
                                           
   
                                                              
   
int
DecodeTimezone(char *str, int *tzp)
{
  int tz;
  int hr, min, sec = 0;
  char *cp;

                                            
  if (*str != '+' && *str != '-')
  {
    return DTERR_BAD_FORMAT;
  }

  errno = 0;
  hr = strtoint(str + 1, &cp, 10);
  if (errno == ERANGE)
  {
    return DTERR_TZDISP_OVERFLOW;
  }

                           
  if (*cp == ':')
  {
    errno = 0;
    min = strtoint(cp + 1, &cp, 10);
    if (errno == ERANGE)
    {
      return DTERR_TZDISP_OVERFLOW;
    }
    if (*cp == ':')
    {
      errno = 0;
      sec = strtoint(cp + 1, &cp, 10);
      if (errno == ERANGE)
      {
        return DTERR_TZDISP_OVERFLOW;
      }
    }
  }
                                                    
  else if (*cp == '\0' && strlen(str) > 3)
  {
    min = hr % 100;
    hr = hr / 100;
                                                                   
  }
  else
  {
    min = 0;
  }

                                                                 
  if (hr < 0 || hr > MAX_TZDISP_HOUR)
  {
    return DTERR_TZDISP_OVERFLOW;
  }
  if (min < 0 || min >= MINS_PER_HOUR)
  {
    return DTERR_TZDISP_OVERFLOW;
  }
  if (sec < 0 || sec >= SECS_PER_MINUTE)
  {
    return DTERR_TZDISP_OVERFLOW;
  }

  tz = (hr * MINS_PER_HOUR + min) * SECS_PER_MINUTE + sec;
  if (*str == '-')
  {
    tz = -tz;
  }

  *tzp = -tz;

  if (*cp != '\0')
  {
    return DTERR_BAD_FORMAT;
  }

  return 0;
}

                          
                                                             
   
                                                                         
                                                                             
                                                                            
                                                                            
                                        
   
                                            
   
                                                          
                              
   
int
DecodeTimezoneAbbrev(int field, char *lowtoken, int *offset, pg_tz **tz)
{
  int type;
  const datetkn *tp;

  tp = abbrevcache[field];
                                                     
  if (tp == NULL || strncmp(lowtoken, tp->token, TOKMAXLEN) != 0)
  {
    if (zoneabbrevtbl)
    {
      tp = datebsearch(lowtoken, zoneabbrevtbl->abbrevs, zoneabbrevtbl->numabbrevs);
    }
    else
    {
      tp = NULL;
    }
  }
  if (tp == NULL)
  {
    type = UNKNOWN_FIELD;
    *offset = 0;
    *tz = NULL;
  }
  else
  {
    abbrevcache[field] = tp;
    type = tp->type;
    if (type == DYNTZ)
    {
      *offset = 0;
      *tz = FetchDynamicTimeZone(zoneabbrevtbl, tp);
    }
    else
    {
      *offset = tp->value;
      *tz = NULL;
    }
  }

  return type;
}

                   
                                          
   
                                                
                                                                       
                                                   
   
                                            
   
                                                          
                              
   
int
DecodeSpecial(int field, char *lowtoken, int *val)
{
  int type;
  const datetkn *tp;

  tp = datecache[field];
                                                     
  if (tp == NULL || strncmp(lowtoken, tp->token, TOKMAXLEN) != 0)
  {
    tp = datebsearch(lowtoken, datetktbl, szdatetktbl);
  }
  if (tp == NULL)
  {
    type = UNKNOWN_FIELD;
    *val = 0;
  }
  else
  {
    datecache[field] = tp;
    type = tp->type;
    *val = tp->value;
  }

  return type;
}

             
   
                                          
   
static inline void
ClearPgTm(struct pg_tm *tm, fsec_t *fsec)
{
  tm->tm_year = 0;
  tm->tm_mon = 0;
  tm->tm_mday = 0;
  tm->tm_hour = 0;
  tm->tm_min = 0;
  tm->tm_sec = 0;
  *fsec = 0;
}

                    
                                                                 
                                                                
                                          
   
                                                        
                                                          
   
                                                                    
                                                    
   
int
DecodeInterval(char **field, int *ftype, int nf, int range, int *dtype, struct pg_tm *tm, fsec_t *fsec)
{
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
      dterr = DecodeTime(field[i], fmask, range, &tmask, tm, fsec);
      if (dterr)
      {
        return dterr;
      }
      type = DTK_DAY;
      break;

    case DTK_TZ:

         
                                                                     
                                                                   
                     
         
      Assert(*field[i] == '-' || *field[i] == '+');

         
                                                                     
                                                           
         
      if (strchr(field[i] + 1, ':') != NULL && DecodeTime(field[i] + 1, fmask, range, &tmask, tm, fsec) == 0)
      {
        if (*field[i] == '-')
        {
                                           
          tm->tm_hour = -tm->tm_hour;
          tm->tm_min = -tm->tm_min;
          tm->tm_sec = -tm->tm_sec;
          *fsec = -(*fsec);
        }

           
                                                           
                                                                   
                                      
           
        type = DTK_DAY;
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
          type = DTK_HOUR;
          break;
        case INTERVAL_MASK(MINUTE):
        case INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE):
        case INTERVAL_MASK(DAY) | INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE):
          type = DTK_MINUTE;
          break;
        case INTERVAL_MASK(SECOND):
        case INTERVAL_MASK(MINUTE) | INTERVAL_MASK(SECOND):
        case INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE) | INTERVAL_MASK(SECOND):
        case INTERVAL_MASK(DAY) | INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE) | INTERVAL_MASK(SECOND):
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
        if (((double)val * MONTHS_PER_YEAR + val2) > INT_MAX || ((double)val * MONTHS_PER_YEAR + val2) < INT_MIN)
        {
          return DTERR_FIELD_OVERFLOW;
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
                                              
        tm->tm_sec += val / 1000;
        val -= (val / 1000) * 1000;
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
        tmask = DTK_M(DAY);
        break;

      case DTK_WEEK:
        tm->tm_mday += val * 7;
        AdjustFractDays(fval, tm, fsec, 7);
        tmask = DTK_M(WEEK);
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
        tmask = DTK_M(YEAR);
        break;

      case DTK_DECADE:
        tm->tm_year += val * 10;
        if (fval != 0)
        {
          tm->tm_mon += fval * MONTHS_PER_YEAR * 10;
        }
        tmask = DTK_M(DECADE);
        break;

      case DTK_CENTURY:
        tm->tm_year += val * 100;
        if (fval != 0)
        {
          tm->tm_mon += fval * MONTHS_PER_YEAR * 100;
        }
        tmask = DTK_M(CENTURY);
        break;

      case DTK_MILLENNIUM:
        tm->tm_year += val * 1000;
        if (fval != 0)
        {
          tm->tm_mon += fval * MONTHS_PER_YEAR * 1000;
        }
        tmask = DTK_M(MILLENNIUM);
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

   
                                                                       
   
                                                                         
                            
   
static int
ParseISO8601Number(char *str, char **endptr, int *ipart, double *fpart)
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
ISO8601IntegerWidth(char *fieldstart)
{
                                       
  if (*fieldstart == '-')
  {
    fieldstart++;
  }
  return strspn(fieldstart, "0123456789");
}

                           
                                                                     
                                                               
                            
                        
                                                                  
                                                                  
   
                                                                
                                                                          
                                                                  
                                          
   
                                      
                                                      
                                                                       
   
int
DecodeISO8601Interval(char *str, int *dtype, struct pg_tm *tm, fsec_t *fsec)
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
DecodeUnits(int field, char *lowtoken, int *val)
{
  int type;
  const datetkn *tp;

  tp = deltacache[field];
                                                     
  if (tp == NULL || strncmp(lowtoken, tp->token, TOKMAXLEN) != 0)
  {
    tp = datebsearch(lowtoken, deltatktbl, szdeltatktbl);
  }
  if (tp == NULL)
  {
    type = UNKNOWN_FIELD;
    *val = 0;
  }
  else
  {
    deltacache[field] = tp;
    type = tp->type;
    *val = tp->value;
  }

  return type;
}                    

   
                                                                              
   
                                                                          
                                                      
   
                                                                          
                                                                             
                                   
   
void
DateTimeParseError(int dterr, const char *str, const char *datatype)
{
  switch (dterr)
  {
  case DTERR_FIELD_OVERFLOW:
    ereport(ERROR, (errcode(ERRCODE_DATETIME_FIELD_OVERFLOW), errmsg("date/time field value out of range: \"%s\"", str)));
    break;
  case DTERR_MD_FIELD_OVERFLOW:
                                                                    
    ereport(ERROR, (errcode(ERRCODE_DATETIME_FIELD_OVERFLOW), errmsg("date/time field value out of range: \"%s\"", str), errhint("Perhaps you need a different \"datestyle\" setting.")));
    break;
  case DTERR_INTERVAL_OVERFLOW:
    ereport(ERROR, (errcode(ERRCODE_INTERVAL_FIELD_OVERFLOW), errmsg("interval field value out of range: \"%s\"", str)));
    break;
  case DTERR_TZDISP_OVERFLOW:
    ereport(ERROR, (errcode(ERRCODE_INVALID_TIME_ZONE_DISPLACEMENT_VALUE), errmsg("time zone displacement out of range: \"%s\"", str)));
    break;
  case DTERR_BAD_FORMAT:
  default:
    ereport(ERROR, (errcode(ERRCODE_INVALID_DATETIME_FORMAT), errmsg("invalid input syntax for type %s: \"%s\"", datatype, str)));
    break;
  }
}

                 
                                                                            
                                             
   
static const datetkn *
datebsearch(const char *key, const datetkn *base, int nel)
{
  if (nel > 0)
  {
    const datetkn *last = base + nel - 1, *position;
    int result;

    while (last >= base)
    {
      position = base + ((last - base) >> 1);
                                                                 
      result = (int)key[0] - (int)position->token[0];
      if (result == 0)
      {
                                                           
        result = strncmp(key, position->token, TOKMAXLEN);
        if (result == 0)
        {
          return position;
        }
      }
      if (result < 0)
      {
        last = position - 1;
      }
      else
      {
        base = position + 1;
      }
    }
  }
  return NULL;
}

                    
                                                               
   
                                                                         
                                                                      
   
static char *
EncodeTimezone(char *str, int tz, int style)
{
  int hour, min, sec;

  sec = abs(tz);
  min = sec / SECS_PER_MINUTE;
  sec -= min * SECS_PER_MINUTE;
  hour = min / MINS_PER_HOUR;
  min -= hour * MINS_PER_HOUR;

                                                             
  *str++ = (tz <= 0 ? '+' : '-');

  if (sec != 0)
  {
    str = pg_ltostr_zeropad(str, hour, 2);
    *str++ = ':';
    str = pg_ltostr_zeropad(str, min, 2);
    *str++ = ':';
    str = pg_ltostr_zeropad(str, sec, 2);
  }
  else if (min != 0 || style == USE_XSD_DATES)
  {
    str = pg_ltostr_zeropad(str, hour, 2);
    *str++ = ':';
    str = pg_ltostr_zeropad(str, min, 2);
  }
  else
  {
    str = pg_ltostr_zeropad(str, hour, 2);
  }
  return str;
}

                    
                              
   
void
EncodeDateOnly(struct pg_tm *tm, int style, char *str)
{
  Assert(tm->tm_mon >= 1 && tm->tm_mon <= MONTHS_PER_YEAR);

  switch (style)
  {
  case USE_ISO_DATES:
  case USE_XSD_DATES:
                                          
    str = pg_ltostr_zeropad(str, (tm->tm_year > 0) ? tm->tm_year : -(tm->tm_year - 1), 4);
    *str++ = '-';
    str = pg_ltostr_zeropad(str, tm->tm_mon, 2);
    *str++ = '-';
    str = pg_ltostr_zeropad(str, tm->tm_mday, 2);
    break;

  case USE_SQL_DATES:
                                                    
    if (DateOrder == DATEORDER_DMY)
    {
      str = pg_ltostr_zeropad(str, tm->tm_mday, 2);
      *str++ = '/';
      str = pg_ltostr_zeropad(str, tm->tm_mon, 2);
    }
    else
    {
      str = pg_ltostr_zeropad(str, tm->tm_mon, 2);
      *str++ = '/';
      str = pg_ltostr_zeropad(str, tm->tm_mday, 2);
    }
    *str++ = '/';
    str = pg_ltostr_zeropad(str, (tm->tm_year > 0) ? tm->tm_year : -(tm->tm_year - 1), 4);
    break;

  case USE_GERMAN_DATES:
                                  
    str = pg_ltostr_zeropad(str, tm->tm_mday, 2);
    *str++ = '.';
    str = pg_ltostr_zeropad(str, tm->tm_mon, 2);
    *str++ = '.';
    str = pg_ltostr_zeropad(str, (tm->tm_year > 0) ? tm->tm_year : -(tm->tm_year - 1), 4);
    break;

  case USE_POSTGRES_DATES:
  default:
                                                  
    if (DateOrder == DATEORDER_DMY)
    {
      str = pg_ltostr_zeropad(str, tm->tm_mday, 2);
      *str++ = '-';
      str = pg_ltostr_zeropad(str, tm->tm_mon, 2);
    }
    else
    {
      str = pg_ltostr_zeropad(str, tm->tm_mon, 2);
      *str++ = '-';
      str = pg_ltostr_zeropad(str, tm->tm_mday, 2);
    }
    *str++ = '-';
    str = pg_ltostr_zeropad(str, (tm->tm_year > 0) ? tm->tm_year : -(tm->tm_year - 1), 4);
    break;
  }

  if (tm->tm_year <= 0)
  {
    memcpy(str, " BC", 3);                     
    str += 3;
  }
  *str = '\0';
}

                    
                            
   
                                                                               
                                                                         
                                                                                
           
   
void
EncodeTimeOnly(struct pg_tm *tm, fsec_t fsec, bool print_tz, int tz, int style, char *str)
{
  str = pg_ltostr_zeropad(str, tm->tm_hour, 2);
  *str++ = ':';
  str = pg_ltostr_zeropad(str, tm->tm_min, 2);
  *str++ = ':';
  str = AppendSeconds(str, tm->tm_sec, fsec, MAX_TIME_PRECISION, true);
  if (print_tz)
  {
    str = EncodeTimezone(str, tz, style);
  }
  *str = '\0';
}

                    
                                                   
   
                                                                               
                                                                               
                                                                        
                                                                          
                                            
   
                          
                                       
                                   
                                  
                                   
                                     
   
void
EncodeDateTime(struct pg_tm *tm, fsec_t fsec, bool print_tz, int tz, const char *tzn, int style, char *str)
{
  int day;

  Assert(tm->tm_mon >= 1 && tm->tm_mon <= MONTHS_PER_YEAR);

     
                                                                     
     
  if (tm->tm_isdst < 0)
  {
    print_tz = false;
  }

  switch (style)
  {
  case USE_ISO_DATES:
  case USE_XSD_DATES:
                                               
    str = pg_ltostr_zeropad(str, (tm->tm_year > 0) ? tm->tm_year : -(tm->tm_year - 1), 4);
    *str++ = '-';
    str = pg_ltostr_zeropad(str, tm->tm_mon, 2);
    *str++ = '-';
    str = pg_ltostr_zeropad(str, tm->tm_mday, 2);
    *str++ = (style == USE_ISO_DATES) ? ' ' : 'T';
    str = pg_ltostr_zeropad(str, tm->tm_hour, 2);
    *str++ = ':';
    str = pg_ltostr_zeropad(str, tm->tm_min, 2);
    *str++ = ':';
    str = AppendTimestampSeconds(str, tm, fsec);
    if (print_tz)
    {
      str = EncodeTimezone(str, tz, style);
    }
    break;

  case USE_SQL_DATES:
                                                    
    if (DateOrder == DATEORDER_DMY)
    {
      str = pg_ltostr_zeropad(str, tm->tm_mday, 2);
      *str++ = '/';
      str = pg_ltostr_zeropad(str, tm->tm_mon, 2);
    }
    else
    {
      str = pg_ltostr_zeropad(str, tm->tm_mon, 2);
      *str++ = '/';
      str = pg_ltostr_zeropad(str, tm->tm_mday, 2);
    }
    *str++ = '/';
    str = pg_ltostr_zeropad(str, (tm->tm_year > 0) ? tm->tm_year : -(tm->tm_year - 1), 4);
    *str++ = ' ';
    str = pg_ltostr_zeropad(str, tm->tm_hour, 2);
    *str++ = ':';
    str = pg_ltostr_zeropad(str, tm->tm_min, 2);
    *str++ = ':';
    str = AppendTimestampSeconds(str, tm, fsec);

       
                                                                     
                                                                       
                                                              
       
    if (print_tz)
    {
      if (tzn)
      {
        sprintf(str, " %.*s", MAXTZLEN, tzn);
        str += strlen(str);
      }
      else
      {
        str = EncodeTimezone(str, tz, style);
      }
    }
    break;

  case USE_GERMAN_DATES:
                                          
    str = pg_ltostr_zeropad(str, tm->tm_mday, 2);
    *str++ = '.';
    str = pg_ltostr_zeropad(str, tm->tm_mon, 2);
    *str++ = '.';
    str = pg_ltostr_zeropad(str, (tm->tm_year > 0) ? tm->tm_year : -(tm->tm_year - 1), 4);
    *str++ = ' ';
    str = pg_ltostr_zeropad(str, tm->tm_hour, 2);
    *str++ = ':';
    str = pg_ltostr_zeropad(str, tm->tm_min, 2);
    *str++ = ':';
    str = AppendTimestampSeconds(str, tm, fsec);

    if (print_tz)
    {
      if (tzn)
      {
        sprintf(str, " %.*s", MAXTZLEN, tzn);
        str += strlen(str);
      }
      else
      {
        str = EncodeTimezone(str, tz, style);
      }
    }
    break;

  case USE_POSTGRES_DATES:
  default:
                                                                     
    day = date2j(tm->tm_year, tm->tm_mon, tm->tm_mday);
    tm->tm_wday = j2day(day);
    memcpy(str, days[tm->tm_wday], 3);
    str += 3;
    *str++ = ' ';
    if (DateOrder == DATEORDER_DMY)
    {
      str = pg_ltostr_zeropad(str, tm->tm_mday, 2);
      *str++ = ' ';
      memcpy(str, months[tm->tm_mon - 1], 3);
      str += 3;
    }
    else
    {
      memcpy(str, months[tm->tm_mon - 1], 3);
      str += 3;
      *str++ = ' ';
      str = pg_ltostr_zeropad(str, tm->tm_mday, 2);
    }
    *str++ = ' ';
    str = pg_ltostr_zeropad(str, tm->tm_hour, 2);
    *str++ = ':';
    str = pg_ltostr_zeropad(str, tm->tm_min, 2);
    *str++ = ':';
    str = AppendTimestampSeconds(str, tm, fsec);
    *str++ = ' ';
    str = pg_ltostr_zeropad(str, (tm->tm_year > 0) ? tm->tm_year : -(tm->tm_year - 1), 4);

    if (print_tz)
    {
      if (tzn)
      {
        sprintf(str, " %.*s", MAXTZLEN, tzn);
        str += strlen(str);
      }
      else
      {
           
                                                               
                                                                   
                                                                 
                                                           
           
        *str++ = ' ';
        str = EncodeTimezone(str, tz, style);
      }
    }
    break;
  }

  if (tm->tm_year <= 0)
  {
    memcpy(str, " BC", 3);                     
    str += 3;
  }
  *str = '\0';
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

                    
                                                                   
   
                                                       
                                                                  
                                                              
                       
   
                                                            
                                                                    
                                                     
                           
                                                                    
                    
   
                                                       
                                                    
                                                   
   
void
EncodeInterval(struct pg_tm *tm, fsec_t fsec, int style, char *str)
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
      cp = AppendSeconds(cp, sec, fsec, MAX_INTERVAL_PRECISION, true);
      *cp = '\0';
    }
    else if (has_year_month)
    {
      sprintf(cp, "%d-%d", year, mon);
    }
    else if (has_day)
    {
      sprintf(cp, "%d %d:%02d:", mday, hour, min);
      cp += strlen(cp);
      cp = AppendSeconds(cp, sec, fsec, MAX_INTERVAL_PRECISION, true);
      *cp = '\0';
    }
    else
    {
      sprintf(cp, "%d:%02d:", hour, min);
      cp += strlen(cp);
      cp = AppendSeconds(cp, sec, fsec, MAX_INTERVAL_PRECISION, true);
      *cp = '\0';
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
      cp = AppendSeconds(cp, sec, fsec, MAX_INTERVAL_PRECISION, false);
      *cp++ = 'S';
      *cp++ = '\0';
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
      cp = AppendSeconds(cp, sec, fsec, MAX_INTERVAL_PRECISION, true);
      *cp = '\0';
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
      cp = AppendSeconds(cp, sec, fsec, MAX_INTERVAL_PRECISION, false);
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

   
                                                                           
                                                                   
   
static bool
CheckDateTokenTable(const char *tablename, const datetkn *base, int nel)
{
  bool ok = true;
  int i;

  for (i = 0; i < nel; i++)
  {
                                                
    if (strlen(base[i].token) > TOKMAXLEN)
    {
                                                       
      elog(LOG, "token too long in %s table: \"%.*s\"", tablename, TOKMAXLEN + 1, base[i].token);
      ok = false;
      break;                                 
    }
                                
    if (i > 0 && strcmp(base[i - 1].token, base[i].token) >= 0)
    {
      elog(LOG, "ordering error in %s table: \"%s\" >= \"%s\"", tablename, base[i - 1].token, base[i].token);
      ok = false;
    }
  }
  return ok;
}

bool
CheckDateTokenTables(void)
{
  bool ok = true;

  Assert(UNIX_EPOCH_JDATE == date2j(1970, 1, 1));
  Assert(POSTGRES_EPOCH_JDATE == date2j(2000, 1, 1));

  ok &= CheckDateTokenTable("datetktbl", datetktbl, szdatetktbl);
  ok &= CheckDateTokenTable("deltatktbl", deltatktbl, szdeltatktbl);
  return ok;
}

   
                                                                         
                                                         
   
                                                                              
                                                                         
                                                                           
                                                                              
                           
   
                                                                             
                                           
   
                                                                              
                                                                             
   
Node *
TemporalSimplify(int32 max_precis, Node *node)
{
  FuncExpr *expr = castNode(FuncExpr, node);
  Node *ret = NULL;
  Node *typmod;

  Assert(list_length(expr->args) >= 2);

  typmod = (Node *)lsecond(expr->args);

  if (IsA(typmod, Const) && !((Const *)typmod)->constisnull)
  {
    Node *source = (Node *)linitial(expr->args);
    int32 old_precis = exprTypmod(source);
    int32 new_precis = DatumGetInt32(((Const *)typmod)->constvalue);

    if (new_precis < 0 || new_precis == max_precis || (old_precis >= 0 && new_precis >= old_precis))
    {
      ret = relabel_to_typmod(source, new_precis);
    }
  }

  return ret;
}

   
                                                                        
                                                                     
                                    
   
                                                                               
                                                                      
   
TimeZoneAbbrevTable *
ConvertTimeZoneAbbrevs(struct tzEntry *abbrevs, int n)
{
  TimeZoneAbbrevTable *tbl;
  Size tbl_size;
  int i;

                                                
  tbl_size = offsetof(TimeZoneAbbrevTable, abbrevs) + n * sizeof(datetkn);
  tbl_size = MAXALIGN(tbl_size);
                                                
  for (i = 0; i < n; i++)
  {
    struct tzEntry *abbr = abbrevs + i;

    if (abbr->zone != NULL)
    {
      Size dsize;

      dsize = offsetof(DynamicZoneAbbrev, zone) + strlen(abbr->zone) + 1;
      tbl_size += MAXALIGN(dsize);
    }
  }

                            
  tbl = malloc(tbl_size);
  if (!tbl)
  {
    return NULL;
  }

                          
  tbl->tblsize = tbl_size;
  tbl->numabbrevs = n;
                                                                   
  tbl_size = offsetof(TimeZoneAbbrevTable, abbrevs) + n * sizeof(datetkn);
  tbl_size = MAXALIGN(tbl_size);
  for (i = 0; i < n; i++)
  {
    struct tzEntry *abbr = abbrevs + i;
    datetkn *dtoken = tbl->abbrevs + i;

                                                   
    strlcpy(dtoken->token, abbr->abbrev, TOKMAXLEN + 1);
    if (abbr->zone != NULL)
    {
                                                              
      DynamicZoneAbbrev *dtza;
      Size dsize;

      dtza = (DynamicZoneAbbrev *)((char *)tbl + tbl_size);
      dtza->tz = NULL;
      strcpy(dtza->zone, abbr->zone);

      dtoken->type = DYNTZ;
                                                                 
      dtoken->value = (int32)tbl_size;

      dsize = offsetof(DynamicZoneAbbrev, zone) + strlen(abbr->zone) + 1;
      tbl_size += MAXALIGN(dsize);
    }
    else
    {
      dtoken->type = abbr->is_dst ? DTZ : TZ;
      dtoken->value = abbr->offset;
    }
  }

                                                              
  Assert(tbl->tblsize == tbl_size);

                                      
  Assert(CheckDateTokenTable("timezone abbreviations", tbl->abbrevs, n));

  return tbl;
}

   
                                                      
   
                                                                             
   
void
InstallTimeZoneAbbrevs(TimeZoneAbbrevTable *tbl)
{
  zoneabbrevtbl = tbl;
                                                                    
  memset(abbrevcache, 0, sizeof(abbrevcache));
}

   
                                                                          
   
static pg_tz *
FetchDynamicTimeZone(TimeZoneAbbrevTable *tbl, const datetkn *tp)
{
  DynamicZoneAbbrev *dtza;

                                                                    
  Assert(tp->type == DYNTZ);
  Assert(tp->value > 0 && tp->value < tbl->tblsize);

  dtza = (DynamicZoneAbbrev *)((char *)tbl + tp->value);

                                                         
  if (dtza->tz == NULL)
  {
    dtza->tz = pg_tzset(dtza->zone);

       
                                                                         
                                                              
       
    if (dtza->tz == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("time zone \"%s\" not recognized", dtza->zone), errdetail("This time zone name appears in the configuration file for time zone abbreviation \"%s\".", tp->token)));
    }
  }
  return dtza->tz;
}

   
                                                                               
                                                      
   
Datum
pg_timezone_abbrevs(PG_FUNCTION_ARGS)
{
  FuncCallContext *funcctx;
  int *pindex;
  Datum result;
  HeapTuple tuple;
  Datum values[3];
  bool nulls[3];
  const datetkn *tp;
  char buffer[TOKMAXLEN + 1];
  int gmtoffset;
  bool is_dst;
  unsigned char *p;
  struct pg_tm tm;
  Interval *resInterval;

                                                         
  if (SRF_IS_FIRSTCALL())
  {
    TupleDesc tupdesc;
    MemoryContext oldcontext;

                                                              
    funcctx = SRF_FIRSTCALL_INIT();

       
                                                                        
       
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

                                          
    pindex = (int *)palloc(sizeof(int));
    *pindex = 0;
    funcctx->user_fctx = (void *)pindex;

       
                                                                        
                      
       
    tupdesc = CreateTemplateTupleDesc(3);
    TupleDescInitEntry(tupdesc, (AttrNumber)1, "abbrev", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)2, "utc_offset", INTERVALOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)3, "is_dst", BOOLOID, -1, 0);

    funcctx->tuple_desc = BlessTupleDesc(tupdesc);
    MemoryContextSwitchTo(oldcontext);
  }

                                                
  funcctx = SRF_PERCALL_SETUP();
  pindex = (int *)funcctx->user_fctx;

  if (zoneabbrevtbl == NULL || *pindex >= zoneabbrevtbl->numabbrevs)
  {
    SRF_RETURN_DONE(funcctx);
  }

  tp = zoneabbrevtbl->abbrevs + *pindex;

  switch (tp->type)
  {
  case TZ:
    gmtoffset = tp->value;
    is_dst = false;
    break;
  case DTZ:
    gmtoffset = tp->value;
    is_dst = true;
    break;
  case DYNTZ:
  {
                                                     
    pg_tz *tzp;
    TimestampTz now;
    int isdst;

    tzp = FetchDynamicTimeZone(zoneabbrevtbl, tp);
    now = GetCurrentTransactionStartTimestamp();
    gmtoffset = -DetermineTimeZoneAbbrevOffsetTS(now, tp->token, tzp, &isdst);
    is_dst = (bool)isdst;
    break;
  }
  default:
    elog(ERROR, "unrecognized timezone type %d", (int)tp->type);
    gmtoffset = 0;                          
    is_dst = false;
    break;
  }

  MemSet(nulls, 0, sizeof(nulls));

     
                                                                            
                                
     
  strlcpy(buffer, tp->token, sizeof(buffer));
  for (p = (unsigned char *)buffer; *p; p++)
  {
    *p = pg_toupper(*p);
  }

  values[0] = CStringGetTextDatum(buffer);

                                                  
  MemSet(&tm, 0, sizeof(struct pg_tm));
  tm.tm_sec = gmtoffset;
  resInterval = (Interval *)palloc(sizeof(Interval));
  tm2interval(&tm, 0, resInterval);
  values[1] = IntervalPGetDatum(resInterval);

  values[2] = BoolGetDatum(is_dst);

  (*pindex)++;

  tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
  result = HeapTupleGetDatum(tuple);

  SRF_RETURN_NEXT(funcctx, result);
}

   
                                                                       
                                                            
   
Datum
pg_timezone_names(PG_FUNCTION_ARGS)
{
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  bool randomAccess;
  TupleDesc tupdesc;
  Tuplestorestate *tupstore;
  pg_tzenum *tzenum;
  pg_tz *tz;
  Datum values[4];
  bool nulls[4];
  int tzoff;
  struct pg_tm tm;
  fsec_t fsec;
  const char *tzn;
  Interval *resInterval;
  struct pg_tm itm;
  MemoryContext oldcontext;

                                                                 
  if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that cannot accept a set")));
  }
  if (!(rsinfo->allowedModes & SFRM_Materialize))
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("materialize mode required, but it is not allowed in this context")));
  }

                                                                           
  oldcontext = MemoryContextSwitchTo(rsinfo->econtext->ecxt_per_query_memory);

  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
  {
    elog(ERROR, "return type must be a row type");
  }

  randomAccess = (rsinfo->allowedModes & SFRM_Materialize_Random) != 0;
  tupstore = tuplestore_begin_heap(randomAccess, false, work_mem);
  rsinfo->returnMode = SFRM_Materialize;
  rsinfo->setResult = tupstore;
  rsinfo->setDesc = tupdesc;

  MemoryContextSwitchTo(oldcontext);

                                         
  tzenum = pg_tzenumerate_start();

                                          
  for (;;)
  {
    tz = pg_tzenumerate_next(tzenum);
    if (!tz)
    {
      break;
    }

                                                  
    if (timestamp2tm(GetCurrentTransactionStartTimestamp(), &tzoff, &tm, &fsec, &tzn, tz) != 0)
    {
      continue;                                 
    }

       
                                                                         
                                                                          
                                                                          
                                                                        
                                                                          
                                                                      
                                                              
       
    if (tzn && strlen(tzn) > 31)
    {
      continue;
    }

    MemSet(nulls, 0, sizeof(nulls));

    values[0] = CStringGetTextDatum(pg_get_timezone_name(tz));
    values[1] = CStringGetTextDatum(tzn ? tzn : "");

    MemSet(&itm, 0, sizeof(struct pg_tm));
    itm.tm_sec = -tzoff;
    resInterval = (Interval *)palloc(sizeof(Interval));
    tm2interval(&itm, 0, resInterval);
    values[2] = IntervalPGetDatum(resInterval);

    values[3] = BoolGetDatum(tm.tm_isdst > 0);

    tuplestore_putvalues(tupstore, tupdesc, values, nulls);
  }

  pg_tzenumerate_end(tzenum);
  return (Datum)0;
}
