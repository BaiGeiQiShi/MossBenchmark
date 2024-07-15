                                                                            
   
              
                                                                         
   
   
                                                                         
                                                                        
   
                  
                                              
   
   
          
   
                                                                         
                                                                        
                                                                           
                                                                          
   
                                                                            
   
#include "postgres.h"

#include "storage/freespace.h"
#include "storage/indexfsm.h"

   
                     
   

   
                                                      
   
                                                            
   
BlockNumber
GetFreeIndexPage(Relation rel)
{
  BlockNumber blkno = GetPageWithFreeSpace(rel, BLCKSZ / 2);

  if (blkno != InvalidBlockNumber)
  {
    RecordUsedIndexPage(rel, blkno);
  }

  return blkno;
}

   
                                                        
   
void
RecordFreeIndexPage(Relation rel, BlockNumber freeBlock)
{
  RecordPageWithFreeSpace(rel, freeBlock, BLCKSZ - 1);
}

   
                                                        
   
void
RecordUsedIndexPage(Relation rel, BlockNumber usedBlock)
{
  RecordPageWithFreeSpace(rel, usedBlock, 0);
}

   
                                                                         
   
void
IndexFreeSpaceMapVacuum(Relation rel)
{
  FreeSpaceMapVacuum(rel);
}
