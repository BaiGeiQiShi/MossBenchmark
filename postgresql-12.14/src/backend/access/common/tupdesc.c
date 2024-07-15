                                                                            
   
             
                                            
   
                                                                         
                                                                        
   
   
                  
                                         
   
         
                                                                          
                 
   
                                                                            
   

#include "postgres.h"

#include "access/htup_details.h"
#include "access/tupdesc_details.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "parser/parse_type.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/hashutils.h"
#include "utils/resowner_private.h"
#include "utils/syscache.h"

   
                           
                                                                 
   
                                                                            
                                        
   
TupleDesc
CreateTemplateTupleDesc(int natts)
{
  TupleDesc desc;

     
                   
     
  AssertArg(natts >= 0);

     
                                                                    
                     
     
                                                                        
                                                                      
                                                                        
                                                                             
                                                                             
                                                                      
                                                                  
     
  desc = (TupleDesc)palloc(offsetof(struct TupleDescData, attrs) + natts * sizeof(FormData_pg_attribute));

     
                                             
     
  desc->natts = natts;
  desc->constr = NULL;
  desc->tdtypeid = RECORDOID;
  desc->tdtypmod = -1;
  desc->tdrefcount = -1;                                   

  return desc;
}

   
                   
                                                               
                             
   
                                                                            
                                        
   
TupleDesc
CreateTupleDesc(int natts, Form_pg_attribute *attrs)
{
  TupleDesc desc;
  int i;

  desc = CreateTemplateTupleDesc(natts);

  for (i = 0; i < natts; ++i)
  {
    memcpy(TupleDescAttr(desc, i), attrs[i], ATTRIBUTE_FIXED_PART_SIZE);
  }

  return desc;
}

   
                       
                                                                      
               
   
                                                   
   
TupleDesc
CreateTupleDescCopy(TupleDesc tupdesc)
{
  TupleDesc desc;
  int i;

  desc = CreateTemplateTupleDesc(tupdesc->natts);

                                     
  memcpy(TupleDescAttr(desc, 0), TupleDescAttr(tupdesc, 0), desc->natts * sizeof(FormData_pg_attribute));

     
                                                                    
                           
     
  for (i = 0; i < desc->natts; i++)
  {
    Form_pg_attribute att = TupleDescAttr(desc, i);

    att->attnotnull = false;
    att->atthasdef = false;
    att->atthasmissing = false;
    att->attidentity = '\0';
    att->attgenerated = '\0';
  }

                                                      
  desc->tdtypeid = tupdesc->tdtypeid;
  desc->tdtypmod = tupdesc->tdtypmod;

  return desc;
}

   
                             
                                                                      
                                                        
   
TupleDesc
CreateTupleDescCopyConstr(TupleDesc tupdesc)
{
  TupleDesc desc;
  TupleConstr *constr = tupdesc->constr;
  int i;

  desc = CreateTemplateTupleDesc(tupdesc->natts);

                                     
  memcpy(TupleDescAttr(desc, 0), TupleDescAttr(tupdesc, 0), desc->natts * sizeof(FormData_pg_attribute));

                                                   
  if (constr)
  {
    TupleConstr *cpy = (TupleConstr *)palloc0(sizeof(TupleConstr));

    cpy->has_not_null = constr->has_not_null;
    cpy->has_generated_stored = constr->has_generated_stored;

    if ((cpy->num_defval = constr->num_defval) > 0)
    {
      cpy->defval = (AttrDefault *)palloc(cpy->num_defval * sizeof(AttrDefault));
      memcpy(cpy->defval, constr->defval, cpy->num_defval * sizeof(AttrDefault));
      for (i = cpy->num_defval - 1; i >= 0; i--)
      {
        if (constr->defval[i].adbin)
        {
          cpy->defval[i].adbin = pstrdup(constr->defval[i].adbin);
        }
      }
    }

    if (constr->missing)
    {
      cpy->missing = (AttrMissing *)palloc(tupdesc->natts * sizeof(AttrMissing));
      memcpy(cpy->missing, constr->missing, tupdesc->natts * sizeof(AttrMissing));
      for (i = tupdesc->natts - 1; i >= 0; i--)
      {
        if (constr->missing[i].am_present)
        {
          Form_pg_attribute attr = TupleDescAttr(tupdesc, i);

          cpy->missing[i].am_value = datumCopy(constr->missing[i].am_value, attr->attbyval, attr->attlen);
        }
      }
    }

    if ((cpy->num_check = constr->num_check) > 0)
    {
      cpy->check = (ConstrCheck *)palloc(cpy->num_check * sizeof(ConstrCheck));
      memcpy(cpy->check, constr->check, cpy->num_check * sizeof(ConstrCheck));
      for (i = cpy->num_check - 1; i >= 0; i--)
      {
        if (constr->check[i].ccname)
        {
          cpy->check[i].ccname = pstrdup(constr->check[i].ccname);
        }
        if (constr->check[i].ccbin)
        {
          cpy->check[i].ccbin = pstrdup(constr->check[i].ccbin);
        }
        cpy->check[i].ccvalid = constr->check[i].ccvalid;
        cpy->check[i].ccnoinherit = constr->check[i].ccnoinherit;
      }
    }

    desc->constr = cpy;
  }

                                                      
  desc->tdtypeid = tupdesc->tdtypeid;
  desc->tdtypmod = tupdesc->tdtypmod;

  return desc;
}

   
                 
                                                         
                                                                    
                                                    
   
                                                   
   
