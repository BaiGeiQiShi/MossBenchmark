                                              

#include "postgres_fe.h"
#include <ctype.h>
#include <float.h>
#include <limits.h>

#include "pgtypeslib_extern.h"
#include "pgtypes_error.h"

#define Max(x, y) ((x) > (y) ? (x) : (y))
#define Min(x, y) ((x) < (y) ? (x) : (y))

#define init_var(v) memset(v, 0, sizeof(numeric))

#define digitbuf_alloc(size) ((NumericDigit *)pgtypes_alloc(size))
#define digitbuf_free(buf)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if ((buf) != NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
      free(buf);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  } while (0)

#include "pgtypes_numeric.h"

#if 0
              
                    
   
                                                               
                 
              
   
static int
apply_typmod(numeric *var, long typmod)
{
	int			precision;
	int			scale;
	int			maxweight;
	int			i;

	                                                 
	if (typmod < (long) (VARHDRSZ))
		return 0;

	typmod -= VARHDRSZ;
	precision = (typmod >> 16) & 0xffff;
	scale = typmod & 0xffff;
	maxweight = precision - scale;

	                           
	i = scale + var->weight + 1;
	if (i >= 0 && var->ndigits > i)
	{
		int			carry = (var->digits[i] > 4) ? 1 : 0;

		var->ndigits = i;

		while (carry)
		{
			carry += var->digits[--i];
			var->digits[i] = carry % 10;
			carry /= 10;
		}

		if (i < 0)
		{
			var->digits--;
			var->ndigits++;
			var->weight++;
		}
	}
	else
		var->ndigits = Max(0, Min(i, var->ndigits));

	   
                                                                        
                                                                            
                                                                         
                                                                          
                                                   
    
	if (var->weight >= maxweight)
	{
		                                                          
		int			tweight = var->weight;

		for (i = 0; i < var->ndigits; i++)
		{
			if (var->digits[i])
				break;
			tweight--;
		}

		if (tweight >= maxweight && i < var->ndigits)
		{
			errno = PGTYPES_NUM_OVERFLOW;
			return -1;
		}
	}

	var->rscale = scale;
	var->dscale = scale;
	return 0;
}
#endif

              
                 
   
                                                                                
              
   
static int
alloc_var(numeric *var, int ndigits)
{
  digitbuf_free(var->buf);
  var->buf = digitbuf_alloc(ndigits + 1);
  if (var->buf == NULL)
  {
    return -1;
  }
  var->buf[0] = 0;
  var->digits = var->buf + 1;
  var->ndigits = ndigits;
  return 0;
}

numeric *
PGTYPESnumeric_new(void)
{
  numeric *var;

  if ((var = (numeric *)pgtypes_alloc(sizeof(numeric))) == NULL)
  {
    return NULL;
  }

  if (alloc_var(var, 0) < 0)
  {
    free(var);
    return NULL;
  }

  return var;
}

decimal *
PGTYPESdecimal_new(void)
{
  decimal *var;

  if ((var = (decimal *)pgtypes_alloc(sizeof(decimal))) == NULL)
  {
    return NULL;
  }

  memset(var, 0, sizeof(decimal));

  return var;
}

              
                      
   
                                                     
              
   
