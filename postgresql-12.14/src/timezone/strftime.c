                                                   

   
                                                               
                        
   
                                                                      
                                                                      
            
                                                                     
                                                                   
                                                                        
                                                                         
                                                                          
                                                                           
                                                                           
                                                
   
                                                                         
                                                                         
                                                                              
                                                                            
                                                                              
                                                                           
                                                                         
                                                                              
                                                                             
                                                                          
                
   

   
                                                                       
   
                                                                       
   
                  
                             
   

#include "postgres.h"

#include <fcntl.h>

#include "private.h"

struct lc_time_T
{
  const char *mon[MONSPERYEAR];
  const char *month[MONSPERYEAR];
  const char *wday[DAYSPERWEEK];
  const char *weekday[DAYSPERWEEK];
  const char *X_fmt;
  const char *x_fmt;
  const char *c_fmt;
  const char *am;
  const char *pm;
  const char *date_fmt;
};

#define Locale (&C_time_locale)

static const struct lc_time_T C_time_locale = {{"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"}, {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"}, {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}, {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"},

               
    "%H:%M:%S",

       
             
       
                                                                             
                                                        
       
    "%m/%d/%y",

       
             
       
                                                                             
                                                                              
                    
       
    "%a %b %e %T %Y",

            
    "AM",

            
    "PM",

                  
    "%a %b %e %H:%M:%S %Z %Y"};

enum warn
{
  IN_NONE,
  IN_SOME,
  IN_THIS,
  IN_ALL
};

static char *
_add(const char *, char *, const char *);
static char *
_conv(int, const char *, char *, const char *);
static char *
_fmt(const char *, const struct pg_tm *, char *, const char *, enum warn *);
static char *
_yconv(int, int, bool, bool, char *, char const *);

   
                                                                               
                                   
   
                                
   
size_t
pg_strftime(char *s, size_t maxsize, const char *format, const struct pg_tm *t)
{
  char *p;
  int saved_errno = errno;
  enum warn warn = IN_NONE;

  p = _fmt(format, t, s, s + maxsize, &warn);
  if (!p)
  {
    errno = EOVERFLOW;
    return 0;
  }
  if (p == s + maxsize)
  {
    errno = ERANGE;
    return 0;
  }
  *p = '\0';
  errno = saved_errno;
  return p - s;
}

