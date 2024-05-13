/*------------------------------------------------------------------------
 *
 * geqo_erx.c
 *	 edge recombination crossover [ER]
 *
 * src/backend/optimizer/geqo/geqo_erx.c
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

/* the edge recombination algorithm is adopted from Genitor : */
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
#include "optimizer/geqo_recombination.h"
#include "optimizer/geqo_random.h"

#if defined(ERX)

static int
gimme_edge(PlannerInfo *root, Gene gene1, Gene gene2, Edge *edge_table);
static void
remove_gene(PlannerInfo *root, Gene gene, Edge edge, Edge *edge_table);
static Gene
gimme_gene(PlannerInfo *root, Edge edge, Edge *edge_table);

static Gene
edge_failure(PlannerInfo *root, Gene *gene, int index, Edge *edge_table, int num_gene);

/* alloc_edge_table
 *
 *	 allocate memory for edge table
 *
 */

Edge *
alloc_edge_table(PlannerInfo *root, int num_gene)
{










}

/* free_edge_table
 *
 *	  deallocate memory of edge table
 *
 */
void
free_edge_table(PlannerInfo *root, Edge *edge_table)
{

}

/* gimme_edge_table
 *
 *	 fills a data structure which represents the set of explicit
 *	 edges between points in the (2) input genes
 *
 *	 assumes circular tours and bidirectional edges
 *
 *	 gimme_edge() will set "shared" edges to negative values
 *
 *	 returns average number edges/city in range 2.0 - 4.0
 *	 where 2.0=homogeneous; 4.0=diverse
 *
 */
float
gimme_edge_table(PlannerInfo *root, Gene *tour1, Gene *tour2, int num_gene, Edge *edge_table)
{





































}

/* gimme_edge
 *
 *	  registers edge from city1 to city2 in input edge table
 *
 *	  no assumptions about directionality are made;
 *	  therefore it is up to the calling routine to
 *	  call gimme_edge twice to make a bi-directional edge
 *	  between city1 and city2;
 *	  uni-directional edges are possible as well (just call gimme_edge
 *	  once with the direction from city1 to city2)
 *
 *	  returns 1 if edge was not already registered and was just added;
 *			  0 if edge was already registered and edge_table is
 *unchanged
 */
static int
gimme_edge(PlannerInfo *root, Gene gene1, Gene gene2, Edge *edge_table)
{




























}

/* gimme_tour
 *
 *	  creates a new tour using edges from the edge table.
 *	  priority is given to "shared" edges (i.e. edges which
 *	  all parent genes possess and are marked as negative
 *	  in the edge table.)
 *
 */
int
gimme_tour(PlannerInfo *root, Edge *edge_table, Gene *new_gene, int num_gene)
{



































}

/* remove_gene
 *
 *	 removes input gene from edge_table.
 *	 input edge is used
 *	 to identify deletion locations within edge table.
 *
 */
static void
remove_gene(PlannerInfo *root, Gene gene, Edge edge, Edge *edge_table)
{





























}

/* gimme_gene
 *
 *	  priority is given to "shared" edges
 *	  (i.e. edges which both genes possess)
 *
 */
static Gene
gimme_gene(PlannerInfo *root, Edge edge, Edge *edge_table)
{





















































































}

/* edge_failure
 *
 *	  routine for handling edge failure
 *
 */
static Gene
edge_failure(PlannerInfo *root, Gene *gene, int index, Edge *edge_table, int num_gene)
{



































































































}

#endif /* defined(ERX) */