static int
set_var_from_str(char *str, char **ptr, numeric *dest)
{
  bool have_dp = false;
  int i = 0;

  errno = 0;
  *ptr = str;
  while (*(*ptr))
  {
    if (!isspace((unsigned char)*(*ptr)))
    {
      break;
    }
    (*ptr)++;
  }

  if (pg_strncasecmp(*ptr, "NaN", 3) == 0)
  {
    *ptr += 3;
    dest->sign = NUMERIC_NAN;

                                           
    while (*(*ptr))
    {
      if (!isspace((unsigned char)*(*ptr)))
      {
        errno = PGTYPES_NUM_BAD_NUMERIC;
        return -1;
      }
      (*ptr)++;
    }

    return 0;
  }

  if (alloc_var(dest, strlen((*ptr))) < 0)
  {
    return -1;
  }
  dest->weight = -1;
  dest->dscale = 0;
  dest->sign = NUMERIC_POS;

  switch (*(*ptr))
  {
  case '+':
    dest->sign = NUMERIC_POS;
    (*ptr)++;
    break;

  case '-':
    dest->sign = NUMERIC_NEG;
    (*ptr)++;
    break;
  }

  if (*(*ptr) == '.')
  {
    have_dp = true;
    (*ptr)++;
  }

  if (!isdigit((unsigned char)*(*ptr)))
  {
    errno = PGTYPES_NUM_BAD_NUMERIC;
    return -1;
  }

  while (*(*ptr))
  {
    if (isdigit((unsigned char)*(*ptr)))
    {
      dest->digits[i++] = *(*ptr)++ - '0';
      if (!have_dp)
      {
        dest->weight++;
      }
      else
      {
        dest->dscale++;
      }
    }
    else if (*(*ptr) == '.')
    {
      if (have_dp)
      {
        errno = PGTYPES_NUM_BAD_NUMERIC;
        return -1;
      }
      have_dp = true;
      (*ptr)++;
    }
    else
    {
      break;
    }
  }
  dest->ndigits = i;

                               
  if (*(*ptr) == 'e' || *(*ptr) == 'E')
  {
    long exponent;
    char *endptr;

    (*ptr)++;
    exponent = strtol(*ptr, &endptr, 10);
    if (endptr == (*ptr))
    {
      errno = PGTYPES_NUM_BAD_NUMERIC;
      return -1;
    }
    (*ptr) = endptr;
    if (exponent >= INT_MAX / 2 || exponent <= -(INT_MAX / 2))
    {
      errno = PGTYPES_NUM_BAD_NUMERIC;
      return -1;
    }
    dest->weight += (int)exponent;
    dest->dscale -= (int)exponent;
    if (dest->dscale < 0)
    {
      dest->dscale = 0;
    }
  }

                                         
  while (*(*ptr))
  {
    if (!isspace((unsigned char)*(*ptr)))
    {
      errno = PGTYPES_NUM_BAD_NUMERIC;
      return -1;
    }
    (*ptr)++;
  }

                                
  while (dest->ndigits > 0 && *(dest->digits) == 0)
  {
    (dest->digits)++;
    (dest->weight)--;
    (dest->ndigits)--;
  }
  if (dest->ndigits == 0)
  {
    dest->weight = 0;
  }

  dest->rscale = dest->dscale;
  return 0;
}

              
                        
   
                                                               
                                                        
              
   
static char *
get_str_from_var(numeric *var, int dscale)
{
  char *str;
  char *cp;
  int i;
  int d;

  if (var->sign == NUMERIC_NAN)
  {
    str = (char *)pgtypes_alloc(4);
    if (str == NULL)
    {
      return NULL;
    }
    sprintf(str, "NaN");
    return str;
  }

     
                                                                    
     
  i = dscale + var->weight + 1;
  if (i >= 0 && var->ndigits > i)
  {
    int carry = (var->digits[i] > 4) ? 1 : 0;

    var->ndigits = i;

    while (carry)
    {
      carry += var->digits[--i];
      var->digits[i] = carry % 10;
      carry /= 10;
    }

    if (i < 0)
    {
      var->digits--;
      var->ndigits++;
      var->weight++;
    }
  }
  else
  {
    var->ndigits = Max(0, Min(i, var->ndigits));
  }

     
                                   
     
  if ((str = (char *)pgtypes_alloc(Max(0, dscale) + Max(0, var->weight) + 4)) == NULL)
  {
    return NULL;
  }
  cp = str;

     
                                       
     
  if (var->sign == NUMERIC_NEG)
  {
    *cp++ = '-';
  }

     
                                                
     
  i = Max(var->weight, 0);
  d = 0;

  while (i >= 0)
  {
    if (i <= var->weight && d < var->ndigits)
    {
      *cp++ = var->digits[d++] + '0';
    }
    else
    {
      *cp++ = '0';
    }
    i--;
  }

     
                                                                             
     
  if (dscale > 0)
  {
    *cp++ = '.';
    while (i >= -dscale)
    {
      if (i <= var->weight && d < var->ndigits)
      {
        *cp++ = var->digits[d++] + '0';
      }
      else
      {
        *cp++ = '0';
      }
      i--;
    }
  }

     
                                        
     
  *cp = '\0';
  return str;
}

numeric *
PGTYPESnumeric_from_asc(char *str, char **endptr)
{
  numeric *value = (numeric *)pgtypes_alloc(sizeof(numeric));
  int ret;

  char *realptr;
  char **ptr = (endptr != NULL) ? endptr : &realptr;

  if (!value)
  {
    return NULL;
  }

  ret = set_var_from_str(str, ptr, value);
  if (ret)
  {
    PGTYPESnumeric_free(value);
    return NULL;
  }

  return value;
}

