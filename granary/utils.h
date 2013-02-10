/*
 * utils.h
 *
 *  Created on: 2012-11-09
 *      Author: pag
 *     Version: $Id$
 */

#ifndef Granary_UTILS_H_
#define Granary_UTILS_H_

#include "granary/pp.h"

#ifndef GRANARY_DONT_INCLUDE_CSTDLIB
#   include "granary/allocator.h"
#   include "granary/type_traits.h"

#   include <stdint.h>
#   include <cstring>
#   include <algorithm>
#   include <atomic>
#   include <new>
#endif

namespace granary {

    template <typename ToT, typename FromT>
    FORCE_INLINE ToT unsafe_cast(const FromT &v) throw()  {
        ToT dest;
        memcpy(&dest, &v, sizeof(ToT));
        return dest;
    }

    template <typename ToT, typename FromT>
    FORCE_INLINE ToT unsafe_cast(FromT *v) throw() {
        return unsafe_cast<ToT>(
            reinterpret_cast<uintptr_t>(v)
        );
    }


    namespace detail {
        template <typename T, unsigned extra>
        struct cache_aligned_impl {
            T val;
            unsigned char padding[CACHE_LINE_SIZE - extra];
        } __attribute__((packed));


        /// No padding needed.
        template <typename T>
        struct cache_aligned_impl<T, 0> {
            T val;
        };
    }


    /// Repesents a cache aligned type.
    template <typename T>
    struct cache_aligned
        : public detail::cache_aligned_impl<T, sizeof(T) % CACHE_LINE_SIZE>
    { } __attribute__((aligned (CONFIG_MIN_CACHE_LINE_SIZE)));


    /// Returns an offset of some application code from the beginning of
    /// application code.
    int32_t to_application_offset(uint64_t addr) throw();


    /// Converts an offset from the beginning of application code into an
    /// application code pointer (represented as a uint64_t)
    uint64_t from_application_offset(int32_t) throw();


    /// Represents a simple boxed array type. This is mostly means to more
    /// easily pass transiently-allocated arrays around as arguments/return
    /// values.
    template <typename T>
    struct array {
    private:

        T *elms;
        unsigned num_elms;

    public:

        array(void) throw()
            : elms(nullptr)
            , num_elms(0U)
        { }

        array(T *elms_, unsigned num_elms_) throw()
            : elms(elms_)
            , num_elms(num_elms_)
        { }

        array(array<T> &&that) throw()
            : elms(that.elms)
            , num_elms(that.num_elms)
        { }

        array<T> &operator=(array<T> &&that) throw() {
            elms = that.elms;
            num_elms = that.num_elms;
            return *this;
        }

        inline operator T *(void) throw() {
            return elms;
        }

        inline T &operator[](unsigned i) throw() {
            return elms[i];
        }

        inline const T &operator[](unsigned i) const throw() {
            return elms[i];
        }

        inline T *begin(void) throw() {
            return elms;
        }

        inline T *end(void) throw() {
            return elms + num_elms;
        }

#ifndef GRANARY_DONT_INCLUDE_CSTDLIB
        inline void sort(void) throw() {
            std::sort(begin(), end());
        }
#endif
    };


#ifndef GRANARY_DONT_INCLUDE_CSTDLIB

    /// Represents a type where the first type ends with a value
    /// of the second type, and the second type "spills" over to
    /// form a very large array. The assumed use of this type is
    /// that type `T` ends with an array of length 1 of element
    /// of type `V`.
    template <typename T, typename V>
    T *new_trailing_vla(unsigned array_size) throw() {
        const size_t needed_space(sizeof(T) + (array_size - 1) * sizeof(V));
        char *internal(reinterpret_cast<char *>(
            detail::global_allocate(needed_space)));

        if(!std::is_trivial<T>::value) {
            new (internal) T;
        }

        if(!std::is_trivial<V>::value) {
            char *arr_ptr(internal + sizeof(T));
            for(unsigned i(1); i < array_size; ++i) {
                new (arr_ptr) V;
                arr_ptr += sizeof(V);
            }
        }

        return unsafe_cast<T *>(internal);
    }
#endif

    namespace {
        template<typename T>
        class type_id_impl {
        public:
            static unsigned get(void) throw() {
                static unsigned id_(0);
                return id_++;
            }
        };
    }


    /// Used to statically define IDs for particular classes.
    template <typename T, typename Category=void>
    class type_id {
    public:
        static unsigned get(void) throw() {
            static type_id id_(type_id_impl<Category>::get());
            return id_;
        }
    };


    /// Represents a way of indirectly calling a function and being able to
    /// save--or pretend to save--its return value (if any) for later returning
    /// in a type-safe way.
    template <typename R, typename... Args>
    struct function_call {
    private:
        R (*func)(Args...);
        R ret_value;

    public:

        inline function_call(R (*func_)(Args...)) throw()
            : func(func_)
        { }

        inline void operator()(Args... args) throw() {
            ret_value = func(args...);
        }

        inline R yield(void) throw() {
            return ret_value;
        }
    };

    /// Read-critical section returning nothing.
    template <typename... Args>
    struct function_call<void, Args...> {
    private:
        void (*func)(Args...);

    public:

        inline function_call(void (*func_)(Args...)) throw()
            : func(func_)
        { }

        inline void operator()(Args... args) throw() {
            func(args...);
        }

        inline void yield(void) throw() {
            return;
        }
    };
}


#endif /* Granary_UTILS_H_ */
