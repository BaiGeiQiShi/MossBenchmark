                                                                            
   
                                                          
   
   
                                                                         
                                                                        
   
                          
   
                                                                            
   
#include "postgres_fe.h"

#include "fe_utils/mbprint.h"

#include "libpq-fe.h"

   
                                                                       
                                                                        
                                                                    
                                                                     
                                 
   
                                                                        
                                          
   

typedef unsigned int pg_wchar;

static int
pg_get_utf8_id(void)
{
  static int utf8_id = -1;

  if (utf8_id < 0)
  {
    utf8_id = pg_char_to_encoding("utf8");
  }
  return utf8_id;
}

#define PG_UTF8 pg_get_utf8_id()

   
                                                      
                                                             
   
                                                               
   
static pg_wchar
utf8_to_unicode(const unsigned char *c)
{
  if ((*c & 0x80) == 0)
  {
    return (pg_wchar)c[0];
  }
  else if ((*c & 0xe0) == 0xc0)
  {
    return (pg_wchar)(((c[0] & 0x1f) << 6) | (c[1] & 0x3f));
  }
  else if ((*c & 0xf0) == 0xe0)
  {
    return (pg_wchar)(((c[0] & 0x0f) << 12) | ((c[1] & 0x3f) << 6) | (c[2] & 0x3f));
  }
  else if ((*c & 0xf8) == 0xf0)
  {
    return (pg_wchar)(((c[0] & 0x07) << 18) | ((c[1] & 0x3f) << 12) | ((c[2] & 0x3f) << 6) | (c[3] & 0x3f));
  }
  else
  {
                                            
    return 0xffffffff;
  }
}

   
                                                                       
                                                                           
                                                                          
                                                                   
   
static int
utf_charcheck(const unsigned char *c)
{
  if ((*c & 0x80) == 0)
  {
    return 1;
  }
  else if ((*c & 0xe0) == 0xc0)
  {
                       
    if (((c[1] & 0xc0) == 0x80) && ((c[0] & 0x1f) > 0x01))
    {
      return 2;
    }
    return -1;
  }
  else if ((*c & 0xf0) == 0xe0)
  {
                         
    if (((c[1] & 0xc0) == 0x80) && (((c[0] & 0x0f) != 0x00) || ((c[1] & 0x20) == 0x20)) && ((c[2] & 0xc0) == 0x80))
    {
      int z = c[0] & 0x0f;
      int yx = ((c[1] & 0x3f) << 6) | (c[0] & 0x3f);
      int lx = yx & 0x7f;

                                                                 
      if (((z == 0x0f) && (((yx & 0xffe) == 0xffe) || (((yx & 0xf80) == 0xd80) && (lx >= 0x30) && (lx <= 0x4f)))) || ((z == 0x0d) && ((yx & 0xb00) == 0x800)))
      {
        return -1;
      }
      return 3;
    }
    return -1;
  }
  else if ((*c & 0xf8) == 0xf0)
  {
    int u = ((c[0] & 0x07) << 2) | ((c[1] & 0x30) >> 4);

                        
    if (((c[1] & 0xc0) == 0x80) && (u > 0x00) && (u <= 0x10) && ((c[2] & 0xc0) == 0x80) && ((c[3] & 0xc0) == 0x80))
    {
                                           
      if (((c[1] & 0x0f) == 0x0f) && ((c[2] & 0x3f) == 0x3f) && ((c[3] & 0x3e) == 0x3e))
      {
        return -1;
      }
      return 4;
    }
    return -1;
  }
  return -1;
}

static void
mb_utf_validate(unsigned char *pwcs)
{
  unsigned char *p = pwcs;

  while (*pwcs)
  {
    int len;

    if ((len = utf_charcheck(pwcs)) > 0)
    {
      if (p != pwcs)
      {
        int i;

        for (i = 0; i < len; i++)
        {
          *p++ = *pwcs++;
        }
      }
      else
      {
        pwcs += len;
        p += len;
      }
    }
    else
    {
                            
      pwcs++;
    }
  }
  if (p != pwcs)
  {
    *p = '\0';
  }
}

   
                                              
   

   
                                                   
                                                       
                                                                    
   
int
pg_wcswidth(const char *pwcs, size_t len, int encoding)
{
  int width = 0;

  while (len > 0)
  {
    int chlen, chwidth;

    chlen = PQmblen(pwcs, encoding);
    if (len < (size_t)chlen)
    {
      break;                     
    }

    chwidth = PQdsplen(pwcs, encoding);
    if (chwidth > 0)
    {
      width += chwidth;
    }

    pwcs += chlen;
    len -= chlen;
  }
  return width;
}

   
                                                                             
           
                                                                             
                                                      
                                                                     
                             
   
                                                
   
