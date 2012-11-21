/*
 * module.cc
 *
 *  Created on: 2012-11-08
 *      Author: pag
 *     Version: $Id$
 */

#include "granary/utils.h"
#include "granary/instruction.h"

#include "granary/kernel/module.h"
#include "granary/kernel/printf.h"
#include "granary/kernel/linux/types.h"

extern "C" {
    void notify_module_state_change(struct kernel_module *module) {
        if(module->is_granary) {
            return;
        }

        kernel::module *mod(granary::unsafe_cast<kernel::module *>(module->address));

        granary::printf("Notified about module (%s) state change!!\n", mod->name);

        (void) mod;
    }
}
