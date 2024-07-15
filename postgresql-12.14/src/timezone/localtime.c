                                                        

   
                                                         
                                     
   
                  
                              
   

   
                                            
                                                                 
   

                                                                    
#include "c.h"

#include <fcntl.h>

#include "datatype/timestamp.h"
#include "pgtz.h"

#include "private.h"
#include "tzfile.h"

#ifndef WILDABBR
   
                                                                 
                                                                      
                    
                                                                      
                    
                                                                  
                                                     
                                                                  
                                              
                                                             
                                                           
                                                                   
                                                                      
                                                                          
                                                                          
                                                                          
                                                                
   
#define WILDABBR "   "
#endif                        

static const char wildabbr[] = WILDABBR;

static const char gmt[] = "GMT";

   
                                                                       
                                                                      
   
static struct state *tzdefrules_s = NULL;
static int tzdefrules_loaded = 0;

   
                                                                         
                                         
                                                 
                                                          
   
#define TZDEFRULESTRING ",M3.2.0,M11.1.0"

                                                             

enum r_type
{
  JULIAN_DAY,                                
  DAY_OF_YEAR,                               
  MONTH_NTH_DAY_OF_WEEK                                        
};

struct rule
{
  enum r_type r_type;                   
  int r_day;                                  
  int r_week;                                  
  int r_mon;                                    
  int32 r_time;                                    
};

   
                                    
   

static struct pg_tm *
gmtsub(pg_time_t const *, int32, struct pg_tm *);
static bool
increment_overflow(int *, int);
static bool
increment_overflow_time(pg_time_t *, int32);
static int64
leapcorr(struct state const *, pg_time_t);
static struct pg_tm *
timesub(pg_time_t const *, int32, struct state const *, struct pg_tm *);
static bool
typesequiv(struct state const *, int, int);

   
                                               
                                                               
                                                                
                                                               
                                          
   

static struct pg_tm tm;

                                                                    
static void
init_ttinfo(struct ttinfo *s, int32 utoff, bool isdst, int desigidx)
{
  s->tt_utoff = utoff;
  s->tt_isdst = isdst;
  s->tt_desigidx = desigidx;
  s->tt_ttisstd = false;
  s->tt_ttisut = false;
}

static int32
detzcode(const char *const codep)
{
  int32 result;
  int i;
  int32 one = 1;
  int32 halfmaxval = one << (32 - 2);
  int32 maxval = halfmaxval - 1 + halfmaxval;
  int32 minval = -1 - maxval;

  result = codep[0] & 0x7f;
  for (i = 1; i < 4; ++i)
  {
    result = (result << 8) | (codep[i] & 0xff);
  }

  if (codep[0] & 0x80)
  {
       
                                                                           
                                                         
       
    result -= !TWOS_COMPLEMENT(int32) && result != 0;
    result += minval;
  }
  return result;
}

static int64
detzcode64(const char *const codep)
{
  uint64 result;
  int i;
  int64 one = 1;
  int64 halfmaxval = one << (64 - 2);
  int64 maxval = halfmaxval - 1 + halfmaxval;
  int64 minval = -TWOS_COMPLEMENT(int64) - maxval;

  result = codep[0] & 0x7f;
  for (i = 1; i < 8; ++i)
  {
    result = (result << 8) | (codep[i] & 0xff);
  }

  if (codep[0] & 0x80)
  {
       
                                                                           
                                                         
       
    result -= !TWOS_COMPLEMENT(int64) && result != 0;
    result += minval;
  }
  return result;
}

static bool
differ_by_repeat(const pg_time_t t1, const pg_time_t t0)
{
  if (TYPE_BIT(pg_time_t) - TYPE_SIGNED(pg_time_t) < SECSPERREPEAT_BITS)
  {
    return 0;
  }
  return t1 - t0 == SECSPERREPEAT;
}

                                                          
union input_buffer
{
                                                               
  struct tzhead tzhead;

                           
  char buf[2 * sizeof(struct tzhead) + 2 * sizeof(struct state) + 4 * TZ_MAX_TIMES];
};

                                             
union local_storage
{
                                                                         
  struct file_analysis
  {
                            
    union input_buffer u;

                                                                      
    struct state st;
  } u;

                                           
};

                                                                  
                                                                     
                                       
                                                                             
                                                                           
   
