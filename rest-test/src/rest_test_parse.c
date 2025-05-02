#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include "ds_str.h"

#include "rest_test_symt.h"
#include "rest_test.h"
#include "rest_test_token.h"
#include "rest_test_parse.h"


#define CLEANUP(...) \
do {\
   ERRORF(__VA_ARGS__);\
   goto cleanup;\
} while (0)


static bool array_push (void ***array, size_t *nitems, void *el)
{
   void **tmp = realloc (*array, (sizeof *tmp) * ((*nitems) + 2));
   if (!tmp)
      return false;
   *array = tmp;
   tmp[*nitems] = el;
   tmp[(*nitems) + 1] = NULL;
   (*nitems) = (*nitems) + 1;
   return true;
}



/* *********************************************************************************
 * Directive identification
 */

enum directive_t {
   directive_UNKNOWN,
   directive_GLOBAL,
   directive_PARENT,
   directive_LOCAL,

   directive_TEST,
   directive_METHOD,
   directive_URI,
   directive_HTTP_VERSION,
   directive_HEADER,
   directive_BODY,

   directive_ASSERT,
};
struct prefix_t {
   const char *prefix;
   enum directive_t directive;
};

static const struct prefix_t directives[] = {
   { ".global",         directive_GLOBAL        },
   { ".parent",         directive_PARENT        },
   { ".local",          directive_LOCAL         },

   { ".test",           directive_TEST          },
   { ".method",         directive_METHOD        },
   { ".uri",            directive_URI           },
   { ".http_version",   directive_HTTP_VERSION  },
   { ".header",         directive_HEADER        },
   { ".body",           directive_BODY          },

   { ".assert",         directive_ASSERT        },
};

static size_t nprefix = sizeof directives/sizeof directives[0];

static enum directive_t directive_value (const char *line)
{
   for (size_t i=0; i<nprefix; i++) {
      if ((strstr (line, directives[i].prefix)) == line) {
         return directives[i].directive;
      }
   }

   return directive_UNKNOWN;
}







/* *********************************************************************************
 * Public functions.
 */

rest_test_t **rest_test_parse_file (rest_test_symt_t *st, const char *filename)
{
   FILE *inf = fopen (filename, "r");
   if (!inf) {
      return NULL;
   }
   rest_test_t **ret = rest_test_parse_stream (st, inf, filename);
   fclose (inf);

   return ret;
}

rest_test_t **rest_test_parse_stream (rest_test_symt_t *st, FILE *inf, const char *source)
{
   bool error = true;
   size_t line_no = 1;
   struct rest_test_token_t *token = NULL;
   rest_test_t **ret = NULL;
   size_t nitems = 0;
   rest_test_t *current = rest_test_new ("", source, line_no, st);
   rest_test_symt_t *global = st;

   // At most two parameters for a directive
   struct rest_test_token_t *params[2] = { NULL, NULL };

   // Find the topmost symbol table
   while (rest_test_symt_parent (global)) {
      global = rest_test_symt_parent (global);
   }

   // Read directive token, then dispatch an action based on the directive
   while ((token = rest_test_token_next (inf, source, &line_no))) {
      enum rest_test_token_type_t type = rest_test_token_type (token);
      const char *value = rest_test_token_value (token);
      if (type == token_UNKNOWN) {
         CLEANUP ("[%s:%zu] Unknown token type found\n", source, line_no);
      }
      if (type == token_NONE) {
         if (!(array_push ((void ***)&ret, &nitems, current))) {
            CLEANUP ("OOM appending to array\n");
         }
         current = NULL;
         break;
      }
      if (type != token_DIRECTIVE) {
         CLEANUP ("Expected directive, found token of type '%s':[%s]\n",
                  rest_test_token_type_string (type), value);
      }

      rest_test_token_del (&params[0]);
      rest_test_token_del (&params[1]);

#define CHECK_PARAMS(n)  \
for (size_t i=0; i<n; i++) {\
   if (params[i] == NULL) {\
      CLEANUP ("[%s:%zu] (%s) Expected parameter %zu, found NULL\n",\
               source, line_no, value, i+1);\
   }\
}
      bool dispatch_code = false;
      switch (directive_value (value)) {
         case directive_GLOBAL:
            params[0] = rest_test_token_next (inf, source, &line_no);
            params[1] = rest_test_token_next (inf, source, &line_no);
            CHECK_PARAMS(2);
            dispatch_code = rest_test_symt_add(global, params[0], params[1]);
            break;

         case directive_PARENT:
            params[0] = rest_test_token_next (inf, source, &line_no);
            params[1] = rest_test_token_next (inf, source, &line_no);
            CHECK_PARAMS(2);
            dispatch_code = rest_test_symt_add(st, params[0], params[1]);
            break;

         case directive_LOCAL:
         case directive_TEST:
         case directive_METHOD:
         case directive_URI:
         case directive_HTTP_VERSION:
         case directive_HEADER:
         case directive_BODY:
         case directive_ASSERT:
         case directive_UNKNOWN:
         default:
            break;
      }

   }


   error = false;
cleanup:
   rest_test_token_del (&token);
   rest_test_token_del (&params[0]);
   rest_test_token_del (&params[1]);
   rest_test_del (&current);

   if (error) {
      for (size_t i=0; ret && ret[i]; i++) {
         rest_test_del (&ret[i]);
      }
      free (ret);
      ret = NULL;
   }

   return ret;
}

