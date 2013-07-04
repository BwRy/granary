/* Copyright 2012-2013 Peter Goodman, all rights reserved. */
/*
 * instrument.cc
 *
 *  Created on: 2013-06-28
 *      Author: Peter Goodman
 */

#include "clients/cfg/instrument.h"
#include "clients/cfg/events.h"

using namespace granary;


#if GRANARY_IN_KERNEL
#   include "granary/kernel/linux/module.h"
extern "C" {
    /// Returns the kernel module information for a given app address.
    extern const kernel_module *kernel_get_module(app_pc addr);
}
#endif


namespace client {


    /// Used to link together all basic blocks.
    std::atomic<basic_block_state *> BASIC_BLOCKS = \
        ATOMIC_VAR_INIT(nullptr);


    /// Entry point for the on-enter-function event handler.
    static app_pc EVENT_ENTER_FUNCTION = nullptr;


    /// Exit point for the on-exit-function event handler.
    static app_pc EVENT_EXIT_FUNCTION = nullptr;


    /// Exit point for the on-exit-function event handler.
    static app_pc EVENT_AFTER_FUNCTION = nullptr;


    /// Entry point for the on-enter-bb event handler.
    static app_pc EVENT_ENTER_BASIC_BLOCK = nullptr;


    /// Unique IDs for basic blocks.
    static std::atomic<unsigned> BASIC_BLOCK_ID = ATOMIC_VAR_INIT(0);


    /// Argument registers.
    static operand ARGS[5];


    /// Generate an entry point function that saves/restores the appropriate
    /// registers.
    template <typename R, typename... Args>
    static app_pc generate_entry_point(R (*func)(Args...)) throw() {
        app_pc func_pc(unsafe_cast<app_pc>(func));
        register_manager dead_regs(find_used_regs_in_func(func_pc));

        for(unsigned i(sizeof...(Args)); i--; ) {
            dead_regs.revive(ARGS[i]);
        }

        instruction_list ls;
        instruction in(ls.append(label_()));

        in = save_and_restore_registers(dead_regs, ls, in);
        in = insert_align_stack_after(ls, in);
        in = insert_cti_after(
            ls, in,
            func_pc, false, operand(),
            CTI_CALL);
        in.set_mangled();
        in = insert_restore_old_stack_alignment_after(ls, in);
        ls.append(ret_());

        app_pc entry_point_pc(global_state::FRAGMENT_ALLOCATOR->\
            allocate_array<uint8_t>(ls.encoded_size()));

        ls.encode(entry_point_pc);

        return entry_point_pc;
    }


    /// Initialise the control-flow graph client.
    void init(void) throw() {
        ARGS[0] = reg::arg1;
        ARGS[1] = reg::arg2;
        ARGS[2] = reg::arg3;
        ARGS[3] = reg::arg4;
        ARGS[4] = reg::arg5;

        EVENT_ENTER_FUNCTION = generate_entry_point(&event_enter_function);
        EVENT_EXIT_FUNCTION = generate_entry_point(&event_exit_function);
        EVENT_AFTER_FUNCTION = generate_entry_point(&event_after_function);
        EVENT_ENTER_BASIC_BLOCK = generate_entry_point(&event_enter_basic_block);
    }


    /// Base case for adding instructions that pass arguments to an event
    /// handler.
    static instruction add_event_args(
        instruction_list &, instruction in, unsigned
    ) throw() {
        return in;
    }


    /// Inductive step for adding arguments that pass arguments to an event
    /// handler.
    template <typename Arg, typename... Args>
    static instruction add_event_args(
        instruction_list &ls,
        instruction in,
        unsigned i, Arg arg,
        Args... args
    ) throw() {
        in = ls.insert_after(in, mov_imm_(
            ARGS[i],
            int64_(reinterpret_cast<uint64_t>(arg))));
        return add_event_args(ls, in, i + 1, args...);
    }


