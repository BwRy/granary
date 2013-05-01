/* Copyright 2012-2013 Peter Goodman, all rights reserved. */
/*
 * test_random.cc
 *
 *  Created on: 2013-05-01
 *      Author: Peter Goodman
 */

#include "granary/test.h"

#if CONFIG_RUN_TEST_CASES

#include "clients/watchpoints/policies/null_policy.h"
#include "clients/watchpoints/tests/pp.h"

namespace test {

    extern "C" {
        static uint64_t WP_R_TEST = ~0ULL;
    }

    /// From: python-dbg, _PyObject_DebugDumpAddress+581
    static uint32_t test_movzbl(void) throw() {
        register uint32_t ret = 0;
        ASM(
            "movq $WP_R_TEST, %%rax;"
            "movzbl (%%rax), %%eax;"
            "cmp $0xfb, %%al;"
            "jmp 1f;"
         "1: mov %%eax, %0;"
            : "=r"(ret)
            :
            : "%rax"
        );
        return ret;
    }

    /// Test that MOV instructions are correctly watched.
    static void random_tests_watched_correctly(void) {
        (void) WP_R_TEST;

        // Simple un/watched, no flags dependencies, no register dependencies.

        granary::app_pc movzbl((granary::app_pc) test_movzbl);
        granary::basic_block call_movzbl(granary::code_cache::find(
            movzbl, granary::policy_for<client::watchpoint_null_policy>()));

        WP_R_TEST = ~0ULL;
        ASSERT(0xFF == call_movzbl.call<uint32_t>());
    }

    ADD_TEST(random_tests_watched_correctly,
        "Check that interesting instructions encountered in the wild are "
        "correctly watched.")
}

#endif /* CONFIG_RUN_TEST_CASES */
