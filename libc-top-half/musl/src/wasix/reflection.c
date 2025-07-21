#include <errno.h>
#include <wasix/reflection.h>

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