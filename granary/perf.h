/* Copyright 2012-2013 Peter Goodman, all rights reserved. */
/*
 * perf.h
 *
 *  Created on: 2013-02-13
 *      Author: pag
 *     Version: $Id$
 */

#ifndef Granary_PERF_H_
#define Granary_PERF_H_

#include "granary/globals.h"

#if CONFIG_ENABLE_PERF_COUNTS

namespace granary {

    /// Forward declarations.
    struct instruction;
    struct instruction_list;
    struct basic_block;

    struct perf {

        static void visit_decoded(instruction ) throw();
        static void visit_encoded(instruction ) throw();
        static void visit_encoded(basic_block &) throw();

        static void visit_mangle_indirect_jmp(void) throw();
        static void visit_mangle_indirect_call(void) throw();
        static void visit_mangle_return(void) throw();

        static void visit_ibl_stub(unsigned) throw();
        static void visit_ibl(instruction_list &) throw();
        static void visit_ibl_exit(instruction_list &) throw();

        static void visit_dbl(instruction_list &) throw();
        static void visit_dbl_patch(instruction_list &) throw();
        static void visit_dbl_stub(unsigned) throw();

        static void visit_rbl(instruction_list &) throw();

        static void visit_mem_ref(unsigned) throw();

        static void visit_align_nop(void) throw();

        static void visit_address_lookup(void) throw();
        static void visit_address_lookup_hit(void) throw();
        static void visit_address_lookup_cpu(bool) throw();

#if GRANARY_IN_KERNEL
        static void visit_interrupt(void) throw();
        static void visit_recursive_interrupt(void) throw();
        static void visit_delayed_interrupt(void) throw();
        static unsigned long num_delayed_interrupts(void) throw();
        static void visit_protected_module(void) throw();
#endif

        static void report(void) throw();
    };

}

#endif /* CONFIG_ENABLE_PERF_COUNTS */

#endif /* Granary_PERF_H_ */
