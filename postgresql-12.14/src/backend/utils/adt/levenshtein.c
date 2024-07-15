                                                                            
   
                 
                                          
   
                                                     
   
                                                                              
                                                                                
                                                                         
                                                                             
                                 
   
                                                                              
                                                                              
                                                                       
                                                                      
   
                                                                
   
                  
                                       
   
                                                                            
   
#define MAX_LEVENSHTEIN_STRLEN 255

   
                                                                              
                                    
   
                                                
                                                
                                                                           
                                                                      
                                      
                                                                           
                                                                        
   
                                                                         
                                                                         
                                                                        
                                                                         
           
   
                                                                          
                                                                         
                                                                         
                                                                         
          
   
                                                                              
                                                                          
                                                                             
                                                                      
                                                                               
                                                                              
                                                                              
                                                                          
                                                                       
                                                                            
                                                                         
                                                                            
                            
   
int
#ifdef LEVENSHTEIN_LESS_EQUAL
varstr_levenshtein_less_equal(const char *source, int slen, const char *target, int tlen, int ins_c, int del_c, int sub_c, int max_d, bool trusted)
#else
varstr_levenshtein(const char *source, int slen, const char *target, int tlen, int ins_c, int del_c, int sub_c, bool trusted)
#endif
{
  int m, n;
  int *prev;
  int *curr;
  int *s_char_len = NULL;
  int i, j;
  const char *y;

     
                                                                      
                                                                            
        
     
#ifdef LEVENSHTEIN_LESS_EQUAL
  int start_column, stop_column;

#undef START_COLUMN
#undef STOP_COLUMN
#define START_COLUMN start_column
#define STOP_COLUMN stop_column
#else
#undef START_COLUMN
#undef STOP_COLUMN
#define START_COLUMN 0
#define STOP_COLUMN m
#endif

                                                                  
  m = pg_mbstrlen_with_len(source, slen);
  n = pg_mbstrlen_with_len(target, tlen);

     
                                                                            
                                       
     
  if (!m)
  {
    return n * ins_c;
  }
  if (!n)
  {
    return m * del_c;
  }

     
                                                                    
                                                                    
                                                                       
                                                                            
                                                                        
     
  if (!trusted && (m > MAX_LEVENSHTEIN_STRLEN || n > MAX_LEVENSHTEIN_STRLEN))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("levenshtein argument exceeds maximum length of %d characters", MAX_LEVENSHTEIN_STRLEN)));
  }

#ifdef LEVENSHTEIN_LESS_EQUAL
                                          
  start_column = 0;
  stop_column = m + 1;

     
                                                                             
                                                                            
                                                                         
                          
     
  if (max_d >= 0)
  {
    int min_theo_d;                                    
    int max_theo_d;                                    
    int net_inserts = n - m;

    min_theo_d = net_inserts < 0 ? -net_inserts * del_c : net_inserts * ins_c;
    if (min_theo_d > max_d)
    {
      return max_d + 1;
    }
    if (ins_c + del_c < sub_c)
    {
      sub_c = ins_c + del_c;
    }
    max_theo_d = min_theo_d + sub_c * Min(m, n);
    if (max_d >= max_theo_d)
    {
      max_d = -1;
    }
    else if (ins_c + del_c > 0)
    {
         
                                                                        
                                                                     
                                                                        
                                                                         
                                                                        
                                                                     
                                                                      
                                                                         
                                                                    
                                                                    
         
      int slack_d = max_d - min_theo_d;
      int best_column = net_inserts < 0 ? -net_inserts : 0;

      stop_column = best_column + (slack_d / (ins_c + del_c)) + 1;
      if (stop_column > m)
      {
        stop_column = m + 1;
      }
    }
  }
