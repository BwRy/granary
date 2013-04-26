/* Copyright 2012-2013 Peter Goodman, all rights reserved. */
/*
 * null_policy.cc
 *
 *  Created on: 2013-04-24
 *      Author: pag
 */

#include "clients/watchpoints/policies/null_policy.h"

using namespace granary;

namespace client { namespace wp {

    void null_policy::visit_read(
        cpu_state_handle &,
        thread_state_handle &,
        granary::basic_block_state &,
        instruction_list &,
        watchpoint_tracker &,
        unsigned
    ) throw() { }


    void null_policy::visit_write(
        cpu_state_handle &,
        thread_state_handle &,
        granary::basic_block_state &,
        instruction_list &,
        watchpoint_tracker &,
        unsigned
    ) throw() { }


#if CONFIG_CLIENT_HANDLE_INTERRUPT
    interrupt_handled_state null_policy::handle_interrupt(
        cpu_state_handle &,
        thread_state_handle &,
        granary::basic_block_state &,
        interrupt_stack_frame &,
        interrupt_vector
    ) throw() {
        return INTERRUPT_DEFER;
    }
} /* wp namespace */

    /// Handle an interrupt in kernel code. Returns true iff the client handles
    /// the interrupt.
    interrupt_handled_state handle_kernel_interrupt(
        cpu_state_handle &,
        thread_state_handle &,
        interrupt_stack_frame &,
        interrupt_vector
    ) throw() {
        return INTERRUPT_DEFER;
    }

#endif /* CONFIG_CLIENT_HANDLE_INTERRUPT */

} /* client namespace */
