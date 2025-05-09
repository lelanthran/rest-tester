#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "ds_hmap.h"
#include "ds_stack.h"
#include "ds_array.h"
#include "ds_str.h"

#include "rest_test_token.h"
#include "rest_test_symt.h"
#include "rest_test.h"

/* ***************************************************************************
 *
 * # My problems with Hurl
 * 1. Horrible syntax - no delineation between requests and response, and between
 *    requests. No test metadata (names would be nice). This makes the input and the
 *    output slow to visually skim for specific things. A test on a header field in
 *    the response looks exactly the same as sending that header in the request.
 * 2. No persistence between runs, which makes storing state between different Hurl
 *    scripts impossible. Had to use shell-script chicanary to achieve this by
 *    concatenating a generated file with an existing file when doing two-phase OTP
 *    logins.
 * 3. No way to substitute environment variables within a test: more shell abuse
 *    necessary to *generate* "Hurl" files that get later run/concatenated.
 * 4. No easy way to insert results from a shell. More caller-shell-script abuse to
 *    generate Hurl files and concatenate them with other Hurl files.
 * 5. Documentation is, by necessity, a tutorial. The input format is so logically
 *    inconsistent that even ChatGPT makes syntax errors all the time. A simpler
 *    syntax would result in nothing more than a single-page reference being needed.
 *    This is preferable to me than the current Hurl tutorial which is ambiguous.
 *    Possibly the manpage (which is not available online at the Hurl site) is a
 *    better reference but they don't make it available on the site.
 * 6. Having it in Rust makes it hard to script from Python (or anything else; for
 *    now I just wanted Python). It should have been a library, which is used by a
 *    very thin CLI wrapper.
 * 7. Does not report number of failures to the caller. Only options are an error
 *    code being returned, or printing of overly verbose statistics **per test!**
 * 8. No site-configuration and project-configuration possible (no $PWD/hurlrc,
 *    ~/.hurlrc, /etc/hurlrc, etc). Shell-script wrappers needed for each project
 *    because cannot set the project/user/site options.
 *
 * # Proposal
 * 1. Simpler syntax: All lines begin with a period followed by a symbol. This is a
 *    directive to the parser. Only body content is excepted and uses block markers.
 * 2. Three modes: global-mode, request-building and response-checking.
 * 3. When in request-building non-directive lines are added to the request. Content
 *    of these lines can have variable substitution (including environment
 *    variables).
 * 4. When in response-checking mode, lines are checked against the response. These
 *    lines will also be variable-substituted prior to being used.
 * 5. Starts off doing sequential requests, directive can switch to parallel
 *    requests.
 * 6. Cookies automatically stored, response/request headers automatically stored.
 *    This allows a session-id to be sent in the header and subsequently used in
 *    future requests during the same session.
 * 7. Body is indicated using multiline strings ``` on a line by itself to delineate
 *    body. A flag can change this to whatever the user wants.
 * 8. Hurl uses xpath/jsonpath to locate nodes. Need something similar. Maybe just
 *    use an XML and JSON library? A directive to parse JSON and to parse XML (and
 *    fail if not JSON or not XML) combined with a tree/path specification could
 *    work (not sure about JSON arrays, though).
 * 9. Symbol tables must be dynamically scoped - if FOO exists globally but the
 *    specific test sets FOO in the request or the response, then that is the FOO
 *    that will be used in that scope.
 * 10. No need for a specific CAPTURE directive - everything is captured into
 *    scoped symbol tables, including content. Scoping isolates tests from each
 *    other.
 * 11. Easy to insert output of a shell command. Maybe use backticks? Or ${{...}} if
 *     using handlbars for variables.
 *
 *
 *
 * EXPLORATORY NOTES.
 * ---------------------------------------------------------------------------------
 *
 * Handlerbars contain symbols. The value of these symbols are substituted into the
 * request as follows:
 * {{symbol}} => symbol itself is substituted
 * {{symbol(...)}} => builtin function is invoked with the specified parameters.
 * Parameters must be literals or handlerbars themselves (maybe make all symbols
 * within handlebars substituted, as already using quoted strings for strings?).
 *
 * Lines beginning with `.symbol` are directives. Directives can set variable
 * values, like so:
 *    # Mandatory indicator of a new test, defaults to mode request-building
 *    .test "Some Testname goes here"    # Name must be unique within a file
 *    .method "POST"
 *    .request-URI "{{uriBase}}/somePath"
 *    .version HTTP/1.1
 *
 *
 *    # Set global symbol
 *    .global Date ${{date +"%s"}}
 *    # Set local symbol
 *    .local SessionID "12345"
 *
 *    # Set some headers
 *    .header X-App-Date {{Date}}
 *    .header X-App-Session-Id {{SessionID}}
 *
 *    # Set the content (all content for a request is concatenated)
 *    ```
 *    Some Content Goes Here
 *    ```
 *
 *    # Switch between request/response/global modes (Alternating allowed)
 *    .switch-mode response-checking
 *
 *    # Test a response
 *    .assert Content-type == "application/json"
 *    .assert HTTP == "200"
 *    .assert BODY contains "Some content stuff"
 *    .assert BODY starts-with "Requested resource is"
 *
 * }
 *
 * Multiline strings must be available.
 *
 *
 * IMPLEMENTATION NOTES
 * ---------------------------------------------------------------------------------
 *
 * 1. Symbol tables, serialisable to/from files, is necessary.
 * 2. Line-by-line reading with a dispatch for directives. The only lines without
 *    a directive are within a multiline BODY block.
 * 3. Like Hurl, there will be a two-phase execution:
 *    1. Parse the input into an array of data-structures, including filenames and
 *       line-numbers.
 *    2. Execute each data-structure in that array (execute request, check response)
 * 4. Dump the data-structures in human-readable form. The test names must be
 *    prefixed with a filename (or filesystem path).
 */