#endif

     
                                                                             
                                                                          
                                                                           
                                                                    
                                                                            
                                                                         
     
  if (m != slen || n != tlen)
  {
    int i;
    const char *cp = source;

    s_char_len = (int *)palloc((m + 1) * sizeof(int));
    for (i = 0; i < m; ++i)
    {
      s_char_len[i] = pg_mblen(cp);
      cp += s_char_len[i];
    }
    s_char_len[i] = 0;
  }

                                                        
  ++m;
  ++n;

                                                    
  prev = (int *)palloc(2 * m * sizeof(int));
  curr = prev + m;

     
                                                                             
                                     
     
  for (i = START_COLUMN; i < STOP_COLUMN; i++)
  {
    prev[i] = i * del_c;
  }

                                               
  for (y = target, j = 1; j < n; j++)
  {
    int *temp;
    const char *x = source;
    int y_char_len = n != tlen + 1 ? pg_mblen(y) : 1;

#ifdef LEVENSHTEIN_LESS_EQUAL

       
                                                                          
                                                                          
                                                                        
                                                                         
       
    if (stop_column < m)
    {
      prev[stop_column] = max_d + 1;
      ++stop_column;
    }

       
                                                                         
                                                                         
                                                                          
                                         
       
    if (start_column == 0)
    {
      curr[0] = j * ins_c;
      i = 1;
    }
    else
    {
      i = start_column;
    }
#else
    curr[0] = j * ins_c;
    i = 1;
#endif

       
                                                                   
                                                                       
                                                                       
                                                                       
                                    
       
    if (s_char_len != NULL)
    {
      for (; i < STOP_COLUMN; i++)
      {
        int ins;
        int del;
        int sub;
        int x_char_len = s_char_len[i - 1];

           
                                                                      
           
                                                                       
                                                                 
                                                                      
                                                                   
                            
           
        ins = prev[i] + ins_c;
        del = curr[i - 1] + del_c;
        if (x[x_char_len - 1] == y[y_char_len - 1] && x_char_len == y_char_len && (x_char_len == 1 || rest_of_char_same(x, y, x_char_len)))
        {
          sub = prev[i - 1];
        }
        else
        {
          sub = prev[i - 1] + sub_c;
        }

                                             
        curr[i] = Min(ins, del);
        curr[i] = Min(curr[i], sub);

                                      
        x += x_char_len;
      }
    }
    else
    {
      for (; i < STOP_COLUMN; i++)
      {
        int ins;
        int del;
        int sub;

                                                                        
        ins = prev[i] + ins_c;
        del = curr[i - 1] + del_c;
        sub = prev[i - 1] + ((*x == *y) ? 0 : sub_c);

                                             
        curr[i] = Min(ins, del);
        curr[i] = Min(curr[i], sub);

                                      
        x++;
      }
    }

                                             
    temp = curr;
    curr = prev;
    prev = temp;

                                  
    y += y_char_len;

#ifdef LEVENSHTEIN_LESS_EQUAL

       
                                                                           
                                                                        
                                                                           
                                                                      
                                            
       
    if (max_d >= 0)
    {
         
                                                                     
                                                                       
                                                                      
                                                                       
                                                                         
                       
         
      int zp = j - (n - m);

                                                         
      while (stop_column > 0)
      {
        int ii = stop_column - 1;
        int net_inserts = ii - zp;

        if (prev[ii] + (net_inserts > 0 ? net_inserts * ins_c : -net_inserts * del_c) <= max_d)
        {
          break;
        }
        stop_column--;
      }

                                                           
      while (start_column < stop_column)
      {
        int net_inserts = start_column - zp;

        if (prev[start_column] + (net_inserts > 0 ? net_inserts * ins_c : -net_inserts * del_c) <= max_d)
        {
          break;
        }

           
                                                                       
                                                              
                                        
           
        prev[start_column] = max_d + 1;
        curr[start_column] = max_d + 1;
        if (start_column != 0)
        {
          source += (s_char_len != NULL) ? s_char_len[start_column - 1] : 1;
        }
        start_column++;
      }

                                                           
      if (start_column >= stop_column)
      {
        return max_d + 1;
      }
    }
#endif
  }

     
                                                                      
                                              
     
  return prev[m - 1];
}
