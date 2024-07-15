                                                                            
   
             
                                                                        
                                            
   
                                                                         
                                                                        
   
   
                  
                                  
   
                                                                            
   
#include "postgres.h"

#include <ctype.h>

#include "parser/scansup.h"
#include "mb/pg_wchar.h"

                    
            
   
                                                                             
         
   
                                                                           
           
                    
   

char *
scanstr(const char *s)
{
  char *newStr;
  int len, i, j;

  if (s == NULL || s[0] == '\0')
  {
    return pstrdup("");
  }

  len = strlen(s);

  newStr = palloc(len + 1);                               

  for (i = 0, j = 0; i < len; i++)
  {
    if (s[i] == '\'')
    {
         
                                                                      
                                                                
         
      i++;
                                                                
      Assert(s[i] == '\'');
      newStr[j] = s[i];
    }
    else if (s[i] == '\\')
    {
      i++;
      switch (s[i])
      {
      case 'b':
        newStr[j] = '\b';
        break;
      case 'f':
        newStr[j] = '\f';
        break;
      case 'n':
        newStr[j] = '\n';
        break;
      case 'r':
        newStr[j] = '\r';
        break;
      case 't':
        newStr[j] = '\t';
        break;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      {
        int k;
        long octVal = 0;

        for (k = 0; s[i + k] >= '0' && s[i + k] <= '7' && k < 3; k++)
        {
          octVal = (octVal << 3) + (s[i + k] - '0');
        }
        i += k - 1;
        newStr[j] = ((char)octVal);
      }
      break;
      default:
        newStr[j] = s[i];
        break;
      }             
    }                   
    else
    {
      newStr[j] = s[i];
    }
    j++;
  }
  newStr[j] = '\0';
  return newStr;
}

   
                                                                    
                                                                         
   
                                                                 
   
                                                                  
   
                                                                      
                                                                     
                                                                  
                                                   
   
char *
downcase_truncate_identifier(const char *ident, int len, bool warn)
{
  return downcase_identifier(ident, len, warn, true);
}

   
                                                
   
char *
downcase_identifier(const char *ident, int len, bool warn, bool truncate)
{
  char *result;
  int i;
  bool enc_is_single_byte;

  result = palloc(len + 1);
  enc_is_single_byte = pg_database_encoding_max_length() == 1;

     
                                                                          
                                                                         
                                                                           
                                                                         
                                                                           
                                                                   
                                                                       
     
  for (i = 0; i < len; i++)
  {
    unsigned char ch = (unsigned char)ident[i];

    if (ch >= 'A' && ch <= 'Z')
    {
      ch += 'a' - 'A';
    }
    else if (enc_is_single_byte && IS_HIGHBIT_SET(ch) && isupper(ch))
    {
      ch = tolower(ch);
    }
    result[i] = (char)ch;
  }
  result[i] = '\0';

  if (i >= NAMEDATALEN && truncate)
  {
    truncate_identifier(result, i, warn);
  }

  return result;
}

   
                                                                            
   
                                                                      
                        
   
                                                                         
                                        
   
void
truncate_identifier(char *ident, int len, bool warn)
{
  if (len >= NAMEDATALEN)
  {
    len = pg_mbcliplen(ident, len, NAMEDATALEN - 1);
    if (warn)
    {
         
                                                                       
                                                                      
         
      char buf[NAMEDATALEN];

      memcpy(buf, ident, len);
      buf[len] = '\0';
      ereport(NOTICE, (errcode(ERRCODE_NAME_TOO_LONG), errmsg("identifier \"%s\" will be truncated to \"%s\"", ident, buf)));
    }
    ident[len] = '\0';
  }
}

   
                                                                               
   
                                                                             
                                                               
   
                                                                             
                                     
   
bool
scanner_isspace(char ch)
{
                                                           
  if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f')
  {
    return true;
  }
  return false;
}
