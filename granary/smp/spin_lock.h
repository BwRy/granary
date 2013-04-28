/* Copyright 2012-2013 Peter Goodman, all rights reserved. */
/*
 * spin_lock.h
 *
 *  Created on: 2013-01-20
 *      Author: pag
 */

#ifndef SPIN_LOCK_H_
#define SPIN_LOCK_H_

#include <atomic>

#if GRANARY_IN_KERNEL
extern "C" {
    extern void kernel_preempt_disable(void);
    extern void kernel_preempt_enable(void);
}
#endif

namespace granary { namespace smp {

    /// Simple implementation of a spin lock.
    struct atomic_spin_lock {
    private:

        std::atomic<bool> is_locked;

    public:

        ~atomic_spin_lock(void) throw() = default;

        atomic_spin_lock(const atomic_spin_lock &) throw() = delete;
        atomic_spin_lock &operator=(const atomic_spin_lock &) throw() = delete;

        atomic_spin_lock(void) throw()
            : is_locked(ATOMIC_VAR_INIT(false))
        { }

        inline void acquire(void) throw() {
            IF_KERNEL( kernel_preempt_disable(); )

            for(;;) {
                if(is_locked.load(std::memory_order_acquire)) {
                    ASM("pause;");
                    continue;
                }

                if(!is_locked.exchange(true, std::memory_order_acquire)) {
                    break;
                }

                ASM("pause;");
            }
        }

        inline void release(void) throw() {
            is_locked.store(false, std::memory_order_release);
            IF_KERNEL( kernel_preempt_enable(); )
        }
    };

}}

#if GRANARY_IN_KERNEL

namespace granary { namespace smp {
    typedef atomic_spin_lock spin_lock;
}}

#else
#   include <pthread.h>

namespace granary { namespace smp {

    /// Simple implementation of lock using pthread mutexes.
    struct spin_lock {
    private:

        pthread_mutex_t mutex;

    public:

        ~spin_lock(void) throw() {
            pthread_mutex_destroy(&mutex);
        }

        spin_lock(const spin_lock &) throw() = delete;
        spin_lock &operator=(const spin_lock &) throw() = delete;

        spin_lock(void) throw() {
            pthread_mutex_init(&mutex, nullptr);
        }

        inline void acquire(void) throw() {
            pthread_mutex_lock(&mutex);
        }

        inline void release(void) throw() {
            pthread_mutex_unlock(&mutex);
        }
    };
}}

#endif

#endif /* SPIN_LOCK_H_ */
