/*-------------------------------------------------------------------------
 *
 *	  Utility functions for conversion procs.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/mb/conv.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "mb/pg_wchar.h"

/*
 * local2local: a generic single byte charset encoding
 * conversion between two ASCII-superset encodings.
 *
 * l points to the source string of length len
 * p is the output area (must be large enough!)
 * src_encoding is the PG identifier for the source encoding
 * dest_encoding is the PG identifier for the target encoding
 * tab holds conversion entries for the source charset
 * starting from 128 (0x80). each entry in the table holds the corresponding
 * code point for the target charset, or 0 if there is no equivalent code.
 */
void
local2local(const unsigned char *l, unsigned char *p, int len, int src_encoding, int dest_encoding, const unsigned char *tab)
{
  unsigned char c1, c2;

  while (len > 0)
  {
    c1 = *l;
    if (c1 == 0)
    {

    }
    if (!IS_HIGHBIT_SET(c1))
    {
      *p++ = c1;
    }
    else
    {









    }
    l++;
    len--;
  }
  *p = '\0';
}

/*
 * LATINn ---> MIC when the charset's local codes map directly to MIC
 *
 * l points to the source string of length len
 * p is the output area (must be large enough!)
 * lc is the mule character set id for the local encoding
 * encoding is the PG identifier for the local encoding
 */
void
latin2mic(const unsigned char *l, unsigned char *p, int len, int lc, int encoding)
{
  int c1;

  while (len > 0)
  {
    c1 = *l;
    if (c1 == 0)
    {

    }
    if (IS_HIGHBIT_SET(c1))
    {

    }
    *p++ = c1;
    l++;
    len--;
  }
  *p = '\0';
}

/*
 * MIC ---> LATINn when the charset's local codes map directly to MIC
 *
 * mic points to the source string of length len
 * p is the output area (must be large enough!)
 * lc is the mule character set id for the local encoding
 * encoding is the PG identifier for the local encoding
 */
void
mic2latin(const unsigned char *mic, unsigned char *p, int len, int lc, int encoding)
{
  int c1;

  while (len > 0)
  {
    c1 = *mic;
    if (c1 == 0)
    {

    }
    if (!IS_HIGHBIT_SET(c1))
    {
      /* easy for ASCII */
      *p++ = c1;
      mic++;
      len--;
    }
    else
    {
      int l = pg_mic_mblen(mic);












    }
  }
  *p = '\0';
}

/*
 * ASCII ---> MIC
 *
 * While ordinarily SQL_ASCII encoding is forgiving of high-bit-set
 * characters, here we must take a hard line because we don't know
 * the appropriate MIC equivalent.
 */
void
pg_ascii2mic(const unsigned char *l, unsigned char *p, int len)
{
  int c1;

  while (len > 0)
  {








  }
  *p = '\0';
}

/*
 * MIC ---> ASCII
 */
void
pg_mic2ascii(const unsigned char *mic, unsigned char *p, int len)
{














}

/*
 * latin2mic_with_table: a generic single byte charset encoding
 * conversion from a local charset to the mule internal code.
 *
 * l points to the source string of length len
 * p is the output area (must be large enough!)
 * lc is the mule character set id for the local encoding
 * encoding is the PG identifier for the local encoding
 * tab holds conversion entries for the local charset
 * starting from 128 (0x80). each entry in the table holds the corresponding
 * code point for the mule encoding, or 0 if there is no equivalent code.
 */
void
latin2mic_with_table(const unsigned char *l, unsigned char *p, int len, int lc, int encoding, const unsigned char *tab)
{
  unsigned char c1, c2;

  while (len > 0)
  {
    c1 = *l;
    if (c1 == 0)
    {

    }
    if (!IS_HIGHBIT_SET(c1))
    {
      *p++ = c1;
    }
    else
    {










    }
    l++;
    len--;
  }
  *p = '\0';
}

/*
 * mic2latin_with_table: a generic single byte charset encoding
 * conversion from the mule internal code to a local charset.
 *
 * mic points to the source string of length len
 * p is the output area (must be large enough!)
 * lc is the mule character set id for the local encoding
 * encoding is the PG identifier for the local encoding
 * tab holds conversion entries for the mule internal code's second byte,
 * starting from 128 (0x80). each entry in the table holds the corresponding
 * code point for the local charset, or 0 if there is no equivalent code.
 */
void
mic2latin_with_table(const unsigned char *mic, unsigned char *p, int len, int lc, int encoding, const unsigned char *tab)
{
  unsigned char c1, c2;

  while (len > 0)
  {
    c1 = *mic;
    if (c1 == 0)
    {

    }
    if (!IS_HIGHBIT_SET(c1))
    {
      /* easy for ASCII */
      *p++ = c1;
      mic++;
      len--;
    }
    else
    {
      int l = pg_mic_mblen(mic);













    }
  }
  *p = '\0';
}