    /// Add a call to an event handler that passes in the basic block state.
    template <typename... Args>
    void add_event_call(
        instruction_list &ls,
        instruction in,
        app_pc event_handler,
        Args... args
    ) throw() {
        IF_USER( in = ls.insert_after(in,
            lea_(reg::rsp, reg::rsp[-REDZONE_SIZE])); )

        // Spill argument registers.
        for(unsigned i(0); i < sizeof...(Args); ++i) {
            in = ls.insert_after(in, push_(ARGS[i]));
        }

        // Assign input variables (assumed to be integrals) to the argument
        // registers.
        in = add_event_args(ls, in, 0, args...);

        // Add a call out to an event handler.
        in = insert_cti_after(
            ls, in,
            event_handler, false, operand(),
            CTI_CALL);
        in.set_mangled();

        // Pop any pushed argument registers.
        for(unsigned i(sizeof...(Args)); i--; ) {
            in = ls.insert_after(in, pop_(ARGS[i]));
        }

        IF_USER( in = ls.insert_after(in,
            lea_(reg::rsp, reg::rsp[REDZONE_SIZE])); )
    }


    /// Chain the basic block into the list of basic blocks.
    void commit_to_basic_block(basic_block_state &bb) throw() {
        basic_block_state *prev(nullptr);
        basic_block_state *curr(&bb);
        do {
            prev = BASIC_BLOCKS.load();
            curr->next = prev;
        } while(!BASIC_BLOCKS.compare_exchange_weak(prev, curr));
    }


    /// Invoked if/when Granary discards a basic block (e.g. due to a race
    /// condition when two cores compete to translate the same basic block).
    void discard_basic_block(basic_block_state &) throw() { }


    /// Initialise the basic block state and add in calls to event handlers.
    static void instrument_basic_block(
        granary::basic_block_state &bb,
        instruction_list &ls
    ) throw() {
        instruction in(ls.last());
        register_manager used_regs;
        register_manager entry_regs;

        bb.num_executions = 0;
        IF_KERNEL( bb.num_interrupts = 0; )

        used_regs.kill_all();
        entry_regs.kill_all();

        IF_KERNEL( app_pc start_pc(nullptr); )
        IF_KERNEL( app_pc end_pc(nullptr); )

        for(instruction prev_in; in.is_valid(); in = prev_in) {

            prev_in = in.prev();

            // Track the registers.
            used_regs.revive(in);
            entry_regs.visit(in);

#if GRANARY_IN_KERNEL
            // Track the translation so that we can find the offset.
            if(in.pc()) {
                start_pc = in.pc();
                if(!end_pc) {
                    end_pc = start_pc;
                }
            }
#endif /* GRANARY_IN_KERNEL */

            // Call, switch into a the entry basic block policy.
            if(in.is_call()) {

                instrumentation_policy target_policy =
                    policy_for<cfg_entry_policy>();
                target_policy.force_attach(true);
                in.set_policy(target_policy);
                add_event_call(ls, in, EVENT_AFTER_FUNCTION, &bb);

            // Returning from a function.
            } else if(in.is_return()) {

                bb.is_function_exit = true;
                in = ls.insert_before(in, label_());
                add_event_call(ls, in, EVENT_EXIT_FUNCTION, &bb);

            // Non-call CTI.
            } else if(in.is_cti()) {
                instrumentation_policy target_policy =
                    policy_for<cfg_exit_policy>();
                target_policy.force_attach(true);
                in.set_policy(target_policy);
            }
        }

        bb.block_id = BASIC_BLOCK_ID.fetch_add(1);
        bb.used_regs = used_regs.encode();
        bb.entry_regs = entry_regs.encode();

#if GRANARY_IN_KERNEL
        // Record the name and relative offset of the code within the
        // module. We can correlate this offset with an `objdump` or do
        // static instrumentation of the ELF/KO based on this offset.
        const kernel_module *module(kernel_get_module(start_pc));
        if(module) {
            const uintptr_t app_begin(
                reinterpret_cast<uintptr_t>(module->text_begin));

            const uintptr_t start_pc_uint(
                reinterpret_cast<uintptr_t>(start_pc));

            bb.app_offset_begin = start_pc_uint - app_begin;

            bb.num_bytes_in_block = reinterpret_cast<uintptr_t>(end_pc) \
                                  - start_pc_uint;

            bb.app_name = module->name;
        }
#endif /* GRANARY_IN_KERNEL */
    }


