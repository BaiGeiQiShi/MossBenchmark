                                                                            
   
          
                                        
   
                                                                         
                                                                        
   
   
                  
                                 
   
                                                                            
   
#include "postgres.h"

#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>

#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "catalog/dependency.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_type.h"
#include "commands/copy.h"
#include "commands/defrem.h"
#include "commands/trigger.h"
#include "executor/execPartition.h"
#include "executor/executor.h"
#include "executor/nodeModifyTable.h"
#include "executor/tuptable.h"
#include "foreign/fdwapi.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "optimizer/optimizer.h"
#include "nodes/makefuncs.h"
#include "parser/parse_coerce.h"
#include "parser/parse_collate.h"
#include "parser/parse_expr.h"
#include "parser/parse_relation.h"
#include "port/pg_bswap.h"
#include "rewrite/rewriteHandler.h"
#include "storage/fd.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/partcache.h"
#include "utils/portal.h"
#include "utils/rel.h"
#include "utils/rls.h"
#include "utils/snapmgr.h"

#define ISOCTAL(c) (((c) >= '0') && ((c) <= '7'))
#define OCTVALUE(c) ((c) - '0')

   
                                                                        
                    
   
typedef enum CopyDest
{
  COPY_FILE,                                           
  COPY_OLD_FE,                                       
  COPY_NEW_FE,                                       
  COPY_CALLBACK                                
} CopyDest;

   
                                                           
   
typedef enum EolType
{
  EOL_UNKNOWN,
  EOL_NL,
  EOL_CR,
  EOL_CRNL
} EolType;

   
                                                                  
   
typedef enum CopyInsertMethod
{
  CIM_SINGLE,                                                      
  CIM_MULTI,                                               
  CIM_MULTI_CONDITIONAL                                           
} CopyInsertMethod;

   
                                                                       
                                                                               
                                                        
   
                                                                               
                                                                               
                                                                          
                                                                               
                                                                     
                                                                       
                                                                             
                                                                           
                                                                               
                                       
   
typedef struct CopyStateData
{
                            
  CopyDest copy_dest;                                              
  FILE *copy_file;                                                
  StringInfo fe_msgbuf;                                                      
                                                                    
  bool is_copy_from;                                      
  bool reached_eof;                                                       
                                                                      
  EolType eol_type;                                  
  int file_encoding;                                                        
  bool need_transcoding;                                           
  bool encoding_embeds_ascii;                                   

                                        
  Relation rel;                                                        
  QueryDesc *queryDesc;                                                  
  List *attnumlist;                                                        
  char *filename;                                                             
  bool is_program;                                                           
  copy_data_source_cb data_source_cb;                                
  bool binary;                                            
  bool freeze;                                                     
  bool csv_mode;                                                         
  bool header_line;                                         
  char *null_print;                                                              
  int null_print_len;                                     
  char *null_print_client;                                                 
  char *delim;                                                               
  char *quote;                                                             
  char *escape;                                                             
  List *force_quote;                                            
  bool force_quote_all;                                   
  bool *force_quote_flags;                                         
  List *force_notnull;                                          
  bool *force_notnull_flags;                                        
  List *force_null;                                             
  bool *force_null_flags;                                          
  bool convert_selectively;                                                
  List *convert_select;                                                      
  bool *convert_select_flags;                                           
  Node *whereClause;                                                 

                                                                    
  const char *cur_relname;                                    
  uint64 cur_lineno;                                           
  const char *cur_attname;                                     
  const char *cur_attval;                                            

     
                                    
     
  MemoryContext copycontext;                                 

     
                               
     
  FmgrInfo *out_functions;                                        
  MemoryContext rowcontext;                                 

     
                                 
     
  AttrNumber num_defaults;
  FmgrInfo oid_in_function;
  FmgrInfo *in_functions;                                              
  Oid *typioparams;                                                    
  int *defmap;                                              
  ExprState **defexprs;                                         
  bool volatile_defexprs;                                   
  List *range_table;
  ExprState *qualexpr;

  TransitionCaptureState *transition_capture;

     
                                                                       
     
                                                                          
                                                                          
                                                                            
                               
     
  StringInfoData attribute_buf;

                                                  

  int max_fields;
  char **raw_fields;

     
                                                                         
                                                                           
                                                                         
                                                                             
                                                      
     
  StringInfoData line_buf;
  bool line_buf_converted;                                    
  bool line_buf_valid;                                            

     
                                                                        
                                                                        
                                                                     
                                                            
                           
     
#define RAW_BUF_SIZE 65536                                     
  char *raw_buf;
  int raw_buf_index;                           
  int raw_buf_len;                                
} CopyStateData;

                                      
typedef struct
{
  DestReceiver pub;                                       
  CopyState cstate;                                    
  uint64 processed;                            
} DR_copy;

   
                                                           
   
                                                                       
                                                               
                                                                           
                                                                          
                         
   
#define MAX_BUFFERED_TUPLES 1000

   
                                                                          
                           
   
#define MAX_BUFFERED_BYTES 65535

                                                                      
#define MAX_PARTITION_BUFFERS 32

                                                                        
typedef struct CopyMultiInsertBuffer
{
  TupleTableSlot *slots[MAX_BUFFERED_TUPLES];                            
  ResultRelInfo *resultRelInfo;                                              
  BulkInsertState bistate;                                                      
  int nused;                                                                           
  uint64 linenos[MAX_BUFFERED_TUPLES];                                   
                                                          
} CopyMultiInsertBuffer;

   
                                                                            
                                                                               
                                                        
   
typedef struct CopyMultiInsertInfo
{
  List *multiInsertBuffers;                                             
  int bufferedTuples;                                                       
  int bufferedBytes;                                                      
  CopyState cstate;                                                      
  EState *estate;                                             
  CommandId mycid;                                        
  int ti_options;                                     
} CopyMultiInsertInfo;

   
                                                                              
                                                                             
                                               
   
                                                                            
                                                                            
                                                                            
                                                                            
                                                                             
   

   
                                                                      
                                              
   
#define IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(extralen)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  if (1)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (raw_buf_ptr + (extralen) >= copy_buf_len && !hit_eof)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      raw_buf_ptr = prev_raw_ptr;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      need_data = true;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      continue;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    ((void)0)

                                                          
#define IF_NEED_REFILL_AND_EOF_BREAK(extralen)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  if (1)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (raw_buf_ptr + (extralen) >= copy_buf_len && hit_eof)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      if (extralen)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
        raw_buf_ptr = copy_buf_len;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      result = true;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
      break;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    ((void)0)

   
                                                                   
                                  
   
#define REFILL_LINEBUF                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  if (1)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (raw_buf_ptr > cstate->raw_buf_index)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      appendBinaryStringInfo(&cstate->line_buf, cstate->raw_buf + cstate->raw_buf_index, raw_buf_ptr - cstate->raw_buf_index);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
      cstate->raw_buf_index = raw_buf_ptr;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    ((void)0)

                                                    
#define NO_END_OF_COPY_GOTO                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  if (1)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    raw_buf_ptr = prev_raw_ptr + 1;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    goto not_end_of_copy;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    ((void)0)

static const char BinarySignature[11] = "PGCOPY\n\377\r\n\0";

                                    
static CopyState
BeginCopy(ParseState *pstate, bool is_from, Relation rel, RawStmt *raw_query, Oid queryRelId, List *attnamelist, List *options);
static void
EndCopy(CopyState cstate);
static void
ClosePipeToProgram(CopyState cstate);
static CopyState
BeginCopyTo(ParseState *pstate, Relation rel, RawStmt *query, Oid queryRelId, const char *filename, bool is_program, List *attnamelist, List *options);
static void
EndCopyTo(CopyState cstate);
static uint64
DoCopyTo(CopyState cstate);
static uint64
CopyTo(CopyState cstate);
static void
CopyOneRowTo(CopyState cstate, TupleTableSlot *slot);
static bool
CopyReadLine(CopyState cstate);
static bool
CopyReadLineText(CopyState cstate);
static int
CopyReadAttributesText(CopyState cstate);
static int
CopyReadAttributesCSV(CopyState cstate);
static Datum
CopyReadBinaryAttribute(CopyState cstate, int column_no, FmgrInfo *flinfo, Oid typioparam, int32 typmod, bool *isnull);
static void
CopyAttributeOutText(CopyState cstate, char *string);
static void
CopyAttributeOutCSV(CopyState cstate, char *string, bool use_quote, bool single_attr);
static List *
CopyGetAttnums(TupleDesc tupDesc, Relation rel, List *attnamelist);
static char *
limit_printout_length(const char *str);

                                        
static void
SendCopyBegin(CopyState cstate);
static void
ReceiveCopyBegin(CopyState cstate);
static void
SendCopyEnd(CopyState cstate);
static void
CopySendData(CopyState cstate, const void *databuf, int datasize);
static void
CopySendString(CopyState cstate, const char *str);
static void
CopySendChar(CopyState cstate, char c);
static void
CopySendEndOfRow(CopyState cstate);
static int
CopyGetData(CopyState cstate, void *databuf, int minread, int maxread);
static void
CopySendInt32(CopyState cstate, int32 val);
static bool
CopyGetInt32(CopyState cstate, int32 *val);
static void
CopySendInt16(CopyState cstate, int16 val);
static bool
CopyGetInt16(CopyState cstate, int16 *val);

   
                                                                          
                               
   
static void
SendCopyBegin(CopyState cstate)
{
  if (PG_PROTOCOL_MAJOR(FrontendProtocol) >= 3)
  {
                 
    StringInfoData buf;
    int natts = list_length(cstate->attnumlist);
    int16 format = (cstate->binary ? 1 : 0);
    int i;

    pq_beginmessage(&buf, 'H');
    pq_sendbyte(&buf, format);                     
    pq_sendint16(&buf, natts);
    for (i = 0; i < natts; i++)
    {
      pq_sendint16(&buf, format);                         
    }
    pq_endmessage(&buf);
    cstate->copy_dest = COPY_NEW_FE;
  }
  else
  {
                 
    if (cstate->binary)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("COPY BINARY is not supported to stdout or from stdin")));
    }
    pq_putemptymessage('H');
                                                     
    pq_startcopyout();
    cstate->copy_dest = COPY_OLD_FE;
  }
}

static void
ReceiveCopyBegin(CopyState cstate)
{
  if (PG_PROTOCOL_MAJOR(FrontendProtocol) >= 3)
  {
                 
    StringInfoData buf;
    int natts = list_length(cstate->attnumlist);
    int16 format = (cstate->binary ? 1 : 0);
    int i;

    pq_beginmessage(&buf, 'G');
    pq_sendbyte(&buf, format);                     
    pq_sendint16(&buf, natts);
    for (i = 0; i < natts; i++)
    {
      pq_sendint16(&buf, format);                         
    }
    pq_endmessage(&buf);
    cstate->copy_dest = COPY_NEW_FE;
    cstate->fe_msgbuf = makeStringInfo();
  }
  else
  {
                 
    if (cstate->binary)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("COPY BINARY is not supported to stdout or from stdin")));
    }
    pq_putemptymessage('G');
                                                          
    pq_startmsgread();
    cstate->copy_dest = COPY_OLD_FE;
  }
                                                            
  pq_flush();
}

static void
SendCopyEnd(CopyState cstate)
{
  if (cstate->copy_dest == COPY_NEW_FE)
  {
                                        
    Assert(cstate->fe_msgbuf->len == 0);
                                
    pq_putemptymessage('c');
  }
  else
  {
    CopySendData(cstate, "\\.", 2);
                                                                     
    CopySendEndOfRow(cstate);
    pq_endcopyout(false);
  }
}

             
                                                                        
                                                            
                                                    
                                                                       
                                                             
   
                                                        
             
   
static void
CopySendData(CopyState cstate, const void *databuf, int datasize)
{
  appendBinaryStringInfo(cstate->fe_msgbuf, databuf, datasize);
}

static void
CopySendString(CopyState cstate, const char *str)
{
  appendBinaryStringInfo(cstate->fe_msgbuf, str, strlen(str));
}

static void
CopySendChar(CopyState cstate, char c)
{
  appendStringInfoCharMacro(cstate->fe_msgbuf, c);
}

