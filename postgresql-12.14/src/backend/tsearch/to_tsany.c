                                                                            
   
              
                                
   
                                                                         
   
   
                  
                                    
   
                                                                            
   
#include "postgres.h"

#include "tsearch/ts_cache.h"
#include "tsearch/ts_utils.h"
#include "utils/builtins.h"
#include "utils/jsonapi.h"

typedef struct MorphOpaque
{
  Oid cfg_id;
  int qoperator;                     
} MorphOpaque;

typedef struct TSVectorBuildState
{
  ParsedText *prs;
  Oid cfgId;
} TSVectorBuildState;

static void
add_to_tsvector(void *_state, char *elem_value, int elem_len);

Datum
get_current_ts_config(PG_FUNCTION_ARGS)
{
  PG_RETURN_OID(getTSCurrentConfig(true));
}

   
               
   
static int
compareWORD(const void *a, const void *b)
{
  int res;

  res = tsCompareString(((const ParsedWord *)a)->word, ((const ParsedWord *)a)->len, ((const ParsedWord *)b)->word, ((const ParsedWord *)b)->len, false);

  if (res == 0)
  {
    if (((const ParsedWord *)a)->pos.pos == ((const ParsedWord *)b)->pos.pos)
    {
      return 0;
    }

    res = (((const ParsedWord *)a)->pos.pos > ((const ParsedWord *)b)->pos.pos) ? 1 : -1;
  }

  return res;
}

static int
uniqueWORD(ParsedWord *a, int32 l)
{
  ParsedWord *ptr, *res;
  int tmppos;

  if (l == 1)
  {
    tmppos = LIMITPOS(a->pos.pos);
    a->alen = 2;
    a->pos.apos = (uint16 *)palloc(sizeof(uint16) * a->alen);
    a->pos.apos[0] = 1;
    a->pos.apos[1] = tmppos;
    return l;
  }

  res = a;
  ptr = a + 1;

     
                                   
     
  qsort((void *)a, l, sizeof(ParsedWord), compareWORD);

     
                                                  
     
  tmppos = LIMITPOS(a->pos.pos);
  a->alen = 2;
  a->pos.apos = (uint16 *)palloc(sizeof(uint16) * a->alen);
  a->pos.apos[0] = 1;
  a->pos.apos[1] = tmppos;

     
                                                  
     
  while (ptr - a < l)
  {
    if (!(ptr->len == res->len && strncmp(ptr->word, res->word, res->len) == 0))
    {
         
                                             
         
      res++;
      res->len = ptr->len;
      res->word = ptr->word;
      tmppos = LIMITPOS(ptr->pos.pos);
      res->alen = 2;
      res->pos.apos = (uint16 *)palloc(sizeof(uint16) * res->alen);
      res->pos.apos[0] = 1;
      res->pos.apos[1] = tmppos;
    }
    else
    {
         
                                                                      
                                                                      
                                                       
         
      pfree(ptr->word);
      if (res->pos.apos[0] < MAXNUMPOS - 1 && res->pos.apos[res->pos.apos[0]] != MAXENTRYPOS - 1 && res->pos.apos[res->pos.apos[0]] != LIMITPOS(ptr->pos.pos))
      {
        if (res->pos.apos[0] + 1 >= res->alen)
        {
          res->alen *= 2;
          res->pos.apos = (uint16 *)repalloc(res->pos.apos, sizeof(uint16) * res->alen);
        }
        if (res->pos.apos[0] == 0 || res->pos.apos[res->pos.apos[0]] != LIMITPOS(ptr->pos.pos))
        {
          res->pos.apos[res->pos.apos[0] + 1] = LIMITPOS(ptr->pos.pos);
          res->pos.apos[0]++;
        }
      }
    }
    ptr++;
  }

  return res + 1 - a;
}

   
                                             
   
                                               
   
TSVector
make_tsvector(ParsedText *prs)
{
  int i, j, lenstr = 0, totallen;
  TSVector in;
  WordEntry *ptr;
  char *str;
  int stroff;

                             
  if (prs->curwords > 0)
  {
    prs->curwords = uniqueWORD(prs->words, prs->curwords);
  }

                              
  for (i = 0; i < prs->curwords; i++)
  {
    lenstr += prs->words[i].len;
    if (prs->words[i].alen)
    {
      lenstr = SHORTALIGN(lenstr);
      lenstr += sizeof(uint16) + prs->words[i].pos.apos[0] * sizeof(WordEntryPos);
    }
  }

  if (lenstr > MAXSTRPOS)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("string is too long for tsvector (%d bytes, max %d bytes)", lenstr, MAXSTRPOS)));
  }

  totallen = CALCDATASIZE(prs->curwords, lenstr);
  in = (TSVector)palloc0(totallen);
  SET_VARSIZE(in, totallen);
  in->size = prs->curwords;

  ptr = ARRPTR(in);
  str = STRPTR(in);
  stroff = 0;
  for (i = 0; i < prs->curwords; i++)
  {
    ptr->len = prs->words[i].len;
    ptr->pos = stroff;
    memcpy(str + stroff, prs->words[i].word, prs->words[i].len);
    stroff += prs->words[i].len;
    pfree(prs->words[i].word);
    if (prs->words[i].alen)
    {
      int k = prs->words[i].pos.apos[0];
      WordEntryPos *wptr;

      if (k > 0xFFFF)
      {
        elog(ERROR, "positions array too long");
      }

      ptr->haspos = 1;
      stroff = SHORTALIGN(stroff);
      *(uint16 *)(str + stroff) = (uint16)k;
      wptr = POSDATAPTR(in, ptr);
      for (j = 0; j < k; j++)
      {
        WEP_SETWEIGHT(wptr[j], 0);
        WEP_SETPOS(wptr[j], prs->words[i].pos.apos[j + 1]);
      }
      stroff += sizeof(uint16) + k * sizeof(WordEntryPos);
      pfree(prs->words[i].pos.apos);
    }
    else
    {
      ptr->haspos = 0;
    }
    ptr++;
  }

  if (prs->words)
  {
    pfree(prs->words);
  }

  return in;
}