    granary::instrumentation_policy cfg_entry_policy::visit_app_instructions(
        granary::cpu_state_handle,
        granary::basic_block_state &bb,
        granary::instruction_list &ls
    ) throw() {
        instrument_basic_block(bb, ls);
        bb.is_function_entry = true;
        bb.is_app_code = true;
        bb.function_id = bb.block_id;

        // Add an event handler that executes before the basic block executes.
        instruction in(ls.insert_before(ls.first(), label_()));
        add_event_call(ls, in, EVENT_ENTER_FUNCTION, &bb);

        return policy_for<cfg_exit_policy>();
    }


    granary::instrumentation_policy cfg_entry_policy::visit_host_instructions(
        granary::cpu_state_handle,
        granary::basic_block_state &bb,
        granary::instruction_list &ls
    ) throw() {
        instrument_basic_block(bb, ls);
        bb.is_function_entry = true;
        bb.is_app_code = false;
        bb.function_id = bb.block_id;

        // Add an event handler that executes before the basic block executes.
        instruction in(ls.insert_before(ls.first(), label_()));
        add_event_call(ls, in, EVENT_ENTER_FUNCTION, &bb);

        return granary::policy_for<cfg_exit_policy>();
    }


    granary::instrumentation_policy cfg_exit_policy::visit_app_instructions(
        granary::cpu_state_handle,
        granary::basic_block_state &bb,
        granary::instruction_list &ls
    ) throw() {
        instrument_basic_block(bb, ls);
        bb.is_function_entry = false;
        bb.is_app_code = true;

        // Add an event handler that executes before the basic block executes.
        instruction in(ls.insert_before(ls.first(), label_()));
        add_event_call(ls, in, EVENT_ENTER_BASIC_BLOCK, &bb);

        return policy_for<cfg_exit_policy>();
    }


    granary::instrumentation_policy cfg_exit_policy::visit_host_instructions(
        granary::cpu_state_handle,
        granary::basic_block_state &bb,
        granary::instruction_list &ls
    ) throw() {
        instrument_basic_block(bb, ls);
        bb.is_function_entry = false;
        bb.is_app_code = false;

        // Add an event handler that executes before the basic block executes.
        instruction in(ls.insert_before(ls.first(), label_()));
        add_event_call(ls, in, EVENT_ENTER_BASIC_BLOCK, &bb);

        return granary::policy_for<cfg_exit_policy>();
    }


#if CONFIG_CLIENT_HANDLE_INTERRUPT

    granary::interrupt_handled_state cfg_entry_policy::handle_interrupt(
        granary::cpu_state_handle,
        granary::thread_state_handle,
        granary::basic_block_state &bb,
        granary::interrupt_stack_frame &,
        granary::interrupt_vector
    ) throw() {
        bb.num_interrupts.fetch_add(1);
        return granary::INTERRUPT_DEFER;
    }


    granary::interrupt_handled_state cfg_exit_policy::handle_interrupt(
        granary::cpu_state_handle,
        granary::thread_state_handle,
        granary::basic_block_state &bb,
        granary::interrupt_stack_frame &,
        granary::interrupt_vector
    ) throw() {
        bb.num_interrupts.fetch_add(1);
        return granary::INTERRUPT_DEFER;
    }


    /// Handle an interrupt in kernel code. Returns true iff the client handles
    /// the interrupt.
    granary::interrupt_handled_state handle_kernel_interrupt(
        granary::cpu_state_handle,
        granary::thread_state_handle,
        granary::interrupt_stack_frame &,
        granary::interrupt_vector
    ) throw() {
        return granary::INTERRUPT_DEFER;
    }

#endif

}
