/*
 * wrapper.cc
 *
 *   Copyright: Copyright 2012 Peter Goodman, all rights reserved.
 *      Author: Peter Goodman
 */


/// Prevent globals.h and others from including (parts of) the C standard
/// library.
#define GRANARY_DONT_INCLUDE_CSTDLIB


/// The generated types. This is either the functions/types of libc or the
/// functions/types of the kernel. This is included *before* detach.h (which
/// includes globals.h) so that any prototypes depending on things like
/// stdint.h will still be resolved.
#include "granary/types.h"


/// Prototypes of exported symbols.
#include "granary/detach.h"

#if CONFIG_ENABLE_WRAPPERS

/// Auto-generated file that assigns unique IDs to each function.
#   define WRAP_FOR_DETACH(func) DETACH_ID_ ## func,
namespace granary {
    enum function_wrapper_id {
#   include "granary/gen/detach.inc"
        LAST_DETACH_ID
    };
}
#   undef WRAP_FOR_DETACH


/// Wrapper templates.
#   include "granary/wrapper.h"


/// Needed for C-style variadic functions.
#   include <cstdarg>


/// Wrappers (potentially auto-generated) that are specialized to specific
/// functions (by means of IDs) or to types, all contained in types.h.
#   if GRANARY_IN_KERNEL
#       include "granary/gen/kernel_wrappers.h"
#   else
#       include "granary/gen/user_wrappers.h"
#   endif


/// Auto-generated table of all detachable functions and their wrapper
/// instantiations. These depend on the partial specializations from
/// user/kernel_wrappers.h.
#   define WRAP_FOR_DETACH(func) \
    {   (app_pc) ::func, \
        wrapper_of<DETACH_ID_ ## func>(::func)},
namespace granary {
    const function_wrapper FUNCTION_WRAPPERS[] = {
#   include "granary/gen/detach.inc"
        {nullptr, nullptr}
    };
}
#   undef WRAP_FOR_DETACH

#endif /* CONFIG_ENABLE_WRAPPERS */