void
pg_wcssize(const unsigned char *pwcs, size_t len, int encoding, int *result_width, int *result_height, int *result_format_size)
{
  int w, chlen = 0, linewidth = 0;
  int width = 0;
  int height = 1;
  int format_size = 0;

  for (; *pwcs && len > 0; pwcs += chlen)
  {
    chlen = PQmblen((const char *)pwcs, encoding);
    if (len < (size_t)chlen)
    {
      break;
    }
    w = PQdsplen((const char *)pwcs, encoding);

    if (chlen == 1)                       
    {
      if (*pwcs == '\n')              
      {
        if (linewidth > width)
        {
          width = linewidth;
        }
        linewidth = 0;
        height += 1;
        format_size += 1;                   
      }
      else if (*pwcs == '\r')               
      {
        linewidth += 2;
        format_size += 2;
      }
      else if (*pwcs == '\t')          
      {
        do
        {
          linewidth++;
          format_size++;
        } while (linewidth % 8 != 0);
      }
      else if (w < 0)                         
      {
        linewidth += 4;
        format_size += 4;
      }
      else                      
      {
        linewidth += w;
        format_size += 1;
      }
    }
    else if (w < 0)                             
    {
      linewidth += 6;             
      format_size += 6;
    }
    else                      
    {
      linewidth += w;
      format_size += chlen;
    }
    len -= chlen;
  }
  if (linewidth > width)
  {
    width = linewidth;
  }
  format_size += 1;                   

                   
  if (result_width)
  {
    *result_width = width;
  }
  if (result_height)
  {
    *result_height = height;
  }
  if (result_format_size)
  {
    *result_format_size = format_size;
  }
}

   
                                                            
                                                        
   
                                              
   
void
pg_wcsformat(const unsigned char *pwcs, size_t len, int encoding, struct lineptr *lines, int count)
{
  int w, chlen = 0;
  int linewidth = 0;
  unsigned char *ptr = lines->ptr;                           

  for (; *pwcs && len > 0; pwcs += chlen)
  {
    chlen = PQmblen((const char *)pwcs, encoding);
    if (len < (size_t)chlen)
    {
      break;
    }
    w = PQdsplen((const char *)pwcs, encoding);

    if (chlen == 1)                       
    {
      if (*pwcs == '\n')              
      {
        *ptr++ = '\0';
        lines->width = linewidth;
        linewidth = 0;
        lines++;
        count--;
        if (count <= 0)
        {
          exit(1);              
        }

                                                      
        lines->ptr = ptr;
      }
      else if (*pwcs == '\r')               
      {
        strcpy((char *)ptr, "\\r");
        linewidth += 2;
        ptr += 2;
      }
      else if (*pwcs == '\t')          
      {
        do
        {
          *ptr++ = ' ';
          linewidth++;
        } while (linewidth % 8 != 0);
      }
      else if (w < 0)                         
      {
        sprintf((char *)ptr, "\\x%02X", *pwcs);
        linewidth += 4;
        ptr += 4;
      }
      else                      
      {
        linewidth += w;
        *ptr++ = *pwcs;
      }
    }
    else if (w < 0)                             
    {
      if (encoding == PG_UTF8)
      {
        sprintf((char *)ptr, "\\u%04X", utf8_to_unicode(pwcs));
      }
      else
      {
           
                                                                    
                                                                       
                                       
           
        sprintf((char *)ptr, "\\u????");
      }
      ptr += 6;
      linewidth += 6;
    }
    else                      
    {
      int i;

      for (i = 0; i < chlen; i++)
      {
        *ptr++ = pwcs[i];
      }
      linewidth += w;
    }
    len -= chlen;
  }
  lines->width = linewidth;
  *ptr++ = '\0';                                 

  if (count <= 0)
  {
    exit(1);              
  }

  (lines + 1)->ptr = NULL;                           
}

   
                                                                            
   
                                                               
   
unsigned char *
mbvalidate(unsigned char *pwcs, int encoding)
{
  if (encoding == PG_UTF8)
  {
    mb_utf_validate(pwcs);
  }
  else
  {
       
                                                                        
            
       
  }

  return pwcs;
}