char *
PGTYPESnumeric_to_asc(numeric *num, int dscale)
{
  numeric *numcopy = PGTYPESnumeric_new();
  char *s;

  if (numcopy == NULL)
  {
    return NULL;
  }

  if (PGTYPESnumeric_copy(num, numcopy) < 0)
  {
    PGTYPESnumeric_free(numcopy);
    return NULL;
  }

  if (dscale < 0)
  {
    dscale = num->dscale;
  }

                                                
  s = get_str_from_var(numcopy, dscale);
  PGTYPESnumeric_free(numcopy);
  return s;
}

              
                
   
                           
                                            
              
   
static void
zero_var(numeric *var)
{
  digitbuf_free(var->buf);
  var->buf = NULL;
  var->digits = NULL;
  var->ndigits = 0;
  var->weight = 0;                                                   
  var->sign = NUMERIC_POS;                          
}

void
PGTYPESnumeric_free(numeric *var)
{
  digitbuf_free(var->buf);
  free(var);
}

void
PGTYPESdecimal_free(decimal *var)
{
  free(var);
}

              
               
   
                                                
                                         
                                    
                                   
              
   
static int
cmp_abs(numeric *var1, numeric *var2)
{
  int i1 = 0;
  int i2 = 0;
  int w1 = var1->weight;
  int w2 = var2->weight;
  int stat;

  while (w1 > w2 && i1 < var1->ndigits)
  {
    if (var1->digits[i1++] != 0)
    {
      return 1;
    }
    w1--;
  }
  while (w2 > w1 && i2 < var2->ndigits)
  {
    if (var2->digits[i2++] != 0)
    {
      return -1;
    }
    w2--;
  }

  if (w1 == w2)
  {
    while (i1 < var1->ndigits && i2 < var2->ndigits)
    {
      stat = var1->digits[i1++] - var2->digits[i2++];
      if (stat)
      {
        if (stat > 0)
        {
          return 1;
        }
        return -1;
      }
    }
  }

  while (i1 < var1->ndigits)
  {
    if (var1->digits[i1++] != 0)
    {
      return 1;
    }
  }
  while (i2 < var2->ndigits)
  {
    if (var2->digits[i2++] != 0)
    {
      return -1;
    }
  }

  return 0;
}

              
               
   
                                                         
                                                             
              
   
static int
add_abs(numeric *var1, numeric *var2, numeric *result)
{
  NumericDigit *res_buf;
  NumericDigit *res_digits;
  int res_ndigits;
  int res_weight;
  int res_rscale;
  int res_dscale;
  int i, i1, i2;
  int carry = 0;

                                                                 
  int var1ndigits = var1->ndigits;
  int var2ndigits = var2->ndigits;
  NumericDigit *var1digits = var1->digits;
  NumericDigit *var2digits = var2->digits;

  res_weight = Max(var1->weight, var2->weight) + 1;
  res_rscale = Max(var1->rscale, var2->rscale);
  res_dscale = Max(var1->dscale, var2->dscale);
  res_ndigits = res_rscale + res_weight + 1;
  if (res_ndigits <= 0)
  {
    res_ndigits = 1;
  }

  if ((res_buf = digitbuf_alloc(res_ndigits)) == NULL)
  {
    return -1;
  }
  res_digits = res_buf;

  i1 = res_rscale + var1->weight + 1;
  i2 = res_rscale + var2->weight + 1;
  for (i = res_ndigits - 1; i >= 0; i--)
  {
    i1--;
    i2--;
    if (i1 >= 0 && i1 < var1ndigits)
    {
      carry += var1digits[i1];
    }
    if (i2 >= 0 && i2 < var2ndigits)
    {
      carry += var2digits[i2];
    }

    if (carry >= 10)
    {
      res_digits[i] = carry - 10;
      carry = 1;
    }
    else
    {
      res_digits[i] = carry;
      carry = 0;
    }
  }

  while (res_ndigits > 0 && *res_digits == 0)
  {
    res_digits++;
    res_weight--;
    res_ndigits--;
  }
  while (res_ndigits > 0 && res_digits[res_ndigits - 1] == 0)
  {
    res_ndigits--;
  }

  if (res_ndigits == 0)
  {
    res_weight = 0;
  }

  digitbuf_free(result->buf);
  result->ndigits = res_ndigits;
  result->buf = res_buf;
  result->digits = res_digits;
  result->weight = res_weight;
  result->rscale = res_rscale;
  result->dscale = res_dscale;

  return 0;
}

              
               
   
                                                                       
                                                                  
                   
   
                                                    
              
   
