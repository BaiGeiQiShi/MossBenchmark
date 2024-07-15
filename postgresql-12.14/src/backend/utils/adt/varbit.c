                                                                            
   
            
                                                              
   
                                                       
                                                                 
                                                                
                                                                       
                                                     
                                                            
   
                                                                     
                                                                     
                                                                     
                        
   
                                                                       
                                    
   
                                                   
   
                                                                         
                                                                        
   
                  
                                    
   
                                                                            
   

#include "postgres.h"

#include "access/htup_details.h"
#include "common/int.h"
#include "libpq/pqformat.h"
#include "nodes/nodeFuncs.h"
#include "nodes/supportnodes.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/varbit.h"

#define HEXDIG(z) ((z) < 10 ? ((z) + '0') : ((z)-10 + 'A'))

                                                                           
#define VARBIT_PAD(vb)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    int32 pad_ = VARBITPAD(vb);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    Assert(pad_ >= 0 && pad_ < BITS_PER_BYTE);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    if (pad_ > 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      *(VARBITS(vb) + VARBITBYTES(vb) - 1) &= BITMASK << pad_;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  } while (0)

   
                                                                         
                                                   
   
#define VARBIT_PAD_LAST(vb, ptr)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    int32 pad_ = VARBITPAD(vb);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    Assert(pad_ >= 0 && pad_ < BITS_PER_BYTE);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    if (pad_ > 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      *((ptr)-1) &= BITMASK << pad_;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  } while (0)

                                          
#ifdef USE_ASSERT_CHECKING
#define VARBIT_CORRECTLY_PADDED(vb)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    int32 pad_ = VARBITPAD(vb);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    Assert(pad_ >= 0 && pad_ < BITS_PER_BYTE);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    Assert(pad_ == 0 || (*(VARBITS(vb) + VARBITBYTES(vb) - 1) & ~(BITMASK << pad_)) == 0);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  } while (0)
#else
#define VARBIT_CORRECTLY_PADDED(vb) ((void)0)
#endif

static VarBit *
bit_catenate(VarBit *arg1, VarBit *arg2);
static VarBit *
bitsubstring(VarBit *arg, int32 s, int32 l, bool length_not_specified);
static VarBit *
bit_overlay(VarBit *t1, VarBit *t2, int sp, int sl);

   
                                                  
   
static int32
anybit_typmodin(ArrayType *ta, const char *typename)
{
  int32 typmod;
  int32 *tl;
  int n;

  tl = ArrayGetIntegerTypmods(ta, &n);

     
                                                                       
                                                       
     
  if (n != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid type modifier")));
  }

  if (*tl < 1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("length for type %s must be at least 1", typename)));
  }
  if (*tl > (MaxAttrSize * BITS_PER_BYTE))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("length for type %s cannot exceed %d", typename, MaxAttrSize * BITS_PER_BYTE)));
  }

  typmod = *tl;

  return typmod;
}

   
                                                    
   
static char *
anybit_typmodout(int32 typmod)
{
  char *res = (char *)palloc(64);

  if (typmod >= 0)
  {
    snprintf(res, 64, "(%d)", typmod);
  }
  else
  {
    *res = '\0';
  }

  return res;
}

   
            
                                                                           
                                                                   
                                        
   
Datum
bit_in(PG_FUNCTION_ARGS)
{
  char *input_string = PG_GETARG_CSTRING(0);

#ifdef NOT_USED
  Oid typelem = PG_GETARG_OID(1);
#endif
  int32 atttypmod = PG_GETARG_INT32(2);
  VarBit *result;                                     
  char *sp;                                                 
  bits8 *r;                                      
  int len,                                                  
      bitlen,                                               
      slen;                                            
  bool bit_not_hex;                                            
  int bc;
  bits8 x = 0;

                                                     
  if (input_string[0] == 'b' || input_string[0] == 'B')
  {
    bit_not_hex = true;
    sp = input_string + 1;
  }
  else if (input_string[0] == 'x' || input_string[0] == 'X')
  {
    bit_not_hex = false;
    sp = input_string + 1;
  }
  else
  {
       
                                                                           
                              
       
    bit_not_hex = true;
    sp = input_string;
  }

     
                                                                            
                                                         
     
  slen = strlen(sp);
  if (bit_not_hex)
  {
    bitlen = slen;
  }
  else
  {
    if (slen > VARBITMAXLEN / 4)
    {
      ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("bit string length exceeds the maximum allowed (%d)", VARBITMAXLEN)));
    }
    bitlen = slen * 4;
  }

     
                                                                            
                                   
     
  if (atttypmod <= 0)
  {
    atttypmod = bitlen;
  }
  else if (bitlen != atttypmod)
  {
    ereport(ERROR, (errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH), errmsg("bit string length %d does not match type bit(%d)", bitlen, atttypmod)));
  }

  len = VARBITTOTALLEN(atttypmod);
                                                                           
  result = (VarBit *)palloc0(len);
  SET_VARSIZE(result, len);
  VARBITLEN(result) = atttypmod;

  r = VARBITS(result);
  if (bit_not_hex)
  {
                                                    
                                                              
    x = HIGHBIT;
    for (; *sp; sp++)
    {
      if (*sp == '1')
      {
        *r |= x;
      }
      else if (*sp != '0')
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("\"%c\" is not a valid binary digit", *sp)));
      }

      x >>= 1;
      if (x == 0)
      {
        x = HIGHBIT;
        r++;
      }
    }
  }
  else
  {
                                                    
    for (bc = 0; *sp; sp++)
    {
      if (*sp >= '0' && *sp <= '9')
      {
        x = (bits8)(*sp - '0');
      }
      else if (*sp >= 'A' && *sp <= 'F')
      {
        x = (bits8)(*sp - 'A') + 10;
      }
      else if (*sp >= 'a' && *sp <= 'f')
      {
        x = (bits8)(*sp - 'a') + 10;
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("\"%c\" is not a valid hexadecimal digit", *sp)));
      }

      if (bc)
      {
        *r++ |= x;
        bc = 0;
      }
      else
      {
        *r = x << 4;
        bc = 1;
      }
    }
  }

  PG_RETURN_VARBIT_P(result);
}

