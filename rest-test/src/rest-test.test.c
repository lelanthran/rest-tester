#include <stdio.h>
#include <stdlib.h>

#include "rest_test_symt.h"
#include "rest_test.h"

int main (void)
{
   int ret = EXIT_FAILURE;

   rest_test_symt_t *symt = rest_test_symt_new (NULL);
   if (!symt) {
      ERRORF ("OOM error allocating symbol table");
      goto cleanup;
   }

   rest_test_symt_del (&symt);
   ret = EXIT_SUCCESS;

cleanup:
   ret = EXIT_SUCCESS;
   return ret;
}


