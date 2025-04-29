#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "ds_hmap.h"
#include "ds_str.h"

#include "rest_test_symt.h"

struct rest_test_symt_t {
   char              *name;
   rest_test_symt_t  *parent;
   ds_hmap_t         *hmap;
};


rest_test_symt_t *rest_test_symt_new (const char *name, rest_test_symt_t *parent, size_t nbuckets)
{
   rest_test_symt_t *ret = calloc (1, sizeof *ret);
   if (!ret)
      return NULL;

   ret->parent = parent;

   if (!(ret->name = ds_str_dup (name)) || !(ret->hmap = ds_hmap_new (nbuckets))) {
      free (ret->name);
      // I did not forget to free hmap here; hmap always NULL in this block
      free (ret);
      return NULL;
   }

   return ret;
}

void rest_test_symt_del (rest_test_symt_t **symt)
{
   if (!symt || !*symt)
      return;

   char **keys = NULL;
   size_t nkeys = ds_hmap_keys((*symt)->hmap, (void ***)&keys, NULL);
   for (size_t i=0; i<nkeys; i++) {
      char *value = NULL;
      if ((ds_hmap_get_str_str((*symt)->hmap, keys[i], &value))) {
         free (value);
      }
   }

   free (keys);
   ds_hmap_del ((*symt)->hmap);
   free ((*symt)->name);
   free (*symt);
   *symt = NULL;
}

const char *rest_test_symt_name (rest_test_symt_t *symt)
{
   return symt ? symt->name : "";
}

rest_test_symt_t *rest_test_symt_parent (rest_test_symt_t *symt)
{
   return symt ? symt->parent : NULL;
}


void rest_test_symt_dump (rest_test_symt_t *symt, FILE *fout)
{
   if (!symt)
      return;

   if (!fout)
      fout = stdout;

   char **keys = NULL;
   size_t nkeys = ds_hmap_keys(symt->hmap, (void ***)&keys, NULL);
   for (size_t i=0; i<nkeys; i++) {
      char *value = NULL;
      if (!(ds_hmap_get_str_str(symt->hmap, keys[i], &value))) {
         continue;
      }
      fprintf(fout, "symbol-table [%s] [%s:%s]\n", symt->name, keys[i], value);
   }

   free (keys);
}

bool rest_test_symt_add (rest_test_symt_t *symt,
                         const char *symbol, const char *value)
{
   char *copy = ds_str_dup (value);
   if (!copy)
      return false;
   char *existing = NULL;
   if ((ds_hmap_get_str_str(symt->hmap, symbol, &existing)))
      free (existing);

   return ds_hmap_set_str_str(symt->hmap, symbol, copy) != NULL;
}

void rest_test_symt_clear (rest_test_symt_t *symt, const char *symbol)
{
   char *value = NULL;
   if ((ds_hmap_get_str_str(symt->hmap, symbol, &value))) {
      ds_hmap_remove_str (symt->hmap, symbol);
      free (value);
   }
}


const char *rest_test_symt_value (rest_test_symt_t *symt, const char *symbol)
{
   char *value = NULL;
   if (!symt)
      return NULL;

   if (!(ds_hmap_get_str_str (symt->hmap, symbol, &value)))
      return rest_test_symt_value (symt->parent, symbol);

   return value;
}


