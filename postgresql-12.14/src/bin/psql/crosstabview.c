   
                                              
   
                                                                
   
                               
   
#include "postgres_fe.h"

#include "common.h"
#include "crosstabview.h"
#include "pqexpbuffer.h"
#include "psqlscanslash.h"
#include "settings.h"

#include "common/logging.h"

   
                                                                               
                        
   
typedef struct _pivot_field
{
     
                                                                        
                                                                         
                                                                   
     
  char *name;

     
                                                                   
                                                                         
                                                                             
                                                                  
     
  char *sort_value;

     
                                                                     
                                                                      
                                                                             
                                                                           
                                
     
  int rank;
} pivot_field;

                      
typedef struct _avl_node
{
                     
  pivot_field field;

     
                                                                             
              
     
  int height;

     
                                                                          
                                                                       
            
     
  struct _avl_node *children[2];
} avl_node;

   
                                                               
                                    
   
typedef struct _avl_tree
{
  int count;                                 
  avl_node *root;                       
  avl_node *end;                                            
} avl_tree;

static bool
printCrosstab(const PGresult *results, int num_columns, pivot_field *piv_columns, int field_for_columns, int num_rows, pivot_field *piv_rows, int field_for_rows, int field_for_data);
static void
avlInit(avl_tree *tree);
static void
avlMergeValue(avl_tree *tree, char *name, char *sort_value);
static int
avlCollectFields(avl_tree *tree, avl_node *node, pivot_field *fields, int idx);
static void
avlFree(avl_tree *tree, avl_node *node);
static void
rankSort(int num_columns, pivot_field *piv_columns);
static int
indexOfColumn(char *arg, const PGresult *res);
static int
pivotFieldCompare(const void *a, const void *b);
static int
rankCompare(const void *a, const void *b);

   
                                    
   
                                                                         
                                                             
                                                    
   
bool
PrintResultsInCrosstab(const PGresult *res)
{
  bool retval = false;
  avl_tree piv_columns;
  avl_tree piv_rows;
  pivot_field *array_columns = NULL;
  pivot_field *array_rows = NULL;
  int num_columns = 0;
  int num_rows = 0;
  int field_for_rows;
  int field_for_columns;
  int field_for_data;
  int sort_field_for_columns;
  int rn;

  avlInit(&piv_rows);
  avlInit(&piv_columns);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    pg_log_error("\\crosstabview: statement did not return a result set");
    goto error_return;
  }

  if (PQnfields(res) < 3)
  {
    pg_log_error("\\crosstabview: query must return at least three columns");
    goto error_return;
  }

                                                           
  if (pset.ctv_args[0] == NULL)
  {
    field_for_rows = 0;
  }
  else
  {
    field_for_rows = indexOfColumn(pset.ctv_args[0], res);
    if (field_for_rows < 0)
    {
      goto error_return;
    }
  }

                                                              
  if (pset.ctv_args[1] == NULL)
  {
    field_for_columns = 1;
  }
  else
  {
    field_for_columns = indexOfColumn(pset.ctv_args[1], res);
    if (field_for_columns < 0)
    {
      goto error_return;
    }
  }

                                              
  if (field_for_columns == field_for_rows)
  {
    pg_log_error("\\crosstabview: vertical and horizontal headers must be different columns");
    goto error_return;
  }

                                                
  if (pset.ctv_args[2] == NULL)
  {
    int i;

       
                                                                       
                                                                       
                                               
       
    if (PQnfields(res) != 3)
    {
      pg_log_error("\\crosstabview: data column must be specified when query returns more than three columns");
      goto error_return;
    }

    field_for_data = -1;
    for (i = 0; i < PQnfields(res); i++)
    {
      if (i != field_for_rows && i != field_for_columns)
      {
        field_for_data = i;
        break;
      }
    }
    Assert(field_for_data >= 0);
  }
  else
  {
    field_for_data = indexOfColumn(pset.ctv_args[2], res);
    if (field_for_data < 0)
    {
      goto error_return;
    }
  }

                                                                   
  if (pset.ctv_args[3] == NULL)
  {
    sort_field_for_columns = -1;                     
  }
  else
  {
    sort_field_for_columns = indexOfColumn(pset.ctv_args[3], res);
    if (sort_field_for_columns < 0)
    {
      goto error_return;
    }
  }

     
                                                                    
                                                                          
                      
     

  for (rn = 0; rn < PQntuples(res); rn++)
  {
    char *val;
    char *val1;

                    
    val = PQgetisnull(res, rn, field_for_columns) ? NULL : PQgetvalue(res, rn, field_for_columns);
    val1 = NULL;

    if (sort_field_for_columns >= 0 && !PQgetisnull(res, rn, sort_field_for_columns))
    {
      val1 = PQgetvalue(res, rn, sort_field_for_columns);
    }

    avlMergeValue(&piv_columns, val, val1);

    if (piv_columns.count > CROSSTABVIEW_MAX_COLUMNS)
    {
      pg_log_error("\\crosstabview: maximum number of columns (%d) exceeded", CROSSTABVIEW_MAX_COLUMNS);
      goto error_return;
    }

                  
    val = PQgetisnull(res, rn, field_for_rows) ? NULL : PQgetvalue(res, rn, field_for_rows);

    avlMergeValue(&piv_rows, val, NULL);
  }

     
                                                             
     

  num_columns = piv_columns.count;
  num_rows = piv_rows.count;

  array_columns = (pivot_field *)pg_malloc(sizeof(pivot_field) * num_columns);

  array_rows = (pivot_field *)pg_malloc(sizeof(pivot_field) * num_rows);

  avlCollectFields(&piv_columns, piv_columns.root, array_columns, 0);
  avlCollectFields(&piv_rows, piv_rows.root, array_rows, 0);

     
                                                                         
            
     
  if (sort_field_for_columns >= 0)
  {
    rankSort(num_columns, array_columns);
  }

     
                                                 
     
  retval = printCrosstab(res, num_columns, array_columns, field_for_columns, num_rows, array_rows, field_for_rows, field_for_data);

