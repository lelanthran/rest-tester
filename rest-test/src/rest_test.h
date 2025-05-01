
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

   rest_test_t *rest_test_new (const char *name,
                               const char *fname,
                               size_t line_no,
                               rest_test_t *parent);
   void rest_test_del (rest_test_t **rt);



#ifdef __cplusplus
};
#endif


#endif