void
TupleDescCopy(TupleDesc dst, TupleDesc src)
{
  int i;

                                                
  memcpy(dst, src, TupleDescSize(src));

     
                                                                    
                           
     
  for (i = 0; i < dst->natts; i++)
  {
    Form_pg_attribute att = TupleDescAttr(dst, i);

    att->attnotnull = false;
    att->atthasdef = false;
    att->atthasmissing = false;
    att->attidentity = '\0';
    att->attgenerated = '\0';
  }
  dst->constr = NULL;

     
                                                                          
                                                    
     
  dst->tdrefcount = -1;
}

   
                      
                                                                     
                           
   
                                                   
   
void
TupleDescCopyEntry(TupleDesc dst, AttrNumber dstAttno, TupleDesc src, AttrNumber srcAttno)
{
  Form_pg_attribute dstAtt = TupleDescAttr(dst, dstAttno - 1);
  Form_pg_attribute srcAtt = TupleDescAttr(src, srcAttno - 1);

     
                   
     
  AssertArg(PointerIsValid(src));
  AssertArg(PointerIsValid(dst));
  AssertArg(srcAttno >= 1);
  AssertArg(srcAttno <= src->natts);
  AssertArg(dstAttno >= 1);
  AssertArg(dstAttno <= dst->natts);

  memcpy(dstAtt, srcAtt, ATTRIBUTE_FIXED_PART_SIZE);

     
                                                                   
     
                                                                             
                                                                          
                                                                             
                                                                          
                                            
     
  dstAtt->attnum = dstAttno;
  dstAtt->attcacheoff = -1;

                                                                    
  dstAtt->attnotnull = false;
  dstAtt->atthasdef = false;
  dstAtt->atthasmissing = false;
  dstAtt->attidentity = '\0';
  dstAtt->attgenerated = '\0';
}

   
                                               
   
void
FreeTupleDesc(TupleDesc tupdesc)
{
  int i;

     
                                                                       
                                        
     
  Assert(tupdesc->tdrefcount <= 0);

  if (tupdesc->constr)
  {
    if (tupdesc->constr->num_defval > 0)
    {
      AttrDefault *attrdef = tupdesc->constr->defval;

      for (i = tupdesc->constr->num_defval - 1; i >= 0; i--)
      {
        if (attrdef[i].adbin)
        {
          pfree(attrdef[i].adbin);
        }
      }
      pfree(attrdef);
    }
    if (tupdesc->constr->missing)
    {
      AttrMissing *attrmiss = tupdesc->constr->missing;

      for (i = tupdesc->natts - 1; i >= 0; i--)
      {
        if (attrmiss[i].am_present && !TupleDescAttr(tupdesc, i)->attbyval)
        {
          pfree(DatumGetPointer(attrmiss[i].am_value));
        }
      }
      pfree(attrmiss);
    }
    if (tupdesc->constr->num_check > 0)
    {
      ConstrCheck *check = tupdesc->constr->check;

      for (i = tupdesc->constr->num_check - 1; i >= 0; i--)
      {
        if (check[i].ccname)
        {
          pfree(check[i].ccname);
        }
        if (check[i].ccbin)
        {
          pfree(check[i].ccbin);
        }
      }
      pfree(check);
    }
    pfree(tupdesc->constr);
  }

  pfree(tupdesc);
}

   
                                                                        
                         
   
                                                                          
                                                         
   
void
IncrTupleDescRefCount(TupleDesc tupdesc)
{
  Assert(tupdesc->tdrefcount >= 0);

  ResourceOwnerEnlargeTupleDescs(CurrentResourceOwner);
  tupdesc->tdrefcount++;
  ResourceOwnerRememberTupleDesc(CurrentResourceOwner, tupdesc);
}

   
                                                                        
                                                                        
                      
   
                                                                          
                                                             
   