// Store headers. Essentially just a key/value pair and a source (file + line)
struct header_t {
   char     *source;
   size_t   line_no;
   char     *name;
   char     *value;
};

// Store the request information
struct req_t {
   int                lasterr;
   rest_test_token_t *method;
   rest_test_token_t *uri;
   rest_test_token_t *http_version;
   rest_test_token_t *body;
   ds_hmap_t         *headers;    // struct header_t *
};

// Store the response information
struct rsp_t {
   int          lasterr;
   char        *http_version;
   char        *status_code;
   char        *reason;
   char        *body;
   ds_hmap_t   *headers;  // struct header_t *
};

// Store each assertion. Assertions are stored as a stack of operators and operands
// to allow logical booleans in the assertion expressions. This makes it easy to
// support multiple expressions with logical boolean conjunctions, and to support
// nested expressions. When the stack of operator + operands is constructed
// correctly (i.e. postfix notation) then evaluating an expression becomes simple.
struct assertion_t {
   char        *source;
   size_t       line_no;
   ds_stack_t  *stack;
};

// The test record.
struct rest_test_t {
   // Caller need not check for failure after every access
   int lasterr;

   // Symbol table
   rest_test_symt_t  *st;

   // Identify the test
   char  *fname;
   size_t line_no;
   char  *name;

   // The request and response data; note that all responses are stored in memory
   struct req_t req;
   struct rsp_t rsp;

   // The assertions
   ds_array_t *assertions;    // struct assertion_t *
};




/* *********************************************************************************
 * Helpers...
 */

#define CLEANUP(...) \
do {\
   ERRORF(__VA_ARGS__);\
   goto cleanup;\
} while (0)

#define TOLOWER(s)   \
for (size_t i=0; s[i]; i++) {\
   s[i] = (unsigned char)tolower(s[i]);\
}

#define SET_TOKEN_FIELD(obj,field,value)  \
do {\
   if (!obj || obj->lasterr)\
      return false;\
   rest_test_token_t *tmp = rest_test_token_dup (value);\
   if (!tmp) {\
      obj->lasterr = -1;\
      return false;\
   }\
   rest_test_token_del (&obj->field);\
   obj->field = tmp;\
   return true;\
} while (0)

#define SET_STRING_FIELD(obj,field,value)  \
do {\
   if (!obj || obj->lasterr)\
      return false;\
   char *tmp = value ? ds_str_dup (value) : ds_str_dup ("");\
   if (!tmp) {\
      obj->lasterr = -1;\
      return false;\
   }\
   free (obj->field);\
   obj->field = tmp;\
   return true;\
} while (0)


/* *********************************************************************************
 * Header functions.
 */

static void header_del (struct header_t **h)
{
   if (!h || !*h)
      return;
   free ((*h)->source);
   free ((*h)->name);
   free ((*h)->value);
   free (*h);
   *h = NULL;
}

