                                                                            
   
                 
                                                                         
   
                                   
   
                                                                
   
                                                                            
   

#include "postgres_fe.h"

#include "datapagemap.h"

#include "common/logging.h"

struct datapagemap_iterator
{
  datapagemap_t *map;
  BlockNumber nextblkno;
};

       
                    
   

   
                              
   
void
datapagemap_add(datapagemap_t *map, BlockNumber blkno)
{
  int offset;
  int bitno;

  offset = blkno / 8;
  bitno = blkno % 8;

                                          
  if (map->bitmapsize <= offset)
  {
    int oldsize = map->bitmapsize;
    int newsize;

       
                                                                   
                                                                           
                                                                         
                                 
       
    newsize = offset + 1;
    newsize += 10;

    map->bitmap = pg_realloc(map->bitmap, newsize);

                                             
    memset(&map->bitmap[oldsize], 0, newsize - oldsize);

    map->bitmapsize = newsize;
  }

                   
  map->bitmap[offset] |= (1 << bitno);
}

   
                                                        
   
                                                                           
                                                                           
             
   
datapagemap_iterator_t *
datapagemap_iterate(datapagemap_t *map)
{
  datapagemap_iterator_t *iter;

  iter = pg_malloc(sizeof(datapagemap_iterator_t));
  iter->map = map;
  iter->nextblkno = 0;

  return iter;
}

bool
datapagemap_next(datapagemap_iterator_t *iter, BlockNumber *blkno)
{
  datapagemap_t *map = iter->map;

  for (;;)
  {
    BlockNumber blk = iter->nextblkno;
    int nextoff = blk / 8;
    int bitno = blk % 8;

    if (nextoff >= map->bitmapsize)
    {
      break;
    }

    iter->nextblkno++;

    if (map->bitmap[nextoff] & (1 << bitno))
    {
      *blkno = blk;
      return true;
    }
  }

                                        
  return false;
}

   
                                                             
   
void
datapagemap_print(datapagemap_t *map)
{
  datapagemap_iterator_t *iter;
  BlockNumber blocknum;

  iter = datapagemap_iterate(map);
  while (datapagemap_next(iter, &blocknum))
  {
    pg_log_debug("block %u", blocknum);
  }

  pg_free(iter);
}
