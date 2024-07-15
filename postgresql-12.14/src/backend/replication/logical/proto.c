                                                                            
   
           
                                           
   
                                                                
   
                  
                                            
   
                                                                            
   
#include "postgres.h"

#include "access/sysattr.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_type.h"
#include "libpq/pqformat.h"
#include "replication/logicalproto.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

   
                           
   
#define LOGICALREP_IS_REPLICA_IDENTITY 1

#define TRUNCATE_CASCADE (1 << 0)
#define TRUNCATE_RESTART_SEQS (1 << 1)

static void
logicalrep_write_attrs(StringInfo out, Relation rel);
static void
logicalrep_write_tuple(StringInfo out, Relation rel, HeapTuple tuple);

static void
logicalrep_read_attrs(StringInfo in, LogicalRepRelation *rel);
static void
logicalrep_read_tuple(StringInfo in, LogicalRepTupleData *tuple);

static void
logicalrep_write_namespace(StringInfo out, Oid nspid);
static const char *
logicalrep_read_namespace(StringInfo in);

   
                                     
   
void
logicalrep_write_begin(StringInfo out, ReorderBufferTXN *txn)
{
  pq_sendbyte(out, 'B');            

                    
  pq_sendint64(out, txn->final_lsn);
  pq_sendint64(out, txn->commit_time);
  pq_sendint32(out, txn->xid);
}

   
                                           
   
void
logicalrep_read_begin(StringInfo in, LogicalRepBeginData *begin_data)
{
                   
  begin_data->final_lsn = pq_getmsgint64(in);
  if (begin_data->final_lsn == InvalidXLogRecPtr)
  {
    elog(ERROR, "final_lsn not set in begin message");
  }
  begin_data->committime = pq_getmsgint64(in);
  begin_data->xid = pq_getmsgint(in, 4);
}

   
                                      
   
void
logicalrep_write_commit(StringInfo out, ReorderBufferTXN *txn, XLogRecPtr commit_lsn)
{
  uint8 flags = 0;

  pq_sendbyte(out, 'C');                     

                                             
  pq_sendbyte(out, flags);

                   
  pq_sendint64(out, commit_lsn);
  pq_sendint64(out, txn->end_lsn);
  pq_sendint64(out, txn->commit_time);
}

   
                                            
   
void
logicalrep_read_commit(StringInfo in, LogicalRepCommitData *commit_data)
{
                                   
  uint8 flags = pq_getmsgbyte(in);

  if (flags != 0)
  {
    elog(ERROR, "unrecognized flags %u in commit message", flags);
  }

                   
  commit_data->commit_lsn = pq_getmsgint64(in);
  commit_data->end_lsn = pq_getmsgint64(in);
  commit_data->committime = pq_getmsgint64(in);
}

   
                                      
   
void
logicalrep_write_origin(StringInfo out, const char *origin, XLogRecPtr origin_lsn)
{
  pq_sendbyte(out, 'O');             

                    
  pq_sendint64(out, origin_lsn);

                     
  pq_sendstring(out, origin);
}

   
                                       
   
char *
logicalrep_read_origin(StringInfo in, XLogRecPtr *origin_lsn)
{
                    
  *origin_lsn = pq_getmsgint64(in);

                     
  return pstrdup(pq_getmsgstring(in));
}

   
                                      
   
void
logicalrep_write_insert(StringInfo out, Relation rel, HeapTuple newtuple)
{
  pq_sendbyte(out, 'I');                    

                                      
  pq_sendint32(out, RelationGetRelid(rel));

  pq_sendbyte(out, 'N');                        
  logicalrep_write_tuple(out, rel, newtuple);
}

   
                            
   
                        
   
LogicalRepRelId
logicalrep_read_insert(StringInfo in, LogicalRepTupleData *newtup)
{
  char action;
  LogicalRepRelId relid;

                            
  relid = pq_getmsgint(in, 4);

  action = pq_getmsgbyte(in);
  if (action != 'N')
  {
    elog(ERROR, "expected new tuple but got %d", action);
  }

  logicalrep_read_tuple(in, newtup);

  return relid;
}

   
                                      
   
void
logicalrep_write_update(StringInfo out, Relation rel, HeapTuple oldtuple, HeapTuple newtuple)
{
  pq_sendbyte(out, 'U');                    

  Assert(rel->rd_rel->relreplident == REPLICA_IDENTITY_DEFAULT || rel->rd_rel->relreplident == REPLICA_IDENTITY_FULL || rel->rd_rel->relreplident == REPLICA_IDENTITY_INDEX);

                                      
  pq_sendint32(out, RelationGetRelid(rel));

  if (oldtuple != NULL)
  {
    if (rel->rd_rel->relreplident == REPLICA_IDENTITY_FULL)
    {
      pq_sendbyte(out, 'O');                        
    }
    else
    {
      pq_sendbyte(out, 'K');                      
    }
    logicalrep_write_tuple(out, rel, oldtuple);
  }

  pq_sendbyte(out, 'N');                        
  logicalrep_write_tuple(out, rel, newtuple);
}

   
                            
   
LogicalRepRelId
logicalrep_read_update(StringInfo in, bool *has_oldtuple, LogicalRepTupleData *oldtup, LogicalRepTupleData *newtup)
{
  char action;
  LogicalRepRelId relid;

                            
  relid = pq_getmsgint(in, 4);

                              
  action = pq_getmsgbyte(in);
  if (action != 'K' && action != 'O' && action != 'N')
  {
    elog(ERROR, "expected action 'N', 'O' or 'K', got %c", action);
  }

                           
  if (action == 'K' || action == 'O')
  {
    logicalrep_read_tuple(in, oldtup);
    *has_oldtuple = true;

    action = pq_getmsgbyte(in);
  }
  else
  {
    *has_oldtuple = false;
  }

                            
  if (action != 'N')
  {
    elog(ERROR, "expected action 'N', got %c", action);
  }

  logicalrep_read_tuple(in, newtup);

  return relid;
}

   
                                      
   
void
logicalrep_write_delete(StringInfo out, Relation rel, HeapTuple oldtuple)
{
  Assert(rel->rd_rel->relreplident == REPLICA_IDENTITY_DEFAULT || rel->rd_rel->relreplident == REPLICA_IDENTITY_FULL || rel->rd_rel->relreplident == REPLICA_IDENTITY_INDEX);

  pq_sendbyte(out, 'D');                    

                                      
  pq_sendint32(out, RelationGetRelid(rel));

  if (rel->rd_rel->relreplident == REPLICA_IDENTITY_FULL)
  {
    pq_sendbyte(out, 'O');                        
  }
  else
  {
    pq_sendbyte(out, 'K');                      
  }

  logicalrep_write_tuple(out, rel, oldtuple);
}

   
                            
   
                        
   
LogicalRepRelId
logicalrep_read_delete(StringInfo in, LogicalRepTupleData *oldtup)
{
  char action;
  LogicalRepRelId relid;

                            
  relid = pq_getmsgint(in, 4);

                              
  action = pq_getmsgbyte(in);
  if (action != 'K' && action != 'O')
  {
    elog(ERROR, "expected action 'O' or 'K', got %c", action);
  }

  logicalrep_read_tuple(in, oldtup);

  return relid;
}

   
                                        
   
void
logicalrep_write_truncate(StringInfo out, int nrelids, Oid relids[], bool cascade, bool restart_seqs)
{
  int i;
  uint8 flags = 0;

  pq_sendbyte(out, 'T');                      

  pq_sendint32(out, nrelids);

                                      
  if (cascade)
  {
    flags |= TRUNCATE_CASCADE;
  }
  if (restart_seqs)
  {
    flags |= TRUNCATE_RESTART_SEQS;
  }
  pq_sendint8(out, flags);

  for (i = 0; i < nrelids; i++)
  {
    pq_sendint32(out, relids[i]);
  }
}

   
                              
   
List *
logicalrep_read_truncate(StringInfo in, bool *cascade, bool *restart_seqs)
{
  int i;
  int nrelids;
  List *relids = NIL;
  uint8 flags;

  nrelids = pq_getmsgint(in, 4);

                                      
  flags = pq_getmsgint(in, 1);
  *cascade = (flags & TRUNCATE_CASCADE) > 0;
  *restart_seqs = (flags & TRUNCATE_RESTART_SEQS) > 0;

  for (i = 0; i < nrelids; i++)
  {
    relids = lappend_oid(relids, pq_getmsgint(in, 4));
  }

  return relids;
}

   
                                                    
   
void
logicalrep_write_rel(StringInfo out, Relation rel)
{
  char *relname;

  pq_sendbyte(out, 'R');                       

                                      
  pq_sendint32(out, RelationGetRelid(rel));

                                    
  logicalrep_write_namespace(out, RelationGetNamespace(rel));
  relname = RelationGetRelationName(rel);
  pq_sendstring(out, relname);

                             
  pq_sendbyte(out, rel->rd_rel->relreplident);

                               
  logicalrep_write_attrs(out, rel);
}

   
                                                                        
   
LogicalRepRelation *
logicalrep_read_rel(StringInfo in)
{
  LogicalRepRelation *rel = palloc(sizeof(LogicalRepRelation));

  rel->remoteid = pq_getmsgint(in, 4);

                                      
  rel->nspname = pstrdup(logicalrep_read_namespace(in));
  rel->relname = pstrdup(pq_getmsgstring(in));

                                  
  rel->replident = pq_getmsgbyte(in);

                                 
  logicalrep_read_attrs(in, rel);

  return rel;
}

   
                                         
   
                                                   
   
void
logicalrep_write_typ(StringInfo out, Oid typoid)
{
  Oid basetypoid = getBaseType(typoid);
  HeapTuple tup;
  Form_pg_type typtup;

  pq_sendbyte(out, 'Y');                   

  tup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(basetypoid));
  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for type %u", basetypoid);
  }
  typtup = (Form_pg_type)GETSTRUCT(tup);

                                      
  pq_sendint32(out, typoid);

                                
  logicalrep_write_namespace(out, typtup->typnamespace);
  pq_sendstring(out, NameStr(typtup->typname));

  ReleaseSysCache(tup);
}

   
                                          
   
void
logicalrep_read_typ(StringInfo in, LogicalRepTyp *ltyp)
{
  ltyp->remoteid = pq_getmsgint(in, 4);

                                  
  ltyp->nspname = pstrdup(logicalrep_read_namespace(in));
  ltyp->typname = pstrdup(pq_getmsgstring(in));
}

   
                                                                             
   
static void
logicalrep_write_tuple(StringInfo out, Relation rel, HeapTuple tuple)
{
  TupleDesc desc;
  Datum values[MaxTupleAttributeNumber];
  bool isnull[MaxTupleAttributeNumber];
  int i;
  uint16 nliveatts = 0;

  desc = RelationGetDescr(rel);

  for (i = 0; i < desc->natts; i++)
  {
    if (TupleDescAttr(desc, i)->attisdropped || TupleDescAttr(desc, i)->attgenerated)
    {
      continue;
    }
    nliveatts++;
  }
  pq_sendint16(out, nliveatts);

                                                     
  enlargeStringInfo(out, tuple->t_len + nliveatts * (1 + 4));

  heap_deform_tuple(tuple, desc, values, isnull);

                        
  for (i = 0; i < desc->natts; i++)
  {
    HeapTuple typtup;
    Form_pg_type typclass;
    Form_pg_attribute att = TupleDescAttr(desc, i);
    char *outputstr;

    if (att->attisdropped || att->attgenerated)
    {
      continue;
    }

    if (isnull[i])
    {
      pq_sendbyte(out, 'n');                  
      continue;
    }
    else if (att->attlen == -1 && VARATT_IS_EXTERNAL_ONDISK(values[i]))
    {
      pq_sendbyte(out, 'u');                             
      continue;
    }

    typtup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(att->atttypid));
    if (!HeapTupleIsValid(typtup))
    {
      elog(ERROR, "cache lookup failed for type %u", att->atttypid);
    }
    typclass = (Form_pg_type)GETSTRUCT(typtup);

    pq_sendbyte(out, 't');                          

    outputstr = OidOutputFunctionCall(typclass->typoutput, values[i]);
    pq_sendcountedtext(out, outputstr, strlen(outputstr), false);
    pfree(outputstr);

    ReleaseSysCache(typtup);
  }
}

   
                                            
   
                                                        
   
static void
logicalrep_read_tuple(StringInfo in, LogicalRepTupleData *tuple)
{
  int i;
  int natts;

                                
  natts = pq_getmsgint(in, 2);

  memset(tuple->changed, 0, sizeof(tuple->changed));

                     
  for (i = 0; i < natts; i++)
  {
    char kind;

    kind = pq_getmsgbyte(in);

    switch (kind)
    {
    case 'n':           
      tuple->values[i] = NULL;
      tuple->changed[i] = true;
      break;
    case 'u':                       
                                                             
      tuple->values[i] = NULL;
      break;
    case 't':                           
    {
      int len;

      tuple->changed[i] = true;

      len = pq_getmsgint(in, 4);                  

                    
      tuple->values[i] = palloc(len + 1);
      pq_copymsgbytes(in, tuple->values[i], len);
      tuple->values[i][len] = '\0';
    }
    break;
    default:
      elog(ERROR, "unrecognized data representation type '%c'", kind);
    }
  }
}

   
                                            
   
static void
logicalrep_write_attrs(StringInfo out, Relation rel)
{
  TupleDesc desc;
  int i;
  uint16 nliveatts = 0;
  Bitmapset *idattrs = NULL;
  bool replidentfull;

  desc = RelationGetDescr(rel);

                                      
  for (i = 0; i < desc->natts; i++)
  {
    if (TupleDescAttr(desc, i)->attisdropped || TupleDescAttr(desc, i)->attgenerated)
    {
      continue;
    }
    nliveatts++;
  }
  pq_sendint16(out, nliveatts);

                                                       
  replidentfull = (rel->rd_rel->relreplident == REPLICA_IDENTITY_FULL);
  if (!replidentfull)
  {
    idattrs = RelationGetIndexAttrBitmap(rel, INDEX_ATTR_BITMAP_IDENTITY_KEY);
  }

                           
  for (i = 0; i < desc->natts; i++)
  {
    Form_pg_attribute att = TupleDescAttr(desc, i);
    uint8 flags = 0;

    if (att->attisdropped || att->attgenerated)
    {
      continue;
    }

                                                                          
    if (replidentfull || bms_is_member(att->attnum - FirstLowInvalidHeapAttributeNumber, idattrs))
    {
      flags |= LOGICALREP_IS_REPLICA_IDENTITY;
    }

    pq_sendbyte(out, flags);

                        
    pq_sendstring(out, NameStr(att->attname));

                           
    pq_sendint32(out, (int)att->atttypid);

                        
    pq_sendint32(out, att->atttypmod);
  }

  bms_free(idattrs);
}

   
                                                  
   
static void
logicalrep_read_attrs(StringInfo in, LogicalRepRelation *rel)
{
  int i;
  int natts;
  char **attnames;
  Oid *atttyps;
  Bitmapset *attkeys = NULL;

  natts = pq_getmsgint(in, 2);
  attnames = palloc(natts * sizeof(char *));
  atttyps = palloc(natts * sizeof(Oid));

                           
  for (i = 0; i < natts; i++)
  {
    uint8 flags;

                                           
    flags = pq_getmsgbyte(in);
    if (flags & LOGICALREP_IS_REPLICA_IDENTITY)
    {
      attkeys = bms_add_member(attkeys, i);
    }

                        
    attnames[i] = pstrdup(pq_getmsgstring(in));

                           
    atttyps[i] = (Oid)pq_getmsgint(in, 4);

                                          
    (void)pq_getmsgint(in, 4);
  }

  rel->attnames = attnames;
  rel->atttyps = atttyps;
  rel->attkeys = attkeys;
  rel->natts = natts;
}

   
                                                                            
   
static void
logicalrep_write_namespace(StringInfo out, Oid nspid)
{
  if (nspid == PG_CATALOG_NAMESPACE)
  {
    pq_sendbyte(out, '\0');
  }
  else
  {
    char *nspname = get_namespace_name(nspid);

    if (nspname == NULL)
    {
      elog(ERROR, "cache lookup failed for namespace %u", nspid);
    }

    pq_sendstring(out, nspname);
  }
}

   
                                                                      
   
static const char *
logicalrep_read_namespace(StringInfo in)
{
  const char *nspname = pq_getmsgstring(in);

  if (nspname[0] == '\0')
  {
    nspname = "pg_catalog";
  }

  return nspname;
}
