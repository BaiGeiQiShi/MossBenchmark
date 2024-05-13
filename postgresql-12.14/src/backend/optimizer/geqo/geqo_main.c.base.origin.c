/*------------------------------------------------------------------------
 *
 * geqo_main.c
 *	  solution to the query optimization problem
 *	  by means of a Genetic Algorithm (GA)
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/optimizer/geqo/geqo_main.c
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

#include <math.h>

#include "optimizer/geqo_misc.h"
#include "optimizer/geqo_mutation.h"
#include "optimizer/geqo_pool.h"
#include "optimizer/geqo_random.h"
#include "optimizer/geqo_selection.h"

/*
 * Configuration options
 */
int Geqo_effort;
int Geqo_pool_size;
int Geqo_generations;
double Geqo_selection_bias;
double Geqo_seed;

static int
gimme_pool_size(int nr_rel);
static int
gimme_number_generations(int pool_size);

/* complain if no recombination mechanism is #define'd */
#if !defined(ERX) && !defined(PMX) && !defined(CX) && !defined(PX) && !defined(OX1) && !defined(OX2)
#error "must choose one GEQO recombination mechanism in geqo.h"
#endif

/*
 * geqo
 *	  solution of the query optimization problem
 *	  similar to a constrained Traveling Salesman Problem (TSP)
 */

RelOptInfo *
geqo(PlannerInfo *root, int number_of_rels, List *initial_rels)
{











































































































































































































































}

/*
 * Return either configured pool size or a good default
 *
 * The default is based on query size (no. of relations) = 2^(QS+1),
 * but constrained to a range based on the effort value.
 */
static int
gimme_pool_size(int nr_rel)
{

























}

/*
 * Return either configured number of generations or a good default
 *
 * The default is the same as the pool size, which allows us to be
 * sure that less-fit individuals get pushed out of the breeding
 * population before the run finishes.
 */
static int
gimme_number_generations(int pool_size)
{






}