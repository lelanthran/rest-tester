
#ifndef H_REST_TEST_SYMT
#define H_REST_TEST_SYMT

typedef struct rest_test_symt_t rest_test_symt_t;

/* *****************************************************************************
 * TODO: Symbol tables should store token_t types, for the value.  For now using
 * bare strings for keys and for values, but it makes for better error reporting to
 * be able to display to the user where a symbol was first defined.
 */

/* *****************************************************************************
 * A very simple symbol table implementation. Simply put, this is a hashmap with
 * support for a parent hashmap. When looking for a symbol, if it is not found in
 * the specified symbol table the search is recursively repeated on the parent node
 * until a match is found or there are no more parents.
 *
 * In this way multiple scopes can be implemented.
 */
#ifdef __cplusplus
extern "C" {
#endif

   // Create a new symbol table with mandatory name and mandatory bucket count.
   // On failure NULL is returned. On success a symbol table is returned which must
   // be freed using rest_test_symt_del().
   rest_test_symt_t *rest_test_symt_new (const char *name,
                                         rest_test_symt_t *parent,
                                         size_t buckets);

   // Deletes a symbol table returned from rest_test_symt_new()
   void rest_test_symt_del (rest_test_symt_t **symt);

   // Returns the direct fields of the symbol table
   const char *rest_test_symt_name (rest_test_symt_t *symt);
   rest_test_symt_t *rest_test_symt_parent (rest_test_symt_t *symt);

   // Sets the direct fields
   const char *rest_test_symt_set_name (rest_test_symt_t *symt, const char *name);

   // Dumps the symbol table to the specified file stream in a human readable
   // format. If `fout` is NULL then `stdout` is used.
   void rest_test_symt_dump (rest_test_symt_t *symt, FILE *fout);

   // Add a new entry to the specific symbol table. Any existing value with the same
   // symbol is removed. If an entry could not be added (for example, an
   // out-of-memory condition occurs), then false is returned. If the entry is added
   // then true is returned.
   bool rest_test_symt_add (rest_test_symt_t *symt,
                            const char *symbol, const char *value);

   // Removes a value from the symbol table.
   void rest_test_symt_clear (rest_test_symt_t *symt, const char *symbol);

   // Returns a value from the symbol table. If the symbol does not exist in the
   // specified table a recursive search of the ancestors is performed until a
   // matching symbol is found or there are no more parents.
   //
   // Returns the value of the symbol if the symbol exists, or NULL if the symbol
   // does not exist.
   const char *rest_test_symt_value (rest_test_symt_t *symt, const char *symbol);

#ifdef __cplusplus
};
#endif


#endif


