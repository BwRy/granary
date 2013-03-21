/*
 * policy.cc
 *
 *  Created on: 2013-01-24
 *      Author: pag
 */

#include "granary/globals.h"
#include "granary/policy.h"
#include "granary/mangle.h"

#include "clients/instrument.h"

namespace granary {


    /// Policy basic block visitor functions for each policy. The code
    /// cache will use this array of function pointers (initialised
    /// partially at compile time and partially at run time) to determine
    /// which client-code basic block visitor functions should be called.
    instrumentation_policy::basic_block_visitor
    instrumentation_policy::POLICY_FUNCTIONS[
        1 << mangled_address::POLICY_NUM_BITS
    ] = {
        &instrumentation_policy::missing_policy
    };


    /// Policy ID tracker.
    std::atomic<unsigned> instrumentation_policy::NEXT_POLICY_ID(
        ATOMIC_VAR_INIT(instrumentation_policy::NUM_POLICIES));


    /// Get the policy for a policy-extended mangled address.
    instrumentation_policy::instrumentation_policy(
        const mangled_address &addr
    ) throw()
        : id(addr.as_policy_address.policy_id)
        , pseudo_id_increment(id % NUM_POLICIES)
        , properties(addr.as_policy_address.policy_properties)
    { }


    /// Mangle an address according to a policy.
    mangled_address::mangled_address(
        app_pc addr_,
        instrumentation_policy policy_
    ) throw()
        : as_address(addr_)
    {
        as_int <<= NUM_MANGLED_BITS;
        as_policy_address.policy_id = policy_.id;
        as_policy_address.policy_properties = policy_.properties;
    }


    /// Extract the original, unmangled address from a mangled address.
    app_pc mangled_address::unmangled_address(void) const throw() {

        /// Used to extract "address space" high-order bits for recovering a
        /// native address.
        union {
            struct {
                uint64_t saved_bits:(64 - NUM_MANGLED_BITS);
                uint8_t lost_bits:NUM_MANGLED_BITS; // high
            } bits __attribute__((packed));
            const void *as_addr;
            uint64_t as_uint;
        } recovery_addr;

        recovery_addr.as_addr = this;
        recovery_addr.bits.saved_bits = 0ULL;

        return reinterpret_cast<app_pc>(
            (as_uint >> NUM_MANGLED_BITS) | recovery_addr.as_uint);
    }


    /// Instrumentation policy for basic blocks where the policy is missing.
    struct missing_policy_policy : public instrumentation_policy {
    public:

        /// Instruction a basic block.
        instrumentation_policy visit_basic_block(
            cpu_state_handle &,
            thread_state_handle &,
            basic_block_state &,
            instruction_list &
        ) throw() {
            return granary::policy_for<missing_policy_policy>();
        }
    };


    /// Function called when a policy is missing (i.e. hasn't been initialised).
    instrumentation_policy instrumentation_policy::missing_policy(
        cpu_state_handle &,
        thread_state_handle &,
        basic_block_state &,
        instruction_list &
    ) throw() {
        granary_break_on_fault();
        granary_fault();
        return policy_for<missing_policy_policy>();
    }


    instrumentation_policy START_POLICY;


    STATIC_INITIALISE_ID(start_policy, {
        START_POLICY = policy_for<decltype(GRANARY_INIT_POLICY)>();
    })
}