Datum
to_tsvector_byid(PG_FUNCTION_ARGS)
{
  Oid cfgId = PG_GETARG_OID(0);
  text *in = PG_GETARG_TEXT_PP(1);
  ParsedText prs;
  TSVector out;

  prs.lenwords = VARSIZE_ANY_EXHDR(in) / 6;                              
                                                        
  if (prs.lenwords < 2)
  {
    prs.lenwords = 2;
  }
  prs.curwords = 0;
  prs.pos = 0;
  prs.words = (ParsedWord *)palloc(sizeof(ParsedWord) * prs.lenwords);

  parsetext(cfgId, &prs, VARDATA_ANY(in), VARSIZE_ANY_EXHDR(in));

  PG_FREE_IF_COPY(in, 1);

  out = make_tsvector(&prs);

  PG_RETURN_TSVECTOR(out);
}

Datum
to_tsvector(PG_FUNCTION_ARGS)
{
  text *in = PG_GETARG_TEXT_PP(0);
  Oid cfgId;

  cfgId = getTSCurrentConfig(true);
  PG_RETURN_DATUM(DirectFunctionCall2(to_tsvector_byid, ObjectIdGetDatum(cfgId), PointerGetDatum(in)));
}

   
                                                         
   
static TSVector
jsonb_to_tsvector_worker(Oid cfgId, Jsonb *jb, uint32 flags)
{
  TSVectorBuildState state;
  ParsedText prs;

  prs.words = NULL;
  prs.curwords = 0;
  state.prs = &prs;
  state.cfgId = cfgId;

  iterate_jsonb_values(jb, flags, &state, add_to_tsvector);

  return make_tsvector(&prs);
}

Datum
jsonb_string_to_tsvector_byid(PG_FUNCTION_ARGS)
{
  Oid cfgId = PG_GETARG_OID(0);
  Jsonb *jb = PG_GETARG_JSONB_P(1);
  TSVector result;

  result = jsonb_to_tsvector_worker(cfgId, jb, jtiString);
  PG_FREE_IF_COPY(jb, 1);

  PG_RETURN_TSVECTOR(result);
}

Datum
jsonb_string_to_tsvector(PG_FUNCTION_ARGS)
{
  Jsonb *jb = PG_GETARG_JSONB_P(0);
  Oid cfgId;
  TSVector result;

  cfgId = getTSCurrentConfig(true);
  result = jsonb_to_tsvector_worker(cfgId, jb, jtiString);
  PG_FREE_IF_COPY(jb, 0);

  PG_RETURN_TSVECTOR(result);
}

Datum
jsonb_to_tsvector_byid(PG_FUNCTION_ARGS)
{
  Oid cfgId = PG_GETARG_OID(0);
  Jsonb *jb = PG_GETARG_JSONB_P(1);
  Jsonb *jbFlags = PG_GETARG_JSONB_P(2);
  TSVector result;
  uint32 flags = parse_jsonb_index_flags(jbFlags);

  result = jsonb_to_tsvector_worker(cfgId, jb, flags);
  PG_FREE_IF_COPY(jb, 1);
  PG_FREE_IF_COPY(jbFlags, 2);

  PG_RETURN_TSVECTOR(result);
}

