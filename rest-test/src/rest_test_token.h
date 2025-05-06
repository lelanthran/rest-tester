
#ifndef H_REST_TEST_TOKEN
#define H_REST_TEST_TOKEN

enum rest_test_token_type_t {
   token_NONE,
   token_UNKNOWN,
   token_DIRECTIVE,
   token_STRING,
   token_SYMBOL,
   token_INTEGER,
   token_ASSERT_END,
};

typedef struct rest_test_token_t rest_test_token_t;

#ifdef __cplusplus
extern "C" {
#endif

   const char *rest_test_token_type_string (enum rest_test_token_type_t type);
   enum rest_test_token_type_t rest_test_token_type_type (const char *s);

   rest_test_token_t *rest_test_token_new (enum rest_test_token_type_t type,
                                           const char *value,
                                           const char *source,
                                           size_t line_no);
   void rest_test_token_del (rest_test_token_t **token);

   rest_test_token_t *rest_test_token_next (FILE *inf,
                                            const char *source,
                                            size_t *line_no);

   enum rest_test_token_type_t rest_test_token_type (rest_test_token_t *token);
   const char *rest_test_token_value (rest_test_token_t *token);

#ifdef __cplusplus
};
#endif


#endif


