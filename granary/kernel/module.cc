/*
 * module.cc
 *
 *  Created on: 2012-11-08
 *      Author: pag
 *     Version: $Id$
 */

#include "granary/globals.h"
#include "granary/policy.h"
#include "granary/code_cache.h"
#include "granary/basic_block.h"
#include "granary/attach.h"
#include "granary/detach.h"
#include "granary/perf.h"
#include "clients/instrument.h"


#include "granary/kernel/module.h"
#include "granary/kernel/printf.h"

#ifdef GRANARY_DONT_INCLUDE_CSTDLIB
#   undef GRANARY_DONT_INCLUDE_CSTDLIB
#endif

#include "granary/types.h"

int foo(void) throw() {
    return 10;
}

extern "C" {
    void notify_module_state_change(struct kernel_module *module) {
        using namespace granary;

        if(module->is_granary) {

            granary::printf("        Module is Granary; initialising...\n");
            granary::init();
            int x = 0;
            auto policy(GRANARY_INIT_POLICY);
            granary::attach(granary::policy_for<decltype(policy)>());
            x = foo();
            granary::detach();

            granary::printf("x=%d\n", x);
            return;
        }

        types::module *mod(unsafe_cast<types::module *>(module->address));

        granary::printf("        Notified about module (%s) state change!!\n", mod->name);


        (void) mod;
    }
}
