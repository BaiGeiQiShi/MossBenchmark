                                                                            
   
           
   
                                                                         
                                                                        
   
   
                  
                      
   
                                                                            
   

#include "c.h"

#include <float.h>
#include <math.h>

#if HAVE_FPCLASS                                                

#if HAVE_IEEEFP_H
#include <ieeefp.h>
#endif

int
isinf(double d)
{
  fpclass_t type = fpclass(d);

  switch (type)
  {
  case FP_NINF:
  case FP_PINF:
    return 1;
  default:
    break;
  }
  return 0;
}
#else

#if defined(HAVE_FP_CLASS) || defined(HAVE_FP_CLASS_D)

#if HAVE_FP_CLASS_H
#include <fp_class.h>
#endif

int
isinf(double x)
{
#if HAVE_FP_CLASS
  int fpclass = fp_class(x);
#else
  int fpclass = fp_class_d(x);
#endif

  if (fpclass == FP_POS_INF)
  {
    return 1;
  }
  if (fpclass == FP_NEG_INF)
  {
    return -1;
  }
  return 0;
}

#elif defined(HAVE_CLASS)

int
isinf(double x)
{
  int fpclass = class(x);

  if (fpclass == FP_PLUS_INF)
  {
    return 1;
  }
  if (fpclass == FP_MINUS_INF)
  {
    return -1;
  }
  return 0;
}

#endif

#endif
