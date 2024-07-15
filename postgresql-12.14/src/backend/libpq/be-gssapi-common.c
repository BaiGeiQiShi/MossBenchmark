                                                                            
   
                      
                                                            
   
                                                                         
                                                                        
   
                  
                                              
   
                                                                            
   

#include "postgres.h"

#include "libpq/be-gssapi-common.h"

   
                                                                               
                                                                 
                                                  
   
static void
pg_GSS_error_int(char *s, size_t len, OM_uint32 stat, int type)
{
  gss_buffer_desc gmsg;
  size_t i = 0;
  OM_uint32 lmin_s, msg_ctx = 0;

  do
  {
    if (gss_display_status(&lmin_s, stat, type, GSS_C_NO_OID, &msg_ctx, &gmsg) != GSS_S_COMPLETE)
    {
      break;
    }
    if (i > 0)
    {
      if (i < len)
      {
        s[i] = ' ';
      }
      i++;
    }
    if (i < len)
    {
      memcpy(s + i, gmsg.value, Min(len - i, gmsg.length));
    }
    i += gmsg.length;
    gss_release_buffer(&lmin_s, &gmsg);
  } while (msg_ctx);

                           
  if (i < len)
  {
    s[i] = '\0';
  }
  else
  {
    elog(COMMERROR, "incomplete GSS error report");
    s[len - 1] = '\0';
  }
}

   
                                                           
   
                                                                 
                                             
   
                                                                         
                                                                      
                                             
   
                                                                            
                                                                              
                    
   
void
pg_GSS_error(const char *errmsg, OM_uint32 maj_stat, OM_uint32 min_stat)
{
  char msg_major[128], msg_minor[128];

                                  
  pg_GSS_error_int(msg_major, sizeof(msg_major), maj_stat, GSS_C_GSS_CODE);

                                            
  pg_GSS_error_int(msg_minor, sizeof(msg_minor), min_stat, GSS_C_MECH_CODE);

     
                                                                       
                                          
     
  ereport(COMMERROR, (errmsg_internal("%s", errmsg), errdetail_internal("%s: %s", msg_major, msg_minor)));
}