static char *
_fmt(const char *format, const struct pg_tm *t, char *pt, const char *ptlim, enum warn *warnp)
{
  for (; *format; ++format)
  {
    if (*format == '%')
    {
    label:
      switch (*++format)
      {
      case '\0':
        --format;
        break;
      case 'A':
        pt = _add((t->tm_wday < 0 || t->tm_wday >= DAYSPERWEEK) ? "?" : Locale->weekday[t->tm_wday], pt, ptlim);
        continue;
      case 'a':
        pt = _add((t->tm_wday < 0 || t->tm_wday >= DAYSPERWEEK) ? "?" : Locale->wday[t->tm_wday], pt, ptlim);
        continue;
      case 'B':
        pt = _add((t->tm_mon < 0 || t->tm_mon >= MONSPERYEAR) ? "?" : Locale->month[t->tm_mon], pt, ptlim);
        continue;
      case 'b':
      case 'h':
        pt = _add((t->tm_mon < 0 || t->tm_mon >= MONSPERYEAR) ? "?" : Locale->mon[t->tm_mon], pt, ptlim);
        continue;
      case 'C':

           
                                                         
                                                           
                                                   
           
        pt = _yconv(t->tm_year, TM_YEAR_BASE, true, false, pt, ptlim);
        continue;
      case 'c':
      {
        enum warn warn2 = IN_SOME;

        pt = _fmt(Locale->c_fmt, t, pt, ptlim, &warn2);
        if (warn2 == IN_ALL)
        {
          warn2 = IN_THIS;
        }
        if (warn2 > *warnp)
        {
          *warnp = warn2;
        }
      }
        continue;
      case 'D':
        pt = _fmt("%m/%d/%y", t, pt, ptlim, warnp);
        continue;
      case 'd':
        pt = _conv(t->tm_mday, "%02d", pt, ptlim);
        continue;
      case 'E':
      case 'O':

           
                                                                
                                                                   
                                                               
                            
           
        goto label;
      case 'e':
        pt = _conv(t->tm_mday, "%2d", pt, ptlim);
        continue;
      case 'F':
        pt = _fmt("%Y-%m-%d", t, pt, ptlim, warnp);
        continue;
      case 'H':
        pt = _conv(t->tm_hour, "%02d", pt, ptlim);
        continue;
      case 'I':
        pt = _conv((t->tm_hour % 12) ? (t->tm_hour % 12) : 12, "%02d", pt, ptlim);
        continue;
      case 'j':
        pt = _conv(t->tm_yday + 1, "%03d", pt, ptlim);
        continue;
      case 'k':

           
                                                                   
                                                                  
                                                             
                                                                  
                             
           
        pt = _conv(t->tm_hour, "%2d", pt, ptlim);
        continue;
#ifdef KITCHEN_SINK
      case 'K':

           
                                                 
           
        pt = _add("kitchen sink", pt, ptlim);
        continue;
#endif                           
      case 'l':

           
                                                                
                                                                  
                                                                   
                                                     
           
        pt = _conv((t->tm_hour % 12) ? (t->tm_hour % 12) : 12, "%2d", pt, ptlim);
        continue;
      case 'M':
        pt = _conv(t->tm_min, "%02d", pt, ptlim);
        continue;
      case 'm':
        pt = _conv(t->tm_mon + 1, "%02d", pt, ptlim);
        continue;
      case 'n':
        pt = _add("\n", pt, ptlim);
        continue;
      case 'p':
        pt = _add((t->tm_hour >= (HOURSPERDAY / 2)) ? Locale->pm : Locale->am, pt, ptlim);
        continue;
      case 'R':
        pt = _fmt("%H:%M", t, pt, ptlim, warnp);
        continue;
      case 'r':
        pt = _fmt("%I:%M:%S %p", t, pt, ptlim, warnp);
        continue;
      case 'S':
        pt = _conv(t->tm_sec, "%02d", pt, ptlim);
        continue;
      case 'T':
        pt = _fmt("%H:%M:%S", t, pt, ptlim, warnp);
        continue;
      case 't':
        pt = _add("\t", pt, ptlim);
        continue;
      case 'U':
        pt = _conv((t->tm_yday + DAYSPERWEEK - t->tm_wday) / DAYSPERWEEK, "%02d", pt, ptlim);
        continue;
      case 'u':

           
                                                                 
                                                               
                       
           
        pt = _conv((t->tm_wday == 0) ? DAYSPERWEEK : t->tm_wday, "%d", pt, ptlim);
        continue;
      case 'V':                           
      case 'G':                                  
      case 'g':                                 
                   
                                                                                      
                                                                                          
                             
                                     
                   
                                                                                        
                                                                                     
                                                                                         
                                                                                           
                                                                                           
                                                                                           
                                                                                         
                                                                                         
                                                                                         
                                                                
                                     
                   
        {
          int year;
          int base;
          int yday;
          int wday;
          int w;

          year = t->tm_year;
          base = TM_YEAR_BASE;
          yday = t->tm_yday;
          wday = t->tm_wday;
          for (;;)
          {
            int len;
            int bot;
            int top;

            len = isleap_sum(year, base) ? DAYSPERLYEAR : DAYSPERNYEAR;

               
                                                            
                   
               
            bot = ((yday + 11 - wday) % DAYSPERWEEK) - 3;

               
                                                          
               
            top = bot - (len % DAYSPERWEEK);
            if (top < -3)
            {
              top += DAYSPERWEEK;
            }
            top += len;
            if (yday >= top)
            {
              ++base;
              w = 1;
              break;
            }
            if (yday >= bot)
            {
              w = 1 + ((yday - bot) / DAYSPERWEEK);
              break;
            }
            --base;
            yday += isleap_sum(year, base) ? DAYSPERLYEAR : DAYSPERNYEAR;
          }
          if (*format == 'V')
          {
            pt = _conv(w, "%02d", pt, ptlim);
          }
          else if (*format == 'g')
          {
            *warnp = IN_ALL;
            pt = _yconv(year, base, false, true, pt, ptlim);
          }
          else
          {
            pt = _yconv(year, base, true, true, pt, ptlim);
          }
        }
        continue;
      case 'v':

           
                                                               
                                          
           
        pt = _fmt("%e-%b-%Y", t, pt, ptlim, warnp);
        continue;
      case 'W':
        pt = _conv((t->tm_yday + DAYSPERWEEK - (t->tm_wday ? (t->tm_wday - 1) : (DAYSPERWEEK - 1))) / DAYSPERWEEK, "%02d", pt, ptlim);
        continue;
      case 'w':
        pt = _conv(t->tm_wday, "%d", pt, ptlim);
        continue;
      case 'X':
        pt = _fmt(Locale->X_fmt, t, pt, ptlim, warnp);
        continue;
      case 'x':
      {
        enum warn warn2 = IN_SOME;

        pt = _fmt(Locale->x_fmt, t, pt, ptlim, &warn2);
        if (warn2 == IN_ALL)
        {
          warn2 = IN_THIS;
        }
        if (warn2 > *warnp)
        {
          *warnp = warn2;
        }
      }
        continue;
      case 'y':
        *warnp = IN_ALL;
        pt = _yconv(t->tm_year, TM_YEAR_BASE, false, true, pt, ptlim);
        continue;
      case 'Y':
        pt = _yconv(t->tm_year, TM_YEAR_BASE, true, true, pt, ptlim);
        continue;
      case 'Z':
        if (t->tm_zone != NULL)
        {
          pt = _add(t->tm_zone, pt, ptlim);
        }

           
                                                                   
                                                       
                         
           
        continue;
      case 'z':
      {
        long diff;
        char const *sign;
        bool negative;

        if (t->tm_isdst < 0)
        {
          continue;
        }
        diff = t->tm_gmtoff;
        negative = diff < 0;
        if (diff == 0)
        {
          if (t->tm_zone != NULL)
          {
            negative = t->tm_zone[0] == '-';
          }
        }
        if (negative)
        {
          sign = "-";
          diff = -diff;
        }
        else
        {
          sign = "+";
        }
        pt = _add(sign, pt, ptlim);
        diff /= SECSPERMIN;
        diff = (diff / MINSPERHOUR) * 100 + (diff % MINSPERHOUR);
        pt = _conv(diff, "%04d", pt, ptlim);
      }
        continue;
      case '+':
        pt = _fmt(Locale->date_fmt, t, pt, ptlim, warnp);
        continue;
      case '%':

           
                                                          
                                                           
                                                    
           
      default:
        break;
      }
    }
    if (pt == ptlim)
    {
      break;
    }
    *pt++ = *format;
  }
  return pt;
}

