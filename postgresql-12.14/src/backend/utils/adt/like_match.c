                                                                            
   
                
                                          
   
                                                                            
                                                                        
                                                              
                                                                            
                                                                   
   
                                                                 
   
            
                                          
                                                                              
                                                                              
   
                                                                
   
                  
                                      
   
                                                                            
   

   
                                                                             
                                     
                                                                           
   
                                                                      
                        
   
                                                                
                                                             
   
                                                                         
   
                                                                          
                                                  
   
                                         
   
                                                       
                                                                      
                                         
   
                                                                      
                                                                   
                                                              
   
                                                                     
                                                                  
                                                                       
                       
   

                       
                                                                        
   
                         
                                
                                                                     
   
                                                                       
                                                                   
                       
   

#ifdef MATCH_LOWER
#define GETCHAR(t) MATCH_LOWER(t)
#else
#define GETCHAR(t) (t)
#endif

static int
MatchText(const char *t, int tlen, const char *p, int plen, pg_locale_t locale, bool locale_is_c)
{
                                              
  if (plen == 1 && *p == '%')
  {
    return LIKE_TRUE;
  }

                                                                          
  check_stack_depth();

     
                                                                           
                                                                             
                                                                           
                                                                           
                                                                   
                
     
  while (tlen > 0 && plen > 0)
  {
    if (*p == '\\')
    {
                                                                  
      NextByte(p, plen);
                                                             
      if (plen <= 0)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_ESCAPE_SEQUENCE), errmsg("LIKE pattern must not end with escape character")));
      }
      if (GETCHAR(*p) != GETCHAR(*t))
      {
        return LIKE_FALSE;
      }
    }
    else if (*p == '%')
    {
      char firstpat;

         
                                                                     
                                                                      
                                                                        
         
                                                                         
                                                                        
                                                                       
                                                                         
                                                                      
                                                                         
                                             
         
      NextByte(p, plen);

      while (plen > 0)
      {
        if (*p == '%')
        {
          NextByte(p, plen);
        }
        else if (*p == '_')
        {
                                                                   
          if (tlen <= 0)
          {
            return LIKE_ABORT;
          }
          NextChar(t, tlen);
          NextByte(p, plen);
        }
        else
        {
          break;                                          
        }
      }

         
                                                                       
                                            
         
      if (plen <= 0)
      {
        return LIKE_TRUE;
      }

         
                                                                       
                                                                         
                                                                         
                                                                     
                                                                        
                                                                      
                          
         
      if (*p == '\\')
      {
        if (plen < 2)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_ESCAPE_SEQUENCE), errmsg("LIKE pattern must not end with escape character")));
        }
        firstpat = GETCHAR(p[1]);
      }
      else
      {
        firstpat = GETCHAR(*p);
      }

      while (tlen > 0)
      {
        if (GETCHAR(*t) == firstpat)
        {
          int matched = MatchText(t, tlen, p, plen, locale, locale_is_c);

          if (matched != LIKE_FALSE)
          {
            return matched;                    
          }
        }

        NextChar(t, tlen);
      }

         
                                                                       
                                         
         
      return LIKE_ABORT;
    }
    else if (*p == '_')
    {
                                                                    
      NextChar(t, tlen);
      NextByte(p, plen);
      continue;
    }
    else if (GETCHAR(*p) != GETCHAR(*t))
    {
                                                              
      return LIKE_FALSE;
    }

       
                                           
       
                                                                     
                                                                           
                                                                          
                                                                        
                                                                          
                                                                   
                                                                           
                                                                          
       
    NextByte(t, tlen);
    NextByte(p, plen);
  }

  if (tlen > 0)
  {
    return LIKE_FALSE;                                      
  }

     
                                                                       
                                                                        
     
  while (plen > 0 && *p == '%')
  {
    NextByte(p, plen);
  }
  if (plen <= 0)
  {
    return LIKE_TRUE;
  }

     
                                                                            
                            
     
  return LIKE_ABORT;
}                  

   
                                                           
                                                                              
   
#ifdef do_like_escape

static text *
do_like_escape(text *pat, text *esc)
{
  text *result;
  char *p, *e, *r;
  int plen, elen;
  bool afterescape;

  p = VARDATA_ANY(pat);
  plen = VARSIZE_ANY_EXHDR(pat);
  e = VARDATA_ANY(esc);
  elen = VARSIZE_ANY_EXHDR(esc);

     
                                                                         
                                                             
     
  result = (text *)palloc(plen * 2 + VARHDRSZ);
  r = VARDATA(result);

  if (elen == 0)
  {
       
                                                                     
                                                          
       
    while (plen > 0)
    {
      if (*p == '\\')
      {
        *r++ = '\\';
      }
      CopyAdvChar(r, p, plen);
    }
  }
  else
  {
       
                                                             
       
    NextChar(e, elen);
    if (elen != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_ESCAPE_SEQUENCE), errmsg("invalid escape string"), errhint("Escape string must be empty or one character.")));
    }

    e = VARDATA_ANY(esc);

       
                                                                
       
    if (*e == '\\')
    {
      memcpy(result, pat, VARSIZE_ANY(pat));
      return result;
    }

       
                                                                           
                                                                      
                                   
       
    afterescape = false;
    while (plen > 0)
    {
      if (CHAREQ(p, e) && !afterescape)
      {
        *r++ = '\\';
        NextChar(p, plen);
        afterescape = true;
      }
      else if (*p == '\\')
      {
        *r++ = '\\';
        if (!afterescape)
        {
          *r++ = '\\';
        }
        NextChar(p, plen);
        afterescape = false;
      }
      else
      {
        CopyAdvChar(r, p, plen);
        afterescape = false;
      }
    }
  }

  SET_VARSIZE(result, r - ((char *)result));

  return result;
}
#endif                     

#ifdef CHAREQ
#undef CHAREQ
#endif

#undef NextChar
#undef CopyAdvChar
#undef MatchText

#ifdef do_like_escape
#undef do_like_escape
#endif

#undef GETCHAR

#ifdef MATCH_LOWER
#undef MATCH_LOWER

#endif
