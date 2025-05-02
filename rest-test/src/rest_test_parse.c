#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "rest_test_symt.h"
#include "rest_test.h"
#include "rest_test_parse.h"


#define MAX_INPUT_LINE_LENGTH       (1024 * 1024)  // 1MB should be enough

#define CLEANUP(...) \
do {\
   ERRORF(__VA_ARGS__);\
   goto cleanup;\
} while (0)

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
   directive_ASSERT,
};
struct prefix_t {
   const char *prefix;
   enum directive_t directive;
};

static enum directive_t get_directive (const char *line)
{
   static const struct prefix_t prefixes[] = {
      { ".global",         directive_GLOBAL        },
      { ".parent",         directive_PARENT        },
      { ".local",          directive_LOCAL         },

      { ".test",           directive_TEST          },
      { ".method",         directive_METHOD        },
      { ".uri",            directive_URI           },
      { ".http_version",   directive_HTTP_VERSION  },
      { ".header",         directive_HEADER        },
      { ".assert",         directive_ASSERT        },
   };

   static size_t nprefix = sizeof prefixes/sizeof prefixes[0];
   for (size_t i=0; i<nprefixes; i++) {
      if ((strstr (line, prefixes[i].prefix)) == line) {
         return prefixes[i].directive;
      }
   }

   return directive_UNKNOWN;
}

rest_test_t **rest_test_parse_stream (rest_test_symt_t *st, FILE *inf, const char *source)
{
   bool error = true;
   size_t line_no = 0;
   rest_test_t **ret = NULL;
   char **tokens = NULL;

   rest_test_t *current = rest_test_new("UNSET", source, line_no, st);
   char *input = calloc (1, MAX_INPUT_LINE_LENGTH);

   if (!input || !current)
      CLEANUP ("Failed to allocate temporary storage for input lines\n");

   while (fgets (input, MAX_INPUT_LINE_LENGTH - 1, inf)) {
      // Update the line number
      line_no++;

      // Ensure that the line was not truncated while reading
      size_t input_len = strlen (input);
      if (input[input_len - 1] != '\n') {
         CLEANUP ("[%s:%zu] Maximum line length (%zuKB) exceeded\n",
                  source, line_no, input_len / 1024);
      }

      // Tokenise the input
      for (size_t i=0; tokens && tokens[i]; i++) {
         free (tokens[i]);
      }
      free (tokens);
      tokens = tokenise (input, &line_no);
      if (!tokens)
         continue;

      // Dispatch the correct function for this input
      bool dispatched = false;
      enum directive_t directive = get_directive (tokens[0]);
      switch (directive) {
         case directive_GLOBAL:
            dispatched = rest_test_symt_add_root (st, tokens[1], tokens[2]);
            break;

         case directive_PARENT:
            dispatched = rest_test_symt_add_parent (current, tokens[1], tokens[2]);
            break;

         case directive_LOCAL:
            dispatched = rest_test_symt_add (current, tokens[1], tokens[2]);
            break;

         case directive_TEST:
            // TODO: Save current, create new current
            break;

         case directive_METHOD:
            dispatched = rest_test_req_set_method (current, tokens[1]);
            break;

         case directive_URI:
            dispatched = rest_test_req_set_uri (current, tokens[1]);
            break;

         case directive_HTTP_VERSION:
            dispatched = rest_test_req_set_http_version (current, tokens[1]);
            break;

         case directive_HEADER:
            dispatched = rest_test_req_set_header (current, tokens[1]);
            break;

         case directive_ASSERT:
            dispatched = rest_test_assert (current, tokens[1]);
            break;

         case directive_UNKNOWN:
         default:

      }
   }

   error = false;

cleanup:
   rest_test_del (&current);
   free (input);

   if (error) {
      for (size_t i=0; ret && ret[i]; i++) {
         rest_test_del (&ret[i]);
      }
      free (ret);
      ret = NULL;
   }

   return ret;
}