Datum
bit_out(PG_FUNCTION_ARGS)
{
#if 1
                             
  return varbit_out(fcinfo);
#else

     
                                                                        
                                  
     
  VarBit *s = PG_GETARG_VARBIT_P(0);
  char *result, *r;
  bits8 *sp;
  int i, len, bitlen;

                                                                          
  VARBIT_CORRECTLY_PADDED(s);

  bitlen = VARBITLEN(s);
  len = (bitlen + 3) / 4;
  result = (char *)palloc(len + 2);
  sp = VARBITS(s);
  r = result;
  *r++ = 'X';
                                                                
  for (i = 0; i < len; i += 2, sp++)
  {
    *r++ = HEXDIG((*sp) >> 4);
    *r++ = HEXDIG((*sp) & 0xF);
  }

     
                                                                          
                       
     
  if (i > len)
  {
    r--;
  }
  *r = '\0';

  PG_RETURN_CSTRING(result);
#endif
}

   
                                                        
   
Datum
bit_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);

#ifdef NOT_USED
  Oid typelem = PG_GETARG_OID(1);
#endif
  int32 atttypmod = PG_GETARG_INT32(2);
  VarBit *result;
  int len, bitlen;

  bitlen = pq_getmsgint(buf, sizeof(int32));
  if (bitlen < 0 || bitlen > VARBITMAXLEN)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("invalid length in external bit string")));
  }

     
                                                                            
                                   
     
  if (atttypmod > 0 && bitlen != atttypmod)
  {
    ereport(ERROR, (errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH), errmsg("bit string length %d does not match type bit(%d)", bitlen, atttypmod)));
  }

  len = VARBITTOTALLEN(bitlen);
  result = (VarBit *)palloc(len);
  SET_VARSIZE(result, len);
  VARBITLEN(result) = bitlen;

  pq_copymsgbytes(buf, (char *)VARBITS(result), VARBITBYTES(result));

                                                    
  VARBIT_PAD(result);

  PG_RETURN_VARBIT_P(result);
}

   
                                               
   
Datum
bit_send(PG_FUNCTION_ARGS)
{
                                                      
  return varbit_send(fcinfo);
}

   
         
                                                        
                                                            
   
                                                                         
                                                                              
   
Datum
bit(PG_FUNCTION_ARGS)
{
  VarBit *arg = PG_GETARG_VARBIT_P(0);
  int32 len = PG_GETARG_INT32(1);
  bool isExplicit = PG_GETARG_BOOL(2);
  VarBit *result;
  int rlen;

                                                                        
  if (len <= 0 || len > VARBITMAXLEN || len == VARBITLEN(arg))
  {
    PG_RETURN_VARBIT_P(arg);
  }

  if (!isExplicit)
  {
    ereport(ERROR, (errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH), errmsg("bit string length %d does not match type bit(%d)", VARBITLEN(arg), len)));
  }

  rlen = VARBITTOTALLEN(len);
                                              
  result = (VarBit *)palloc0(rlen);
  SET_VARSIZE(result, rlen);
  VARBITLEN(result) = len;

  memcpy(VARBITS(result), VARBITS(arg), Min(VARBITBYTES(result), VARBITBYTES(arg)));

     
                                                                             
                                                                            
                                                           
     
  VARBIT_PAD(result);

  PG_RETURN_VARBIT_P(result);
}

Datum
bittypmodin(PG_FUNCTION_ARGS)
{
  ArrayType *ta = PG_GETARG_ARRAYTYPE_P(0);

  PG_RETURN_INT32(anybit_typmodin(ta, "bit"));
}

Datum
bittypmodout(PG_FUNCTION_ARGS)
{
  int32 typmod = PG_GETARG_INT32(0);

  PG_RETURN_CSTRING(anybit_typmodout(typmod));
}

   
               
                                                                      
                                                                 
                                                                        
   
