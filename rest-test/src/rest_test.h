
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

   // Set all the fields in the request
   bool rest_test_req_set_method (rest_test_t *rt, const char *method);
   bool rest_test_req_set_uri (rest_test_t *rt, const char *uri);
   bool rest_test_req_set_http_version (rest_test_t *rt, const char *http_version);
   bool rest_test_req_set_body (rest_test_t *rt, const char *body);
   bool rest_test_req_append_body (rest_test_t *rt, const char *body);
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


#ifdef __cplusplus
};
#endif


#endif


