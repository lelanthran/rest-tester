#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "ds_hmap.h"
#include "ds_str.h"

#include "rest_test_token.h"
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
      rest_test_token_t *value = NULL;
      if ((ds_hmap_get_str_ptr((*symt)->hmap, keys[i], (void **)&value))) {
         rest_test_token_del (&value);
      }
   }

   free (keys);
   ds_hmap_del ((*symt)->hmap);
   free ((*symt)->name);
   free (*symt);
   *symt = NULL;
}

const char *rest_test_symt_name (const rest_test_symt_t *symt)
{
   return symt ? symt->name : "";
}

rest_test_symt_t *rest_test_symt_parent (const rest_test_symt_t *symt)
{
   return symt ? symt->parent : NULL;
}

const char *rest_test_symt_set_name (rest_test_symt_t *symt, const char *name)
{
   char *tmp = ds_str_dup (name);
   if (!tmp)
      return NULL;

   free (symt->name);
   symt->name = tmp;
   return tmp;
}


void rest_test_symt_dump (const rest_test_symt_t *symt, FILE *fout)
{
   if (!symt)
      return;

   if (!fout)
      fout = stdout;

   char **keys = NULL;
   size_t nkeys = ds_hmap_keys(symt->hmap, (void ***)&keys, NULL);
   for (size_t i=0; i<nkeys; i++) {
      rest_test_token_t *token = NULL;
      if (!(ds_hmap_get_str_ptr(symt->hmap, keys[i], (void **)&token))) {
         continue;
      }
      const char *value = rest_test_token_value (token);
      fprintf(fout, "symbol-table [%s] [%s:%s]\n", symt->name, keys[i], value);
   }

   free (keys);
}

bool rest_test_symt_add (rest_test_symt_t *symt,
                         const char *symbol, rest_test_token_t *token)
{
   if (!symt)
      return false;

   enum rest_test_token_type_t type = rest_test_token_type (token);
   const char *value = rest_test_token_value (token);
   const char *source = rest_test_token_source (token);
   size_t line_no = rest_test_token_line_no (token);
   rest_test_token_t *copy = rest_test_token_new (type, value, source, line_no);
   if (!copy)
      return false;
   rest_test_token_t *existing = NULL;
   if ((ds_hmap_get_str_ptr (symt->hmap, symbol, (void **)&existing)))
      rest_test_token_del (&existing);

   return ds_hmap_set_str_ptr (symt->hmap, symbol, copy) != NULL;
}

void rest_test_symt_clear (rest_test_symt_t *symt, const char *symbol)
{
   rest_test_token_t *value = NULL;
   if ((ds_hmap_get_str_ptr(symt->hmap, symbol, (void **)&value))) {
      ds_hmap_remove_str (symt->hmap, symbol);
      rest_test_token_del (&value);
   }
}


const rest_test_token_t *rest_test_symt_value (const rest_test_symt_t *symt, const char *symbol)
{
   rest_test_token_t *value = NULL;
   if (!symt)
      return NULL;

   if (!(ds_hmap_get_str_ptr (symt->hmap, symbol, (void **)&value)))
      return rest_test_symt_value (symt->parent, symbol);

   return value;
}


