/*-------------------------------------------------------------------------
 *
 * pqformat.c
 *		Routines for formatting and parsing frontend/backend messages
 *
 * Outgoing messages are built up in a StringInfo buffer (which is expansible)
 * and then sent in a single call to pq_putmessage.  This module provides data
 * formatting/conversion routines that are needed to produce valid messages.
 * Note in particular the distinction between "raw data" and "text"; raw data
 * is message protocol characters and binary values that are not subject to
 * character set conversion, while text is converted by character encoding
 * rules.
 *
 * Incoming messages are similarly read into a StringInfo buffer, via
 * pq_getmessage, and then parsed and converted from that using the routines
 * in this module.
 *
 * These same routines support reading and writing of external binary formats
 * (typsend/typreceive routines).  The conversion routines for individual
 * data types are exactly the same, only initialization and completion
 * are different.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *	src/backend/libpq/pqformat.c
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 * Message assembly and output:
 *		pq_beginmessage - initialize StringInfo buffer
 *		pq_sendbyte		- append a raw byte to a StringInfo
 *buffer pq_sendint		- append a binary integer to a StringInfo buffer
 *		pq_sendint64	- append a binary 8-byte int to a StringInfo
 *buffer pq_sendfloat4	- append a float4 to a StringInfo buffer pq_sendfloat8
 *- append a float8 to a StringInfo buffer pq_sendbytes	- append raw data to a
 *StringInfo buffer pq_sendcountedtext - append a counted text string (with
 *character set conversion) pq_sendtext		- append a text string (with
 *conversion) pq_sendstring	- append a null-terminated text string (with
 *conversion) pq_send_ascii_string - append a null-terminated text string
 *(without conversion) pq_endmessage	- send the completed message to the
 *frontend Note: it is also possible to append data to the StringInfo buffer
 *using the regular StringInfo routines, but this is discouraged since required
 * character set conversion may not occur.
 *
 * typsend support (construct a bytea value containing external binary data):
 *		pq_begintypsend - initialize StringInfo buffer
 *		pq_endtypsend	- return the completed string as a "bytea*"
 *
 * Special-case message output:
 *		pq_puttextmessage - generate a character set-converted message
 *in one step pq_putemptymessage - convenience routine for message with empty
 *body
 *
 * Message parsing after input:
 *		pq_getmsgbyte	- get a raw byte from a message buffer
 *		pq_getmsgint	- get a binary integer from a message buffer
 *		pq_getmsgint64	- get a binary 8-byte int from a message buffer
 *		pq_getmsgfloat4 - get a float4 from a message buffer
 *		pq_getmsgfloat8 - get a float8 from a message buffer
 *		pq_getmsgbytes	- get raw data from a message buffer
 *		pq_copymsgbytes - copy raw data from a message buffer
 *		pq_getmsgtext	- get a counted text string (with conversion)
 *		pq_getmsgstring - get a null-terminated text string (with
 *conversion) pq_getmsgrawstring - get a null-terminated text string - NO
 *conversion pq_getmsgend	- verify message fully consumed
 */

#include "postgres.h"

#include <sys/param.h>

#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "mb/pg_wchar.h"
#include "port/pg_bswap.h"

/* --------------------------------
 *		pq_beginmessage		- initialize for sending a message
 * --------------------------------
 */
void
pq_beginmessage(StringInfo buf, char msgtype)
{








}

/* --------------------------------

 *		pq_beginmessage_reuse - initialize for sending a message, reuse
 buffer
 *
 * This requires the buffer to be allocated in a sufficiently long-lived
 * memory context.
 * --------------------------------
 */
void
pq_beginmessage_reuse(StringInfo buf, char msgtype)
{








}

/* --------------------------------
 *		pq_sendbytes	- append raw data to a StringInfo buffer
 * --------------------------------
 */
void
pq_sendbytes(StringInfo buf, const char *data, int datalen)
{


}

/* --------------------------------
 *		pq_sendcountedtext - append a counted text string (with
 *character set conversion)
 *
 * The data sent to the frontend by this routine is a 4-byte count field
 * followed by the string.  The count includes itself or not, as per the
 * countincludesself flag (pre-3.0 protocol requires it to include itself).
 * The passed text string need not be null-terminated, and the data sent
 * to the frontend isn't either.
 * --------------------------------
 */
void
pq_sendcountedtext(StringInfo buf, const char *str, int slen, bool countincludesself)
{
















}

/* --------------------------------
 *		pq_sendtext		- append a text string (with conversion)
 *
 * The passed text string need not be null-terminated, and the data sent
 * to the frontend isn't either.  Note that this is not actually useful
 * for direct frontend transmissions, since there'd be no way for the
 * frontend to determine the string length.  But it is useful for binary
 * format conversions.
 * --------------------------------
 */
void
pq_sendtext(StringInfo buf, const char *str, int slen)
{













}

/* --------------------------------
 *		pq_sendstring	- append a null-terminated text string (with
 *conversion)
 *
 * NB: passed text string must be null-terminated, and so is the data
 * sent to the frontend.
 * --------------------------------
 */
void
pq_sendstring(StringInfo buf, const char *str)
{














}

/* --------------------------------
 *		pq_send_ascii_string	- append a null-terminated text string
 *(without conversion)
 *
 * This function intentionally bypasses encoding conversion, instead just
 * silently replacing any non-7-bit-ASCII characters with question marks.
 * It is used only when we are having trouble sending an error message to
 * the client with normal localization and encoding conversion.  The caller
 * should already have taken measures to ensure the string is just ASCII;
 * the extra work here is just to make certain we don't send a badly encoded
 * string to the client (which might or might not be robust about that).
 *
 * NB: passed text string must be null-terminated, and so is the data
 * sent to the frontend.
 * --------------------------------
 */