static int
sub_abs(numeric *var1, numeric *var2, numeric *result)
{
  NumericDigit *res_buf;
  NumericDigit *res_digits;
  int res_ndigits;
  int res_weight;
  int res_rscale;
  int res_dscale;
  int i, i1, i2;
  int borrow = 0;

                                                                 
  int var1ndigits = var1->ndigits;
  int var2ndigits = var2->ndigits;
  NumericDigit *var1digits = var1->digits;
  NumericDigit *var2digits = var2->digits;

  res_weight = var1->weight;
  res_rscale = Max(var1->rscale, var2->rscale);
  res_dscale = Max(var1->dscale, var2->dscale);
  res_ndigits = res_rscale + res_weight + 1;
  if (res_ndigits <= 0)
  {
    res_ndigits = 1;
  }

  if ((res_buf = digitbuf_alloc(res_ndigits)) == NULL)
  {
    return -1;
  }
  res_digits = res_buf;

  i1 = res_rscale + var1->weight + 1;
  i2 = res_rscale + var2->weight + 1;
  for (i = res_ndigits - 1; i >= 0; i--)
  {
    i1--;
    i2--;
    if (i1 >= 0 && i1 < var1ndigits)
    {
      borrow += var1digits[i1];
    }
    if (i2 >= 0 && i2 < var2ndigits)
    {
      borrow -= var2digits[i2];
    }

    if (borrow < 0)
    {
      res_digits[i] = borrow + 10;
      borrow = -1;
    }
    else
    {
      res_digits[i] = borrow;
      borrow = 0;
    }
  }

  while (res_ndigits > 0 && *res_digits == 0)
  {
    res_digits++;
    res_weight--;
    res_ndigits--;
  }
  while (res_ndigits > 0 && res_digits[res_ndigits - 1] == 0)
  {
    res_ndigits--;
  }

  if (res_ndigits == 0)
  {
    res_weight = 0;
  }

  digitbuf_free(result->buf);
  result->ndigits = res_ndigits;
  result->buf = res_buf;
  result->digits = res_digits;
  result->weight = res_weight;
  result->rscale = res_rscale;
  result->dscale = res_dscale;

  return 0;
}

              
               
   
                                                                         
                                                                 
              
   
int
PGTYPESnumeric_add(numeric *var1, numeric *var2, numeric *result)
{
     
                                                         
     
  if (var1->sign == NUMERIC_POS)
  {
    if (var2->sign == NUMERIC_POS)
    {
         
                                                             
         
      if (add_abs(var1, var2, result) != 0)
      {
        return -1;
      }
      result->sign = NUMERIC_POS;
    }
    else
    {
         
                                                                         
         
      switch (cmp_abs(var1, var2))
      {
      case 0:
                      
                                  
                         
                      
           
        zero_var(result);
        result->rscale = Max(var1->rscale, var2->rscale);
        result->dscale = Max(var1->dscale, var2->dscale);
        break;

      case 1:
                      
                                 
                                             
                      
           
        if (sub_abs(var1, var2, result) != 0)
        {
          return -1;
        }
        result->sign = NUMERIC_POS;
        break;

      case -1:
                      
                                 
                                             
                      
           
        if (sub_abs(var2, var1, result) != 0)
        {
          return -1;
        }
        result->sign = NUMERIC_NEG;
        break;
      }
    }
  }
  else
  {
    if (var2->sign == NUMERIC_POS)
    {
                    
                                            
                                      
                    
         
      switch (cmp_abs(var1, var2))
      {
      case 0:
                      
                                  
                         
                      
           
        zero_var(result);
        result->rscale = Max(var1->rscale, var2->rscale);
        result->dscale = Max(var1->dscale, var2->dscale);
        break;

      case 1:
                      
                                 
                                             
                      
           
        if (sub_abs(var1, var2, result) != 0)
        {
          return -1;
        }
        result->sign = NUMERIC_NEG;
        break;

      case -1:
                      
                                 
                                             
                      
           
        if (sub_abs(var2, var1, result) != 0)
        {
          return -1;
        }
        result->sign = NUMERIC_POS;
        break;
      }
    }
    else
    {
                    
                           
                                           
                    
         
      if (add_abs(var1, var2, result) != 0)
      {
        return -1;
      }
      result->sign = NUMERIC_NEG;
    }
  }

  return 0;
}

              
               
   
                                                                         
                                                                 
              
   
int
PGTYPESnumeric_sub(numeric *var1, numeric *var2, numeric *result)
{
     
                                                         
     
  if (var1->sign == NUMERIC_POS)
  {
    if (var2->sign == NUMERIC_NEG)
    {
                    
                                            
                                           
                    
         
      if (add_abs(var1, var2, result) != 0)
      {
        return -1;
      }
      result->sign = NUMERIC_POS;
    }
    else
    {
                    
                           
                                      
                    
         
      switch (cmp_abs(var1, var2))
      {
      case 0:
                      
                                  
                         
                      
           
        zero_var(result);
        result->rscale = Max(var1->rscale, var2->rscale);
        result->dscale = Max(var1->dscale, var2->dscale);
        break;

      case 1:
                      
                                 
                                             
                      
           
        if (sub_abs(var1, var2, result) != 0)
        {
          return -1;
        }
        result->sign = NUMERIC_POS;
        break;

      case -1:
                      
                                 
                                             
                      
           
        if (sub_abs(var2, var1, result) != 0)
        {
          return -1;
        }
        result->sign = NUMERIC_NEG;
        break;
      }
    }
  }
  else
  {
    if (var2->sign == NUMERIC_NEG)
    {
                    
                           
                                      
                    
         
      switch (cmp_abs(var1, var2))
      {
      case 0:
                      
                                  
                         
                      
           
        zero_var(result);
        result->rscale = Max(var1->rscale, var2->rscale);
        result->dscale = Max(var1->dscale, var2->dscale);
        break;

      case 1:
                      
                                 
                                             
                      
           
        if (sub_abs(var1, var2, result) != 0)
        {
          return -1;
        }
        result->sign = NUMERIC_NEG;
        break;

      case -1:
                      
                                 
                                             
                      
           
        if (sub_abs(var2, var1, result) != 0)
        {
          return -1;
        }
        result->sign = NUMERIC_POS;
        break;
      }
    }
    else
    {
                    
                                            
                                           
                    
         
      if (add_abs(var1, var2, result) != 0)
      {
        return -1;
      }
      result->sign = NUMERIC_NEG;
    }
  }

  return 0;
}

              
               
   
                                                                      
                                                                  
              
   
int
PGTYPESnumeric_mul(numeric *var1, numeric *var2, numeric *result)
{
  NumericDigit *res_buf;
  NumericDigit *res_digits;
  int res_ndigits;
  int res_weight;
  int res_sign;
  int i, ri, i1, i2;
  long sum = 0;
  int global_rscale = var1->rscale + var2->rscale;

  res_weight = var1->weight + var2->weight + 2;
  res_ndigits = var1->ndigits + var2->ndigits + 1;
  if (var1->sign == var2->sign)
  {
    res_sign = NUMERIC_POS;
  }
  else
  {
    res_sign = NUMERIC_NEG;
  }

  if ((res_buf = digitbuf_alloc(res_ndigits)) == NULL)
  {
    return -1;
  }
  res_digits = res_buf;
  memset(res_digits, 0, res_ndigits);

  ri = res_ndigits;
  for (i1 = var1->ndigits - 1; i1 >= 0; i1--)
  {
    sum = 0;
    i = --ri;

    for (i2 = var2->ndigits - 1; i2 >= 0; i2--)
    {
      sum += res_digits[i] + var1->digits[i1] * var2->digits[i2];
      res_digits[i--] = sum % 10;
      sum /= 10;
    }
    res_digits[i] = sum;
  }

  i = res_weight + global_rscale + 2;
  if (i >= 0 && i < res_ndigits)
  {
    sum = (res_digits[i] > 4) ? 1 : 0;
    res_ndigits = i;
    i--;
    while (sum)
    {
      sum += res_digits[i];
      res_digits[i--] = sum % 10;
      sum /= 10;
    }
  }

  while (res_ndigits > 0 && *res_digits == 0)
  {
    res_digits++;
    res_weight--;
    res_ndigits--;
  }
  while (res_ndigits > 0 && res_digits[res_ndigits - 1] == 0)
  {
    res_ndigits--;
  }

  if (res_ndigits == 0)
  {
    res_sign = NUMERIC_POS;
    res_weight = 0;
  }

  digitbuf_free(result->buf);
  result->buf = res_buf;
  result->digits = res_digits;
  result->ndigits = res_ndigits;
  result->weight = res_weight;
  result->rscale = global_rscale;
  result->sign = res_sign;
  result->dscale = var1->dscale + var2->dscale;

  return 0;
}

   
                                        
   
                                                                  
                                                                     
   
                                                 
   
