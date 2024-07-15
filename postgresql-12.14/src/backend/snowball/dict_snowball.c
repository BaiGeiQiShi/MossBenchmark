                                                                            
   
                   
                        
   
                                                                         
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include "commands/defrem.h"
#include "tsearch/ts_locale.h"
#include "tsearch/ts_utils.h"

                                                                   
#ifdef MAXINT
#undef MAXINT
#endif
#ifdef MININT
#undef MININT
#endif

                                                       
#include "snowball/libstemmer/header.h"
#include "snowball/libstemmer/stem_ISO_8859_1_danish.h"
#include "snowball/libstemmer/stem_ISO_8859_1_dutch.h"
#include "snowball/libstemmer/stem_ISO_8859_1_english.h"
#include "snowball/libstemmer/stem_ISO_8859_1_finnish.h"
#include "snowball/libstemmer/stem_ISO_8859_1_french.h"
#include "snowball/libstemmer/stem_ISO_8859_1_german.h"
#include "snowball/libstemmer/stem_ISO_8859_1_indonesian.h"
#include "snowball/libstemmer/stem_ISO_8859_1_irish.h"
#include "snowball/libstemmer/stem_ISO_8859_1_italian.h"
#include "snowball/libstemmer/stem_ISO_8859_1_norwegian.h"
#include "snowball/libstemmer/stem_ISO_8859_1_porter.h"
#include "snowball/libstemmer/stem_ISO_8859_1_portuguese.h"
#include "snowball/libstemmer/stem_ISO_8859_1_spanish.h"
#include "snowball/libstemmer/stem_ISO_8859_1_swedish.h"
#include "snowball/libstemmer/stem_ISO_8859_2_hungarian.h"
#include "snowball/libstemmer/stem_ISO_8859_2_romanian.h"
#include "snowball/libstemmer/stem_KOI8_R_russian.h"
#include "snowball/libstemmer/stem_UTF_8_arabic.h"
#include "snowball/libstemmer/stem_UTF_8_danish.h"
#include "snowball/libstemmer/stem_UTF_8_dutch.h"
#include "snowball/libstemmer/stem_UTF_8_english.h"
#include "snowball/libstemmer/stem_UTF_8_finnish.h"
#include "snowball/libstemmer/stem_UTF_8_french.h"
#include "snowball/libstemmer/stem_UTF_8_german.h"
#include "snowball/libstemmer/stem_UTF_8_hungarian.h"
#include "snowball/libstemmer/stem_UTF_8_indonesian.h"
#include "snowball/libstemmer/stem_UTF_8_irish.h"
#include "snowball/libstemmer/stem_UTF_8_italian.h"
#include "snowball/libstemmer/stem_UTF_8_lithuanian.h"
#include "snowball/libstemmer/stem_UTF_8_nepali.h"
#include "snowball/libstemmer/stem_UTF_8_norwegian.h"
#include "snowball/libstemmer/stem_UTF_8_porter.h"
#include "snowball/libstemmer/stem_UTF_8_portuguese.h"
#include "snowball/libstemmer/stem_UTF_8_romanian.h"
#include "snowball/libstemmer/stem_UTF_8_russian.h"
#include "snowball/libstemmer/stem_UTF_8_spanish.h"
#include "snowball/libstemmer/stem_UTF_8_swedish.h"
#include "snowball/libstemmer/stem_UTF_8_tamil.h"
#include "snowball/libstemmer/stem_UTF_8_turkish.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(dsnowball_init);

PG_FUNCTION_INFO_V1(dsnowball_lexize);

                               
typedef struct stemmer_module
{
  const char *name;
  pg_enc enc;
  struct SN_env *(*create)(void);
  void (*close)(struct SN_env *);
  int (*stem)(struct SN_env *);
} stemmer_module;

                                                                            
#define STEMMER_MODULE(name, enc, senc)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    #name, enc, name##_##senc##_create_env, name##_##senc##_close_env, name##_##senc##_stem                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  }

static const stemmer_module stemmer_modules[] = {
       
                                                
       
    STEMMER_MODULE(danish, PG_LATIN1, ISO_8859_1), STEMMER_MODULE(dutch, PG_LATIN1, ISO_8859_1), STEMMER_MODULE(english, PG_LATIN1, ISO_8859_1), STEMMER_MODULE(finnish, PG_LATIN1, ISO_8859_1), STEMMER_MODULE(french, PG_LATIN1, ISO_8859_1), STEMMER_MODULE(german, PG_LATIN1, ISO_8859_1), STEMMER_MODULE(indonesian, PG_LATIN1, ISO_8859_1), STEMMER_MODULE(irish, PG_LATIN1, ISO_8859_1), STEMMER_MODULE(italian, PG_LATIN1, ISO_8859_1), STEMMER_MODULE(norwegian, PG_LATIN1, ISO_8859_1), STEMMER_MODULE(porter, PG_LATIN1, ISO_8859_1), STEMMER_MODULE(portuguese, PG_LATIN1, ISO_8859_1), STEMMER_MODULE(spanish, PG_LATIN1, ISO_8859_1), STEMMER_MODULE(swedish, PG_LATIN1, ISO_8859_1), STEMMER_MODULE(hungarian, PG_LATIN2, ISO_8859_2), STEMMER_MODULE(romanian, PG_LATIN2, ISO_8859_2), STEMMER_MODULE(russian, PG_KOI8R, KOI8_R), STEMMER_MODULE(arabic, PG_UTF8, UTF_8), STEMMER_MODULE(danish, PG_UTF8, UTF_8), STEMMER_MODULE(dutch, PG_UTF8, UTF_8), STEMMER_MODULE(english, PG_UTF8, UTF_8),
    STEMMER_MODULE(finnish, PG_UTF8, UTF_8), STEMMER_MODULE(french, PG_UTF8, UTF_8), STEMMER_MODULE(german, PG_UTF8, UTF_8), STEMMER_MODULE(hungarian, PG_UTF8, UTF_8), STEMMER_MODULE(indonesian, PG_UTF8, UTF_8), STEMMER_MODULE(irish, PG_UTF8, UTF_8), STEMMER_MODULE(italian, PG_UTF8, UTF_8), STEMMER_MODULE(lithuanian, PG_UTF8, UTF_8), STEMMER_MODULE(nepali, PG_UTF8, UTF_8), STEMMER_MODULE(norwegian, PG_UTF8, UTF_8), STEMMER_MODULE(porter, PG_UTF8, UTF_8), STEMMER_MODULE(portuguese, PG_UTF8, UTF_8), STEMMER_MODULE(romanian, PG_UTF8, UTF_8), STEMMER_MODULE(russian, PG_UTF8, UTF_8), STEMMER_MODULE(spanish, PG_UTF8, UTF_8), STEMMER_MODULE(swedish, PG_UTF8, UTF_8), STEMMER_MODULE(tamil, PG_UTF8, UTF_8), STEMMER_MODULE(turkish, PG_UTF8, UTF_8),

       
                                                                         
                
       
    STEMMER_MODULE(english, PG_SQL_ASCII, ISO_8859_1),

    {NULL, 0, NULL, NULL, NULL}                      
};

