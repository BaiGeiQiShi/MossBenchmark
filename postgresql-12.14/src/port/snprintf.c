   
                                                 
                            
                                                                      
                                                                         
   
                                                                      
                                                                      
            
                                                                     
                                                                   
                                                                        
                                                                         
                                                                          
                                                                           
                                                                           
                                                
   
                                                                           
                                                                         
                                                                              
                                                                            
                                                                              
                                                                           
                                                                         
                                                                              
                                                                             
                                                                          
                
   
                       
   

#include "c.h"

#include <math.h>

   
                                                                        
                                                                             
                                                                           
                                                                   
   
#define PG_NL_ARGMAX 31

   
                                   
   
                                                                 
                                                                  
                                                                        
   
                                                                     
                                          
   
                                                    
   
                                                              
   
                                                               
   
                                               
   
                                                     
   
                                                                            
   
                                                                       
                                                                      
                                     
   
   
                                                                               
                                                     
   
                                                                         
                                                                          
                                                           
   
                                                                           
                              
   
                                                                           
                                                                         
                                                                        
                                                                           
                           
   
                                                                           
                                                
   

                                                                
             
                                               
                                                  
                                                                      
                                                                           
                                                                

                       
#undef vsnprintf
#undef snprintf
#undef vsprintf
#undef sprintf
#undef vfprintf
#undef fprintf
#undef vprintf
#undef printf

   
                                                                             
                                                                         
                                                                            
                                                           
   
#if defined(_MSC_VER) && _MSC_VER < 1900                 
#define snprintf(str, size, ...) sprintf(str, __VA_ARGS__)
#endif

   
                                                   
   
                                                                    
                                                                     
   
                                                                          
                                                                        
                                                                          
                                                                               
                                                                             
                                   
   
typedef struct
{
  char *bufptr;                                    
  char *bufstart;                           
  char *bufend;                                       
                                                                        
  FILE *stream;                                           
  int nchars;                                           
  bool failed;                                       
} PrintfTarget;

   
                                                                          
                                                                         
                                                                           
                                                            
   
typedef enum
{
  ATYPE_NONE = 0,
  ATYPE_INT,
  ATYPE_LONG,
  ATYPE_LONGLONG,
  ATYPE_DOUBLE,
  ATYPE_CHARPTR
} PrintfArgType;

typedef union
{
  int i;
  long l;
  long long ll;
  double d;
  char *cptr;
} PrintfArgValue;

static void
flushbuffer(PrintfTarget *target);
static void
dopr(PrintfTarget *target, const char *format, va_list args);

   
                                    
   
                                                                           
                                                                
   

int
pg_vsnprintf(char *str, size_t count, const char *fmt, va_list args)
{
  PrintfTarget target;
  char onebyte[1];

     
                                                                   
                                                                          
                                                                           
                      
     
  if (count == 0)
  {
    str = onebyte;
    count = 1;
  }
  target.bufstart = target.bufptr = str;
  target.bufend = str + count - 1;
  target.stream = NULL;
  target.nchars = 0;
  target.failed = false;
  dopr(&target, fmt, args);
  *(target.bufptr) = '\0';
  return target.failed ? -1 : (target.bufptr - target.bufstart + target.nchars);
}

int
pg_snprintf(char *str, size_t count, const char *fmt, ...)
{
  int len;
  va_list args;

  va_start(args, fmt);
  len = pg_vsnprintf(str, count, fmt, args);
  va_end(args);
  return len;
}

int
pg_vsprintf(char *str, const char *fmt, va_list args)
{
  PrintfTarget target;

  target.bufstart = target.bufptr = str;
  target.bufend = NULL;
  target.stream = NULL;
  target.nchars = 0;                                   
  target.failed = false;
  dopr(&target, fmt, args);
  *(target.bufptr) = '\0';
  return target.failed ? -1 : (target.bufptr - target.bufstart + target.nchars);
}