static char *
_conv(int n, const char *format, char *pt, const char *ptlim)
{
  char buf[INT_STRLEN_MAXIMUM(int) + 1];

  sprintf(buf, format, n);
  return _add(buf, pt, ptlim);
}

static char *
_add(const char *str, char *pt, const char *ptlim)
{
  while (pt < ptlim && (*pt = *str++) != '\0')
  {
    ++pt;
  }
  return pt;
}

   
                                                              
                                                              
                                                              
                                                             
                                
   

static char *
_yconv(int a, int b, bool convert_top, bool convert_yy, char *pt, const char *ptlim)
{
  int lead;
  int trail;

#define DIVISOR 100
  trail = a % DIVISOR + b % DIVISOR;
  lead = a / DIVISOR + b / DIVISOR + trail / DIVISOR;
  trail %= DIVISOR;
  if (trail < 0 && lead > 0)
  {
    trail += DIVISOR;
    --lead;
  }
  else if (lead < 0 && trail > 0)
  {
    trail -= DIVISOR;
    ++lead;
  }
  if (convert_top)
  {
    if (lead == 0 && trail < 0)
    {
      pt = _add("-0", pt, ptlim);
    }
    else
    {
      pt = _conv(lead, "%02d", pt, ptlim);
    }
  }
  if (convert_yy)
  {
    pt = _conv(((trail < 0) ? -trail : trail), "%02d", pt, ptlim);
  }
  return pt;
}