/*
 * comparison routine for bsearch()
 * this routine is intended for combined UTF8 -> local code
 */
static int
compare3(const void *p1, const void *p2)
{







}

/*
 * comparison routine for bsearch()
 * this routine is intended for local code -> combined UTF8
 */
static int
compare4(const void *p1, const void *p2)
{





}

/*
 * store 32bit character representation into multibyte stream
 */
static inline unsigned char *
store_coded_char(unsigned char *dest, uint32 code)
{

















}

/*
 * Convert a character using a conversion radix tree.
 *
 * 'l' is the length of the input character in bytes, and b1-b4 are
 * the input character's bytes.
 */
static inline uint32
pg_mb_radix_conv(const pg_mb_radix_tree *rt, int l, unsigned char b1, unsigned char b2, unsigned char b3, unsigned char b4)
{









































































































}

/*
 * UTF8 ---> local code
 *
 * utf: input string in UTF8 encoding (need not be null-terminated)
 * len: length of input string (in bytes)
 * iso: pointer to the output area (must be large enough!)
                  (output string will be null-terminated)
 * map: conversion map for single characters
 * cmap: conversion map for combined characters
 *		  (optional, pass NULL if none)
 * cmapsize: number of entries in the conversion map for combined characters
 *		  (optional, pass 0 if none)
 * conv_func: algorithmic encoding conversion function
 *		  (optional, pass NULL if none)
 * encoding: PG identifier for the local encoding
 *
 * For each character, the cmap (if provided) is consulted first; if no match,
 * the map is consulted next; if still no match, the conv_func (if provided)
 * is applied.  An error is raised if no match is found.
 *
 * See pg_wchar.h for more details about the data structures used here.
 */
void
UtfToLocal(const unsigned char *utf, int len, unsigned char *iso, const pg_mb_radix_tree *map, const pg_utf_to_local_combined *cmap, int cmapsize, utf_local_conversion_func conv_func, int encoding)
{
  uint32 iutf;
  int l;
  const pg_utf_to_local_combined *cp;

  if (!PG_VALID_ENCODING(encoding))
  {

  }

  for (; len > 0; len -= l)
  {
    unsigned char b1 = 0;
    unsigned char b2 = 0;
    unsigned char b3 = 0;
    unsigned char b4 = 0;

    /* "break" cases all represent errors */
    if (*utf == '\0')
    {

    }

    l = pg_utf_mblen(utf);
    if (len < l)
    {

    }

    if (!pg_utf8_islegal(utf, l))
    {

    }

    if (l == 1)
    {
      /* ASCII case is easy, assume it's one-to-one conversion */
      *iso++ = *utf++;
      continue;
    }

    /* collect coded char of length l */

























    /* First, try with combined map if possible */




































































    /* Now check ordinary map */











    /* if there's a conversion function, try that */











    /* failed to translate this character */

  }

  /* if we broke out of loop early, must be invalid input */
  if (len > 0)
  {

  }

  *iso = '\0';
}

/*
 * local code ---> UTF8
 *
 * iso: input string in local encoding (need not be null-terminated)
 * len: length of input string (in bytes)
 * utf: pointer to the output area (must be large enough!)
                  (output string will be null-terminated)
 * map: conversion map for single characters
 * cmap: conversion map for combined characters
 *		  (optional, pass NULL if none)
 * cmapsize: number of entries in the conversion map for combined characters
 *		  (optional, pass 0 if none)
 * conv_func: algorithmic encoding conversion function
 *		  (optional, pass NULL if none)
 * encoding: PG identifier for the local encoding
 *
 * For each character, the map is consulted first; if no match, the cmap
 * (if provided) is consulted next; if still no match, the conv_func
 * (if provided) is applied.  An error is raised if no match is found.
 *
 * See pg_wchar.h for more details about the data structures used here.
 */
void
LocalToUtf(const unsigned char *iso, int len, unsigned char *utf, const pg_mb_radix_tree *map, const pg_local_to_utf_combined *cmap, int cmapsize, utf_local_conversion_func conv_func, int encoding)
{
  uint32 iiso;
  int l;
  const pg_local_to_utf_combined *cp;

  if (!PG_VALID_ENCODING(encoding))
  {

  }

  for (; len > 0; len -= l)
  {
    unsigned char b1 = 0;
    unsigned char b2 = 0;
    unsigned char b3 = 0;
    unsigned char b4 = 0;

    /* "break" cases all represent errors */
    if (*iso == '\0')
    {

    }

    if (!IS_HIGHBIT_SET(*iso))
    {
      /* ASCII case is easy, assume it's one-to-one conversion */
      *utf++ = *iso++;
      l = 1;
      continue;
    }







    /* collect coded char of length l */





















































    /* if there's a conversion function, try that */











    /* failed to translate this character */

  }

  /* if we broke out of loop early, must be invalid input */
  if (len > 0)
  {

  }

  *utf = '\0';
}