void
DecrTupleDescRefCount(TupleDesc tupdesc)
{
  Assert(tupdesc->tdrefcount > 0);

  ResourceOwnerForgetTupleDesc(CurrentResourceOwner, tupdesc);
  if (--tupdesc->tdrefcount == 0)
  {
    FreeTupleDesc(tupdesc);
  }
}

   
                                                         
   
                                                                        
                                                                             
                                                                    
                                        
   
bool
equalTupleDescs(TupleDesc tupdesc1, TupleDesc tupdesc2)
{
  int i, j, n;

  if (tupdesc1->natts != tupdesc2->natts)
  {
    return false;
  }
  if (tupdesc1->tdtypeid != tupdesc2->tdtypeid)
  {
    return false;
  }

  for (i = 0; i < tupdesc1->natts; i++)
  {
    Form_pg_attribute attr1 = TupleDescAttr(tupdesc1, i);
    Form_pg_attribute attr2 = TupleDescAttr(tupdesc2, i);

       
                                                                         
                                                                          
                                                                        
                                                                       
                                                                       
                                                                         
                                                    
       
                                                                           
               
       
    if (strcmp(NameStr(attr1->attname), NameStr(attr2->attname)) != 0)
    {
      return false;
    }
    if (attr1->atttypid != attr2->atttypid)
    {
      return false;
    }
    if (attr1->attstattarget != attr2->attstattarget)
    {
      return false;
    }
    if (attr1->attlen != attr2->attlen)
    {
      return false;
    }
    if (attr1->attndims != attr2->attndims)
    {
      return false;
    }
    if (attr1->atttypmod != attr2->atttypmod)
    {
      return false;
    }
    if (attr1->attbyval != attr2->attbyval)
    {
      return false;
    }
    if (attr1->attstorage != attr2->attstorage)
    {
      return false;
    }
    if (attr1->attalign != attr2->attalign)
    {
      return false;
    }
    if (attr1->attnotnull != attr2->attnotnull)
    {
      return false;
    }
    if (attr1->atthasdef != attr2->atthasdef)
    {
      return false;
    }
    if (attr1->attidentity != attr2->attidentity)
    {
      return false;
    }
    if (attr1->attgenerated != attr2->attgenerated)
    {
      return false;
    }
    if (attr1->attisdropped != attr2->attisdropped)
    {
      return false;
    }
    if (attr1->attislocal != attr2->attislocal)
    {
      return false;
    }
    if (attr1->attinhcount != attr2->attinhcount)
    {
      return false;
    }
    if (attr1->attcollation != attr2->attcollation)
    {
      return false;
    }
                                                                      
  }

  if (tupdesc1->constr != NULL)
  {
    TupleConstr *constr1 = tupdesc1->constr;
    TupleConstr *constr2 = tupdesc2->constr;

    if (constr2 == NULL)
    {
      return false;
    }
    if (constr1->has_not_null != constr2->has_not_null)
    {
      return false;
    }
    if (constr1->has_generated_stored != constr2->has_generated_stored)
    {
      return false;
    }
    n = constr1->num_defval;
    if (n != (int)constr2->num_defval)
    {
      return false;
    }
    for (i = 0; i < n; i++)
    {
      AttrDefault *defval1 = constr1->defval + i;
      AttrDefault *defval2 = constr2->defval;

         
                                                                        
                                                                        
                                       
         
      for (j = 0; j < n; defval2++, j++)
      {
        if (defval1->adnum == defval2->adnum)
        {
          break;
        }
      }
      if (j >= n)
      {
        return false;
      }
      if (strcmp(defval1->adbin, defval2->adbin) != 0)
      {
        return false;
      }
    }
    if (constr1->missing)
    {
      if (!constr2->missing)
      {
        return false;
      }
      for (i = 0; i < tupdesc1->natts; i++)
      {
        AttrMissing *missval1 = constr1->missing + i;
        AttrMissing *missval2 = constr2->missing + i;

        if (missval1->am_present != missval2->am_present)
        {
          return false;
        }
        if (missval1->am_present)
        {
          Form_pg_attribute missatt1 = TupleDescAttr(tupdesc1, i);

          if (!datumIsEqual(missval1->am_value, missval2->am_value, missatt1->attbyval, missatt1->attlen))
          {
            return false;
          }
        }
      }
    }
    else if (constr2->missing)
    {
      return false;
    }
    n = constr1->num_check;
    if (n != (int)constr2->num_check)
    {
      return false;
    }
    for (i = 0; i < n; i++)
    {
      ConstrCheck *check1 = constr1->check + i;
      ConstrCheck *check2 = constr2->check;

         
                                                                        
                                                                   
                                     
         
      for (j = 0; j < n; check2++, j++)
      {
        if (strcmp(check1->ccname, check2->ccname) == 0 && strcmp(check1->ccbin, check2->ccbin) == 0 && check1->ccvalid == check2->ccvalid && check1->ccnoinherit == check2->ccnoinherit)
        {
          break;
        }
      }
      if (j >= n)
      {
        return false;
      }
    }
  }
  else if (tupdesc2->constr != NULL)
  {
    return false;
  }
  return true;
}

   
                 
                                                 
   
                                                                           
                                                                   
   
                                                                             
                                                                         
   