Datum
varbit_in(PG_FUNCTION_ARGS)
{
  char *input_string = PG_GETARG_CSTRING(0);

#ifdef NOT_USED
  Oid typelem = PG_GETARG_OID(1);
#endif
  int32 atttypmod = PG_GETARG_INT32(2);
  VarBit *result;                                     
  char *sp;                                                 
  bits8 *r;                                      
  int len,                                                  
      bitlen,                                               
      slen;                                            
  bool bit_not_hex;                                            
  int bc;
  bits8 x = 0;

                                                     
  if (input_string[0] == 'b' || input_string[0] == 'B')
  {
    bit_not_hex = true;
    sp = input_string + 1;
  }
  else if (input_string[0] == 'x' || input_string[0] == 'X')
  {
    bit_not_hex = false;
    sp = input_string + 1;
  }
  else
  {
    bit_not_hex = true;
    sp = input_string;
  }

     
                                                                            
                                                         
     
  slen = strlen(sp);
  if (bit_not_hex)
  {
    bitlen = slen;
  }
  else
  {
    if (slen > VARBITMAXLEN / 4)
    {
      ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("bit string length exceeds the maximum allowed (%d)", VARBITMAXLEN)));
    }
    bitlen = slen * 4;
  }

     
                                                                            
                                   
     
  if (atttypmod <= 0)
  {
    atttypmod = bitlen;
  }
  else if (bitlen > atttypmod)
  {
    ereport(ERROR, (errcode(ERRCODE_STRING_DATA_RIGHT_TRUNCATION), errmsg("bit string too long for type bit varying(%d)", atttypmod)));
  }

  len = VARBITTOTALLEN(bitlen);
                                                                           
  result = (VarBit *)palloc0(len);
  SET_VARSIZE(result, len);
  VARBITLEN(result) = Min(bitlen, atttypmod);

  r = VARBITS(result);
  if (bit_not_hex)
  {
                                                    
                                                              
    x = HIGHBIT;
    for (; *sp; sp++)
    {
      if (*sp == '1')
      {
        *r |= x;
      }
      else if (*sp != '0')
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("\"%c\" is not a valid binary digit", *sp)));
      }

      x >>= 1;
      if (x == 0)
      {
        x = HIGHBIT;
        r++;
      }
    }
  }
  else
  {
                                                    
    for (bc = 0; *sp; sp++)
    {
      if (*sp >= '0' && *sp <= '9')
      {
        x = (bits8)(*sp - '0');
      }
      else if (*sp >= 'A' && *sp <= 'F')
      {
        x = (bits8)(*sp - 'A') + 10;
      }
      else if (*sp >= 'a' && *sp <= 'f')
      {
        x = (bits8)(*sp - 'a') + 10;
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("\"%c\" is not a valid hexadecimal digit", *sp)));
      }

      if (bc)
      {
        *r++ |= x;
        bc = 0;
      }
      else
      {
        *r = x << 4;
        bc = 1;
      }
    }
  }

  PG_RETURN_VARBIT_P(result);
}

   
                
                                                             
   
                                                                             
                                                            
   
Datum
varbit_out(PG_FUNCTION_ARGS)
{
  VarBit *s = PG_GETARG_VARBIT_P(0);
  char *result, *r;
  bits8 *sp;
  bits8 x;
  int i, k, len;

                                                                          
  VARBIT_CORRECTLY_PADDED(s);

  len = VARBITLEN(s);
  result = (char *)palloc(len + 1);
  sp = VARBITS(s);
  r = result;
  for (i = 0; i <= len - BITS_PER_BYTE; i += BITS_PER_BYTE, sp++)
  {
                          
    x = *sp;
    for (k = 0; k < BITS_PER_BYTE; k++)
    {
      *r++ = IS_HIGHBIT_SET(x) ? '1' : '0';
      x <<= 1;
    }
  }
  if (i < len)
  {
                                     
    x = *sp;
    for (k = i; k < len; k++)
    {
      *r++ = IS_HIGHBIT_SET(x) ? '1' : '0';
      x <<= 1;
    }
  }
  *r = '\0';

  PG_RETURN_CSTRING(result);
}

   
                                                              
   
                                                                   
   
Datum
varbit_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);

#ifdef NOT_USED
  Oid typelem = PG_GETARG_OID(1);