static int
tzloadbody(char const *name, char *canonname, struct state *sp, bool doextend, union local_storage *lsp)
{
  int i;
  int fid;
  int stored;
  ssize_t nread;
  union input_buffer *up = &lsp->u.u;
  int tzheadsize = sizeof(struct tzhead);

  sp->goback = sp->goahead = false;

  if (!name)
  {
    name = TZDEFAULT;
    if (!name)
    {
      return EINVAL;
    }
  }

  if (name[0] == ':')
  {
    ++name;
  }

  fid = pg_open_tzfile(name, canonname);
  if (fid < 0)
  {
    return ENOENT;                                       
  }

  nread = read(fid, up->buf, sizeof up->buf);
  if (nread < tzheadsize)
  {
    int err = nread < 0 ? errno : EINVAL;

    close(fid);
    return err;
  }
  if (close(fid) < 0)
  {
    return errno;
  }
  for (stored = 4; stored <= 8; stored *= 2)
  {
    int32 ttisstdcnt = detzcode(up->tzhead.tzh_ttisstdcnt);
    int32 ttisutcnt = detzcode(up->tzhead.tzh_ttisutcnt);
    int64 prevtr = 0;
    int32 prevcorr = 0;
    int32 leapcnt = detzcode(up->tzhead.tzh_leapcnt);
    int32 timecnt = detzcode(up->tzhead.tzh_timecnt);
    int32 typecnt = detzcode(up->tzhead.tzh_typecnt);
    int32 charcnt = detzcode(up->tzhead.tzh_charcnt);
    char const *p = up->buf + tzheadsize;

       
                                                                    
                                                                        
                                            
       
    if (!(0 <= leapcnt && leapcnt < TZ_MAX_LEAPS && 0 <= typecnt && typecnt < TZ_MAX_TYPES && 0 <= timecnt && timecnt < TZ_MAX_TIMES && 0 <= charcnt && charcnt < TZ_MAX_CHARS && (ttisstdcnt == typecnt || ttisstdcnt == 0) && (ttisutcnt == typecnt || ttisutcnt == 0)))
    {
      return EINVAL;
    }
    if (nread < (tzheadsize                                     
                    + timecnt * stored                
                    + timecnt                           
                    + typecnt * 6                         
                    + charcnt                           
                    + leapcnt * (stored + 4)              
                    + ttisstdcnt                           
                    + ttisutcnt))                         
    {
      return EINVAL;
    }
    sp->leapcnt = leapcnt;
    sp->timecnt = timecnt;
    sp->typecnt = typecnt;
    sp->charcnt = charcnt;

       
                                                                      
                                                                 
                   
       
    timecnt = 0;
    for (i = 0; i < sp->timecnt; ++i)
    {
      int64 at = stored == 4 ? detzcode(p) : detzcode64(p);

      sp->types[i] = at <= TIME_T_MAX;
      if (sp->types[i])
      {
        pg_time_t attime = ((TYPE_SIGNED(pg_time_t) ? at < TIME_T_MIN : at < 0) ? TIME_T_MIN : at);

        if (timecnt && attime <= sp->ats[timecnt - 1])
        {
          if (attime < sp->ats[timecnt - 1])
          {
            return EINVAL;
          }
          sp->types[i - 1] = 0;
          timecnt--;
        }
        sp->ats[timecnt++] = attime;
      }
      p += stored;
    }

    timecnt = 0;
    for (i = 0; i < sp->timecnt; ++i)
    {
      unsigned char typ = *p++;

      if (sp->typecnt <= typ)
      {
        return EINVAL;
      }
      if (sp->types[i])
      {
        sp->types[timecnt++] = typ;
      }
    }
    sp->timecnt = timecnt;
    for (i = 0; i < sp->typecnt; ++i)
    {
      struct ttinfo *ttisp;
      unsigned char isdst, desigidx;

      ttisp = &sp->ttis[i];
      ttisp->tt_utoff = detzcode(p);
      p += 4;
      isdst = *p++;
      if (!(isdst < 2))
      {
        return EINVAL;
      }
      ttisp->tt_isdst = isdst;
      desigidx = *p++;
      if (!(desigidx < sp->charcnt))
      {
        return EINVAL;
      }
      ttisp->tt_desigidx = desigidx;
    }
    for (i = 0; i < sp->charcnt; ++i)
    {
      sp->chars[i] = *p++;
    }
    sp->chars[i] = '\0';                         

                                                                      
    leapcnt = 0;
    for (i = 0; i < sp->leapcnt; ++i)
    {
      int64 tr = stored == 4 ? detzcode(p) : detzcode64(p);
      int32 corr = detzcode(p + stored);

      p += stored + 4;
                                                        
      if (tr < 0)
      {
        return EINVAL;
      }
      if (tr <= TIME_T_MAX)
      {
           
                                                                       
                                                                      
                                                                      
                                                       
           
        if (tr - prevtr < 28 * SECSPERDAY - 1 || (corr != prevcorr - 1 && corr != prevcorr + 1))
        {
          return EINVAL;
        }
        sp->lsis[leapcnt].ls_trans = prevtr = tr;
        sp->lsis[leapcnt].ls_corr = prevcorr = corr;
        leapcnt++;
      }
    }
    sp->leapcnt = leapcnt;

    for (i = 0; i < sp->typecnt; ++i)
    {
      struct ttinfo *ttisp;

      ttisp = &sp->ttis[i];
      if (ttisstdcnt == 0)
      {
        ttisp->tt_ttisstd = false;
      }
      else
      {
        if (*p != true && *p != false)
        {
          return EINVAL;
        }
        ttisp->tt_ttisstd = *p++;
      }
    }
    for (i = 0; i < sp->typecnt; ++i)
    {
      struct ttinfo *ttisp;

      ttisp = &sp->ttis[i];
      if (ttisutcnt == 0)
      {
        ttisp->tt_ttisut = false;
      }
      else
      {
        if (*p != true && *p != false)
        {
          return EINVAL;
        }
        ttisp->tt_ttisut = *p++;
      }
    }

       
                                           
       
    if (up->tzhead.tzh_version[0] == '\0')
    {
      break;
    }
    nread -= p - up->buf;
    memmove(up->buf, p, nread);
  }
  if (doextend && nread > 2 && up->buf[0] == '\n' && up->buf[nread - 1] == '\n' && sp->typecnt + 2 <= TZ_MAX_TYPES)
  {
    struct state *ts = &lsp->u.st;

    up->buf[nread - 1] = '\0';
    if (tzparse(&up->buf[1], ts, false))
    {
         
                                                                
                                                                      
                                                                       
                                                                     
                                                                    
                  
         
      int gotabbr = 0;
      int charcnt = sp->charcnt;

      for (i = 0; i < ts->typecnt; i++)
      {
        char *tsabbr = ts->chars + ts->ttis[i].tt_desigidx;
        int j;

        for (j = 0; j < charcnt; j++)
        {
          if (strcmp(sp->chars + j, tsabbr) == 0)
          {
            ts->ttis[i].tt_desigidx = j;
            gotabbr++;
            break;
          }
        }
        if (!(j < charcnt))
        {
          int tsabbrlen = strlen(tsabbr);

          if (j + tsabbrlen < TZ_MAX_CHARS)
          {
            strcpy(sp->chars + j, tsabbr);
            charcnt = j + tsabbrlen + 1;
            ts->ttis[i].tt_desigidx = j;
            gotabbr++;
          }
        }
      }
      if (gotabbr == ts->typecnt)
      {
        sp->charcnt = charcnt;

           
                                                                      
                                                                       
                       
           
        while (1 < sp->timecnt && (sp->types[sp->timecnt - 1] == sp->types[sp->timecnt - 2]))
        {
          sp->timecnt--;
        }

        for (i = 0; i < ts->timecnt; i++)
        {
          if (sp->timecnt == 0 || (sp->ats[sp->timecnt - 1] < ts->ats[i] + leapcorr(sp, ts->ats[i])))
          {
            break;
          }
        }
        while (i < ts->timecnt && sp->timecnt < TZ_MAX_TIMES)
        {
          sp->ats[sp->timecnt] = ts->ats[i] + leapcorr(sp, ts->ats[i]);
          sp->types[sp->timecnt] = (sp->typecnt + ts->types[i]);
          sp->timecnt++;
          i++;
        }
        for (i = 0; i < ts->typecnt; i++)
        {
          sp->ttis[sp->typecnt++] = ts->ttis[i];
        }
      }
    }
  }
  if (sp->typecnt == 0)
  {
    return EINVAL;
  }
  if (sp->timecnt > 1)
  {
    for (i = 1; i < sp->timecnt; ++i)
    {
      if (typesequiv(sp, sp->types[i], sp->types[0]) && differ_by_repeat(sp->ats[i], sp->ats[0]))
      {
        sp->goback = true;
        break;
      }
    }
    for (i = sp->timecnt - 2; i >= 0; --i)
    {
      if (typesequiv(sp, sp->types[sp->timecnt - 1], sp->types[i]) && differ_by_repeat(sp->ats[sp->timecnt - 1], sp->ats[i]))
      {
        sp->goahead = true;
        break;
      }
    }
  }

     
                                                                         
                                                                             
                                      
     
                                                                           
                                                                 
                                                                             
                                                                  
                                                            
     

     
                                                                        
            
     
  for (i = 0; i < sp->timecnt; ++i)
  {
    if (sp->types[i] == 0)
    {
      break;
    }
  }
  i = i < sp->timecnt ? -1 : 0;

     
                                                                   
                                                                           
                                                  
     
  if (i < 0 && sp->timecnt > 0 && sp->ttis[sp->types[0]].tt_isdst)
  {
    i = sp->types[0];
    while (--i >= 0)
    {
      if (!sp->ttis[i].tt_isdst)
      {
        break;
      }
    }
  }

     
                                                                          
                                                                  
     

     
                                                                            
                   
     
  if (i < 0)
  {
    i = 0;
    while (sp->ttis[i].tt_isdst)
    {
      if (++i >= sp->typecnt)
      {
        i = 0;
        break;
      }
    }
  }

     
                                                                             
                                                                             
                                                     
     
  sp->defaulttype = i;

  return 0;
}

                                                                  
                                                                        
                                                                             
                                                                           
   
int
tzload(const char *name, char *canonname, struct state *sp, bool doextend)
{
  union local_storage *lsp = malloc(sizeof *lsp);

  if (!lsp)
  {
    return errno;
  }
  else
  {
    int err = tzloadbody(name, canonname, sp, doextend, lsp);

    free(lsp);
    return err;
  }
}

