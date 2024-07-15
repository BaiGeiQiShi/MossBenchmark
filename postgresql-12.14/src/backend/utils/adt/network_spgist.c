                                                                            
   
                    
                                        
   
                                                                       
                                                                        
                                                                             
                                                                         
   
                                                                         
                                                                            
   
                                                                         
                                                                             
                                                                           
                                                                       
                                                                          
                                                                             
                                                                        
                                                                        
                                          
   
                                                                         
                                                                        
   
                  
                                            
   
                                                                            
   
#include "postgres.h"

#include <sys/socket.h>

#include "access/spgist.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "utils/inet.h"

static int
inet_spg_node_number(const inet *val, int commonbits);
static int
inet_spg_consistent_bitmap(const inet *prefix, int nkeys, ScanKey scankeys, bool leaf);

   
                                      
   
Datum
inet_spg_config(PG_FUNCTION_ARGS)
{
                                                                  
  spgConfigOut *cfg = (spgConfigOut *)PG_GETARG_POINTER(1);

  cfg->prefixType = CIDROID;
  cfg->labelType = VOIDOID;
  cfg->canReturnData = true;
  cfg->longValuesOK = false;

  PG_RETURN_VOID();
}

   
                               
   
Datum
inet_spg_choose(PG_FUNCTION_ARGS)
{
  spgChooseIn *in = (spgChooseIn *)PG_GETARG_POINTER(0);
  spgChooseOut *out = (spgChooseOut *)PG_GETARG_POINTER(1);
  inet *val = DatumGetInetPP(in->datum), *prefix;
  int commonbits;

     
                                                                           
                          
     
  if (!in->hasPrefix)
  {
                                                    
    Assert(!in->allTheSame);
    Assert(in->nNodes == 2);

    out->resultType = spgMatchNode;
    out->result.matchNode.nodeN = (ip_family(val) == PGSQL_AF_INET) ? 0 : 1;
    out->result.matchNode.restDatum = InetPGetDatum(val);

    PG_RETURN_VOID();
  }

                                    
  Assert(in->nNodes == 4 || in->allTheSame);

  prefix = DatumGetInetPP(in->prefixDatum);
  commonbits = ip_bits(prefix);

     
                                                                          
                                                                       
     
  if (ip_family(val) != ip_family(prefix))
  {
                             
    out->resultType = spgSplitTuple;
    out->result.splitTuple.prefixHasPrefix = false;
    out->result.splitTuple.prefixNNodes = 2;
    out->result.splitTuple.prefixNodeLabels = NULL;

                                                         
    out->result.splitTuple.childNodeN = (ip_family(prefix) == PGSQL_AF_INET) ? 0 : 1;

    out->result.splitTuple.postfixHasPrefix = true;
    out->result.splitTuple.postfixPrefixDatum = InetPGetDatum(prefix);

    PG_RETURN_VOID();
  }

     
                                                                            
     
  if (ip_bits(val) < commonbits || bitncmp(ip_addr(prefix), ip_addr(val), commonbits) != 0)
  {
                                                         
    commonbits = bitncommon(ip_addr(prefix), ip_addr(val), Min(ip_bits(val), commonbits));

                             
    out->resultType = spgSplitTuple;
    out->result.splitTuple.prefixHasPrefix = true;
    out->result.splitTuple.prefixPrefixDatum = InetPGetDatum(cidr_set_masklen_internal(val, commonbits));
    out->result.splitTuple.prefixNNodes = 4;
    out->result.splitTuple.prefixNodeLabels = NULL;

                                                         
    out->result.splitTuple.childNodeN = inet_spg_node_number(prefix, commonbits);

    out->result.splitTuple.postfixHasPrefix = true;
    out->result.splitTuple.postfixPrefixDatum = InetPGetDatum(prefix);

    PG_RETURN_VOID();
  }

     
                                                                        
                                                                            
                                                 
     
  out->resultType = spgMatchNode;
  out->result.matchNode.nodeN = inet_spg_node_number(val, commonbits);
  out->result.matchNode.restDatum = InetPGetDatum(val);

  PG_RETURN_VOID();
}

   
                             
   
Datum
inet_spg_picksplit(PG_FUNCTION_ARGS)
{
  spgPickSplitIn *in = (spgPickSplitIn *)PG_GETARG_POINTER(0);
  spgPickSplitOut *out = (spgPickSplitOut *)PG_GETARG_POINTER(1);
  inet *prefix, *tmp;
  int i, commonbits;
  bool differentFamilies = false;

                                                 
  prefix = DatumGetInetPP(in->datums[0]);
  commonbits = ip_bits(prefix);

                                                                        
  for (i = 1; i < in->nTuples; i++)
  {
    tmp = DatumGetInetPP(in->datums[i]);

    if (ip_family(tmp) != ip_family(prefix))
    {
      differentFamilies = true;
      break;
    }

    if (ip_bits(tmp) < commonbits)
    {
      commonbits = ip_bits(tmp);
    }
    commonbits = bitncommon(ip_addr(prefix), ip_addr(tmp), commonbits);
    if (commonbits == 0)
    {
      break;
    }
  }

                                                 
  out->nodeLabels = NULL;
  out->mapTuplesToNodes = (int *)palloc(sizeof(int) * in->nTuples);
  out->leafTupleDatums = (Datum *)palloc(sizeof(Datum) * in->nTuples);

  if (differentFamilies)
  {
                             
    out->hasPrefix = false;
    out->nNodes = 2;

    for (i = 0; i < in->nTuples; i++)
    {
      tmp = DatumGetInetPP(in->datums[i]);
      out->mapTuplesToNodes[i] = (ip_family(tmp) == PGSQL_AF_INET) ? 0 : 1;
      out->leafTupleDatums[i] = InetPGetDatum(tmp);
    }
  }
  else
  {
                             
    out->hasPrefix = true;
    out->prefixDatum = InetPGetDatum(cidr_set_masklen_internal(prefix, commonbits));
    out->nNodes = 4;

    for (i = 0; i < in->nTuples; i++)
    {
      tmp = DatumGetInetPP(in->datums[i]);
      out->mapTuplesToNodes[i] = inet_spg_node_number(tmp, commonbits);
      out->leafTupleDatums[i] = InetPGetDatum(tmp);
    }
  }

  PG_RETURN_VOID();
}

   
                                                        
   
Datum
inet_spg_inner_consistent(PG_FUNCTION_ARGS)
{
  spgInnerConsistentIn *in = (spgInnerConsistentIn *)PG_GETARG_POINTER(0);
  spgInnerConsistentOut *out = (spgInnerConsistentOut *)PG_GETARG_POINTER(1);
  int i;
  int which;

  if (!in->hasPrefix)
  {
    Assert(!in->allTheSame);
    Assert(in->nNodes == 2);

                                                       
    which = 1 | (1 << 1);

    for (i = 0; i < in->nkeys; i++)
    {
      StrategyNumber strategy = in->scankeys[i].sk_strategy;
      inet *argument = DatumGetInetPP(in->scankeys[i].sk_argument);

      switch (strategy)
      {
      case RTLessStrategyNumber:
      case RTLessEqualStrategyNumber:
        if (ip_family(argument) == PGSQL_AF_INET)
        {
          which &= 1;
        }
        break;

      case RTGreaterEqualStrategyNumber:
      case RTGreaterStrategyNumber:
        if (ip_family(argument) == PGSQL_AF_INET6)
        {
          which &= (1 << 1);
        }
        break;

      case RTNotEqualStrategyNumber:
        break;

      default:
                                                               
        if (ip_family(argument) == PGSQL_AF_INET)
        {
          which &= 1;
        }
        else
        {
          which &= (1 << 1);
        }
        break;
      }
    }
  }
  else if (!in->allTheSame)
  {
    Assert(in->nNodes == 4);

                                                       
    which = inet_spg_consistent_bitmap(DatumGetInetPP(in->prefixDatum), in->nkeys, in->scankeys, false);
  }
  else
  {
                                                                       
    which = ~0;
  }

  out->nNodes = 0;

  if (which)
  {
    out->nodeNumbers = (int *)palloc(sizeof(int) * in->nNodes);

    for (i = 0; i < in->nNodes; i++)
    {
      if (which & (1 << i))
      {
        out->nodeNumbers[out->nNodes] = i;
        out->nNodes++;
      }
    }
  }

  PG_RETURN_VOID();
}

   
                                                       
   
Datum
inet_spg_leaf_consistent(PG_FUNCTION_ARGS)
{
  spgLeafConsistentIn *in = (spgLeafConsistentIn *)PG_GETARG_POINTER(0);
  spgLeafConsistentOut *out = (spgLeafConsistentOut *)PG_GETARG_POINTER(1);
  inet *leaf = DatumGetInetPP(in->leafDatum);

                            
  out->recheck = false;

                             
  out->leafValue = InetPGetDatum(leaf);

                                           
  PG_RETURN_BOOL(inet_spg_consistent_bitmap(leaf, in->nkeys, in->scankeys, true));
}

   
                                                                            
   
                                                                 
                                                                    
                                                                 
                                                                      
                              
   
static int
inet_spg_node_number(const inet *val, int commonbits)
{
  int nodeN = 0;

  if (commonbits < ip_maxbits(val) && ip_addr(val)[commonbits / 8] & (1 << (7 - commonbits % 8)))
  {
    nodeN |= 1;
  }
  if (commonbits < ip_bits(val))
  {
    nodeN |= 2;
  }

  return nodeN;
}

   
                                                                       
   
                                                                       
                                                                  
                 
   
                                                                            
                                                                     
   