#endif
  int32 atttypmod = PG_GETARG_INT32(2);
  VarBit *result;
  int len, bitlen;

  bitlen = pq_getmsgint(buf, sizeof(int32));
  if (bitlen < 0 || bitlen > VARBITMAXLEN)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("invalid length in external bit string")));
  }

     
                                                                            
                                   
     
  if (atttypmod > 0 && bitlen > atttypmod)
  {
    ereport(ERROR, (errcode(ERRCODE_STRING_DATA_RIGHT_TRUNCATION), errmsg("bit string too long for type bit varying(%d)", atttypmod)));
  }

  len = VARBITTOTALLEN(bitlen);
  result = (VarBit *)palloc(len);
  SET_VARSIZE(result, len);
  VARBITLEN(result) = bitlen;

  pq_copymsgbytes(buf, (char *)VARBITS(result), VARBITBYTES(result));

                                                    
  VARBIT_PAD(result);

  PG_RETURN_VARBIT_P(result);
}

   
                                                     
   
Datum
varbit_send(PG_FUNCTION_ARGS)
{
  VarBit *s = PG_GETARG_VARBIT_P(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendint32(&buf, VARBITLEN(s));
  pq_sendbytes(&buf, (char *)VARBITS(s), VARBITBYTES(s));
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

   
                    
   
                                                                       
   
                                                                             
                                                                             
                                                                  
   
Datum
varbit_support(PG_FUNCTION_ARGS)
{
  Node *rawreq = (Node *)PG_GETARG_POINTER(0);
  Node *ret = NULL;

  if (IsA(rawreq, SupportRequestSimplify))
  {
    SupportRequestSimplify *req = (SupportRequestSimplify *)rawreq;
    FuncExpr *expr = req->fcall;
    Node *typmod;

    Assert(list_length(expr->args) >= 2);

    typmod = (Node *)lsecond(expr->args);

    if (IsA(typmod, Const) && !((Const *)typmod)->constisnull)
    {
      Node *source = (Node *)linitial(expr->args);
      int32 new_typmod = DatumGetInt32(((Const *)typmod)->constvalue);
      int32 old_max = exprTypmod(source);
      int32 new_max = new_typmod;

                                                                   
      if (new_max <= 0 || (old_max > 0 && old_max <= new_max))
      {
        ret = relabel_to_typmod(source, new_typmod);
      }
    }
  }

  PG_RETURN_POINTER(ret);
}

   
            
                                                           
                                                                    
   
                                                                     
                                                            
   
Datum
varbit(PG_FUNCTION_ARGS)
{
  VarBit *arg = PG_GETARG_VARBIT_P(0);
  int32 len = PG_GETARG_INT32(1);
  bool isExplicit = PG_GETARG_BOOL(2);
  VarBit *result;
  int rlen;

                                                                        
  if (len <= 0 || len >= VARBITLEN(arg))
  {
    PG_RETURN_VARBIT_P(arg);
  }

  if (!isExplicit)
  {
    ereport(ERROR, (errcode(ERRCODE_STRING_DATA_RIGHT_TRUNCATION), errmsg("bit string too long for type bit varying(%d)", len)));
  }

  rlen = VARBITTOTALLEN(len);
  result = (VarBit *)palloc(rlen);
  SET_VARSIZE(result, rlen);
  VARBITLEN(result) = len;

  memcpy(VARBITS(result), VARBITS(arg), VARBITBYTES(result));

                                                    
  VARBIT_PAD(result);

  PG_RETURN_VARBIT_P(result);
}

Datum
varbittypmodin(PG_FUNCTION_ARGS)
{
  ArrayType *ta = PG_GETARG_ARRAYTYPE_P(0);

  PG_RETURN_INT32(anybit_typmodin(ta, "varbit"));
}

Datum
varbittypmodout(PG_FUNCTION_ARGS)
{
  int32 typmod = PG_GETARG_INT32(0);

  PG_RETURN_CSTRING(anybit_typmodout(typmod));
}

   
                        
   
                                                                               
                                                                       
   
                                                                          
                                                                                
                                                                          
                                                           
   
                                                                             
                                                       
   
                                                                          
                                                                           
                          
   

   
           
   
                                                                                
                                                                                
                                                                               
                                                       
   
static int32
bit_cmp(VarBit *arg1, VarBit *arg2)
{
  int bitlen1, bytelen1, bitlen2, bytelen2;
  int32 cmp;

  bytelen1 = VARBITBYTES(arg1);
  bytelen2 = VARBITBYTES(arg2);

  cmp = memcmp(VARBITS(arg1), VARBITS(arg2), Min(bytelen1, bytelen2));
  if (cmp == 0)
  {
    bitlen1 = VARBITLEN(arg1);
    bitlen2 = VARBITLEN(arg2);
    if (bitlen1 != bitlen2)
    {
      cmp = (bitlen1 < bitlen2) ? -1 : 1;
    }
  }
  return cmp;
}

Datum
biteq(PG_FUNCTION_ARGS)
{
  VarBit *arg1 = PG_GETARG_VARBIT_P(0);
  VarBit *arg2 = PG_GETARG_VARBIT_P(1);
  bool result;
  int bitlen1, bitlen2;

  bitlen1 = VARBITLEN(arg1);
  bitlen2 = VARBITLEN(arg2);

                                             
  if (bitlen1 != bitlen2)
  {
    result = false;
  }
  else
  {
    result = (bit_cmp(arg1, arg2) == 0);
  }

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(result);
}

Datum
bitne(PG_FUNCTION_ARGS)
{
  VarBit *arg1 = PG_GETARG_VARBIT_P(0);
  VarBit *arg2 = PG_GETARG_VARBIT_P(1);
  bool result;
  int bitlen1, bitlen2;

  bitlen1 = VARBITLEN(arg1);
  bitlen2 = VARBITLEN(arg2);

                                             
  if (bitlen1 != bitlen2)
  {
    result = true;
  }
  else
  {
    result = (bit_cmp(arg1, arg2) != 0);
  }

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(result);
}

Datum
bitlt(PG_FUNCTION_ARGS)
{
  VarBit *arg1 = PG_GETARG_VARBIT_P(0);
  VarBit *arg2 = PG_GETARG_VARBIT_P(1);
  bool result;

  result = (bit_cmp(arg1, arg2) < 0);

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(result);
}

Datum
bitle(PG_FUNCTION_ARGS)
{
  VarBit *arg1 = PG_GETARG_VARBIT_P(0);
  VarBit *arg2 = PG_GETARG_VARBIT_P(1);
  bool result;

  result = (bit_cmp(arg1, arg2) <= 0);

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(result);
}

Datum
bitgt(PG_FUNCTION_ARGS)
{
  VarBit *arg1 = PG_GETARG_VARBIT_P(0);
  VarBit *arg2 = PG_GETARG_VARBIT_P(1);
  bool result;

  result = (bit_cmp(arg1, arg2) > 0);

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(result);
}

Datum
bitge(PG_FUNCTION_ARGS)
{
  VarBit *arg1 = PG_GETARG_VARBIT_P(0);
  VarBit *arg2 = PG_GETARG_VARBIT_P(1);
  bool result;

  result = (bit_cmp(arg1, arg2) >= 0);

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(result);
}

Datum
bitcmp(PG_FUNCTION_ARGS)
{
  VarBit *arg1 = PG_GETARG_VARBIT_P(0);
  VarBit *arg2 = PG_GETARG_VARBIT_P(1);
  int32 result;

  result = bit_cmp(arg1, arg2);

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_INT32(result);
}

   
          
                                
   
Datum
bitcat(PG_FUNCTION_ARGS)
{
  VarBit *arg1 = PG_GETARG_VARBIT_P(0);
  VarBit *arg2 = PG_GETARG_VARBIT_P(1);

  PG_RETURN_VARBIT_P(bit_catenate(arg1, arg2));
}

static VarBit *
bit_catenate(VarBit *arg1, VarBit *arg2)
{
  VarBit *result;
  int bitlen1, bitlen2, bytelen, bit1pad, bit2shift;
  bits8 *pr, *pa;

  bitlen1 = VARBITLEN(arg1);
  bitlen2 = VARBITLEN(arg2);

  if (bitlen1 > VARBITMAXLEN - bitlen2)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("bit string length exceeds the maximum allowed (%d)", VARBITMAXLEN)));
  }
  bytelen = VARBITTOTALLEN(bitlen1 + bitlen2);

  result = (VarBit *)palloc(bytelen);
  SET_VARSIZE(result, bytelen);
  VARBITLEN(result) = bitlen1 + bitlen2;

                                   
  memcpy(VARBITS(result), VARBITS(arg1), VARBITBYTES(arg1));

                                  
  bit1pad = VARBITPAD(arg1);
  if (bit1pad == 0)
  {
    memcpy(VARBITS(result) + VARBITBYTES(arg1), VARBITS(arg2), VARBITBYTES(arg2));
  }
  else if (bitlen2 > 0)
  {
                                              
    bit2shift = BITS_PER_BYTE - bit1pad;
    pr = VARBITS(result) + VARBITBYTES(arg1) - 1;
    for (pa = VARBITS(arg2); pa < VARBITEND(arg2); pa++)
    {
      *pr |= ((*pa >> bit2shift) & BITMASK);
      pr++;
      if (pr < VARBITEND(result))
      {
        *pr = (*pa << bit1pad) & BITMASK;
      }
    }
  }

                                                         

  return result;
}

   
             
                                             
                       
                     
   
