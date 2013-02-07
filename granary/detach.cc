/*
 * detach.cc
 *
 *  Created on: Nov 18, 2012
 *      Author: pag
 */


#include "granary/detach.h"
#include "granary/gen/detach.h"

#include "deps/murmurhash/murmurhash.h"

namespace granary {

    namespace detail {
        int16_t HASH_TABLE_INDICES[LAST_DETACH_ID];
    }

    STATIC_INITIALISE({
        for(unsigned i(0); i < LAST_DETACH_ID; ++i) {
            const function_wrapper &wrapper(FUNCTION_WRAPPERS[i]);
            (void) wrapper;
        }
    });

	/// Returns the address of a detach point. For example, in the
	/// kernel, if pc == &printk is a detach point then this will
	/// return the address of the printk wrapper (which might itself
	/// be printk).
	///
	/// Returns:
	///		A translated target address, or nullptr if this isn't a
	/// 	detach target.
	app_pc find_detach_target(app_pc) throw() {
		return nullptr;
	}


	/// Detach Granary.
	void detach(void) throw() {
	    ASM("");
	}
}
