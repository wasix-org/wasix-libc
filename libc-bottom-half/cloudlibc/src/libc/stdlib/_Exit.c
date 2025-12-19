// Copyright (c) 2015-2016 Nuxi, https://nuxi.nl/
//
// SPDX-License-Identifier: BSD-2-Clause

#include <wasi/api.h>
#include <_/cdefs.h>
#include <stdnoreturn.h>
#include <unistd.h>


noreturn void _Exit(int status) {
  __wasi_proc_exit2(status);
  #ifdef __wasm_exception_handling__
  extern noreturn void __vfork_restore();
	__vfork_restore();
	#endif
  __builtin_unreachable();
}

__strong_reference(_Exit, _exit);