static bool
typesequiv(const struct state *sp, int a, int b)
{
  bool result;

  if (sp == NULL || a < 0 || a >= sp->typecnt || b < 0 || b >= sp->typecnt)
  {
    result = false;
  }
  else
  {
    const struct ttinfo *ap = &sp->ttis[a];
    const struct ttinfo *bp = &sp->ttis[b];

    result = (ap->tt_utoff == bp->tt_utoff && ap->tt_isdst == bp->tt_isdst && ap->tt_ttisstd == bp->tt_ttisstd && ap->tt_ttisut == bp->tt_ttisut && (strcmp(&sp->chars[ap->tt_desigidx], &sp->chars[bp->tt_desigidx]) == 0));
  }
  return result;
}

static const int mon_lengths[2][MONSPERYEAR] = {{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}, {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}};

static const int year_lengths[2] = {DAYSPERNYEAR, DAYSPERLYEAR};

   
                                                                              
                                                           
                                       
   

static const char *
getzname(const char *strp)
{
  char c;

  while ((c = *strp) != '\0' && !is_digit(c) && c != ',' && c != '-' && c != '+')
  {
    ++strp;
  }
  return strp;
}

   
                                                                           
                                                       
                                      
   
                                                                     
                                                                  
                                                                              
   

static const char *
getqzname(const char *strp, const int delim)
{
  int c;

  while ((c = *strp) != '\0' && c != delim)
  {
    ++strp;
  }
  return strp;
}

   
                                                                              
                                                                           
         
                                                                              
   

static const char *
getnum(const char *strp, int *const nump, const int min, const int max)
{
  char c;
  int num;

  if (strp == NULL || !is_digit(c = *strp))
  {
    return NULL;
  }
  num = 0;
  do
  {
    num = num * 10 + (c - '0');
    if (num > max)
    {
      return NULL;                    
    }
    c = *++strp;
  } while (is_digit(c));
  if (num < min)
  {
    return NULL;                    
  }
  *nump = num;
  return strp;
}

   
                                                                        
                                          
                                     
                                                                             
               
   

