/*
 * bb.cc
 *
 *   Copyright: Copyright 2012 Peter Goodman, all rights reserved.
 *      Author: Peter Goodman
 */

#include "granary/basic_block.h"
#include "granary/instruction.h"
#include "granary/state.h"
#include "granary/detach.h"

#include <cstring>
#include <map>

namespace granary {

    enum {

        BB_PADDING      = 0xCC,
        BB_MAGIC        = 0xCCAACC,
        BB_MAGIC_MASK   = 0xFFFFFF00,

        /// number of byte states (bit pairs) per byte, i.e. we have a 4-to-1
        /// compression ratio of the instruction bytes to the state set bytes
        BB_BYTE_STATES_PER_BYTE = 4,

        /// byte and 32-bit alignment of basic block info structs in memory
        BB_INFO_BYTE_ALIGNMENT = 8,
        BB_INFO_INT32_ALIGNMENT = 2,

        /// misc.
        BITS_PER_BYTE = 8,
        BITS_PER_STATE = BITS_PER_BYTE / BB_BYTE_STATES_PER_BYTE,
        BITS_PER_QWORD = BITS_PER_BYTE * 8,

        /// aligned state size
        STATE_SIZE_ = sizeof(basic_block_state),
        STATE_SIZE = STATE_SIZE_ + ALIGN_TO(STATE_SIZE_, 16)
    };


    /// Get the state of a byte in the basic block.
    static code_cache_byte_state get_state(uint8_t *pc_byte_states,
                                           unsigned i) throw() {
        const unsigned j(i / BB_BYTE_STATES_PER_BYTE); // byte offset
        const unsigned k(2 * (i % BB_BYTE_STATES_PER_BYTE)); // bit offset
        return static_cast<code_cache_byte_state>(
            1 << ((pc_byte_states[j] >> k) & 0x03));
    }


    /// Set one of the states into a trit.
    static void set_state(uint8_t *pc_byte_states,
                          unsigned i,
                          code_cache_byte_state state) throw() {

        static uint8_t state_to_mask[] = {
            0xFF,
            0,  /* BB_BYTE_NATIVE:          1 << 0 */
            1,  /* BB_BYTE_MANGLED:         1 << 1 */
            0xFF,
            2,  /* BB_BYTE_INSTRUMENTED:    1 << 2 */
            0xFF,
            0xFF,
            0xFF,
            3   /* BB_BYTE_PADDING:         1 << 3 */
        };

        const unsigned j(i / BB_BYTE_STATES_PER_BYTE); // byte offset
        const unsigned k(2 * (i % BB_BYTE_STATES_PER_BYTE)); // bit offset
        pc_byte_states[j] &= ~(0x3 << k);
        pc_byte_states[j] |= state_to_mask[state] << k;
    }


    /// Get the state of all bytes of an instruction.
    static code_cache_byte_state get_instruction_state(const instruction in) throw() {
        if(nullptr != in.pc()) {
            return code_cache_byte_state::BB_BYTE_NATIVE;

        // here we take over the INSTR_HAS_CUSTOM_STUB to represent a mangled
        // instruction. Mangling in Granary's case has to do with a jump to a
        // specific basic block that can resolve what to do.
        } else if(in.is_mangled()) {
            return code_cache_byte_state::BB_BYTE_MANGLED;

        } else {
            return code_cache_byte_state::BB_BYTE_INSTRUMENTED;
        }
    }


    /// Set the state of a pair of bits in memory.
    static unsigned initialize_state_bytes(basic_block_info *info,
                                           instruction_list &ls,
                                           app_pc pc) throw() {

        unsigned num_instruction_bits(info->num_bytes * BITS_PER_STATE);
        num_instruction_bits += ALIGN_TO(num_instruction_bits, BITS_PER_QWORD);
        unsigned num_state_bytes = num_instruction_bits / BITS_PER_BYTE;

        // initialize all state bytes to have their states as padding bytes
        memset((void *) pc, static_cast<int>(~0U), num_state_bytes);

        auto in(ls.first());
        unsigned byte_offset(0);

        // for each instruction
        for(unsigned i = 0, max = ls.length(); i < max; ++i) {
            code_cache_byte_state state(get_instruction_state(*in));
            unsigned num_bytes(in->encoded_size());

            // for each byte of each instruction
            for(unsigned j = i; j < (i + num_bytes); ++j) {
                set_state(pc, j, state);
            }

            byte_offset += num_bytes;
            in = in.next();
        }

        return num_state_bytes;
    }

}
    void break_on_bb(uint64_t addr) {
        (void) addr;
    }

