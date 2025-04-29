#include <stdlib.h>

#include "ds_hmap.h"

#include "rest_test_symt.h"

struct rest_test_symt_t {
   rest_test_symt_t *parent;
   ds_hmap_t *hmap;
};


rest_test_symt_t *rest_test_symt_new (rest_test_symt_t *parent)
{
   rest_test_symt_t *ret = calloc (1, sizeof *ret);
   if (!ret)
      return NULL;

   ret->parent = parent;

   if (!(ret->hmap = ds_hmap_new (512))) {
      free (ret);
      return NULL;
   }

   return ret;
}

void rest_test_symt_del (rest_test_symt_t **symt)
{
   if (!symt || !*symt)
      return;

   ds_hmap_del ((*symt)->hmap);
   free (*symt);
   *symt = NULL;
}


