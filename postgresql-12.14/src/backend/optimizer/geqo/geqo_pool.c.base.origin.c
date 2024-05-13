/*------------------------------------------------------------------------
 *
 * geqo_pool.c
 *	  Genetic Algorithm (GA) pool stuff
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/optimizer/geqo/geqo_pool.c
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

/* -- parts of this are adapted from D. Whitley's Genitor algorithm -- */

#include "postgres.h"

#include <float.h>
#include <limits.h>
#include <math.h>

#include "optimizer/geqo_copy.h"
#include "optimizer/geqo_pool.h"
#include "optimizer/geqo_recombination.h"

static int
compare(const void *arg1, const void *arg2);

/*
 * alloc_pool
 *		allocates memory for GA pool
 */
Pool *
alloc_pool(PlannerInfo *root, int pool_size, int string_length)
{




















}

/*
 * free_pool
 *		deallocates memory for GA pool
 */
void
free_pool(PlannerInfo *root, Pool *pool)
{















}

/*
 * random_init_pool
 *		initialize genetic pool
 */
void
random_init_pool(PlannerInfo *root, Pool *pool)
{





































}

/*
 * sort_pool
 *	 sorts input pool according to worth, from smallest to largest
 *
 *	 maybe you have to change compare() for different ordering ...
 */
void
sort_pool(PlannerInfo *root, Pool *pool)
{

}

/*
 * compare
 *	 qsort comparison function for sort_pool
 */
static int
compare(const void *arg1, const void *arg2)
{















}

/* alloc_chromo
 *	  allocates a chromosome and string space
 */
Chromosome *
alloc_chromo(PlannerInfo *root, int string_length)
{






}

/* free_chromo
 *	  deallocates a chromosome and string space
 */
void
free_chromo(PlannerInfo *root, Chromosome *chromo)
{


}

/* spread_chromo
 *	 inserts a new chromosome into the pool, displacing worst gene in pool
 *	 assumes best->worst = smallest->largest
 */
void
spread_chromo(PlannerInfo *root, Chromosome *chromo, Pool *pool)
{

















































































}