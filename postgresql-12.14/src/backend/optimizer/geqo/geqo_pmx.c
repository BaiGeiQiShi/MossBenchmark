                                                                           
   
              
   
                                                
                                                
                              
   
                                         
   
                                                                            
   

                   
                                                                         
                                                              
                                                    
                                                             
                                                                         
   

                                                 
                                                               
                    
                                  
                                  
                                         
                                       
                    
                                                              
                                                              
                                                              
                    
                                                               

#include "postgres.h"
#include "optimizer/geqo_random.h"
#include "optimizer/geqo_recombination.h"

#if defined(PMX)

       
   
                                
   
void
pmx(PlannerInfo *root, Gene *tour1, Gene *tour2, Gene *offspring, int num_gene)
{
  int *failed = (int *)palloc((num_gene + 1) * sizeof(int));
  int *from = (int *)palloc((num_gene + 1) * sizeof(int));
  int *indx = (int *)palloc((num_gene + 1) * sizeof(int));
  int *check_list = (int *)palloc((num_gene + 1) * sizeof(int));

  int left, right, temp, i, j, k;
  int mx_fail, found, mx_hold;

                                                             
                                                 
  for (k = 0; k < num_gene; k++)
  {
    failed[k] = -1;
    from[k] = -1;
    check_list[k + 1] = 0;
  }

                               
  left = geqo_randint(root, num_gene - 1, 0);
  right = geqo_randint(root, num_gene - 1, 0);

  if (left > right)
  {
    temp = left;
    left = right;
    right = temp;
  }

                                 
  for (k = 0; k < num_gene; k++)
  {
    offspring[k] = tour2[k];
    from[k] = DAD;
    check_list[tour2[k]]++;
  }

                                 
  for (k = left; k <= right; k++)
  {
    check_list[offspring[k]]--;
    offspring[k] = tour1[k];
    from[k] = MOM;
    check_list[tour1[k]]++;
  }

                     

  mx_fail = 0;

              

  for (k = left; k <= right; k++)
  {                                      

    if (tour1[k] == tour2[k])
    {
      found = 1;                          
    }

    else
    {
      found = 0;                          

      j = 0;
      while (!(found) && (j < num_gene))
      {
        if ((offspring[j] == tour1[k]) && (from[j] == DAD))
        {

          check_list[offspring[j]]--;
          offspring[j] = tour2[k];
          found = 1;
          check_list[tour2[k]]++;
        }

        j++;
      }
    }

    if (!(found))
    {                             
      failed[mx_fail] = (int)tour1[k];
      indx[mx_fail] = k;
      mx_fail++;
    }

  }              

              

                                              
  if (mx_fail > 0)
  {
    mx_hold = mx_fail;

    for (k = 0; k < mx_hold; k++)
    {
      found = 0;

      j = 0;
      while (!(found) && (j < num_gene))
      {

        if ((failed[k] == (int)offspring[j]) && (from[j] == DAD))
        {
          check_list[offspring[j]]--;
          offspring[j] = tour2[indx[k]];
          check_list[tour2[indx[k]]]++;

          found = 1;
          failed[k] = -1;
          mx_fail--;
        }

        j++;
      }

    }               

  }              

              

  for (k = 1; k <= num_gene; k++)
  {

    if (check_list[k] > 1)
    {
      i = 0;

      while (i < num_gene)
      {
        if ((offspring[i] == (Gene)k) && (from[i] == DAD))
        {
          j = 1;

          while (j <= num_gene)
          {
            if (check_list[j] == 0)
            {
              offspring[i] = (Gene)j;
              check_list[k]--;
              check_list[j]++;
              i = num_gene + 1;
              j = i;
            }

            j++;
          }

        }              

        i++;
      }                
    }
  }               

  pfree(failed);
  pfree(from);
  pfree(indx);
  pfree(check_list);
}

#endif                   
