/*
 * instruction.cc
 *
 *   Copyright: Copyright 2012 Peter Goodman, all rights reserved.
 *      Author: Peter Goodman
 */

#include "granary/globals.h"
#include "granary/instruction.h"
#include "granary/gen/instruction.h"

namespace granary {

    /// Operand that represents a PC stored somewhere in memory.
    operand mem_pc_(app_pc *pc) {
        operand op;
        op.kind = dynamorio::BASE_DISP_kind;
        op.value.addr = pc;
        op.size = dynamorio::OPSZ_8;
        op.seg.segment = dynamorio::DR_REG_NULL;
        return op;
    }


    instruction jecxz_(dynamorio::opnd_t t) {
        instruction in__; // need to make sure granary policy is 0
        dynamorio::dcontext_t dc__ = {
            false, /* x86_mode */
            0, /* private code */
            &(in__.instr) /* allocated_instr */
        };
        dynamorio::dcontext_t *dc = &dc__;
        dynamorio::instr_create_0dst_2src(
            (dc),
            dynamorio::OP_jecxz,
            (t),
            dynamorio::opnd_create_reg(dynamorio::DR_REG_ECX)
        );
        return in__;
    }


    /// used frequently in instruction functions
    typename dynamorio::dcontext_t *instruction::DCONTEXT = \
        dynamorio::get_thread_private_dcontext();


    /// constructor
    instruction::instruction(void) throw() {
        memset(this, 0, sizeof *this);
        dynamorio::instr_set_x86_mode(&(this->instr), false);
        instr.flags |= dynamorio::INSTR_HAS_CUSTOM_STUB;
    }


    /// Copy assignment.
    instruction &instruction::operator=(const instruction &&that) throw() {
        if(&that == this) {
            return *this;
        }

        // save key info
        //dynamorio::byte *bytes(instr.bytes);
        //dynamorio::uint eflags(instr.eflags);

        app_pc translation(instr.translation);
        void *note(instr.note);
        dynamorio::instr_t *next(instr.next);
        dynamorio::instr_t *prev(instr.prev);
        uint8_t granary_flags(instr.granary_flags);
        uint8_t granary_policy(instr.granary_flags);

        memcpy(&instr, &(that.instr), sizeof instr);

        // TODO:
        // will need to decide what the best approach is here; there are
        // interesting interactions when dealing with nop2byte_, which manually
        // allocates is bits, and when such a nop is (stage)encoded and then
        // assigned to another instruction.

        // restore key info
        //instr.bytes = bytes;
        //instr.eflags = eflags;
        instr.translation = translation;
        instr.note = note;
        instr.next = next;
        instr.prev = prev;
        instr.granary_flags = granary_flags;
        instr.granary_policy = granary_policy;

        return *this;
    }


    /// Encodes an instruction into a sequence of bytes.
    app_pc instruction::encode(app_pc pc_) throw() {

        // address calculation for relative jumps uses the note
        // field; second-pass encoding for hot patchable instructions
        // takes advantage of this
        instr.note = pc_;
        app_pc ret(dynamorio::instr_encode(DCONTEXT, &instr, pc_));

        instr.translation = pc_;
        return ret;
    }


    /// Encodes an instruction into a sequence of bytes, but where the staging
    /// ground is not necessarily the instruction's final location.
    app_pc instruction::stage_encode(app_pc staged_pc, app_pc final_pc) throw() {

        // address calculation for relative jumps uses the note
        // field; second-pass encoding for hot patchable instructions
        // takes advantage of this
        instr.note = final_pc;

        return dynamorio::instr_encode_to_copy(
            DCONTEXT, &instr, staged_pc, final_pc);
    }


    /// Implicit constructor for registers.
    operand::operand(typename dynamorio::reg_id_t reg_) throw() {
        *this = dynamorio::opnd_create_reg(reg_);
    }

    /// De-reference an address stored in a register
    operand_base_disp operand::operator*(void) const throw() {
        operand_base_disp op;
        op.base = value.reg;
        op.size = size;
        return op;
    }


    /// Accessing some byte offset from the operand (assuming it points to
    /// some memory).
    operand_base_disp operand::operator[](int64_t num_bytes) const throw() {
        operand_base_disp op;
        op.base = value.reg;
        op.disp = num_bytes;
        op.size = size;
        return op;
    }


    operand_base_disp::operand_base_disp(void) throw()
        : base(dynamorio::DR_REG_NULL)
        , index(dynamorio::DR_REG_NULL)
        , size(dynamorio::OPSZ_lea)
        , scale(1)
        , disp(0)
    { }


    /// Add an index into an lea operand
    operand_base_disp operand::operator+(operand index) const throw() {
        operand_base_disp ret;
        ret.base = value.reg;
        ret.index = index.value.reg;
        ret.scale = 1;
        ret.disp = 0;
        return ret;
    }


    /// Add in the base register to this LEA operand.
    operand_base_disp operand::operator+(operand_base_disp lea) const throw() {
        lea.base = value.reg;
        return lea;
    }


