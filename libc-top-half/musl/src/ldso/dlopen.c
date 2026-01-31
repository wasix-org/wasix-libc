#include "dlfcn.h"
#include "dynlink.h"
#include "wasi/api.h"
#include "string.h"
#include "stdlib.h"

__wasi_dl_handle_t __wasix_main_dl_handle = 0;

struct __wasix_dl_entry {
	char *name;
	__wasi_dl_handle_t handle;
	int flags;
	int refcount;
	struct __wasix_dl_entry *next;
};

static struct __wasix_dl_entry *__wasix_dl_list = NULL;

static struct __wasix_dl_entry *__wasix_find_by_name(const char *name)
{
	struct __wasix_dl_entry *cur = __wasix_dl_list;
	while (cur) {
		if (cur->name && strcmp(cur->name, name) == 0) {
			return cur;
		}
		cur = cur->next;
	}
	return NULL;
}

static struct __wasix_dl_entry *__wasix_find_by_handle(__wasi_dl_handle_t handle)
{
	struct __wasix_dl_entry *cur = __wasix_dl_list;
	while (cur) {
		if (cur->handle == handle) {
			return cur;
		}
		cur = cur->next;
	}
	return NULL;
}

int __wasix_dl_handle_alive(__wasi_dl_handle_t handle)
{
	return __wasix_find_by_handle(handle) != NULL;
}

static void __wasix_add_or_update(const char *name, __wasi_dl_handle_t handle, int mode)
{
	struct __wasix_dl_entry *entry = __wasix_find_by_name(name);
	if (entry) {
		entry->handle = handle;
		entry->flags |= mode;
		entry->refcount += 1;
		return;
	}

	entry = __wasix_find_by_handle(handle);
	if (entry) {
		entry->flags |= mode;
		entry->refcount += 1;
		return;
	}

	entry = (struct __wasix_dl_entry *)calloc(1, sizeof(*entry));
	if (!entry) {
		return;
	}

	entry->name = strdup(name);
	if (!entry->name) {
		free(entry);
		return;
	}

	entry->handle = handle;
	entry->flags = mode;
	entry->refcount = 1;
	entry->next = __wasix_dl_list;
	__wasix_dl_list = entry;
}

size_t __wasix_dl_copy_global_handles(__wasi_dl_handle_t *out, size_t max)
{
	size_t count = 0;
	struct __wasix_dl_entry *cur = __wasix_dl_list;
	while (cur && count < max) {
		if (cur->flags & RTLD_GLOBAL) {
			out[count++] = cur->handle;
		}
		cur = cur->next;
	}
	return count;
}

int __wasix_dl_release(__wasi_dl_handle_t handle, int *should_close)
{
	struct __wasix_dl_entry *cur = __wasix_dl_list;
	struct __wasix_dl_entry *prev = NULL;

	while (cur) {
		if (cur->handle == handle) {
			if (cur->refcount > 1) {
				cur->refcount -= 1;
				*should_close = 0;
				return 1;
			}

			if (prev) {
				prev->next = cur->next;
			} else {
				__wasix_dl_list = cur->next;
			}
			free(cur->name);
			free(cur);
			*should_close = 1;
			return 1;
		}
		prev = cur;
		cur = cur->next;
	}

	return 0;
}

void *dlopen(const char *file, int mode)
{
	__wasi_dl_handle_t ret = 0;
	char err_buf[256];
	err_buf[0] = '\0';

	char *ld_library_path = getenv("LD_LIBRARY_PATH");

	if (file && (mode & RTLD_NOLOAD)) {
		struct __wasix_dl_entry *entry = __wasix_find_by_name(file);
		if (entry) {
			entry->flags |= mode;
			entry->refcount += 1;
			return (void *)entry->handle;
		}
		__dl_seterr("dlopen failed: library \"%s\" wasn't loaded and RTLD_NOLOAD prevented it", file);
		return NULL;
	}

	int err = __wasi_dlopen(file, mode, (uint8_t *)err_buf, sizeof(err_buf), ld_library_path, &ret);

	if (err != 0)
	{
		if (err_buf[0] != '\0')
		{
			__dl_seterr("%s", err_buf);
		}
		else
		{
			__dl_seterr("dlopen failed with error %s", strerror(err));
		}
		return NULL;
	}

	if (file == NULL && ret != 0)
	{
		__wasix_main_dl_handle = ret;
	}
	else if (file != NULL && ret != 0)
	{
		__wasix_add_or_update(file, ret, mode);
	}

	return (void *)ret;
}
