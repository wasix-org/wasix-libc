#ifndef wasix_call_dynamic_h
#define wasix_call_dynamic_h
// Wrappers for the dynamic call interface.
//
// Currently they are just aliases for the raw syscall, but this may change in the future if we need to
// add additional logic before or after the call is made.

#include <wasi/api_wasi.h>
#include <wasi/api_wasix.h>
#include <wasix/function_pointer.h>
#include <wasix/value_type.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

// TODO: Document how parameter mismatch is handled vs internal errors.
/**
 * Call a function pointer with dynamic parameters.
 * 
 * Returns:
 *  0 on success
 *  -1 on error, with errno set to the error code.
 */
int wasix_call_dynamic(
    /**
     * An index into the __indirect_function_table
     *
     * Your module needs to either import or export the table to be able
     * to call this function.
     */
    wasix_function_pointer_t function_id,
    /**
     * A buffer with the parameters to pass to the function.
     */
    wasix_raw_value_with_type_t *values,
    /**
     * The length of the array pointed to by `values`, in number
     * of elements.
     */
    size_t values_len,
    /**
     * A pointer to a buffer for the results of the function call.
     */
    wasix_raw_value_with_type_t *results,
    /**
     * The length of the array pointed to by `results`, in number
     * of elements. The value pointed to by this pointer will be updated
     * to reflect the actual number of results written.
     */
    size_t *results_len,
    /**
     * If this is set to true, the function will return an error if the
     * length and types of the parameters and results do not match the
     * function signature.
     *
     * If this is set to false, the function will not perform any checks.
     * Any missing values will be assumed to be zero and any extra values
     * will be ignored. Also, results that don't fit will be discarded
     * and extra space in the results buffer will be set to zero.
     */
    bool strict);

#ifdef __cplusplus
}
#endif
#endif