uint32
hashTupleDesc(TupleDesc desc)
{
  uint32 s;
  int i;

  s = hash_combine(0, hash_uint32(desc->natts));
  s = hash_combine(s, hash_uint32(desc->tdtypeid));
  for (i = 0; i < desc->natts; ++i)
  {
    s = hash_combine(s, hash_uint32(TupleDescAttr(desc, i)->atttypid));
  }

  return s;
}

   
                      
                                                              
                                             
   
                                                                         
                                                                         
                                                                              
                                                                           
                                                             
   
                                                                            
                                                                   
                                
   
void
TupleDescInitEntry(TupleDesc desc, AttrNumber attributeNumber, const char *attributeName, Oid oidtypeid, int32 typmod, int attdim)
{
  HeapTuple tuple;
  Form_pg_type typeForm;
  Form_pg_attribute att;

     
                   
     
  AssertArg(PointerIsValid(desc));
  AssertArg(attributeNumber >= 1);
  AssertArg(attributeNumber <= desc->natts);

     
                                     
     
  att = TupleDescAttr(desc, attributeNumber - 1);

  att->attrelid = 0;                  

     
                                                                         
                                                                           
                                                                             
     
  if (attributeName == NULL)
  {
    MemSet(NameStr(att->attname), 0, NAMEDATALEN);
  }
  else if (attributeName != NameStr(att->attname))
  {
    namestrcpy(&(att->attname), attributeName);
  }

  att->attstattarget = -1;
  att->attcacheoff = -1;
  att->atttypmod = typmod;

  att->attnum = attributeNumber;
  att->attndims = attdim;

  att->attnotnull = false;
  att->atthasdef = false;
  att->atthasmissing = false;
  att->attidentity = '\0';
  att->attgenerated = '\0';
  att->attisdropped = false;
  att->attislocal = true;
  att->attinhcount = 0;
                                                                          

  tuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(oidtypeid));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for type %u", oidtypeid);
  }
  typeForm = (Form_pg_type)GETSTRUCT(tuple);

  att->atttypid = oidtypeid;
  att->attlen = typeForm->typlen;
  att->attbyval = typeForm->typbyval;
  att->attalign = typeForm->typalign;
  att->attstorage = typeForm->typstorage;
  att->attcollation = typeForm->typcollation;

  ReleaseSysCache(tuple);
}

   
                             
                                                                
                                                    
   
void
TupleDescInitBuiltinEntry(TupleDesc desc, AttrNumber attributeNumber, const char *attributeName, Oid oidtypeid, int32 typmod, int attdim)
{
  Form_pg_attribute att;

                     
  AssertArg(PointerIsValid(desc));
  AssertArg(attributeNumber >= 1);
  AssertArg(attributeNumber <= desc->natts);

                                       
  att = TupleDescAttr(desc, attributeNumber - 1);
  att->attrelid = 0;                  

                                                               
  Assert(attributeName != NULL);
  namestrcpy(&(att->attname), attributeName);

  att->attstattarget = -1;
  att->attcacheoff = -1;
  att->atttypmod = typmod;

  att->attnum = attributeNumber;
  att->attndims = attdim;

  att->attnotnull = false;
  att->atthasdef = false;
  att->atthasmissing = false;
  att->attidentity = '\0';
  att->attgenerated = '\0';
  att->attisdropped = false;
  att->attislocal = true;
  att->attinhcount = 0;
                                                                          

  att->atttypid = oidtypeid;

     
                                                                        
                                                                           
                                                                    
     
  switch (oidtypeid)
  {
  case TEXTOID:
  case TEXTARRAYOID:
    att->attlen = -1;
    att->attbyval = false;
    att->attalign = 'i';
    att->attstorage = 'x';
    att->attcollation = DEFAULT_COLLATION_OID;
    break;

  case BOOLOID:
    att->attlen = 1;
    att->attbyval = true;
    att->attalign = 'c';
    att->attstorage = 'p';
    att->attcollation = InvalidOid;
    break;

  case INT4OID:
    att->attlen = 4;
    att->attbyval = true;
    att->attalign = 'i';
    att->attstorage = 'p';
    att->attcollation = InvalidOid;
    break;

  case INT8OID:
    att->attlen = 8;
    att->attbyval = FLOAT8PASSBYVAL;
    att->attalign = 'd';
    att->attstorage = 'p';
    att->attcollation = InvalidOid;
    break;

  default:
    elog(ERROR, "unsupported type %u", oidtypeid);
  }
}

   
                               
   
                                                                              
          
   