static int
select_div_scale(numeric *var1, numeric *var2, int *rscale)
{
  int weight1, weight2, qweight, i;
  NumericDigit firstdigit1, firstdigit2;
  int res_dscale;

     
                                                                             
                                                                  
                                                                        
                                                                        
                                   
     

                                                                        

  weight1 = 0;                                    
  firstdigit1 = 0;
  for (i = 0; i < var1->ndigits; i++)
  {
    firstdigit1 = var1->digits[i];
    if (firstdigit1 != 0)
    {
      weight1 = var1->weight - i;
      break;
    }
  }

  weight2 = 0;                                    
  firstdigit2 = 0;
  for (i = 0; i < var2->ndigits; i++)
  {
    firstdigit2 = var2->digits[i];
    if (firstdigit2 != 0)
    {
      weight2 = var2->weight - i;
      break;
    }
  }

     
                                                                         
                                                            
     
  qweight = weight1 - weight2;
  if (firstdigit1 <= firstdigit2)
  {
    qweight--;
  }

                            
  res_dscale = NUMERIC_MIN_SIG_DIGITS - qweight;
  res_dscale = Max(res_dscale, var1->dscale);
  res_dscale = Max(res_dscale, var2->dscale);
  res_dscale = Max(res_dscale, NUMERIC_MIN_DISPLAY_SCALE);
  res_dscale = Min(res_dscale, NUMERIC_MAX_DISPLAY_SCALE);

                           
  *rscale = res_dscale + 4;

  return res_dscale;
}

