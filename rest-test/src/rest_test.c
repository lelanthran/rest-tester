
#include "rest_test.h"

/* ***************************************************************************
 *
 * # My problems with Hurl
 * 1. Horrible syntax - no delineation between requests and response, between
 *    requests. No test metadata (names would be nice). This makes the input and the
 *    output slow to visually skim for specific things. A test on a header field  in
 *    the response looks exactly the same as sending that header in the request.
 * 2. No persistence between runs, which makes storing state between different Hurl
 *    scripts impossible. Had to use shell-script chicanary to achieve this by
 *    concatenating a generated file with an existing file when doing two-phase OTP
 *    logins.
 * 3. No way to substitute environment variables within a test: more shell abuse
 *    necessary to *generate* "Hurl" files that get later run/concatenated.
 * 4. No easy way to insert results from a shell. More caller-shell-script abuse to
 *    generate Hurl files and concatenate them with other Hurl files.
 * 4. Documentation is, by necessity, a tutorial. The input format is so logically
 *    inconsistent that even ChatGPT makes syntax errors all the time. A simpler
 *    syntax would result in nothing more than a single-page reference being needed.
 * 5. Having it in Rust makes it hard to script from Python (or anything else; for
 *    now I just wanted Python). It should have been a library, which is used by a
 *    very thin CLI wrapper.
 * 6. Does not report number of failures to the caller. Only options are an error
 *    code being returned, or printing of overly verbose statistics.
 * 7. No site-configuration and project-configuration possible (no $PWD/hurlrc,
 *    ~/.hurlrc, /etc/hurlrc, etc).
 *
 * # Proposal
 * 1. Simpler syntax: All lines begin with a period followed by a symbol. This is a
 *    directive to the parser. Only body content is excepted and uses block markers.
 * 2. Two modes: request-building and response-checking.
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
 * 7. Body is indicated using multiline strings ``` on a line by itself to both
 *    start and stop body. A flag can change this to whatever the user wants.
 * 8. Hurl uses xpath/jsonpath to locate nodes. Need something similar. Maybe just
 *    use an XML and JSON library?
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
 * {{symbol(...)}} => a function is invoked with the specified parameters.
 * Parameters must be literals or handlerbars themselves.
 *
 * Lines beginning with `.symbol` are directives. Directives can set variable
 * values, like so:
 *    # Mandatory indicator of a new test, defaults to mode request-building
 *    .test "Some Testname goes here"    # Name must be unique within a file
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
 *    .assert HTTP 200
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


