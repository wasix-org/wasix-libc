#include <errno.h>
#include <wasi/api_wasi.h>
#include <wasi/api_wasix.h>
#include <wasix/context.h>

wasix_context_id_t __wasix_context_main(void) {
  return 0; // Main context is always ID 0 for now
}

int wasix_context_create(wasix_context_id_t *context_id, void (*entrypoint)()) {
  int err =
      __wasi_context_create(context_id, (__wasi_function_pointer_t)entrypoint);

  if (err != __WASI_ERRNO_SUCCESS) {
    errno = err;
    return -1;
  }

  return 0;
}

int wasix_context_switch(wasix_context_id_t target_context_id) {
  int err = __wasi_context_switch(target_context_id);

  if (err != __WASI_ERRNO_SUCCESS) {
    errno = err;
    return -1;
  }

  return 0;
}

int wasix_context_destroy(wasix_context_id_t context_id) {
  int err = __wasi_context_destroy(context_id);

  if (err != __WASI_ERRNO_SUCCESS) {
    errno = err;
    return -1;
  }

  return 0;
}