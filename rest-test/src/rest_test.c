#include <stdio.h>
#include <stdlib.h>

#include "ds_hmap.h"
#include "ds_stack.h"
#include "ds_array.h"
#include "ds_str.h"

#include "rest_test.h"
#include "rest_test_symt.h"

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
   char        *method;
   char        *request_uri;
   char        *http_version;
   char        *body;
   ds_hmap_t   *rqst_headers;    // struct header_t *
};

// Store the response information
struct rsp_t {
   char        *http_version;
   char        *status_code;
   char        *reason;
   char        *body;
   ds_hmap_t   *rsp_headers;  // struct header_t *
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
   // Symbol table
   rest_test_symt_t  *st;

   // Identify the test
   char  *fname;
   char  *id;

   // The request and response data; note that all responses are stored in memory
   struct req_t req;
   struct rsp_t rsp;

   // The assertions
   ds_array_t *assertions;    // struct assertion_t *
};


#define CLEANUP(...)      do {\
   ERRORF(__VA_ARGS__);\
   goto cleanup;\
} while (0)

/* *********************************************************************************
 * Header functions.
 */

void header_del (struct header_t **h)
{
   if (!h || !*h)
      return;
   free ((*h)->source);
   free ((*h)->name);
   free ((*h)->value);
   free (*h);
   *h = NULL;
}

struct header_t *header_new (const char *source,
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
   ret->name = ds_str_dup (copy);
   ret->value = ds_str_dup (delim);
   if (!ret->name || !ret->value)
      CLEANUP ("[%s:%zu %s] OOM copying header values", source, line_no, line);

   ds_str_trim (ret->name);
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

/* *********************************************************************************
 * Response functions.
 */

/* *********************************************************************************
 * Assertion functions.
 */

