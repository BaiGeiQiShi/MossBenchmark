                                               

#include "postgres_fe.h"

#include <time.h>
#include <ctype.h>
#include <limits.h>

#include "pgtypeslib_extern.h"
#include "dt.h"
#include "pgtypes_error.h"
#include "pgtypes_date.h"

date *
PGTYPESdate_new(void)
{
  date *result;

  result = (date *)pgtypes_alloc(sizeof(date));
                                                  
  return result;
}

void
PGTYPESdate_free(date *d)
{
  free(d);
}

date
PGTYPESdate_from_timestamp(timestamp dt)
{
  date dDate;

  dDate = 0;                                

  if (!TIMESTAMP_NOT_FINITE(dt))
  {
                              
    dDate = (dt / USECS_PER_DAY);
  }

  return dDate;
}

date
PGTYPESdate_from_asc(char *str, char **endptr)
{
  date dDate;
  fsec_t fsec;
  struct tm tt, *tm = &tt;
  int dtype;
  int nf;
  char *field[MAXDATEFIELDS];
  int ftype[MAXDATEFIELDS];
  char lowstr[MAXDATELEN + MAXDATEFIELDS];
  char *realptr;
  char **ptr = (endptr != NULL) ? endptr : &realptr;

  bool EuroDates = false;

  errno = 0;
  if (strlen(str) > MAXDATELEN)
  {
    errno = PGTYPES_DATE_BAD_DATE;
    return INT_MIN;
  }

  if (ParseDateTime(str, lowstr, field, ftype, &nf, ptr) != 0 || DecodeDateTime(field, ftype, nf, &dtype, tm, &fsec, EuroDates) != 0)
  {
    errno = PGTYPES_DATE_BAD_DATE;
    return INT_MIN;
  }

  switch (dtype)
  {
  case DTK_DATE:
    break;

  case DTK_EPOCH:
    if (GetEpochTime(tm) < 0)
    {
      errno = PGTYPES_DATE_BAD_DATE;
      return INT_MIN;
    }
    break;

  default:
    errno = PGTYPES_DATE_BAD_DATE;
    return INT_MIN;
  }

  dDate = (date2j(tm->tm_year, tm->tm_mon, tm->tm_mday) - date2j(2000, 1, 1));

  return dDate;
}

char *
PGTYPESdate_to_asc(date dDate)
{
  struct tm tt, *tm = &tt;
  char buf[MAXDATELEN + 1];
  int DateStyle = 1;
  bool EuroDates = false;

  j2date(dDate + date2j(2000, 1, 1), &(tm->tm_year), &(tm->tm_mon), &(tm->tm_mday));
  EncodeDateOnly(tm, DateStyle, buf, EuroDates);
  return pgtypes_strdup(buf);
}

void
PGTYPESdate_julmdy(date jd, int *mdy)
{
  int y, m, d;

  j2date((int)(jd + date2j(2000, 1, 1)), &y, &m, &d);
  mdy[0] = m;
  mdy[1] = d;
  mdy[2] = y;
}

void
PGTYPESdate_mdyjul(int *mdy, date *jdate)
{
                       
                      
                       

  *jdate = (date)(date2j(mdy[2], mdy[0], mdy[1]) - date2j(2000, 1, 1));
}

int
PGTYPESdate_dayofweek(date dDate)
{
     
                                                                   
                               
     
  return (int)(dDate + date2j(2000, 1, 1) + 1) % 7;
}

void
PGTYPESdate_today(date *d)
{
  struct tm ts;

  GetCurrentDateTime(&ts);
  if (errno == 0)
  {
    *d = date2j(ts.tm_year, ts.tm_mon, ts.tm_mday) - date2j(2000, 1, 1);
  }
  return;
}

#define PGTYPES_DATE_NUM_MAX_DIGITS                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
                   

#define PGTYPES_FMTDATE_DAY_DIGITS_LZ 1                                
#define PGTYPES_FMTDATE_DOW_LITERAL_SHORT 2
#define PGTYPES_FMTDATE_MONTH_DIGITS_LZ 3
#define PGTYPES_FMTDATE_MONTH_LITERAL_SHORT 4
#define PGTYPES_FMTDATE_YEAR_DIGITS_SHORT 5
#define PGTYPES_FMTDATE_YEAR_DIGITS_LONG 6