Datum
bitsubstr(PG_FUNCTION_ARGS)
{
  PG_RETURN_VARBIT_P(bitsubstring(PG_GETARG_VARBIT_P(0), PG_GETARG_INT32(1), PG_GETARG_INT32(2), false));
}

Datum
bitsubstr_no_len(PG_FUNCTION_ARGS)
{
  PG_RETURN_VARBIT_P(bitsubstring(PG_GETARG_VARBIT_P(0), PG_GETARG_INT32(1), -1, true));
}

static VarBit *
bitsubstring(VarBit *arg, int32 s, int32 l, bool length_not_specified)
{
  VarBit *result;
  int bitlen, rbitlen, len, ishift, i;
  int32 e, s1, e1;
  bits8 *r, *ps;

  bitlen = VARBITLEN(arg);
  s1 = Max(s, 1);
                                                           
  if (length_not_specified)
  {
    e1 = bitlen + 1;
  }
  else if (l < 0)
  {
                                                                       
    ereport(ERROR, (errcode(ERRCODE_SUBSTRING_ERROR), errmsg("negative substring length not allowed")));
    e1 = -1;                                 
  }
  else if (pg_add_s32_overflow(s, l, &e))
  {
       
                                                                        
                                            
       
    e1 = bitlen + 1;
  }
  else
  {
    e1 = Min(e, bitlen + 1);
  }
  if (s1 > bitlen || e1 <= s1)
  {
                                                
    len = VARBITTOTALLEN(0);
    result = (VarBit *)palloc(len);
    SET_VARSIZE(result, len);
    VARBITLEN(result) = 0;
  }
  else
  {
       
                                                                           
                        
       
    rbitlen = e1 - s1;
    len = VARBITTOTALLEN(rbitlen);
    result = (VarBit *)palloc(len);
    SET_VARSIZE(result, len);
    VARBITLEN(result) = rbitlen;
    len -= VARHDRSZ + VARBITHDRSZ;
                                              
    if ((s1 - 1) % BITS_PER_BYTE == 0)
    {
                                     
      memcpy(VARBITS(result), VARBITS(arg) + (s1 - 1) / BITS_PER_BYTE, len);
    }
    else
    {
                                                                
      ishift = (s1 - 1) % BITS_PER_BYTE;
      r = VARBITS(result);
      ps = VARBITS(arg) + (s1 - 1) / BITS_PER_BYTE;
      for (i = 0; i < len; i++)
      {
        *r = (*ps << ishift) & BITMASK;
        if ((++ps) < VARBITEND(arg))
        {
          *r |= *ps >> (BITS_PER_BYTE - ishift);
        }
        r++;
      }
    }

                                                      
    VARBIT_PAD(result);
  }

  return result;
}

   
              
                                                           
   
                                                                               
                                                                   
   
Datum
bitoverlay(PG_FUNCTION_ARGS)
{
  VarBit *t1 = PG_GETARG_VARBIT_P(0);
  VarBit *t2 = PG_GETARG_VARBIT_P(1);
  int sp = PG_GETARG_INT32(2);                               
  int sl = PG_GETARG_INT32(3);                       

  PG_RETURN_VARBIT_P(bit_overlay(t1, t2, sp, sl));
}

Datum
bitoverlay_no_len(PG_FUNCTION_ARGS)
{
  VarBit *t1 = PG_GETARG_VARBIT_P(0);
  VarBit *t2 = PG_GETARG_VARBIT_P(1);
  int sp = PG_GETARG_INT32(2);                               
  int sl;

  sl = VARBITLEN(t2);                             
  PG_RETURN_VARBIT_P(bit_overlay(t1, t2, sp, sl));
}

static VarBit *
bit_overlay(VarBit *t1, VarBit *t2, int sp, int sl)
{
  VarBit *result;
  VarBit *s1;
  VarBit *s2;
  int sp_pl_sl;

     
                                                                          
                                                                     
                                                      
     
  if (sp <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_SUBSTRING_ERROR), errmsg("negative substring length not allowed")));
  }
  if (pg_add_s32_overflow(sp, sl, &sp_pl_sl))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
  }

  s1 = bitsubstring(t1, 1, sp - 1, false);
  s2 = bitsubstring(t1, sp_pl_sl, -1, true);
  result = bit_catenate(s1, t2);
  result = bit_catenate(result, s2);

  return result;
}

   
                             
                                     
   
Datum
bitlength(PG_FUNCTION_ARGS)
{
  VarBit *arg = PG_GETARG_VARBIT_P(0);

  PG_RETURN_INT32(VARBITLEN(arg));
}

Datum
bitoctetlength(PG_FUNCTION_ARGS)
{
  VarBit *arg = PG_GETARG_VARBIT_P(0);

  PG_RETURN_INT32(VARBITBYTES(arg));
}

   
           
                                             
   
