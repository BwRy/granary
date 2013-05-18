/* Copyright 2012-2013 Peter Goodman, all rights reserved. */
/*
 * watched_wrappers.h
 *
 *  Created on: 2013-05-11
 *      Author: Peter Goodman
 */

#ifndef WATCHPOINT_WATCHED_WRAPPERS_H_
#define WATCHPOINT_WATCHED_WRAPPERS_H_


#include "clients/watchpoints/policies/null_policy.h"


using namespace client::wp;


/// Specify the descriptor type to the generic watchpoint framework.
namespace client { namespace wp {
    template <typename>
    struct descriptor_type {
        typedef void type;
    };
}}


#define APP_WRAPPER_FOR_pointer
POINTER_WRAPPER({
    PRE_OUT {
        if(!is_valid_address(arg)) {
            return;
        }
        auto unwatched_arg(unwatched_address(arg));
        RELAX_WRAP_DEPTH;
        PRE_OUT_WRAP(*unwatched_arg);
    }
    INHERIT_PRE_IN
    INHERIT_POST_INOUT
    INHERIT_RETURN_INOUT
})


#define APP_WRAPPER_FOR_void_pointer
TYPE_WRAPPER(void *, {
    NO_PRE_IN
    PRE_OUT {
        if(is_watched_address(arg)) {
            arg = unwatched_address(arg);
        }
    }
    NO_POST
    NO_RETURN
})


#if defined(CAN_WRAP___kmalloc) && CAN_WRAP___kmalloc
#   define APP_WRAPPER_FOR___kmalloc
    FUNCTION_WRAPPER(APP, __kmalloc, (void *), (size_t size, gfp_t gfp), {
        void *ptr(__kmalloc(size, gfp));
        if(!ptr) {
            return ptr;
        }
        add_watchpoint(ptr);
        return ptr;
    })
#endif


#if defined(CAN_WRAP_kfree) && CAN_WRAP_kfree
#   define APP_WRAPPER_FOR_kfree
    FUNCTION_WRAPPER_VOID(APP, kfree, (const void *ptr), {
        return kfree(unwatched_address(ptr));
    })
#endif


#if defined(CAN_WRAP_kmem_cache_alloc) && CAN_WRAP_kmem_cache_alloc
#   define APP_WRAPPER_FOR_kmem_cache_alloc
    FUNCTION_WRAPPER(APP, kmem_cache_alloc, (void *), (struct kmem_cache *cache, gfp_t gfp), {
        PRE_OUT_WRAP(cache);
        void *ptr(kmem_cache_alloc(cache, gfp));
        if(!ptr) {
            return ptr;
        }

        // Add watchpoint before constructor so that internal pointers
        // maintain their invariants (e.g. list_head structures).
        memset(ptr, 0, cache->object_size);
        add_watchpoint(ptr);
        if(is_valid_address(cache->ctor)) {
            cache->ctor(ptr);
        }

        return ptr;
    })
#endif


#if defined(CAN_WRAP_kmem_cache_alloc_trace) && CAN_WRAP_kmem_cache_alloc_trace
#   define APP_WRAPPER_FOR_kmem_cache_alloc_trace
    FUNCTION_WRAPPER(APP, kmem_cache_alloc_trace, (void *), (struct kmem_cache *cache, gfp_t gfp, size_t size), {
        if(cache) {
            PRE_OUT_WRAP(cache);
        }

        void *ptr(kmem_cache_alloc_trace(cache, gfp, size));
        if(!ptr) {
            return ptr;
        }

        // Add watchpoint before constructor so that internal pointers
        // maintain their invariants (e.g. list_head structures).
        memset(ptr, 0, size);
        add_watchpoint(ptr);
        if(is_valid_address(cache->ctor)) {
            cache->ctor(ptr);
        }

        return ptr;
    })
#endif


#if defined(CAN_WRAP_kmem_cache_alloc_node) && CAN_WRAP_kmem_cache_alloc_node
#   define APP_WRAPPER_FOR_kmem_cache_alloc_node
    FUNCTION_WRAPPER(APP, kmem_cache_alloc_node, (void *), (struct kmem_cache *cache, gfp_t gfp, int node), {
        if(cache) {
            PRE_OUT_WRAP(cache);
        }

        void *ptr(kmem_cache_alloc_node(cache, gfp, node));
        if(!ptr) {
            return ptr;
        }

        // Add watchpoint before constructor so that internal pointers
        // maintain their invariants (e.g. list_head structures).
        memset(ptr, 0, cache->object_size);
        add_watchpoint(ptr);
        if(is_valid_address(cache->ctor)) {
            cache->ctor(ptr);
        }

        return ptr;
    })
#endif


#if defined(CAN_WRAP_kmem_cache_free) && CAN_WRAP_kmem_cache_free
#   define APP_WRAPPER_FOR_kmem_cache_free
    FUNCTION_WRAPPER(APP, kmem_cache_free, (void), (struct kmem_cache *cache, void *ptr), {
        PRE_OUT_WRAP(cache);
        kmem_cache_free(cache, unwatched_address(ptr));
    })
#endif


#endif /* WATCHPOINT_WATCHED_WRAPPERS_H_ */