int
pg_sprintf(char *str, const char *fmt, ...)
{
  int len;
  va_list args;

  va_start(args, fmt);
  len = pg_vsprintf(str, fmt, args);
  va_end(args);
  return len;
}

int
pg_vfprintf(FILE *stream, const char *fmt, va_list args)
{
  PrintfTarget target;
  char buffer[1024];                        

  if (stream == NULL)
  {
    errno = EINVAL;
    return -1;
  }
  target.bufstart = target.bufptr = buffer;
  target.bufend = buffer + sizeof(buffer);                           
  target.stream = stream;
  target.nchars = 0;
  target.failed = false;
  dopr(&target, fmt, args);
                                          
  flushbuffer(&target);
  return target.failed ? -1 : target.nchars;
}

int
pg_fprintf(FILE *stream, const char *fmt, ...)
{
  int len;
  va_list args;

  va_start(args, fmt);
  len = pg_vfprintf(stream, fmt, args);
  va_end(args);
  return len;
}

int
pg_vprintf(const char *fmt, va_list args)
{
  return pg_vfprintf(stdout, fmt, args);
}

int
pg_printf(const char *fmt, ...)
{
  int len;
  va_list args;

  va_start(args, fmt);
  len = pg_vfprintf(stdout, fmt, args);
  va_end(args);
  return len;
}

   
                                                                            
                                                                       
   
static void
flushbuffer(PrintfTarget *target)
{
  size_t nc = target->bufptr - target->bufstart;

     
                                                                     
                                            
     
  if (!target->failed && nc > 0)
  {
    size_t written;

    written = fwrite(target->bufstart, 1, nc, target->stream);
    target->nchars += written;
    if (written != nc)
    {
      target->failed = true;
    }
  }
  target->bufptr = target->bufstart;
}

static bool
find_arguments(const char *format, va_list args, PrintfArgValue *argvalues);
static void
fmtstr(const char *value, int leftjust, int minlen, int maxwidth, int pointflag, PrintfTarget *target);
static void
fmtptr(const void *value, PrintfTarget *target);
static void
fmtint(long long value, char type, int forcesign, int leftjust, int minlen, int zpad, int precision, int pointflag, PrintfTarget *target);
static void
fmtchar(int value, int leftjust, int minlen, PrintfTarget *target);
static void
fmtfloat(double value, char type, int forcesign, int leftjust, int minlen, int zpad, int precision, int pointflag, PrintfTarget *target);
static void
dostr(const char *str, int slen, PrintfTarget *target);
static void
dopr_outch(int c, PrintfTarget *target);
static void
dopr_outchmulti(int c, int slen, PrintfTarget *target);
static int
adjust_sign(int is_negative, int forcesign, int *signvalue);
static int
compute_padlen(int minlen, int vallen, int leftjust);
static void
leading_pad(int zpad, int signvalue, int *padlen, PrintfTarget *target);
static void
trailing_pad(int padlen, PrintfTarget *target);

   
                                                                           
                                                                        
   
                                                                           
                                                                  
   
#ifndef HAVE_STRCHRNUL

static inline const char *
strchrnul(const char *s, int c)
{
  while (*s != '\0' && *s != c)
  {
    s++;
  }
  return s;
}

#else

   
                                                                         
                                                                      
                                                                             
                                                                       
   
#ifndef _GNU_SOURCE
extern char *
strchrnul(const char *s, int c);
#endif

