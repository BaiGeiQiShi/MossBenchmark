                                                                            
   
                   
                                                              
   
                                                                         
   
   
                  
                                         
   
                                                                            
   
#include "postgres.h"

#include "catalog/pg_collation.h"
#include "catalog/pg_operator.h"
#include "commands/vacuum.h"
#include "tsearch/ts_type.h"
#include "utils/builtins.h"
#include "utils/hashutils.h"

                            
typedef struct
{
  char *lexeme;                                    
  int length;                            
} LexemeHashKey;

                                                         
typedef struct
{
  LexemeHashKey key;                                         
  int frequency;                       
  int delta;                                   
} TrackItem;

static void
compute_tsvector_stats(VacAttrStats *stats, AnalyzeAttrFetchFunc fetchfunc, int samplerows, double totalrows);
static void
prune_lexemes_hashtable(HTAB *lexemes_tab, int b_current);
static uint32
lexeme_hash(const void *key, Size keysize);
static int
lexeme_match(const void *key1, const void *key2, Size keysize);
static int
lexeme_compare(const void *key1, const void *key2);
static int
trackitem_compare_frequencies_desc(const void *e1, const void *e2);
static int
trackitem_compare_lexemes(const void *e1, const void *e2);

   
                                                                      
   
Datum
ts_typanalyze(PG_FUNCTION_ARGS)
{
  VacAttrStats *stats = (VacAttrStats *)PG_GETARG_POINTER(0);
  Form_pg_attribute attr = stats->attr;

                                                                      
                                                                   
  if (attr->attstattarget < 0)
  {
    attr->attstattarget = default_statistics_target;
  }

  stats->compute_stats = compute_tsvector_stats;
                                                                     
  stats->minrows = 300 * attr->attstattarget;

  PG_RETURN_BOOL(true);
}

   
                                                                        
   
                                                                         
                                                                         
                  
   
                                                                           
                                                                           
                                                                             
                                                                             
                                                                              
   
                                                                        
                                              
   
                                                                               
                                                                         
                                                                              
                                                                         
                                            
   
                                                         
                                                                          
                                                                            
                                                                             
                                                                             
                                                                             
                                                                            
                                                                            
                                                                       
                                                                            
                                                                         
                                                                          
                                                                            
                                                                      
                                                                         
                                                                          
                                                                            
                                                                       
                                                                          
   
                                                                        
                                                                          
                                                                          
                                                                         
                                                                      
                                                                             
                              
   
                                                                          
                                                                              
                                                                            
                                                                        
                                                                   
                                                             
   
                                                                        
                                                                      
                                                                        
                                                                             
                                                                        
                                                                        
                                                                          
                                                                           
         
   
static void
compute_tsvector_stats(VacAttrStats *stats, AnalyzeAttrFetchFunc fetchfunc, int samplerows, double totalrows)
{
  int num_mcelem;
  int null_cnt = 0;
  double total_width = 0;

                                        
  HTAB *lexemes_tab;
  HASHCTL hash_ctl;
  HASH_SEQ_STATUS scan_status;

                                                               
  int b_current;

                                         
  int bucket_width;
  int vector_no, lexeme_no;
  LexemeHashKey hash_key;
  TrackItem *item;

     
                                                                       
                                                                           
                                                                             
                                                                   
     
  num_mcelem = stats->attr->attstattarget * 10;

     
                                                                       
                    
     
  bucket_width = (num_mcelem + 10) * 1000 / 7;

     
                                                                           
                                                                             
                                                 
     
  MemSet(&hash_ctl, 0, sizeof(hash_ctl));
  hash_ctl.keysize = sizeof(LexemeHashKey);
  hash_ctl.entrysize = sizeof(TrackItem);
  hash_ctl.hash = lexeme_hash;
  hash_ctl.match = lexeme_match;
  hash_ctl.hcxt = CurrentMemoryContext;
  lexemes_tab = hash_create("Analyzed lexemes table", num_mcelem, &hash_ctl, HASH_ELEM | HASH_FUNCTION | HASH_COMPARE | HASH_CONTEXT);

                            
  b_current = 1;
  lexeme_no = 0;

                                
  for (vector_no = 0; vector_no < samplerows; vector_no++)
  {
    Datum value;
    bool isnull;
    TSVector vector;
    WordEntry *curentryptr;
    char *lexemesptr;
    int j;

    vacuum_delay_point();

    value = fetchfunc(stats, vector_no, &isnull);

       
                               
       
    if (isnull)
    {
      null_cnt++;
      continue;
    }

       
                                                                  
                                                          
                                                                         
                    
       
    total_width += VARSIZE_ANY(DatumGetPointer(value));

       
                                           
       
    vector = DatumGetTSVector(value);

       
                                                                       
                           
       
    lexemesptr = STRPTR(vector);
    curentryptr = ARRPTR(vector);
    for (j = 0; j < vector->size; j++)
    {
      bool found;

         
                                                                    
                                                                         
                                                                     
                                               
         
      hash_key.lexeme = lexemesptr + curentryptr->pos;
      hash_key.length = curentryptr->len;

                                                                
      item = (TrackItem *)hash_search(lexemes_tab, (const void *)&hash_key, HASH_ENTER, &found);

      if (found)
      {
                                                        
        item->frequency++;
      }
      else
      {
                                                  
        item->frequency = 1;
        item->delta = b_current - 1;

        item->key.lexeme = palloc(hash_key.length);
        memcpy(item->key.lexeme, hash_key.lexeme, hash_key.length);
      }

                                                                
      lexeme_no++;

                                                                 
      if (lexeme_no % bucket_width == 0)
      {
        prune_lexemes_hashtable(lexemes_tab, b_current);
        b_current++;
      }

                                                         
      curentryptr++;
    }

                                                             
    if (TSVectorGetDatum(vector) != value)
    {
      pfree(vector);
    }
  }

                                                                        
  if (null_cnt < samplerows)
  {
    int nonnull_cnt = samplerows - null_cnt;
    int i;
    TrackItem **sort_table;
    int track_len;
    int cutoff_freq;
    int minfreq, maxfreq;

    stats->stats_valid = true;
                                                         
    stats->stanullfrac = (double)null_cnt / (double)samplerows;
    stats->stawidth = total_width / (double)nonnull_cnt;

                                                       
    stats->stadistinct = -1.0 * (1.0 - stats->stanullfrac);

       
                                                                       
                                                                          
                                                              
       
                                                                     
                                        
       
    cutoff_freq = 9 * lexeme_no / bucket_width;

    i = hash_get_num_entries(lexemes_tab);                          
    sort_table = (TrackItem **)palloc(sizeof(TrackItem *) * i);

    hash_seq_init(&scan_status, lexemes_tab);
    track_len = 0;
    minfreq = lexeme_no;
    maxfreq = 0;
    while ((item = (TrackItem *)hash_seq_search(&scan_status)) != NULL)
    {
      if (item->frequency > cutoff_freq)
      {
        sort_table[track_len++] = item;
        minfreq = Min(minfreq, item->frequency);
        maxfreq = Max(maxfreq, item->frequency);
      }
    }
    Assert(track_len <= i);

                                                 
    elog(DEBUG3,
        "tsvector_stats: target # mces = %d, bucket width = %d, "
        "# lexemes = %d, hashtable size = %d, usable entries = %d",
        num_mcelem, bucket_width, lexeme_no, i, track_len);

       
                                                                         
                                                                           
                                                          
       
    if (num_mcelem < track_len)
    {
      qsort(sort_table, track_len, sizeof(TrackItem *), trackitem_compare_frequencies_desc);
                                                                 
      minfreq = sort_table[num_mcelem - 1]->frequency;
    }
    else
    {
      num_mcelem = track_len;
    }

                                    
    if (num_mcelem > 0)
    {
      MemoryContext old_context;
      Datum *mcelem_values;
      float4 *mcelem_freqs;

         
                                                                      
                                                                     
                                                                       
                                                                    
                                                            
         
                                                                     
                                                                  
                                                                   
                                                                        
                                                              
                                                  
         
      qsort(sort_table, num_mcelem, sizeof(TrackItem *), trackitem_compare_lexemes);

                                                        
      old_context = MemoryContextSwitchTo(stats->anl_context);

         
                                                                     
                                                                    
                                                                
                                                         
         
                                                                         
                                                                      
                                                                       
                    
         
      mcelem_values = (Datum *)palloc(num_mcelem * sizeof(Datum));
      mcelem_freqs = (float4 *)palloc((num_mcelem + 2) * sizeof(float4));

         
                                                                        
                                        
         
      for (i = 0; i < num_mcelem; i++)
      {
        TrackItem *item = sort_table[i];

        mcelem_values[i] = PointerGetDatum(cstring_to_text_with_len(item->key.lexeme, item->key.length));
        mcelem_freqs[i] = (double)item->frequency / (double)nonnull_cnt;
      }
      mcelem_freqs[i++] = (double)minfreq / (double)nonnull_cnt;
      mcelem_freqs[i] = (double)maxfreq / (double)nonnull_cnt;
      MemoryContextSwitchTo(old_context);

      stats->stakind[0] = STATISTIC_KIND_MCELEM;
      stats->staop[0] = TextEqualOperator;
      stats->stacoll[0] = DEFAULT_COLLATION_OID;
      stats->stanumbers[0] = mcelem_freqs;
                                                              
      stats->numnumbers[0] = num_mcelem + 2;
      stats->stavalues[0] = mcelem_values;
      stats->numvalues[0] = num_mcelem;
                                      
      stats->statypid[0] = TEXTOID;
      stats->statyplen[0] = -1;                             
      stats->statypbyval[0] = false;
      stats->statypalign[0] = 'i';
    }
  }
  else
  {
                                                                 
    stats->stats_valid = true;
    stats->stanullfrac = 1.0;
    stats->stawidth = 0;                     
    stats->stadistinct = 0.0;                
  }

     
                                                                            
                                                                       
     
}

   
                                                                          
                                                           
   
