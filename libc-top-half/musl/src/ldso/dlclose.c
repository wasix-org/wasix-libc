#include <dlfcn.h>
#include "dynlink.h"
#include "wasi/api.h"

extern __wasi_dl_handle_t __wasix_main_dl_handle;
extern int __wasix_dl_release(__wasi_dl_handle_t handle, int *should_close);

int dlclose(void *p)
{
	if (__wasix_main_dl_handle != 0 && (__wasi_dl_handle_t)p == __wasix_main_dl_handle)
	{
		return 0;
	}

	int should_close = 0;
	int known = __wasix_dl_release((__wasi_dl_handle_t)p, &should_close);
	if (known && !should_close)
	{
		return 0;
	}

	int err = __wasi_dl_invalid_handle((__wasi_dl_handle_t)p);
	if (err != 0)
	{
		__dl_seterr("Invalid dl handle");
		return -1;
	}
	return 0;
}