static void
CopySendEndOfRow(CopyState cstate)
{
  StringInfo fe_msgbuf = cstate->fe_msgbuf;

  switch (cstate->copy_dest)
  {
  case COPY_FILE:
    if (!cstate->binary)
    {
                                                        
#ifndef WIN32
      CopySendChar(cstate, '\n');
#else
      CopySendString(cstate, "\r\n");
#endif
    }

    if (fwrite(fe_msgbuf->data, fe_msgbuf->len, 1, cstate->copy_file) != 1 || ferror(cstate->copy_file))
    {
      if (cstate->is_program)
      {
        if (errno == EPIPE)
        {
             
                                                               
                                                               
                                                               
                                
             
          ClosePipeToProgram(cstate);

             
                                                                
                                                              
                                                       
             
          errno = EPIPE;
        }
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not write to COPY program: %m")));
      }
      else
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not write to COPY file: %m")));
      }
    }
    break;
  case COPY_OLD_FE:
                                                                 
    if (!cstate->binary)
    {
      CopySendChar(cstate, '\n');
    }

    if (pq_putbytes(fe_msgbuf->data, fe_msgbuf->len))
    {
                                                           
      ereport(FATAL, (errcode(ERRCODE_CONNECTION_FAILURE), errmsg("connection lost during COPY to stdout")));
    }
    break;
  case COPY_NEW_FE:
                                                                 
    if (!cstate->binary)
    {
      CopySendChar(cstate, '\n');
    }

                                                          
    (void)pq_putmessage('d', fe_msgbuf->data, fe_msgbuf->len);
    break;
  case COPY_CALLBACK:
    Assert(false);                         
    break;
  }

  resetStringInfo(fe_msgbuf);
}

   
                                                             
   
                                                                        
                                                                        
                                        
   
                                                                         
                                                                          
                                                                           
   
                                           
   
static int
CopyGetData(CopyState cstate, void *databuf, int minread, int maxread)
{
  int bytesread = 0;

  switch (cstate->copy_dest)
  {
  case COPY_FILE:
    bytesread = fread(databuf, 1, maxread, cstate->copy_file);
    if (ferror(cstate->copy_file))
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read from COPY file: %m")));
    }
    if (bytesread == 0)
    {
      cstate->reached_eof = true;
    }
    break;
  case COPY_OLD_FE:

       
                                                                       
                                                                     
                                                                       
                                                                   
                                                           
       
    if (pq_getbytes((char *)databuf, minread))
    {
                                                             
      ereport(ERROR, (errcode(ERRCODE_CONNECTION_FAILURE), errmsg("unexpected EOF on client connection with an open transaction")));
    }
    bytesread = minread;
    break;
  case COPY_NEW_FE:
    while (maxread > 0 && bytesread < minread && !cstate->reached_eof)
    {
      int avail;

      while (cstate->fe_msgbuf->cursor >= cstate->fe_msgbuf->len)
      {
                                            
        int mtype;

      readmessage:
        HOLD_CANCEL_INTERRUPTS();
        pq_startmsgread();
        mtype = pq_getbyte();
        if (mtype == EOF)
        {
          ereport(ERROR, (errcode(ERRCODE_CONNECTION_FAILURE), errmsg("unexpected EOF on client connection with an open transaction")));
        }
        if (pq_getmessage(cstate->fe_msgbuf, 0))
        {
          ereport(ERROR, (errcode(ERRCODE_CONNECTION_FAILURE), errmsg("unexpected EOF on client connection with an open transaction")));
        }
        RESUME_CANCEL_INTERRUPTS();
        switch (mtype)
        {
        case 'd':               
          break;
        case 'c':               
                                                        
          cstate->reached_eof = true;
          return bytesread;
        case 'f':               
          ereport(ERROR, (errcode(ERRCODE_QUERY_CANCELED), errmsg("COPY from stdin failed: %s", pq_getmsgstring(cstate->fe_msgbuf))));
          break;
        case 'H':            
        case 'S':           

             
                                                             
                                                           
                                                         
                            
             
          goto readmessage;
        default:
          ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("unexpected message type 0x%02X during COPY from stdin", mtype)));
          break;
        }
      }
      avail = cstate->fe_msgbuf->len - cstate->fe_msgbuf->cursor;
      if (avail > maxread)
      {
        avail = maxread;
      }
      pq_copymsgbytes(cstate->fe_msgbuf, databuf, avail);
      databuf = (void *)((char *)databuf + avail);
      maxread -= avail;
      bytesread += avail;
    }
    break;
  case COPY_CALLBACK:
    bytesread = cstate->data_source_cb(databuf, minread, maxread);
    break;
  }

  return bytesread;
}

   
                                                 
   

   
                                                      
   
static void
CopySendInt32(CopyState cstate, int32 val)
{
  uint32 buf;

  buf = pg_hton32((uint32)val);
  CopySendData(cstate, &buf, sizeof(buf));
}

   
                                                                  
   
                                    
   
static bool
CopyGetInt32(CopyState cstate, int32 *val)
{
  uint32 buf;

  if (CopyGetData(cstate, &buf, sizeof(buf), sizeof(buf)) != sizeof(buf))
  {
    *val = 0;                                
    return false;
  }
  *val = (int32)pg_ntoh32(buf);
  return true;
}

   
                                                      
   
static void
CopySendInt16(CopyState cstate, int16 val)
{
  uint16 buf;

  buf = pg_hton16((uint16)val);
  CopySendData(cstate, &buf, sizeof(buf));
}

   
                                                                  
   
static bool
CopyGetInt16(CopyState cstate, int16 *val)
{
  uint16 buf;

  if (CopyGetData(cstate, &buf, sizeof(buf), sizeof(buf)) != sizeof(buf))
  {
    *val = 0;                                
    return false;
  }
  *val = (int16)pg_ntoh16(buf);
  return true;
}

   
                                                    
   
                                                                      
   
                                                                         
                                                                          
                                                                        
                        
   
