/*-------------------------------------------------------------------------
 *
 * bufmask.c
 *	  Routines for buffer masking. Used to mask certain bits
 *	  in a page which can be different when the WAL is generated
 *	  and when the WAL is applied.
 *
 * Portions Copyright (c) 2016-2019, PostgreSQL Global Development Group
 *
 * Contains common routines required for masking a page.
 *
 * IDENTIFICATION
 *	  src/backend/access/common/bufmask.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/bufmask.h"

/*
 * mask_page_lsn
 *
 * In consistency checks, the LSN of the two pages compared will likely be
 * different because of concurrent operations when the WAL is generated and
 * the state of the page when WAL is applied. Also, mask out checksum as
 * masking anything else on page means checksum is not going to match as well.
 */
void
mask_page_lsn_and_checksum(Page page)
{




}

/*
 * mask_page_hint_bits
 *
 * Mask hint bits in PageHeader. We want to ignore differences in hint bits,
 * since they can be set without emitting any WAL.
 */
void
mask_page_hint_bits(Page page)
{















}

/*
 * mask_unused_space
 *
 * Mask the unused space of a page between pd_lower and pd_upper.
 */
void
mask_unused_space(Page page)
{











}

/*
 * mask_lp_flags
 *
 * In some index AMs, line pointer flags can be modified in master without
 * emitting any WAL record.
 */
void
mask_lp_flags(Page page)
{












}

/*
 * mask_page_content
 *
 * In some index AMs, the contents of deleted pages need to be almost
 * completely ignored.
 */
void
mask_page_content(Page page)
{






}
