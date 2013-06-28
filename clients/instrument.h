/* Copyright 2012-2013 Peter Goodman, all rights reserved. */
/*
 * instrument.h
 *
 *  Created on: 2013-04-24
 *      Author: pag
 */

#ifndef INSTRUMENT_H_
#define INSTRUMENT_H_

/// Null policy.
#ifdef CLIENT_NULL
#   include "clients/null/instrument.h"
#endif


/// Entry policy.
#ifdef CLIENT_ENTRY
#   include "clients/track_entry_exit/instrument.h"
#endif


/// Null policy.
#ifdef CLIENT_WATCHPOINT_NULL
#   include "clients/watchpoints/clients/null/instrument.h"
#endif


/// Null and watched watchpoint policy.
#ifdef CLIENT_WATCHPOINT_WATCHED
#   include "clients/watchpoints/clients/everything_watched/instrument.h"
#endif


/// Bounds checking watchpoint policy.
#ifdef CLIENT_WATCHPOINT_BOUND
#   include "clients/watchpoints/clients/bounds_checker/instrument.h"
#endif


/// Leak detector watchpoint policy.
#ifdef CLIENT_WATCHPOINT_LEAK
#   include "clients/watchpoints/clients/leak_detector/instrument.h"
#endif



/// Default to NULL policy if no policy was chosen.
#ifndef GRANARY_INIT_POLICY
#   include "clients/null/instrument.h"
#endif


#endif /* INSTRUMENT_H_ */