static bool
CopyLoadRawBuf(CopyState cstate)
{
  int nbytes;
  int inbytes;

  if (cstate->raw_buf_index < cstate->raw_buf_len)
  {
                                        
    nbytes = cstate->raw_buf_len - cstate->raw_buf_index;
    memmove(cstate->raw_buf, cstate->raw_buf + cstate->raw_buf_index, nbytes);
  }
  else
  {
    nbytes = 0;                            
  }

  inbytes = CopyGetData(cstate, cstate->raw_buf + nbytes, 1, RAW_BUF_SIZE - nbytes);
  nbytes += inbytes;
  cstate->raw_buf[nbytes] = '\0';
  cstate->raw_buf_index = 0;
  cstate->raw_buf_len = nbytes;
  return (inbytes > 0);
}

   
                                           
   
                                                                              
                                                                            
                                                                             
                    
   
                                                                        
                                                                         
                                                                     
                                                                        
   
                                                                      
                                                                 
   
                                                                          
                                                    
   
void
DoCopy(ParseState *pstate, const CopyStmt *stmt, int stmt_location, int stmt_len, uint64 *processed)
{
  CopyState cstate;
  bool is_from = stmt->is_from;
  bool pipe = (stmt->filename == NULL);
  Relation rel;
  Oid relid;
  RawStmt *query = NULL;
  Node *whereClause = NULL;

     
                                                                    
                       
     
  if (!pipe)
  {
    if (stmt->is_program)
    {
      if (!is_member_of_role(GetUserId(), DEFAULT_ROLE_EXECUTE_SERVER_PROGRAM))
      {
        ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser or a member of the pg_execute_server_program role to COPY to or from an external program"),
                           errhint("Anyone can COPY to stdout or from stdin. "
                                   "psql's \\copy command also works for anyone.")));
      }
    }
    else
    {
      if (is_from && !is_member_of_role(GetUserId(), DEFAULT_ROLE_READ_SERVER_FILES))
      {
        ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser or a member of the pg_read_server_files role to COPY from a file"),
                           errhint("Anyone can COPY to stdout or from stdin. "
                                   "psql's \\copy command also works for anyone.")));
      }

      if (!is_from && !is_member_of_role(GetUserId(), DEFAULT_ROLE_WRITE_SERVER_FILES))
      {
        ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser or a member of the pg_write_server_files role to COPY to a file"),
                           errhint("Anyone can COPY to stdout or from stdin. "
                                   "psql's \\copy command also works for anyone.")));
      }
    }
  }

  if (stmt->relation)
  {
    LOCKMODE lockmode = is_from ? RowExclusiveLock : AccessShareLock;
    RangeTblEntry *rte;
    TupleDesc tupDesc;
    List *attnums;
    ListCell *cur;

    Assert(!stmt->query);

                                                                      
    rel = table_openrv(stmt->relation, lockmode);

    relid = RelationGetRelid(rel);

    rte = addRangeTableEntryForRelation(pstate, rel, lockmode, NULL, false, false);
    rte->requiredPerms = (is_from ? ACL_INSERT : ACL_SELECT);

    if (stmt->whereClause)
    {
                                        
      addRTEtoQuery(pstate, rte, false, true, true);

                                             
      whereClause = transformExpr(pstate, stmt->whereClause, EXPR_KIND_COPY_WHERE);

                                                 
      whereClause = coerce_to_boolean(pstate, whereClause, "WHERE");

                                             
      assign_expr_collations(pstate, whereClause);

      whereClause = eval_const_expressions(NULL, whereClause);

      whereClause = (Node *)canonicalize_qual((Expr *)whereClause, false);
      whereClause = (Node *)make_ands_implicit((Expr *)whereClause);
    }

    tupDesc = RelationGetDescr(rel);
    attnums = CopyGetAttnums(tupDesc, rel, stmt->attlist);
    foreach (cur, attnums)
    {
      int attno = lfirst_int(cur) - FirstLowInvalidHeapAttributeNumber;

      if (is_from)
      {
        rte->insertedCols = bms_add_member(rte->insertedCols, attno);
      }
      else
      {
        rte->selectedCols = bms_add_member(rte->selectedCols, attno);
      }
    }
    ExecCheckRTPerms(pstate->p_rtable, true);

       
                                                   
       
                                                                      
                                                                         
                                                                 
       
                                                                        
                                                                         
                               
       
                                                                     
                                               
       
    if (check_enable_rls(rte->relid, InvalidOid, false) == RLS_ENABLED)
    {
      SelectStmt *select;
      ColumnRef *cr;
      ResTarget *target;
      RangeVar *from;
      List *targetList = NIL;

      if (is_from)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("COPY FROM not supported with row-level security"), errhint("Use INSERT statements instead.")));
      }

         
                           
         
                                                                       
                                                                        
                                                                    
                    
         
                                                                       
                                                                       
                                                                
         
      if (!stmt->attlist)
      {
        cr = makeNode(ColumnRef);
        cr->fields = list_make1(makeNode(A_Star));
        cr->location = -1;

        target = makeNode(ResTarget);
        target->name = NULL;
        target->indirection = NIL;
        target->val = (Node *)cr;
        target->location = -1;

        targetList = list_make1(target);
      }
      else
      {
        ListCell *lc;

        foreach (lc, stmt->attlist)
        {
             
                                                                 
                                                             
                                                                
                           
             
          cr = makeNode(ColumnRef);
          cr->fields = list_make1(lfirst(lc));
          cr->location = -1;

                                                                
          target = makeNode(ResTarget);
          target->name = NULL;
          target->indirection = NIL;
          target->val = (Node *)cr;
          target->location = -1;

                                                                     
          targetList = lappend(targetList, target);
        }
      }

         
                                                                      
                                                   
         
      from = makeRangeVar(get_namespace_name(RelationGetNamespace(rel)), pstrdup(RelationGetRelationName(rel)), -1);

                       
      select = makeNode(SelectStmt);
      select->targetList = targetList;
      select->fromClause = list_make1(from);

      query = makeNode(RawStmt);
      query->stmt = (Node *)select;
      query->stmt_location = stmt_location;
      query->stmt_len = stmt_len;

         
                                                                        
                                                                     
         
                                                                
         
      table_close(rel, NoLock);
      rel = NULL;
    }
  }
  else
  {
    Assert(stmt->query);

    query = makeNode(RawStmt);
    query->stmt = stmt->query;
    query->stmt_location = stmt_location;
    query->stmt_len = stmt_len;

    relid = InvalidOid;
    rel = NULL;
  }

  if (is_from)
  {
    Assert(rel);

                                                       
    if (XactReadOnly && !rel->rd_islocaltemp)
    {
      PreventCommandIfReadOnly("COPY FROM");
    }
    PreventCommandIfParallelMode("COPY FROM");

    cstate = BeginCopyFrom(pstate, rel, stmt->filename, stmt->is_program, NULL, stmt->attlist, stmt->options);
    cstate->whereClause = whereClause;
    *processed = CopyFrom(cstate);                                 
    EndCopyFrom(cstate);
  }
  else
  {
    cstate = BeginCopyTo(pstate, rel, query, relid, stmt->filename, stmt->is_program, stmt->attlist, stmt->options);
    *processed = DoCopyTo(cstate);                                 
    EndCopyTo(cstate);
  }

     
                                                                           
                                                                          
                                                                    
     
  if (rel != NULL)
  {
    table_close(rel, (is_from ? NoLock : AccessShareLock));
  }
}

   
                                               
   
                                                                           
                                                     
   
                                                         
   
                                                                            
                                                                      
                                                                             
                                                            
   
                                                                               
                                                                        
                                         
   
void
ProcessCopyOptions(ParseState *pstate, CopyState cstate, bool is_from, List *options)
{
  bool format_specified = false;
  ListCell *option;

                                                       
  if (cstate == NULL)
  {
    cstate = (CopyStateData *)palloc0(sizeof(CopyStateData));
  }

  cstate->is_copy_from = is_from;

  cstate->file_encoding = -1;

                                                    
  foreach (option, options)
  {
    DefElem *defel = lfirst_node(DefElem, option);

    if (strcmp(defel->defname, "format") == 0)
    {
      char *fmt = defGetString(defel);

      if (format_specified)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      format_specified = true;
      if (strcmp(fmt, "text") == 0)
                            ;
      else if (strcmp(fmt, "csv") == 0)
      {
        cstate->csv_mode = true;
      }
      else if (strcmp(fmt, "binary") == 0)
      {
        cstate->binary = true;
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("COPY format \"%s\" not recognized", fmt), parser_errposition(pstate, defel->location)));
      }
    }
    else if (strcmp(defel->defname, "freeze") == 0)
    {
      if (cstate->freeze)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      cstate->freeze = defGetBoolean(defel);
    }
    else if (strcmp(defel->defname, "delimiter") == 0)
    {
      if (cstate->delim)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      cstate->delim = defGetString(defel);
    }
    else if (strcmp(defel->defname, "null") == 0)
    {
      if (cstate->null_print)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      cstate->null_print = defGetString(defel);
    }
    else if (strcmp(defel->defname, "header") == 0)
    {
      if (cstate->header_line)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      cstate->header_line = defGetBoolean(defel);
    }
    else if (strcmp(defel->defname, "quote") == 0)
    {
      if (cstate->quote)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      cstate->quote = defGetString(defel);
    }
    else if (strcmp(defel->defname, "escape") == 0)
    {
      if (cstate->escape)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      cstate->escape = defGetString(defel);
    }
    else if (strcmp(defel->defname, "force_quote") == 0)
    {
      if (cstate->force_quote || cstate->force_quote_all)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      if (defel->arg && IsA(defel->arg, A_Star))
      {
        cstate->force_quote_all = true;
      }
      else if (defel->arg && IsA(defel->arg, List))
      {
        cstate->force_quote = castNode(List, defel->arg);
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("argument to option \"%s\" must be a list of column names", defel->defname), parser_errposition(pstate, defel->location)));
      }
    }
    else if (strcmp(defel->defname, "force_not_null") == 0)
    {
      if (cstate->force_notnull)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      if (defel->arg && IsA(defel->arg, List))
      {
        cstate->force_notnull = castNode(List, defel->arg);
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("argument to option \"%s\" must be a list of column names", defel->defname), parser_errposition(pstate, defel->location)));
      }
    }
    else if (strcmp(defel->defname, "force_null") == 0)
    {
      if (cstate->force_null)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }
      if (defel->arg && IsA(defel->arg, List))
      {
        cstate->force_null = castNode(List, defel->arg);
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("argument to option \"%s\" must be a list of column names", defel->defname), parser_errposition(pstate, defel->location)));
      }
    }
    else if (strcmp(defel->defname, "convert_selectively") == 0)
    {
         
                                                                        
                                                                       
                                                
         
      if (cstate->convert_selectively)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      cstate->convert_selectively = true;
      if (defel->arg == NULL || IsA(defel->arg, List))
      {
        cstate->convert_select = castNode(List, defel->arg);
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("argument to option \"%s\" must be a list of column names", defel->defname), parser_errposition(pstate, defel->location)));
      }
    }
    else if (strcmp(defel->defname, "encoding") == 0)
    {
      if (cstate->file_encoding >= 0)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      cstate->file_encoding = pg_char_to_encoding(defGetString(defel));
      if (cstate->file_encoding < 0)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("argument to option \"%s\" must be a valid encoding name", defel->defname), parser_errposition(pstate, defel->location)));
      }
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("option \"%s\" not recognized", defel->defname), parser_errposition(pstate, defel->location)));
    }
  }

     
                                                                        
               
     
  if (cstate->binary && cstate->delim)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cannot specify DELIMITER in BINARY mode")));
  }

  if (cstate->binary && cstate->null_print)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cannot specify NULL in BINARY mode")));
  }

                                        
  if (!cstate->delim)
  {
    cstate->delim = cstate->csv_mode ? "," : "\t";
  }

  if (!cstate->null_print)
  {
    cstate->null_print = cstate->csv_mode ? "" : "\\N";
  }
  cstate->null_print_len = strlen(cstate->null_print);

  if (cstate->csv_mode)
  {
    if (!cstate->quote)
    {
      cstate->quote = "\"";
    }
    if (!cstate->escape)
    {
      cstate->escape = cstate->quote;
    }
  }

                                                         
  if (strlen(cstate->delim) != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("COPY delimiter must be a single one-byte character")));
  }

                                       
  if (strchr(cstate->delim, '\r') != NULL || strchr(cstate->delim, '\n') != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("COPY delimiter cannot be newline or carriage return")));
  }

  if (strchr(cstate->null_print, '\r') != NULL || strchr(cstate->null_print, '\n') != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("COPY null representation cannot use newline or carriage return")));
  }

     
                                                                           
                                                                        
                                                                  
                                                                     
                                                                            
                                                                      
                                                                         
                                    
     
  if (!cstate->csv_mode && strchr("\\.abcdefghijklmnopqrstuvwxyz0123456789", cstate->delim[0]) != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("COPY delimiter cannot be \"%s\"", cstate->delim)));
  }

                    
  if (!cstate->csv_mode && cstate->header_line)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("COPY HEADER available only in CSV mode")));
  }

                   
  if (!cstate->csv_mode && cstate->quote != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("COPY quote available only in CSV mode")));
  }

  if (cstate->csv_mode && strlen(cstate->quote) != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("COPY quote must be a single one-byte character")));
  }

  if (cstate->csv_mode && cstate->delim[0] == cstate->quote[0])
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("COPY delimiter and quote must be different")));
  }

                    
  if (!cstate->csv_mode && cstate->escape != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("COPY escape available only in CSV mode")));
  }

  if (cstate->csv_mode && strlen(cstate->escape) != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("COPY escape must be a single one-byte character")));
  }

                         
  if (!cstate->csv_mode && (cstate->force_quote || cstate->force_quote_all))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("COPY force quote available only in CSV mode")));
  }
  if ((cstate->force_quote || cstate->force_quote_all) && is_from)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("COPY force quote only available using COPY TO")));
  }

                           
  if (!cstate->csv_mode && cstate->force_notnull != NIL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("COPY force not null available only in CSV mode")));
  }
  if (cstate->force_notnull != NIL && !is_from)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("COPY force not null only available using COPY FROM")));
  }

                        
  if (!cstate->csv_mode && cstate->force_null != NIL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("COPY force null available only in CSV mode")));
  }

  if (cstate->force_null != NIL && !is_from)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("COPY force null only available using COPY FROM")));
  }

                                                               
  if (strchr(cstate->null_print, cstate->delim[0]) != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("COPY delimiter must not appear in the NULL specification")));
  }

                                                                    
  if (cstate->csv_mode && strchr(cstate->null_print, cstate->quote[0]) != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("CSV quote character must not appear in the NULL specification")));
  }
}

   
                                                                
   
                                                                          
                                                           
   
                                                                          
                                                                        
                                                                         
                                                                        
                                                                            
   
                                                                           
                                
   
static CopyState
BeginCopy(ParseState *pstate, bool is_from, Relation rel, RawStmt *raw_query, Oid queryRelId, List *attnamelist, List *options)
{
  CopyState cstate;
  TupleDesc tupDesc;
  int num_phys_attrs;
  MemoryContext oldcontext;

                                              
  cstate = (CopyStateData *)palloc0(sizeof(CopyStateData));

     
                                                                           
                                                                 
     
  cstate->copycontext = AllocSetContextCreate(CurrentMemoryContext, "COPY", ALLOCSET_DEFAULT_SIZES);

  oldcontext = MemoryContextSwitchTo(cstate->copycontext);

                                                    
  ProcessCopyOptions(pstate, cstate, is_from, options);

                                                   
  if (rel)
  {
    Assert(!raw_query);

    cstate->rel = rel;

    tupDesc = RelationGetDescr(cstate->rel);
  }
  else
  {
    List *rewritten;
    Query *query;
    PlannedStmt *plan;
    DestReceiver *dest;

    Assert(!is_from);
    cstate->rel = NULL;

       
                                                                           
                                     
       
                                                                          
                                                                       
                                                                    
                                                                        
                                                        
       
    rewritten = pg_analyze_and_rewrite(copyObject(raw_query), pstate->p_sourcetext, NULL, 0, NULL);

                                                           
    if (rewritten == NIL)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("DO INSTEAD NOTHING rules are not supported for COPY")));
    }
    else if (list_length(rewritten) > 1)
    {
      ListCell *lc;

                                                                     
      foreach (lc, rewritten)
      {
        Query *q = lfirst_node(Query, lc);

        if (q->querySource == QSRC_QUAL_INSTEAD_RULE)
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("conditional DO INSTEAD rules are not supported for COPY")));
        }
        if (q->querySource == QSRC_NON_INSTEAD_RULE)
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("DO ALSO rules are not supported for the COPY")));
        }
      }

      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("multi-statement DO INSTEAD rules are not supported for COPY")));
    }

    query = linitial_node(Query, rewritten);

                                                                   
    if (query->utilityStmt != NULL && IsA(query->utilityStmt, CreateTableAsStmt))
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("COPY (SELECT INTO) is not supported")));
    }

    Assert(query->utilityStmt == NULL);

       
                                                                         
                                          
       
    if (query->commandType != CMD_SELECT && query->returningList == NIL)
    {
      Assert(query->commandType == CMD_INSERT || query->commandType == CMD_UPDATE || query->commandType == CMD_DELETE);

      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("COPY query must have a RETURNING clause")));
    }

                        
    plan = pg_plan_query(query, CURSOR_OPT_PARALLEL_OK, NULL);

       
                                                                       
                                                                         
                                                                         
                           
       
                                                                       
                                                                           
                                                                          
                                     
       
    if (queryRelId != InvalidOid)
    {
         
                                                                      
                                                                       
                                                                        
                                                           
         
      if (!list_member_oid(plan->relationOids, queryRelId))
      {
        ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("relation referenced by COPY statement has changed")));
      }
    }

       
                                                                           
                                                   
       
    PushCopiedSnapshot(GetActiveSnapshot());
    UpdateActiveSnapshotCommandId();

                                           
    dest = CreateDestReceiver(DestCopyOut);
    ((DR_copy *)dest)->cstate = cstate;

                                                 
    cstate->queryDesc = CreateQueryDesc(plan, pstate->p_sourcetext, GetActiveSnapshot(), InvalidSnapshot, dest, NULL, NULL, 0);

       
                                                             
       
                                                      
       
    ExecutorStart(cstate->queryDesc, 0);

    tupDesc = cstate->queryDesc->tupDesc;
  }

                                                         
  cstate->attnumlist = CopyGetAttnums(tupDesc, cstate->rel, attnamelist);

  num_phys_attrs = tupDesc->natts;

                                                                         
  cstate->force_quote_flags = (bool *)palloc0(num_phys_attrs * sizeof(bool));
  if (cstate->force_quote_all)
  {
    int i;

    for (i = 0; i < num_phys_attrs; i++)
    {
      cstate->force_quote_flags[i] = true;
    }
  }
  else if (cstate->force_quote)
  {
    List *attnums;
    ListCell *cur;

    attnums = CopyGetAttnums(tupDesc, cstate->rel, cstate->force_quote);

    foreach (cur, attnums)
    {
      int attnum = lfirst_int(cur);
      Form_pg_attribute attr = TupleDescAttr(tupDesc, attnum - 1);

      if (!list_member_int(cstate->attnumlist, attnum))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("FORCE_QUOTE column \"%s\" not referenced by COPY", NameStr(attr->attname))));
      }
      cstate->force_quote_flags[attnum - 1] = true;
    }
  }

                                                                            
  cstate->force_notnull_flags = (bool *)palloc0(num_phys_attrs * sizeof(bool));
  if (cstate->force_notnull)
  {
    List *attnums;
    ListCell *cur;

    attnums = CopyGetAttnums(tupDesc, cstate->rel, cstate->force_notnull);

    foreach (cur, attnums)
    {
      int attnum = lfirst_int(cur);
      Form_pg_attribute attr = TupleDescAttr(tupDesc, attnum - 1);

      if (!list_member_int(cstate->attnumlist, attnum))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("FORCE_NOT_NULL column \"%s\" not referenced by COPY", NameStr(attr->attname))));
      }
      cstate->force_notnull_flags[attnum - 1] = true;
    }
  }

                                                                        
  cstate->force_null_flags = (bool *)palloc0(num_phys_attrs * sizeof(bool));
  if (cstate->force_null)
  {
    List *attnums;
    ListCell *cur;

    attnums = CopyGetAttnums(tupDesc, cstate->rel, cstate->force_null);

    foreach (cur, attnums)
    {
      int attnum = lfirst_int(cur);
      Form_pg_attribute attr = TupleDescAttr(tupDesc, attnum - 1);

      if (!list_member_int(cstate->attnumlist, attnum))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("FORCE_NULL column \"%s\" not referenced by COPY", NameStr(attr->attname))));
      }
      cstate->force_null_flags[attnum - 1] = true;
    }
  }

                                                                 
  if (cstate->convert_selectively)
  {
    List *attnums;
    ListCell *cur;

    cstate->convert_select_flags = (bool *)palloc0(num_phys_attrs * sizeof(bool));

    attnums = CopyGetAttnums(tupDesc, cstate->rel, cstate->convert_select);

    foreach (cur, attnums)
    {
      int attnum = lfirst_int(cur);
      Form_pg_attribute attr = TupleDescAttr(tupDesc, attnum - 1);

      if (!list_member_int(cstate->attnumlist, attnum))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg_internal("selected column \"%s\" not referenced by COPY", NameStr(attr->attname))));
      }
      cstate->convert_select_flags[attnum - 1] = true;
    }
  }

                                                                  
  if (cstate->file_encoding < 0)
  {
    cstate->file_encoding = pg_get_client_encoding();
  }

     
                                                                             
                                                                        
                          
     
  cstate->need_transcoding = (cstate->file_encoding != GetDatabaseEncoding() || pg_database_encoding_max_length() > 1);
                                            
  cstate->encoding_embeds_ascii = PG_ENCODING_IS_CLIENT_ONLY(cstate->file_encoding);

  cstate->copy_dest = COPY_FILE;              

  MemoryContextSwitchTo(oldcontext);

  return cstate;
}

   
                                                                              
   
static void
ClosePipeToProgram(CopyState cstate)
{
  int pclose_rc;

  Assert(cstate->is_program);

  pclose_rc = ClosePipeStream(cstate->copy_file);
  if (pclose_rc == -1)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not close pipe to external command: %m")));
  }
  else if (pclose_rc != 0)
  {
       
                                                                      
                                                                      
                                                                           
                
       
    if (cstate->is_copy_from && !cstate->reached_eof && wait_result_is_signal(pclose_rc, SIGPIPE))
    {
      return;
    }

    ereport(ERROR, (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION), errmsg("program \"%s\" failed", cstate->filename), errdetail_internal("%s", wait_result_to_str(pclose_rc))));
  }
}

   
                                                             
   
static void
EndCopy(CopyState cstate)
{
  if (cstate->is_program)
  {
    ClosePipeToProgram(cstate);
  }
  else
  {
    if (cstate->filename != NULL && FreeFile(cstate->copy_file))
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", cstate->filename)));
    }
  }

  MemoryContextDelete(cstate->copycontext);
  pfree(cstate);
}

   
                                                                       
   
static CopyState
BeginCopyTo(ParseState *pstate, Relation rel, RawStmt *query, Oid queryRelId, const char *filename, bool is_program, List *attnamelist, List *options)
{
  CopyState cstate;
  bool pipe = (filename == NULL);
  MemoryContext oldcontext;

  if (rel != NULL && rel->rd_rel->relkind != RELKIND_RELATION)
  {
    if (rel->rd_rel->relkind == RELKIND_VIEW)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot copy from view \"%s\"", RelationGetRelationName(rel)), errhint("Try the COPY (SELECT ...) TO variant.")));
    }
    else if (rel->rd_rel->relkind == RELKIND_MATVIEW)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot copy from materialized view \"%s\"", RelationGetRelationName(rel)), errhint("Try the COPY (SELECT ...) TO variant.")));
    }
    else if (rel->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot copy from foreign table \"%s\"", RelationGetRelationName(rel)), errhint("Try the COPY (SELECT ...) TO variant.")));
    }
    else if (rel->rd_rel->relkind == RELKIND_SEQUENCE)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot copy from sequence \"%s\"", RelationGetRelationName(rel))));
    }
    else if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot copy from partitioned table \"%s\"", RelationGetRelationName(rel)), errhint("Try the COPY (SELECT ...) TO variant.")));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot copy from non-table relation \"%s\"", RelationGetRelationName(rel))));
    }
  }

  cstate = BeginCopy(pstate, false, rel, query, queryRelId, attnamelist, options);
  oldcontext = MemoryContextSwitchTo(cstate->copycontext);

  if (pipe)
  {
    Assert(!is_program);                                      
    if (whereToSendOutput != DestRemote)
    {
      cstate->copy_file = stdout;
    }
  }
  else
  {
    cstate->filename = pstrdup(filename);
    cstate->is_program = is_program;

    if (is_program)
    {
      cstate->copy_file = OpenPipeStream(cstate->filename, PG_BINARY_W);
      if (cstate->copy_file == NULL)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not execute command \"%s\": %m", cstate->filename)));
      }
    }
    else
    {
      mode_t oumask;                               
      struct stat st;

         
                                                                         
                                                     
         
      if (!is_absolute_path(filename))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("relative path not allowed for COPY to file")));
      }

      oumask = umask(S_IWGRP | S_IWOTH);
      PG_TRY();
      {
        cstate->copy_file = AllocateFile(cstate->filename, PG_BINARY_W);
      }
      PG_CATCH();
      {
        umask(oumask);
        PG_RE_THROW();
      }
      PG_END_TRY();
      umask(oumask);
      if (cstate->copy_file == NULL)
      {
                                                                     
        int save_errno = errno;

        ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\" for writing: %m", cstate->filename),
                           (save_errno == ENOENT || save_errno == EACCES) ? errhint("COPY TO instructs the PostgreSQL server process to write a file. "
                                                                                    "You may want a client-side facility such as psql's \\copy.")
                                                                          : 0));
      }

      if (fstat(fileno(cstate->copy_file), &st))
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", cstate->filename)));
      }

      if (S_ISDIR(st.st_mode))
      {
        ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is a directory", cstate->filename)));
      }
    }
  }

  MemoryContextSwitchTo(oldcontext);

  return cstate;
}

   
                                                                             
                                                                   
   