static int
inet_spg_consistent_bitmap(const inet *prefix, int nkeys, ScanKey scankeys, bool leaf)
{
  int bitmap;
  int commonbits, i;

                                                        
  if (leaf)
  {
    bitmap = 1;
  }
  else
  {
    bitmap = 1 | (1 << 1) | (1 << 2) | (1 << 3);
  }

  commonbits = ip_bits(prefix);

  for (i = 0; i < nkeys; i++)
  {
    inet *argument = DatumGetInetPP(scankeys[i].sk_argument);
    StrategyNumber strategy = scankeys[i].sk_strategy;
    int order;

       
                                   
       
                                                            
       
    if (ip_family(argument) != ip_family(prefix))
    {
      switch (strategy)
      {
      case RTLessStrategyNumber:
      case RTLessEqualStrategyNumber:
        if (ip_family(argument) < ip_family(prefix))
        {
          bitmap = 0;
        }
        break;

      case RTGreaterEqualStrategyNumber:
      case RTGreaterStrategyNumber:
        if (ip_family(argument) > ip_family(prefix))
        {
          bitmap = 0;
        }
        break;

      case RTNotEqualStrategyNumber:
        break;

      default:
                                                                   
        bitmap = 0;
        break;
      }

      if (!bitmap)
      {
        break;
      }

                                                               
      continue;
    }

       
                                  
       
                                                                         
                                                                          
                                                                           
            
       
                                                                          
                                                                        
                                                                        
                                                  
       
    switch (strategy)
    {
    case RTSubStrategyNumber:
      if (commonbits <= ip_bits(argument))
      {
        bitmap &= (1 << 2) | (1 << 3);
      }
      break;

    case RTSubEqualStrategyNumber:
      if (commonbits < ip_bits(argument))
      {
        bitmap &= (1 << 2) | (1 << 3);
      }
      break;

    case RTSuperStrategyNumber:
      if (commonbits == ip_bits(argument) - 1)
      {
        bitmap &= 1 | (1 << 1);
      }
      else if (commonbits >= ip_bits(argument))
      {
        bitmap = 0;
      }
      break;

    case RTSuperEqualStrategyNumber:
      if (commonbits == ip_bits(argument))
      {
        bitmap &= 1 | (1 << 1);
      }
      else if (commonbits > ip_bits(argument))
      {
        bitmap = 0;
      }
      break;

    case RTEqualStrategyNumber:
      if (commonbits < ip_bits(argument))
      {
        bitmap &= (1 << 2) | (1 << 3);
      }
      else if (commonbits == ip_bits(argument))
      {
        bitmap &= 1 | (1 << 1);
      }
      else
      {
        bitmap = 0;
      }
      break;
    }

    if (!bitmap)
    {
      break;
    }

       
                                    
       
                                                                         
                                                                   
                                                                        
                             
       
    order = bitncmp(ip_addr(prefix), ip_addr(argument), Min(commonbits, ip_bits(argument)));

    if (order != 0)
    {
      switch (strategy)
      {
      case RTLessStrategyNumber:
      case RTLessEqualStrategyNumber:
        if (order > 0)
        {
          bitmap = 0;
        }
        break;

      case RTGreaterEqualStrategyNumber:
      case RTGreaterStrategyNumber:
        if (order < 0)
        {
          bitmap = 0;
        }
        break;

      case RTNotEqualStrategyNumber:
        break;

      default:
                                                                   
        bitmap = 0;
        break;
      }

      if (!bitmap)
      {
        break;
      }

         
                                                                      
         
      continue;
    }

       
                                 
       
                                                                         
                                     
       
                                                                         
                                    
       
    if (bitmap & ((1 << 2) | (1 << 3)) && commonbits < ip_bits(argument))
    {
      int nextbit;

      nextbit = ip_addr(argument)[commonbits / 8] & (1 << (7 - commonbits % 8));

      switch (strategy)
      {
      case RTLessStrategyNumber:
      case RTLessEqualStrategyNumber:
        if (!nextbit)
        {
          bitmap &= 1 | (1 << 1) | (1 << 2);
        }
        break;

      case RTGreaterEqualStrategyNumber:
      case RTGreaterStrategyNumber:
        if (nextbit)
        {
          bitmap &= 1 | (1 << 1) | (1 << 3);
        }
        break;

      case RTNotEqualStrategyNumber:
        break;

      default:
        if (!nextbit)
        {
          bitmap &= 1 | (1 << 1) | (1 << 2);
        }
        else
        {
          bitmap &= 1 | (1 << 1) | (1 << 3);
        }
        break;
      }

      if (!bitmap)
      {
        break;
      }
    }

       
                                                                           
                                                                          
       
    if (strategy < RTEqualStrategyNumber || strategy > RTGreaterEqualStrategyNumber)
    {
      continue;
    }

       
                                  
       
                                                                         
                                                                         
                
       
    switch (strategy)
    {
    case RTLessStrategyNumber:
    case RTLessEqualStrategyNumber:
      if (commonbits == ip_bits(argument))
      {
        bitmap &= 1 | (1 << 1);
      }
      else if (commonbits > ip_bits(argument))
      {
        bitmap = 0;
      }
      break;

    case RTGreaterEqualStrategyNumber:
    case RTGreaterStrategyNumber:
      if (commonbits < ip_bits(argument))
      {
        bitmap &= (1 << 2) | (1 << 3);
      }
      break;
    }

    if (!bitmap)
    {
      break;
    }

                                                                   
    if (commonbits != ip_bits(argument))
    {
      continue;
    }

       
                              
       
                                                                      
                                     
       
                                                                         
                                                                         
                                                                     
       
    if (!leaf && bitmap & (1 | (1 << 1)) && commonbits < ip_maxbits(argument))
    {
      int nextbit;

      nextbit = ip_addr(argument)[commonbits / 8] & (1 << (7 - commonbits % 8));

      switch (strategy)
      {
      case RTLessStrategyNumber:
      case RTLessEqualStrategyNumber:
        if (!nextbit)
        {
          bitmap &= 1 | (1 << 2) | (1 << 3);
        }
        break;

      case RTGreaterEqualStrategyNumber:
      case RTGreaterStrategyNumber:
        if (nextbit)
        {
          bitmap &= (1 << 1) | (1 << 2) | (1 << 3);
        }
        break;

      case RTNotEqualStrategyNumber:
        break;

      default:
        if (!nextbit)
        {
          bitmap &= 1 | (1 << 2) | (1 << 3);
        }
        else
        {
          bitmap &= (1 << 1) | (1 << 2) | (1 << 3);
        }
        break;
      }

      if (!bitmap)
      {
        break;
      }
    }

       
                              
       
                                                                      
                                                           
       
    if (leaf)
    {
                                                           
      order = bitncmp(ip_addr(prefix), ip_addr(argument), ip_maxbits(prefix));

      switch (strategy)
      {
      case RTLessStrategyNumber:
        if (order >= 0)
        {
          bitmap = 0;
        }
        break;

      case RTLessEqualStrategyNumber:
        if (order > 0)
        {
          bitmap = 0;
        }
        break;

      case RTEqualStrategyNumber:
        if (order != 0)
        {
          bitmap = 0;
        }
        break;

      case RTGreaterEqualStrategyNumber:
        if (order < 0)
        {
          bitmap = 0;
        }
        break;

      case RTGreaterStrategyNumber:
        if (order <= 0)
        {
          bitmap = 0;
        }
        break;

      case RTNotEqualStrategyNumber:
        if (order == 0)
        {
          bitmap = 0;
        }
        break;
      }

      if (!bitmap)
      {
        break;
      }
    }
  }

  return bitmap;
}