#endif                     

   
                                              
   
static void
dopr(PrintfTarget *target, const char *format, va_list args)
{
  int save_errno = errno;
  const char *first_pct = NULL;
  int ch;
  bool have_dollar;
  bool have_star;
  bool afterstar;
  int accum;
  int longlongflag;
  int longflag;
  int pointflag;
  int leftjust;
  int fieldwidth;
  int precision;
  int zpad;
  int forcesign;
  int fmtpos;
  int cvalue;
  long long numvalue;
  double fvalue;
  const char *strvalue;
  PrintfArgValue argvalues[PG_NL_ARGMAX + 1];

     
                                                                          
                                                                 
                                                                      
                                                                    
     
  have_dollar = false;

  while (*format != '\0')
  {
                                          
    if (*format != '%')
    {
                                             
      const char *next_pct = strchrnul(format + 1, '%');

                                                  
      dostr(format, next_pct - format, target);
      if (target->failed)
      {
        break;
      }

      if (*next_pct == '\0')
      {
        break;
      }
      format = next_pct;
    }

       
                                                                          
                                                                         
                             
       
    if (first_pct == NULL)
    {
      first_pct = format;
    }

                                                     
    format++;

                                                          
    if (*format == 's')
    {
      format++;
      strvalue = va_arg(args, char *);
      if (strvalue == NULL)
      {
        strvalue = "(null)";
      }
      dostr(strvalue, strlen(strvalue), target);
      if (target->failed)
      {
        break;
      }
      continue;
    }

    fieldwidth = precision = zpad = leftjust = forcesign = 0;
    longflag = longlongflag = pointflag = 0;
    fmtpos = accum = 0;
    have_star = afterstar = false;
  nextch2:
    ch = *format++;
    switch (ch)
    {
    case '-':
      leftjust = 1;
      goto nextch2;
    case '+':
      forcesign = 1;
      goto nextch2;
    case '0':
                                                     
      if (accum == 0 && !pointflag)
      {
        zpad = '0';
      }
                     
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      accum = accum * 10 + (ch - '0');
      goto nextch2;
    case '.':
      if (have_star)
      {
        have_star = false;
      }
      else
      {
        fieldwidth = accum;
      }
      pointflag = 1;
      accum = 0;
      goto nextch2;
    case '*':
      if (have_dollar)
      {
           
                                                                  
                                                                   
                                                                   
           
        afterstar = true;
      }
      else
      {
                                         
        int starval = va_arg(args, int);

        if (pointflag)
        {
          precision = starval;
          if (precision < 0)
          {
            precision = 0;
            pointflag = 0;
          }
        }
        else
        {
          fieldwidth = starval;
          if (fieldwidth < 0)
          {
            leftjust = 1;
            fieldwidth = -fieldwidth;
          }
        }
      }
      have_star = true;
      accum = 0;
      goto nextch2;
    case '$':
                              
      if (!have_dollar)
      {
                                                            
        if (!find_arguments(first_pct, args, argvalues))
        {
          goto bad_format;
        }
        have_dollar = true;
      }
      if (afterstar)
      {
                                          
        int starval = argvalues[accum].i;

        if (pointflag)
        {
          precision = starval;
          if (precision < 0)
          {
            precision = 0;
            pointflag = 0;
          }
        }
        else
        {
          fieldwidth = starval;
          if (fieldwidth < 0)
          {
            leftjust = 1;
            fieldwidth = -fieldwidth;
          }
        }
        afterstar = false;
      }
      else
      {
        fmtpos = accum;
      }
      accum = 0;
      goto nextch2;
    case 'l':
      if (longflag)
      {
        longlongflag = 1;
      }
      else
      {
        longflag = 1;
      }
      goto nextch2;
    case 'z':
#if SIZEOF_SIZE_T == 8
#ifdef HAVE_LONG_INT_64
      longflag = 1;
#elif defined(HAVE_LONG_LONG_INT_64)
      longlongflag = 1;
#else
#error "Don't know how to print 64bit integers"
#endif
#else
                                             
#endif
      goto nextch2;
    case 'h':
    case '\'':
                        
      goto nextch2;
    case 'd':
    case 'i':
      if (!have_star)
      {
        if (pointflag)
        {
          precision = accum;
        }
        else
        {
          fieldwidth = accum;
        }
      }
      if (have_dollar)
      {
        if (longlongflag)
        {
          numvalue = argvalues[fmtpos].ll;
        }
        else if (longflag)
        {
          numvalue = argvalues[fmtpos].l;
        }
        else
        {
          numvalue = argvalues[fmtpos].i;
        }
      }
      else
      {
        if (longlongflag)
        {
          numvalue = va_arg(args, long long);
        }
        else if (longflag)
        {
          numvalue = va_arg(args, long);
        }
        else
        {
          numvalue = va_arg(args, int);
        }
      }
      fmtint(numvalue, ch, forcesign, leftjust, fieldwidth, zpad, precision, pointflag, target);
      break;
    case 'o':
    case 'u':
    case 'x':
    case 'X':
      if (!have_star)
      {
        if (pointflag)
        {
          precision = accum;
        }
        else
        {
          fieldwidth = accum;
        }
      }
      if (have_dollar)
      {
        if (longlongflag)
        {
          numvalue = (unsigned long long)argvalues[fmtpos].ll;
        }
        else if (longflag)
        {
          numvalue = (unsigned long)argvalues[fmtpos].l;
        }
        else
        {
          numvalue = (unsigned int)argvalues[fmtpos].i;
        }
      }
      else
      {
        if (longlongflag)
        {
          numvalue = (unsigned long long)va_arg(args, long long);
        }
        else if (longflag)
        {
          numvalue = (unsigned long)va_arg(args, long);
        }
        else
        {
          numvalue = (unsigned int)va_arg(args, int);
        }
      }
      fmtint(numvalue, ch, forcesign, leftjust, fieldwidth, zpad, precision, pointflag, target);
      break;
    case 'c':
      if (!have_star)
      {
        if (pointflag)
        {
          precision = accum;
        }
        else
        {
          fieldwidth = accum;
        }
      }
      if (have_dollar)
      {
        cvalue = (unsigned char)argvalues[fmtpos].i;
      }
      else
      {
        cvalue = (unsigned char)va_arg(args, int);
      }
      fmtchar(cvalue, leftjust, fieldwidth, target);
      break;
    case 's':
      if (!have_star)
      {
        if (pointflag)
        {
          precision = accum;
        }
        else
        {
          fieldwidth = accum;
        }
      }
      if (have_dollar)
      {
        strvalue = argvalues[fmtpos].cptr;
      }
      else
      {
        strvalue = va_arg(args, char *);
      }
                                                           
      if (strvalue == NULL)
      {
        strvalue = "(null)";
      }
      fmtstr(strvalue, leftjust, fieldwidth, precision, pointflag, target);
      break;
    case 'p':
                                               
      if (have_dollar)
      {
        strvalue = argvalues[fmtpos].cptr;
      }
      else
      {
        strvalue = va_arg(args, char *);
      }
      fmtptr((const void *)strvalue, target);
      break;
    case 'e':
    case 'E':
    case 'f':
    case 'g':
    case 'G':
      if (!have_star)
      {
        if (pointflag)
        {
          precision = accum;
        }
        else
        {
          fieldwidth = accum;
        }
      }
      if (have_dollar)
      {
        fvalue = argvalues[fmtpos].d;
      }
      else
      {
        fvalue = va_arg(args, double);
      }
      fmtfloat(fvalue, ch, forcesign, leftjust, fieldwidth, zpad, precision, pointflag, target);
      break;
    case 'm':
    {
      char errbuf[PG_STRERROR_R_BUFLEN];
      const char *errm = strerror_r(save_errno, errbuf, sizeof(errbuf));

      dostr(errm, strlen(errm), target);
    }
    break;
    case '%':
      dopr_outch('%', target);
      break;
    default:

         
                                                                 
                                     
         
      goto bad_format;
    }

                                                      
    if (target->failed)
    {
      break;
    }
  }

  return;

bad_format:
  errno = EINVAL;
  target->failed = true;
}

   
                                                                       
   
                                                                        
                                                                    
   