static const char *
getsecs(const char *strp, int32 *const secsp)
{
  int num;

     
                                                                   
                                                                            
                                                                   
     
  strp = getnum(strp, &num, 0, HOURSPERDAY * DAYSPERWEEK - 1);
  if (strp == NULL)
  {
    return NULL;
  }
  *secsp = num * (int32)SECSPERHOUR;
  if (*strp == ':')
  {
    ++strp;
    strp = getnum(strp, &num, 0, MINSPERHOUR - 1);
    if (strp == NULL)
    {
      return NULL;
    }
    *secsp += num * SECSPERMIN;
    if (*strp == ':')
    {
      ++strp;
                                                  
      strp = getnum(strp, &num, 0, SECSPERMIN);
      if (strp == NULL)
      {
        return NULL;
      }
      *secsp += num;
    }
  }
  return strp;
}

   
                                                                 
                                           
                                     
                                                                            
   

static const char *
getoffset(const char *strp, int32 *const offsetp)
{
  bool neg = false;

  if (*strp == '-')
  {
    neg = true;
    ++strp;
  }
  else if (*strp == '+')
  {
    ++strp;
  }
  strp = getsecs(strp, offsetp);
  if (strp == NULL)
  {
    return NULL;                   
  }
  if (neg)
  {
    *offsetp = -*offsetp;
  }
  return strp;
}

   
                                                                      
                                                                         
                                              
                                                                            
   

static const char *
getrule(const char *strp, struct rule *const rulep)
{
  if (*strp == 'J')
  {
       
                   
       
    rulep->r_type = JULIAN_DAY;
    ++strp;
    strp = getnum(strp, &rulep->r_day, 1, DAYSPERNYEAR);
  }
  else if (*strp == 'M')
  {
       
                         
       
    rulep->r_type = MONTH_NTH_DAY_OF_WEEK;
    ++strp;
    strp = getnum(strp, &rulep->r_mon, 1, MONSPERYEAR);
    if (strp == NULL)
    {
      return NULL;
    }
    if (*strp++ != '.')
    {
      return NULL;
    }
    strp = getnum(strp, &rulep->r_week, 1, 5);
    if (strp == NULL)
    {
      return NULL;
    }
    if (*strp++ != '.')
    {
      return NULL;
    }
    strp = getnum(strp, &rulep->r_day, 0, DAYSPERWEEK - 1);
  }
  else if (is_digit(*strp))
  {
       
                    
       
    rulep->r_type = DAY_OF_YEAR;
    strp = getnum(strp, &rulep->r_day, 0, DAYSPERLYEAR - 1);
  }
  else
  {
    return NULL;                     
  }
  if (strp == NULL)
  {
    return NULL;
  }
  if (*strp == '/')
  {
       
                       
       
    ++strp;
    strp = getoffset(strp, &rulep->r_time);
  }
  else
  {
    rulep->r_time = 2 * SECSPERHOUR;                        
  }
  return strp;
}

   
                                                                            
                                                                    
   

