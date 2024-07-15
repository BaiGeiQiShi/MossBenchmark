   
                                        
                                        
   
                                                                 
   
                                                                            
                                                                            
                                                                          
                       
   
                                                                        
                                                                
                                                                          
                                                        
   
                                                                           
                                                             
   
                                                                              
                                                                            
                                                                           
                                                                          
                                                                       
                                                                               
                                                                            
                                                                           
                                                                          
                                              
   
                                 
   
   

   
          
                                                                        
                              
   

   
                                 
   
static struct cvec *
newcvec(int nchrs,                                
    int nranges)                                 
{
  size_t nc = (size_t)nchrs + (size_t)nranges * 2;
  size_t n = sizeof(struct cvec) + nc * sizeof(chr);
  struct cvec *cv = (struct cvec *)MALLOC(n);

  if (cv == NULL)
  {
    return NULL;
  }
  cv->chrspace = nchrs;
  cv->chrs = (chr *)(((char *)cv) + sizeof(struct cvec));
  cv->ranges = cv->chrs + nchrs;
  cv->rangespace = nranges;
  return clearcvec(cv);
}

   
                                         
                                   
   
static struct cvec *
clearcvec(struct cvec *cv)
{
  assert(cv != NULL);
  cv->nchrs = 0;
  cv->nranges = 0;
  cv->cclasscode = -1;
  return cv;
}

   
                                
   
static void
addchr(struct cvec *cv,                       
    chr c)                                    
{
  assert(cv->nchrs < cv->chrspace);
  cv->chrs[cv->nchrs++] = c;
}

   
                                    
   
static void
addrange(struct cvec *cv,                       
    chr from,                                           
    chr to)                                            
{
  assert(cv->nranges < cv->rangespace);
  cv->ranges[cv->nranges * 2] = from;
  cv->ranges[cv->nranges * 2 + 1] = to;
  cv->nranges++;
}

   
                                                        
   
                                                                         
                                                                         
                                                                        
   
                                                                           
                                                                          
                                              
   
static struct cvec *
getcvec(struct vars *v,              
    int nchrs,                                         
    int nranges)                                      
{
                                                       
  if (v->cv != NULL && nchrs <= v->cv->chrspace && nranges <= v->cv->rangespace)
  {
    return clearcvec(v->cv);
  }

                            
  if (v->cv != NULL)
  {
    freecvec(v->cv);
  }
  v->cv = newcvec(nchrs, nranges);
  if (v->cv == NULL)
  {
    ERR(REG_ESPACE);
  }

  return v->cv;
}

   
                          
   
static void
freecvec(struct cvec *cv)
{
  FREE(cv);
}