int
PGTYPESdate_fmt_asc(date dDate, const char *fmtstring, char *outbuf)
{
  static struct
  {
    char *format;
    int component;
  } mapping[] = {   
                                                                                        
                                                                          
                    
      {"ddd", PGTYPES_FMTDATE_DOW_LITERAL_SHORT}, {"dd", PGTYPES_FMTDATE_DAY_DIGITS_LZ}, {"mmm", PGTYPES_FMTDATE_MONTH_LITERAL_SHORT}, {"mm", PGTYPES_FMTDATE_MONTH_DIGITS_LZ}, {"yyyy", PGTYPES_FMTDATE_YEAR_DIGITS_LONG}, {"yy", PGTYPES_FMTDATE_YEAR_DIGITS_SHORT}, {NULL, 0}};

  union un_fmt_comb replace_val;
  int replace_type;

  int i;
  int dow;
  char *start_pattern;
  struct tm tm;

                            
  strcpy(outbuf, fmtstring);

                    
  j2date(dDate + date2j(2000, 1, 1), &(tm.tm_year), &(tm.tm_mon), &(tm.tm_mday));
  dow = PGTYPESdate_dayofweek(dDate);

  for (i = 0; mapping[i].format != NULL; i++)
  {
    while ((start_pattern = strstr(outbuf, mapping[i].format)) != NULL)
    {
      switch (mapping[i].component)
      {
      case PGTYPES_FMTDATE_DOW_LITERAL_SHORT:
        replace_val.str_val = pgtypes_date_weekdays_short[dow];
        replace_type = PGTYPES_TYPE_STRING_CONSTANT;
        break;
      case PGTYPES_FMTDATE_DAY_DIGITS_LZ:
        replace_val.uint_val = tm.tm_mday;
        replace_type = PGTYPES_TYPE_UINT_2_LZ;
        break;
      case PGTYPES_FMTDATE_MONTH_LITERAL_SHORT:
        replace_val.str_val = months[tm.tm_mon - 1];
        replace_type = PGTYPES_TYPE_STRING_CONSTANT;
        break;
      case PGTYPES_FMTDATE_MONTH_DIGITS_LZ:
        replace_val.uint_val = tm.tm_mon;
        replace_type = PGTYPES_TYPE_UINT_2_LZ;
        break;
      case PGTYPES_FMTDATE_YEAR_DIGITS_LONG:
        replace_val.uint_val = tm.tm_year;
        replace_type = PGTYPES_TYPE_UINT_4_LZ;
        break;
      case PGTYPES_FMTDATE_YEAR_DIGITS_SHORT:
        replace_val.uint_val = tm.tm_year % 100;
        replace_type = PGTYPES_TYPE_UINT_2_LZ;
        break;
      default:

           
                                                   
           
        replace_val.str_val = " ";
        replace_type = PGTYPES_TYPE_STRING_CONSTANT;
      }
      switch (replace_type)
      {
      case PGTYPES_TYPE_STRING_MALLOCED:
      case PGTYPES_TYPE_STRING_CONSTANT:
        memcpy(start_pattern, replace_val.str_val, strlen(replace_val.str_val));
        if (replace_type == PGTYPES_TYPE_STRING_MALLOCED)
        {
          free(replace_val.str_val);
        }
        break;
      case PGTYPES_TYPE_UINT:
      {
        char *t = pgtypes_alloc(PGTYPES_DATE_NUM_MAX_DIGITS);

        if (!t)
        {
          return -1;
        }
        snprintf(t, PGTYPES_DATE_NUM_MAX_DIGITS, "%u", replace_val.uint_val);
        memcpy(start_pattern, t, strlen(t));
        free(t);
      }
      break;
      case PGTYPES_TYPE_UINT_2_LZ:
      {
        char *t = pgtypes_alloc(PGTYPES_DATE_NUM_MAX_DIGITS);

        if (!t)
        {
          return -1;
        }
        snprintf(t, PGTYPES_DATE_NUM_MAX_DIGITS, "%02u", replace_val.uint_val);
        memcpy(start_pattern, t, strlen(t));
        free(t);
      }
      break;
      case PGTYPES_TYPE_UINT_4_LZ:
      {
        char *t = pgtypes_alloc(PGTYPES_DATE_NUM_MAX_DIGITS);

        if (!t)
        {
          return -1;
        }
        snprintf(t, PGTYPES_DATE_NUM_MAX_DIGITS, "%04u", replace_val.uint_val);
        memcpy(start_pattern, t, strlen(t));
        free(t);
      }
      break;
      default:

           
                                                  
                                                                   
           
        break;
      }
    }
  }
  return 0;
}

   
                         
   
                              
                                      
                                                                   
                                                                            
                                                                            
                                              
                                                                        
                                                       
   

