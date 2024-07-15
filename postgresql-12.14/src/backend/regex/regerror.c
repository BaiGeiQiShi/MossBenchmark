   
                                   
   
                                                                 
   
                                                                            
                                                                            
                                                                          
                       
   
                                                                        
                                                                
                                                                          
                                                        
   
                                                                           
                                                             
   
                                                                              
                                                                            
                                                                           
                                                                          
                                                                       
                                                                               
                                                                            
                                                                           
                                                                          
                                              
   
                                
   
   

#include "regex/regguts.h"

                               
static const char unk[] = "*** unknown regex error code 0x%x ***";

                                                             
static const struct rerr
{
  int code;
  const char *name;
  const char *explain;
} rerrs[] =

    {
                                            
#include "regex/regerrs.h"                         
        {-1, "", "oops"},                                         
};

   
                                                
   
              
size_t                                                            
pg_regerror(int errcode,                                          
    const regex_t *preg,                                             
    char *errbuf,                                                   
    size_t errbuf_size)                                           
{
  const struct rerr *r;
  const char *msg;
  char convbuf[sizeof(unk) + 50];                          
  size_t len;
  int icode;

  switch (errcode)
  {
  case REG_ATOI:                             
    for (r = rerrs; r->code >= 0; r++)
    {
      if (strcmp(r->name, errbuf) == 0)
      {
        break;
      }
    }
    sprintf(convbuf, "%d", r->code);                     
    msg = convbuf;
    break;
  case REG_ITOA:                                      
    icode = atoi(errbuf);                                    
    for (r = rerrs; r->code >= 0; r++)
    {
      if (r->code == icode)
      {
        break;
      }
    }
    if (r->code >= 0)
    {
      msg = r->name;
    }
    else
    {                                   
      sprintf(convbuf, "REG_%u", (unsigned)icode);
      msg = convbuf;
    }
    break;
  default:                                
    for (r = rerrs; r->code >= 0; r++)
    {
      if (r->code == errcode)
      {
        break;
      }
    }
    if (r->code >= 0)
    {
      msg = r->explain;
    }
    else
    {                      
      sprintf(convbuf, unk, errcode);
      msg = convbuf;
    }
    break;
  }

  len = strlen(msg) + 1;                                  
  if (errbuf_size > 0)
  {
    if (errbuf_size > len)
    {
      strcpy(errbuf, msg);
    }
    else
    {                      
      memcpy(errbuf, msg, errbuf_size - 1);
      errbuf[errbuf_size - 1] = '\0';
    }
  }

  return len;
}
