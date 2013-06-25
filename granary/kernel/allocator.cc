/* Copyright 2012-2013 Peter Goodman, all rights reserved. */
/*
 * heap.cc
 *
 *   Copyright: Copyright 2012 Peter Goodman, all rights reserved.
 *      Author: Peter Goodman
 */

#include "granary/state.h"
#include "granary/allocator.h"

namespace granary { namespace detail {

    void *(*__malloc_exec)(unsigned long, int);
    void *(*__malloc)(unsigned long);
    void (*__free)(void *);

    void *global_allocate_executable(unsigned long size, int where) throw() {
        return __malloc_exec(size, where);
    }


    void global_free_executable(void *addr, unsigned long) throw() {
        return __free(addr);
    }


    void *global_allocate(unsigned long size) throw() {
        void * mem(__malloc(size));
        //memset(mem, 0, size);
        return mem;
    }

    void global_free(void *addr) throw() {
        return __free(addr);
    }

}}

extern "C" {


    void *(**kernel_malloc_exec)(unsigned long, int) = &(granary::detail::__malloc_exec);
    void *(**kernel_malloc)(unsigned long) = &(granary::detail::__malloc);
    void (**kernel_free)(void *) = &(granary::detail::__free);


    void *granary_heap_alloc(void *, unsigned long long size) {
#if CONFIG_PRECISE_ALLOCATE
        granary::detail::global_allocate(size);
#else
        granary::cpu_state_handle cpu;
        return cpu->transient_allocator.allocate_untyped(16, size);
#endif
    }


    void granary_heap_free(void *, void *addr, unsigned long long) {
#if CONFIG_PRECISE_ALLOCATE
        granary::detail::global_free(addr);
#endif
        (void) addr;
    }


    /// Return temporarily allocated space for an instruction.
    void *granary_heap_alloc_temp_instr(void) {
        granary::cpu_state_handle cpu;
        return cpu->instruction_allocator.allocate_array<uint8_t>(32);
    }
}