void
pq_send_ascii_string(StringInfo buf, const char *str)
{











}

/* --------------------------------
 *		pq_sendfloat4	- append a float4 to a StringInfo buffer
 *
 * The point of this routine is to localize knowledge of the external binary
 * representation of float4, which is a component of several datatypes.
 *
 * We currently assume that float4 should be byte-swapped in the same way
 * as int4.  This rule is not perfect but it gives us portability across
 * most IEEE-float-using architectures.
 * --------------------------------
 */
void
pq_sendfloat4(StringInfo buf, float4 f)
{








}

/* --------------------------------
 *		pq_sendfloat8	- append a float8 to a StringInfo buffer
 *
 * The point of this routine is to localize knowledge of the external binary
 * representation of float8, which is a component of several datatypes.
 *
 * We currently assume that float8 should be byte-swapped in the same way
 * as int8.  This rule is not perfect but it gives us portability across
 * most IEEE-float-using architectures.
 * --------------------------------
 */
void
pq_sendfloat8(StringInfo buf, float8 f)
{








}

/* --------------------------------
 *		pq_endmessage	- send the completed message to the frontend
 *
 * The data buffer is pfree()d, but if the StringInfo was allocated with
 * makeStringInfo then the caller must still pfree it.
 * --------------------------------
 */
void
pq_endmessage(StringInfo buf)
{





}

/* --------------------------------
 *		pq_endmessage_reuse	- send the completed message to the
 frontend
 *
 * The data buffer is *not* freed, allowing to reuse the buffer with
 * pq_beginmessage_reuse.
 --------------------------------
 */

void
pq_endmessage_reuse(StringInfo buf)
{


}

/* --------------------------------
 *		pq_begintypsend		- initialize for constructing a bytea
 *result
 * --------------------------------
 */
void
pq_begintypsend(StringInfo buf)
{






}

/* --------------------------------
 *		pq_endtypsend	- finish constructing a bytea result
 *
 * The data buffer is returned as the palloc'd bytea value.  (We expect
 * that it will be suitably aligned for this because it has been palloc'd.)
 * We assume the StringInfoData is just a local variable in the caller and
 * need not be pfree'd.
 * --------------------------------
 */
bytea *
pq_endtypsend(StringInfo buf)
{







}

/* --------------------------------
 *		pq_puttextmessage - generate a character set-converted message
 *in one step
 *
 *		This is the same as the pqcomm.c routine pq_putmessage, except
 *that the message body is a null-terminated string to which encoding conversion
 *applies.
 * --------------------------------
 */
void
pq_puttextmessage(char msgtype, const char *str)
{











}

/* --------------------------------
 *		pq_putemptymessage - convenience routine for message with empty
 *body
 * --------------------------------
 */
void
pq_putemptymessage(char msgtype)
{

}

/* --------------------------------
 *		pq_getmsgbyte	- get a raw byte from a message buffer
 * --------------------------------
 */
int
pq_getmsgbyte(StringInfo msg)
{





}

/* --------------------------------
 *		pq_getmsgint	- get a binary integer from a message buffer
 *
 *		Values are treated as unsigned.
 * --------------------------------
 */
unsigned int
pq_getmsgint(StringInfo msg, int b)
{

























}

/* --------------------------------
 *		pq_getmsgint64	- get a binary 8-byte int from a message buffer
 *
 * It is tempting to merge this with pq_getmsgint, but we'd have to make the
 * result int64 for all data widths --- that could be a big performance
 * hit on machines where int64 isn't efficient.
 * --------------------------------
 */
int64
pq_getmsgint64(StringInfo msg)
{





}

/* --------------------------------
 *		pq_getmsgfloat4 - get a float4 from a message buffer
 *
 * See notes for pq_sendfloat4.
 * --------------------------------
 */
float4
pq_getmsgfloat4(StringInfo msg)
{








}

/* --------------------------------
 *		pq_getmsgfloat8 - get a float8 from a message buffer
 *
 * See notes for pq_sendfloat8.
 * --------------------------------
 */
float8
pq_getmsgfloat8(StringInfo msg)
{








}

/* --------------------------------
 *		pq_getmsgbytes	- get raw data from a message buffer
 *
 *		Returns a pointer directly into the message buffer; note this
 *		may not have any particular alignment.
 * --------------------------------
 */
const char *
pq_getmsgbytes(StringInfo msg, int datalen)
{









}

/* --------------------------------
 *		pq_copymsgbytes - copy raw data from a message buffer
 *
 *		Same as above, except data is copied to caller's buffer.
 * --------------------------------
 */
void
pq_copymsgbytes(StringInfo msg, char *buf, int datalen)
{






}

/* --------------------------------
 *		pq_getmsgtext	- get a counted text string (with conversion)
 *
 *		Always returns a pointer to a freshly palloc'd result.
 *		The result has a trailing null, *and* we return its strlen in
 **nbytes.
 * --------------------------------
 */
char *
pq_getmsgtext(StringInfo msg, int rawbytes, int *nbytes)
{























}

/* --------------------------------
 *		pq_getmsgstring - get a null-terminated text string (with
 *conversion)
 *
 *		May return a pointer directly into the message buffer, or a
 *pointer to a palloc'd conversion result.
 * --------------------------------
 */
const char *
pq_getmsgstring(StringInfo msg)
{


















}

/* --------------------------------
 *		pq_getmsgrawstring - get a null-terminated text string - NO
 *conversion
 *
 *		Returns a pointer directly into the message buffer.
 * --------------------------------
 */
const char *
pq_getmsgrawstring(StringInfo msg)
{


















}

/* --------------------------------
 *		pq_getmsgend	- verify message fully consumed
 * --------------------------------
 */
void
pq_getmsgend(StringInfo msg)
{




}