static int32
transtime(const int year, const struct rule *const rulep, const int32 offset)
{
  bool leapyear;
  int32 value;
  int i;
  int d, m1, yy0, yy1, yy2, dow;

  INITIALIZE(value);
  leapyear = isleap(year);
  switch (rulep->r_type)
  {

  case JULIAN_DAY:

       
                                                                   
                                                                     
                                                                 
                                            
       
    value = (rulep->r_day - 1) * SECSPERDAY;
    if (leapyear && rulep->r_day >= 60)
    {
      value += SECSPERDAY;
    }
    break;

  case DAY_OF_YEAR:

       
                                                                    
                                                        
       
    value = rulep->r_day * SECSPERDAY;
    break;

  case MONTH_NTH_DAY_OF_WEEK:

       
                                          
       

       
                                                                  
              
       
    m1 = (rulep->r_mon + 9) % 12 + 1;
    yy0 = (rulep->r_mon <= 2) ? (year - 1) : year;
    yy1 = yy0 / 100;
    yy2 = yy0 % 100;
    dow = ((26 * m1 - 2) / 10 + 1 + yy2 + yy2 / 4 + yy1 / 4 - 2 * yy1) % 7;
    if (dow < 0)
    {
      dow += DAYSPERWEEK;
    }

       
                                                                       
                                                                       
       
    d = rulep->r_day - dow;
    if (d < 0)
    {
      d += DAYSPERWEEK;
    }
    for (i = 1; i < rulep->r_week; ++i)
    {
      if (d + DAYSPERWEEK >= mon_lengths[(int)leapyear][rulep->r_mon - 1])
      {
        break;
      }
      d += DAYSPERWEEK;
    }

       
                                                                 
       
    value = d * SECSPERDAY;
    for (i = 0; i < rulep->r_mon - 1; ++i)
    {
      value += mon_lengths[(int)leapyear][i] * SECSPERDAY;
    }
    break;
  }

     
                                                                    
                                                                            
                                                                       
     
  return value + rulep->r_time + offset;
}

   
                                                                       
                
                                              
   
bool
tzparse(const char *name, struct state *sp, bool lastditch)
{
  const char *stdname;
  const char *dstname = NULL;
  size_t stdlen;
  size_t dstlen;
  size_t charcnt;
  int32 stdoffset;
  int32 dstoffset;
  char *cp;
  bool load_ok;

  stdname = name;
  if (lastditch)
  {
                                                         
    stdlen = strlen(name);                                   
    name += stdlen;
    stdoffset = 0;
  }
  else
  {
    if (*name == '<')
    {
      name++;
      stdname = name;
      name = getqzname(name, '>');
      if (*name != '>')
      {
        return false;
      }
      stdlen = name - stdname;
      name++;
    }
    else
    {
      name = getzname(name);
      stdlen = name - stdname;
    }
    if (*name == '\0')                                             
    {
      return false;
    }
    name = getoffset(name, &stdoffset);
    if (name == NULL)
    {
      return false;
    }
  }
  charcnt = stdlen + 1;
  if (sizeof sp->chars < charcnt)
  {
    return false;
  }

     
                                                                            
                                                                         
                                                                           
                                                                             
                                                                           
                                                                         
                                                                   
     
  sp->goback = sp->goahead = false;                               
  sp->leapcnt = 0;                                                            

  if (*name != '\0')
  {
    if (*name == '<')
    {
      dstname = ++name;
      name = getqzname(name, '>');
      if (*name != '>')
      {
        return false;
      }
      dstlen = name - dstname;
      name++;
    }
    else
    {
      dstname = name;
      name = getzname(name);
      dstlen = name - dstname;                          
    }
    if (!dstlen)
    {
      return false;
    }
    charcnt += dstlen + 1;
    if (sizeof sp->chars < charcnt)
    {
      return false;
    }
    if (*name != '\0' && *name != ',' && *name != ';')
    {
      name = getoffset(name, &dstoffset);
      if (name == NULL)
      {
        return false;
      }
    }
    else
    {
      dstoffset = stdoffset - SECSPERHOUR;
    }
    if (*name == '\0')
    {
         
                                                                      
                                                                         
                                                                         
                                                         
         
      if (tzdefrules_loaded == 0)
      {
                                   
        if (tzdefrules_s == NULL)
        {
          tzdefrules_s = (struct state *)malloc(sizeof(struct state));
        }
        if (tzdefrules_s != NULL)
        {
          if (tzload(TZDEFRULES, NULL, tzdefrules_s, false) == 0)
          {
            tzdefrules_loaded = 1;
          }
          else
          {
            tzdefrules_loaded = -1;
          }
                                                                     
          tzdefrules_s->leapcnt = 0;
        }
      }
      load_ok = (tzdefrules_loaded > 0);
      if (load_ok)
      {
        memcpy(sp, tzdefrules_s, sizeof(struct state));
      }
      else
      {
                                                                       
        name = TZDEFRULESTRING;
      }
    }
    if (*name == ',' || *name == ';')
    {
      struct rule start;
      struct rule end;
      int year;
      int yearlim;
      int timecnt;
      pg_time_t janfirst;
      int32 janoffset = 0;
      int yearbeg;

      ++name;
      if ((name = getrule(name, &start)) == NULL)
      {
        return false;
      }
      if (*name++ != ',')
      {
        return false;
      }
      if ((name = getrule(name, &end)) == NULL)
      {
        return false;
      }
      if (*name != '\0')
      {
        return false;
      }
      sp->typecnt = 2;                            

         
                                                            
         
      init_ttinfo(&sp->ttis[0], -stdoffset, false, 0);
      init_ttinfo(&sp->ttis[1], -dstoffset, true, stdlen + 1);
      sp->defaulttype = 0;
      timecnt = 0;
      janfirst = 0;
      yearbeg = EPOCH_YEAR;

      do
      {
        int32 yearsecs = year_lengths[isleap(yearbeg - 1)] * SECSPERDAY;

        yearbeg--;
        if (increment_overflow_time(&janfirst, -yearsecs))
        {
          janoffset = -yearsecs;
          break;
        }
      } while (EPOCH_YEAR - YEARSPERREPEAT / 2 < yearbeg);

      yearlim = yearbeg + YEARSPERREPEAT + 1;
      for (year = yearbeg; year < yearlim; year++)
      {
        int32 starttime = transtime(year, &start, stdoffset), endtime = transtime(year, &end, dstoffset);
        int32 yearsecs = (year_lengths[isleap(year)] * SECSPERDAY);
        bool reversed = endtime < starttime;

        if (reversed)
        {
          int32 swap = starttime;

          starttime = endtime;
          endtime = swap;
        }
        if (reversed || (starttime < endtime && (endtime - starttime < (yearsecs + (stdoffset - dstoffset)))))
        {
          if (TZ_MAX_TIMES - 2 < timecnt)
          {
            break;
          }
          sp->ats[timecnt] = janfirst;
          if (!increment_overflow_time(&sp->ats[timecnt], janoffset + starttime))
          {
            sp->types[timecnt++] = !reversed;
          }
          sp->ats[timecnt] = janfirst;
          if (!increment_overflow_time(&sp->ats[timecnt], janoffset + endtime))
          {
            sp->types[timecnt++] = reversed;
            yearlim = year + YEARSPERREPEAT + 1;
          }
        }
        if (increment_overflow_time(&janfirst, janoffset + yearsecs))
        {
          break;
        }
        janoffset = 0;
      }
      sp->timecnt = timecnt;
      if (!timecnt)
      {
        sp->ttis[0] = sp->ttis[1];
        sp->typecnt = 1;                      
      }
      else if (YEARSPERREPEAT < year - yearbeg)
      {
        sp->goback = sp->goahead = true;
      }
    }
    else
    {
      int32 theirstdoffset;
      int32 theirdstoffset;
      int32 theiroffset;
      bool isdst;
      int i;
      int j;

      if (*name != '\0')
      {
        return false;
      }

         
                                                              
         
      theirstdoffset = 0;
      for (i = 0; i < sp->timecnt; ++i)
      {
        j = sp->types[i];
        if (!sp->ttis[j].tt_isdst)
        {
          theirstdoffset = -sp->ttis[j].tt_utoff;
          break;
        }
      }
      theirdstoffset = 0;
      for (i = 0; i < sp->timecnt; ++i)
      {
        j = sp->types[i];
        if (sp->ttis[j].tt_isdst)
        {
          theirdstoffset = -sp->ttis[j].tt_utoff;
          break;
        }
      }

         
                                                         
         
      isdst = false;
      theiroffset = theirstdoffset;

         
                                                                       
             
         
      for (i = 0; i < sp->timecnt; ++i)
      {
        j = sp->types[i];
        sp->types[i] = sp->ttis[j].tt_isdst;
        if (sp->ttis[j].tt_ttisut)
        {
                                                
        }
        else
        {
             
                                                           
                                                                     
                                                                     
                                                            
                              
             
             
                                                                     
                                                           
             
          if (isdst && !sp->ttis[j].tt_ttisstd)
          {
            sp->ats[i] += dstoffset - theirdstoffset;
          }
          else
          {
            sp->ats[i] += stdoffset - theirstdoffset;
          }
        }
        theiroffset = -sp->ttis[j].tt_utoff;
        if (sp->ttis[j].tt_isdst)
        {
          theirdstoffset = theiroffset;
        }
        else
        {
          theirstdoffset = theiroffset;
        }
      }

         
                                
         
      init_ttinfo(&sp->ttis[0], -stdoffset, false, 0);
      init_ttinfo(&sp->ttis[1], -dstoffset, true, stdlen + 1);
      sp->typecnt = 2;
      sp->defaulttype = 0;
    }
  }
  else
  {
    dstlen = 0;
    sp->typecnt = 1;                         
    sp->timecnt = 0;
    init_ttinfo(&sp->ttis[0], -stdoffset, false, 0);
    sp->defaulttype = 0;
  }
  sp->charcnt = charcnt;
  cp = sp->chars;
  memcpy(cp, stdname, stdlen);
  cp += stdlen;
  *cp++ = '\0';
  if (dstlen != 0)
  {
    memcpy(cp, dstname, dstlen);
    *(cp + dstlen) = '\0';
  }
  return true;
}

