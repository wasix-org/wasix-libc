#include "dlfcn.h"
#include "dynlink.h"
#include "wasi/api.h"
#include "string.h"

extern __wasi_dl_handle_t __wasix_main_dl_handle;
size_t __wasix_dl_copy_global_handles(__wasi_dl_handle_t *out, size_t max);
int __wasix_dl_handle_alive(__wasi_dl_handle_t handle);

static void *__wasix_dlsym_handle(__wasi_dl_handle_t handle, const char *s, char *err_buf, size_t err_buf_len, int *err_out, __wasi_size_t *ret_out)
{
	err_buf[0] = '\0';
	int err = __wasi_dlsym(handle, s, (uint8_t *)err_buf, err_buf_len, ret_out);
	if (err == 0) {
		return (void *)*ret_out;
	}
	*err_out = err;
	return NULL;
}

void *dlsym(void *restrict p, const char *restrict s)
{
	__wasi_size_t ret;
	char err_buf[256];
	err_buf[0] = '\0';

	if (p == RTLD_DEFAULT) {
		__wasi_dl_handle_t handles[64];
		char last_err_buf[256];
		int last_err = 0;
		last_err_buf[0] = '\0';

		if (__wasix_main_dl_handle) {
			void *found = __wasix_dlsym_handle(__wasix_main_dl_handle, s, err_buf, sizeof(err_buf), &last_err, &ret);
			if (found) {
				return found;
			}
			if (err_buf[0] != '\0') {
				strncpy(last_err_buf, err_buf, sizeof(last_err_buf));
				last_err_buf[sizeof(last_err_buf) - 1] = '\0';
			}
		}

		size_t count = __wasix_dl_copy_global_handles(handles, sizeof(handles) / sizeof(handles[0]));
		for (size_t i = 0; i < count; i++) {
			void *found = __wasix_dlsym_handle(handles[i], s, err_buf, sizeof(err_buf), &last_err, &ret);
			if (found) {
				return found;
			}
			if (err_buf[0] != '\0') {
				strncpy(last_err_buf, err_buf, sizeof(last_err_buf));
				last_err_buf[sizeof(last_err_buf) - 1] = '\0';
			}
		}

		if (last_err_buf[0] != '\0') {
			__dl_seterr("%s", last_err_buf);
		} else if (last_err != 0) {
			__dl_seterr("dlsym failed with error %s", strerror(last_err));
		} else {
			__dl_seterr("dlsym failed: symbol \"%s\" not found", s);
		}
		return NULL;
	}

	if (p != NULL && (__wasi_dl_handle_t)p != __wasix_main_dl_handle && !__wasix_dl_handle_alive((__wasi_dl_handle_t)p)) {
		__dl_seterr("dlsym failed: invalid handle");
		return NULL;
	}

	int err = __wasi_dlsym((__wasi_dl_handle_t)p, s, (uint8_t *)err_buf, sizeof(err_buf), &ret);

	if (err != 0)
	{
		if (err_buf[0] != '\0')
		{
			__dl_seterr("%s", err_buf);
		}
		else
		{
			__dl_seterr("dlsym failed with error %s", strerror(err));
		}
		return NULL;
	}

	return (void *)ret;
}