#define PGTYPES_DATE_MONTH_MAXLENGTH 20                              
int
PGTYPESdate_defmt_asc(date *d, const char *fmt, const char *str)
{
     
                                                                            
                            
     
  int token[3][2];
  int token_values[3] = {-1, -1, -1};
  char *fmt_token_order;
  char *fmt_ystart, *fmt_mstart, *fmt_dstart;
  unsigned int i;
  int reading_digit;
  int token_count;
  char *str_copy;
  struct tm tm;

  tm.tm_year = tm.tm_mon = tm.tm_mday = 0;                          

  if (!d || !str || !fmt)
  {
    errno = PGTYPES_DATE_ERR_EARGS;
    return -1;
  }

                              
  fmt_ystart = strstr(fmt, "yy");
  fmt_mstart = strstr(fmt, "mm");
  fmt_dstart = strstr(fmt, "dd");

  if (!fmt_ystart || !fmt_mstart || !fmt_dstart)
  {
    errno = PGTYPES_DATE_ERR_EARGS;
    return -1;
  }

  if (fmt_ystart < fmt_mstart)
  {
             
    if (fmt_dstart < fmt_ystart)
    {
                 
      fmt_token_order = "dym";
    }
    else if (fmt_dstart > fmt_mstart)
    {
                 
      fmt_token_order = "ymd";
    }
    else
    {
                 
      fmt_token_order = "ydm";
    }
  }
  else
  {
                                 
             
    if (fmt_dstart < fmt_mstart)
    {
                 
      fmt_token_order = "dmy";
    }
    else if (fmt_dstart > fmt_ystart)
    {
                 
      fmt_token_order = "myd";
    }
    else
    {
                 
      fmt_token_order = "mdy";
    }
  }

     
                                                                      
                             
     
                                                                        
              
     
                                                                      
     

                                    
  reading_digit = 1;
  for (i = 0; str[i]; i++)
  {
    if (!isdigit((unsigned char)str[i]))
    {
      reading_digit = 0;
      break;
    }
  }
  if (reading_digit)
  {
    int frag_length[3];
    int target_pos;

    i = strlen(str);
    if (i != 8 && i != 6)
    {
      errno = PGTYPES_DATE_ERR_ENOSHORTDATE;
      return -1;
    }
                                               

       
                                                                           
                                               
       
    str_copy = pgtypes_alloc(strlen(str) + 1 + 2);
    if (!str_copy)
    {
      return -1;
    }

                                           
    if (i == 6)
    {
      frag_length[0] = 2;
      frag_length[1] = 2;
      frag_length[2] = 2;
    }
    else
    {
      if (fmt_token_order[0] == 'y')
      {
        frag_length[0] = 4;
        frag_length[1] = 2;
        frag_length[2] = 2;
      }
      else if (fmt_token_order[1] == 'y')
      {
        frag_length[0] = 2;
        frag_length[1] = 4;
        frag_length[2] = 2;
      }
      else
      {
        frag_length[0] = 2;
        frag_length[1] = 2;
        frag_length[2] = 4;
      }
    }
    target_pos = 0;

       
                                                                         
                                                                       
               
       
    for (i = 0; i < 3; i++)
    {
      int start_pos = 0;

      if (i >= 1)
      {
        start_pos += frag_length[0];
      }
      if (i == 2)
      {
        start_pos += frag_length[1];
      }

      strncpy(str_copy + target_pos, str + start_pos, frag_length[i]);
      target_pos += frag_length[i];
      if (i != 2)
      {
        str_copy[target_pos] = ' ';
        target_pos++;
      }
    }
    str_copy[target_pos] = '\0';
  }
  else
  {
    str_copy = pgtypes_strdup(str);
    if (!str_copy)
    {
      return -1;
    }

                                                
    for (i = 0; str_copy[i]; i++)
    {
      str_copy[i] = (char)pg_tolower((unsigned char)str_copy[i]);
    }
  }

                                 
  reading_digit = 0;
  token_count = 0;
  for (i = 0; i < strlen(str_copy); i++)
  {
    if (!isdigit((unsigned char)str_copy[i]) && reading_digit)
    {
                                 
      token[token_count][1] = i - 1;
      reading_digit = 0;
      token_count++;
    }
    else if (isdigit((unsigned char)str_copy[i]) && !reading_digit)
    {
                                 
      token[token_count][0] = i;
      reading_digit = 1;
    }
  }

     
                                                                            
               
     
  if (reading_digit)
  {
    token[token_count][1] = i - 1;
    token_count++;
  }

  if (token_count < 2)
  {
       
                                                                         
               
       
    free(str_copy);
    errno = PGTYPES_DATE_ERR_ENOSHORTDATE;
    return -1;
  }

  if (token_count != 3)
  {
       
                                                                    
                                                                        
       
    char *month_lower_tmp = pgtypes_alloc(PGTYPES_DATE_MONTH_MAXLENGTH);
    char *start_pos;
    int j;
    int offset;
    int found = 0;
    char **list;

    if (!month_lower_tmp)
    {
                                             
      free(str_copy);
      return -1;
    }
    list = pgtypes_date_months;
    for (i = 0; list[i]; i++)
    {
      for (j = 0; j < PGTYPES_DATE_MONTH_MAXLENGTH; j++)
      {
        month_lower_tmp[j] = (char)pg_tolower((unsigned char)list[i][j]);
        if (!month_lower_tmp[j])
        {
                                   
          break;
        }
      }
      if ((start_pos = strstr(str_copy, month_lower_tmp)))
      {
        offset = start_pos - str_copy;

           
                                                                     
                     
           
        if (offset < token[0][0])
        {
          token[2][0] = token[1][0];
          token[2][1] = token[1][1];
          token[1][0] = token[0][0];
          token[1][1] = token[0][1];
          token_count = 0;
        }
        else if (offset < token[1][0])
        {
          token[2][0] = token[1][0];
          token[2][1] = token[1][1];
          token_count = 1;
        }
        else
        {
          token_count = 2;
        }
        token[token_count][0] = offset;
        token[token_count][1] = offset + strlen(month_lower_tmp) - 1;

           
                                                                      
                                    
           
        token_values[token_count] = i + 1;
        found = 1;
        break;
      }

         
                                                                       
                                                                         
                                          
         
      if (list == pgtypes_date_months)
      {
        if (list[i + 1] == NULL)
        {
          list = months;
          i = -1;
        }
      }
    }
    if (!found)
    {
      free(month_lower_tmp);
      free(str_copy);
      errno = PGTYPES_DATE_ERR_ENOTDMY;
      return -1;
    }

       
                                                     
                                                              
       
                                                                        
                                                                          
                                                                         
                                                                          
                                                                         
                                                          
       
    if (fmt_token_order[token_count] != 'm')
    {
                                        
      token_values[token_count] = -1;
    }
    free(month_lower_tmp);
  }

                                                              
  for (i = 0; i < 3; i++)
  {
    *(str_copy + token[i][1] + 1) = '\0';
                                                                      
    if (token_values[i] == -1)
    {
      errno = 0;
      token_values[i] = strtol(str_copy + token[i][0], (char **)NULL, 10);
                                                 
      if (errno)
      {
        token_values[i] = -1;
      }
    }
    if (fmt_token_order[i] == 'd')
    {
      tm.tm_mday = token_values[i];
    }
    else if (fmt_token_order[i] == 'm')
    {
      tm.tm_mon = token_values[i];
    }
    else if (fmt_token_order[i] == 'y')
    {
      tm.tm_year = token_values[i];
    }
  }
  free(str_copy);

  if (tm.tm_mday < 1 || tm.tm_mday > 31)
  {
    errno = PGTYPES_DATE_BAD_DAY;
    return -1;
  }

  if (tm.tm_mon < 1 || tm.tm_mon > MONTHS_PER_YEAR)
  {
    errno = PGTYPES_DATE_BAD_MONTH;
    return -1;
  }

  if (tm.tm_mday == 31 && (tm.tm_mon == 4 || tm.tm_mon == 6 || tm.tm_mon == 9 || tm.tm_mon == 11))
  {
    errno = PGTYPES_DATE_BAD_DAY;
    return -1;
  }

  if (tm.tm_mon == 2 && tm.tm_mday > 29)
  {
    errno = PGTYPES_DATE_BAD_DAY;
    return -1;
  }

  *d = date2j(tm.tm_year, tm.tm_mon, tm.tm_mday) - date2j(2000, 1, 1);

  return 0;
}
