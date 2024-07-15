                          

                                                                                
                                                                           
                                                                    
 
                                                                           
                                                
 
                                                                           
                                               
                                                                              

#include "postgres.h"                                    

#include "executor/executor.h"                               
#include "utils/geo_decls.h"                       

PG_MODULE_MAGIC;

                                                               

int
add_one(int arg);
float8 *
add_one_float8(float8 *arg);
Point *
makepoint(Point *pointx, Point *pointy);
text *
copytext(text *t);
text *
concat_text(text *arg1, text *arg2);
bool
c_overpaid(HeapTupleHeader t,                                  
    int32 limit);

              

int
add_one(int arg)
{
  return arg + 1;
}

                                

float8 *
add_one_float8(float8 *arg)
{
  float8 *result = (float8 *)palloc(sizeof(float8));

  *result = *arg + 1.0;

  return result;
}

Point *
makepoint(Point *pointx, Point *pointy)
{
  Point *new_point = (Point *)palloc(sizeof(Point));

  new_point->x = pointx->x;
  new_point->y = pointy->y;

  return new_point;
}

                                   

text *
copytext(text *t)
{
     
                                                       
     
  text *new_t = (text *)palloc(VARSIZE(t));

  SET_VARSIZE(new_t, VARSIZE(t));

     
                                                            
     
  memcpy((void *)VARDATA(new_t),                  
      (void *)VARDATA(t),                    
      VARSIZE(t) - VARHDRSZ);                        
  return new_t;
}

text *
concat_text(text *arg1, text *arg2)
{
  int32 arg1_size = VARSIZE(arg1) - VARHDRSZ;
  int32 arg2_size = VARSIZE(arg2) - VARHDRSZ;
  int32 new_text_size = arg1_size + arg2_size + VARHDRSZ;
  text *new_text = (text *)palloc(new_text_size);

  SET_VARSIZE(new_text, new_text_size);
  memcpy(VARDATA(new_text), VARDATA(arg1), arg1_size);
  memcpy(VARDATA(new_text) + arg1_size, VARDATA(arg2), arg2_size);
  return new_text;
}

                     

bool
c_overpaid(HeapTupleHeader t,                                  
    int32 limit)
{
  bool isnull;
  int32 salary;

  salary = DatumGetInt32(GetAttributeByName(t, "salary", &isnull));
  if (isnull)
  {
    return false;
  }
  return salary > limit;
}