error_return:
  avlFree(&piv_columns, piv_columns.root);
  avlFree(&piv_rows, piv_rows.root);
  pg_free(array_columns);
  pg_free(array_rows);

  return retval;
}

   
                                                                             
                                   
   
static bool
printCrosstab(const PGresult *results, int num_columns, pivot_field *piv_columns, int field_for_columns, int num_rows, pivot_field *piv_rows, int field_for_rows, int field_for_data)
{
  printQueryOpt popt = pset.popt;
  printTableContent cont;
  int i, rn;
  char col_align;
  int *horiz_map;
  bool retval = false;

  printTableInit(&cont, &popt.topt, popt.title, num_columns + 1, num_rows);

                                                           

                                                                      
  printTableAddHeader(&cont, PQfname(results, field_for_rows), false, column_type_alignment(PQftype(results, field_for_rows)));

     
                                                                           
                                                                          
                                       
     
  horiz_map = (int *)pg_malloc(sizeof(int) * num_columns);
  for (i = 0; i < num_columns; i++)
  {
    horiz_map[piv_columns[i].rank] = i;
  }

     
                                                     
     
  col_align = column_type_alignment(PQftype(results, field_for_data));

  for (i = 0; i < num_columns; i++)
  {
    char *colname;

    colname = piv_columns[horiz_map[i]].name ? piv_columns[horiz_map[i]].name : (popt.nullPrint ? popt.nullPrint : "");

    printTableAddHeader(&cont, colname, false, col_align);
  }
  pg_free(horiz_map);

                                                                          
  for (i = 0; i < num_rows; i++)
  {
    int k = piv_rows[i].rank;

    cont.cells[k * (num_columns + 1)] = piv_rows[i].name ? piv_rows[i].name : (popt.nullPrint ? popt.nullPrint : "");
  }
  cont.cellsadded = num_rows * (num_columns + 1);

     
                                        
     
  for (rn = 0; rn < PQntuples(results); rn++)
  {
    int row_number;
    int col_number;
    pivot_field *rp, *cp;
    pivot_field elt;

                         
    if (!PQgetisnull(results, rn, field_for_rows))
    {
      elt.name = PQgetvalue(results, rn, field_for_rows);
    }
    else
    {
      elt.name = NULL;
    }
    rp = (pivot_field *)bsearch(&elt, piv_rows, num_rows, sizeof(pivot_field), pivotFieldCompare);
    Assert(rp != NULL);
    row_number = rp->rank;

                            
    if (!PQgetisnull(results, rn, field_for_columns))
    {
      elt.name = PQgetvalue(results, rn, field_for_columns);
    }
    else
    {
      elt.name = NULL;
    }

    cp = (pivot_field *)bsearch(&elt, piv_columns, num_columns, sizeof(pivot_field), pivotFieldCompare);
    Assert(cp != NULL);
    col_number = cp->rank;

                               
    if (col_number >= 0 && row_number >= 0)
    {
      int idx;

                                           
      idx = 1 + col_number + row_number * (num_columns + 1);

         
                                                               
         
      if (cont.cells[idx] != NULL)
      {
        pg_log_error("\\crosstabview: query result contains multiple data values for row \"%s\", column \"%s\"", rp->name ? rp->name : (popt.nullPrint ? popt.nullPrint : "(null)"), cp->name ? cp->name : (popt.nullPrint ? popt.nullPrint : "(null)"));
        goto error;
      }

      cont.cells[idx] = !PQgetisnull(results, rn, field_for_data) ? PQgetvalue(results, rn, field_for_data) : (popt.nullPrint ? popt.nullPrint : "");
    }
  }

     
                                                                            
               
     
  for (i = 0; i < cont.cellsadded; i++)
  {
    if (cont.cells[i] == NULL)
    {
      cont.cells[i] = "";
    }
  }

  printTable(&cont, pset.queryFout, false, pset.logfile);
  retval = true;

error:
  printTableCleanup(&cont);

  return retval;
}

   
                                                                                
                                                                                   
                                                                                
           
   