static void _header_del (const void *key, size_t keylen,
                         void *header, size_t headerlen,
                         void *hmap)
{
   struct header_t *h = header;
   const char *k = key;
   ds_hmap_t *hm = hmap;
   (void)headerlen;
   header_del (&h);
   ds_hmap_remove (hm, k, keylen);
}

static void _header_print (const void *key, size_t keylen,
                           void *header, size_t headerlen,
                           void *outf)
{
   struct header_t *h = header;
   const char *k = key;
   FILE *f = outf;
   (void)headerlen;
   (void)keylen;
   fprintf (f, "  [%s:%zu] [%s] [%s]\n", h->source, h->line_no, k, h->value);
}


static struct header_t *header_new (const char *source,
                                    size_t line_no,
                                    const char *line)
{
   bool error = true;
   struct header_t *ret = calloc (1, sizeof *ret);
   char *copy = ds_str_dup (line);

   if (!ret || !copy)
      CLEANUP ("[%s:%zu %s] OOM allocating header object\n", source, line_no, line);

   if (!(ret->source = ds_str_dup (source)))
      CLEANUP ("[%s:%zu %s] OOM allocating source\n", source, line_no, line);

   char *delim = strchr (copy, ':');
   if (!delim)
      CLEANUP ("[%s:%zu %s] Invalid header, missing `:`\n", source, line_no, line);

   *delim++ = 0;
   ret->line_no = line_no;
   ret->name = ds_str_dup (copy);
   ret->value = ds_str_dup (delim);
   if (!ret->name || !ret->value)
      CLEANUP ("[%s:%zu %s] OOM copying header values", source, line_no, line);

   ds_str_trim (ret->name);
   TOLOWER (ret->name);

   ds_str_trim (ret->value);

   error = false;

cleanup:
   free (copy);
   if (error) {
      header_del (&ret);
   }

   return ret;
}




/* *********************************************************************************
 * Request functions.
 */

static void req_clear (struct req_t *req)
{
   rest_test_token_del (&req->method);
   rest_test_token_del (&req->uri);
   rest_test_token_del (&req->http_version);
   rest_test_token_del (&req->body);

   ds_hmap_iterate (req->headers, _header_del, req->headers);
   ds_hmap_del (req->headers);
   req->headers = NULL;

   memset (req, 0, sizeof *req);
}

static bool req_method (struct req_t *req, const rest_test_token_t *method)
{
   SET_TOKEN_FIELD (req, method, method);
}

static bool req_uri (struct req_t *req, const rest_test_token_t *uri)
{
   SET_TOKEN_FIELD (req, uri, uri);
}

static bool req_http_version (struct req_t *req, const rest_test_token_t *http_version)
{
   SET_TOKEN_FIELD (req, http_version, http_version);
}

static bool req_body (struct req_t *req, const rest_test_token_t *body)
{
   SET_TOKEN_FIELD (req, body, body);
}

static bool req_body_append (struct req_t *req, const rest_test_token_t *body)
{
   if (!req || req->lasterr)
      return false;

   return req->body
      ? rest_test_token_append (req->body, body)
      : req_body (req, body);
}





/* *********************************************************************************
 * Response functions.
 */

static void rsp_clear (struct rsp_t *rsp)
{
   free (rsp->http_version);
   free (rsp->status_code);
   free (rsp->reason);
   free (rsp->body);

   ds_hmap_iterate (rsp->headers, _header_del, rsp->headers);
   ds_hmap_del (rsp->headers);
   rsp->headers = NULL;

   memset (rsp, 0, sizeof *rsp);
}

static bool rsp_http_version (struct rsp_t *rsp, const char *http_version)
{
   SET_STRING_FIELD (rsp, http_version, http_version);
}

static bool rsp_status_code (struct rsp_t *rsp, const char *status_code)
{
   SET_STRING_FIELD (rsp, status_code, status_code);
}

static bool rsp_reason (struct rsp_t *rsp, const char *reason)
{
   SET_STRING_FIELD (rsp, reason, reason);
}

static bool rsp_body (struct rsp_t *rsp, const char *body)
{
   SET_STRING_FIELD (rsp, body, body);
}

