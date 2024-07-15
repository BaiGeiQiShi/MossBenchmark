                                                                            
   
             
                                      
   
                                                                         
                                                                        
   
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include "storage/itemptr.h"

   
                     
                                                              
                             
   
         
                                                       
   
bool
ItemPointerEquals(ItemPointer pointer1, ItemPointer pointer2)
{
     
                                                                             
                                                          
     
  StaticAssertStmt(sizeof(ItemPointerData) == 3 * sizeof(uint16), "ItemPointerData struct is improperly padded");

  if (ItemPointerGetBlockNumber(pointer1) == ItemPointerGetBlockNumber(pointer2) && ItemPointerGetOffsetNumber(pointer1) == ItemPointerGetOffsetNumber(pointer2))
  {
    return true;
  }
  else
  {
    return false;
  }
}

   
                      
                                                      
   
int32
ItemPointerCompare(ItemPointer arg1, ItemPointer arg2)
{
     
                                                                      
                                                                   
     
  BlockNumber b1 = ItemPointerGetBlockNumberNoCheck(arg1);
  BlockNumber b2 = ItemPointerGetBlockNumberNoCheck(arg2);

  if (b1 < b2)
  {
    return -1;
  }
  else if (b1 > b2)
  {
    return 1;
  }
  else if (ItemPointerGetOffsetNumberNoCheck(arg1) < ItemPointerGetOffsetNumberNoCheck(arg2))
  {
    return -1;
  }
  else if (ItemPointerGetOffsetNumberNoCheck(arg1) > ItemPointerGetOffsetNumberNoCheck(arg2))
  {
    return 1;
  }
  else
  {
    return 0;
  }
}
