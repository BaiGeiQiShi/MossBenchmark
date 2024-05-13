/*-------------------------------------------------------------------------
 *
 * geqo_selection.c
 *	  linear selection scheme for the genetic query optimizer
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/optimizer/geqo/geqo_selection.c
 *
 *-------------------------------------------------------------------------
 */

/* contributed by:
   =*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=
   *  Martin Utesch				 * Institute of Automatic
   Control	   * =							 =
   University of Mining and Technology =
   *  utesch@aut.tu-freiberg.de  * Freiberg, Germany *
   =*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=
 */

/* this is adopted from D. Whitley's Genitor algorithm */

/*************************************************************/
/*															 */
/*	Copyright (c) 1990
 */
/*	Darrell L. Whitley
 */
/*	Computer Science Department
 */
/*	Colorado State University
 */
/*															 */
/*	Permission is hereby granted to copy all or any part of  */
/*	this program for free distribution.   The author's name  */
/*	and this copyright notice must be included in any copy.  */
/*															 */
/*************************************************************/

#include "postgres.h"

#include <math.h>

#include "optimizer/geqo_copy.h"
#include "optimizer/geqo_random.h"
#include "optimizer/geqo_selection.h"

static int
linear_rand(PlannerInfo *root, int max, double bias);

/*
 * geqo_selection
 *	 according to bias described by input parameters,
 *	 first and second genes are selected from the pool
 */
void
geqo_selection(PlannerInfo *root, Chromosome *momma, Chromosome *daddy, Pool *pool, double bias)
{























}

/*
 * linear_rand
 *	  generates random integer between 0 and input max number
 *	  using input linear bias
 *
 *	  bias is y-intercept of linear distribution
 *
 *	  probability distribution function is: f(x) = bias - 2(bias - 1)x
 *			 bias = (prob of first rule) / (prob of middle rule)
 */
static int
linear_rand(PlannerInfo *root, int pool_size, double bias)
{























}