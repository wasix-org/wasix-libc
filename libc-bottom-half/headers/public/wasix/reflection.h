#ifndef wasix_reflection_h
#define wasix_reflection_h
// Wrappers for the closures interface
//
// Currently they are just aliases for the raw syscall, but this may change in
// the future if we need to add additional logic before or after the call is
// made.

#include <wasi/api_wasi.h>
#include <wasi/api_wasix.h>
#include <wasix/function_pointer.h>
#include <wasix/value_type.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef __wasi_reflection_result_t wasix_reflection_result_t;

/**
 * Provides information about the signature of a function in the indirect
 * function table at runtime.
 *
 * ### Errors
 *
 * Besides the standard error codes, `reflect_signature` may set `errno` to the
 * following values:
 *
 * - EINVAL: The function pointer is not valid, i.e. it does not point to a
 * function in the indirect function table or the function has a unsupported
 * signature. The sizes in the result are undefined in this case.
 *
 * - EOVERFLOW: The argument_types and result_types buffers were not big enough
 * to hold the signature. They will be left unchanged. The reflection result
 * will be valid.
 *
 * @return
 * Returns 0 on success, or `-1` on failure. If an error occurs, `errno` is set
 * to the appropriate error code.
 */
int wasix_reflect_signature(
    /**
     * An index into the indirect function table.
     */
    wasix_function_pointer_t function_id,
    /**
     * A buffer for a list of types that the function takes as arguments.
     *
     * - If the buffer is too small to hold all argument types, it and
     * `result_types` remain untouched, and `errno` is set to `EOVERFLOW`.
     * - If the buffer is big enough, the types will be written to the buffer.
     * - If the buffer is too big, all remaining bytes will be unchanged.
     *
     * If the `argument_types_len` is 0 this buffer is never accessed and can be
     * null.
     */
    wasix_value_type_t *argument_types,
    /**
     * Size of the buffer for argument types.
     */
    uint16_t argument_types_len,
    /**
     * A buffer for a list of types that the function returns as results.
     *
     * - If the buffer is too small to hold all argument types, it and
     * `argument_types` remain untouched, and `errno` is set to `EOVERFLOW`.
     * - If  the buffer is big enough, the types will be written to the buffer.
     * - If the buffer is too big, all remaining bytes will be unchanged.
     *
     * If the `result_types_len` is 0 this buffer is never accessed and can be
     * null.
     */
    wasix_value_type_t *result_types,
    /**
     * Size of the buffer for result types.
     */
    uint16_t result_types_len,
    /**
     * The number of arguments and results and whether this result is cacheable.
     */
    wasix_reflection_result_t *result);

#ifdef __cplusplus
}
#endif
#endif