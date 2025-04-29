
#ifndef H_REST_TEST_SYMT
#define H_REST_TEST_SYMT

typedef struct rest_test_symt_t rest_test_symt_t;

#ifdef __cplusplus
extern "C" {
#endif

   rest_test_symt_t *rest_test_symt_new (rest_test_symt_t *parent);
   void rest_test_symt_del (rest_test_symt_t **symt);


#ifdef __cplusplus
};
#endif


#endif


