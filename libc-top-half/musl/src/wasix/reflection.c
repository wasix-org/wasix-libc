#include <errno.h>
#include <wasix/reflection.h>

// #include <stdlib.h>
// #include <stddef.h>
// #include <string.h>
// #define HT_INITIAL_CAP 16
// #define HT_MAX_LOAD 0.5
// #define EMPTY_KEY 0
// _Static_assert(EMPTY_KEY == 0, "EMPTY_KEY must be 0 so we can use calloc for
// initialization");

// typedef struct {
//     int key;
//     int value;
// } ht_entry_t;

// ht_entry_t * hash_table = NULL;
// size_t ht_capacity = 0;
// size_t ht_entries = 0;
// size_t ht_max_entries = 0;

// static int* ht_get(int key) {
//     if (hash_table == NULL) {
//         return NULL; // Hash table not initialized
//     }

//     size_t index = key % ht_capacity;
//     while (hash_table[index].key != EMPTY_KEY) {
//         if (hash_table[index].key == key) {
//             return &hash_table[index].value; // Return pointer to value
//         }
//         index = (index + 1) % ht_capacity; // Linear probing
//     }
//     return NULL; // Key not found
// }

// static void ht_grow() {
//     assert(hash_table != NULL);

//     size_t new_capacity = ht_capacity * 2;

//     ht_entry_t *new_table = calloc(new_capacity, sizeof(ht_entry_t));

//     for (size_t i = 0; i < ht_capacity; i++) {
//         if (hash_table[i].key != EMPTY_KEY) {
//             size_t index = hash_table[i].key % new_capacity;
//             while (new_table[index].key != EMPTY_KEY) {
//                 index = (index + 1) % new_capacity; // Linear probing
//             }
//             new_table[index] = hash_table[i];
//         }
//     }

//     free(hash_table);
//     hash_table = new_table;
//     ht_capacity = new_capacity;
//     ht_max_entries = ht_capacity * HT_MAX_LOAD;
// }

// void ht_init() {
//     hash_table = calloc(HT_INITIAL_CAP * sizeof(ht_entry_t));
//     ht_capacity = HT_INITIAL_CAP;
//     ht_max_entries = ht_capacity * HT_MAX_LOAD;
// }

// static void ht_insert(int key, int value) {
//     if (key == EMPTY_KEY) {
//         return; // Do not insert empty key
//     }

//     if (hash_table == NULL) {
//         ht_init();
//     }

//     if (ht_entries >= ht_max_entries) {
//         ht_grow();
//     }

//     size_t index = key % ht_capacity;
//     while (hash_table[index].key != EMPTY_KEY) {
//         if (hash_table[index].key == key) {
//             return; // Key already exists, we don't do updates
//         }
//         index = (index + 1) % ht_capacity; // Linear probing
//     }

//     hash_table[index].key = key;
//     hash_table[index].value = value;
//     ht_entries++;
// }

int wasix_reflect_signature(wasix_function_pointer_t function_id,
                            wasix_value_type_t *argument_types,
                            uint16_t argument_types_len,
                            wasix_value_type_t *result_types,
                            uint16_t result_types_len,
                            wasix_reflection_result_t *result) {
  if (result == NULL) {
    errno = EMEMVIOLATION;
    return -1;
  }

  if (function_id == 0) {
    result->arguments = 0;
    result->results = 0;
    result->cacheable = __WASI_BOOL_TRUE;
    errno = EINVAL;
    return -1;
  }

  // Call the underlying reflection syscall
  int err =
      __wasi_reflect_signature(function_id, argument_types, argument_types_len,
                               result_types, result_types_len, result);

  if (err != __WASI_ERRNO_SUCCESS) {
    errno = err;
    return -1;
  }

  return 0;
}