int
PGTYPESnumeric_div(numeric *var1, numeric *var2, numeric *result)
{
  NumericDigit *res_digits;
  int res_ndigits;
  int res_sign;
  int res_weight;
  numeric dividend;
  numeric divisor[10];
  int ndigits_tmp;
  int weight_tmp;
  int rscale_tmp;
  int ri;
  int i;
  long guess;
  long first_have;
  long first_div;
  int first_nextdigit;
  int stat = 0;
  int rscale;
  int res_dscale = select_div_scale(var1, var2, &rscale);
  int err = -1;
  NumericDigit *tmp_buf;

     
                                         
     
  ndigits_tmp = var2->ndigits + 1;
  if (ndigits_tmp == 1)
  {
    errno = PGTYPES_NUM_DIVIDE_ZERO;
    return -1;
  }

     
                                                                         
     
  if (var1->sign == var2->sign)
  {
    res_sign = NUMERIC_POS;
  }
  else
  {
    res_sign = NUMERIC_NEG;
  }
  res_weight = var1->weight - var2->weight + 1;
  res_ndigits = rscale + res_weight;
  if (res_ndigits <= 0)
  {
    res_ndigits = 1;
  }

     
                           
     
  if (var1->ndigits == 0)
  {
    zero_var(result);
    result->rscale = rscale;
    return 0;
  }

     
                                
     
  init_var(&dividend);
  for (i = 1; i < 10; i++)
  {
    init_var(&divisor[i]);
  }

     
                                                                 
     
  divisor[1].ndigits = ndigits_tmp;
  divisor[1].rscale = var2->ndigits;
  divisor[1].sign = NUMERIC_POS;
  divisor[1].buf = digitbuf_alloc(ndigits_tmp);
  if (divisor[1].buf == NULL)
  {
    goto done;
  }
  divisor[1].digits = divisor[1].buf;
  divisor[1].digits[0] = 0;
  memcpy(&(divisor[1].digits[1]), var2->digits, ndigits_tmp - 1);

     
                                 
     
  dividend.ndigits = var1->ndigits;
  dividend.weight = 0;
  dividend.rscale = var1->ndigits;
  dividend.sign = NUMERIC_POS;
  dividend.buf = digitbuf_alloc(var1->ndigits);
  if (dividend.buf == NULL)
  {
    goto done;
  }
  dividend.digits = dividend.buf;
  memcpy(dividend.digits, var1->digits, var1->ndigits);

     
                                                                            
                                                                           
                         
     
  tmp_buf = digitbuf_alloc(res_ndigits + 2);
  if (tmp_buf == NULL)
  {
    goto done;
  }
  digitbuf_free(result->buf);
  result->buf = tmp_buf;
  res_digits = result->buf;
  result->digits = res_digits;
  result->ndigits = res_ndigits;
  result->weight = res_weight;
  result->rscale = rscale;
  result->sign = res_sign;
  res_digits[0] = 0;

  first_div = divisor[1].digits[1] * 10;
  if (ndigits_tmp > 2)
  {
    first_div += divisor[1].digits[2];
  }

  first_have = 0;
  first_nextdigit = 0;

  weight_tmp = 1;
  rscale_tmp = divisor[1].rscale;

  for (ri = 0; ri <= res_ndigits; ri++)
  {
    first_have = first_have * 10;
    if (first_nextdigit >= 0 && first_nextdigit < dividend.ndigits)
    {
      first_have += dividend.digits[first_nextdigit];
    }
    first_nextdigit++;

    guess = (first_have * 10) / first_div + 1;
    if (guess > 9)
    {
      guess = 9;
    }

    while (guess > 0)
    {
      if (divisor[guess].buf == NULL)
      {
        int i;
        long sum = 0;

        memcpy(&divisor[guess], &divisor[1], sizeof(numeric));
        divisor[guess].buf = digitbuf_alloc(divisor[guess].ndigits);
        if (divisor[guess].buf == NULL)
        {
          goto done;
        }
        divisor[guess].digits = divisor[guess].buf;
        for (i = divisor[1].ndigits - 1; i >= 0; i--)
        {
          sum += divisor[1].digits[i] * guess;
          divisor[guess].digits[i] = sum % 10;
          sum /= 10;
        }
      }

      divisor[guess].weight = weight_tmp;
      divisor[guess].rscale = rscale_tmp;

      stat = cmp_abs(&dividend, &divisor[guess]);
      if (stat >= 0)
      {
        break;
      }

      guess--;
    }

    res_digits[ri + 1] = guess;
    if (stat == 0)
    {
      ri++;
      break;
    }

    weight_tmp--;
    rscale_tmp++;

    if (guess == 0)
    {
      continue;
    }

    if (sub_abs(&dividend, &divisor[guess], &dividend) != 0)
    {
      goto done;
    }

    first_nextdigit = dividend.weight - weight_tmp;
    first_have = 0;
    if (first_nextdigit >= 0 && first_nextdigit < dividend.ndigits)
    {
      first_have = dividend.digits[first_nextdigit];
    }
    first_nextdigit++;
  }

  result->ndigits = ri + 1;
  if (ri == res_ndigits + 1)
  {
    int carry = (res_digits[ri] > 4) ? 1 : 0;

    result->ndigits = ri;
    res_digits[ri] = 0;

    while (carry && ri > 0)
    {
      carry += res_digits[--ri];
      res_digits[ri] = carry % 10;
      carry /= 10;
    }
  }

  while (result->ndigits > 0 && *(result->digits) == 0)
  {
    (result->digits)++;
    (result->weight)--;
    (result->ndigits)--;
  }
  while (result->ndigits > 0 && result->digits[result->ndigits - 1] == 0)
  {
    (result->ndigits)--;
  }
  if (result->ndigits == 0)
  {
    result->sign = NUMERIC_POS;
  }

  result->dscale = res_dscale;
  err = 0;                                                

done:

     
             
     
  if (dividend.buf != NULL)
  {
    digitbuf_free(dividend.buf);
  }

  for (i = 1; i < 10; i++)
  {
    if (divisor[i].buf != NULL)
    {
      digitbuf_free(divisor[i].buf);
    }
  }

  return err;
}

