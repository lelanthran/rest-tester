
#include "rest_test.h"

/* ***************************************************************************
 * Some notes.
 * Each file composed of one or more request. Each request starts with a single
 * HTTP verb, followed by the path followed by HTTP/1.1.
 * Thereafter headers follow. All values within handlebars are substituted with
 * a variable (if it exists).
 * All newlines are converted to crlf. After the headers (2x crlf) everything found
 * is used as the body. The body ends either at EOF or at the next request.
 *
 * Handlerbars contain symbols. The value of these symbols are substituted into the
 * request as follows:
 * {{symbol}} => symbol itself is substituted
 * {{symbol(...)}} => a function is invoked with the specified parameters.
 * Parameters must be literals or handlerbars themselves.
 *
 * Lines beginning with `[symbol]` are directives. Directives can set variable
 * values, like so:
 *    [NAME Some name goes here]
 * or can start a new section (for example ASSERT)
 *    
 * }
 *
