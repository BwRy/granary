/*
 * test_sigsetjmp.cc
 *
 *  Created on: 2013-02-15
 *      Author: pag
 *     Version: $Id$
 */

extern "C" {
#include <setjmp.h>
}

#include "granary/test.h"

namespace test {

    static int before_longjmp(0);
    static int after_longjmp(0);

    static int after_setjmp(0);
    static int after_unroll(0);

    static int after_rollback(0);

    static int after_all(0);

    static void rollback(sigjmp_buf &env) throw() {
        ++before_longjmp;
        siglongjmp(env, 1);
        ++after_longjmp;
    }

    static void jmp_loc(void) throw() {
        sigjmp_buf env;

        if(!sigsetjmp(env, 0)) {
            ++after_setjmp;
            rollback(env);
            ++after_rollback;
        } else {
            ++after_unroll;
        }

        if(!sigsetjmp(env, 1)) {
            ++after_setjmp;
            rollback(env);
            ++after_rollback;
        } else {
            ++after_unroll;
        }

        ++after_all;

        printf("hi\n");
        FILE *ff = fopen("./tmp.temp", "w");
        fprintf(ff, "hello world!");
        fclose(ff);
    }

    static void test_sigsetjmp(void) {

        granary::app_pc func((granary::app_pc) jmp_loc);
        granary::basic_block bb_func(granary::code_cache::find(
            func, granary::test_policy()));

        bb_func.call<void>();

        ASSERT(2 == before_longjmp);
        ASSERT(0 == after_longjmp);
        ASSERT(2 == after_setjmp);
        ASSERT(2 == after_unroll);
        ASSERT(0 == after_rollback);
        ASSERT(1 == after_all);
    }


    ADD_TEST(test_sigsetjmp,
        "Test that sigsetjmp works.");
}