static bool
find_arguments(const char *format, va_list args, PrintfArgValue *argvalues)
{
  int ch;
  bool afterstar;
  int accum;
  int longlongflag;
  int longflag;
  int fmtpos;
  int i;
  int last_dollar;
  PrintfArgType argtypes[PG_NL_ARGMAX + 1];

                                                 
  last_dollar = 0;
  MemSet(argtypes, 0, sizeof(argtypes));

     
                                                                         
                                                                         
     
                                                                             
                                                                           
                                                                            
     
  while (*format != '\0')
  {
                                          
    if (*format != '%')
    {
                                                                       
      format = strchr(format + 1, '%');
      if (format == NULL)
      {
        break;
      }
    }

                                                     
    format++;
    longflag = longlongflag = 0;
    fmtpos = accum = 0;
    afterstar = false;
  nextch1:
    ch = *format++;
    switch (ch)
    {
    case '-':
    case '+':
      goto nextch1;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      accum = accum * 10 + (ch - '0');
      goto nextch1;
    case '.':
      accum = 0;
      goto nextch1;
    case '*':
      if (afterstar)
      {
        return false;                                   
      }
      afterstar = true;
      accum = 0;
      goto nextch1;
    case '$':
      if (accum <= 0 || accum > PG_NL_ARGMAX)
      {
        return false;
      }
      if (afterstar)
      {
        if (argtypes[accum] && argtypes[accum] != ATYPE_INT)
        {
          return false;
        }
        argtypes[accum] = ATYPE_INT;
        last_dollar = Max(last_dollar, accum);
        afterstar = false;
      }
      else
      {
        fmtpos = accum;
      }
      accum = 0;
      goto nextch1;
    case 'l':
      if (longflag)
      {
        longlongflag = 1;
      }
      else
      {
        longflag = 1;
      }
      goto nextch1;
    case 'z':
#if SIZEOF_SIZE_T == 8
#ifdef HAVE_LONG_INT_64
      longflag = 1;
#elif defined(HAVE_LONG_LONG_INT_64)
      longlongflag = 1;
#else
#error "Don't know how to print 64bit integers"
#endif
#else
                                             
#endif
      goto nextch1;
    case 'h':
    case '\'':
                        
      goto nextch1;
    case 'd':
    case 'i':
    case 'o':
    case 'u':
    case 'x':
    case 'X':
      if (fmtpos)
      {
        PrintfArgType atype;

        if (longlongflag)
        {
          atype = ATYPE_LONGLONG;
        }
        else if (longflag)
        {
          atype = ATYPE_LONG;
        }
        else
        {
          atype = ATYPE_INT;
        }
        if (argtypes[fmtpos] && argtypes[fmtpos] != atype)
        {
          return false;
        }
        argtypes[fmtpos] = atype;
        last_dollar = Max(last_dollar, fmtpos);
      }
      else
      {
        return false;                                 
      }
      break;
    case 'c':
      if (fmtpos)
      {
        if (argtypes[fmtpos] && argtypes[fmtpos] != ATYPE_INT)
        {
          return false;
        }
        argtypes[fmtpos] = ATYPE_INT;
        last_dollar = Max(last_dollar, fmtpos);
      }
      else
      {
        return false;                                 
      }
      break;
    case 's':
    case 'p':
      if (fmtpos)
      {
        if (argtypes[fmtpos] && argtypes[fmtpos] != ATYPE_CHARPTR)
        {
          return false;
        }
        argtypes[fmtpos] = ATYPE_CHARPTR;
        last_dollar = Max(last_dollar, fmtpos);
      }
      else
      {
        return false;                                 
      }
      break;
    case 'e':
    case 'E':
    case 'f':
    case 'g':
    case 'G':
      if (fmtpos)
      {
        if (argtypes[fmtpos] && argtypes[fmtpos] != ATYPE_DOUBLE)
        {
          return false;
        }
        argtypes[fmtpos] = ATYPE_DOUBLE;
        last_dollar = Max(last_dollar, fmtpos);
      }
      else
      {
        return false;                                 
      }
      break;
    case 'm':
    case '%':
      break;
    default:
      return false;                          
    }

       
                                                                 
                                 
       
    if (afterstar)
    {
      return false;                                 
    }
  }

     
                                                                       
                                                                     
                                                                         
     
  for (i = 1; i <= last_dollar; i++)
  {
    switch (argtypes[i])
    {
    case ATYPE_NONE:
      return false;
    case ATYPE_INT:
      argvalues[i].i = va_arg(args, int);
      break;
    case ATYPE_LONG:
      argvalues[i].l = va_arg(args, long);
      break;
    case ATYPE_LONGLONG:
      argvalues[i].ll = va_arg(args, long long);
      break;
    case ATYPE_DOUBLE:
      argvalues[i].d = va_arg(args, double);
      break;
    case ATYPE_CHARPTR:
      argvalues[i].cptr = va_arg(args, char *);
      break;
    }
  }

  return true;
}

