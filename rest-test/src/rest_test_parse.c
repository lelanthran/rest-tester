#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include "ds_str.h"

#include "rest_test_symt.h"
#include "rest_test.h"
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
   for (size_t i=0; i<nprefix; i++) {
      if ((strstr (line, prefixes[i].prefix)) == line) {
         return prefixes[i].directive;
      }
   }

   return directive_UNKNOWN;
}







/* *********************************************************************************
 * Tokenisation functions
 */

enum token_type_t {
   token_NONE,
   token_UNKNOWN,
   token_DIRECTIVE,
   token_STRING,
   token_SYMBOL,
   token_INTEGER,
};

struct token_t {
   enum token_type_t type;
   char *value;
   char *source;
   size_t line_no;
};

static const char *token_type_string (enum token_type_t type)
{
   static const struct {
      enum token_type_t type;
      const char *name;
   } types[] = {
      { token_NONE,          "token_NONE"      },
      { token_UNKNOWN,       "token_UNKNOWN"   },
      { token_DIRECTIVE,     "token_DIRECTIVE" },
      { token_STRING,        "token_STRING"    },
      { token_SYMBOL,        "token_SYMBOL"    },
      { token_INTEGER,       "token_INTEGER"   },
   };
   static const size_t ntypes = sizeof types / sizeof types[0];

   for (size_t i=0; i<ntypes; i++) {
      if (types[i].type == type)
         return types[i].name;
   }

   return "token_type_???";
}

static void token_del (struct token_t **token)
{
   if (!token || !*token)
      return;

   free ((*token)->value);
   free ((*token)->source);
   free ((*token));
   *token = NULL;
}

static struct token_t *token_new (enum token_type_t type,
                                  const char *value,
                                  const char *source,
                                  size_t line_no)
{
   struct token_t *ret = calloc (1, sizeof *ret);
   if (!ret) {
      return NULL;
   }

   ret->type = type;
   ret->line_no = line_no;
   ret->value = ds_str_dup (value);
   ret->source = ds_str_dup (source);

   if (!ret->value || !ret->source) {
      token_del (&ret);
      return NULL;
   }

   return ret;
}

static int readchar (FILE *inf, size_t *line_no)
{
   int ret = fgetc (inf);
   if (ret == '\n')
      (*line_no) = (*line_no) + 1;

   return ret;
}

static int unreadchar (int c, FILE *inf, size_t *line_no)
{
   if (c == '\n')
      (*line_no) = (*line_no) - 1;

   return ungetc (c, inf);
}

static char *read_directive (FILE *inf, size_t *line_no)
{
   char *ret = NULL;
   int c;

   while ((c = readchar (inf, line_no)) != EOF) {
      char tmp[2] = { (char)c, 0 };
      if (c == '.' || c == '-' || c == '_' || isalnum (c)) {
         if (!(ds_str_append (&ret, tmp, NULL))) {
            ERRORF ("Failed to allocate directive\n");
            free (ret);
            return NULL;
         }
      } else {
         unreadchar (c, inf, line_no);
         break;
      }
   }

   return ret;
}

static char *read_string (FILE *inf, size_t *line_no)
{
   bool error = true;
   int c = readchar (inf, line_no); // Swallow the first '"' character
   char *ret = ds_str_dup ("");

   if (!ret) {
      CLEANUP ("Failed to allocate initial string\n");
   }

   while ((c = readchar (inf, line_no)) != EOF) {
      if (c == '"')
         break;

      if (c == '\\') {
         if ((c = readchar (inf, line_no)) == EOF) {
            CLEANUP ("Unexpected EOF after escape character '\\'\n");
         }
      }
      char tmp[2] = { (char)c, 0 };
      if (!(ds_str_append (&ret, tmp, NULL))) {
         CLEANUP ("Failed to allocate string\n");
      }
   }

   if (c != '"') {
      CLEANUP ("Unterminated string\n");
   }

   error = false;
cleanup:
   if (error) {
      free (ret);
      ret = NULL;
   }

   return ret;
}

static bool isodigit (int c)
{
   static const char *odigits = "01234567";
   return strchr (odigits, c) == NULL ? false : true;
}

static char *read_octhex (FILE *inf, size_t *line_no)
{
   bool error = true;
   int c = readchar (inf, line_no);
   char *ret = NULL;
   char tmp[2] = { (char)c, 0 };
   bool ishex = false;

   if (!(ds_str_append (&ret, tmp, NULL))) {
      CLEANUP ("Failed to allocate initial digit for integer\n");
   }
   if ((c = readchar (inf, line_no)) == EOF) {
      return ret;
   }
   if (c  == 'x' || c  == 'X') {
      ishex = true;
   }
   tmp[0] = (char)c;
   if (!(ds_str_append (&ret, tmp, NULL))) {
      CLEANUP ("Failed to allocate second digit for integer\n");
   }

   while ((c = readchar (inf, line_no)) != EOF) {
      if (isspace (c)) {
         break;
      }
      if (ishex && !(isxdigit (c))) {
         CLEANUP ("Unexpected xdigit '%c'\n", c);
      }
      if (!ishex && !(isodigit (c))) {
         CLEANUP ("Unexpected odigit '%c'\n", c);
      }
      tmp[0] = (char)c;
      if (!(ds_str_append (&ret, tmp, NULL))) {
         CLEANUP ("Failed to allocate string for oct-or-hex '%c'\n", c);
      }
   }

   error = false;
cleanup:
   if (error) {
      free (ret);
      ret = NULL;
   }

   return ret;
}

