                                                                            
   
            
                                                                 
   
   
                                                                         
                                                                        
   
                  
                                
   
                                                                            
   

#include "postgres.h"

#include "nodes/bitmapset.h"
#include "nodes/params.h"
#include "storage/shmem.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"

   
                                                          
   
                                                                          
                                         
   
ParamListInfo
makeParamList(int numParams)
{
  ParamListInfo retval;
  Size size;

  size = offsetof(ParamListInfoData, params) + numParams * sizeof(ParamExternData);

  retval = (ParamListInfo)palloc(size);
  retval->paramFetch = NULL;
  retval->paramFetchArg = NULL;
  retval->paramCompile = NULL;
  retval->paramCompileArg = NULL;
  retval->parserSetup = NULL;
  retval->parserSetupArg = NULL;
  retval->numParams = numParams;

  return retval;
}

   
                                   
   
                                                    
   
                                                                         
                                                                        
                                                                        
                                                                         
   
ParamListInfo
copyParamList(ParamListInfo from)
{
  ParamListInfo retval;

  if (from == NULL || from->numParams <= 0)
  {
    return NULL;
  }

  retval = makeParamList(from->numParams);

  for (int i = 0; i < from->numParams; i++)
  {
    ParamExternData *oprm;
    ParamExternData *nprm = &retval->params[i];
    ParamExternData prmdata;
    int16 typLen;
    bool typByVal;

                                                         
    if (from->paramFetch != NULL)
    {
      oprm = from->paramFetch(from, i + 1, false, &prmdata);
    }
    else
    {
      oprm = &from->params[i];
    }

                                      
    *nprm = *oprm;

                                                                  
    if (nprm->isnull || !OidIsValid(nprm->ptype))
    {
      continue;
    }
    get_typlenbyval(nprm->ptype, &typLen, &typByVal);
    nprm->value = datumCopy(nprm->value, typByVal, typLen);
  }

  return retval;
}

   
                                                                       
   
Size
EstimateParamListSpace(ParamListInfo paramLI)
{
  int i;
  Size sz = sizeof(int);

  if (paramLI == NULL || paramLI->numParams <= 0)
  {
    return sz;
  }

  for (i = 0; i < paramLI->numParams; i++)
  {
    ParamExternData *prm;
    ParamExternData prmdata;
    Oid typeOid;
    int16 typLen;
    bool typByVal;

                                                         
    if (paramLI->paramFetch != NULL)
    {
      prm = paramLI->paramFetch(paramLI, i + 1, false, &prmdata);
    }
    else
    {
      prm = &paramLI->params[i];
    }

    typeOid = prm->ptype;

    sz = add_size(sz, sizeof(Oid));                            
    sz = add_size(sz, sizeof(uint16));                       

                                
    if (OidIsValid(typeOid))
    {
      get_typlenbyval(typeOid, &typLen, &typByVal);
    }
    else
    {
                                                                     
      typLen = sizeof(Datum);
      typByVal = true;
    }
    sz = add_size(sz, datumEstimateSpace(prm->value, prm->isnull, typByVal, typLen));
  }

  return sz;
}

   
                                                                     
   
                                                                          
                                                                             
                                                                         
                                                                           
                                                                          
                                                                            
                                                                              
            
   
                                                                         
                                                                         
                                       
   
void
SerializeParamList(ParamListInfo paramLI, char **start_address)
{
  int nparams;
  int i;

                                   
  if (paramLI == NULL || paramLI->numParams <= 0)
  {
    nparams = 0;
  }
  else
  {
    nparams = paramLI->numParams;
  }
  memcpy(*start_address, &nparams, sizeof(int));
  *start_address += sizeof(int);

                                     
  for (i = 0; i < nparams; i++)
  {
    ParamExternData *prm;
    ParamExternData prmdata;
    Oid typeOid;
    int16 typLen;
    bool typByVal;

                                                         
    if (paramLI->paramFetch != NULL)
    {
      prm = paramLI->paramFetch(paramLI, i + 1, false, &prmdata);
    }
    else
    {
      prm = &paramLI->params[i];
    }

    typeOid = prm->ptype;

                         
    memcpy(*start_address, &typeOid, sizeof(Oid));
    *start_address += sizeof(Oid);

                      
    memcpy(*start_address, &prm->pflags, sizeof(uint16));
    *start_address += sizeof(uint16);

                             
    if (OidIsValid(typeOid))
    {
      get_typlenbyval(typeOid, &typLen, &typByVal);
    }
    else
    {
                                                                     
      typLen = sizeof(Datum);
      typByVal = true;
    }
    datumSerialize(prm->value, prm->isnull, typByVal, typLen, start_address);
  }
}

   
                                   
   
                                                    
   
                                                                         
                                                                        
                                                                        
                                                                         
   
ParamListInfo
RestoreParamList(char **start_address)
{
  ParamListInfo paramLI;
  int nparams;

  memcpy(&nparams, *start_address, sizeof(int));
  *start_address += sizeof(int);

  paramLI = makeParamList(nparams);

  for (int i = 0; i < nparams; i++)
  {
    ParamExternData *prm = &paramLI->params[i];

                        
    memcpy(&prm->ptype, *start_address, sizeof(Oid));
    *start_address += sizeof(Oid);

                     
    memcpy(&prm->pflags, *start_address, sizeof(uint16));
    *start_address += sizeof(uint16);

                            
    prm->value = datumRestore(start_address, &prm->isnull);
  }

  return paramLI;
}