int
PGTYPESnumeric_cmp(numeric *var1, numeric *var2)
{
                                                    

                                                         
  if (var1->sign == NUMERIC_POS && var2->sign == NUMERIC_POS)
  {
    return cmp_abs(var1, var2);
  }

                                                                      
  if (var1->sign == NUMERIC_NEG && var2->sign == NUMERIC_NEG)
  {
       
                                                                         
       
    return cmp_abs(var2, var1);
  }

                                                 
  if (var1->sign == NUMERIC_POS && var2->sign == NUMERIC_NEG)
  {
    return 1;
  }
  if (var1->sign == NUMERIC_NEG && var2->sign == NUMERIC_POS)
  {
    return -1;
  }

  errno = PGTYPES_NUM_BAD_NUMERIC;
  return INT_MAX;
}

int
PGTYPESnumeric_from_int(signed int int_val, numeric *var)
{
                           
  signed long int long_int = int_val;

  return PGTYPESnumeric_from_long(long_int, var);
}

int
PGTYPESnumeric_from_long(signed long int long_val, numeric *var)
{
                                                 
                                        

     
                                                                            
                   
     

  int size = 0;
  int i;
  signed long int abs_long_val = long_val;
  signed long int extract;
  signed long int reach_limit;

  if (abs_long_val < 0)
  {
    abs_long_val *= -1;
    var->sign = NUMERIC_NEG;
  }
  else
  {
    var->sign = NUMERIC_POS;
  }

  reach_limit = 1;
  do
  {
    size++;
    reach_limit *= 10;
  } while (reach_limit - 1 < abs_long_val && reach_limit <= LONG_MAX / 10);

  if (reach_limit > LONG_MAX / 10)
  {
                                      
    size += 2;
  }
  else
  {
                         
    size++;
    reach_limit /= 10;
  }

  if (alloc_var(var, size) < 0)
  {
    return -1;
  }

  var->rscale = 1;
  var->dscale = 1;
  var->weight = size - 2;

  i = 0;
  do
  {
    extract = abs_long_val - (abs_long_val % reach_limit);
    var->digits[i] = extract / reach_limit;
    abs_long_val -= extract;
    i++;
    reach_limit /= 10;

       
                                                                       
                                                                        
                                   
       
  } while (abs_long_val > 0);

  return 0;
}