Datum
jsonb_to_tsvector(PG_FUNCTION_ARGS)
{
  Jsonb *jb = PG_GETARG_JSONB_P(0);
  Jsonb *jbFlags = PG_GETARG_JSONB_P(1);
  Oid cfgId;
  TSVector result;
  uint32 flags = parse_jsonb_index_flags(jbFlags);

  cfgId = getTSCurrentConfig(true);
  result = jsonb_to_tsvector_worker(cfgId, jb, flags);
  PG_FREE_IF_COPY(jb, 0);
  PG_FREE_IF_COPY(jbFlags, 1);

  PG_RETURN_TSVECTOR(result);
}

   
                                                        
   
static TSVector
json_to_tsvector_worker(Oid cfgId, text *json, uint32 flags)
{
  TSVectorBuildState state;
  ParsedText prs;

  prs.words = NULL;
  prs.curwords = 0;
  state.prs = &prs;
  state.cfgId = cfgId;

  iterate_json_values(json, flags, &state, add_to_tsvector);

  return make_tsvector(&prs);
}

Datum
json_string_to_tsvector_byid(PG_FUNCTION_ARGS)
{
  Oid cfgId = PG_GETARG_OID(0);
  text *json = PG_GETARG_TEXT_P(1);
  TSVector result;

  result = json_to_tsvector_worker(cfgId, json, jtiString);
  PG_FREE_IF_COPY(json, 1);

  PG_RETURN_TSVECTOR(result);
}

Datum
json_string_to_tsvector(PG_FUNCTION_ARGS)
{
  text *json = PG_GETARG_TEXT_P(0);
  Oid cfgId;
  TSVector result;

  cfgId = getTSCurrentConfig(true);
  result = json_to_tsvector_worker(cfgId, json, jtiString);
  PG_FREE_IF_COPY(json, 0);

  PG_RETURN_TSVECTOR(result);
}

Datum
json_to_tsvector_byid(PG_FUNCTION_ARGS)
{
  Oid cfgId = PG_GETARG_OID(0);
  text *json = PG_GETARG_TEXT_P(1);
  Jsonb *jbFlags = PG_GETARG_JSONB_P(2);
  TSVector result;
  uint32 flags = parse_jsonb_index_flags(jbFlags);

  result = json_to_tsvector_worker(cfgId, json, flags);
  PG_FREE_IF_COPY(json, 1);
  PG_FREE_IF_COPY(jbFlags, 2);

  PG_RETURN_TSVECTOR(result);
}

Datum
json_to_tsvector(PG_FUNCTION_ARGS)
{
  text *json = PG_GETARG_TEXT_P(0);
  Jsonb *jbFlags = PG_GETARG_JSONB_P(1);
  Oid cfgId;
  TSVector result;
  uint32 flags = parse_jsonb_index_flags(jbFlags);

  cfgId = getTSCurrentConfig(true);
  result = json_to_tsvector_worker(cfgId, json, flags);
  PG_FREE_IF_COPY(json, 0);
  PG_FREE_IF_COPY(jbFlags, 1);

  PG_RETURN_TSVECTOR(result);
}

   
                                                                              
   
static void
add_to_tsvector(void *_state, char *elem_value, int elem_len)
{
  TSVectorBuildState *state = (TSVectorBuildState *)_state;
  ParsedText *prs = state->prs;
  int32 prevwords;

  if (prs->words == NULL)
  {
       
                                                                        
                                                       
       
    prs->lenwords = 16;
    prs->words = (ParsedWord *)palloc(sizeof(ParsedWord) * prs->lenwords);
    prs->curwords = 0;
    prs->pos = 0;
  }

  prevwords = prs->curwords;

  parsetext(state->cfgId, prs, elem_value, elem_len);

     
                                                                             
                                                                          
                                                                             
                                        
     
  if (prs->curwords > prevwords)
  {
    prs->pos += 1;
  }
}

   
              
   

   
                                            
   
                                                                            
                                                                           
                 
   
                                                                        
                                             
   