namespace granary {

    /// Return the meta information for the current basic block, given some
    /// pointer into the instructions of the basic block.
    basic_block::basic_block(app_pc current_pc_) throw()
        : cache_pc_current(current_pc_)
    {

        // round up to the next 8-byte aligned quantity
        uint64_t addr(reinterpret_cast<uint64_t>(current_pc_));
        unsigned num_bytes = ALIGN_TO(addr, BB_INFO_BYTE_ALIGNMENT);
        addr += num_bytes;

        // go search for the basic block info by finding potential
        // addresses of the meta info, then checking their magic values.
        for(uint64_t *ints(reinterpret_cast<uint64_t *>(addr)); ; ++ints) {
            const basic_block_info * const potential_bb(
                unsafe_cast<basic_block_info *>(ints));

            if(BB_MAGIC == potential_bb->magic) {
                break;
            }

            num_bytes += BB_INFO_BYTE_ALIGNMENT;
        }

        // we've found the basic block info
        info = unsafe_cast<basic_block_info *>(current_pc_ + num_bytes);
        cache_pc_end = cache_pc_current + num_bytes;
        cache_pc_start = cache_pc_end - info->num_bytes;
        state = unsafe_cast<basic_block_state *>(
            cache_pc_start - basic_block_state::size());
        pc_byte_states = reinterpret_cast<uint8_t *>(
            cache_pc_end + sizeof(basic_block_info));
    }


    /// Returns the next safe interrupt location. This is used in the event that
    /// a basic block is interrupted and we need to determine if we have to
    /// delay the interrupt (e.g. across potentially non-reentrant
    /// instrumentation code) or if we can take the interrupt immediately.
    app_pc basic_block::next_safe_interrupt_location(void) const throw() {
        app_pc next(cache_pc_current);

        for(;;) {
            const code_cache_byte_state byte_state(get_state(
                pc_byte_states, next - cache_pc_start));

            // we're in front of a native byte, which is fine.
            if(code_cache_byte_state::BB_BYTE_NATIVE & byte_state) {
                return next;

            // In the case of a mangled instruction, we will follow the discipline
            // that there can only be one mangled instruction in a translated
            // fragment, which is a safe interrupt point.
            //
            // otherwise, we should be in a different kind of basic block
            // altogether, and the interrupt policy should be handled where we
            // have that context.
            } else if(code_cache_byte_state::BB_BYTE_MANGLED & byte_state) {
                if(basic_block_kind::BB_TRANSLATED_FRAGMENT == info->kind) {
                    return next;
                }

                return nullptr;

            // we might need to delay, but only if the previous byte is instrumented
            // if the previous byte is instrumented, then it likely means we're in
            // a sequence of instrumented instruction, which might not be re-entrant
            // and so we need to advance the pc to find a safe interrupt location.
            } else if(code_cache_byte_state::BB_BYTE_INSTRUMENTED & byte_state) {

                if(next == cache_pc_start) {
                    return next;
                }

                const code_cache_byte_state prev_byte_state(get_state(
                    pc_byte_states, next - cache_pc_start - 1));

                if(code_cache_byte_state::BB_BYTE_NATIVE == prev_byte_state) {
                    return next;
                }

                ++next;

            // this is bad; we should never be executing padding instructions
            // (which are interrupts)
            } else {
                return nullptr;
            }
        }

        return nullptr; // shouldn't be reached
    }