Datum
bit_and(PG_FUNCTION_ARGS)
{
  VarBit *arg1 = PG_GETARG_VARBIT_P(0);
  VarBit *arg2 = PG_GETARG_VARBIT_P(1);
  VarBit *result;
  int len, bitlen1, bitlen2, i;
  bits8 *p1, *p2, *r;

  bitlen1 = VARBITLEN(arg1);
  bitlen2 = VARBITLEN(arg2);
  if (bitlen1 != bitlen2)
  {
    ereport(ERROR, (errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH), errmsg("cannot AND bit strings of different sizes")));
  }

  len = VARSIZE(arg1);
  result = (VarBit *)palloc(len);
  SET_VARSIZE(result, len);
  VARBITLEN(result) = bitlen1;

  p1 = VARBITS(arg1);
  p2 = VARBITS(arg2);
  r = VARBITS(result);
  for (i = 0; i < VARBITBYTES(arg1); i++)
  {
    *r++ = *p1++ & *p2++;
  }

                                                 

  PG_RETURN_VARBIT_P(result);
}

   
          
                                            
   
Datum
bit_or(PG_FUNCTION_ARGS)
{
  VarBit *arg1 = PG_GETARG_VARBIT_P(0);
  VarBit *arg2 = PG_GETARG_VARBIT_P(1);
  VarBit *result;
  int len, bitlen1, bitlen2, i;
  bits8 *p1, *p2, *r;

  bitlen1 = VARBITLEN(arg1);
  bitlen2 = VARBITLEN(arg2);
  if (bitlen1 != bitlen2)
  {
    ereport(ERROR, (errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH), errmsg("cannot OR bit strings of different sizes")));
  }
  len = VARSIZE(arg1);
  result = (VarBit *)palloc(len);
  SET_VARSIZE(result, len);
  VARBITLEN(result) = bitlen1;

  p1 = VARBITS(arg1);
  p2 = VARBITS(arg2);
  r = VARBITS(result);
  for (i = 0; i < VARBITBYTES(arg1); i++)
  {
    *r++ = *p1++ | *p2++;
  }

                                                 

  PG_RETURN_VARBIT_P(result);
}

   
          
                                             
   
Datum
bitxor(PG_FUNCTION_ARGS)
{
  VarBit *arg1 = PG_GETARG_VARBIT_P(0);
  VarBit *arg2 = PG_GETARG_VARBIT_P(1);
  VarBit *result;
  int len, bitlen1, bitlen2, i;
  bits8 *p1, *p2, *r;

  bitlen1 = VARBITLEN(arg1);
  bitlen2 = VARBITLEN(arg2);
  if (bitlen1 != bitlen2)
  {
    ereport(ERROR, (errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH), errmsg("cannot XOR bit strings of different sizes")));
  }

  len = VARSIZE(arg1);
  result = (VarBit *)palloc(len);
  SET_VARSIZE(result, len);
  VARBITLEN(result) = bitlen1;

  p1 = VARBITS(arg1);
  p2 = VARBITS(arg2);
  r = VARBITS(result);
  for (i = 0; i < VARBITBYTES(arg1); i++)
  {
    *r++ = *p1++ ^ *p2++;
  }

                                                 

  PG_RETURN_VARBIT_P(result);
}

   
          
                                          
   
Datum
bitnot(PG_FUNCTION_ARGS)
{
  VarBit *arg = PG_GETARG_VARBIT_P(0);
  VarBit *result;
  bits8 *p, *r;

  result = (VarBit *)palloc(VARSIZE(arg));
  SET_VARSIZE(result, VARSIZE(arg));
  VARBITLEN(result) = VARBITLEN(arg);

  p = VARBITS(arg);
  r = VARBITS(result);
  for (; p < VARBITEND(arg); p++)
  {
    *r++ = ~*p;
  }

                                                                        
  VARBIT_PAD_LAST(result, r);

  PG_RETURN_VARBIT_P(result);
}

   
                
                                                              
   
Datum
bitshiftleft(PG_FUNCTION_ARGS)
{
  VarBit *arg = PG_GETARG_VARBIT_P(0);
  int32 shft = PG_GETARG_INT32(1);
  VarBit *result;
  int byte_shift, ishift, len;
  bits8 *p, *r;

                                              
  if (shft < 0)
  {
                                              
    if (shft < -VARBITMAXLEN)
    {
      shft = -VARBITMAXLEN;
    }
    PG_RETURN_DATUM(DirectFunctionCall2(bitshiftright, VarBitPGetDatum(arg), Int32GetDatum(-shft)));
  }

  result = (VarBit *)palloc(VARSIZE(arg));
  SET_VARSIZE(result, VARSIZE(arg));
  VARBITLEN(result) = VARBITLEN(arg);
  r = VARBITS(result);

                                                                 
  if (shft >= VARBITLEN(arg))
  {
    MemSet(r, 0, VARBITBYTES(arg));
    PG_RETURN_VARBIT_P(result);
  }

  byte_shift = shft / BITS_PER_BYTE;
  ishift = shft % BITS_PER_BYTE;
  p = VARBITS(arg) + byte_shift;

  if (ishift == 0)
  {
                                          
    len = VARBITBYTES(arg) - byte_shift;
    memcpy(r, p, len);
    MemSet(r + len, 0, byte_shift);
  }
  else
  {
    for (; p < VARBITEND(arg); r++)
    {
      *r = *p << ishift;
      if ((++p) < VARBITEND(arg))
      {
        *r |= *p >> (BITS_PER_BYTE - ishift);
      }
    }
    for (; r < VARBITEND(result); r++)
    {
      *r = 0;
    }
  }

                                                         

  PG_RETURN_VARBIT_P(result);
}

   
                 
                                                         
   
