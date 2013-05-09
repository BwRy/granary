/* Copyright 2012-2013 Peter Goodman, all rights reserved. */
/*
 * bound_policy.asm
 *
 *  Created on: 2013-05-07
 *      Author: Peter Goodman
 */

#include "granary/x86/asm_defines.asm"
#include "granary/x86/asm_helpers.asm"
#include "granary/pp.h"

#include "clients/watchpoints/config.h"

    .extern SYMBOL(_ZN6client2wp11DESCRIPTORSE)
    .extern SYMBOL(_ZN6client2wp12bound_policy14visit_overflowEmPPhj)

START_FILE

/// Watchpoint descriptor.
#define DESC(reg) SPILL_REG_NOT_RAX(reg)


/// Table address, as well as temporary for calculating the descriptor index.
#define TABLE(reg) SPILL_REG_NOT_RAX(DESC(reg))


/// Left shift for partial indexes to get the partial index into the high
/// position.
#define PARTIAL_INDEX_LSH \
    (64 - (WP_PARTIAL_INDEX_WIDTH + WP_PARTIAL_INDEX_GRANULARITY))


/// Right shift for partial indexes to get it into the low position.
#define PARTIAL_INDEX_RSH (64 - WP_PARTIAL_INDEX_WIDTH)


/// Right shift for counter indexes to get it into the low position.
#define COUNTER_INDEX_RSH (64 - (WP_COUNTER_INDEX_WIDTH - 1))


/// Define a register-specific and operand-size specific bounds checker.
#define BOUNDS_CHECKER(reg, size) \
    DECLARE_FUNC(CAT(CAT(granary_bounds_check_, CAT(size, _)), reg)) @N@\
    GLOBAL_LABEL(CAT(CAT(granary_bounds_check_, CAT(size, _)), reg):) @N@@N@\
        .cfi_startproc @N@\
        \
        COMMENT(Spill some registers. The DEST and TABLE registers are) \
        COMMENT(guaranteed to not be RAX. We unconditionally spill RAX) \
        COMMENT(because we will clobber its low order bits with the) \
        COMMENT(arithmetic flags.) \
        pushq %rax;@N@ .cfi_def_cfa_offset 16 @N@\
        pushq %DESC(reg);@N@ .cfi_def_cfa_offset 24 @N@\
        pushq %TABLE(reg);@N@ .cfi_def_cfa_offset 32 @N@\
        @N@\
        \
        COMMENT(Save the register so we can restore it later when we need to) \
        COMMENT(compare its lower order bits against the bounds.) \
        pushq %reg;@N@ .cfi_def_cfa_offset 40 @N@\
        @N@\
        \
        COMMENT(Save the watched address into a non-RAX reg before we clobber) \
        COMMENT(RAX for saving the flags.) \
        mov %reg, %DESC(reg);@N@\
        IF_PARTIAL_INDEX( mov %reg, %TABLE(reg);@N@ )\
        @N@\
        \
        COMMENT(Save the flags.) \
        lahf;@N@\
        seto %al;@N@\
        @N@\
        \
        COMMENT(Compute the combined index.) \
        IF_PARTIAL_INDEX( shl $ PARTIAL_INDEX_LSH, %TABLE(reg);@N@ ) \
        IF_PARTIAL_INDEX( shr $ PARTIAL_INDEX_RSH, %TABLE(reg);@N@ ) \
        shr $ COUNTER_INDEX_RSH, %DESC(reg);@N@ \
        IF_PARTIAL_INDEX( shl $ WP_PARTIAL_INDEX_WIDTH, %DESC(reg);@N@ ) \
        IF_PARTIAL_INDEX( or %TABLE(reg), %DESC(reg);@N@ ) \
        @N@\
        \
        COMMENT(Get the descriptor pointer from the index.) \
        lea SYMBOL(_ZN6client2wp11DESCRIPTORSE)(%rip), %TABLE(reg);@N@\
        lea (%TABLE(reg),%DESC(reg),8), %DESC(reg);@N@\
        popq %TABLE(reg);@N@ .cfi_def_cfa_offset 32 @N@\
        movq (%DESC(reg)), %DESC(reg);@N@\
        @N@\
        \
        COMMENT(Compare against the lower bound.) \
        cmpw (%DESC(reg)), %REG64_TO_REG16(TABLE(reg));@N@\
        jl .CAT(Lbounds_check_fail_, CAT(reg, size));@N@\
        @N@\
        \
        COMMENT(Compare against the upper bound.) \
        addq $ (size - 1), %TABLE(reg); \
        cmpw %REG64_TO_REG16(TABLE(reg)), 2(%DESC(reg));@N@\
        jle .CAT(Lbounds_check_fail_, CAT(reg, size));@N@\
        jmp .CAT(Lbounds_check_passed_, CAT(reg, size));@N@\
        @N@\
        \
    .CAT(Lbounds_check_fail_, CAT(reg, size)): @N@\
        COMMENT(Trigger an overflow exception if a bounds check fails.) \
        call CAT(granary_overflow_handler_, CAT(CAT(size, _), reg)); \
        @N@\
        \
    .CAT(Lbounds_check_passed_, CAT(reg, size)): @N@\
        COMMENT(Restore the flags.) \
        addb $0x7F, %al;@N@\
        sahf;@N@\
        @N@\
        \
        COMMENT(Unspill.) \
        popq %TABLE(reg);@N@ .cfi_def_cfa_offset 24 @N@\
        popq %DESC(reg);@N@ .cfi_def_cfa_offset 16 @N@\
        popq %rax;@N@ .cfi_def_cfa_offset 8 @N@\
        ret;@N@\
        .cfi_endproc @N@\
    END_FUNC(CAT(CAT(granary_bounds_check_, CAT(size, _)), reg)) @N@@N@@N@


