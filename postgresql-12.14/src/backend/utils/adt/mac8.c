                                                                            
   
          
                                                                    
   
                                                                         
                                                                             
   
                                               
   
                                                                        
                    
   
                                                                         
   
                  
                                   
   
                                                                            
   

#include "postgres.h"

#include "libpq/pqformat.h"
#include "utils/builtins.h"
#include "utils/hashutils.h"
#include "utils/inet.h"

   
                                                  
   
#define hibits(addr) ((unsigned long)(((addr)->a << 24) | ((addr)->b << 16) | ((addr)->c << 8) | ((addr)->d)))

#define lobits(addr) ((unsigned long)(((addr)->e << 24) | ((addr)->f << 16) | ((addr)->g << 8) | ((addr)->h)))

static unsigned char
hex2_to_uchar(const unsigned char *str, const unsigned char *ptr);

static const signed char hexlookup[128] = {
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    10,
    11,
    12,
    13,
    14,
    15,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    10,
    11,
    12,
    13,
    14,
    15,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
};

   
                                                                  
   
                                                                               
                                              
   
                                                                               
                                                              
   
static inline unsigned char
hex2_to_uchar(const unsigned char *ptr, const unsigned char *str)
{
  unsigned char ret = 0;
  signed char lookup;

                                  
  if (*ptr > 127)
  {
    goto invalid_input;
  }

  lookup = hexlookup[*ptr];
  if (lookup < 0)
  {
    goto invalid_input;
  }

  ret = lookup << 4;

                                    
  ptr++;

  if (*ptr > 127)
  {
    goto invalid_input;
  }

  lookup = hexlookup[*ptr];
  if (lookup < 0)
  {
    goto invalid_input;
  }

  ret += lookup;

  return ret;

invalid_input:
  ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "macaddr8", str)));

                                     
  return 0;
}

   
                                                                             
   
Datum
macaddr8_in(PG_FUNCTION_ARGS)
{
  const unsigned char *str = (unsigned char *)PG_GETARG_CSTRING(0);
  const unsigned char *ptr = str;
  macaddr8 *result;
  unsigned char a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0, h = 0;
  int count = 0;
  unsigned char spacer = '\0';

                           
  while (*ptr && isspace(*ptr))
  {
    ptr++;
  }

                                        
  while (*ptr && *(ptr + 1))
  {
       
                                                                         
                                                                          
                                                            
       

                                   
    count++;

    switch (count)
    {
    case 1:
      a = hex2_to_uchar(ptr, str);
      break;
    case 2:
      b = hex2_to_uchar(ptr, str);
      break;
    case 3:
      c = hex2_to_uchar(ptr, str);
      break;
    case 4:
      d = hex2_to_uchar(ptr, str);
      break;
    case 5:
      e = hex2_to_uchar(ptr, str);
      break;
    case 6:
      f = hex2_to_uchar(ptr, str);
      break;
    case 7:
      g = hex2_to_uchar(ptr, str);
      break;
    case 8:
      h = hex2_to_uchar(ptr, str);
      break;
    default:
                                       
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "macaddr8", str)));
    }

                                                       
    ptr += 2;

                                                                   
    if (*ptr == ':' || *ptr == '-' || *ptr == '.')
    {
                                                                       
      if (spacer == '\0')
      {
        spacer = *ptr;
      }

                                                  
      else if (spacer != *ptr)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "macaddr8", str)));
      }

                                
      ptr++;
    }

                                                                 
    if (count == 6 || count == 8)
    {
      if (isspace(*ptr))
      {
        while (*++ptr && isspace(*ptr))
          ;

                                                                  
        if (*ptr)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "macaddr8", str)));
        }
      }
    }
  }

                                                
  if (count == 6)
  {
    h = f;
    g = e;
    f = d;

    d = 0xFF;
    e = 0xFE;
  }
  else if (count != 8)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "macaddr8", str)));
  }

  result = (macaddr8 *)palloc0(sizeof(macaddr8));

  result->a = a;
  result->b = b;
  result->c = c;
  result->d = d;
  result->e = e;
  result->f = f;
  result->g = g;
  result->h = h;

  PG_RETURN_MACADDR8_P(result);
}

   
                                                        
   
Datum
macaddr8_out(PG_FUNCTION_ARGS)
{
  macaddr8 *addr = PG_GETARG_MACADDR8_P(0);
  char *result;

  result = (char *)palloc(32);

  snprintf(result, 32, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", addr->a, addr->b, addr->c, addr->d, addr->e, addr->f, addr->g, addr->h);

  PG_RETURN_CSTRING(result);
}

   
                                                                                  
   
                                                                   
   
Datum
macaddr8_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  macaddr8 *addr;

  addr = (macaddr8 *)palloc0(sizeof(macaddr8));

  addr->a = pq_getmsgbyte(buf);
  addr->b = pq_getmsgbyte(buf);
  addr->c = pq_getmsgbyte(buf);

  if (buf->len == 6)
  {
    addr->d = 0xFF;
    addr->e = 0xFE;
  }
  else
  {
    addr->d = pq_getmsgbyte(buf);
    addr->e = pq_getmsgbyte(buf);
  }

  addr->f = pq_getmsgbyte(buf);
  addr->g = pq_getmsgbyte(buf);
  addr->h = pq_getmsgbyte(buf);

  PG_RETURN_MACADDR8_P(addr);
}

   
                                                              
   
Datum
macaddr8_send(PG_FUNCTION_ARGS)
{
  macaddr8 *addr = PG_GETARG_MACADDR8_P(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendbyte(&buf, addr->a);
  pq_sendbyte(&buf, addr->b);
  pq_sendbyte(&buf, addr->c);
  pq_sendbyte(&buf, addr->d);
  pq_sendbyte(&buf, addr->e);
  pq_sendbyte(&buf, addr->f);
  pq_sendbyte(&buf, addr->g);
  pq_sendbyte(&buf, addr->h);

  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

   
                                                            
   
static int32
macaddr8_cmp_internal(macaddr8 *a1, macaddr8 *a2)
{
  if (hibits(a1) < hibits(a2))
  {
    return -1;
  }
  else if (hibits(a1) > hibits(a2))
  {
    return 1;
  }
  else if (lobits(a1) < lobits(a2))
  {
    return -1;
  }
  else if (lobits(a1) > lobits(a2))
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

Datum
macaddr8_cmp(PG_FUNCTION_ARGS)
{
  macaddr8 *a1 = PG_GETARG_MACADDR8_P(0);
  macaddr8 *a2 = PG_GETARG_MACADDR8_P(1);

  PG_RETURN_INT32(macaddr8_cmp_internal(a1, a2));
}

   
                                 
   

Datum
macaddr8_lt(PG_FUNCTION_ARGS)
{
  macaddr8 *a1 = PG_GETARG_MACADDR8_P(0);
  macaddr8 *a2 = PG_GETARG_MACADDR8_P(1);

  PG_RETURN_BOOL(macaddr8_cmp_internal(a1, a2) < 0);
}

Datum
macaddr8_le(PG_FUNCTION_ARGS)
{
  macaddr8 *a1 = PG_GETARG_MACADDR8_P(0);
  macaddr8 *a2 = PG_GETARG_MACADDR8_P(1);

  PG_RETURN_BOOL(macaddr8_cmp_internal(a1, a2) <= 0);
}

Datum
macaddr8_eq(PG_FUNCTION_ARGS)
{
  macaddr8 *a1 = PG_GETARG_MACADDR8_P(0);
  macaddr8 *a2 = PG_GETARG_MACADDR8_P(1);

  PG_RETURN_BOOL(macaddr8_cmp_internal(a1, a2) == 0);
}

Datum
macaddr8_ge(PG_FUNCTION_ARGS)
{
  macaddr8 *a1 = PG_GETARG_MACADDR8_P(0);
  macaddr8 *a2 = PG_GETARG_MACADDR8_P(1);

  PG_RETURN_BOOL(macaddr8_cmp_internal(a1, a2) >= 0);
}

Datum
macaddr8_gt(PG_FUNCTION_ARGS)
{
  macaddr8 *a1 = PG_GETARG_MACADDR8_P(0);
  macaddr8 *a2 = PG_GETARG_MACADDR8_P(1);

  PG_RETURN_BOOL(macaddr8_cmp_internal(a1, a2) > 0);
}

Datum
macaddr8_ne(PG_FUNCTION_ARGS)
{
  macaddr8 *a1 = PG_GETARG_MACADDR8_P(0);
  macaddr8 *a2 = PG_GETARG_MACADDR8_P(1);

  PG_RETURN_BOOL(macaddr8_cmp_internal(a1, a2) != 0);
}

   
                                                  
   
Datum
hashmacaddr8(PG_FUNCTION_ARGS)
{
  macaddr8 *key = PG_GETARG_MACADDR8_P(0);

  return hash_any((unsigned char *)key, sizeof(macaddr8));
}

Datum
hashmacaddr8extended(PG_FUNCTION_ARGS)
{
  macaddr8 *key = PG_GETARG_MACADDR8_P(0);

  return hash_any_extended((unsigned char *)key, sizeof(macaddr8), PG_GETARG_INT64(1));
}

   
                                               
   
Datum
macaddr8_not(PG_FUNCTION_ARGS)
{
  macaddr8 *addr = PG_GETARG_MACADDR8_P(0);
  macaddr8 *result;

  result = (macaddr8 *)palloc0(sizeof(macaddr8));
  result->a = ~addr->a;
  result->b = ~addr->b;
  result->c = ~addr->c;
  result->d = ~addr->d;
  result->e = ~addr->e;
  result->f = ~addr->f;
  result->g = ~addr->g;
  result->h = ~addr->h;

  PG_RETURN_MACADDR8_P(result);
}

Datum
macaddr8_and(PG_FUNCTION_ARGS)
{
  macaddr8 *addr1 = PG_GETARG_MACADDR8_P(0);
  macaddr8 *addr2 = PG_GETARG_MACADDR8_P(1);
  macaddr8 *result;

  result = (macaddr8 *)palloc0(sizeof(macaddr8));
  result->a = addr1->a & addr2->a;
  result->b = addr1->b & addr2->b;
  result->c = addr1->c & addr2->c;
  result->d = addr1->d & addr2->d;
  result->e = addr1->e & addr2->e;
  result->f = addr1->f & addr2->f;
  result->g = addr1->g & addr2->g;
  result->h = addr1->h & addr2->h;

  PG_RETURN_MACADDR8_P(result);
}

Datum
macaddr8_or(PG_FUNCTION_ARGS)
{
  macaddr8 *addr1 = PG_GETARG_MACADDR8_P(0);
  macaddr8 *addr2 = PG_GETARG_MACADDR8_P(1);
  macaddr8 *result;

  result = (macaddr8 *)palloc0(sizeof(macaddr8));
  result->a = addr1->a | addr2->a;
  result->b = addr1->b | addr2->b;
  result->c = addr1->c | addr2->c;
  result->d = addr1->d | addr2->d;
  result->e = addr1->e | addr2->e;
  result->f = addr1->f | addr2->f;
  result->g = addr1->g | addr2->g;
  result->h = addr1->h | addr2->h;

  PG_RETURN_MACADDR8_P(result);
}

   
                                                                  
   
Datum
macaddr8_trunc(PG_FUNCTION_ARGS)
{
  macaddr8 *addr = PG_GETARG_MACADDR8_P(0);
  macaddr8 *result;

  result = (macaddr8 *)palloc0(sizeof(macaddr8));

  result->a = addr->a;
  result->b = addr->b;
  result->c = addr->c;
  result->d = 0;
  result->e = 0;
  result->f = 0;
  result->g = 0;
  result->h = 0;

  PG_RETURN_MACADDR8_P(result);
}

   
                                                    
   
Datum
macaddr8_set7bit(PG_FUNCTION_ARGS)
{
  macaddr8 *addr = PG_GETARG_MACADDR8_P(0);
  macaddr8 *result;

  result = (macaddr8 *)palloc0(sizeof(macaddr8));

  result->a = addr->a | 0x02;
  result->b = addr->b;
  result->c = addr->c;
  result->d = addr->d;
  result->e = addr->e;
  result->f = addr->f;
  result->g = addr->g;
  result->h = addr->h;

  PG_RETURN_MACADDR8_P(result);
}

                                                             
                         
                                                             

Datum
macaddrtomacaddr8(PG_FUNCTION_ARGS)
{
  macaddr *addr6 = PG_GETARG_MACADDR_P(0);
  macaddr8 *result;

  result = (macaddr8 *)palloc0(sizeof(macaddr8));

  result->a = addr6->a;
  result->b = addr6->b;
  result->c = addr6->c;
  result->d = 0xFF;
  result->e = 0xFE;
  result->f = addr6->d;
  result->g = addr6->e;
  result->h = addr6->f;

  PG_RETURN_MACADDR8_P(result);
}

Datum
macaddr8tomacaddr(PG_FUNCTION_ARGS)
{
  macaddr8 *addr = PG_GETARG_MACADDR8_P(0);
  macaddr *result;

  result = (macaddr *)palloc0(sizeof(macaddr));

  if ((addr->d != 0xFF) || (addr->e != 0xFE))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("macaddr8 data out of range to convert to macaddr"),
                       errhint("Only addresses that have FF and FE as values in the "
                               "4th and 5th bytes from the left, for example "
                               "xx:xx:xx:ff:fe:xx:xx:xx, are eligible to be converted "
                               "from macaddr8 to macaddr.")));
  }

  result->a = addr->a;
  result->b = addr->b;
  result->c = addr->c;
  result->d = addr->f;
  result->e = addr->g;
  result->f = addr->h;

  PG_RETURN_MACADDR_P(result);
}
