/* Copyright 2012-2013 Peter Goodman, all rights reserved. */
/*
 * bb.h
 *
 *      Author: Peter Goodman
 */

#ifndef granary_BB_H_
#define granary_BB_H_

#include "granary/globals.h"
#include "granary/policy.h"
#include "granary/detach.h"

namespace granary {

    /// Forward declarations.
    struct basic_block;
    struct basic_block_state;
    struct instruction_list;
    struct instruction;
    struct cpu_state_handle;
    struct thread_state_handle;
    struct instrumentation_policy;
    struct code_cache;


    /// different states of bytes in the code cache.
    enum code_cache_byte_state {
        BB_BYTE_NATIVE         = (1 << 0),
        BB_BYTE_DELAY_BEGIN    = (1 << 1),
        BB_BYTE_DELAY_CONT     = (1 << 2),
        BB_BYTE_DELAY_END      = (1 << 3)
    };


    /// Defines the meta-information block that ends each basic block in the
    /// code cache.
    struct basic_block_info {
    public:

        enum {
            HEADER = 0xD4D5D682
        };

        /// magic number (sequence of 4 int3 instructions) which signals the
        /// beginning of a bb_meta block.
        uint32_t magic;

        /// Number of bytes in this basic block, *including* the number of
        /// bytes of padding, and the patch bytes.
        uint16_t num_bytes;

        /// Number of bytes of patch instructions beginning this basic block.
        uint16_t num_patch_bytes:15;

        /// Does this basic block have any state bits following it? It will only
        /// have state bits of there is a delay region.
        bool has_delay_range:1;

        /// Represents the translation policy used to translate this basic
        /// block. This includes policy properties.
        uint16_t policy_bits;

        /// Number of bytes of instructions in the generating basic block.
        uint16_t generating_num_bytes;

#if GRANARY_IN_KERNEL
        /// Low 32 bits of the basic block state address.
        uint32_t state_addr_low_32;
#else
        basic_block_state *state_addr;
#endif
        /// The native pc that "generated" the instructions of this basic block.
        /// That is, if we decoded and instrumented some basic block starting at
        /// pc X, then the generating pc is X.
        uintptr_t generating_pc;

    } __attribute__((packed));


#if GRANARY_IN_KERNEL
    /// Used for packing/unpacking a basic block state address from a basic
    /// block info address.
    union basic_block_state_address {
        struct {
            uint32_t low;
            uint32_t high;
        } __attribute__((packed));
        basic_block_state *state_addr;
        basic_block_info *info_addr;
    } __attribute__((packed));
#endif


    /// Represents a basic block. Basic blocks are not concrete objects in the
    /// sense that they are used to build basic blocks; they are an abstraction
    /// imposed on some bytes in the code cache as a convenience.
    struct basic_block {
    public:

        friend struct code_cache;


        /// points to the counting set, where every pair of bits represents the
        /// state of some byte in the code cache; this counting set immediately
        /// follows the info block in memory.
        IF_KERNEL( uint8_t *pc_byte_states; )


        /// The meta information for the specific basic block.
        basic_block_info *info;


    public:


        /// The instrumentation policy of used to translate the code for this
        /// basic block.
        instrumentation_policy policy;


        /// Beginning on native/instrumented instructions within the basic
        /// block. This skips past any emitted stub instructions at the
        /// beginning of the basic block.
        app_pc cache_pc_start;


        /// The current instruction within the basic block. This is the
        /// instruction that was used to "find" and "rebuild" this information
        /// about the basic block from the code cache.
        app_pc cache_pc_current;


        /// The end of the instructions within the basic block. This includes
        /// illegal instructions emitted at the end of the basic block.
        app_pc cache_pc_end;


        /// construct a basic block from a pc that points into the code cache.
        basic_block(app_pc current_pc_) throw();


#if GRANARY_IN_KERNEL
        /// Returns true iff this interrupt must be delayed. If the interrupt
        /// must be delayed then the arguments are updated in place with the
        /// range of code that must be copied and re-relativised in order to
        /// safely execute the interruptible code. The range of addresses is
        /// [begin, end), where the `end` address is the next code cache address
        /// to execute after the interrupt has been handled.
        bool get_interrupt_delay_range(app_pc &, app_pc &) const throw();
#endif


        /// Compute the size of a basic block given an instruction list. This
        /// computes the size of each instruction, the amount of padding, meta
        /// information, etc.
        static unsigned size(instruction_list &) throw();


        /// Compute the size of an existing basic block.
        unsigned size(void) const throw();


        /// Call the code within the basic block as if is a function.
        template <typename R, typename... Args>
        R call(Args... args) throw() {
            typedef R (func_type)(Args...);
            function_call<R, Args...> func(
                unsafe_cast<func_type *>(cache_pc_start));
            func(args...);
#if !CONFIG_ENABLE_DIRECT_RETURN
            detach();
#endif
            return func.yield();
        }


    protected:


        typedef void (client_instrumenter)(
            cpu_state_handle cpu,
            basic_block_state *bb,
            instruction_list &ls);


        /// Decode and translate a single basic block of application/module code.
        static basic_block translate(
            instrumentation_policy policy,
            cpu_state_handle cpu,
            app_pc start_pc
        ) throw();


        /// Emit an instruction list as code into a byte array. This will also
        /// emit the basic block meta information and local storage.
        ///
        /// Note: it is assumed that pc is well-aligned, e.g. to an 8 or 16 byte
        ///       boundary.
        ///
        /// Note: it is assumed that enough space has been allocated for the
        ///       instructions and the basic block meta-info, etc.
        ///
        /// Args:
        ///     policy:         The policy of this basic block.
        ///     ls:             The instructions to encode.
        ///     generating_pc:  The program PC whose decoding/translation
        ///                     generated the instruction list ls.
        ///     generated_pc:   A pointer to the memory location where we will
        ///                     store this basic block. When the block is
        ///                     emitted, this pointer is updated to the address
        ///                     of the memory location immediately following
        ///                     the basic block.
        static app_pc emit(
            instrumentation_policy kind,
            instruction_list &ls,
            instruction bb_begin,
            basic_block_state *block_storage,
            app_pc generating_pc,
            unsigned byte_len,
            app_pc generated_pc
        ) throw();


    public:

        /// Return a pointer to the basic block state structure of this basic
        /// block.
        inline basic_block_state *state(void) const throw() {
#if GRANARY_IN_KERNEL
            basic_block_state_address info_addr;
            info_addr.info_addr = info;
            info_addr.low = info->state_addr_low_32;
            return info_addr.state_addr;
#else
            return info->state_addr;
#endif
        }
    };

}

#endif /* granary_BB_H_ */