static uint64
DoCopyTo(CopyState cstate)
{
  bool pipe = (cstate->filename == NULL);
  bool fe_copy = (pipe && whereToSendOutput == DestRemote);
  uint64 processed;

  PG_TRY();
  {
    if (fe_copy)
    {
      SendCopyBegin(cstate);
    }

    processed = CopyTo(cstate);

    if (fe_copy)
    {
      SendCopyEnd(cstate);
    }
  }
  PG_CATCH();
  {
       
                                                                       
                                                                          
               
       
    pq_endcopyout(true);
    PG_RE_THROW();
  }
  PG_END_TRY();

  return processed;
}

   
                                                       
   
static void
EndCopyTo(CopyState cstate)
{
  if (cstate->queryDesc != NULL)
  {
                                                  
    ExecutorFinish(cstate->queryDesc);
    ExecutorEnd(cstate->queryDesc);
    FreeQueryDesc(cstate->queryDesc);
    PopActiveSnapshot();
  }

                        
  EndCopy(cstate);
}

   
                                        
   
static uint64
CopyTo(CopyState cstate)
{
  TupleDesc tupDesc;
  int num_phys_attrs;
  ListCell *cur;
  uint64 processed;

  if (cstate->rel)
  {
    tupDesc = RelationGetDescr(cstate->rel);
  }
  else
  {
    tupDesc = cstate->queryDesc->tupDesc;
  }
  num_phys_attrs = tupDesc->natts;
  cstate->null_print_client = cstate->null_print;              

                                                                    
  cstate->fe_msgbuf = makeStringInfo();

                                                      
  cstate->out_functions = (FmgrInfo *)palloc(num_phys_attrs * sizeof(FmgrInfo));
  foreach (cur, cstate->attnumlist)
  {
    int attnum = lfirst_int(cur);
    Oid out_func_oid;
    bool isvarlena;
    Form_pg_attribute attr = TupleDescAttr(tupDesc, attnum - 1);

    if (cstate->binary)
    {
      getTypeBinaryOutputInfo(attr->atttypid, &out_func_oid, &isvarlena);
    }
    else
    {
      getTypeOutputInfo(attr->atttypid, &out_func_oid, &isvarlena);
    }
    fmgr_info(out_func_oid, &cstate->out_functions[attnum - 1]);
  }

     
                                                                         
                                                                          
                                                                        
                                                                 
     
  cstate->rowcontext = AllocSetContextCreate(CurrentMemoryContext, "COPY TO", ALLOCSET_DEFAULT_SIZES);

  if (cstate->binary)
  {
                                           
    int32 tmp;

                   
    CopySendData(cstate, BinarySignature, 11);
                     
    tmp = 0;
    CopySendInt32(cstate, tmp);
                             
    tmp = 0;
    CopySendInt32(cstate, tmp);
  }
  else
  {
       
                                                                  
                                                                       
       
    if (cstate->need_transcoding)
    {
      cstate->null_print_client = pg_server_to_any(cstate->null_print, cstate->null_print_len, cstate->file_encoding);
    }

                                                      
    if (cstate->header_line)
    {
      bool hdr_delim = false;

      foreach (cur, cstate->attnumlist)
      {
        int attnum = lfirst_int(cur);
        char *colname;

        if (hdr_delim)
        {
          CopySendChar(cstate, cstate->delim[0]);
        }
        hdr_delim = true;

        colname = NameStr(TupleDescAttr(tupDesc, attnum - 1)->attname);

        CopyAttributeOutCSV(cstate, colname, false, list_length(cstate->attnumlist) == 1);
      }

      CopySendEndOfRow(cstate);
    }
  }

  if (cstate->rel)
  {
    TupleTableSlot *slot;
    TableScanDesc scandesc;

    scandesc = table_beginscan(cstate->rel, GetActiveSnapshot(), 0, NULL);
    slot = table_slot_create(cstate->rel, NULL);

    processed = 0;
    while (table_scan_getnextslot(scandesc, ForwardScanDirection, slot))
    {
      CHECK_FOR_INTERRUPTS();

                                     
      slot_getallattrs(slot);

                                    
      CopyOneRowTo(cstate, slot);
      processed++;
    }

    ExecDropSingleTupleTableSlot(slot);
    table_endscan(scandesc);
  }
  else
  {
                                                             
    ExecutorRun(cstate->queryDesc, ForwardScanDirection, 0L, true);
    processed = ((DR_copy *)cstate->queryDesc->dest)->processed;
  }

  if (cstate->binary)
  {
                                            
    CopySendInt16(cstate, -1);
                                       
    CopySendEndOfRow(cstate);
  }

  MemoryContextDelete(cstate->rowcontext);

  return processed;
}

   
                                 
   
static void
CopyOneRowTo(CopyState cstate, TupleTableSlot *slot)
{
  bool need_delim = false;
  FmgrInfo *out_functions = cstate->out_functions;
  MemoryContext oldcontext;
  ListCell *cur;
  char *string;

  MemoryContextReset(cstate->rowcontext);
  oldcontext = MemoryContextSwitchTo(cstate->rowcontext);

  if (cstate->binary)
  {
                                 
    CopySendInt16(cstate, list_length(cstate->attnumlist));
  }

                                                  
  slot_getallattrs(slot);

  foreach (cur, cstate->attnumlist)
  {
    int attnum = lfirst_int(cur);
    Datum value = slot->tts_values[attnum - 1];
    bool isnull = slot->tts_isnull[attnum - 1];

    if (!cstate->binary)
    {
      if (need_delim)
      {
        CopySendChar(cstate, cstate->delim[0]);
      }
      need_delim = true;
    }

    if (isnull)
    {
      if (!cstate->binary)
      {
        CopySendString(cstate, cstate->null_print_client);
      }
      else
      {
        CopySendInt32(cstate, -1);
      }
    }
    else
    {
      if (!cstate->binary)
      {
        string = OutputFunctionCall(&out_functions[attnum - 1], value);
        if (cstate->csv_mode)
        {
          CopyAttributeOutCSV(cstate, string, cstate->force_quote_flags[attnum - 1], list_length(cstate->attnumlist) == 1);
        }
        else
        {
          CopyAttributeOutText(cstate, string);
        }
      }
      else
      {
        bytea *outputbytes;

        outputbytes = SendFunctionCall(&out_functions[attnum - 1], value);
        CopySendInt32(cstate, VARSIZE(outputbytes) - VARHDRSZ);
        CopySendData(cstate, VARDATA(outputbytes), VARSIZE(outputbytes) - VARHDRSZ);
      }
    }
  }

  CopySendEndOfRow(cstate);

  MemoryContextSwitchTo(oldcontext);
}

   
                                        
   
                                                         
   
void
CopyFromErrorCallback(void *arg)
{
  CopyState cstate = (CopyState)arg;
  char curlineno_str[32];

  snprintf(curlineno_str, sizeof(curlineno_str), UINT64_FORMAT, cstate->cur_lineno);

  if (cstate->binary)
  {
                                         
    if (cstate->cur_attname)
    {
      errcontext("COPY %s, line %s, column %s", cstate->cur_relname, curlineno_str, cstate->cur_attname);
    }
    else
    {
      errcontext("COPY %s, line %s", cstate->cur_relname, curlineno_str);
    }
  }
  else
  {
    if (cstate->cur_attname && cstate->cur_attval)
    {
                                                    
      char *attval;

      attval = limit_printout_length(cstate->cur_attval);
      errcontext("COPY %s, line %s, column %s: \"%s\"", cstate->cur_relname, curlineno_str, cstate->cur_attname, attval);
      pfree(attval);
    }
    else if (cstate->cur_attname)
    {
                                                                   
      errcontext("COPY %s, line %s, column %s: null input", cstate->cur_relname, curlineno_str, cstate->cur_attname);
    }
    else
    {
         
                                                 
         
                                                                       
                                                                         
                                                                  
                                                                        
                                                                     
                                                                         
         
      if (cstate->line_buf_valid && (cstate->line_buf_converted || !cstate->need_transcoding))
      {
        char *lineval;

        lineval = limit_printout_length(cstate->line_buf.data);
        errcontext("COPY %s, line %s: \"%s\"", cstate->cur_relname, curlineno_str, lineval);
        pfree(lineval);
      }
      else
      {
        errcontext("COPY %s, line %s", cstate->cur_relname, curlineno_str);
      }
    }
  }
}

   
                                                                              
   
                                                                           
                                                                               
                                                                          
                                                                           
                                                                        
   
static char *
limit_printout_length(const char *str)
{
#define MAX_COPY_DATA_DISPLAY 100

  int slen = strlen(str);
  int len;
  char *res;

                                    
  if (slen <= MAX_COPY_DATA_DISPLAY)
  {
    return pstrdup(str);
  }

                                           
  len = pg_mbcliplen(str, slen, MAX_COPY_DATA_DISPLAY);

     
                                                             
     
  res = (char *)palloc(len + 4);
  memcpy(res, str, len);
  strcpy(res + len, "...");

  return res;
}

   
                                                                       
                  
   
static CopyMultiInsertBuffer *
CopyMultiInsertBufferInit(ResultRelInfo *rri)
{
  CopyMultiInsertBuffer *buffer;

  buffer = (CopyMultiInsertBuffer *)palloc(sizeof(CopyMultiInsertBuffer));
  memset(buffer->slots, 0, sizeof(TupleTableSlot *) * MAX_BUFFERED_TUPLES);
  buffer->resultRelInfo = rri;
  buffer->bistate = GetBulkInsertState();
  buffer->nused = 0;

  return buffer;
}

   
                                             
   
static inline void
CopyMultiInsertInfoSetupBuffer(CopyMultiInsertInfo *miinfo, ResultRelInfo *rri)
{
  CopyMultiInsertBuffer *buffer;

  buffer = CopyMultiInsertBufferInit(rri);

                                                               
  rri->ri_CopyMultiInsertBuffer = buffer;
                                              
  miinfo->multiInsertBuffers = lappend(miinfo->multiInsertBuffers, buffer);
}

   
                                                        
   
                                                                            
                   
   
static void
CopyMultiInsertInfoInit(CopyMultiInsertInfo *miinfo, ResultRelInfo *rri, CopyState cstate, EState *estate, CommandId mycid, int ti_options)
{
  miinfo->multiInsertBuffers = NIL;
  miinfo->bufferedTuples = 0;
  miinfo->bufferedBytes = 0;
  miinfo->cstate = cstate;
  miinfo->estate = estate;
  miinfo->mycid = mycid;
  miinfo->ti_options = ti_options;

     
                                                                      
                                                                            
                                          
     
  if (rri->ri_RelationDesc->rd_rel->relkind != RELKIND_PARTITIONED_TABLE)
  {
    CopyMultiInsertInfoSetupBuffer(miinfo, rri);
  }
}

   
                                        
   
static inline bool
CopyMultiInsertInfoIsFull(CopyMultiInsertInfo *miinfo)
{
  if (miinfo->bufferedTuples >= MAX_BUFFERED_TUPLES || miinfo->bufferedBytes >= MAX_BUFFERED_BYTES)
  {
    return true;
  }
  return false;
}

   
                                              
   
static inline bool
CopyMultiInsertInfoIsEmpty(CopyMultiInsertInfo *miinfo)
{
  return miinfo->bufferedTuples == 0;
}

   
                                                         
   
static inline void
CopyMultiInsertBufferFlush(CopyMultiInsertInfo *miinfo, CopyMultiInsertBuffer *buffer)
{
  MemoryContext oldcontext;
  int i;
  uint64 save_cur_lineno;
  CopyState cstate = miinfo->cstate;
  EState *estate = miinfo->estate;
  CommandId mycid = miinfo->mycid;
  int ti_options = miinfo->ti_options;
  bool line_buf_valid = cstate->line_buf_valid;
  int nused = buffer->nused;
  ResultRelInfo *resultRelInfo = buffer->resultRelInfo;
  TupleTableSlot **slots = buffer->slots;

                                                                        
  estate->es_result_relation_info = resultRelInfo;

     
                                                                         
                  
     
  cstate->line_buf_valid = false;
  save_cur_lineno = cstate->cur_lineno;

     
                                                                         
                                
     
  oldcontext = MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));
  table_multi_insert(resultRelInfo->ri_RelationDesc, slots, nused, mycid, ti_options, buffer->bistate);
  MemoryContextSwitchTo(oldcontext);

  for (i = 0; i < nused; i++)
  {
       
                                                                          
                                          
       
    if (resultRelInfo->ri_NumIndices > 0)
    {
      List *recheckIndexes;

      cstate->cur_lineno = buffer->linenos[i];
      recheckIndexes = ExecInsertIndexTuples(buffer->slots[i], estate, false, NULL, NIL);
      ExecARInsertTriggers(estate, resultRelInfo, slots[i], recheckIndexes, cstate->transition_capture);
      list_free(recheckIndexes);
    }

       
                                                                      
                        
       
    else if (resultRelInfo->ri_TrigDesc != NULL && (resultRelInfo->ri_TrigDesc->trig_insert_after_row || resultRelInfo->ri_TrigDesc->trig_insert_new_table))
    {
      cstate->cur_lineno = buffer->linenos[i];
      ExecARInsertTriggers(estate, resultRelInfo, slots[i], NIL, cstate->transition_capture);
    }

    ExecClearTuple(slots[i]);
  }

                                    
  buffer->nused = 0;

                                                             
  cstate->line_buf_valid = line_buf_valid;
  cstate->cur_lineno = save_cur_lineno;
}

   
                                                    
   
                                              
   
static inline void
CopyMultiInsertBufferCleanup(CopyMultiInsertInfo *miinfo, CopyMultiInsertBuffer *buffer)
{
  int i;

                                 
  Assert(buffer->nused == 0);

                                   
  buffer->resultRelInfo->ri_CopyMultiInsertBuffer = NULL;

  FreeBulkInsertState(buffer->bistate);

                                                                          
  for (i = 0; i < MAX_BUFFERED_TUPLES && buffer->slots[i] != NULL; i++)
  {
    ExecDropSingleTupleTableSlot(buffer->slots[i]);
  }

  table_finish_bulk_insert(buffer->resultRelInfo->ri_RelationDesc, miinfo->ti_options);

  pfree(buffer);
}

   
                                                                 
   
                                                                               
                                       
   
                                                                              
                                                                      
               
   