void
TupleDescInitEntryCollation(TupleDesc desc, AttrNumber attributeNumber, Oid collationid)
{
     
                   
     
  AssertArg(PointerIsValid(desc));
  AssertArg(attributeNumber >= 1);
  AssertArg(attributeNumber <= desc->natts);

  TupleDescAttr(desc, attributeNumber - 1)->attcollation = collationid;
}

   
                        
   
                                                                         
   
                                                                           
                                                                         
             
   
TupleDesc
BuildDescForRelation(List *schema)
{
  int natts;
  AttrNumber attnum;
  ListCell *l;
  TupleDesc desc;
  bool has_not_null;
  char *attname;
  Oid atttypid;
  int32 atttypmod;
  Oid attcollation;
  int attdim;

     
                                     
     
  natts = list_length(schema);
  desc = CreateTemplateTupleDesc(natts);
  has_not_null = false;

  attnum = 0;

  foreach (l, schema)
  {
    ColumnDef *entry = lfirst(l);
    AclResult aclresult;
    Form_pg_attribute att;

       
                                                                          
                                                                  
                            
       
    attnum++;

    attname = entry->colname;
    typenameTypeIdAndMod(NULL, entry->typeName, &atttypid, &atttypmod);

    aclresult = pg_type_aclcheck(atttypid, GetUserId(), ACL_USAGE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error_type(aclresult, atttypid);
    }

    attcollation = GetColumnDefCollation(NULL, entry, atttypid);
    attdim = list_length(entry->typeName->arrayBounds);

    if (entry->typeName->setof)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("column \"%s\" cannot be declared SETOF", attname)));
    }

    TupleDescInitEntry(desc, attnum, attname, atttypid, atttypmod, attdim);
    att = TupleDescAttr(desc, attnum - 1);

                                                             
    TupleDescInitEntryCollation(desc, attnum, attcollation);
    if (entry->storage)
    {
      att->attstorage = entry->storage;
    }

                                                                    
    att->attnotnull = entry->is_not_null;
    has_not_null |= entry->is_not_null;
    att->attislocal = entry->is_local;
    att->attinhcount = entry->inhcount;
  }

  if (has_not_null)
  {
    TupleConstr *constr = (TupleConstr *)palloc0(sizeof(TupleConstr));

    constr->has_not_null = true;
    constr->has_generated_stored = false;
    constr->defval = NULL;
    constr->missing = NULL;
    constr->num_defval = 0;
    constr->check = NULL;
    constr->num_check = 0;
    desc->constr = constr;
  }
  else
  {
    desc->constr = NULL;
  }

  return desc;
}

   
                      
   
                                                                    
                                                  
   
                                 
   
                                                                          
                                    
   
TupleDesc
BuildDescFromLists(List *names, List *types, List *typmods, List *collations)
{
  int natts;
  AttrNumber attnum;
  ListCell *l1;
  ListCell *l2;
  ListCell *l3;
  ListCell *l4;
  TupleDesc desc;

  natts = list_length(names);
  Assert(natts == list_length(types));
  Assert(natts == list_length(typmods));
  Assert(natts == list_length(collations));

     
                                     
     
  desc = CreateTemplateTupleDesc(natts);

  attnum = 0;
  forfour(l1, names, l2, types, l3, typmods, l4, collations)
  {
    char *attname = strVal(lfirst(l1));
    Oid atttypid = lfirst_oid(l2);
    int32 atttypmod = lfirst_int(l3);
    Oid attcollation = lfirst_oid(l4);

    attnum++;

    TupleDescInitEntry(desc, attnum, attname, atttypid, atttypmod, 0);
    TupleDescInitEntryCollation(desc, attnum, attcollation);
  }

  return desc;
}
