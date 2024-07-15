                                                                            
                   
                                
   
                                                                
   
                                           
                                                              
   
   
                  
                                         
   
                                                                            
   
#include "postgres.h"

#include "common/int.h"
#include "utils/builtins.h"
#include "utils/formatting.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"

static text *
dotrim(const char *string, int stringlen, const char *set, int setlen, bool doltrim, bool dortrim);

                                                                      
   
         
   
           
   
                            
   
            
   
                                                          
   
                                                                      

Datum
lower(PG_FUNCTION_ARGS)
{
  text *in_string = PG_GETARG_TEXT_PP(0);
  char *out_string;
  text *result;

  out_string = str_tolower(VARDATA_ANY(in_string), VARSIZE_ANY_EXHDR(in_string), PG_GET_COLLATION());
  result = cstring_to_text(out_string);
  pfree(out_string);

  PG_RETURN_TEXT_P(result);
}

                                                                      
   
         
   
           
   
                            
   
            
   
                                                          
   
                                                                      

Datum
upper(PG_FUNCTION_ARGS)
{
  text *in_string = PG_GETARG_TEXT_PP(0);
  char *out_string;
  text *result;

  out_string = str_toupper(VARDATA_ANY(in_string), VARSIZE_ANY_EXHDR(in_string), PG_GET_COLLATION());
  result = cstring_to_text(out_string);
  pfree(out_string);

  PG_RETURN_TEXT_P(result);
}

                                                                      
   
           
   
           
   
                              
   
            
   
                                                                     
                                                                   
                                                           
                
   
                                                                      

Datum
initcap(PG_FUNCTION_ARGS)
{
  text *in_string = PG_GETARG_TEXT_PP(0);
  char *out_string;
  text *result;

  out_string = str_initcap(VARDATA_ANY(in_string), VARSIZE_ANY_EXHDR(in_string), PG_GET_COLLATION());
  result = cstring_to_text(out_string);
  pfree(out_string);

  PG_RETURN_TEXT_P(result);
}

                                                                      
   
        
   
           
   
                                                    
   
            
   
                                                                    
                                                                       
                                            
   
                                                                      

Datum
lpad(PG_FUNCTION_ARGS)
{
  text *string1 = PG_GETARG_TEXT_PP(0);
  int32 len = PG_GETARG_INT32(1);
  text *string2 = PG_GETARG_TEXT_PP(2);
  text *ret;
  char *ptr1, *ptr2, *ptr2start, *ptr2end, *ptr_ret;
  int m, s1len, s2len;

  int bytelen;

                                              
  if (len < 0)
  {
    len = 0;
  }

  s1len = VARSIZE_ANY_EXHDR(string1);
  if (s1len < 0)
  {
    s1len = 0;                       
  }

  s2len = VARSIZE_ANY_EXHDR(string2);
  if (s2len < 0)
  {
    s2len = 0;                       
  }

  s1len = pg_mbstrlen_with_len(VARDATA_ANY(string1), s1len);

  if (s1len > len)
  {
    s1len = len;                                    
  }

  if (s2len <= 0)
  {
    len = s1len;                                        
  }

  bytelen = pg_database_encoding_max_length() * len;

                                  
  if (len != 0 && bytelen / pg_database_encoding_max_length() != len)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("requested length too large")));
  }

  ret = (text *)palloc(VARHDRSZ + bytelen);

  m = len - s1len;

  ptr2 = ptr2start = VARDATA_ANY(string2);
  ptr2end = ptr2 + s2len;
  ptr_ret = VARDATA(ret);

  while (m--)
  {
    int mlen = pg_mblen(ptr2);

    memcpy(ptr_ret, ptr2, mlen);
    ptr_ret += mlen;
    ptr2 += mlen;
    if (ptr2 == ptr2end)                               
    {
      ptr2 = ptr2start;
    }
  }

  ptr1 = VARDATA_ANY(string1);

  while (s1len--)
  {
    int mlen = pg_mblen(ptr1);

    memcpy(ptr_ret, ptr1, mlen);
    ptr_ret += mlen;
    ptr1 += mlen;
  }

  SET_VARSIZE(ret, ptr_ret - (char *)ret);

  PG_RETURN_TEXT_P(ret);
}

                                                                      
   
        
   
           
   
                                                    
   
            
   
                                                                     
                                                                       
                                            
   
                                                                      