static inline void
CopyMultiInsertInfoFlush(CopyMultiInsertInfo *miinfo, ResultRelInfo *curr_rri)
{
  ListCell *lc;

  foreach (lc, miinfo->multiInsertBuffers)
  {
    CopyMultiInsertBuffer *buffer = (CopyMultiInsertBuffer *)lfirst(lc);

    CopyMultiInsertBufferFlush(miinfo, buffer);
  }

  miinfo->bufferedTuples = 0;
  miinfo->bufferedBytes = 0;

     
                                                                             
                                                                            
                                                                         
                             
     
  while (list_length(miinfo->multiInsertBuffers) > MAX_PARTITION_BUFFERS)
  {
    CopyMultiInsertBuffer *buffer;

    buffer = (CopyMultiInsertBuffer *)linitial(miinfo->multiInsertBuffers);

       
                                                                          
                                                                      
       
    if (buffer->resultRelInfo == curr_rri)
    {
      miinfo->multiInsertBuffers = list_delete_first(miinfo->multiInsertBuffers);
      miinfo->multiInsertBuffers = lappend(miinfo->multiInsertBuffers, buffer);
      buffer = (CopyMultiInsertBuffer *)linitial(miinfo->multiInsertBuffers);
    }

    CopyMultiInsertBufferCleanup(miinfo, buffer);
    miinfo->multiInsertBuffers = list_delete_first(miinfo->multiInsertBuffers);
  }
}

   
                                             
   
static inline void
CopyMultiInsertInfoCleanup(CopyMultiInsertInfo *miinfo)
{
  ListCell *lc;

  foreach (lc, miinfo->multiInsertBuffers)
  {
    CopyMultiInsertBufferCleanup(miinfo, lfirst(lc));
  }

  list_free(miinfo->multiInsertBuffers);
}

   
                                                                        
   
                                                    
   
static inline TupleTableSlot *
CopyMultiInsertInfoNextFreeSlot(CopyMultiInsertInfo *miinfo, ResultRelInfo *rri)
{
  CopyMultiInsertBuffer *buffer = rri->ri_CopyMultiInsertBuffer;
  int nused = buffer->nused;

  Assert(buffer != NULL);
  Assert(nused < MAX_BUFFERED_TUPLES);

  if (buffer->slots[nused] == NULL)
  {
    buffer->slots[nused] = table_slot_create(rri->ri_RelationDesc, NULL);
  }
  return buffer->slots[nused];
}

   
                                                                      
                                                      
   
static inline void
CopyMultiInsertInfoStore(CopyMultiInsertInfo *miinfo, ResultRelInfo *rri, TupleTableSlot *slot, int tuplen, uint64 lineno)
{
  CopyMultiInsertBuffer *buffer = rri->ri_CopyMultiInsertBuffer;

  Assert(buffer != NULL);
  Assert(slot == buffer->slots[buffer->nused]);

                                                                        
  buffer->linenos[buffer->nused] = lineno;

                                      
  buffer->nused++;

                                                        
  miinfo->bufferedTuples++;
  miinfo->bufferedBytes += tuplen;
}

   
                               
   
uint64
CopyFrom(CopyState cstate)
{
  ResultRelInfo *resultRelInfo;
  ResultRelInfo *target_resultRelInfo;
  ResultRelInfo *prevResultRelInfo = NULL;
  EState *estate = CreateExecutorState();                            
  ModifyTableState *mtstate;
  ExprContext *econtext;
  TupleTableSlot *singleslot = NULL;
  MemoryContext oldcontext = CurrentMemoryContext;

  PartitionTupleRouting *proute = NULL;
  ErrorContextCallback errcallback;
  CommandId mycid = GetCurrentCommandId(true);
  int ti_options = 0;                                            
  BulkInsertState bistate = NULL;
  CopyInsertMethod insertMethod;
  CopyMultiInsertInfo multiInsertInfo = {0};                      
  uint64 processed = 0;
  bool has_before_insert_row_trig;
  bool has_instead_insert_row_trig;
  bool leafpart_use_multi_insert = false;

  Assert(cstate->rel);

     
                                                                           
                                                                           
                                                                     
     
  if (cstate->rel->rd_rel->relkind != RELKIND_RELATION && cstate->rel->rd_rel->relkind != RELKIND_FOREIGN_TABLE && cstate->rel->rd_rel->relkind != RELKIND_PARTITIONED_TABLE && !(cstate->rel->trigdesc && cstate->rel->trigdesc->trig_insert_instead_row))
  {
    if (cstate->rel->rd_rel->relkind == RELKIND_VIEW)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot copy to view \"%s\"", RelationGetRelationName(cstate->rel)), errhint("To enable copying to a view, provide an INSTEAD OF INSERT trigger.")));
    }
    else if (cstate->rel->rd_rel->relkind == RELKIND_MATVIEW)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot copy to materialized view \"%s\"", RelationGetRelationName(cstate->rel))));
    }
    else if (cstate->rel->rd_rel->relkind == RELKIND_SEQUENCE)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot copy to sequence \"%s\"", RelationGetRelationName(cstate->rel))));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot copy to non-table relation \"%s\"", RelationGetRelationName(cstate->rel))));
    }
  }

               
                                              
     
                                                              
                                                          
                                                                        
                                                                         
                                                                            
                                                                          
                                       
     
                                                                           
                                                                             
                                                                         
                                                                           
                                                             
     
            
                 
                     
                 
                       
              
     
                                                                             
                                                                            
                                                                     
     
                                                                       
                                                                          
                                                                  
                                                                          
                                                                     
     
                                                                          
                                                                        
                                                                           
                                                                           
                                                                           
                                                                        
                
     
                                                                        
                                                                    
                                                                        
                                                     
               
     
                                                                              
  if (RELKIND_HAS_STORAGE(cstate->rel->rd_rel->relkind) && (cstate->rel->rd_createSubid != InvalidSubTransactionId || cstate->rel->rd_newRelfilenodeSubid != InvalidSubTransactionId))
  {
    ti_options |= TABLE_INSERT_SKIP_FSM;
    if (!XLogIsNeeded())
    {
      ti_options |= TABLE_INSERT_SKIP_WAL;
    }
  }

     
                                                                           
                                                                        
                                                                            
                                                                            
                                                                          
                                                                           
                                                                             
                                                                            
                                                  
     
  if (cstate->freeze)
  {
       
                                                                     
                                                                          
                                                                          
                                                                     
                                                                           
                                                                        
                                              
       
    if (cstate->rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot perform COPY FREEZE on a partitioned table")));
    }

       
                                                                       
                                                                         
                                                                         
                                                                        
                                                                    
                                       
       
    InvalidateCatalogSnapshot();
    if (!ThereAreNoPriorRegisteredSnapshots() || !ThereAreNoReadyPortals())
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TRANSACTION_STATE), errmsg("cannot perform COPY FREEZE because of prior transaction activity")));
    }

    if (cstate->rel->rd_createSubid != GetCurrentSubTransactionId() && cstate->rel->rd_newRelfilenodeSubid != GetCurrentSubTransactionId())
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot perform COPY FREEZE because the table was not created or truncated in the current subtransaction")));
    }

    ti_options |= TABLE_INSERT_FROZEN;
  }

     
                                                                  
                                                                            
                                                     
     
  resultRelInfo = makeNode(ResultRelInfo);
  InitResultRelInfo(resultRelInfo, cstate->rel, 1,                                               
      NULL, 0);
  target_resultRelInfo = resultRelInfo;

                                                              
  CheckValidResultRel(resultRelInfo, CMD_INSERT);

  ExecOpenIndices(resultRelInfo, false);

  estate->es_result_relations = resultRelInfo;
  estate->es_num_result_relations = 1;
  estate->es_result_relation_info = resultRelInfo;

  ExecInitRangeTable(estate, cstate->range_table);

     
                                                                        
                                       
     
  mtstate = makeNode(ModifyTableState);
  mtstate->ps.plan = NULL;
  mtstate->ps.state = estate;
  mtstate->operation = CMD_INSERT;
  mtstate->resultRelInfo = estate->es_result_relations;
  mtstate->rootResultRelInfo = estate->es_result_relations;

  if (resultRelInfo->ri_FdwRoutine != NULL && resultRelInfo->ri_FdwRoutine->BeginForeignInsert != NULL)
  {
    resultRelInfo->ri_FdwRoutine->BeginForeignInsert(mtstate, resultRelInfo);
  }

                                        
  AfterTriggerBeginQuery();

     
                                                                             
                                                          
     
                                                                      
                                                                       
                                          
     
  cstate->transition_capture = mtstate->mt_transition_capture = MakeTransitionCaptureState(cstate->rel->trigdesc, RelationGetRelid(cstate->rel), CMD_INSERT);

     
                                                                        
                             
     
  if (cstate->rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    proute = ExecSetupPartitionTupleRouting(estate, NULL, cstate->rel);
  }

  if (cstate->whereClause)
  {
    cstate->qualexpr = ExecInitQual(castNode(List, cstate->whereClause), &mtstate->ps);
  }

     
                                                                    
                                                                            
                                                                           
                                                                       
                      
     
  if (resultRelInfo->ri_TrigDesc != NULL && (resultRelInfo->ri_TrigDesc->trig_insert_before_row || resultRelInfo->ri_TrigDesc->trig_insert_instead_row))
  {
       
                                                                        
                                                                        
                                                                          
                                                                
       
    insertMethod = CIM_SINGLE;
  }
  else if (proute != NULL && resultRelInfo->ri_TrigDesc != NULL && resultRelInfo->ri_TrigDesc->trig_insert_new_table)
  {
       
                                                                        
                                                                        
                                                                          
                                                                           
                                                                 
       
    insertMethod = CIM_SINGLE;
  }
  else if (resultRelInfo->ri_FdwRoutine != NULL || cstate->volatile_defexprs)
  {
       
                                                                         
                                                                    
                                                                      
                       
       
                                                                    
                                                                         
                     
       
    insertMethod = CIM_SINGLE;
  }
  else if (contain_volatile_functions(cstate->whereClause))
  {
       
                                                                      
                                                                          
                                                                  
       
    insertMethod = CIM_SINGLE;
  }
  else
  {
       
                                                                    
                                                                         
                                                                         
                                                                           
                                                                           
                                                                          
                                                                          
                                                                       
                                                                        
                                          
       
    if (proute)
    {
      insertMethod = CIM_MULTI_CONDITIONAL;
    }
    else
    {
      insertMethod = CIM_MULTI;
    }

    CopyMultiInsertInfoInit(&multiInsertInfo, resultRelInfo, cstate, estate, mycid, ti_options);
  }

     
                                                                        
                                                                           
                                                                       
                       
     
  if (insertMethod == CIM_SINGLE || insertMethod == CIM_MULTI_CONDITIONAL)
  {
    singleslot = table_slot_create(resultRelInfo->ri_RelationDesc, &estate->es_tupleTable);
    bistate = GetBulkInsertState();
  }

  has_before_insert_row_trig = (resultRelInfo->ri_TrigDesc && resultRelInfo->ri_TrigDesc->trig_insert_before_row);

  has_instead_insert_row_trig = (resultRelInfo->ri_TrigDesc && resultRelInfo->ri_TrigDesc->trig_insert_instead_row);

     
                                                                          
                                                                             
                                                                            
                                                     
     
  ExecBSInsertTriggers(estate, resultRelInfo);

  econtext = GetPerTupleExprContext(estate);

                                                     
  errcallback.callback = CopyFromErrorCallback;
  errcallback.arg = (void *)cstate;
  errcallback.previous = error_context_stack;
  error_context_stack = &errcallback;

  for (;;)
  {
    TupleTableSlot *myslot;
    bool skip_tuple;

    CHECK_FOR_INTERRUPTS();

       
                                                                         
                                                  
       
    ResetPerTupleExprContext(estate);

                                                  
    if (insertMethod == CIM_SINGLE || proute)
    {
      myslot = singleslot;
      Assert(myslot != NULL);
    }
    else
    {
      Assert(resultRelInfo == target_resultRelInfo);
      Assert(insertMethod == CIM_MULTI);

      myslot = CopyMultiInsertInfoNextFreeSlot(&multiInsertInfo, resultRelInfo);
    }

       
                                                                           
                                                                         
       
    MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));

    ExecClearTuple(myslot);

                                                           
    if (!NextCopyFrom(cstate, econtext, myslot->tts_values, myslot->tts_isnull))
    {
      break;
    }

    ExecStoreVirtualTuple(myslot);

       
                                                                         
                                                               
       
    myslot->tts_tableOid = RelationGetRelid(target_resultRelInfo->ri_RelationDesc);

                                                                 
    MemoryContextSwitchTo(oldcontext);

    if (cstate->whereClause)
    {
      econtext->ecxt_scantuple = myslot;
                                                           
      if (!ExecQual(cstate->qualexpr, econtext))
      {
        continue;
      }
    }

                                                          
    if (proute)
    {
      TupleConversionMap *map;

         
                                                              
                                                                         
                                                             
         
      resultRelInfo = ExecFindPartition(mtstate, target_resultRelInfo, proute, myslot, estate);

      if (prevResultRelInfo != resultRelInfo)
      {
                                                              
        has_before_insert_row_trig = (resultRelInfo->ri_TrigDesc && resultRelInfo->ri_TrigDesc->trig_insert_before_row);

        has_instead_insert_row_trig = (resultRelInfo->ri_TrigDesc && resultRelInfo->ri_TrigDesc->trig_insert_instead_row);

           
                                                                       
                                                                    
           
        leafpart_use_multi_insert = insertMethod == CIM_MULTI_CONDITIONAL && !has_before_insert_row_trig && !has_instead_insert_row_trig && resultRelInfo->ri_FdwRoutine == NULL;

                                                                    
        if (leafpart_use_multi_insert)
        {
          if (resultRelInfo->ri_CopyMultiInsertBuffer == NULL)
          {
            CopyMultiInsertInfoSetupBuffer(&multiInsertInfo, resultRelInfo);
          }
        }
        else if (insertMethod == CIM_MULTI_CONDITIONAL && !CopyMultiInsertInfoIsEmpty(&multiInsertInfo))
        {
             
                                                               
                                                            
             
          CopyMultiInsertInfoFlush(&multiInsertInfo, resultRelInfo);
        }

        if (bistate != NULL)
        {
          ReleaseBulkInsertStatePin(bistate);
        }
        prevResultRelInfo = resultRelInfo;
      }

         
                                                                        
         
      estate->es_result_relation_info = resultRelInfo;

         
                                                                        
                                                     
         
      if (cstate->transition_capture != NULL)
      {
        if (has_before_insert_row_trig)
        {
             
                                                                
                                                                    
                                
             
          cstate->transition_capture->tcs_original_insert_tuple = NULL;
          cstate->transition_capture->tcs_map = resultRelInfo->ri_PartitionInfo->pi_PartitionToRootMap;
        }
        else
        {
             
                                                               
                                                               
             
          cstate->transition_capture->tcs_original_insert_tuple = myslot;
          cstate->transition_capture->tcs_map = NULL;
        }
      }

         
                                                                         
                  
         
      map = resultRelInfo->ri_PartitionInfo->pi_RootToPartitionMap;
      if (insertMethod == CIM_SINGLE || !leafpart_use_multi_insert)
      {
                              
        if (map != NULL)
        {
          TupleTableSlot *new_slot;

          new_slot = resultRelInfo->ri_PartitionInfo->pi_PartitionTupleSlot;
          myslot = execute_attr_map_slot(map->attrMap, myslot, new_slot);
        }
      }
      else
      {
           
                                                                 
                              
           
        TupleTableSlot *batchslot;

                                                           
        Assert(insertMethod == CIM_MULTI_CONDITIONAL);

        batchslot = CopyMultiInsertInfoNextFreeSlot(&multiInsertInfo, resultRelInfo);

        if (map != NULL)
        {
          myslot = execute_attr_map_slot(map->attrMap, myslot, batchslot);
        }
        else
        {
             
                                                                 
                                                                 
                                                                   
                                                                   
                                         
             
          ExecCopySlot(batchslot, myslot);
          myslot = batchslot;
        }
      }

                                                            
      myslot->tts_tableOid = RelationGetRelid(resultRelInfo->ri_RelationDesc);
    }

    skip_tuple = false;

                                    
    if (has_before_insert_row_trig)
    {
      if (!ExecBRInsertTriggers(estate, resultRelInfo, myslot))
      {
        skip_tuple = true;                   
      }
    }

    if (!skip_tuple)
    {
         
                                                                         
                                                                      
                                 
         
      if (has_instead_insert_row_trig)
      {
        ExecIRInsertTriggers(estate, resultRelInfo, myslot);
      }
      else
      {
                                              
        if (resultRelInfo->ri_RelationDesc->rd_att->constr && resultRelInfo->ri_RelationDesc->rd_att->constr->has_generated_stored)
        {
          ExecComputeStoredGenerated(estate, myslot);
        }

           
                                                                    
                      
           
        if (resultRelInfo->ri_FdwRoutine == NULL && resultRelInfo->ri_RelationDesc->rd_att->constr)
        {
          ExecConstraints(resultRelInfo, myslot, estate);
        }

           
                                                                     
                                                                       
                                                                    
                      
           
        if (resultRelInfo->ri_PartitionCheck && (proute == NULL || has_before_insert_row_trig))
        {
          ExecPartitionCheck(resultRelInfo, myslot, estate, true);
        }

                                                                      
        if (insertMethod == CIM_MULTI || leafpart_use_multi_insert)
        {
             
                                                                
                                                                
             
          ExecMaterializeSlot(myslot);

                                                  
          CopyMultiInsertInfoStore(&multiInsertInfo, resultRelInfo, myslot, cstate->line_buf.len, cstate->cur_lineno);

             
                                                              
                                          
             
          if (CopyMultiInsertInfoIsFull(&multiInsertInfo))
          {
            CopyMultiInsertInfoFlush(&multiInsertInfo, resultRelInfo);
          }
        }
        else
        {
          List *recheckIndexes = NIL;

                                   
          if (resultRelInfo->ri_FdwRoutine != NULL)
          {
            myslot = resultRelInfo->ri_FdwRoutine->ExecForeignInsert(estate, resultRelInfo, myslot, NULL);

            if (myslot == NULL)                   
            {
              continue;                        
            }

               
                                                               
                                                              
                                
               
            myslot->tts_tableOid = RelationGetRelid(resultRelInfo->ri_RelationDesc);
          }
          else
          {
                                                                     
            table_tuple_insert(resultRelInfo->ri_RelationDesc, myslot, mycid, ti_options, bistate);

            if (resultRelInfo->ri_NumIndices > 0)
            {
              recheckIndexes = ExecInsertIndexTuples(myslot, estate, false, NULL, NIL);
            }
          }

                                         
          ExecARInsertTriggers(estate, resultRelInfo, myslot, recheckIndexes, cstate->transition_capture);

          list_free(recheckIndexes);
        }
      }

         
                                                                        
                                                                       
                                                            
         
      processed++;
    }
  }

                                           
  if (insertMethod != CIM_SINGLE)
  {
    if (!CopyMultiInsertInfoIsEmpty(&multiInsertInfo))
    {
      CopyMultiInsertInfoFlush(&multiInsertInfo, NULL);
    }
  }

                      
  error_context_stack = errcallback.previous;

  if (bistate != NULL)
  {
    FreeBulkInsertState(bistate);
  }

  MemoryContextSwitchTo(oldcontext);

     
                                                                          
                     
     
  if (cstate->copy_dest == COPY_OLD_FE)
  {
    pq_endmsgread();
  }

                                                  
  ExecASInsertTriggers(estate, target_resultRelInfo, cstate->transition_capture);

                                    
  AfterTriggerEndQuery(estate);

  ExecResetTupleTable(estate->es_tupleTable, false);

                                  
  if (target_resultRelInfo->ri_FdwRoutine != NULL && target_resultRelInfo->ri_FdwRoutine->EndForeignInsert != NULL)
  {
    target_resultRelInfo->ri_FdwRoutine->EndForeignInsert(estate, target_resultRelInfo);
  }

                                              
  if (insertMethod != CIM_SINGLE)
  {
    CopyMultiInsertInfoCleanup(&multiInsertInfo);
  }

  ExecCloseIndices(target_resultRelInfo);

                                                                            
  if (proute)
  {
    ExecCleanupTupleRouting(mtstate, proute);
  }

                                          
  ExecCleanUpTriggerState(estate);

  FreeExecutorState(estate);

  return processed;
}

   
                                                   
   
                                            
                                                 
                                                                            
                                                                           
   
                                                                            
   
