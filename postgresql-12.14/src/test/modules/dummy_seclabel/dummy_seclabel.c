   
                    
   
                                  
   
                                                                    
                                                                               
                          
   
                                                                         
                                                                        
   
#include "postgres.h"

#include "commands/seclabel.h"
#include "miscadmin.h"
#include "utils/rel.h"

PG_MODULE_MAGIC;

                              
void
_PG_init(void);

PG_FUNCTION_INFO_V1(dummy_seclabel_dummy);

static void
dummy_object_relabel(const ObjectAddress *object, const char *seclabel)
{
  if (seclabel == NULL || strcmp(seclabel, "unclassified") == 0 || strcmp(seclabel, "classified") == 0)
  {
    return;
  }

  if (strcmp(seclabel, "secret") == 0 || strcmp(seclabel, "top secret") == 0)
  {
    if (!superuser())
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("only superuser can set '%s' label", seclabel)));
    }
    return;
  }
  ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("'%s' is not a valid security label", seclabel)));
}

void
_PG_init(void)
{
  register_label_provider("dummy", dummy_object_relabel);
}

   
                                                                            
                                                                 
   
Datum
dummy_seclabel_dummy(PG_FUNCTION_ARGS)
{
  PG_RETURN_VOID();
}