Datum
rpad(PG_FUNCTION_ARGS)
{
  text *string1 = PG_GETARG_TEXT_PP(0);
  int32 len = PG_GETARG_INT32(1);
  text *string2 = PG_GETARG_TEXT_PP(2);
  text *ret;
  char *ptr1, *ptr2, *ptr2start, *ptr2end, *ptr_ret;
  int m, s1len, s2len;

  int bytelen;

                                              
  if (len < 0)
  {
    len = 0;
  }

  s1len = VARSIZE_ANY_EXHDR(string1);
  if (s1len < 0)
  {
    s1len = 0;                       
  }

  s2len = VARSIZE_ANY_EXHDR(string2);
  if (s2len < 0)
  {
    s2len = 0;                       
  }

  s1len = pg_mbstrlen_with_len(VARDATA_ANY(string1), s1len);

  if (s1len > len)
  {
    s1len = len;                                    
  }

  if (s2len <= 0)
  {
    len = s1len;                                        
  }

  bytelen = pg_database_encoding_max_length() * len;

                                  
  if (len != 0 && bytelen / pg_database_encoding_max_length() != len)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("requested length too large")));
  }

  ret = (text *)palloc(VARHDRSZ + bytelen);
  m = len - s1len;

  ptr1 = VARDATA_ANY(string1);
  ptr_ret = VARDATA(ret);

  while (s1len--)
  {
    int mlen = pg_mblen(ptr1);

    memcpy(ptr_ret, ptr1, mlen);
    ptr_ret += mlen;
    ptr1 += mlen;
  }

  ptr2 = ptr2start = VARDATA_ANY(string2);
  ptr2end = ptr2 + s2len;

  while (m--)
  {
    int mlen = pg_mblen(ptr2);

    memcpy(ptr_ret, ptr2, mlen);
    ptr_ret += mlen;
    ptr2 += mlen;
    if (ptr2 == ptr2end)                               
    {
      ptr2 = ptr2start;
    }
  }

  SET_VARSIZE(ret, ptr_ret - (char *)ret);

  PG_RETURN_TEXT_P(ret);
}

                                                                      
   
         
   
           
   
                                      
   
            
   
                                                                   
                                          
   
                                                                      

Datum
btrim(PG_FUNCTION_ARGS)
{
  text *string = PG_GETARG_TEXT_PP(0);
  text *set = PG_GETARG_TEXT_PP(1);
  text *ret;

  ret = dotrim(VARDATA_ANY(string), VARSIZE_ANY_EXHDR(string), VARDATA_ANY(set), VARSIZE_ANY_EXHDR(set), true, true);

  PG_RETURN_TEXT_P(ret);
}

                                                                      
   
                                          
   
                                                                      

