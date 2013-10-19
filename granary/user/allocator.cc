/* Copyright 2012-2013 Peter Goodman, all rights reserved. */
/*
 * allocator.cc
 *
 *   Copyright: Copyright 2012 Peter Goodman, all rights reserved.
 *      Author: Peter Goodman
 */

#include "granary/globals.h"
#include "granary/state.h"
#include "granary/detach.h"

#include <cstdlib>
#include <stdint.h>

#define PROT_ALL (~0)

extern "C" {

#   include <sys/mman.h>
#   include <unistd.h>

    /// DynamoRIO-compatible heap allocation

    void *granary_heap_alloc(void *, unsigned long long size) {
        granary::cpu_state_handle cpu;
        return cpu->transient_allocator.allocate_untyped(16, size);
    }

    void granary_heap_free(void *, void *addr, unsigned long size) {
#if CONFIG_PRECISE_ALLOCATE
        granary::detail::global_free(addr, size);
#else
        UNUSED(addr);
        UNUSED(size);
#endif
    }


    /// Return temporarily allocated space for an instruction.
    void *granary_heap_alloc_temp_instr(void) {
        granary::cpu_state_handle cpu;
        return cpu->instruction_allocator.allocate_array<uint8_t>(32);
    }


    /// (un)protect up to two pages
    static int make_page_executable(void *page_start) {

        return -1 != mprotect(
            page_start,
            granary::PAGE_SIZE,
            PROT_EXEC | PROT_READ | PROT_WRITE
        );
    }
}

#ifndef MAP_ANONYMOUS
#   ifdef MAP_ANON
#       define MAP_ANONYMOUS MAP_ANON
#   else
#       define MAP_ANONYMOUS 0
#   endif
#endif

#ifndef MAP_SHARED
#   define MAP_SHARED 0
#endif

namespace granary { namespace detail {

    void *global_allocate_executable(unsigned long size, int) throw() {

        size = size + ALIGN_TO(size, PAGE_SIZE);

        void *allocated(mmap(
            nullptr,
            size,
            PROT_READ | PROT_WRITE | PROT_EXEC,
            MAP_ANONYMOUS | MAP_PRIVATE,
            -1,
            0));

        if(MAP_FAILED == allocated) {
            granary_break_on_fault();
            granary_fault();
        }

        IF_TEST( memset(allocated, 0xCC, size); )

        return allocated;
    }


    void global_free_executable(void *addr, unsigned long size) throw() {
        munmap(addr, size);
    }


    void *global_allocate(unsigned long size) throw() {
        return malloc(size);
    }

    void global_free(void *addr, unsigned long) throw() {
        free(addr);
    }
}}

/// TODO: Not sure if these are needed or not in user space anymore, e.g. with
///       instrumenting a C++ program like clang.
#if 0 && GRANARY_USE_PIC && !GRANARY_IN_KERNEL
    extern "C" {
        extern void _Znwm(void);
        extern void _Znam(void);
        extern void _ZdlPv(void);
        //extern void _ZdaPv(void);
    }

    /// Make sure that the global operator new/delete are detach points.
    GRANARY_DETACH_POINT(_Znwm) // operator new
    GRANARY_DETACH_POINT(_Znam) // operator new[]
    GRANARY_DETACH_POINT(_ZdlPv) // operator delete
    //GRANARY_DETACH_POINT(_ZdaPv) // operator delete[]

#endif

/// Add some illegal detach points.
GRANARY_DETACH_POINT_ERROR(granary::detail::global_allocate)
GRANARY_DETACH_POINT_ERROR(granary::detail::global_allocate_executable)
GRANARY_DETACH_POINT_ERROR(granary::detail::global_free)
GRANARY_DETACH_POINT_ERROR(granary::detail::global_free_executable)
