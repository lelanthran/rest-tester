
#ifndef H_REST_TEST
#define H_REST_TEST

typedef struct rest_test_t rest_test_t;

#define ERRORF(...)      do {\
   fprintf (stderr, "%s:%04i:", __FILE__, __LINE__);\
   fprintf (stderr, __VA_ARGS__);\
} while (0)

#ifdef __cplusplus
extern "C" {
#endif

   // Create and delete a single test
   rest_test_t *rest_test_new (const char *name,
                               const char *fname,
                               size_t line_no,
                               rest_test_symt_t *parent);
   void rest_test_del (rest_test_t **rt);
   void rest_test_dump (rest_test_t *rt, FILE *fout);

   // Get the last error value
   int rest_test_lasterr (rest_test_t *rt);

   // Get the symbol table
   rest_test_symt_t *rest_test_symt (rest_test_t *rt);

   // Set the name, the filename and the line number
   const char *rest_test_set_name (rest_test_t *rt, const char *name);
   const char *rest_test_set_fname (rest_test_t *rt, const char *fname);
   size_t rest_test_set_line_no (rest_test_t *rt, size_t line_no);
   // Get the name, filename and line number
   const char *rest_test_get_name (rest_test_t *rt);
   const char *rest_test_get_fname (rest_test_t *rt);
   size_t rest_test_get_line_no (rest_test_t *rt);

   // Set all the fields in the request
   bool rest_test_req_set_method (rest_test_t *rt, const rest_test_token_t *method);
   bool rest_test_req_set_uri (rest_test_t *rt, const rest_test_token_t *uri);
   bool rest_test_req_set_http_version (rest_test_t *rt, const rest_test_token_t *http_version);
   bool rest_test_req_set_body (rest_test_t *rt, const rest_test_token_t *body);
   bool rest_test_req_append_body (rest_test_t *rt, const rest_test_token_t *body);
   bool rest_test_req_set_header (rest_test_t *rt,
                                  const char *source, size_t line_no,
                                  const char *value);

   // Get all the fields in the request
   const char *rest_test_req_method (rest_test_t *rt);
   const char *rest_test_req_uri (rest_test_t *rt);
   const char *rest_test_req_http_version (rest_test_t *rt);
   const char *rest_test_req_body (rest_test_t *rt);
   const char *rest_test_req_header (rest_test_t *rt, const char *header);

   // Set all the fields in the response
   bool rest_test_rsp_set_http_version (rest_test_t *rt, const char *http_version);
   bool rest_test_rsp_set_status_code (rest_test_t *rt, const char *status_code);
   bool rest_test_rsp_set_reason (rest_test_t *rt, const char *reason);
   bool rest_test_rsp_set_body (rest_test_t *rt, const char *body);
   bool rest_test_rsp_append_body (rest_test_t *rt, const char *body);
   bool rest_test_rsp_set_header (rest_test_t *rt,
                                  const char *source, size_t line_no,
                                  const char *value);


   // Get all the fields in the response
   const char *rest_test_rsp_http_version (rest_test_t *rt);
   const char *rest_test_rsp_status_code (rest_test_t *rt);
   const char *rest_test_rsp_reason (rest_test_t *rt);
   const char *rest_test_rsp_body (rest_test_t *rt);
   const char *rest_test_rsp_header (rest_test_t *rt, const char *header);

   // Evaluate all the request fields in the test, performing both interpolation and
   // substitution. The token that caused the error is returned in the `errtoken`
   // parameter and the caller MUST NOT delete it. When there are no errors this
   // `errtoken` parameter should be ignored on return from the function.
   //
   // If the `errtoken` parameter is NULL, the token causing the error will not be
   // returned.
   bool rest_test_eval_req (rest_test_t *rt, rest_test_token_t **errtoken);

#ifdef __cplusplus
};
#endif


#endif