/// An overflow handler.
#define OVERFLOW_HANDLER(reg, size) \
    DECLARE_FUNC(CAT(granary_overflow_handler_, CAT(CAT(size, _), reg))) @N@\
    GLOBAL_LABEL(CAT(granary_overflow_handler_, CAT(CAT(size, _), reg)):) @N@\
        .cfi_startproc @N@\
        \
        COMMENT(Save all register state.)\
        pushf; @N@\
        @PUSH_ALL_REGS@ @N@\
        .cfi_def_cfa_offset 136; @N@\
        @N@\
        \
        COMMENT(Save the overflowed address and the return address as)\
        COMMENT(arguments for a common overflow handler.)\
        movq %reg, %ARG1; @N@\
        lea -144(%rsp), %ARG2; @N@\
        movq $ size, %ARG3; @N@\
        @N@\
        \
        COMMENT(Align the stack.)\
        push %rsp; @N@\
        push (%rsp); @N@\
        and $-0x10, %rsp; @N@\
        @N@\
        \
        COMMENT(Call out to our common handler.)\
        call SYMBOL(_ZN6client2wp12bound_policy14visit_overflowEmPPhj); @N@\
        @N@\
        \
        COMMENT(Unalign the stack.)\
        mov 8(%rsp), %rsp; @N@\
        @N@\
        \
        COMMENT(Restore all register state.)\
        @POP_ALL_REGS@ @N@\
        popf; @N@\
        .cfi_def_cfa_offset 8; @N@\
        ret;@N@\
        .cfi_endproc @N@\
    END_FUNC(CAT(granary_overflow_, CAT(CAT(size, _), reg))) @N@\
    @N@


/// Define a bounds checker and splat the rest of the checkers.
#define DEFINE_CHECKERS(reg, rest) \
    DEFINE_CHECKER(reg) \
    rest


/// Define the last bounds checker.
#define DEFINE_CHECKER(reg) \
    BOUNDS_CHECKER(reg, 1) \
    BOUNDS_CHECKER(reg, 2) \
    BOUNDS_CHECKER(reg, 4) \
    BOUNDS_CHECKER(reg, 8) \
    BOUNDS_CHECKER(reg, 16)


/// Define an overflow handler and splat the rest of the checkers.
#define DEFINE_HANDLERS(reg, rest) \
    DEFINE_HANDLER(reg) \
    rest


/// Define the last overflow handler.
#define DEFINE_HANDLER(reg) \
    OVERFLOW_HANDLER(reg, 1) \
    OVERFLOW_HANDLER(reg, 2) \
    OVERFLOW_HANDLER(reg, 4) \
    OVERFLOW_HANDLER(reg, 8) \
    OVERFLOW_HANDLER(reg, 16)


/// Define all of the bounds checkers.
GLOBAL_LABEL(granary_first_bounds_checker:)
ALL_REGS(DEFINE_CHECKERS, DEFINE_CHECKER)
GLOBAL_LABEL(granary_last_bounds_checker:)

ALL_REGS(DEFINE_HANDLERS, DEFINE_HANDLER)

END_FILE