int
PGTYPESnumeric_copy(numeric *src, numeric *dst)
{
  int i;

  if (dst == NULL)
  {
    return -1;
  }
  zero_var(dst);

  dst->weight = src->weight;
  dst->rscale = src->rscale;
  dst->dscale = src->dscale;
  dst->sign = src->sign;

  if (alloc_var(dst, src->ndigits) != 0)
  {
    return -1;
  }

  for (i = 0; i < src->ndigits; i++)
  {
    dst->digits[i] = src->digits[i];
  }

  return 0;
}

int
PGTYPESnumeric_from_double(double d, numeric *dst)
{
  char buffer[DBL_DIG + 100];
  numeric *tmp;
  int i;

  if (sprintf(buffer, "%.*g", DBL_DIG, d) <= 0)
  {
    return -1;
  }

  if ((tmp = PGTYPESnumeric_from_asc(buffer, NULL)) == NULL)
  {
    return -1;
  }
  i = PGTYPESnumeric_copy(tmp, dst);
  PGTYPESnumeric_free(tmp);
  if (i != 0)
  {
    return -1;
  }

  errno = 0;
  return 0;
}

static int
numericvar_to_double(numeric *var, double *dp)
{
  char *tmp;
  double val;
  char *endptr;
  numeric *varcopy = PGTYPESnumeric_new();

  if (varcopy == NULL)
  {
    return -1;
  }

  if (PGTYPESnumeric_copy(var, varcopy) < 0)
  {
    PGTYPESnumeric_free(varcopy);
    return -1;
  }

  tmp = get_str_from_var(varcopy, varcopy->dscale);
  PGTYPESnumeric_free(varcopy);

  if (tmp == NULL)
  {
    return -1;
  }

     
                                                          
     
  errno = 0;
  val = strtod(tmp, &endptr);
  if (errno == ERANGE)
  {
    free(tmp);
    if (val == 0)
    {
      errno = PGTYPES_NUM_UNDERFLOW;
    }
    else
    {
      errno = PGTYPES_NUM_OVERFLOW;
    }
    return -1;
  }

                                                       
  if (*endptr != '\0')
  {
                              
    free(tmp);
    errno = PGTYPES_NUM_BAD_NUMERIC;
    return -1;
  }
  free(tmp);
  *dp = val;
  return 0;
}

int
PGTYPESnumeric_to_double(numeric *nv, double *dp)
{
  double tmp;

  if (numericvar_to_double(nv, &tmp) != 0)
  {
    return -1;
  }
  *dp = tmp;
  return 0;
}

int
PGTYPESnumeric_to_int(numeric *nv, int *ip)
{
  long l;
  int i;

  if ((i = PGTYPESnumeric_to_long(nv, &l)) != 0)
  {
    return i;
  }

                                                               
#if SIZEOF_LONG > SIZEOF_INT

  if (l < INT_MIN || l > INT_MAX)
  {
    errno = PGTYPES_NUM_OVERFLOW;
    return -1;
  }

#endif

  *ip = (int)l;
  return 0;
}

int
PGTYPESnumeric_to_long(numeric *nv, long *lp)
{
  char *s = PGTYPESnumeric_to_asc(nv, 0);
  char *endptr;

  if (s == NULL)
  {
    return -1;
  }

  errno = 0;
  *lp = strtol(s, &endptr, 10);
  if (endptr == s)
  {
                                         
    free(s);
    return -1;
  }
  free(s);
  if (errno == ERANGE)
  {
    if (*lp == LONG_MIN)
    {
      errno = PGTYPES_NUM_UNDERFLOW;
    }
    else
    {
      errno = PGTYPES_NUM_OVERFLOW;
    }
    return -1;
  }
  return 0;
}

int
PGTYPESnumeric_to_decimal(numeric *src, decimal *dst)
{
  int i;

  if (src->ndigits > DECSIZE)
  {
    errno = PGTYPES_NUM_OVERFLOW;
    return -1;
  }

  dst->weight = src->weight;
  dst->rscale = src->rscale;
  dst->dscale = src->dscale;
  dst->sign = src->sign;
  dst->ndigits = src->ndigits;

  for (i = 0; i < src->ndigits; i++)
  {
    dst->digits[i] = src->digits[i];
  }

  return 0;
}

int
PGTYPESnumeric_from_decimal(decimal *src, numeric *dst)
{
  int i;

  zero_var(dst);

  dst->weight = src->weight;
  dst->rscale = src->rscale;
  dst->dscale = src->dscale;
  dst->sign = src->sign;

  if (alloc_var(dst, src->ndigits) != 0)
  {
    return -1;
  }

  for (i = 0; i < src->ndigits; i++)
  {
    dst->digits[i] = src->digits[i];
  }

  return 0;
}
