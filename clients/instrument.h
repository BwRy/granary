/*
 * instrument.h
 *
 *  Created on: Nov 20, 2012
 *      Author: pag
 */

#ifndef INSTRUMENT_H_
#define INSTRUMENT_H_

#include "granary/client.h"

#define GRANARY_INIT_POLICY (client::null_policy())

namespace client {

    struct null_policy : public granary::instrumentation_policy {
    public:


        /// Instrument a basic block.
        static granary::instrumentation_policy visit_basic_block(
            granary::cpu_state_handle &cpu,
            granary::thread_state_handle &thread,
            granary::basic_block_state &bb,
            granary::instruction_list &ls
        ) throw();


#if CONFIG_CLIENT_HANDLE_INTERRUPT
        /// Handle an interrupt in module code. Returns true iff the client
        /// handles the interrupt.
        static granary::interrupt_handled_state handle_interrupt(
            granary::cpu_state_handle &cpu,
            granary::thread_state_handle &thread,
            granary::basic_block_state &bb,
            granary::interrupt_stack_frame &isf,
            granary::interrupt_vector vector
        ) throw();
#endif

    };


#if CONFIG_CLIENT_HANDLE_INTERRUPT
    /// Handle an interrupt in kernel code. Returns true iff the client handles
    /// the interrupt.
    granary::interrupt_handled_state handle_kernel_interrupt(
        granary::cpu_state_handle &cpu,
        granary::thread_state_handle &thread,
        granary::interrupt_stack_frame &isf,
        granary::interrupt_vector vector
    ) throw();
#endif

}

#endif /* INSTRUMENT_H_ */
