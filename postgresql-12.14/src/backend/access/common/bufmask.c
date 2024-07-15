                                                                            
   
             
                                                            
                                                                
                                  
   
                                                                         
   
                                                         
   
                  
                                         
   
                                                                            
   

#include "postgres.h"

#include "access/bufmask.h"

   
                 
   
                                                                           
                                                                            
                                                                         
                                                                               
   
void
mask_page_lsn_and_checksum(Page page)
{
  PageHeader phdr = (PageHeader)page;

  PageXLogRecPtrSet(phdr->pd_lsn, (uint64)MASK_MARKER);
  phdr->pd_checksum = MASK_MARKER;
}

   
                       
   
                                                                             
                                                   
   
void
mask_page_hint_bits(Page page)
{
  PageHeader phdr = (PageHeader)page;

                                               
  phdr->pd_prune_xid = MASK_MARKER;

                                                                             
  PageClearFull(page);
  PageClearHasFreeLinePointers(page);

     
                                                                             
                                                                     
              
     
  PageClearAllVisible(page);
}

   
                     
   
                                                                  
   
void
mask_unused_space(Page page)
{
  int pd_lower = ((PageHeader)page)->pd_lower;
  int pd_upper = ((PageHeader)page)->pd_upper;
  int pd_special = ((PageHeader)page)->pd_special;

                    
  if (pd_lower > pd_upper || pd_special < pd_upper || pd_lower < SizeOfPageHeaderData || pd_special > BLCKSZ)
  {
    elog(ERROR, "invalid page pd_lower %u pd_upper %u pd_special %u", pd_lower, pd_upper, pd_special);
  }

  memset(page + pd_lower, MASK_MARKER, pd_upper - pd_lower);
}

   
                 
   
                                                                           
                            
   
void
mask_lp_flags(Page page)
{
  OffsetNumber offnum, maxoff;

  maxoff = PageGetMaxOffsetNumber(page);
  for (offnum = FirstOffsetNumber; offnum <= maxoff; offnum = OffsetNumberNext(offnum))
  {
    ItemId itemId = PageGetItemId(page, offnum);

    if (ItemIdIsUsed(itemId))
    {
      itemId->lp_flags = LP_UNUSED;
    }
  }
}

   
                     
   
                                                                      
                       
   
void
mask_page_content(Page page)
{
                         
  memset(page + SizeOfPageHeaderData, MASK_MARKER, BLCKSZ - SizeOfPageHeaderData);

                                  
  memset(&((PageHeader)page)->pd_lower, MASK_MARKER, sizeof(uint16));
  memset(&((PageHeader)page)->pd_upper, MASK_MARKER, sizeof(uint16));
}