Datum
bitshiftright(PG_FUNCTION_ARGS)
{
  VarBit *arg = PG_GETARG_VARBIT_P(0);
  int32 shft = PG_GETARG_INT32(1);
  VarBit *result;
  int byte_shift, ishift, len;
  bits8 *p, *r;

                                             
  if (shft < 0)
  {
                                              
    if (shft < -VARBITMAXLEN)
    {
      shft = -VARBITMAXLEN;
    }
    PG_RETURN_DATUM(DirectFunctionCall2(bitshiftleft, VarBitPGetDatum(arg), Int32GetDatum(-shft)));
  }

  result = (VarBit *)palloc(VARSIZE(arg));
  SET_VARSIZE(result, VARSIZE(arg));
  VARBITLEN(result) = VARBITLEN(arg);
  r = VARBITS(result);

                                                                 
  if (shft >= VARBITLEN(arg))
  {
    MemSet(r, 0, VARBITBYTES(arg));
    PG_RETURN_VARBIT_P(result);
  }

  byte_shift = shft / BITS_PER_BYTE;
  ishift = shft % BITS_PER_BYTE;
  p = VARBITS(arg);

                                             
  MemSet(r, 0, byte_shift);
  r += byte_shift;

  if (ishift == 0)
  {
                                          
    len = VARBITBYTES(arg) - byte_shift;
    memcpy(r, p, len);
    r += len;
  }
  else
  {
    if (r < VARBITEND(result))
    {
      *r = 0;                            
    }
    for (; r < VARBITEND(result); p++)
    {
      *r |= *p >> ishift;
      if ((++r) < VARBITEND(result))
      {
        *r = (*p << (BITS_PER_BYTE - ishift)) & BITMASK;
      }
    }
  }

                                                              
  VARBIT_PAD_LAST(result, r);

  PG_RETURN_VARBIT_P(result);
}

   
                                                                          
                                               
   
Datum
bitfromint4(PG_FUNCTION_ARGS)
{
  int32 a = PG_GETARG_INT32(0);
  int32 typmod = PG_GETARG_INT32(1);
  VarBit *result;
  bits8 *r;
  int rlen;
  int destbitsleft, srcbitsleft;

  if (typmod <= 0 || typmod > VARBITMAXLEN)
  {
    typmod = 1;                         
  }

  rlen = VARBITTOTALLEN(typmod);
  result = (VarBit *)palloc(rlen);
  SET_VARSIZE(result, rlen);
  VARBITLEN(result) = typmod;

  r = VARBITS(result);
  destbitsleft = typmod;
  srcbitsleft = 32;
                                          
  srcbitsleft = Min(srcbitsleft, destbitsleft);
                                            
  while (destbitsleft >= srcbitsleft + 8)
  {
    *r++ = (bits8)((a < 0) ? BITMASK : 0);
    destbitsleft -= 8;
  }
                                   
  if (destbitsleft > srcbitsleft)
  {
    unsigned int val = (unsigned int)(a >> (destbitsleft - 8));

                                                                         
    if (a < 0)
    {
      val |= ((unsigned int)-1) << (srcbitsleft + 8 - destbitsleft);
    }
    *r++ = (bits8)(val & BITMASK);
    destbitsleft -= 8;
  }
                                                                          
                         
  while (destbitsleft >= 8)
  {
    *r++ = (bits8)((a >> (destbitsleft - 8)) & BITMASK);
    destbitsleft -= 8;
  }
                                  
  if (destbitsleft > 0)
  {
    *r = (bits8)((a << (8 - destbitsleft)) & BITMASK);
  }

  PG_RETURN_VARBIT_P(result);
}

Datum
bittoint4(PG_FUNCTION_ARGS)
{
  VarBit *arg = PG_GETARG_VARBIT_P(0);
  uint32 result;
  bits8 *r;

                                                 
  if (VARBITLEN(arg) > sizeof(result) * BITS_PER_BYTE)
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
  }

  result = 0;
  for (r = VARBITS(arg); r < VARBITEND(arg); r++)
  {
    result <<= BITS_PER_BYTE;
    result |= *r;
  }
                                                                      
  result >>= VARBITPAD(arg);

  PG_RETURN_INT32(result);
}

Datum
bitfromint8(PG_FUNCTION_ARGS)
{
  int64 a = PG_GETARG_INT64(0);
  int32 typmod = PG_GETARG_INT32(1);
  VarBit *result;
  bits8 *r;
  int rlen;
  int destbitsleft, srcbitsleft;

  if (typmod <= 0 || typmod > VARBITMAXLEN)
  {
    typmod = 1;                         
  }

  rlen = VARBITTOTALLEN(typmod);
  result = (VarBit *)palloc(rlen);
  SET_VARSIZE(result, rlen);
  VARBITLEN(result) = typmod;

  r = VARBITS(result);
  destbitsleft = typmod;
  srcbitsleft = 64;
                                          
  srcbitsleft = Min(srcbitsleft, destbitsleft);
                                            
  while (destbitsleft >= srcbitsleft + 8)
  {
    *r++ = (bits8)((a < 0) ? BITMASK : 0);
    destbitsleft -= 8;
  }
                                   
  if (destbitsleft > srcbitsleft)
  {
    unsigned int val = (unsigned int)(a >> (destbitsleft - 8));

                                                                         
    if (a < 0)
    {
      val |= ((unsigned int)-1) << (srcbitsleft + 8 - destbitsleft);
    }
    *r++ = (bits8)(val & BITMASK);
    destbitsleft -= 8;
  }
                                                                          
                         
  while (destbitsleft >= 8)
  {
    *r++ = (bits8)((a >> (destbitsleft - 8)) & BITMASK);
    destbitsleft -= 8;
  }
                                  
  if (destbitsleft > 0)
  {
    *r = (bits8)((a << (8 - destbitsleft)) & BITMASK);
  }

  PG_RETURN_VARBIT_P(result);
}

