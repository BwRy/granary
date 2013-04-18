/*
 * test_interrupt_delay.cc
 *
 *  Created on: 2013-04-16
 *      Author: pag
 */

#include "granary/test.h"

#if CONFIG_RUN_TEST_CASES && GRANARY_IN_KERNEL && CONFIG_CLIENT_HANDLE_INTERRUPT && CONFIG_ENABLE_PERF_COUNTS

namespace test {

    namespace {
        static bool interrupted = false;
    }


    /// An instrumentation policy for code that will try to test interrupt
    /// delaying.
    struct interrupt_nop_policy : public granary::instrumentation_policy {
    public:

        /// Instruction a basic block.
        static granary::instrumentation_policy visit_basic_block(
            granary::cpu_state_handle &,
            granary::thread_state_handle &,
            granary::basic_block_state &,
            granary::instruction_list &ls
        ) throw() {
            using namespace granary;

            instruction in(ls.prepend(label_()));
            instruction loop_head(label_());

            in = ls.insert_after(in, push_(reg::rax));
            in.begin_delay_region();
            in = ls.insert_after(in, pushf_());
            in = ls.insert_after(in, cli_());
            in = ls.insert_after(in, mov_imm_(reg::rax, int32_(100000)));
            in = ls.insert_after(in, loop_head);

            // make this a slow loop ;-)
            in = ls.insert_after(in, pushf_());
            in = ls.insert_after(in, popf_());

            in = ls.insert_after(in, sub_(reg::rax, int32_(1)));
            in = ls.insert_after(in, jnz_(instr_(loop_head)));
            in = ls.insert_after(in, popf_());
            // interrupt will likely be here
            in = ls.insert_after(in, pop_(reg::rax));
            in.end_delay_region();

            return granary::policy_for<interrupt_nop_policy>();
        }

        /// Handle an interrupt in module code. Returns true iff the client
        /// handles the interrupt.
        static granary::interrupt_handled_state handle_interrupt(
            granary::cpu_state_handle &,
            granary::thread_state_handle &,
            granary::basic_block_state &,
            granary::interrupt_stack_frame &,
            granary::interrupt_vector
        ) throw() {
            interrupted = true;
            return granary::INTERRUPT_DEFER;
        }
    };


    /// Tight loop; instrumentation will add a lot of NOPs, so we hope it will
    /// (eventually) be interrupted.
    static void tight_loop(void) throw() {
        while(!interrupted) {
            ASM("nop;");
        }
    }


    /// Test that the number of delayed interrupts eventually changes when
    /// executing a tight loop.
    static void test_delay_interrupt(void) throw() {
        const unsigned long ndi(granary::perf::num_delayed_interrupts());

        granary::app_pc func((granary::app_pc) tight_loop);
        granary::basic_block bb_func(granary::code_cache::find(
            func, granary::policy_for<interrupt_nop_policy>()));

        for(unsigned i(0);
            i < 100 && ndi == granary::perf::num_delayed_interrupts();
            ++i) {
            bb_func.call<void>();
        }

        ASSERT(interrupted);
        ASSERT(ndi < granary::perf::num_delayed_interrupts());
    }


    ADD_TEST(test_delay_interrupt,
        "Test that policy-specific interrupt handlers are invoked when "
        "instrumented code is interrupted, and that interrupt delaying is "
        "performed within interrupt delay regions.")
}

#endif