static void
fmtstr(const char *value, int leftjust, int minlen, int maxwidth, int pointflag, PrintfTarget *target)
{
  int padlen, vallen;                    

     
                                                                          
                
     
  if (pointflag)
  {
    vallen = strnlen(value, maxwidth);
  }
  else
  {
    vallen = strlen(value);
  }

  padlen = compute_padlen(minlen, vallen, leftjust);

  if (padlen > 0)
  {
    dopr_outchmulti(' ', padlen, target);
    padlen = 0;
  }

  dostr(value, vallen, target);

  trailing_pad(padlen, target);
}

static void
fmtptr(const void *value, PrintfTarget *target)
{
  int vallen;
  char convert[64];

                                                                          
  vallen = snprintf(convert, sizeof(convert), "%p", value);
  if (vallen < 0)
  {
    target->failed = true;
  }
  else
  {
    dostr(convert, vallen, target);
  }
}

static void
fmtint(long long value, char type, int forcesign, int leftjust, int minlen, int zpad, int precision, int pointflag, PrintfTarget *target)
{
  unsigned long long base;
  unsigned long long uvalue;
  int dosign;
  const char *cvt = "0123456789abcdef";
  int signvalue = 0;
  char convert[64];
  int vallen = 0;
  int padlen;                     
  int zeropad;                           

  switch (type)
  {
  case 'd':
  case 'i':
    base = 10;
    dosign = 1;
    break;
  case 'o':
    base = 8;
    dosign = 0;
    break;
  case 'u':
    base = 10;
    dosign = 0;
    break;
  case 'x':
    base = 16;
    dosign = 0;
    break;
  case 'X':
    cvt = "0123456789ABCDEF";
    base = 16;
    dosign = 0;
    break;
  default:
    return;                          
  }

                                                                            
#if _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4146)
#endif
                  
  if (dosign && adjust_sign((value < 0), forcesign, &signvalue))
  {
    uvalue = -(unsigned long long)value;
  }
  else
  {
    uvalue = (unsigned long long)value;
  }
#if _MSC_VER
#pragma warning(pop)
#endif

     
                                                                           
                
     
  if (value == 0 && pointflag && precision == 0)
  {
    vallen = 0;
  }
  else
  {
                             
    do
    {
      convert[sizeof(convert) - (++vallen)] = cvt[uvalue % base];
      uvalue = uvalue / base;
    } while (uvalue);
  }

  zeropad = Max(0, precision - vallen);

  padlen = compute_padlen(minlen, vallen + zeropad, leftjust);

  leading_pad(zpad, signvalue, &padlen, target);

  if (zeropad > 0)
  {
    dopr_outchmulti('0', zeropad, target);
  }

  dostr(convert + sizeof(convert) - vallen, vallen, target);

  trailing_pad(padlen, target);
}