static void
prune_lexemes_hashtable(HTAB *lexemes_tab, int b_current)
{
  HASH_SEQ_STATUS scan_status;
  TrackItem *item;

  hash_seq_init(&scan_status, lexemes_tab);
  while ((item = (TrackItem *)hash_seq_search(&scan_status)) != NULL)
  {
    if (item->frequency + item->delta <= b_current)
    {
      char *lexeme = item->key.lexeme;

      if (hash_search(lexemes_tab, (const void *)&item->key, HASH_REMOVE, NULL) == NULL)
      {
        elog(ERROR, "hash table corrupted");
      }
      pfree(lexeme);
    }
  }
}

   
                                                                          
                                       
   
static uint32
lexeme_hash(const void *key, Size keysize)
{
  const LexemeHashKey *l = (const LexemeHashKey *)key;

  return DatumGetUInt32(hash_any((const unsigned char *)l->lexeme, l->length));
}

   
                                                                   
   
static int
lexeme_match(const void *key1, const void *key2, Size keysize)
{
                                                                          
  return lexeme_compare(key1, key2);
}

   
                                    
   
static int
lexeme_compare(const void *key1, const void *key2)
{
  const LexemeHashKey *d1 = (const LexemeHashKey *)key1;
  const LexemeHashKey *d2 = (const LexemeHashKey *)key2;

                                
  if (d1->length > d2->length)
  {
    return 1;
  }
  else if (d1->length < d2->length)
  {
    return -1;
  }
                                                       
  return strncmp(d1->lexeme, d2->lexeme, d1->length);
}

   
                                                                              
   
static int
trackitem_compare_frequencies_desc(const void *e1, const void *e2)
{
  const TrackItem *const *t1 = (const TrackItem *const *)e1;
  const TrackItem *const *t2 = (const TrackItem *const *)e2;

  return (*t2)->frequency - (*t1)->frequency;
}

   
                                                        
   
static int
trackitem_compare_lexemes(const void *e1, const void *e2)
{
  const TrackItem *const *t1 = (const TrackItem *const *)e1;
  const TrackItem *const *t2 = (const TrackItem *const *)e2;

  return lexeme_compare(&(*t1)->key, &(*t2)->key);
}