static void
gmtload(struct state *const sp)
{
  if (tzload(gmt, NULL, sp, true) != 0)
  {
    tzparse(gmt, sp, true);
  }
}

   
                                                                      
                                                                        
                                                                        
                           
   
static struct pg_tm *
localsub(struct state const *sp, pg_time_t const *timep, struct pg_tm *const tmp)
{
  const struct ttinfo *ttisp;
  int i;
  struct pg_tm *result;
  const pg_time_t t = *timep;

  if (sp == NULL)
  {
    return gmtsub(timep, 0, tmp);
  }
  if ((sp->goback && t < sp->ats[0]) || (sp->goahead && t > sp->ats[sp->timecnt - 1]))
  {
    pg_time_t newt = t;
    pg_time_t seconds;
    pg_time_t years;

    if (t < sp->ats[0])
    {
      seconds = sp->ats[0] - t;
    }
    else
    {
      seconds = t - sp->ats[sp->timecnt - 1];
    }
    --seconds;
    years = (seconds / SECSPERREPEAT + 1) * YEARSPERREPEAT;
    seconds = years * AVGSECSPERYEAR;
    if (t < sp->ats[0])
    {
      newt += seconds;
    }
    else
    {
      newt -= seconds;
    }
    if (newt < sp->ats[0] || newt > sp->ats[sp->timecnt - 1])
    {
      return NULL;                      
    }
    result = localsub(sp, &newt, tmp);
    if (result)
    {
      int64 newy;

      newy = result->tm_year;
      if (t < sp->ats[0])
      {
        newy -= years;
      }
      else
      {
        newy += years;
      }
      if (!(INT_MIN <= newy && newy <= INT_MAX))
      {
        return NULL;
      }
      result->tm_year = newy;
    }
    return result;
  }
  if (sp->timecnt == 0 || t < sp->ats[0])
  {
    i = sp->defaulttype;
  }
  else
  {
    int lo = 1;
    int hi = sp->timecnt;

    while (lo < hi)
    {
      int mid = (lo + hi) >> 1;

      if (t < sp->ats[mid])
      {
        hi = mid;
      }
      else
      {
        lo = mid + 1;
      }
    }
    i = (int)sp->types[lo - 1];
  }
  ttisp = &sp->ttis[i];

     
                                                                         
                                                                  
                               
     
  result = timesub(&t, ttisp->tt_utoff, sp, tmp);
  if (result)
  {
    result->tm_isdst = ttisp->tt_isdst;
    result->tm_zone = unconstify(char *, &sp->chars[ttisp->tt_desigidx]);
  }
  return result;
}

struct pg_tm *
pg_localtime(const pg_time_t *timep, const pg_tz *tz)
{
  return localsub(&tz->state, timep, &tm);
}

   
                                                    
   
                                                                           
   

static struct pg_tm *
gmtsub(pg_time_t const *timep, int32 offset, struct pg_tm *tmp)
{
  struct pg_tm *result;

                                            
  static struct state *gmtptr = NULL;

  if (gmtptr == NULL)
  {
                               
    gmtptr = (struct state *)malloc(sizeof(struct state));
    if (gmtptr == NULL)
    {
      return NULL;                                    
    }
    gmtload(gmtptr);
  }

  result = timesub(timep, offset, gmtptr, tmp);

     
                                                                          
                                                                  
     
  if (offset != 0)
  {
    tmp->tm_zone = wildabbr;
  }
  else
  {
    tmp->tm_zone = gmtptr->chars;
  }

  return result;
}

struct pg_tm *
pg_gmtime(const pg_time_t *timep)
{
  return gmtsub(timep, 0, &tm);
}

   
                                                                     
                                                                              
   

static int
leaps_thru_end_of_nonneg(int y)
{
  return y / 4 - y / 100 + y / 400;
}

static int
leaps_thru_end_of(const int y)
{
  return (y < 0 ? -1 - leaps_thru_end_of_nonneg(-1 - y) : leaps_thru_end_of_nonneg(y));
}