static void
fmtchar(int value, int leftjust, int minlen, PrintfTarget *target)
{
  int padlen;                    

  padlen = compute_padlen(minlen, 1, leftjust);

  if (padlen > 0)
  {
    dopr_outchmulti(' ', padlen, target);
    padlen = 0;
  }

  dopr_outch(value, target);

  trailing_pad(padlen, target);
}

static void
fmtfloat(double value, char type, int forcesign, int leftjust, int minlen, int zpad, int precision, int pointflag, PrintfTarget *target)
{
  int signvalue = 0;
  int prec;
  int vallen;
  char fmt[8];
  char convert[1024];
  int zeropadlen = 0;                                
  int padlen;                                        

     
                                                                             
                                              
     
                                                                           
                                                                             
                                                                        
                                                                           
                                                                          
                                                                           
                                                                            
                                                                        
                                                            
     
                                                                            
             
     
  if (precision < 0)                                         
  {
    precision = 0;
  }
  prec = Min(precision, 350);

  if (isnan(value))
  {
    strcpy(convert, "NaN");
    vallen = 3;
                                                       
  }
  else
  {
       
                                                                       
                                                                          
                                                                  
                                                    
       
    static const double dzero = 0.0;

    if (adjust_sign((value < 0.0 || (value == 0.0 && memcmp(&value, &dzero, sizeof(double)) != 0)), forcesign, &signvalue))
    {
      value = -value;
    }

    if (isinf(value))
    {
      strcpy(convert, "Infinity");
      vallen = 8;
                                                         
    }
    else if (pointflag)
    {
      zeropadlen = precision - prec;
      fmt[0] = '%';
      fmt[1] = '.';
      fmt[2] = '*';
      fmt[3] = type;
      fmt[4] = '\0';
      vallen = snprintf(convert, sizeof(convert), fmt, prec, value);
    }
    else
    {
      fmt[0] = '%';
      fmt[1] = type;
      fmt[2] = '\0';
      vallen = snprintf(convert, sizeof(convert), fmt, value);
    }
    if (vallen < 0)
    {
      goto fail;
    }

       
                                                                   
                                                                        
                                                                
       
#ifdef WIN32
    if (vallen >= 6 && convert[vallen - 5] == 'e' && convert[vallen - 3] == '0')
    {
      convert[vallen - 3] = convert[vallen - 2];
      convert[vallen - 2] = convert[vallen - 1];
      vallen--;
    }
#endif
  }

  padlen = compute_padlen(minlen, vallen + zeropadlen, leftjust);

  leading_pad(zpad, signvalue, &padlen, target);

  if (zeropadlen > 0)
  {
                                                                 
    char *epos = strrchr(convert, 'e');

    if (!epos)
    {
      epos = strrchr(convert, 'E');
    }
    if (epos)
    {
                               
      dostr(convert, epos - convert, target);
      if (zeropadlen > 0)
      {
        dopr_outchmulti('0', zeropadlen, target);
      }
      dostr(epos, vallen - (epos - convert), target);
    }
    else
    {
                                             
      dostr(convert, vallen, target);
      if (zeropadlen > 0)
      {
        dopr_outchmulti('0', zeropadlen, target);
      }
    }
  }
  else
  {
                                                     
    dostr(convert, vallen, target);
  }

  trailing_pad(padlen, target);
  return;

fail:
  target->failed = true;
}

   
                                                                
   
                                                                       
                                                                       
                                                                
                                                                     
                                                      
   
int
pg_strfromd(char *str, size_t count, int precision, double value)
{
  PrintfTarget target;
  int signvalue = 0;
  int vallen;
  char fmt[8];
  char convert[64];

                                                                       
  Assert(count > 0);
  target.bufstart = target.bufptr = str;
  target.bufend = str + count - 1;
  target.stream = NULL;
  target.nchars = 0;
  target.failed = false;

     
                                                                           
                                                                          
                                              
     
  if (precision < 1)
  {
    precision = 1;
  }
  else if (precision > 32)
  {
    precision = 32;
  }

     
                                                                        
                                                               
     
  if (isnan(value))
  {
    strcpy(convert, "NaN");
    vallen = 3;
  }
  else
  {
    static const double dzero = 0.0;

    if (value < 0.0 || (value == 0.0 && memcmp(&value, &dzero, sizeof(double)) != 0))
    {
      signvalue = '-';
      value = -value;
    }

    if (isinf(value))
    {
      strcpy(convert, "Infinity");
      vallen = 8;
    }
    else
    {
      fmt[0] = '%';
      fmt[1] = '.';
      fmt[2] = '*';
      fmt[3] = 'g';
      fmt[4] = '\0';
      vallen = snprintf(convert, sizeof(convert), fmt, precision, value);
      if (vallen < 0)
      {
        target.failed = true;
        goto fail;
      }

#ifdef WIN32
      if (vallen >= 6 && convert[vallen - 5] == 'e' && convert[vallen - 3] == '0')
      {
        convert[vallen - 3] = convert[vallen - 2];
        convert[vallen - 2] = convert[vallen - 1];
        vallen--;
      }
#endif
    }
  }

  if (signvalue)
  {
    dopr_outch(signvalue, &target);
  }

  dostr(convert, vallen, &target);

fail:
  *(target.bufptr) = '\0';
  return target.failed ? -1 : (target.bufptr - target.bufstart + target.nchars);
}