    /// Initialise the operand ref with an instruction and operand pointer
    operand_ref::operand_ref(instruction *instr_, dynamorio::opnd_t *op_) throw()
        : instr(instr_)
        , op(unsafe_cast<operand *>(op_))
    { }


    /// Assume that a non-const access of a field of the op will be used
    /// as an lvalue in an assignment; invalidate the raw bits.
    operand *operand_ref::operator->(void) throw() {
        instr->invalidate_raw_bits();
        return op;
    }


    /// Assign an operand to this operand ref; this will update the operand
    /// referenced by this ref in place, and will invalidate the raw bits
    /// of the instruction.
    operand_ref &operand_ref::operator=(operand that) throw() {
        *op = that;
        instr->invalidate_raw_bits();
        return *this;
    }

    operand_ref &operand_ref::operator=(operand_base_disp that) throw() {
        *op = operand(that);
        instr->invalidate_raw_bits();
        return *this;
    }


    /// encodes an instruction list into a sequence of bytes
    app_pc instruction_list::encode(app_pc start_pc) throw() {
        if(!length()) {
            return start_pc;
        }

        handle_type item(first());
        app_pc prev_pc(nullptr);
        app_pc pc(start_pc);
        bool has_local_jump(false);

        for(unsigned i = 0, max = length(); i < max; ++i) {

            dynamorio::instr_t *target_instr(nullptr);

            if(item->is_cti()) {
                operand target(item->cti_target());
                if(dynamorio::opnd_is_instr(target)) {
                    target_instr = target.value.instr;
                    item->instr.u.o.src0.value.instr = *item;
                    has_local_jump = true;
                }
            }

            prev_pc = pc;
            pc = item->encode(pc);

            IF_DEBUG(nullptr == pc,
                granary_break_on_encode(prev_pc, *item));

            // restore its correct target for later jump resolution.
            if(target_instr) {
                item->instr.u.o.src0.value.instr = target_instr;
            }

            IF_PERF( perf::visit_encoded(*item); )

            item = item.next();
        }

        // local jumps within the same basic block might be forward jumps
        // (at least in the case of direct call/jump stubs); re-emit those
        // instructions in place with the now-resolved PCs.
        if(has_local_jump) {
            item = first();
            for(unsigned i = 0, max = length(); i < max; ++i) {
                if(item->is_cti()) {
                    operand target(item->cti_target());
                    if(dynamorio::opnd_is_instr(target)) {
                        item->encode(reinterpret_cast<app_pc>(
                            item->instr.note));
                    }
                }
                item = item.next();
            }
        }

        return pc;
    }

    /// Performs a staged encoding of an instruction list into a sequence
    /// of bytes.
    ///
    /// Note: This will not do any fancy jump resolution, alignment, etc.
    app_pc instruction_list::stage_encode(app_pc staged_pc, app_pc final_pc) throw() {
        if(!length()) {
            return staged_pc;
        }

        handle_type item(first());
        for(unsigned i = 0, max = length(); i < max; ++i) {
            app_pc prev_staged_pc(staged_pc);
            staged_pc = item->stage_encode(staged_pc, final_pc);
            final_pc += staged_pc - prev_staged_pc;
            item = item.next();
        }

        return staged_pc;
    }


    /// Widen this instruction if its a CTI.
    void instruction::widen_if_cti(void) throw() {

        // hack for getting dynamorio to maintain proper rip-relative
        // associations in the encoding process.
        if(dynamorio::instr_is_cti(&instr)) {
            dynamorio::instr_set_raw_bits_valid(&instr, false);

            if(dynamorio::instr_is_cti_short(&instr)) {
                dynamorio::convert_to_near_rel_common(
                    DCONTEXT, nullptr, &instr);
            }
        }
    }


    /// Decodes a raw byte, pointed to by *pc, and updated *pc to be the
    /// following byte. The decoded instruction is returned by value. If
    /// the instruction cannot be decoded, then *pc is set to NULL.
    instruction instruction::decode(app_pc *pc) throw() {
        instruction self;
        uint8_t *byte_pc(unsafe_cast<uint8_t *>(*pc));

        *pc = dynamorio::decode_raw(DCONTEXT, byte_pc, &(self.instr));
        dynamorio::decode(DCONTEXT, byte_pc, &(self.instr));

        // keep these associations around
        self.instr.translation = byte_pc;
        self.instr.bytes = byte_pc;

        IF_PERF( perf::visit_decoded(self); )

        self.widen_if_cti();

        return self;
    }


    /// The encoded size of the instruction list.
    unsigned instruction_list::encoded_size(void) throw() {
        auto in = first();
        unsigned size(0U);
        for(unsigned i = 0, max = length(); i < max; ++i) {
            size += in->encoded_size();
            in = in.next();
        }
        return size;
    }


#define MAKE_REG(name, upper_name) operand name = \
    dynamorio::opnd_create_reg(dynamorio::DR_ ## upper_name);

    namespace reg {
#   include "granary/inc/registers.h"
    }
#undef MAKE_REG

}