static struct pg_tm *
timesub(const pg_time_t *timep, int32 offset, const struct state *sp, struct pg_tm *tmp)
{
  const struct lsinfo *lp;
  pg_time_t tdays;
  int idays;                                
  int64 rem;
  int y;
  const int *ip;
  int64 corr;
  bool hit;
  int i;

  corr = 0;
  hit = false;
  i = (sp == NULL) ? 0 : sp->leapcnt;
  while (--i >= 0)
  {
    lp = &sp->lsis[i];
    if (*timep >= lp->ls_trans)
    {
      corr = lp->ls_corr;
      hit = (*timep == lp->ls_trans && (i == 0 ? 0 : lp[-1].ls_corr) < corr);
      break;
    }
  }
  y = EPOCH_YEAR;
  tdays = *timep / SECSPERDAY;
  rem = *timep % SECSPERDAY;
  while (tdays < 0 || tdays >= year_lengths[isleap(y)])
  {
    int newy;
    pg_time_t tdelta;
    int idelta;
    int leapdays;

    tdelta = tdays / DAYSPERLYEAR;
    if (!((!TYPE_SIGNED(pg_time_t) || INT_MIN <= tdelta) && tdelta <= INT_MAX))
    {
      goto out_of_range;
    }
    idelta = tdelta;
    if (idelta == 0)
    {
      idelta = (tdays < 0) ? -1 : 1;
    }
    newy = y;
    if (increment_overflow(&newy, idelta))
    {
      goto out_of_range;
    }
    leapdays = leaps_thru_end_of(newy - 1) - leaps_thru_end_of(y - 1);
    tdays -= ((pg_time_t)newy - y) * DAYSPERNYEAR;
    tdays -= leapdays;
    y = newy;
  }

     
                                                    
     
  idays = tdays;
  rem += offset - corr;
  while (rem < 0)
  {
    rem += SECSPERDAY;
    --idays;
  }
  while (rem >= SECSPERDAY)
  {
    rem -= SECSPERDAY;
    ++idays;
  }
  while (idays < 0)
  {
    if (increment_overflow(&y, -1))
    {
      goto out_of_range;
    }
    idays += year_lengths[isleap(y)];
  }
  while (idays >= year_lengths[isleap(y)])
  {
    idays -= year_lengths[isleap(y)];
    if (increment_overflow(&y, 1))
    {
      goto out_of_range;
    }
  }
  tmp->tm_year = y;
  if (increment_overflow(&tmp->tm_year, -TM_YEAR_BASE))
  {
    goto out_of_range;
  }
  tmp->tm_yday = idays;

     
                                                     
     
  tmp->tm_wday = EPOCH_WDAY + ((y - EPOCH_YEAR) % DAYSPERWEEK) * (DAYSPERNYEAR % DAYSPERWEEK) + leaps_thru_end_of(y - 1) - leaps_thru_end_of(EPOCH_YEAR - 1) + idays;
  tmp->tm_wday %= DAYSPERWEEK;
  if (tmp->tm_wday < 0)
  {
    tmp->tm_wday += DAYSPERWEEK;
  }
  tmp->tm_hour = (int)(rem / SECSPERHOUR);
  rem %= SECSPERHOUR;
  tmp->tm_min = (int)(rem / SECSPERMIN);

     
                                                                         
                            
     
  tmp->tm_sec = (int)(rem % SECSPERMIN) + hit;
  ip = mon_lengths[isleap(y)];
  for (tmp->tm_mon = 0; idays >= ip[tmp->tm_mon]; ++(tmp->tm_mon))
  {
    idays -= ip[tmp->tm_mon];
  }
  tmp->tm_mday = (int)(idays + 1);
  tmp->tm_isdst = 0;
  tmp->tm_gmtoff = offset;
  return tmp;

out_of_range:
  errno = EOVERFLOW;
  return NULL;
}

   
                                         
   

static bool
increment_overflow(int *ip, int j)
{
  int const i = *ip;

               
                                                             
                                                                       
                                                            
                                                                      
               
     
  if ((i >= 0) ? (j > INT_MAX - i) : (j < INT_MIN - i))
  {
    return true;
  }
  *ip += j;
  return false;
}

static bool
increment_overflow_time(pg_time_t *tp, int32 j)
{
               
                  
                                                                    
                                                                         
               
     
  if (!(j < 0 ? (TYPE_SIGNED(pg_time_t) ? TIME_T_MIN - j <= *tp : -1 - j < *tp) : *tp <= TIME_T_MAX - j))
  {
    return true;
  }
  *tp += j;
  return false;
}

static int64
leapcorr(struct state const *sp, pg_time_t t)
{
  struct lsinfo const *lp;
  int i;

  i = sp->leapcnt;
  while (--i >= 0)
  {
    lp = &sp->lsis[i];
    if (t >= lp->ls_trans)
    {
      return lp->ls_corr;
    }
  }
  return 0;
}

   
                                                                            
   
                                                                               
   
                                                                    
                                                                
                                                                        
                                                                        
                                                                        
                                                  
   
                                                                   
                                                                       
                                                                      
                                                                        
   
                                                                          
                                         
   