typedef struct DictSnowball
{
  struct SN_env *z;
  StopList stoplist;
  bool needrecode;                                            
  int (*stem)(struct SN_env *z);

     
                                                                             
                                                                           
                                                       
     
  MemoryContext dictCtx;
} DictSnowball;

static void
locate_stem_module(DictSnowball *d, const char *lang)
{
  const stemmer_module *m;

     
                                                                    
                                                                          
     
  for (m = stemmer_modules; m->name; m++)
  {
    if ((m->enc == PG_SQL_ASCII || m->enc == GetDatabaseEncoding()) && pg_strcasecmp(m->name, lang) == 0)
    {
      d->stem = m->stem;
      d->z = m->create();
      d->needrecode = false;
      return;
    }
  }

     
                                                                        
     
  for (m = stemmer_modules; m->name; m++)
  {
    if (m->enc == PG_UTF8 && pg_strcasecmp(m->name, lang) == 0)
    {
      d->stem = m->stem;
      d->z = m->create();
      d->needrecode = true;
      return;
    }
  }

  ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("no Snowball stemmer available for language \"%s\" and encoding \"%s\"", lang, GetDatabaseEncodingName())));
}

Datum
dsnowball_init(PG_FUNCTION_ARGS)
{
  List *dictoptions = (List *)PG_GETARG_POINTER(0);
  DictSnowball *d;
  bool stoploaded = false;
  ListCell *l;

  d = (DictSnowball *)palloc0(sizeof(DictSnowball));

  foreach (l, dictoptions)
  {
    DefElem *defel = (DefElem *)lfirst(l);

    if (strcmp(defel->defname, "stopwords") == 0)
    {
      if (stoploaded)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("multiple StopWords parameters")));
      }
      readstoplist(defGetString(defel), &d->stoplist, lowerstr);
      stoploaded = true;
    }
    else if (strcmp(defel->defname, "language") == 0)
    {
      if (d->stem)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("multiple Language parameters")));
      }
      locate_stem_module(d, defGetString(defel));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("unrecognized Snowball parameter: \"%s\"", defel->defname)));
    }
  }

  if (!d->stem)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("missing Language parameter")));
  }

  d->dictCtx = CurrentMemoryContext;

  PG_RETURN_POINTER(d);
}

Datum
dsnowball_lexize(PG_FUNCTION_ARGS)
{
  DictSnowball *d = (DictSnowball *)PG_GETARG_POINTER(0);
  char *in = (char *)PG_GETARG_POINTER(1);
  int32 len = PG_GETARG_INT32(2);
  char *txt = lowerstr_with_len(in, len);
  TSLexeme *res = palloc0(sizeof(TSLexeme) * 2);

     
                                                                         
                                                                      
                                                                          
                                                                        
                                                                         
                                                                        
                                                                           
                   
     
  if (len > 1000)
  {
                                                                
    res->lexeme = txt;
  }
  else if (*txt == '\0' || searchstoplist(&(d->stoplist), txt))
  {
                                                  
    pfree(txt);
  }
  else
  {
    MemoryContext saveCtx;

       
                                                                           
       
    if (d->needrecode)
    {
      char *recoded;

      recoded = pg_server_to_any(txt, strlen(txt), PG_UTF8);
      if (recoded != txt)
      {
        pfree(txt);
        txt = recoded;
      }
    }

                                      
    saveCtx = MemoryContextSwitchTo(d->dictCtx);
    SN_set_current(d->z, strlen(txt), (symbol *)txt);
    d->stem(d->z);
    MemoryContextSwitchTo(saveCtx);

    if (d->z->p && d->z->l)
    {
      txt = repalloc(txt, d->z->l + 1);
      memcpy(txt, d->z->p, d->z->l);
      txt[d->z->l] = '\0';
    }

                               
    if (d->needrecode)
    {
      char *recoded;

      recoded = pg_any_to_server(txt, strlen(txt), PG_UTF8);
      if (recoded != txt)
      {
        pfree(txt);
        txt = recoded;
      }
    }

    res->lexeme = txt;
  }

  PG_RETURN_POINTER(res);
}