static void
avlInit(avl_tree *tree)
{
  tree->end = (avl_node *)pg_malloc0(sizeof(avl_node));
  tree->end->children[0] = tree->end->children[1] = tree->end;
  tree->count = 0;
  tree->root = tree->end;
}

                                                            
static void
avlFree(avl_tree *tree, avl_node *node)
{
  if (node->children[0] != tree->end)
  {
    avlFree(tree, node->children[0]);
    pg_free(node->children[0]);
  }
  if (node->children[1] != tree->end)
  {
    avlFree(tree, node->children[1]);
    pg_free(node->children[1]);
  }
  if (node == tree->root)
  {
                                                                
    if (node != tree->end)
    {
      pg_free(node);
    }
                                                                        
    pg_free(tree->end);
  }
}

                                                                     
static void
avlUpdateHeight(avl_node *n)
{
  n->height = 1 + (n->children[0]->height > n->children[1]->height ? n->children[0]->height : n->children[1]->height);
}

                                                                   
static avl_node *
avlRotate(avl_node **current, int dir)
{
  avl_node *before = *current;
  avl_node *after = (*current)->children[dir];

  *current = after;
  before->children[dir] = after->children[!dir];
  avlUpdateHeight(before);
  after->children[!dir] = before;

  return after;
}

static int
avlBalance(avl_node *n)
{
  return n->children[0]->height - n->children[1]->height;
}

   
                                                                              
                                             
                     
   
static void
avlAdjustBalance(avl_tree *tree, avl_node **node)
{
  avl_node *current = *node;
  int b = avlBalance(current) / 2;

  if (b != 0)
  {
    int dir = (1 - b) / 2;

    if (avlBalance(current->children[dir]) == -b)
    {
      avlRotate(&current->children[dir], !dir);
    }
    current = avlRotate(node, dir);
  }
  if (current != tree->end)
  {
    avlUpdateHeight(current);
  }
}

   
                                                                                
                                                                              
                                                                   
   
static void
avlInsertNode(avl_tree *tree, avl_node **node, pivot_field field)
{
  avl_node *current = *node;

  if (current == tree->end)
  {
    avl_node *new_node = (avl_node *)pg_malloc(sizeof(avl_node));

    new_node->height = 1;
    new_node->field = field;
    new_node->children[0] = new_node->children[1] = tree->end;
    tree->count++;
    *node = new_node;
  }
  else
  {
    int cmp = pivotFieldCompare(&field, &current->field);

    if (cmp != 0)
    {
      avlInsertNode(tree, cmp > 0 ? &current->children[1] : &current->children[0], field);
      avlAdjustBalance(tree, node);
    }
  }
}

                                                                 
static void
avlMergeValue(avl_tree *tree, char *name, char *sort_value)
{
  pivot_field field;

  field.name = name;
  field.rank = tree->count;
  field.sort_value = sort_value;
  avlInsertNode(tree, &tree->root, field);
}

   
                                                                                
                                 
                                                                   
                                                             
   
