                                                                            
   
           
                                   
   
   
                                                                
   
   
                  
                               
   
                                                                            
   
#include "postgres.h"

#include "nodes/parsenodes.h"

   
               
   
Value *
makeInteger(int i)
{
  Value *v = makeNode(Value);

  v->type = T_Integer;
  v->val.ival = i;
  return v;
}

   
             
   
                                                        
   
Value *
makeFloat(char *numericStr)
{
  Value *v = makeNode(Value);

  v->type = T_Float;
  v->val.str = numericStr;
  return v;
}

   
              
   
                                                        
   
Value *
makeString(char *str)
{
  Value *v = makeNode(Value);

  v->type = T_String;
  v->val.str = str;
  return v;
}

   
                 
   
                                                        
   
Value *
makeBitString(char *str)
{
  Value *v = makeNode(Value);

  v->type = T_BitString;
  v->val.str = str;
  return v;
}
