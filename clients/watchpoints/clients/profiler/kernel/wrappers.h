/*
 * wrappers.h
 *
 *  Created on: 2013-06-25
 *      Author: akshayk
 */

#ifndef _LEAK_POLICY_WRAPPERS_H_
#define _LEAK_POLICY_WRAPPERS_H_

#include "granary/utils.h"
#include "granary/client.h"
#include "clients/watchpoints/config.h"
#include "clients/watchpoints/instrument.h"
#include "clients/watchpoints/clients/leak_detector/state.h"

using namespace granary;

#include "clients/watchpoints/clients/profiler/kernel/linux/allocator_wrappers.h"
//#include "clients/watchpoints/clients/leak_detector/kernel/linux/allocator_patch.h"



#ifndef APP_WRAPPER_FOR_pointer
#   define APP_WRAPPER_FOR_pointer
    POINTER_WRAPPER({
        PRE_OUT {
            if(!is_valid_address(arg)) {
                return;
            }
#if 0
            client::wp::add_to_scanlist(
                unsafe_cast<app_pc>(arg),
                unsafe_cast<app_pc>(SCAN_HEAD_FUNC(decltype(arg))));
#endif
            PRE_OUT_WRAP(*unwatched_address_check(arg));
        }
        PRE_IN {
            if(!is_valid_address(arg)) {
                return;
            }
#if 0
            client::wp::add_to_scanlist(
                unsafe_cast<app_pc>(arg),
                unsafe_cast<app_pc>(SCAN_HEAD_FUNC(decltype(arg))));
#endif
            PRE_IN_WRAP(*unwatched_address_check(arg));
        }
        INHERIT_POST_INOUT
        RETURN_OUT {
            if(!is_valid_address(arg)){
                return;
            }
#if 0
            client::wp::add_to_scanlist(
                unsafe_cast<app_pc>(arg),
                unsafe_cast<app_pc>(SCAN_HEAD_FUNC(decltype(arg))));
#endif
        }
        RETURN_IN {
            if(!is_valid_address(arg)){
                return;
            }
#if 0
            client::wp::add_to_scanlist(
                unsafe_cast<app_pc>(arg),
                unsafe_cast<app_pc>(SCAN_HEAD_FUNC(decltype(arg))));
#endif
        }
    })
#endif


#define WP_BASIC_POINTER_WRAPPER(base_type) \
    TYPE_WRAPPER(base_type *, { \
        NO_PRE \
        NO_POST \
        NO_RETURN \
    })


#ifndef APP_WRAPPER_FOR_void_pointer
#   define APP_WRAPPER_FOR_void_pointer
    WP_BASIC_POINTER_WRAPPER(void)
#endif


#ifndef APP_WRAPPER_FOR_int8_t_pointer
#   define APP_WRAPPER_FOR_int8_t_pointer
    static_assert(sizeof(char) == sizeof(int8_t), "Type size mismatch.");
    WP_BASIC_POINTER_WRAPPER(char)
#endif


#ifndef APP_WRAPPER_FOR_uint8_t_pointer
#   define APP_WRAPPER_FOR_uint8_t_pointer
    static_assert(sizeof(char) == sizeof(int8_t), "Type size mismatch.");
    WP_BASIC_POINTER_WRAPPER(unsigned char)
#endif


#ifndef APP_WRAPPER_FOR_int16_t_pointer
#   define APP_WRAPPER_FOR_int16_t_pointer
    static_assert(sizeof(short) == sizeof(int16_t), "Type size mismatch.");
    WP_BASIC_POINTER_WRAPPER(short)
#endif


#ifndef APP_WRAPPER_FOR_uint16_t_pointer
#   define APP_WRAPPER_FOR_uint16_t_pointer
    static_assert(sizeof(short) == sizeof(int16_t), "Type size mismatch.");
    WP_BASIC_POINTER_WRAPPER(unsigned short)
#endif


#ifndef APP_WRAPPER_FOR_int32_t_pointer
#   define APP_WRAPPER_FOR_int32_t_pointer
    static_assert(sizeof(int) == sizeof(int32_t), "Type size mismatch.");
    WP_BASIC_POINTER_WRAPPER(int)
#endif


#ifndef APP_WRAPPER_FOR_uint32_t_pointer
#   define APP_WRAPPER_FOR_uint32_t_pointer
    static_assert(sizeof(int) == sizeof(int32_t), "Type size mismatch.");
    WP_BASIC_POINTER_WRAPPER(unsigned)
#endif


#ifndef APP_WRAPPER_FOR_int64_t_pointer
#   define APP_WRAPPER_FOR_int64_t_pointer
    static_assert(sizeof(long) == sizeof(int64_t), "Type size mismatch.");
    WP_BASIC_POINTER_WRAPPER(long)
#endif


#ifndef APP_WRAPPER_FOR_uint64_t_pointer
#   define APP_WRAPPER_FOR_uint64_t_pointer
    static_assert(sizeof(long) == sizeof(int64_t), "Type size mismatch.");
    WP_BASIC_POINTER_WRAPPER(unsigned long)
#endif


#ifndef APP_WRAPPER_FOR_float_pointer
#   define APP_WRAPPER_FOR_float_pointer
    WP_BASIC_POINTER_WRAPPER(float)
#endif


#ifndef APP_WRAPPER_FOR_double_pointer
#   define APP_WRAPPER_FOR_double_pointer
    WP_BASIC_POINTER_WRAPPER(double)
#endif


#endif /* WRAPPERS_H_ */