static void
dostr(const char *str, int slen, PrintfTarget *target)
{
                                              
  if (slen == 1)
  {
    dopr_outch(*str, target);
    return;
  }

  while (slen > 0)
  {
    int avail;

    if (target->bufend != NULL)
    {
      avail = target->bufend - target->bufptr;
    }
    else
    {
      avail = slen;
    }
    if (avail <= 0)
    {
                                               
      if (target->stream == NULL)
      {
        target->nchars += slen;                        
        return;
      }
      flushbuffer(target);
      continue;
    }
    avail = Min(avail, slen);
    memmove(target->bufptr, str, avail);
    target->bufptr += avail;
    str += avail;
    slen -= avail;
  }
}

static void
dopr_outch(int c, PrintfTarget *target)
{
  if (target->bufend != NULL && target->bufptr >= target->bufend)
  {
                                             
    if (target->stream == NULL)
    {
      target->nchars++;                        
      return;
    }
    flushbuffer(target);
  }
  *(target->bufptr++) = c;
}

static void
dopr_outchmulti(int c, int slen, PrintfTarget *target)
{
                                              
  if (slen == 1)
  {
    dopr_outch(c, target);
    return;
  }

  while (slen > 0)
  {
    int avail;

    if (target->bufend != NULL)
    {
      avail = target->bufend - target->bufptr;
    }
    else
    {
      avail = slen;
    }
    if (avail <= 0)
    {
                                               
      if (target->stream == NULL)
      {
        target->nchars += slen;                        
        return;
      }
      flushbuffer(target);
      continue;
    }
    avail = Min(avail, slen);
    memset(target->bufptr, c, avail);
    target->bufptr += avail;
    slen -= avail;
  }
}