int
pg_next_dst_boundary(const pg_time_t *timep, long int *before_gmtoff, int *before_isdst, pg_time_t *boundary, long int *after_gmtoff, int *after_isdst, const pg_tz *tz)
{
  const struct state *sp;
  const struct ttinfo *ttisp;
  int i;
  int j;
  const pg_time_t t = *timep;

  sp = &tz->state;
  if (sp->timecnt == 0)
  {
                                                         
    i = 0;
    while (sp->ttis[i].tt_isdst)
    {
      if (++i >= sp->typecnt)
      {
        i = 0;
        break;
      }
    }
    ttisp = &sp->ttis[i];
    *before_gmtoff = ttisp->tt_utoff;
    *before_isdst = ttisp->tt_isdst;
    return 0;
  }
  if ((sp->goback && t < sp->ats[0]) || (sp->goahead && t > sp->ats[sp->timecnt - 1]))
  {
                                                              
    pg_time_t newt = t;
    pg_time_t seconds;
    pg_time_t tcycles;
    int64 icycles;
    int result;

    if (t < sp->ats[0])
    {
      seconds = sp->ats[0] - t;
    }
    else
    {
      seconds = t - sp->ats[sp->timecnt - 1];
    }
    --seconds;
    tcycles = seconds / YEARSPERREPEAT / AVGSECSPERYEAR;
    ++tcycles;
    icycles = tcycles;
    if (tcycles - icycles >= 1 || icycles - tcycles >= 1)
    {
      return -1;
    }
    seconds = icycles;
    seconds *= YEARSPERREPEAT;
    seconds *= AVGSECSPERYEAR;
    if (t < sp->ats[0])
    {
      newt += seconds;
    }
    else
    {
      newt -= seconds;
    }
    if (newt < sp->ats[0] || newt > sp->ats[sp->timecnt - 1])
    {
      return -1;                      
    }

    result = pg_next_dst_boundary(&newt, before_gmtoff, before_isdst, boundary, after_gmtoff, after_isdst, tz);
    if (t < sp->ats[0])
    {
      *boundary -= seconds;
    }
    else
    {
      *boundary += seconds;
    }
    return result;
  }

  if (t >= sp->ats[sp->timecnt - 1])
  {
                                                                   
    i = sp->types[sp->timecnt - 1];
    ttisp = &sp->ttis[i];
    *before_gmtoff = ttisp->tt_utoff;
    *before_isdst = ttisp->tt_isdst;
    return 0;
  }
  if (t < sp->ats[0])
  {
                                                         
    i = 0;
    while (sp->ttis[i].tt_isdst)
    {
      if (++i >= sp->typecnt)
      {
        i = 0;
        break;
      }
    }
    ttisp = &sp->ttis[i];
    *before_gmtoff = ttisp->tt_utoff;
    *before_isdst = ttisp->tt_isdst;
    *boundary = sp->ats[0];
                                                       
    i = sp->types[0];
    ttisp = &sp->ttis[i];
    *after_gmtoff = ttisp->tt_utoff;
    *after_isdst = ttisp->tt_isdst;
    return 1;
  }
                                                    
  {
    int lo = 1;
    int hi = sp->timecnt - 1;

    while (lo < hi)
    {
      int mid = (lo + hi) >> 1;

      if (t < sp->ats[mid])
      {
        hi = mid;
      }
      else
      {
        lo = mid + 1;
      }
    }
    i = lo;
  }
  j = sp->types[i - 1];
  ttisp = &sp->ttis[j];
  *before_gmtoff = ttisp->tt_utoff;
  *before_isdst = ttisp->tt_isdst;
  *boundary = sp->ats[i];
  j = sp->types[i];
  ttisp = &sp->ttis[j];
  *after_gmtoff = ttisp->tt_utoff;
  *after_isdst = ttisp->tt_isdst;
  return 1;
}

   
                                                                
   
                                                                           
                                                                          
                                                                            
                                                                           
                                                                          
   
                                                                              
                                                      
   
                                                                          
   
bool
pg_interpret_timezone_abbrev(const char *abbrev, const pg_time_t *timep, long int *gmtoff, int *isdst, const pg_tz *tz)
{
  const struct state *sp;
  const char *abbrs;
  const struct ttinfo *ttisp;
  int abbrind;
  int cutoff;
  int i;
  const pg_time_t t = *timep;

  sp = &tz->state;

     
                                                                         
                                           
     
  abbrs = sp->chars;
  abbrind = 0;
  while (abbrind < sp->charcnt)
  {
    if (strcmp(abbrev, abbrs + abbrind) == 0)
    {
      break;
    }
    while (abbrs[abbrind] != '\0')
    {
      abbrind++;
    }
    abbrind++;
  }
  if (abbrind >= sp->charcnt)
  {
    return false;                 
  }

     
                                                                       
                                                                          
                                                                             
                                                 
     
                                                                     
     
  {
    int lo = 0;
    int hi = sp->timecnt;

    while (lo < hi)
    {
      int mid = (lo + hi) >> 1;

      if (t < sp->ats[mid])
      {
        hi = mid;
      }
      else
      {
        lo = mid + 1;
      }
    }
    cutoff = lo;
  }

     
                                                                       
                             
     
  for (i = cutoff - 1; i >= 0; i--)
  {
    ttisp = &sp->ttis[sp->types[i]];
    if (ttisp->tt_desigidx == abbrind)
    {
      *gmtoff = ttisp->tt_utoff;
      *isdst = ttisp->tt_isdst;
      return true;
    }
  }

     
                                                              
     
  for (i = cutoff; i < sp->timecnt; i++)
  {
    ttisp = &sp->ttis[sp->types[i]];
    if (ttisp->tt_desigidx == abbrind)
    {
      *gmtoff = ttisp->tt_utoff;
      *isdst = ttisp->tt_isdst;
      return true;
    }
  }

  return false;                                             
}

   
                                                                     
                                                    
   
bool
pg_get_timezone_offset(const pg_tz *tz, long int *gmtoff)
{
     
                                                                         
                                                                          
                      
     
  const struct state *sp;
  int i;

  sp = &tz->state;
  for (i = 1; i < sp->typecnt; i++)
  {
    if (sp->ttis[i].tt_utoff != sp->ttis[0].tt_utoff)
    {
      return false;
    }
  }
  *gmtoff = sp->ttis[0].tt_utoff;
  return true;
}

   
                                           
   
const char *
pg_get_timezone_name(pg_tz *tz)
{
  if (tz)
  {
    return tz->TZname;
  }
  return NULL;
}

   
                                         
   
                                                                         
                                                                           
                         
   
bool
pg_tz_acceptable(pg_tz *tz)
{
  struct pg_tm *tt;
  pg_time_t time2000;

     
                                                                            
                                                                          
                                                 
     
  time2000 = (POSTGRES_EPOCH_JDATE - UNIX_EPOCH_JDATE) * SECS_PER_DAY;
  tt = pg_localtime(&time2000, tz);
  if (!tt || tt->tm_sec != 0)
  {
    return false;
  }

  return true;
}