    /// Compute the size of a basic block given an instruction list. This
    /// computes the size of each instruction, the amount of padding, meta
    /// information, etc.
    unsigned basic_block::size(instruction_list &ls) throw() {
        unsigned size(ls.encoded_size());

        // alignment and meta info
        size += ALIGN_TO(size, BB_INFO_BYTE_ALIGNMENT);

        // counting set for the bits that have state info about the instruction
        // bytes
        unsigned num_instruction_bits(size * BITS_PER_STATE);
        num_instruction_bits += ALIGN_TO(num_instruction_bits, BITS_PER_QWORD);

        size += sizeof(basic_block_info); // meta info
        size += num_instruction_bits / BITS_PER_BYTE;

        size += basic_block_state::size(); // block-local storage

        return size;
    }


    /// Compute the size of an existing basic block.
    unsigned basic_block::size(void) const throw() {
        unsigned size(info->num_bytes + sizeof *info);
        unsigned num_instruction_bits(info->num_bytes * BITS_PER_STATE);
        num_instruction_bits += ALIGN_TO(num_instruction_bits, BITS_PER_QWORD);
        size += num_instruction_bits / BITS_PER_BYTE;
        size += basic_block_state::size(); // block-local storage
        return size;
    }


    /// Decode and translate a single basic block of application/module code.
	basic_block basic_block::translate(cpu_state_handle &cpu, app_pc *pc) throw() {
		const app_pc start_pc(*pc);

		instruction_list ls;

		for(;;) {
			instruction in(instruction::decode(pc));

			if(in.is_cti()) {
				bool add_cti(in.is_call());
				bool decode_next_cti(add_cti);
				operand target(in.get_cti_target());

				// direct jmp/call
				if(dynamorio::opnd_is_pc(target)) {
					app_pc target_pc(dynamorio::opnd_get_pc(target));

					// is it backward branch within the same basic block?
					// if so then we will try to keep control-flow within
					// the block as an optimisation for tight loops.
					if(start_pc <= target_pc && target_pc <= *pc) {
						// TODO
					}

					// call/jmp directly to detach point
					app_pc detach_target_pc(find_detach_target(target_pc));
					if(detach_target_pc) {
						add_cti = true;
						dynamorio::instr_set_target(in, pc_(detach_target_pc));

					// patch point
					} else {

					}
				}

				if(add_cti) {
					ls.append(in);
				}

				if(!decode_next_cti) {
					break;
				}

			} else {
				ls.append(in);
			}
		}

		unsigned staged_size(basic_block::size(ls));

		// allocated pc, this points to block-local storage
		uint8_t *generated_pc(
            cpu->fragment_allocator.allocate_array<uint8_t>(staged_size));

		// allocated pc immediately following block-local storage
		uint8_t *bb_pc(generated_pc + basic_block_state::size());

		return emit(BB_TRANSLATED_FRAGMENT, ls, start_pc, &bb_pc);
	}


    /// Emit an instruction list as code into a byte array. This will also
    /// emit the basic block meta information.
    ///
    /// Note: it is assumed that no field in *state points back to itself
    ///       or any other temporary storage location.
    basic_block basic_block::emit(basic_block_kind kind,
                                  instruction_list &ls,
                                  app_pc generating_pc,
                                  app_pc *generated_pc) throw() {

        app_pc pc = *generated_pc;
        pc += ALIGN_TO(reinterpret_cast<uint64_t>(pc), 16);

        // add in the instructions
        const app_pc start_pc(pc);
        pc = ls.encode(pc);

        uint64_t pc_uint(reinterpret_cast<uint64_t>(pc));
        app_pc pc_aligned(pc + ALIGN_TO(pc_uint, BB_INFO_BYTE_ALIGNMENT));

        // add in the padding
        for(; pc < pc_aligned; ) {
            *pc++ = BB_PADDING;
        }

        BARRIER;

        basic_block_info *info(unsafe_cast<basic_block_info *>(pc));

        // fill in the info
        info->magic = static_cast<uint32_t>(BB_MAGIC);
        info->kind = kind;
        info->num_bytes = static_cast<unsigned>(pc - start_pc);
        info->hotness = 0;
        info->generating_pc = generating_pc;

        printf("emitting a basic block with %u bytes\n", info->num_bytes);

        // fill in the byte state set
        pc += sizeof(basic_block_info);
        pc += initialize_state_bytes(info, ls, pc);

        *generated_pc = pc;
        return basic_block(start_pc);
    }
}