CopyState
BeginCopyFrom(ParseState *pstate, Relation rel, const char *filename, bool is_program, copy_data_source_cb data_source_cb, List *attnamelist, List *options)
{
  CopyState cstate;
  bool pipe = (filename == NULL);
  TupleDesc tupDesc;
  AttrNumber num_phys_attrs, num_defaults;
  FmgrInfo *in_functions;
  Oid *typioparams;
  int attnum;
  Oid in_func_oid;
  int *defmap;
  ExprState **defexprs;
  MemoryContext oldcontext;
  bool volatile_defexprs;

  cstate = BeginCopy(pstate, true, rel, NULL, InvalidOid, attnamelist, options);
  oldcontext = MemoryContextSwitchTo(cstate->copycontext);

                                  
  cstate->reached_eof = false;
  cstate->eol_type = EOL_UNKNOWN;
  cstate->cur_relname = RelationGetRelationName(cstate->rel);
  cstate->cur_lineno = 0;
  cstate->cur_attname = NULL;
  cstate->cur_attval = NULL;

                                                         
  initStringInfo(&cstate->attribute_buf);
  initStringInfo(&cstate->line_buf);
  cstate->line_buf_converted = false;
  cstate->raw_buf = (char *)palloc(RAW_BUF_SIZE + 1);
  cstate->raw_buf_index = cstate->raw_buf_len = 0;

                                                      
  if (pstate)
  {
    cstate->range_table = pstate->p_rtable;
  }

  tupDesc = RelationGetDescr(cstate->rel);
  num_phys_attrs = tupDesc->natts;
  num_defaults = 0;
  volatile_defexprs = false;

     
                                                                        
                                                                          
                                                                          
                                                                  
     
  in_functions = (FmgrInfo *)palloc(num_phys_attrs * sizeof(FmgrInfo));
  typioparams = (Oid *)palloc(num_phys_attrs * sizeof(Oid));
  defmap = (int *)palloc(num_phys_attrs * sizeof(int));
  defexprs = (ExprState **)palloc(num_phys_attrs * sizeof(ExprState *));

  for (attnum = 1; attnum <= num_phys_attrs; attnum++)
  {
    Form_pg_attribute att = TupleDescAttr(tupDesc, attnum - 1);

                                                   
    if (att->attisdropped)
    {
      continue;
    }

                                                      
    if (cstate->binary)
    {
      getTypeBinaryInputInfo(att->atttypid, &in_func_oid, &typioparams[attnum - 1]);
    }
    else
    {
      getTypeInputInfo(att->atttypid, &in_func_oid, &typioparams[attnum - 1]);
    }
    fmgr_info(in_func_oid, &in_functions[attnum - 1]);

                                    
    if (!list_member_int(cstate->attnumlist, attnum) && !att->attgenerated)
    {
                                                    
                                           
      Expr *defexpr = (Expr *)build_column_default(cstate->rel, attnum);

      if (defexpr != NULL)
      {
                                                
        defexpr = expression_planner(defexpr);

                                                             
        defexprs[num_defaults] = ExecInitExpr(defexpr, NULL);
        defmap[num_defaults] = attnum - 1;
        num_defaults++;

           
                                                                    
                                                          
                                                                      
                                                                      
                                                                    
                                                                  
                                                                  
                                                          
                                                                 
                                                      
                                         
           
        if (!volatile_defexprs)
        {
          volatile_defexprs = contain_volatile_functions_not_nextval((Node *)defexpr);
        }
      }
    }
  }

                                          
  cstate->in_functions = in_functions;
  cstate->typioparams = typioparams;
  cstate->defmap = defmap;
  cstate->defexprs = defexprs;
  cstate->volatile_defexprs = volatile_defexprs;
  cstate->num_defaults = num_defaults;
  cstate->is_program = is_program;

  if (data_source_cb)
  {
    cstate->copy_dest = COPY_CALLBACK;
    cstate->data_source_cb = data_source_cb;
  }
  else if (pipe)
  {
    Assert(!is_program);                                      
    if (whereToSendOutput == DestRemote)
    {
      ReceiveCopyBegin(cstate);
    }
    else
    {
      cstate->copy_file = stdin;
    }
  }
  else
  {
    cstate->filename = pstrdup(filename);

    if (cstate->is_program)
    {
      cstate->copy_file = OpenPipeStream(cstate->filename, PG_BINARY_R);
      if (cstate->copy_file == NULL)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not execute command \"%s\": %m", cstate->filename)));
      }
    }
    else
    {
      struct stat st;

      cstate->copy_file = AllocateFile(cstate->filename, PG_BINARY_R);
      if (cstate->copy_file == NULL)
      {
                                                                     
        int save_errno = errno;

        ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\" for reading: %m", cstate->filename),
                           (save_errno == ENOENT || save_errno == EACCES) ? errhint("COPY FROM instructs the PostgreSQL server process to read a file. "
                                                                                    "You may want a client-side facility such as psql's \\copy.")
                                                                          : 0));
      }

      if (fstat(fileno(cstate->copy_file), &st))
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", cstate->filename)));
      }

      if (S_ISDIR(st.st_mode))
      {
        ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is a directory", cstate->filename)));
      }
    }
  }

  if (cstate->binary)
  {
                                       
    char readSig[11];
    int32 tmp;

                   
    if (CopyGetData(cstate, readSig, 11, 11) != 11 || memcmp(readSig, BinarySignature, 11) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), errmsg("COPY file signature not recognized")));
    }
                     
    if (!CopyGetInt32(cstate, &tmp))
    {
      ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), errmsg("invalid COPY file header (missing flags)")));
    }
    if ((tmp & (1 << 16)) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), errmsg("invalid COPY file header (WITH OIDS)")));
    }
    tmp &= ~(1 << 16);
    if ((tmp >> 16) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), errmsg("unrecognized critical flags in COPY file header")));
    }
                                 
    if (!CopyGetInt32(cstate, &tmp) || tmp < 0)
    {
      ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), errmsg("invalid COPY file header (missing length)")));
    }
                                           
    while (tmp-- > 0)
    {
      if (CopyGetData(cstate, readSig, 1, 1) != 1)
      {
        ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), errmsg("invalid COPY file header (wrong length)")));
      }
    }
  }

                                                       
  if (!cstate->binary)
  {
    AttrNumber attr_count = list_length(cstate->attnumlist);

    cstate->max_fields = attr_count;
    cstate->raw_fields = (char **)palloc(attr_count * sizeof(char *));
  }

  MemoryContextSwitchTo(oldcontext);

  return cstate;
}

   
                                                                       
                                  
   
                                                                            
                                                                            
                                                                              
                    
   
                                                                       
   
bool
NextCopyFromRawFields(CopyState cstate, char ***fields, int *nfields)
{
  int fldct;
  bool done;

                                            
  Assert(!cstate->binary);

                                                
  if (cstate->cur_lineno == 0 && cstate->header_line)
  {
    cstate->cur_lineno++;
    if (CopyReadLine(cstate))
    {
      return false;           
    }
  }

  cstate->cur_lineno++;

                                               
  done = CopyReadLine(cstate);

     
                                                                      
                                                                      
                                                            
     
  if (done && cstate->line_buf.len == 0)
  {
    return false;
  }

                                                   
  if (cstate->csv_mode)
  {
    fldct = CopyReadAttributesCSV(cstate);
  }
  else
  {
    fldct = CopyReadAttributesText(cstate);
  }

  *fields = cstate->raw_fields;
  *nfields = fldct;
  return true;
}

   
                                                                            
   
                                                                         
                                                                            
                                            
   
                                                                         
                                                                     
   