Datum
btrim1(PG_FUNCTION_ARGS)
{
  text *string = PG_GETARG_TEXT_PP(0);
  text *ret;

  ret = dotrim(VARDATA_ANY(string), VARSIZE_ANY_EXHDR(string), " ", 1, true, true);

  PG_RETURN_TEXT_P(ret);
}

   
                                                 
   
static text *
dotrim(const char *string, int stringlen, const char *set, int setlen, bool doltrim, bool dortrim)
{
  int i;

                                                      
  if (stringlen > 0 && setlen > 0)
  {
    if (pg_database_encoding_max_length() > 1)
    {
         
                                                                     
                                                                      
                          
         
      const char **stringchars;
      const char **setchars;
      int *stringmblen;
      int *setmblen;
      int stringnchars;
      int setnchars;
      int resultndx;
      int resultnchars;
      const char *p;
      int len;
      int mblen;
      const char *str_pos;
      int str_len;

      stringchars = (const char **)palloc(stringlen * sizeof(char *));
      stringmblen = (int *)palloc(stringlen * sizeof(int));
      stringnchars = 0;
      p = string;
      len = stringlen;
      while (len > 0)
      {
        stringchars[stringnchars] = p;
        stringmblen[stringnchars] = mblen = pg_mblen(p);
        stringnchars++;
        p += mblen;
        len -= mblen;
      }

      setchars = (const char **)palloc(setlen * sizeof(char *));
      setmblen = (int *)palloc(setlen * sizeof(int));
      setnchars = 0;
      p = set;
      len = setlen;
      while (len > 0)
      {
        setchars[setnchars] = p;
        setmblen[setnchars] = mblen = pg_mblen(p);
        setnchars++;
        p += mblen;
        len -= mblen;
      }

      resultndx = 0;                             
      resultnchars = stringnchars;

      if (doltrim)
      {
        while (resultnchars > 0)
        {
          str_pos = stringchars[resultndx];
          str_len = stringmblen[resultndx];
          for (i = 0; i < setnchars; i++)
          {
            if (str_len == setmblen[i] && memcmp(str_pos, setchars[i], str_len) == 0)
            {
              break;
            }
          }
          if (i >= setnchars)
          {
            break;                    
          }
          string += str_len;
          stringlen -= str_len;
          resultndx++;
          resultnchars--;
        }
      }

      if (dortrim)
      {
        while (resultnchars > 0)
        {
          str_pos = stringchars[resultndx + resultnchars - 1];
          str_len = stringmblen[resultndx + resultnchars - 1];
          for (i = 0; i < setnchars; i++)
          {
            if (str_len == setmblen[i] && memcmp(str_pos, setchars[i], str_len) == 0)
            {
              break;
            }
          }
          if (i >= setnchars)
          {
            break;                    
          }
          stringlen -= str_len;
          resultnchars--;
        }
      }

      pfree(stringchars);
      pfree(stringmblen);
      pfree(setchars);
      pfree(setmblen);
    }
    else
    {
         
                                                                        
         
      if (doltrim)
      {
        while (stringlen > 0)
        {
          char str_ch = *string;

          for (i = 0; i < setlen; i++)
          {
            if (str_ch == set[i])
            {
              break;
            }
          }
          if (i >= setlen)
          {
            break;                    
          }
          string++;
          stringlen--;
        }
      }

      if (dortrim)
      {
        while (stringlen > 0)
        {
          char str_ch = string[stringlen - 1];

          for (i = 0; i < setlen; i++)
          {
            if (str_ch == set[i])
            {
              break;
            }
          }
          if (i >= setlen)
          {
            break;                    
          }
          stringlen--;
        }
      }
    }
  }

                                         
  return cstring_to_text_with_len(string, stringlen);
}

                                                                      
   
             
   
           
   
                                             
   
            
   
                                                                   
                                          
   
                                               
                                                                      

Datum
byteatrim(PG_FUNCTION_ARGS)
{
  bytea *string = PG_GETARG_BYTEA_PP(0);
  bytea *set = PG_GETARG_BYTEA_PP(1);
  bytea *ret;
  char *ptr, *end, *ptr2, *ptr2start, *end2;
  int m, stringlen, setlen;

  stringlen = VARSIZE_ANY_EXHDR(string);
  setlen = VARSIZE_ANY_EXHDR(set);

  if (stringlen <= 0 || setlen <= 0)
  {
    PG_RETURN_BYTEA_P(string);
  }

  m = stringlen;
  ptr = VARDATA_ANY(string);
  end = ptr + stringlen - 1;
  ptr2start = VARDATA_ANY(set);
  end2 = ptr2start + setlen - 1;

  while (m > 0)
  {
    ptr2 = ptr2start;
    while (ptr2 <= end2)
    {
      if (*ptr == *ptr2)
      {
        break;
      }
      ++ptr2;
    }
    if (ptr2 > end2)
    {
      break;
    }
    ptr++;
    m--;
  }

  while (m > 0)
  {
    ptr2 = ptr2start;
    while (ptr2 <= end2)
    {
      if (*end == *ptr2)
      {
        break;
      }
      ++ptr2;
    }
    if (ptr2 > end2)
    {
      break;
    }
    end--;
    m--;
  }

  ret = (bytea *)palloc(VARHDRSZ + m);
  SET_VARSIZE(ret, VARHDRSZ + m);
  memcpy(VARDATA(ret), ptr, m);

  PG_RETURN_BYTEA_P(ret);
}

                                                                      
   
         
   
           
   
                                      
   
            
   
                                                                   
                          
   
                                                                      

Datum
ltrim(PG_FUNCTION_ARGS)
{
  text *string = PG_GETARG_TEXT_PP(0);
  text *set = PG_GETARG_TEXT_PP(1);
  text *ret;

  ret = dotrim(VARDATA_ANY(string), VARSIZE_ANY_EXHDR(string), VARDATA_ANY(set), VARSIZE_ANY_EXHDR(set), true, false);

  PG_RETURN_TEXT_P(ret);
}

                                                                      
   
                                          
   
                                                                      

