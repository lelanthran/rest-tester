#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <unistd.h>

#include "ds_str.h"

#include "rest_test_symt.h"
#include "rest_test.h"
#include "rest_test_parse.h"

#define CLEANUP(...) \
do {\
   ERRORF(__VA_ARGS__);\
   goto cleanup;\
} while (0)

static void file_del (char **fname)
{
   if (fname && *fname) {
      remove (*fname);
   }
   free (*fname);
   *fname = NULL;
}

static char *file_new (const char **strings)
{
   bool error = true;
   int fd = -1;
   char *fname = ds_str_dup ("tmp_XXXXXX");
   if (!fname) {
      CLEANUP ("Failed to create temporary file: %m\n");
   }

   if ((fd = mkstemp (fname)) <= 0) {
      CLEANUP ("Failed to create temporary file: %m\n");
   }

   for (size_t i=0; strings[i]; i++) {
      ssize_t slen = strlen (strings[i]);
      ssize_t nbytes = write (fd, strings[i], slen);
      if (nbytes != slen) {
         CLEANUP ("Wrote %zi/%zi bytes: %m\n", nbytes, slen);
      }
      if ((write (fd, "\n", 1)) != 1) {
         CLEANUP ("Failed to write EOL\n");
      }
   }

   close (fd);

   error = false;
cleanup:
   if (error) {
      file_del (&fname);
   }
   return fname;
}


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
   int errcode = -1;
   printf ("testing rest_test\n");
   rest_test_t *rt = rest_test_new ("Test test", "in.rtest", 1, NULL);
   if (!rt) {
      ERRORF ("Failed to allocate test structure\n");
      goto cleanup;
   }

   rest_test_req_set_method (rt, "POST");
   rest_test_req_set_http_version (rt, "HTTP/1.1");
   rest_test_req_set_uri (rt, "/some/test/ur");
   rest_test_req_set_header (rt, "in.rtest", 5, "X-Header-One: headeR-ONE-VAlue");
   rest_test_req_set_header (rt, "in.rtest", 6, "X-Header-Two: headeR-TWO-VAlue");
   rest_test_req_set_header (rt, "in.rtest", 7, "X-Header-Three: heaDER-THREe-value");
   rest_test_req_set_body (rt, "A Body Line\n");
   rest_test_req_append_body (rt, "Another Body Line\n");

   rest_test_rsp_set_http_version (rt, "http/1.1");
   rest_test_rsp_set_status_code (rt, "201");
   rest_test_rsp_set_reason (rt, "Okay");
   rest_test_rsp_set_body (rt, "A Response Line\n");
   rest_test_rsp_append_body (rt, "Another Response Line\n");

   rest_test_dump (rt, stdout);

   errcode = rest_test_lasterr (rt);
cleanup:
   rest_test_del (&rt);
   return errcode;
}

int test_parser (void)
{
   int ret = 1;
   static const char *test1[] = {
      "# This is a comment",
      "",
      ".test \"First\" \"\n  test\"",
      "",
      ".uri BASE_URI",
      ".method \"POST\"",
      ".http_version \"HTTP/1.1\"",
      ".body \"",
      "   First line ",
      "  Second Line ",
      " Third line ",
      "\"",
      "# Set some variables",
      ".global BASE_URI \"localhost:8081\"",
      ".parent BASE_URI \"localhost:8082\"",
      ".local  BASE_URI \"localhost:8083\"",
      NULL,
   };
   char *testfile1 = file_new (test1);

   if (!testfile1) {
      CLEANUP ("Failed to create file\n");
   }


   rest_test_t **rts = rest_test_parse_file (NULL, testfile1);
   if (!rts) {
      CLEANUP ("Failed to parse [%s]\n", testfile1);
   }

   for (size_t i=0; rts[i]; i++) {
      rest_test_dump (rts[i], stdout);
      rest_test_del (&rts[i]);
   }

   ret = 0;
cleanup:
   free (rts);
   file_del (&testfile1);
   return ret;
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
      { "parser",    test_parser },
   };

   printf ("%i\n", argc);
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

   // No parameters? Run everything
   if (argc == 1) {
      for (size_t j=0; j<sizeof test_funcs/sizeof test_funcs[0]; j++) {
         int retcode = test_funcs[j].fptr ();
         printf ("test %s returned %i\n", test_funcs[j].name, retcode);
         ret += retcode;
         ntests++;
      }
   }


   printf ("Ran %zu tests, with %i errors\n", ntests, ret);
   return ret;
}