bool
NextCopyFrom(CopyState cstate, ExprContext *econtext, Datum *values, bool *nulls)
{
  TupleDesc tupDesc;
  AttrNumber num_phys_attrs, attr_count, num_defaults = cstate->num_defaults;
  FmgrInfo *in_functions = cstate->in_functions;
  Oid *typioparams = cstate->typioparams;
  int i;
  int *defmap = cstate->defmap;
  ExprState **defexprs = cstate->defexprs;

  tupDesc = RelationGetDescr(cstate->rel);
  num_phys_attrs = tupDesc->natts;
  attr_count = list_length(cstate->attnumlist);

                                             
  MemSet(values, 0, num_phys_attrs * sizeof(Datum));
  MemSet(nulls, true, num_phys_attrs * sizeof(bool));

  if (!cstate->binary)
  {
    char **field_strings;
    ListCell *cur;
    int fldct;
    int fieldno;
    char *string;

                                          
    if (!NextCopyFromRawFields(cstate, &field_strings, &fldct))
    {
      return false;
    }

                                      
    if (attr_count > 0 && fldct > attr_count)
    {
      ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), errmsg("extra data after last expected column")));
    }

    fieldno = 0;

                                                       
    foreach (cur, cstate->attnumlist)
    {
      int attnum = lfirst_int(cur);
      int m = attnum - 1;
      Form_pg_attribute att = TupleDescAttr(tupDesc, m);

      if (fieldno >= fldct)
      {
        ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), errmsg("missing data for column \"%s\"", NameStr(att->attname))));
      }
      string = field_strings[fieldno++];

      if (cstate->convert_select_flags && !cstate->convert_select_flags[m])
      {
                                                        
        continue;
      }

      if (cstate->csv_mode)
      {
        if (string == NULL && cstate->force_notnull_flags[m])
        {
             
                                                               
                                            
             
          string = cstate->null_print;
        }
        else if (string != NULL && cstate->force_null_flags[m] && strcmp(string, cstate->null_print) == 0)
        {
             
                                                                  
                                                                
                                                                    
                                   
             
          string = NULL;
        }
      }

      cstate->cur_attname = NameStr(att->attname);
      cstate->cur_attval = string;
      values[m] = InputFunctionCall(&in_functions[m], string, typioparams[m], att->atttypmod);
      if (string != NULL)
      {
        nulls[m] = false;
      }
      cstate->cur_attname = NULL;
      cstate->cur_attval = NULL;
    }

    Assert(fieldno == attr_count);
  }
  else
  {
                
    int16 fld_count;
    ListCell *cur;

    cstate->cur_lineno++;

    if (!CopyGetInt16(cstate, &fld_count))
    {
                                                             
      return false;
    }

    if (fld_count == -1)
    {
         
                                                                   
                                                             
                                                                       
                                             
         
                                                                        
                                                                     
                                                                     
                                                                     
                                     
         
      char dummy;

      if (cstate->copy_dest != COPY_OLD_FE && CopyGetData(cstate, &dummy, 1, 1) > 0)
      {
        ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), errmsg("received copy data after EOF marker")));
      }
      return false;
    }

    if (fld_count != attr_count)
    {
      ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), errmsg("row field count is %d, expected %d", (int)fld_count, attr_count)));
    }

    i = 0;
    foreach (cur, cstate->attnumlist)
    {
      int attnum = lfirst_int(cur);
      int m = attnum - 1;
      Form_pg_attribute att = TupleDescAttr(tupDesc, m);

      cstate->cur_attname = NameStr(att->attname);
      i++;
      values[m] = CopyReadBinaryAttribute(cstate, i, &in_functions[m], typioparams[m], att->atttypmod, &nulls[m]);
      cstate->cur_attname = NULL;
    }
  }

     
                                                                       
                                                                            
                  
     
  for (i = 0; i < num_defaults; i++)
  {
       
                                                                  
                                       
       
    Assert(econtext != NULL);
    Assert(CurrentMemoryContext == econtext->ecxt_per_tuple_memory);

    values[defmap[i]] = ExecEvalExpr(defexprs[i], econtext, &nulls[defmap[i]]);
  }

  return true;
}

   
                                                         
   
void
EndCopyFrom(CopyState cstate)
{
                                                     

  EndCopy(cstate);
}

   
                                                                         
                    
   
                                                                     
                                                                      
                                   
   
static bool
CopyReadLine(CopyState cstate)
{
  bool result;

  resetStringInfo(&cstate->line_buf);
  cstate->line_buf_valid = true;

                                                         
  cstate->line_buf_converted = false;

                                             
  result = CopyReadLineText(cstate);

  if (result)
  {
       
                                                                      
                                                                        
                                    
       
    if (cstate->copy_dest == COPY_NEW_FE)
    {
      do
      {
        cstate->raw_buf_index = cstate->raw_buf_len;
      } while (CopyLoadRawBuf(cstate));
    }
  }
  else
  {
       
                                                                          
                                                        
       
    switch (cstate->eol_type)
    {
    case EOL_NL:
      Assert(cstate->line_buf.len >= 1);
      Assert(cstate->line_buf.data[cstate->line_buf.len - 1] == '\n');
      cstate->line_buf.len--;
      cstate->line_buf.data[cstate->line_buf.len] = '\0';
      break;
    case EOL_CR:
      Assert(cstate->line_buf.len >= 1);
      Assert(cstate->line_buf.data[cstate->line_buf.len - 1] == '\r');
      cstate->line_buf.len--;
      cstate->line_buf.data[cstate->line_buf.len] = '\0';
      break;
    case EOL_CRNL:
      Assert(cstate->line_buf.len >= 2);
      Assert(cstate->line_buf.data[cstate->line_buf.len - 2] == '\r');
      Assert(cstate->line_buf.data[cstate->line_buf.len - 1] == '\n');
      cstate->line_buf.len -= 2;
      cstate->line_buf.data[cstate->line_buf.len] = '\0';
      break;
    case EOL_UNKNOWN:
                              
      Assert(false);
      break;
    }
  }

                                                              
  if (cstate->need_transcoding)
  {
    char *cvt;

    cvt = pg_any_to_server(cstate->line_buf.data, cstate->line_buf.len, cstate->file_encoding);
    if (cvt != cstate->line_buf.data)
    {
                                                    
      resetStringInfo(&cstate->line_buf);
      appendBinaryStringInfo(&cstate->line_buf, cvt, strlen(cvt));
      pfree(cvt);
    }
  }

                                                         
  cstate->line_buf_converted = true;

  return result;
}

   
                                                               
   
static bool
CopyReadLineText(CopyState cstate)
{
  char *copy_raw_buf;
  int raw_buf_ptr;
  int copy_buf_len;
  bool need_data = false;
  bool hit_eof = false;
  bool result = false;
  char mblen_str[2];

                     
  bool first_char_in_line = true;
  bool in_quote = false, last_was_esc = false;
  char quotec = '\0';
  char escapec = '\0';

  if (cstate->csv_mode)
  {
    quotec = cstate->quote[0];
    escapec = cstate->escape[0];
                                                                     
    if (quotec == escapec)
    {
      escapec = '\0';
    }
  }

  mblen_str[1] = '\0';

     
                                                                          
                                                                           
                                          
     
                                                                            
                                                                             
                                             
     
                                                                         
                                                         
     
                                                                       
                                                                          
                                                                            
                                                                             
               
     
                                                                   
                                       
     
  copy_raw_buf = cstate->raw_buf;
  raw_buf_ptr = cstate->raw_buf_index;
  copy_buf_len = cstate->raw_buf_len;

  for (;;)
  {
    int prev_raw_ptr;
    char c;

       
                                                                         
                                                 
                                                                           
                                                                         
                                                                    
                                                             
                                                                       
                                           
       
    if (raw_buf_ptr >= copy_buf_len || need_data)
    {
      REFILL_LINEBUF;

         
                                                                
                                                                 
         
      if (!CopyLoadRawBuf(cstate))
      {
        hit_eof = true;
      }
      raw_buf_ptr = 0;
      copy_buf_len = cstate->raw_buf_len;

         
                                                                  
                        
         
      if (copy_buf_len <= 0)
      {
        result = true;
        break;
      }
      need_data = false;
    }

                                 
    prev_raw_ptr = raw_buf_ptr;
    c = copy_raw_buf[raw_buf_ptr++];

    if (cstate->csv_mode)
    {
         
                                                                        
                                                                        
                                                                      
                                                                 
         
                                                                       
                                                 
         
      if (c == '\\' || c == '\r')
      {
        IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(0);
      }

         
                                                                       
                                                                     
                                                                       
                                                                   
                                                                      
                                                  
         
      if (in_quote && c == escapec)
      {
        last_was_esc = !last_was_esc;
      }
      if (c == quotec && !last_was_esc)
      {
        in_quote = !in_quote;
      }
      if (c != escapec)
      {
        last_was_esc = false;
      }

         
                                                                    
                                                                        
                                                                       
                                                                
         
      if (in_quote && c == (cstate->eol_type == EOL_NL ? '\n' : '\r'))
      {
        cstate->cur_lineno++;
      }
    }

                    
    if (c == '\r' && (!cstate->csv_mode || !in_quote))
    {
                                                            
      if (cstate->eol_type == EOL_UNKNOWN || cstate->eol_type == EOL_CRNL)
      {
           
                                                              
           
                                                                      
                                             
           
        IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(0);

                           
        c = copy_raw_buf[raw_buf_ptr];

        if (c == '\n')
        {
          raw_buf_ptr++;                                
          cstate->eol_type = EOL_CRNL;                          
        }
        else
        {
                                   
          if (cstate->eol_type == EOL_CRNL)
          {
            ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), !cstate->csv_mode ? errmsg("literal carriage return found in data") : errmsg("unquoted carriage return found in data"), !cstate->csv_mode ? errhint("Use \"\\r\" to represent carriage return.") : errhint("Use quoted CSV field to represent carriage return.")));
          }

             
                                                                     
                                                       
             
          cstate->eol_type = EOL_CR;
        }
      }
      else if (cstate->eol_type == EOL_NL)
      {
        ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), !cstate->csv_mode ? errmsg("literal carriage return found in data") : errmsg("unquoted carriage return found in data"), !cstate->csv_mode ? errhint("Use \"\\r\" to represent carriage return.") : errhint("Use quoted CSV field to represent carriage return.")));
      }
                                                            
      break;
    }

                    
    if (c == '\n' && (!cstate->csv_mode || !in_quote))
    {
      if (cstate->eol_type == EOL_CR || cstate->eol_type == EOL_CRNL)
      {
        ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), !cstate->csv_mode ? errmsg("literal newline found in data") : errmsg("unquoted newline found in data"), !cstate->csv_mode ? errhint("Use \"\\n\" to represent newline.") : errhint("Use quoted CSV field to represent newline.")));
      }
      cstate->eol_type = EOL_NL;                          
                                                            
      break;
    }

       
                                                                           
                                     
       
    if (c == '\\' && (!cstate->csv_mode || first_char_in_line))
    {
      char c2;

      IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(0);
      IF_NEED_REFILL_AND_EOF_BREAK(0);

               
                            
                                                                 
                                                            
               
         
      c2 = copy_raw_buf[raw_buf_ptr];

      if (c2 == '.')
      {
        raw_buf_ptr++;                      

           
                                                                 
                                                                       
                                                                
           
        if (cstate->eol_type == EOL_CRNL)
        {
                                      
          IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(0);
                                               
          c2 = copy_raw_buf[raw_buf_ptr++];

          if (c2 == '\n')
          {
            if (!cstate->csv_mode)
            {
              ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), errmsg("end-of-copy marker does not match previous newline style")));
            }
            else
            {
              NO_END_OF_COPY_GOTO;
            }
          }
          else if (c2 != '\r')
          {
            if (!cstate->csv_mode)
            {
              ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), errmsg("end-of-copy marker corrupt")));
            }
            else
            {
              NO_END_OF_COPY_GOTO;
            }
          }
        }

                                    
        IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(0);
                                             
        c2 = copy_raw_buf[raw_buf_ptr++];

        if (c2 != '\r' && c2 != '\n')
        {
          if (!cstate->csv_mode)
          {
            ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), errmsg("end-of-copy marker corrupt")));
          }
          else
          {
            NO_END_OF_COPY_GOTO;
          }
        }

        if ((cstate->eol_type == EOL_NL && c2 != '\n') || (cstate->eol_type == EOL_CRNL && c2 != '\n') || (cstate->eol_type == EOL_CR && c2 != '\r'))
        {
          ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), errmsg("end-of-copy marker does not match previous newline style")));
        }

           
                                                                    
                                                 
           
        if (prev_raw_ptr > cstate->raw_buf_index)
        {
          appendBinaryStringInfo(&cstate->line_buf, cstate->raw_buf + cstate->raw_buf_index, prev_raw_ptr - cstate->raw_buf_index);
        }
        cstate->raw_buf_index = raw_buf_ptr;
        result = true;                 
        break;
      }
      else if (!cstate->csv_mode)
      {
           
                                                                     
                                                                     
                                                                     
                                                             
                                                                    
                                                                 
                                                                  
                                                                       
                                                 
           
                                                                   
                                                                   
                                                                    
                                                                       
                            
           
        raw_buf_ptr++;
        c = c2;
      }
    }

       
                                                                      
                                                                           
                                                                        
                                                                
       
  not_end_of_copy:

       
                                                               
       
                                                                         
                                                                   
                                  
       
    if (cstate->encoding_embeds_ascii && IS_HIGHBIT_SET(c))
    {
      int mblen;

         
                                                                         
                                                                         
                                                          
         
      mblen_str[0] = c;
      mblen = pg_encoding_mblen(cstate->file_encoding, mblen_str);

      IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(mblen - 1);
      IF_NEED_REFILL_AND_EOF_BREAK(mblen - 1);
      raw_buf_ptr += mblen - 1;
    }
    first_char_in_line = false;
  }                        

     
                                                   
     
  REFILL_LINEBUF;

  return result;
}

   
                                                
   
static int
GetDecimalFromHex(char hex)
{
  if (isdigit((unsigned char)hex))
  {
    return hex - '0';
  }
  else
  {
    return tolower((unsigned char)hex) - 'a' + 10;
  }
}

   
                                                             
                                     
   
                                                                      
                                                                         
                                                                  
                                        
   
                                                                   
                                                                   
                                        
   
                                                                         
                                                                        
                                    
   
                                                           
   
static int
CopyReadAttributesText(CopyState cstate)
{
  char delimc = cstate->delim[0];
  int fieldno;
  char *output_ptr;
  char *cur_ptr;
  char *line_end_ptr;

     
                                                                         
                                
     
  if (cstate->max_fields <= 0)
  {
    if (cstate->line_buf.len != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), errmsg("extra data after last expected column")));
    }
    return 0;
  }

  resetStringInfo(&cstate->attribute_buf);

     
                                                                           
                                                                          
                                                                            
                                                                             
                                                        
     
  if (cstate->attribute_buf.maxlen <= cstate->line_buf.len)
  {
    enlargeStringInfo(&cstate->attribute_buf, cstate->line_buf.len);
  }
  output_ptr = cstate->attribute_buf.data;

                                      
  cur_ptr = cstate->line_buf.data;
  line_end_ptr = cstate->line_buf.data + cstate->line_buf.len;

                                       
  fieldno = 0;
  for (;;)
  {
    bool found_delim = false;
    char *start_ptr;
    char *end_ptr;
    int input_len;
    bool saw_non_ascii = false;

                                                            
    if (fieldno >= cstate->max_fields)
    {
      cstate->max_fields *= 2;
      cstate->raw_fields = repalloc(cstate->raw_fields, cstate->max_fields * sizeof(char *));
    }

                                                                
    start_ptr = cur_ptr;
    cstate->raw_fields[fieldno] = output_ptr;

       
                            
       
                                                                          
                                                                        
                                                                          
                                                                        
                                                                         
                                                                      
              
       
    for (;;)
    {
      char c;

      end_ptr = cur_ptr;
      if (cur_ptr >= line_end_ptr)
      {
        break;
      }
      c = *cur_ptr++;
      if (c == delimc)
      {
        found_delim = true;
        break;
      }
      if (c == '\\')
      {
        if (cur_ptr >= line_end_ptr)
        {
          break;
        }
        c = *cur_ptr++;
        switch (c)
        {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        {
                           
          int val;

          val = OCTVALUE(c);
          if (cur_ptr < line_end_ptr)
          {
            c = *cur_ptr;
            if (ISOCTAL(c))
            {
              cur_ptr++;
              val = (val << 3) + OCTVALUE(c);
              if (cur_ptr < line_end_ptr)
              {
                c = *cur_ptr;
                if (ISOCTAL(c))
                {
                  cur_ptr++;
                  val = (val << 3) + OCTVALUE(c);
                }
              }
            }
          }
          c = val & 0377;
          if (c == '\0' || IS_HIGHBIT_SET(c))
          {
            saw_non_ascii = true;
          }
        }
        break;
        case 'x':
                           
          if (cur_ptr < line_end_ptr)
          {
            char hexchar = *cur_ptr;

            if (isxdigit((unsigned char)hexchar))
            {
              int val = GetDecimalFromHex(hexchar);

              cur_ptr++;
              if (cur_ptr < line_end_ptr)
              {
                hexchar = *cur_ptr;
                if (isxdigit((unsigned char)hexchar))
                {
                  cur_ptr++;
                  val = (val << 4) + GetDecimalFromHex(hexchar);
                }
              }
              c = val & 0xff;
              if (c == '\0' || IS_HIGHBIT_SET(c))
              {
                saw_non_ascii = true;
              }
            }
          }
          break;
        case 'b':
          c = '\b';
          break;
        case 'f':
          c = '\f';
          break;
        case 'n':
          c = '\n';
          break;
        case 'r':
          c = '\r';
          break;
        case 't':
          c = '\t';
          break;
        case 'v':
          c = '\v';
          break;

             
                                                         
                       
             
        }
      }

                                  
      *output_ptr++ = c;
    }

                                                     
    input_len = end_ptr - start_ptr;
    if (input_len == cstate->null_print_len && strncmp(start_ptr, cstate->null_print, input_len) == 0)
    {
      cstate->raw_fields[fieldno] = NULL;
    }
    else
    {
         
                                                                      
         
                                                                   
                                                             
         
      if (saw_non_ascii)
      {
        char *fld = cstate->raw_fields[fieldno];

        pg_verifymbstr(fld, output_ptr - fld, false);
      }
    }

                                                  
    *output_ptr++ = '\0';

    fieldno++;
                                               
    if (!found_delim)
    {
      break;
    }
  }

                                       
  output_ptr--;
  Assert(*output_ptr == '\0');
  cstate->attribute_buf.len = (output_ptr - cstate->attribute_buf.data);

  return fieldno;
}

   
                                                             
                                                                       
                                                                   
                                       
   