Datum
ltrim1(PG_FUNCTION_ARGS)
{
  text *string = PG_GETARG_TEXT_PP(0);
  text *ret;

  ret = dotrim(VARDATA_ANY(string), VARSIZE_ANY_EXHDR(string), " ", 1, true, false);

  PG_RETURN_TEXT_P(ret);
}

                                                                      
   
         
   
           
   
                                      
   
            
   
                                                                
                          
   
                                                                      

Datum
rtrim(PG_FUNCTION_ARGS)
{
  text *string = PG_GETARG_TEXT_PP(0);
  text *set = PG_GETARG_TEXT_PP(1);
  text *ret;

  ret = dotrim(VARDATA_ANY(string), VARSIZE_ANY_EXHDR(string), VARDATA_ANY(set), VARSIZE_ANY_EXHDR(set), false, true);

  PG_RETURN_TEXT_P(ret);
}

                                                                      
   
                                          
   
                                                                      

Datum
rtrim1(PG_FUNCTION_ARGS)
{
  text *string = PG_GETARG_TEXT_PP(0);
  text *ret;

  ret = dotrim(VARDATA_ANY(string), VARSIZE_ANY_EXHDR(string), " ", 1, false, true);

  PG_RETURN_TEXT_P(ret);
}

                                                                      
   
             
   
           
   
                                                    
   
            
   
                                                                         
                                                                        
                                                             
                                                      
   
                                                                      

Datum
translate(PG_FUNCTION_ARGS)
{
  text *string = PG_GETARG_TEXT_PP(0);
  text *from = PG_GETARG_TEXT_PP(1);
  text *to = PG_GETARG_TEXT_PP(2);
  text *result;
  char *from_ptr, *to_ptr;
  char *source, *target;
  int m, fromlen, tolen, retlen, i;
  int worst_len;
  int len;
  int source_len;
  int from_index;

  m = VARSIZE_ANY_EXHDR(string);
  if (m <= 0)
  {
    PG_RETURN_TEXT_P(string);
  }
  source = VARDATA_ANY(string);

  fromlen = VARSIZE_ANY_EXHDR(from);
  from_ptr = VARDATA_ANY(from);
  tolen = VARSIZE_ANY_EXHDR(to);
  to_ptr = VARDATA_ANY(to);

     
                                                                            
                                                           
     
  worst_len = pg_database_encoding_max_length() * m;

                                  
  if (worst_len / pg_database_encoding_max_length() != m)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("requested length too large")));
  }

  result = (text *)palloc(worst_len + VARHDRSZ);
  target = VARDATA(result);
  retlen = 0;

  while (m > 0)
  {
    source_len = pg_mblen(source);
    from_index = 0;

    for (i = 0; i < fromlen; i += len)
    {
      len = pg_mblen(&from_ptr[i]);
      if (len == source_len && memcmp(source, &from_ptr[i], len) == 0)
      {
        break;
      }

      from_index++;
    }
    if (i < fromlen)
    {
                      
      char *p = to_ptr;

      for (i = 0; i < from_index; i++)
      {
        p += pg_mblen(p);
        if (p >= (to_ptr + tolen))
        {
          break;
        }
      }
      if (p < (to_ptr + tolen))
      {
        len = pg_mblen(p);
        memcpy(target, p, len);
        target += len;
        retlen += len;
      }
    }
    else
    {
                             
      memcpy(target, source, source_len);
      target += source_len;
      retlen += source_len;
    }

    source += source_len;
    m -= source_len;
  }

  SET_VARSIZE(result, retlen + VARHDRSZ);

     
                                                                             
                                                                          
                                      
     

  PG_RETURN_TEXT_P(result);
}

                                                                      
   
         
   
           
   
                           
   
            
   
                                                                   
            
                                        
                                                                       
                                                                  
                                                                   
                                         
                                                                   
                    
   
                                                                      

