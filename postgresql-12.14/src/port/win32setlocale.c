                                                                            
   
                    
                                                                      
   
                                                                
   
                  
                               
   
   
                                                                        
                                                                            
            
   
                                                
   
                                                                             
                                                                         
                                                                              
                                                                           
                                        
   
                                                                             
                                                                              
                                                                       
                                                                       
                                                                             
                                                                            
                                                                       
           
                                                                            
   

#include "c.h"

#undef setlocale

struct locale_map
{
     
                                                                             
                                                                             
                                                                    
                                                                     
                                  
     
  const char *locale_name_start;
  const char *locale_name_end;

  const char *replacement;                                       
};

   
                                                                 
   
static const struct locale_map locale_map_argument[] = {
       
                             
                                                                          
                                 
       
                                                                           
                                             
       
    {"Hong Kong S.A.R.", NULL, "HKG"}, {"U.A.E.", NULL, "ARE"},

       
                                                                              
                                                                               
                                                                              
                                                                           
                                                                              
              
       
                                                                            
                                        
       
                                                                  
       
    {"Chinese (Traditional)_Macau S.A.R..950", NULL, "ZHM"}, {"Chinese_Macau S.A.R..950", NULL, "ZHM"}, {"Chinese (Traditional)_Macao S.A.R..950", NULL, "ZHM"}, {"Chinese_Macao S.A.R..950", NULL, "ZHM"}, {NULL, NULL, NULL}};

   
                                                                    
   
static const struct locale_map locale_map_result[] = {
       
                                                                             
                                     
       
                                                                         
                                                                           
       
                                                                               
                                                                 
       
    {"Norwegian (Bokm", "l)_Norway", "Norwegian_Norway"}, {"Norwegian Bokm", "l_Norway", "Norwegian_Norway"}, {NULL, NULL, NULL}};

#define MAX_LOCALE_NAME_LEN 100

static const char *
map_locale(const struct locale_map *map, const char *locale)
{
  static char aliasbuf[MAX_LOCALE_NAME_LEN];
  int i;

                                                                     
  for (i = 0; map[i].locale_name_start != NULL; i++)
  {
    const char *needle_start = map[i].locale_name_start;
    const char *needle_end = map[i].locale_name_end;
    const char *replacement = map[i].replacement;
    char *match;
    char *match_start = NULL;
    char *match_end = NULL;

    match = strstr(locale, needle_start);
    if (match)
    {
         
                                                                  
                                            
         
      match_start = match;
      if (needle_end)
      {
        match = strstr(match_start + strlen(needle_start), needle_end);
        if (match)
        {
          match_end = match + strlen(needle_end);
        }
        else
        {
          match_start = NULL;
        }
      }
      else
      {
        match_end = match_start + strlen(needle_start);
      }
    }

    if (match_start)
    {
                                                      
      int matchpos = match_start - locale;
      int replacementlen = strlen(replacement);
      char *rest = match_end;
      int restlen = strlen(rest);

                                                           
      if (matchpos + replacementlen + restlen + 1 > MAX_LOCALE_NAME_LEN)
      {
        return NULL;
      }

      memcpy(&aliasbuf[0], &locale[0], matchpos);
      memcpy(&aliasbuf[matchpos], replacement, replacementlen);
                                    
      memcpy(&aliasbuf[matchpos + replacementlen], rest, restlen + 1);

      return aliasbuf;
    }
  }

                                                 
  return locale;
}

char *
pgwin32_setlocale(int category, const char *locale)
{
  const char *argument;
  char *result;

  if (locale == NULL)
  {
    argument = NULL;
  }
  else
  {
    argument = map_locale(locale_map_argument, locale);
  }

                                          
  result = setlocale(category, argument);

     
                                                                      
                                                                    
     
  if (result)
  {
    result = unconstify(char *, map_locale(locale_map_result, result));
  }

  return result;
}