static int
adjust_sign(int is_negative, int forcesign, int *signvalue)
{
  if (is_negative)
  {
    *signvalue = '-';
    return true;
  }
  else if (forcesign)
  {
    *signvalue = '+';
  }
  return false;
}

static int
compute_padlen(int minlen, int vallen, int leftjust)
{
  int padlen;

  padlen = minlen - vallen;
  if (padlen < 0)
  {
    padlen = 0;
  }
  if (leftjust)
  {
    padlen = -padlen;
  }
  return padlen;
}

static void
leading_pad(int zpad, int signvalue, int *padlen, PrintfTarget *target)
{
  int maxpad;

  if (*padlen > 0 && zpad)
  {
    if (signvalue)
    {
      dopr_outch(signvalue, target);
      --(*padlen);
      signvalue = 0;
    }
    if (*padlen > 0)
    {
      dopr_outchmulti(zpad, *padlen, target);
      *padlen = 0;
    }
  }
  maxpad = (signvalue != 0);
  if (*padlen > maxpad)
  {
    dopr_outchmulti(' ', *padlen - maxpad, target);
    *padlen = maxpad;
  }
  if (signvalue)
  {
    dopr_outch(signvalue, target);
    if (*padlen > 0)
    {
      --(*padlen);
    }
    else if (*padlen < 0)
    {
      ++(*padlen);
    }
  }
}

static void
trailing_pad(int padlen, PrintfTarget *target)
{
  if (padlen < 0)
  {
    dopr_outchmulti(' ', -padlen, target);
  }
}