Datum
ascii(PG_FUNCTION_ARGS)
{
  text *string = PG_GETARG_TEXT_PP(0);
  int encoding = GetDatabaseEncoding();
  unsigned char *data;

  if (VARSIZE_ANY_EXHDR(string) <= 0)
  {
    PG_RETURN_INT32(0);
  }

  data = (unsigned char *)VARDATA_ANY(string);

  if (encoding == PG_UTF8 && *data > 127)
  {
                                           

    int result = 0, tbytes = 0, i;

    if (*data >= 0xF0)
    {
      result = *data & 0x07;
      tbytes = 3;
    }
    else if (*data >= 0xE0)
    {
      result = *data & 0x0F;
      tbytes = 2;
    }
    else
    {
      Assert(*data > 0xC0);
      result = *data & 0x1f;
      tbytes = 1;
    }

    Assert(tbytes > 0);

    for (i = 1; i <= tbytes; i++)
    {
      Assert((data[i] & 0xC0) == 0x80);
      result = (result << 6) + (data[i] & 0x3f);
    }

    PG_RETURN_INT32(result);
  }
  else
  {
    if (pg_encoding_max_length(encoding) > 1 && *data > 127)
    {
      ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("requested character too large")));
    }

    PG_RETURN_INT32((int32)*data);
  }
}

                                                                      
   
       
   
           
   
                      
   
            
   
                                                              
   
                                                           
                                                                  
                                            
   
                                                                      
                                                                   
                                       
   
                                                                      

Datum
chr(PG_FUNCTION_ARGS)
{
  uint32 cvalue = PG_GETARG_UINT32(0);
  text *result;
  int encoding = GetDatabaseEncoding();

  if (encoding == PG_UTF8 && cvalue > 127)
  {
                                                           
    int bytes;
    unsigned char *wch;

       
                                                                          
                                                                         
                 
       
    if (cvalue > 0x0010ffff)
    {
      ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("requested character too large for encoding: %d", cvalue)));
    }

    if (cvalue > 0xffff)
    {
      bytes = 4;
    }
    else if (cvalue > 0x07ff)
    {
      bytes = 3;
    }
    else
    {
      bytes = 2;
    }

    result = (text *)palloc(VARHDRSZ + bytes);
    SET_VARSIZE(result, VARHDRSZ + bytes);
    wch = (unsigned char *)VARDATA(result);

    if (bytes == 2)
    {
      wch[0] = 0xC0 | ((cvalue >> 6) & 0x1F);
      wch[1] = 0x80 | (cvalue & 0x3F);
    }
    else if (bytes == 3)
    {
      wch[0] = 0xE0 | ((cvalue >> 12) & 0x0F);
      wch[1] = 0x80 | ((cvalue >> 6) & 0x3F);
      wch[2] = 0x80 | (cvalue & 0x3F);
    }
    else
    {
      wch[0] = 0xF0 | ((cvalue >> 18) & 0x07);
      wch[1] = 0x80 | ((cvalue >> 12) & 0x3F);
      wch[2] = 0x80 | ((cvalue >> 6) & 0x3F);
      wch[3] = 0x80 | (cvalue & 0x3F);
    }

       
                                                                         
                                                                           
             
       
    if (!pg_utf8_islegal(wch, bytes))
    {
      ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("requested character not valid for encoding: %d", cvalue)));
    }
  }
  else
  {
    bool is_mb;

       
                                                                          
                                  
       
    if (cvalue == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("null character not permitted")));
    }

    is_mb = pg_encoding_max_length(encoding) > 1;

    if ((is_mb && (cvalue > 127)) || (!is_mb && (cvalue > 255)))
    {
      ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("requested character too large for encoding: %d", cvalue)));
    }

    result = (text *)palloc(VARHDRSZ + 1);
    SET_VARSIZE(result, VARHDRSZ + 1);
    *VARDATA(result) = (char)cvalue;
  }

  PG_RETURN_TEXT_P(result);
}

                                                                      
   
          
   
           
   
                                      
   
            
   
                         
   
                                                                      

Datum
repeat(PG_FUNCTION_ARGS)
{
  text *string = PG_GETARG_TEXT_PP(0);
  int32 count = PG_GETARG_INT32(1);
  text *result;
  int slen, tlen;
  int i;
  char *cp, *sp;

  if (count < 0)
  {
    count = 0;
  }

  slen = VARSIZE_ANY_EXHDR(string);

  if (unlikely(pg_mul_s32_overflow(count, slen, &tlen)) || unlikely(pg_add_s32_overflow(tlen, VARHDRSZ, &tlen)))
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("requested length too large")));
  }

  result = (text *)palloc(tlen);

  SET_VARSIZE(result, tlen);
  cp = VARDATA(result);
  sp = VARDATA_ANY(string);
  for (i = 0; i < count; i++)
  {
    memcpy(cp, sp, slen);
    cp += slen;
    CHECK_FOR_INTERRUPTS();
  }

  PG_RETURN_TEXT_P(result);
}
