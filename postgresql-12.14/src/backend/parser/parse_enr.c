                                                                            
   
               
                                                                    
   
                                                                         
                                                                        
   
   
                  
                                    
   
                                                                            
   
#include "postgres.h"

#include "parser/parse_enr.h"

bool
name_matches_visible_ENR(ParseState *pstate, const char *refname)
{
  return (get_visible_ENR_metadata(pstate->p_queryEnv, refname) != NULL);
}

EphemeralNamedRelationMetadata
get_visible_ENR(ParseState *pstate, const char *refname)
{
  return get_visible_ENR_metadata(pstate->p_queryEnv, refname);
}