static bool rsp_body_append (struct rsp_t *rsp, const char *body)
{
   if (!rsp || rsp->lasterr)
      return false;

   size_t param_len = strlen (body) + 1;
   size_t body_len = rsp->body ? strlen (rsp->body) + 1 : 0;
   char *tmp = malloc (param_len + body_len);
   if (!tmp) {
      rsp->lasterr = -1;
      return false;
   }
   *tmp = 0;

   strcpy (tmp, rsp->body ? rsp->body : "");
   strcat (tmp, body);
   free (rsp->body);
   rsp->body = tmp;
   return true;
}





/* *********************************************************************************
 * Assertion functions.
 */






/* *********************************************************************************
 * Public functions.
 */

#define TEST_RT_STRING(rt)  \
if (!rt || rt->lasterr) return ""

#define TEST_RT_BOOL(rt)  \
if (!rt || rt->lasterr) return false

rest_test_t *rest_test_new (const char *name,
                            const char *fname,
                            size_t line_no,
                            rest_test_symt_t *parent)
{
   bool error = true;

   rest_test_t *ret = calloc (1, sizeof *ret);
   if (!ret)
      CLEANUP ("OOM error allocating rest_test_t structure\n");

   ret->st = rest_test_symt_new (name, parent, 32);
   ret->name = ds_str_dup (name);
   ret->fname = ds_str_dup (fname);
   ret->line_no = line_no;
   ret->req.headers = ds_hmap_new (32);
   ret->rsp.headers = ds_hmap_new (32);
   ret->assertions = ds_array_new ();

   if (!ret->st || !ret->name || !ret->fname || ! ret->req.headers
         || !ret->rsp.headers || !ret->assertions) {
      CLEANUP ("Failed to initialise test structure\n");
   }

   error = false;

cleanup:
   if (error) {
      rest_test_del (&ret);
   }

   return ret;
}

void rest_test_del (rest_test_t **rt)
{
   if (!rt || !*rt)
      return;

   rest_test_symt_del (&(*rt)->st);
   free ((*rt)->name);
   free ((*rt)->fname);
   // ds_array_iterate ((*rt)->assertions, _assertion_del, (*rt)->assertions);
   ds_array_del ((*rt)->assertions);

   req_clear (&(*rt)->req);
   rsp_clear (&(*rt)->rsp);

   free (*rt);
}

void rest_test_dump (rest_test_t *rt, FILE *outf)
{
   if (!outf)
      outf = stdout;
   if (!rt) {
      fprintf (outf, "NULL test");
      return;
   }
   fprintf (outf, "Source:                [%s:%zu]\n", rt->fname, rt->line_no);
   fprintf (outf, "Test:                  [%s]\n", rt->name);
   fprintf (outf, "Last error:            [%i]\n", rt->lasterr);
   fprintf (outf, "Req->method:           [%s]\n", rest_test_token_value (rt->req.method));
   fprintf (outf, "Req->uri:              [%s]\n", rest_test_token_value (rt->req.uri));
   fprintf (outf, "Req->http_version:     [%s]\n", rest_test_token_value (rt->req.http_version));
   fprintf (outf, "Req->body:             [%s]\n", rest_test_token_value (rt->req.body));
   ds_hmap_iterate (rt->req.headers, _header_print, outf);
   fprintf (outf, "Rsp->http_version:     [%s]\n", rt->rsp.http_version);
   fprintf (outf, "Rsp->status_code:      [%s]\n", rt->rsp.status_code);
   fprintf (outf, "Rsp->reason:           [%s]\n", rt->rsp.reason);
   fprintf (outf, "Rsp->body:             [%s]\n", rt->rsp.body);
   ds_hmap_iterate (rt->rsp.headers, _header_print, outf);
   rest_test_symt_dump (rt->st, outf);
}

// Get the last error value
int rest_test_lasterr (rest_test_t *rt)
{
   return rt ? rt->lasterr : -2;
}

rest_test_symt_t *rest_test_symt (rest_test_t *rt)
{
   return rt ? rt->st : NULL;
}

const char *rest_test_set_name (rest_test_t *rt, const char *name)
{
   TEST_RT_BOOL(rt);
   char *tmp = ds_str_dup (name);
   if (!tmp)
      return false;

   free (rt->name);
   rt->name = tmp;
   return rest_test_symt_set_name (rt->st, tmp);
}

const char *rest_test_set_fname (rest_test_t *rt, const char *fname)
{
   TEST_RT_BOOL(rt);
   char *tmp = ds_str_dup (fname);
   if (!tmp)
      return false;

   free (rt->fname);
   rt->fname = tmp;
   return tmp;
}

