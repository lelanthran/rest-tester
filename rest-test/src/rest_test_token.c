#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "ds_str.h"

#include "rest_test_token.h"
#include "rest_test_symt.h"
#include "rest_test.h"


#define CLEANUP(...) \
do {\
   ERRORF(__VA_ARGS__);\
   goto cleanup;\
} while (0)


struct rest_test_token_t {
   enum rest_test_token_type_t type;
   char *value;
   char *source;
   size_t line_no;
};

static const struct {
   enum rest_test_token_type_t type;
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

const char *rest_test_token_type_string (enum rest_test_token_type_t type)
{
   for (size_t i=0; i<ntypes; i++) {
      if (types[i].type == type)
         return types[i].name;
   }

   return "token_type_???";
}

enum rest_test_token_type_t rest_test_token_type_type (const char *s)
{
   for (size_t i=0; i<ntypes; i++) {
      if ((strcmp (s, types[i].name)) == 0) {
         return types[i].type;
      }
   }
   return token_UNKNOWN;
}



void rest_test_token_del (rest_test_token_t **token)
{
   if (!token || !*token)
      return;

   free ((*token)->value);
   free ((*token)->source);
   free ((*token));
   *token = NULL;
}

rest_test_token_t *rest_test_token_new (enum rest_test_token_type_t type,
                                        const char *value,
                                        const char *source,
                                        size_t line_no)
{
   rest_test_token_t *ret = calloc (1, sizeof *ret);
   if (!ret) {
      return NULL;
   }

   ret->type = type;
   ret->line_no = line_no;
   ret->value = ds_str_dup (value);
   ret->source = ds_str_dup (source);

   if (!ret->value || !ret->source) {
      rest_test_token_del (&ret);
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
   int delim = readchar (inf, line_no); // Swallow the first '"' or '\'' character
   char *ret = ds_str_dup ("");

   if (!ret) {
      CLEANUP ("Failed to allocate initial string\n");
   }

   int c;
   while ((c = readchar (inf, line_no)) != EOF) {
      if (c == delim)
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

   if (c != delim) {
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


rest_test_token_t *rest_test_token_next (FILE *inf,
                                         const char *source,
                                         size_t *line_no)
{
   bool error = true;
   int c;
   rest_test_token_t *ret = NULL;
   enum rest_test_token_type_t type = token_UNKNOWN;
   char *value = NULL;

   while ((c = readchar (inf, line_no)) != EOF) {
      if (isspace (c)) {
         continue;
      }

      unreadchar (c, inf, line_no);

      if (c == '#') {
         do {
            c = readchar (inf, line_no);
         } while (c != EOF && c != '\n');
         continue;
      }


      if (c == ';') {
         char tmp[2] = { ';', 0 };
         type = token_ASSERT_END;
         if (!(value = ds_str_dup (tmp))) {
            CLEANUP ("[%s:%zu] Failed to read assert-end\n", source, *line_no);
         }

      }

      if (c == '.') {
         type = token_DIRECTIVE;
         if (!(value = read_directive (inf, line_no))) {
            CLEANUP ("[%s:%zu] Failed to read directive\n", source, *line_no);
         }
         break;
      }

      if (c == '\'') {
         type = token_STRING;
         if (!(value = read_string (inf, line_no))) {
            CLEANUP ("[%s:%zu] Failed to read string\n", source, *line_no);
         }
         ERRORF ("Returning [%s]\n", value);
         break;
      }

      if (c == '"' || c =='`') {
         int delim = c;
         value = NULL;
         type = delim == '"' ? token_STRING : token_SHELLCMD;
         while (c == delim) {
            char *tmp = read_string (inf, line_no);
            if (!tmp) {
               CLEANUP ("[%s:%zu] Failed to read string\n", source, *line_no);
            }
            if (!(ds_str_append (&value, tmp, NULL))) {
               free (tmp);
               CLEANUP ("[%s:%zu] Failed to append string\n", source, *line_no);
            }
            free (tmp);
            while ((c = readchar (inf, line_no)) != EOF) {
               if (isspace (c)) {
                  continue;
               }
               break;
            }
            unreadchar (c, inf, line_no);
         }
         ERRORF ("Returning [%s]\n", value);
         break;
      }

      if (c == '0') {
         type = token_INTEGER;
         if (!(value = read_octhex (inf, line_no))) {
            CLEANUP ("[%s:%zu] Failed to read octhex\n", source, *line_no);
         }
         break;
      }

      if (isdigit (c)) {
         type = token_INTEGER;
         if (!(value = read_integer (inf, line_no))) {
            CLEANUP ("[%s:%zu] Failed to read integer\n", source, *line_no);
         }
         break;
      }

      if (isalpha (c) || c == '_') {
         type = token_SYMBOL;
         if (!(value = read_symbol (inf, line_no))) {
            CLEANUP ("[%s:%zu] Failed to read symbol\n", source, *line_no);
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
      ret = rest_test_token_new (type, value, source, *line_no);
   }

   error = false;
cleanup:
   free (value);
   if (error) {
      rest_test_token_del (&ret);
   }

   return ret;
}


enum rest_test_token_type_t rest_test_token_type (rest_test_token_t *token)
{
   return token ? token->type : token_NONE;
}

const char *rest_test_token_value (rest_test_token_t *token)
{
   return token ? token->value : NULL;
}

const char *rest_test_token_source (rest_test_token_t *token)
{
   return token ? token->source : NULL;
}

size_t rest_test_token_line_no (rest_test_token_t *token)
{
   return token ? token->line_no : (size_t)-1;
}