Datum
bittoint8(PG_FUNCTION_ARGS)
{
  VarBit *arg = PG_GETARG_VARBIT_P(0);
  uint64 result;
  bits8 *r;

                                                 
  if (VARBITLEN(arg) > sizeof(result) * BITS_PER_BYTE)
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
  }

  result = 0;
  for (r = VARBITS(arg); r < VARBITEND(arg); r++)
  {
    result <<= BITS_PER_BYTE;
    result |= *r;
  }
                                                                      
  result >>= VARBITPAD(arg);

  PG_RETURN_INT64(result);
}

   
                                                                       
                                                        
                                                 
                                                                       
   
Datum
bitposition(PG_FUNCTION_ARGS)
{
  VarBit *str = PG_GETARG_VARBIT_P(0);
  VarBit *substr = PG_GETARG_VARBIT_P(1);
  int substr_length, str_length, i, is;
  bits8 *s,                                 
      *p;                             
  bits8 cmp,                                           
      mask1,                                               
      mask2,                                              
      end_mask,                                       
      str_mask;                                    
  bool is_match;

                                
  substr_length = VARBITLEN(substr);
  str_length = VARBITLEN(str);

                                                                        
  if ((str_length == 0) || (substr_length > str_length))
  {
    PG_RETURN_INT32(0);
  }

                                            
  if (substr_length == 0)
  {
    PG_RETURN_INT32(1);
  }

                                    
  end_mask = BITMASK << VARBITPAD(substr);
  str_mask = BITMASK << VARBITPAD(str);
  for (i = 0; i < VARBITBYTES(str) - VARBITBYTES(substr) + 1; i++)
  {
    for (is = 0; is < BITS_PER_BYTE; is++)
    {
      is_match = true;
      p = VARBITS(str) + i;
      mask1 = BITMASK >> is;
      mask2 = ~mask1;
      for (s = VARBITS(substr); is_match && s < VARBITEND(substr); s++)
      {
        cmp = *s >> is;
        if (s == VARBITEND(substr) - 1)
        {
          mask1 &= end_mask >> is;
          if (p == VARBITEND(str) - 1)
          {
                                                        
            if (mask1 & ~str_mask)
            {
              is_match = false;
              break;
            }
            mask1 &= str_mask;
          }
        }
        is_match = ((cmp ^ *p) & mask1) == 0;
        if (!is_match)
        {
          break;
        }
                                      
        p++;
        if (p == VARBITEND(str))
        {
          mask2 = end_mask << (BITS_PER_BYTE - is);
          is_match = mask2 == 0;
#if 0
					elog(DEBUG4, "S. %d %d em=%2x sm=%2x r=%d",
						 i, is, end_mask, mask2, is_match);
#endif
          break;
        }
        cmp = *s << (BITS_PER_BYTE - is);
        if (s == VARBITEND(substr) - 1)
        {
          mask2 &= end_mask << (BITS_PER_BYTE - is);
          if (p == VARBITEND(str) - 1)
          {
            if (mask2 & ~str_mask)
            {
              is_match = false;
              break;
            }
            mask2 &= str_mask;
          }
        }
        is_match = ((cmp ^ *p) & mask2) == 0;
      }
                                  
      if (is_match)
      {
        PG_RETURN_INT32(i * BITS_PER_BYTE + is + 1);
      }
    }
  }
  PG_RETURN_INT32(0);
}

   
             
   
                                                          
                                       
   
                                                                       
                                                                
                                                                         
   
Datum
bitsetbit(PG_FUNCTION_ARGS)
{
  VarBit *arg1 = PG_GETARG_VARBIT_P(0);
  int32 n = PG_GETARG_INT32(1);
  int32 newBit = PG_GETARG_INT32(2);
  VarBit *result;
  int len, bitlen;
  bits8 *r, *p;
  int byteNo, bitNo;

  bitlen = VARBITLEN(arg1);
  if (n < 0 || n >= bitlen)
  {
    ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR), errmsg("bit index %d out of valid range (0..%d)", n, bitlen - 1)));
  }

     
                   
     
  if (newBit != 0 && newBit != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("new bit must be 0 or 1")));
  }

  len = VARSIZE(arg1);
  result = (VarBit *)palloc(len);
  SET_VARSIZE(result, len);
  VARBITLEN(result) = bitlen;

  p = VARBITS(arg1);
  r = VARBITS(result);

  memcpy(r, p, VARBITBYTES(arg1));

  byteNo = n / BITS_PER_BYTE;
  bitNo = BITS_PER_BYTE - 1 - (n % BITS_PER_BYTE);

     
                      
     
  if (newBit == 0)
  {
    r[byteNo] &= (~(1 << bitNo));
  }
  else
  {
    r[byteNo] |= (1 << bitNo);
  }

  PG_RETURN_VARBIT_P(result);
}

   
             
   
                                                             
   
                                                                       
                                                                
                                                                         
   
Datum
bitgetbit(PG_FUNCTION_ARGS)
{
  VarBit *arg1 = PG_GETARG_VARBIT_P(0);
  int32 n = PG_GETARG_INT32(1);
  int bitlen;
  bits8 *p;
  int byteNo, bitNo;

  bitlen = VARBITLEN(arg1);
  if (n < 0 || n >= bitlen)
  {
    ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR), errmsg("bit index %d out of valid range (0..%d)", n, bitlen - 1)));
  }

  p = VARBITS(arg1);

  byteNo = n / BITS_PER_BYTE;
  bitNo = BITS_PER_BYTE - 1 - (n % BITS_PER_BYTE);

  if (p[byteNo] & (1 << bitNo))
  {
    PG_RETURN_INT32(1);
  }
  else
  {
    PG_RETURN_INT32(0);
  }
}
