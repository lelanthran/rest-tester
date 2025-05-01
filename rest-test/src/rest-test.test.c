#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "rest_test_symt.h"
#include "rest_test.h"

int test_symt (void)
{
   int errcount = 0;
   static const struct {
      const char *name;
      const char *value;
   } tests[] = {
      { "name-one",     "1-one" },
      { "name-two",     "1-two" },
      { "name-three",   "1-three" },
      { "name-three",   "2-three" },
   };

   rest_test_symt_t *symt = rest_test_symt_new ("test-1", NULL, 3);
   if (!symt) {
      ERRORF ("OOM error allocating symbol table");
      goto cleanup;
   }

   rest_test_symt_t *child = rest_test_symt_new ("test-2", symt, 3);
   if (!child) {
      ERRORF ("OOM error allocating child table");
      goto cleanup;
   }

   for (size_t i=0; i<sizeof tests/sizeof tests[0]; i++) {
      if (!(rest_test_symt_add (symt, tests[i].name, tests[i].value)) ||
            !(rest_test_symt_add (child, tests[i].name, tests[i].value))) {
         ERRORF ("Failed to add [%s:%s]\n", tests[i].name, tests[i].value);
         errcount++;
      }
   }

   rest_test_symt_clear (child, "name-one");
   rest_test_symt_clear (child, "name-three");

   rest_test_symt_dump (symt, stdout);
   rest_test_symt_dump (child, stdout);

   printf ("Child: [%s]\n", rest_test_symt_name (child));
   printf ("Parent: [%s]\n", rest_test_symt_name (rest_test_symt_parent (child)));

   for (size_t i=0; i<sizeof tests/sizeof tests[0]; i++) {
      const char *value = rest_test_symt_value (child, tests[i].name);
      if (!value) {
         ERRORF ("Failed to add [%s:%s]\n", tests[i].name, tests[i].value);
         errcount++;
         continue;
      }
      printf ("[%s:%s]\n", tests[i].name, value);
   }

cleanup:
   rest_test_symt_del (&symt);
   rest_test_symt_del (&child);

   printf ("Encountered %i errors\n", errcount);
   return errcount;
}

int test_rest_test (void)
{
   rest_test_t *rt = rest_test_new ("Test test", "in.rtest", 1, NULL);

   printf ("testing rest_test\n");

   rest_test_del (&rt);
   return 0;
}

int main (int argc, char **argv)
{
   int ret = 0;

   struct {
      const char *name;
      int (*fptr) (void);
   } test_funcs[] = {
      { "symt",      test_symt },
      { "rest_test", test_rest_test },
   };

   size_t ntests = 0;
   for (int i=1; i<argc; i++) {
      for (size_t j=0; j<sizeof test_funcs/sizeof test_funcs[0]; j++) {
         if ((strcmp (test_funcs[j].name, argv[i])) == 0) {
            int retcode = test_funcs[j].fptr ();
            printf ("test %s returned %i\n", test_funcs[j].name, retcode);
            ret += retcode;
            ntests++;
         }
      }
   }

   printf ("Ran %zu tests, with %i errors\n", ntests, ret);
   return ret;
}


