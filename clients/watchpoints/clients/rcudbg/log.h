/* Copyright 2012-2013 Peter Goodman, all rights reserved. */
/*
 * log.h
 *
 *  Created on: 2013-08-18
 *      Author: Peter Goodman
 */

#ifndef RCUDBG_LOG_H_
#define RCUDBG_LOG_H_

#include "granary/client.h"

namespace client {
    enum log_message_id {
        MESSAGE_NOT_READY = 0,
#define RCUDBG_MESSAGE(ident, kind, message, arg_defs, arg_splat) ident,
#include "clients/watchpoints/clients/rcudbg/message.h"
#undef RCUDBG_MESSAGE
        NUM_MESSAGES
    };


    /// A generic message container for log entries from the rcudbg tool.
    struct message_container {
        std::atomic<log_message_id> message_id;
        uint64_t payload[4];
    };


    /// Next offset into the log to which we can write.
    extern std::atomic<uint64_t> NEXT_LOG_OFFSET;

    enum {
        MAX_NUM_MESSAGES = 4096
    };

    extern message_container MESSAGES[];


    template <typename A0>
    void log(log_message_id id, A0 a0) throw() {
        const uint64_t index(NEXT_LOG_OFFSET.fetch_add(1));
        message_container &cont(MESSAGES[index % MAX_NUM_MESSAGES]);
        cont.payload[0] = granary::unsafe_cast<uint64_t>(a0);
        cont.message_id.store(id);
    }


    template <typename A0, typename A1>
    void log(log_message_id id, A0 a0, A1 a1) throw() {
        const uint64_t index(NEXT_LOG_OFFSET.fetch_add(1));
        message_container &cont(MESSAGES[index % MAX_NUM_MESSAGES]);

        cont.payload[0] = granary::unsafe_cast<uint64_t>(a0);
        cont.payload[1] = granary::unsafe_cast<uint64_t>(a1);
        cont.message_id.store(id);
    }


    template <typename A0, typename A1, typename A2>
    void log(log_message_id id, A0 a0, A1 a1, A2 a2) throw() {
        const uint64_t index(NEXT_LOG_OFFSET.fetch_add(1));
        message_container &cont(MESSAGES[index % MAX_NUM_MESSAGES]);

        cont.payload[0] = granary::unsafe_cast<uint64_t>(a0);
        cont.payload[1] = granary::unsafe_cast<uint64_t>(a1);
        cont.payload[2] = granary::unsafe_cast<uint64_t>(a2);
        cont.message_id.store(id);
    }


    template <typename A0, typename A1, typename A2, typename A3>
    void log(log_message_id id, A0 a0, A1 a1, A2 a2, A3 a3) throw() {
        const uint64_t index(NEXT_LOG_OFFSET.fetch_add(1));
        message_container &cont(MESSAGES[index % MAX_NUM_MESSAGES]);

        cont.payload[0] = granary::unsafe_cast<uint64_t>(a0);
        cont.payload[1] = granary::unsafe_cast<uint64_t>(a1);
        cont.payload[2] = granary::unsafe_cast<uint64_t>(a2);
        cont.payload[3] = granary::unsafe_cast<uint64_t>(a3);
        cont.message_id.store(id);
    }
}

#endif /* RCUDBG_LOG_H_ */