size_t rest_test_set_line_no (rest_test_t *rt, size_t line_no)
{
   TEST_RT_BOOL(rt);
   rt->line_no = line_no;
   return line_no;
}

const char *rest_test_get_name (rest_test_t *rt)
{
   return rt ? rt->name : NULL;
}

const char *rest_test_get_fname (rest_test_t *rt)
{
   return rt ? rt->fname : NULL;
}

size_t rest_test_get_line_no (rest_test_t *rt)
{
   return rt ? rt->line_no : (size_t)-1;
}


// Set all the fields in the request
bool rest_test_req_set_method (rest_test_t *rt, const rest_test_token_t *method)
{
   TEST_RT_BOOL(rt);
   return req_method (&rt->req, method);
}

bool rest_test_req_set_uri (rest_test_t *rt, const rest_test_token_t *uri)
{
   TEST_RT_BOOL(rt);
   return req_uri (&rt->req, uri);
}

bool rest_test_req_set_http_version (rest_test_t *rt, const rest_test_token_t *http_version)
{
   TEST_RT_BOOL(rt);
   return req_http_version (&rt->req, http_version);
}

bool rest_test_req_set_body (rest_test_t *rt, const rest_test_token_t *body)
{
   TEST_RT_BOOL(rt);
   if (!(req_body (&rt->req, body))) {
      rt->lasterr = -7;
      return false;
   }
   return true;
}

bool rest_test_req_append_body (rest_test_t *rt, const rest_test_token_t *body)
{
   TEST_RT_BOOL(rt);
   if (!(req_body_append (&rt->req, body))) {
      rt->lasterr = -8;
      return false;
   }

   return true;
}

bool rest_test_req_set_header (rest_test_t *rt,
                               const char *source, size_t line_no,
                               const char *value)
{
   TEST_RT_BOOL(rt);
   struct header_t *h = header_new (source, line_no, value);
   if (!h) {
      rt->lasterr = -5;
      return false;
   }

   return ds_hmap_set_str_ptr (rt->req.headers, h->name, h);
}

// Get all the fields in the request
const char *rest_test_req_method (rest_test_t *rt)
{
   TEST_RT_STRING(rt);
   return rest_test_token_value (rt->req.method);
}

const char *rest_test_req_uri (rest_test_t *rt)
{
   TEST_RT_STRING(rt);
   return rest_test_token_value (rt->req.uri);
}

const char *rest_test_req_http_version (rest_test_t *rt)
{
   TEST_RT_STRING(rt);
   return rest_test_token_value (rt->req.http_version);
}

const char *rest_test_req_body (rest_test_t *rt)
{
   TEST_RT_STRING(rt);
   return rest_test_token_value (rt->req.body);
}

const char *rest_test_req_header (rest_test_t *rt, const char *header)
{
   TEST_RT_STRING(rt);
   char *value = NULL;
   if (!(ds_hmap_get_str_str(rt->req.headers, header, &value))) {
      rt->lasterr = -4;
      return "";
   }
   return value;
}


// Set all the fields in the response
bool rest_test_rsp_set_http_version (rest_test_t *rt, const char *http_version)
{
   TEST_RT_BOOL(rt);
   return rsp_http_version (&rt->rsp, http_version);
}

bool rest_test_rsp_set_status_code (rest_test_t *rt, const char *status_code)
{
   TEST_RT_BOOL(rt);
   return rsp_status_code (&rt->rsp, status_code);
}

bool rest_test_rsp_set_reason (rest_test_t *rt, const char *reason)
{
   TEST_RT_BOOL(rt);
   return rsp_reason (&rt->rsp, reason);
}

bool rest_test_rsp_set_body (rest_test_t *rt, const char *body)
{
   TEST_RT_BOOL(rt);
   if (!(rsp_body (&rt->rsp, body))) {
      rt->lasterr = -9;
      return false;
   }
   return true;
}

bool rest_test_rsp_append_body (rest_test_t *rt, const char *body)
{
   TEST_RT_BOOL(rt);
   if (!(rsp_body_append (&rt->rsp, body))) {
      rt->lasterr = -10;
      return false;
   }
   return true;
}

