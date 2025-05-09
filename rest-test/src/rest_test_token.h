
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
   token_SHELLCMD,
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

   rest_test_token_t *rest_test_token_dup (const rest_test_token_t *src);

   bool rest_test_token_append (rest_test_token_t *existing, const rest_test_token_t *new);

   enum rest_test_token_type_t rest_test_token_type (const rest_test_token_t *token);
   const char *rest_test_token_value (const rest_test_token_t *token);
   const char *rest_test_token_source (const rest_test_token_t *token);
   size_t rest_test_token_line_no (const rest_test_token_t *token);

   bool rest_test_token_set_value (rest_test_token_t *token, const char *value);

#ifdef __cplusplus
};
#endif


#endif


