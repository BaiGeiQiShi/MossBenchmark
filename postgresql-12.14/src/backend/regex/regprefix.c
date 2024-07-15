                                                                            
   
               
                                                             
   
   
                                                                         
                                                   
   
                  
                                   
   
                                                                            
   

#include "regex/regguts.h"

   
                        
   
static int
findprefix(struct cnfa *cnfa, struct colormap *cm, chr *string, size_t *slength);

   
                                                           
   
                   
                                                                        
                                                                      
                                                                          
                           
   
                                                                            
                                                                          
                
   
                                                                        
                                                                              
                                                                            
                                                                           
                                                    
   
int
pg_regprefix(regex_t *re, chr **string, size_t *slength)
{
  struct guts *g;
  struct cnfa *cnfa;
  int st;

                     
  if (string == NULL || slength == NULL)
  {
    return REG_INVARG;
  }
  *string = NULL;                                   
  *slength = 0;
  if (re == NULL || re->re_magic != REMAGIC)
  {
    return REG_INVARG;
  }
  if (re->re_csize != sizeof(chr))
  {
    return REG_MIXED;
  }

                                           
  pg_set_regex_collation(re->re_collation);

             
  g = (struct guts *)re->re_guts;
  if (g->info & REG_UIMPOSSIBLE)
  {
    return REG_NOMATCH;
  }

     
                                                                             
                                                                       
                                                            
     
  assert(g->tree != NULL);
  cnfa = &g->tree->cnfa;

     
                                                                             
                                                                             
                                                                      
     
  *string = (chr *)MALLOC(cnfa->nstates * sizeof(chr));
  if (*string == NULL)
  {
    return REG_ESPACE;
  }

             
  st = findprefix(cnfa, &g->cmap, *string, slength);

  assert(*slength <= cnfa->nstates);

                
  if (st != REG_PREFIX && st != REG_EXACT)
  {
    FREE(*string);
    *string = NULL;
    *slength = 0;
  }

  return st;
}

   
                                                
   
                                                                       
                                                                     
   
static int                            
findprefix(struct cnfa *cnfa, struct colormap *cm, chr *string, size_t *slength)
{
  int st;
  int nextst;
  color thiscolor;
  chr c;
  struct carc *ca;

     
                                                                        
                                                                           
                 
     
  st = cnfa->pre;
  nextst = -1;
  for (ca = cnfa->states[st]; ca->co != COLORLESS; ca++)
  {
    if (ca->co == cnfa->bos[0] || ca->co == cnfa->bos[1])
    {
      if (nextst == -1)
      {
        nextst = ca->to;
      }
      else if (nextst != ca->to)
      {
        return REG_NOMATCH;
      }
    }
    else
    {
      return REG_NOMATCH;
    }
  }
  if (nextst == -1)
  {
    return REG_NOMATCH;
  }

     
                                                                          
                                                                           
                                                             
     
                                                                            
                                                                             
                                                                            
                                                                            
                                                                             
                                                                             
                        
     
  do
  {
    st = nextst;
    nextst = -1;
    thiscolor = COLORLESS;
    for (ca = cnfa->states[st]; ca->co != COLORLESS; ca++)
    {
                                      
      if (ca->co == cnfa->bos[0] || ca->co == cnfa->bos[1])
      {
        continue;
      }
                                                                   
      if (ca->co == cnfa->eos[0] || ca->co == cnfa->eos[1] || ca->co >= cnfa->ncolors)
      {
        thiscolor = COLORLESS;
        break;
      }
      if (thiscolor == COLORLESS)
      {
                                
        thiscolor = ca->co;
        nextst = ca->to;
      }
      else if (thiscolor == ca->co)
      {
                                                 
        nextst = -1;
      }
      else
      {
                                                                    
        thiscolor = COLORLESS;
        break;
      }
    }
                                                                   
    if (thiscolor == COLORLESS)
    {
      break;
    }
                                       
    if (cm->cd[thiscolor].nschrs != 1)
    {
      break;
    }
                                                  
    if (cm->cd[thiscolor].nuchrs != 0)
    {
      break;
    }

       
                                                                     
                                                                         
                                                                          
                                                                          
                                                                           
                                                                       
                                                                          
                                                                           
                                                                         
                                                                      
       
    c = cm->cd[thiscolor].firstchr;
    if (GETCOLOR(cm, c) != thiscolor)
    {
      break;
    }

    string[(*slength)++] = c;

                                                                        
  } while (nextst != -1);

     
                                                                         
                                                                          
                                           
     
  nextst = -1;
  for (ca = cnfa->states[st]; ca->co != COLORLESS; ca++)
  {
    if (ca->co == cnfa->eos[0] || ca->co == cnfa->eos[1])
    {
      if (nextst == -1)
      {
        nextst = ca->to;
      }
      else if (nextst != ca->to)
      {
        nextst = -1;
        break;
      }
    }
    else
    {
      nextst = -1;
      break;
    }
  }
  if (nextst == cnfa->post)
  {
    return REG_EXACT;
  }

     
                                                                         
                                                                       
                                 
     
  if (*slength > 0)
  {
    return REG_PREFIX;
  }

  return REG_NOMATCH;
}