static int
avlCollectFields(avl_tree *tree, avl_node *node, pivot_field *fields, int idx)
{
  if (node == tree->end)
  {
    return idx;
  }

  idx = avlCollectFields(tree, node->children[0], fields, idx);
  fields[idx] = node->field;
  return avlCollectFields(tree, node->children[1], fields, idx + 1);
}

static void
rankSort(int num_columns, pivot_field *piv_columns)
{
  int *hmap;                                           
                                      
  int i;

  hmap = (int *)pg_malloc(sizeof(int) * num_columns * 2);
  for (i = 0; i < num_columns; i++)
  {
    char *val = piv_columns[i].sort_value;

                                                                        
    if (val && ((*val == '-' && strspn(val + 1, "0123456789") == strlen(val + 1)) || strspn(val, "0123456789") == strlen(val)))
    {
      hmap[i * 2] = atoi(val);
      hmap[i * 2 + 1] = i;
    }
    else
    {
                                                                   
      hmap[i * 2] = 0;
      hmap[i * 2 + 1] = i;
    }
  }

  qsort(hmap, num_columns, sizeof(int) * 2, rankCompare);

  for (i = 0; i < num_columns; i++)
  {
    piv_columns[hmap[i * 2 + 1]].rank = i;
  }

  pg_free(hmap);
}

   
                                                    
                                       
                                                    
   
                                                                      
   
                                              
   
static int
indexOfColumn(char *arg, const PGresult *res)
{
  int idx;

  if (arg[0] && strspn(arg, "0123456789") == strlen(arg))
  {
                                                           
    idx = atoi(arg) - 1;
    if (idx < 0 || idx >= PQnfields(res))
    {
      pg_log_error("\\crosstabview: column number %d is out of range 1..%d", idx + 1, PQnfields(res));
      return -1;
    }
  }
  else
  {
    int i;

       
                                                                         
                                                                           
                                     
       
    dequote_downcase_identifier(arg, true, pset.encoding);

                                                        
    idx = -1;
    for (i = 0; i < PQnfields(res); i++)
    {
      if (strcmp(arg, PQfname(res, i)) == 0)
      {
        if (idx >= 0)
        {
                                                               
          pg_log_error("\\crosstabview: ambiguous column name: \"%s\"", arg);
          return -1;
        }
        idx = i;
      }
    }
    if (idx == -1)
    {
      pg_log_error("\\crosstabview: column name not found: \"%s\"", arg);
      return -1;
    }
  }

  return idx;
}

   
                                                        
                                
                                      
                     
                                                
   
static int
pivotFieldCompare(const void *a, const void *b)
{
  const pivot_field *pa = (const pivot_field *)a;
  const pivot_field *pb = (const pivot_field *)b;

                        
  if (!pb->name)
  {
    return pa->name ? -1 : 0;
  }
  else if (!pa->name)
  {
    return 1;
  }

                       
  return strcmp(pa->name, pb->name);
}

static int
rankCompare(const void *a, const void *b)
{
  return *((const int *)a) - *((const int *)b);
}
