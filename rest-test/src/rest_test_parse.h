
#ifndef H_REST_TEST_PARSE
#define H_REST_TEST_PARSE

#ifdef __cplusplus
extern "C" {
#endif

   rest_test_t **rest_test_parse_file (rest_test_symt_t *parent, const char *filename);
   rest_test_t **rest_test_parse_stream (rest_test_symt_t *parent, FILE *inf, const char *source);


#ifdef __cplusplus
};
#endif


#endif


