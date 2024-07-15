                                                                            
   
               
                                                      
   
                                                                          
                                                                            
                                                                          
                                                                             
                                                                            
   
                                                                       
                                                                      
                                                                      
                                                                  
   
   
                                                                         
                                                   
   
                  
                                   
   
                                                                            
   

#include "regex/regguts.h"

#include "regex/regexport.h"

   
                                   
   
int
pg_reg_getnumstates(const regex_t *regex)
{
  struct cnfa *cnfa;

  assert(regex != NULL && regex->re_magic == REMAGIC);
  cnfa = &((struct guts *)regex->re_guts)->search;

  return cnfa->nstates;
}

   
                             
   
int
pg_reg_getinitialstate(const regex_t *regex)
{
  struct cnfa *cnfa;

  assert(regex != NULL && regex->re_magic == REMAGIC);
  cnfa = &((struct guts *)regex->re_guts)->search;

  return cnfa->pre;
}

   
                           
   
int
pg_reg_getfinalstate(const regex_t *regex)
{
  struct cnfa *cnfa;

  assert(regex != NULL && regex->re_magic == REMAGIC);
  cnfa = &((struct guts *)regex->re_guts)->search;

  return cnfa->post;
}

   
                                                                              
                                                                              
                                                                         
                                                                              
                                                                           
                                                                             
                                                                          
                                                                         
                                                                          
                                                                           
                                              
   
                                                                             
                                                                      
                                                                           
                                                  
   
static void
traverse_lacons(struct cnfa *cnfa, int st, int *arcs_count, regex_arc_t *arcs, int arcs_len)
{
  struct carc *ca;

     
                                                                             
                                                                         
                                                                   
     
  check_stack_depth();

  for (ca = cnfa->states[st]; ca->co != COLORLESS; ca++)
  {
    if (ca->co < cnfa->ncolors)
    {
                                                       
      int ndx = (*arcs_count)++;

      if (ndx < arcs_len)
      {
        arcs[ndx].co = ca->co;
        arcs[ndx].to = ca->to;
      }
    }
    else
    {
                                                              
                                                                        
      Assert(ca->to != cnfa->post);

      traverse_lacons(cnfa, ca->to, arcs_count, arcs, arcs_len);
    }
  }
}

   
                                                         
   
int
pg_reg_getnumoutarcs(const regex_t *regex, int st)
{
  struct cnfa *cnfa;
  int arcs_count;

  assert(regex != NULL && regex->re_magic == REMAGIC);
  cnfa = &((struct guts *)regex->re_guts)->search;

  if (st < 0 || st >= cnfa->nstates)
  {
    return 0;
  }
  arcs_count = 0;
  traverse_lacons(cnfa, st, &arcs_count, NULL, 0);
  return arcs_count;
}

   
                                                                      
                                                                  
                                                               
   
void
pg_reg_getoutarcs(const regex_t *regex, int st, regex_arc_t *arcs, int arcs_len)
{
  struct cnfa *cnfa;
  int arcs_count;

  assert(regex != NULL && regex->re_magic == REMAGIC);
  cnfa = &((struct guts *)regex->re_guts)->search;

  if (st < 0 || st >= cnfa->nstates || arcs_len <= 0)
  {
    return;
  }
  arcs_count = 0;
  traverse_lacons(cnfa, st, &arcs_count, arcs, arcs_len);
}

   
                               
   
int
pg_reg_getnumcolors(const regex_t *regex)
{
  struct colormap *cm;

  assert(regex != NULL && regex->re_magic == REMAGIC);
  cm = &((struct guts *)regex->re_guts)->cmap;

  return cm->max + 1;
}

   
                                               
   
                                                                                
                              
   
int
pg_reg_colorisbegin(const regex_t *regex, int co)
{
  struct cnfa *cnfa;

  assert(regex != NULL && regex->re_magic == REMAGIC);
  cnfa = &((struct guts *)regex->re_guts)->search;

  if (co == cnfa->bos[0] || co == cnfa->bos[1])
  {
    return true;
  }
  else
  {
    return false;
  }
}

   
                                         
   
int
pg_reg_colorisend(const regex_t *regex, int co)
{
  struct cnfa *cnfa;

  assert(regex != NULL && regex->re_magic == REMAGIC);
  cnfa = &((struct guts *)regex->re_guts)->search;

  if (co == cnfa->eos[0] || co == cnfa->eos[1])
  {
    return true;
  }
  else
  {
    return false;
  }
}

   
                                                   
   
                                                                            
                                                                             
                                                                    
   
int
pg_reg_getnumcharacters(const regex_t *regex, int co)
{
  struct colormap *cm;

  assert(regex != NULL && regex->re_magic == REMAGIC);
  cm = &((struct guts *)regex->re_guts)->cmap;

  if (co <= 0 || co > cm->max)                                 
  {
    return -1;
  }
  if (cm->cd[co].flags & PSEUDO)                                  
  {
    return -1;
  }

     
                                                                             
                                                                             
                                                                      
                                                                         
                    
     
  if (cm->cd[co].nuchrs != 0)
  {
    return -1;
  }

                                                  
  return cm->cd[co].nschrs;
}

   
                                                                 
                                                                   
                                                                   
   
                                                                    
   
                                                      
   
void
pg_reg_getcharacters(const regex_t *regex, int co, pg_wchar *chars, int chars_len)
{
  struct colormap *cm;
  chr c;

  assert(regex != NULL && regex->re_magic == REMAGIC);
  cm = &((struct guts *)regex->re_guts)->cmap;

  if (co <= 0 || co > cm->max || chars_len <= 0)
  {
    return;
  }
  if (cm->cd[co].flags & PSEUDO)
  {
    return;
  }

     
                                                                         
                                       
     
  for (c = CHR_MIN; c <= MAX_SIMPLE_CHR; c++)
  {
    if (cm->locolormap[c - CHR_MIN] == co)
    {
      *chars++ = c;
      if (--chars_len == 0)
      {
        break;
      }
    }
  }
}