static void
pushval_morph(Datum opaque, TSQueryParserState state, char *strval, int lenval, int16 weight, bool prefix)
{
  int32 count = 0;
  ParsedText prs;
  uint32 variant, pos = 0, cntvar = 0, cntpos = 0, cnt = 0;
  MorphOpaque *data = (MorphOpaque *)DatumGetPointer(opaque);

  prs.lenwords = 4;
  prs.curwords = 0;
  prs.pos = 0;
  prs.words = (ParsedWord *)palloc(sizeof(ParsedWord) * prs.lenwords);

  parsetext(data->cfg_id, &prs, strval, lenval);

  if (prs.curwords > 0)
  {
    while (count < prs.curwords)
    {
         
                                                                       
                                                         
         
      if (pos > 0 && pos + 1 < prs.words[count].pos.pos)
      {
        while (pos + 1 < prs.words[count].pos.pos)
        {
                                                           
          pushStop(state);
          if (cntpos)
          {
            pushOperator(state, data->qoperator, 1);
          }
          cntpos++;
          pos++;
        }
      }

                                        
      pos = prs.words[count].pos.pos;

                                                            
      cntvar = 0;
      while (count < prs.curwords && pos == prs.words[count].pos.pos)
      {
        variant = prs.words[count].nvariant;

                                                          
        cnt = 0;
        while (count < prs.curwords && pos == prs.words[count].pos.pos && variant == prs.words[count].nvariant)
        {
          pushValue(state, prs.words[count].word, prs.words[count].len, weight, ((prs.words[count].flags & TSL_PREFIX) || prefix));
          pfree(prs.words[count].word);
          if (cnt)
          {
            pushOperator(state, OP_AND, 0);
          }
          cnt++;
          count++;
        }

        if (cntvar)
        {
          pushOperator(state, OP_OR, 0);
        }
        cntvar++;
      }

      if (cntpos)
      {
                                    
        pushOperator(state, data->qoperator, 1);
      }

      cntpos++;
    }

    pfree(prs.words);
  }
  else
  {
    pushStop(state);
  }
}

Datum
to_tsquery_byid(PG_FUNCTION_ARGS)
{
  text *in = PG_GETARG_TEXT_PP(1);
  TSQuery query;
  MorphOpaque data;

  data.cfg_id = PG_GETARG_OID(0);
  data.qoperator = OP_AND;

  query = parse_tsquery(text_to_cstring(in), pushval_morph, PointerGetDatum(&data), 0);

  PG_RETURN_TSQUERY(query);
}

Datum
to_tsquery(PG_FUNCTION_ARGS)
{
  text *in = PG_GETARG_TEXT_PP(0);
  Oid cfgId;

  cfgId = getTSCurrentConfig(true);
  PG_RETURN_DATUM(DirectFunctionCall2(to_tsquery_byid, ObjectIdGetDatum(cfgId), PointerGetDatum(in)));
}

Datum
plainto_tsquery_byid(PG_FUNCTION_ARGS)
{
  text *in = PG_GETARG_TEXT_PP(1);
  TSQuery query;
  MorphOpaque data;

  data.cfg_id = PG_GETARG_OID(0);
  data.qoperator = OP_AND;

  query = parse_tsquery(text_to_cstring(in), pushval_morph, PointerGetDatum(&data), P_TSQ_PLAIN);

  PG_RETURN_POINTER(query);
}

Datum
plainto_tsquery(PG_FUNCTION_ARGS)
{
  text *in = PG_GETARG_TEXT_PP(0);
  Oid cfgId;

  cfgId = getTSCurrentConfig(true);
  PG_RETURN_DATUM(DirectFunctionCall2(plainto_tsquery_byid, ObjectIdGetDatum(cfgId), PointerGetDatum(in)));
}

Datum
phraseto_tsquery_byid(PG_FUNCTION_ARGS)
{
  text *in = PG_GETARG_TEXT_PP(1);
  TSQuery query;
  MorphOpaque data;

  data.cfg_id = PG_GETARG_OID(0);
  data.qoperator = OP_PHRASE;

  query = parse_tsquery(text_to_cstring(in), pushval_morph, PointerGetDatum(&data), P_TSQ_PLAIN);

  PG_RETURN_TSQUERY(query);
}

Datum
phraseto_tsquery(PG_FUNCTION_ARGS)
{
  text *in = PG_GETARG_TEXT_PP(0);
  Oid cfgId;

  cfgId = getTSCurrentConfig(true);
  PG_RETURN_DATUM(DirectFunctionCall2(phraseto_tsquery_byid, ObjectIdGetDatum(cfgId), PointerGetDatum(in)));
}

Datum
websearch_to_tsquery_byid(PG_FUNCTION_ARGS)
{
  text *in = PG_GETARG_TEXT_PP(1);
  MorphOpaque data;
  TSQuery query = NULL;

  data.cfg_id = PG_GETARG_OID(0);

  data.qoperator = OP_AND;

  query = parse_tsquery(text_to_cstring(in), pushval_morph, PointerGetDatum(&data), P_TSQ_WEB);

  PG_RETURN_TSQUERY(query);
}

Datum
websearch_to_tsquery(PG_FUNCTION_ARGS)
{
  text *in = PG_GETARG_TEXT_PP(0);
  Oid cfgId;

  cfgId = getTSCurrentConfig(true);
  PG_RETURN_DATUM(DirectFunctionCall2(websearch_to_tsquery_byid, ObjectIdGetDatum(cfgId), PointerGetDatum(in)));
}
