/*-------------------------------------------------------------------------
 *
 * dict_ispell.c
 *		Ispell dictionary interface
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/tsearch/dict_ispell.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "commands/defrem.h"
#include "tsearch/dicts/spell.h"
#include "tsearch/ts_locale.h"
#include "tsearch/ts_utils.h"
#include "utils/builtins.h"

typedef struct
{
  StopList stoplist;
  IspellDict obj;
} DictISpell;

Datum
dispell_init(PG_FUNCTION_ARGS)
{
  List *dictoptions = (List *)PG_GETARG_POINTER(0);
  DictISpell *d;
  bool affloaded = false, dictloaded = false, stoploaded = false;
  ListCell *l;

  d = (DictISpell *)palloc0(sizeof(DictISpell));

  NIStartBuild(&(d->obj));

  foreach (l, dictoptions)
  {
    DefElem *defel = (DefElem *)lfirst(l);

    if (strcmp(defel->defname, "dictfile") == 0)
    {
      if (dictloaded)
      {

      }
      NIImportDictionary(&(d->obj), get_tsearch_config_filename(defGetString(defel), "dict"));
      dictloaded = true;
    }
    else if (strcmp(defel->defname, "afffile") == 0)
    {
      if (affloaded)
      {

      }
      NIImportAffixes(&(d->obj), get_tsearch_config_filename(defGetString(defel), "affix"));
      affloaded = true;
    }
    else if (strcmp(defel->defname, "stopwords") == 0)
    {






    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("unrecognized Ispell parameter: \"%s\"", defel->defname)));
    }
  }

  if (affloaded && dictloaded)
  {
    NISortDictionary(&(d->obj));
    NISortAffixes(&(d->obj));
  }
  else if (!affloaded)
  {

  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("missing DictFile parameter")));
  }

  NIFinishBuild(&(d->obj));

  PG_RETURN_POINTER(d);
}

Datum
dispell_lexize(PG_FUNCTION_ARGS)
{
  DictISpell *d = (DictISpell *)PG_GETARG_POINTER(0);
  char *in = (char *)PG_GETARG_POINTER(1);
  int32 len = PG_GETARG_INT32(2);
  char *txt;
  TSLexeme *res;
  TSLexeme *ptr, *cptr;

  if (len <= 0)
  {

  }

  txt = lowerstr_with_len(in, len);
  res = NINormalizeWord(&(d->obj), txt);

  if (res == NULL)
  {
    PG_RETURN_POINTER(NULL);
  }

  cptr = res;
  for (ptr = cptr; ptr->lexeme; ptr++)
  {
    if (searchstoplist(&(d->stoplist), ptr->lexeme))
    {


    }
    else
    {
      if (cptr != ptr)
      {

      }
      cptr++;
    }
  }
  cptr->lexeme = NULL;

  PG_RETURN_POINTER(res);
}