static int
CopyReadAttributesCSV(CopyState cstate)
{
  char delimc = cstate->delim[0];
  char quotec = cstate->quote[0];
  char escapec = cstate->escape[0];
  int fieldno;
  char *output_ptr;
  char *cur_ptr;
  char *line_end_ptr;

     
                                                                         
                                
     
  if (cstate->max_fields <= 0)
  {
    if (cstate->line_buf.len != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), errmsg("extra data after last expected column")));
    }
    return 0;
  }

  resetStringInfo(&cstate->attribute_buf);

     
                                                                           
                                                                          
                                                                            
                                                                             
                                                        
     
  if (cstate->attribute_buf.maxlen <= cstate->line_buf.len)
  {
    enlargeStringInfo(&cstate->attribute_buf, cstate->line_buf.len);
  }
  output_ptr = cstate->attribute_buf.data;

                                      
  cur_ptr = cstate->line_buf.data;
  line_end_ptr = cstate->line_buf.data + cstate->line_buf.len;

                                       
  fieldno = 0;
  for (;;)
  {
    bool found_delim = false;
    bool saw_quote = false;
    char *start_ptr;
    char *end_ptr;
    int input_len;

                                                            
    if (fieldno >= cstate->max_fields)
    {
      cstate->max_fields *= 2;
      cstate->raw_fields = repalloc(cstate->raw_fields, cstate->max_fields * sizeof(char *));
    }

                                                                
    start_ptr = cur_ptr;
    cstate->raw_fields[fieldno] = output_ptr;

       
                            
       
                                                                         
                                                                     
                                                        
       
    for (;;)
    {
      char c;

                        
      for (;;)
      {
        end_ptr = cur_ptr;
        if (cur_ptr >= line_end_ptr)
        {
          goto endfield;
        }
        c = *cur_ptr++;
                                      
        if (c == delimc)
        {
          found_delim = true;
          goto endfield;
        }
                                                      
        if (c == quotec)
        {
          saw_quote = true;
          break;
        }
                                    
        *output_ptr++ = c;
      }

                    
      for (;;)
      {
        end_ptr = cur_ptr;
        if (cur_ptr >= line_end_ptr)
        {
          ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), errmsg("unterminated CSV quoted field")));
        }

        c = *cur_ptr++;

                                          
        if (c == escapec)
        {
             
                                                                     
                                               
             
          if (cur_ptr < line_end_ptr)
          {
            char nextc = *cur_ptr;

            if (nextc == escapec || nextc == quotec)
            {
              *output_ptr++ = nextc;
              cur_ptr++;
              continue;
            }
          }
        }

           
                                                                    
                                                                  
                                       
           
        if (c == quotec)
        {
          break;
        }

                                    
        *output_ptr++ = c;
      }
    }
  endfield:

                                                  
    *output_ptr++ = '\0';

                                                     
    input_len = end_ptr - start_ptr;
    if (!saw_quote && input_len == cstate->null_print_len && strncmp(start_ptr, cstate->null_print, input_len) == 0)
    {
      cstate->raw_fields[fieldno] = NULL;
    }

    fieldno++;
                                               
    if (!found_delim)
    {
      break;
    }
  }

                                       
  output_ptr--;
  Assert(*output_ptr == '\0');
  cstate->attribute_buf.len = (output_ptr - cstate->attribute_buf.data);

  return fieldno;
}

   
                           
   
static Datum
CopyReadBinaryAttribute(CopyState cstate, int column_no, FmgrInfo *flinfo, Oid typioparam, int32 typmod, bool *isnull)
{
  int32 fld_size;
  Datum result;

  if (!CopyGetInt32(cstate, &fld_size))
  {
    ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), errmsg("unexpected EOF in COPY data")));
  }
  if (fld_size == -1)
  {
    *isnull = true;
    return ReceiveFunctionCall(flinfo, NULL, typioparam, typmod);
  }
  if (fld_size < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), errmsg("invalid field size")));
  }

                                                             
  resetStringInfo(&cstate->attribute_buf);

  enlargeStringInfo(&cstate->attribute_buf, fld_size);
  if (CopyGetData(cstate, cstate->attribute_buf.data, fld_size, fld_size) != fld_size)
  {
    ereport(ERROR, (errcode(ERRCODE_BAD_COPY_FILE_FORMAT), errmsg("unexpected EOF in COPY data")));
  }

  cstate->attribute_buf.len = fld_size;
  cstate->attribute_buf.data[fld_size] = '\0';

                                                     
  result = ReceiveFunctionCall(flinfo, &cstate->attribute_buf, typioparam, typmod);

                                                 
  if (cstate->attribute_buf.cursor != cstate->attribute_buf.len)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("incorrect binary data format")));
  }

  *isnull = false;
  return result;
}

   
                                                                           
   
#define DUMPSOFAR()                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (ptr > start)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
      CopySendData(cstate, start, ptr - start);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  } while (0)

static void
CopyAttributeOutText(CopyState cstate, char *string)
{
  char *ptr;
  char *start;
  char c;
  char delimc = cstate->delim[0];

  if (cstate->need_transcoding)
  {
    ptr = pg_server_to_any(string, strlen(string), cstate->file_encoding);
  }
  else
  {
    ptr = string;
  }

     
                                                                           
                                                                             
                                                                           
                                                                           
                                                                             
                                                 
     
                                                                             
                                                                            
                                                                           
                                                                             
                                       
     
  if (cstate->encoding_embeds_ascii)
  {
    start = ptr;
    while ((c = *ptr) != '\0')
    {
      if ((unsigned char)c < (unsigned char)0x20)
      {
           
                                                                     
                                                                       
                                                                       
                                                                
                     
           
        switch (c)
        {
        case '\b':
          c = 'b';
          break;
        case '\f':
          c = 'f';
          break;
        case '\n':
          c = 'n';
          break;
        case '\r':
          c = 'r';
          break;
        case '\t':
          c = 't';
          break;
        case '\v':
          c = 'v';
          break;
        default:
                                                        
          if (c == delimc)
          {
            break;
          }
                                                    
          ptr++;
          continue;                          
        }
                                                                 
        DUMPSOFAR();
        CopySendChar(cstate, '\\');
        CopySendChar(cstate, c);
        start = ++ptr;                                      
      }
      else if (c == '\\' || c == delimc)
      {
        DUMPSOFAR();
        CopySendChar(cstate, '\\');
        start = ptr++;                                  
      }
      else if (IS_HIGHBIT_SET(c))
      {
        ptr += pg_encoding_mblen(cstate->file_encoding, ptr);
      }
      else
      {
        ptr++;
      }
    }
  }
  else
  {
    start = ptr;
    while ((c = *ptr) != '\0')
    {
      if ((unsigned char)c < (unsigned char)0x20)
      {
           
                                                                     
                                                                       
                                                                       
                                                                
                     
           
        switch (c)
        {
        case '\b':
          c = 'b';
          break;
        case '\f':
          c = 'f';
          break;
        case '\n':
          c = 'n';
          break;
        case '\r':
          c = 'r';
          break;
        case '\t':
          c = 't';
          break;
        case '\v':
          c = 'v';
          break;
        default:
                                                        
          if (c == delimc)
          {
            break;
          }
                                                    
          ptr++;
          continue;                          
        }
                                                                 
        DUMPSOFAR();
        CopySendChar(cstate, '\\');
        CopySendChar(cstate, c);
        start = ++ptr;                                      
      }
      else if (c == '\\' || c == delimc)
      {
        DUMPSOFAR();
        CopySendChar(cstate, '\\');
        start = ptr++;                                  
      }
      else
      {
        ptr++;
      }
    }
  }

  DUMPSOFAR();
}

   
                                                                  
                      
   
static void
CopyAttributeOutCSV(CopyState cstate, char *string, bool use_quote, bool single_attr)
{
  char *ptr;
  char *start;
  char c;
  char delimc = cstate->delim[0];
  char quotec = cstate->quote[0];
  char escapec = cstate->escape[0];

                                                                   
  if (!use_quote && strcmp(string, cstate->null_print) == 0)
  {
    use_quote = true;
  }

  if (cstate->need_transcoding)
  {
    ptr = pg_server_to_any(string, strlen(string), cstate->file_encoding);
  }
  else
  {
    ptr = string;
  }

     
                                                             
     
  if (!use_quote)
  {
       
                                                                           
                                                                
       
    if (single_attr && strcmp(ptr, "\\.") == 0)
    {
      use_quote = true;
    }
    else
    {
      char *tptr = ptr;

      while ((c = *tptr) != '\0')
      {
        if (c == delimc || c == quotec || c == '\n' || c == '\r')
        {
          use_quote = true;
          break;
        }
        if (IS_HIGHBIT_SET(c) && cstate->encoding_embeds_ascii)
        {
          tptr += pg_encoding_mblen(cstate->file_encoding, tptr);
        }
        else
        {
          tptr++;
        }
      }
    }
  }

  if (use_quote)
  {
    CopySendChar(cstate, quotec);

       
                                                                          
       
    start = ptr;
    while ((c = *ptr) != '\0')
    {
      if (c == quotec || c == escapec)
      {
        DUMPSOFAR();
        CopySendChar(cstate, escapec);
        start = ptr;                                  
      }
      if (IS_HIGHBIT_SET(c) && cstate->encoding_embeds_ascii)
      {
        ptr += pg_encoding_mblen(cstate->file_encoding, ptr);
      }
      else
      {
        ptr++;
      }
    }
    DUMPSOFAR();

    CopySendChar(cstate, quotec);
  }
  else
  {
                                                               
    CopySendString(cstate, ptr);
  }
}

   
                                                                  
   
                                                                   
                                                                       
             
   
                                                                              
                                                                          
                                                                             
                                                                  
   
                                                         
   
static List *
CopyGetAttnums(TupleDesc tupDesc, Relation rel, List *attnamelist)
{
  List *attnums = NIL;

  if (attnamelist == NIL)
  {
                                      
    int attr_count = tupDesc->natts;
    int i;

    for (i = 0; i < attr_count; i++)
    {
      if (TupleDescAttr(tupDesc, i)->attisdropped)
      {
        continue;
      }
      if (TupleDescAttr(tupDesc, i)->attgenerated)
      {
        continue;
      }
      attnums = lappend_int(attnums, i + 1);
    }
  }
  else
  {
                                                             
    ListCell *l;

    foreach (l, attnamelist)
    {
      char *name = strVal(lfirst(l));
      int attnum;
      int i;

                              
      attnum = InvalidAttrNumber;
      for (i = 0; i < tupDesc->natts; i++)
      {
        Form_pg_attribute att = TupleDescAttr(tupDesc, i);

        if (att->attisdropped)
        {
          continue;
        }
        if (namestrcmp(&(att->attname), name) == 0)
        {
          if (att->attgenerated)
          {
            ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("column \"%s\" is a generated column", name), errdetail("Generated columns cannot be used in COPY.")));
          }
          attnum = att->attnum;
          break;
        }
      }
      if (attnum == InvalidAttrNumber)
      {
        if (rel != NULL)
        {
          ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", name, RelationGetRelationName(rel))));
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" does not exist", name)));
        }
      }
                                
      if (list_member_int(attnums, attnum))
      {
        ereport(ERROR, (errcode(ERRCODE_DUPLICATE_COLUMN), errmsg("column \"%s\" specified more than once", name)));
      }
      attnums = lappend_int(attnums, attnum);
    }
  }

  return attnums;
}

   
                                          
   
static void
copy_dest_startup(DestReceiver *self, int operation, TupleDesc typeinfo)
{
             
}

   
                                           
   
static bool
copy_dest_receive(TupleTableSlot *slot, DestReceiver *self)
{
  DR_copy *myState = (DR_copy *)self;
  CopyState cstate = myState->cstate;

                     
  CopyOneRowTo(cstate, slot);
  myState->processed++;

  return true;
}

   
                                       
   
static void
copy_dest_shutdown(DestReceiver *self)
{
             
}

   
                                                     
   
static void
copy_dest_destroy(DestReceiver *self)
{
  pfree(self);
}

   
                                                                   
   
DestReceiver *
CreateCopyDestReceiver(void)
{
  DR_copy *self = (DR_copy *)palloc(sizeof(DR_copy));

  self->pub.receiveSlot = copy_dest_receive;
  self->pub.rStartup = copy_dest_startup;
  self->pub.rShutdown = copy_dest_shutdown;
  self->pub.rDestroy = copy_dest_destroy;
  self->pub.mydest = DestCopyOut;

  self->cstate = NULL;                        
  self->processed = 0;

  return (DestReceiver *)self;
}