bool rest_test_rsp_set_header (rest_test_t *rt,
                               const char *source, size_t line_no,
                               const char *value)
{
   TEST_RT_BOOL(rt);
   struct header_t *h = header_new (source, line_no, value);
   if (!h) {
      rt->lasterr = -5;
      return false;
   }

   return ds_hmap_set_str_ptr (rt->rsp.headers, h->name, h);
}

// Get all the fields in the response
const char *rest_test_rsp_http_version (rest_test_t *rt)
{
   TEST_RT_STRING(rt);
   return rt->rsp.http_version;
}

const char *rest_test_rsp_status_code (rest_test_t *rt)
{
   TEST_RT_STRING(rt);
   return rt->rsp.status_code;
}

const char *rest_test_rsp_reason (rest_test_t *rt)
{
   TEST_RT_STRING(rt);
   return rt->rsp.reason;
}

const char *rest_test_rsp_body (rest_test_t *rt)
{
   TEST_RT_STRING(rt);
   return rt->rsp.body;
}

const char *rest_test_rsp_header (rest_test_t *rt, const char *header)
{
   TEST_RT_STRING(rt);
   char *value = NULL;
   if (!(ds_hmap_get_str_str(rt->rsp.headers, header, &value))) {
      rt->lasterr = -4;
      return "";
   }
   return value;
}


static bool eval (rest_test_token_t *token, rest_test_symt_t *st)
{
   const rest_test_token_t *target = NULL;
   const char *newvalue = NULL;
   if (!token)
      return true;

   switch (rest_test_token_type (token)) {
      case token_NONE:
      case token_UNKNOWN:
      case token_DIRECTIVE:
      case token_INTEGER:
      case token_ASSERT_END:
         return true;

      case token_SHELLCMD:
      case token_STRING:
         // TODO: Perform interpolation
         break;

      case token_SYMBOL:
         // TODO: Perform substitution
         if (!(target = rest_test_symt_value (st, rest_test_token_value (token)))) {
            ERRORF ("[%s:%zu] Variable [%s] is not defined.\n",
                  rest_test_token_source (token),
                  rest_test_token_line_no (token),
                  rest_test_token_value (token));
            return false;
         }
         if (!(newvalue = rest_test_token_value (target))) {
            ERRORF ("[%s:%zu] Internal error retrieving value for [%s] (possibly defined as NULL).\n",
                  rest_test_token_source (token),
                  rest_test_token_line_no (token),
                  rest_test_token_value (token));
            return false;
         }
         if (!(rest_test_token_set_value (token, newvalue))) {
            ERRORF ("[%s:%zu] Failed to substitute value for variable [%s] (Possible OOM error).\n",
                  rest_test_token_source (token),
                  rest_test_token_line_no (token),
                  rest_test_token_value (token));
            return false;
         }


         break;
   }

   return true;
}

bool rest_test_eval_req (rest_test_t *rt, rest_test_token_t **errtoken)
{
   bool error = true;
   TEST_RT_STRING(rt);
   rest_test_token_t *et = NULL;

   if (!(eval(rt->req.method, rt->st))) {
      et = rt->req.method;
      CLEANUP ("[%s:%zu] Failed to perform evaluation on method [%s]\n",
               rest_test_token_source (rt->req.method),
               rest_test_token_line_no (rt->req.method),
               rest_test_token_value (rt->req.method));
   }

   if (!(eval(rt->req.uri, rt->st))) {
      et = rt->req.uri;
      CLEANUP ("[%s:%zu] Failed to perform evaluation on uri [%s]\n",
               rest_test_token_source (rt->req.uri),
               rest_test_token_line_no (rt->req.uri),
               rest_test_token_value (rt->req.uri));
   }

   if (!(eval(rt->req.http_version, rt->st))) {
      et = rt->req.http_version;
      CLEANUP ("[%s:%zu] Failed to perform evaluation on http_version [%s]\n",
               rest_test_token_source (rt->req.http_version),
               rest_test_token_line_no (rt->req.http_version),
               rest_test_token_value (rt->req.http_version));
   }

   if (!(eval(rt->req.body, rt->st))) {
      et = rt->req.body;
      CLEANUP ("[%s:%zu] Failed to perform evaluation on body [%s]\n",
               rest_test_token_source (rt->req.body),
               rest_test_token_line_no (rt->req.body),
               rest_test_token_value (rt->req.body));
   }

   error = false;
cleanup:
   if (errtoken) {
      *errtoken = et;
   }


   return !error;
}