static char *read_integer (FILE *inf, size_t *line_no)
{
   bool error = true;
   char *ret = NULL;
   int c;

   while ((c = readchar (inf, line_no)) != EOF) {
      char tmp[2] = { (char)c, 0 };
      if (isspace (c)) {
         break;
      }
      if (!(isdigit (c))) {
         CLEANUP ("Unexpected digit '%c'\n", c);
      }
      if (!(ds_str_append (&ret, tmp, NULL))) {
         CLEANUP ("Failed to allocate string for integer\n");
      }
   }

   error = false;
cleanup:
   if (error) {
      free (ret);
      ret = NULL;
   }

   return ret;
}

static char *read_symbol (FILE *inf, size_t *line_no)
{
   bool error = true;
   char *ret = NULL;
   int c;

   while ((c = readchar (inf, line_no)) != EOF) {
      char tmp[2] = { (char)c, 0 };
      if (isspace (c)) {
         break;
      }
      if (isalnum (c) || c == '_') {
         if (!(ds_str_append (&ret, tmp, NULL))) {
            CLEANUP ("Failed to allocate string for symbol\n");
         }
      } else {
         CLEANUP ("Unexpected character in symbol '%c'\n", c);
      }
   }

   error = false;
cleanup:
   if (error) {
      free (ret);
      ret = NULL;
   }

   return ret;
}


static struct token_t *token_next (FILE *inf, const char *source, size_t *line_no)
{
   bool error = true;
   int c;
   struct token_t *ret = NULL;
   enum token_type_t type = token_UNKNOWN;
   char *value = NULL;

   while ((c = readchar (inf, line_no)) != EOF) {
      if (isspace (c)) {
         continue;
      }

      unreadchar (c, inf, line_no);

      if (c == '.') {
         type = token_DIRECTIVE;
         if (!(value = read_directive (inf, line_no))) {
            CLEANUP ("[%s:%zu] Failed to read directive", source, *line_no);
         }
         break;
      }

      if (c == '"') {
         type = token_STRING;
         if (!(value = read_string (inf, line_no))) {
            CLEANUP ("[%s:%zu] Failed to read string", source, *line_no);
         }
         break;
      }

      if (c == '0') {
         type = token_INTEGER;
         if (!(value = read_octhex (inf, line_no))) {
            CLEANUP ("[%s:%zu] Failed to read octhex", source, *line_no);
         }
         break;
      }

      if (isdigit (c)) {
         type = token_INTEGER;
         if (!(value = read_integer (inf, line_no))) {
            CLEANUP ("[%s:%zu] Failed to read integer", source, *line_no);
         }
         break;
      }

      if (isalpha (c) || c == '_') {
         type = token_SYMBOL;
         if (!(value = read_symbol (inf, line_no))) {
            CLEANUP ("[%s:%zu] Failed to read symbol", source, *line_no);
         }
         break;
      }

      CLEANUP ("Unexpected character encountered in [%s:%zu]: '%c'\n",
               source, *line_no, c);
   }

   if (ferror (inf)) {
      CLEANUP ("Error reading [%s:%zu]: %m\n", source, *line_no);
   }

   if (value) {
      ret = token_new (type, value, source, *line_no);
   }

   error = false;
cleanup:
   free (value);
   if (error) {
      token_del (&ret);
   }

   return ret;
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
   struct token_t *token = NULL;
   rest_test_t **ret = NULL;
   size_t nitems = 0;
   rest_test_t *current = rest_test_new ("", source, line_no, st);

   while ((token = token_next (inf, source, &line_no))) {
      if (token->type == token_UNKNOWN) {
         CLEANUP ("[%s:%zu] Unknown token type found\n", source, line_no);
      }
      if (token->type == token_NONE) {
         if (!(array_push ((void ***)&ret, &nitems, current))) {
            CLEANUP ("OOM appending to array\n");
         }
         current = NULL;
         break;
      }
      if (token->type != token_DIRECTIVE) {
         CLEANUP ("Expected directive, found token of type '%s'\n",
                  token_type_string (token->type));
      }
      // TODO: dispatch to directive-reader based on directive value
   }


   error = false;
cleanup:
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

