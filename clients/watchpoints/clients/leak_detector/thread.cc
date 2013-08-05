/* Copyright 2012-2013 Peter Goodman, all rights reserved. */
/*
 * thread.cc
 *
 *  Created on: 2013-06-24
 *      Author: Peter Goodman, akshayk
 */

#include "granary/list.h"
#include "granary/types.h"
#include "clients/watchpoints/clients/leak_detector/descriptor.h"
#include "clients/watchpoints/clients/leak_detector/instrument.h"

using namespace granary;
using namespace granary::types;

extern "C" {
    struct task_struct *kernel_get_current(void);
    struct thread_info *kernel_current_thread_info(void);
    void kernel_pagetable_walk(void *addr);
}

#define GFP_ATOMIC 0x20U
#define PAGE_SIZE 4096
#define THREAD_SIZE 4*PAGE_SIZE

namespace client { namespace wp {

    uint8_t execution_state = 0;

    extern leak_detector_descriptor *DESCRIPTORS[client::wp::MAX_NUM_WATCHPOINTS];

    struct leak_detector_thread_info {
        struct task_struct *tsk;
        void *stack;
        uint64_t stack_current_ptr;
        uint64_t stack_start;
        uint64_t thread_state;
    };

    static static_data<locked_hash_table<app_pc, app_pc>> object_scan_list;
    static static_data<hash_set<uintptr_t>> watchpoint_collect_list;
    static static_data<locked_hash_table<struct thread_info*, leak_detector_thread_info*>> thread_scan_list;

    static static_data<list<uintptr_t>> watchpoint_list;
    typedef list_item_handle<uintptr_t> handle_type;


    STATIC_INITIALISE_ID(leak_detector, {
        object_scan_list.construct();
        thread_scan_list.construct();
        watchpoint_list.construct();
        watchpoint_collect_list.construct();
    })

#if 0
    template <typename T>
    void add_to_scanlist(T ptr_) throw() {
        scan_object_hash->store(
            reinterpret_cast<uintptr_t>(ptr_),
            unsafe_cast<app_pc>(&scan_function<T>::scan_impl::scan_head));
    }
#endif


    bool init_thread_private_slot(void){
        struct thread_info *info = kernel_current_thread_info();
        leak_detector_thread_info *my_thread_info = NULL;
        if(!thread_scan_list->find(info)){
            my_thread_info = (unsafe_cast<leak_detector_thread_info*>)
                    (granary::types::__kmalloc(sizeof(leak_detector_thread_info), GFP_ATOMIC));

            my_thread_info->stack = granary::types::__kmalloc(THREAD_SIZE, GFP_ATOMIC);
            my_thread_info->stack_current_ptr = 0x0ULL;
            my_thread_info->stack_start = 0x0ULL;
            thread_scan_list->store(info, my_thread_info);
        }
        return true;
    }

    void set_thread_private_info(void){
        struct thread_info *info = kernel_current_thread_info();
        leak_detector_thread_info *my_thread_info =
                unsafe_cast<leak_detector_thread_info*>((unsafe_cast<unsigned long>(info) + 2));
        my_thread_info->thread_state = 0x0ULL;
    }

    leak_detector_thread_info *get_thread_private_info(){
        struct thread_info *info = kernel_current_thread_info();
        leak_detector_thread_info *private_slot =
                unsafe_cast<leak_detector_thread_info*>((unsafe_cast<unsigned long>(info) + 2));

        return private_slot;
    }

    bool add_to_scanlist(granary::app_pc obj1, granary::app_pc obj2){
        object_scan_list->store(obj1, obj2);
        return true;
    }

    bool remove_from_scanlist(granary::app_pc, granary::app_pc){
        return true;
    }

    bool is_active_watchpoint(void *addr){
        if(is_watched_address(addr)){
            descriptor_type<void*>::type *desc = descriptor_of(addr);
            if(desc != nullptr) {
                granary::printf("descriptor : %llx\n", desc);
                if(desc->state.is_active){
                    return true;
                }
            }
        }
        return false;
    }

    void handle_watched_address(uintptr_t addr){
        watchpoint_list->append(addr);
    }





    /// Notify the leak detector that this thread's execution has entered the
    /// code cache.
    void leak_notify_thread_enter_module(void) throw() {
       // struct task_struct *task = kernel_get_current();
       // init_thread_private_slot();
        //UNUSED(task);

        //granary::printf("Entering the code cache.\n");
    }


    /// Notify the leak detector that this thread's execution is leaving the
    /// code cache.
    ///
    /// Note: Entry/exits can be nested in the case of the kernel calling the
    ///       module calling the kernel calling the module.
    void leak_notify_thread_exit_module(void) throw() {
        //granary::printf("Exiting the code cache.\n");
    }

    void htable_scan_object(
        granary::app_pc key,
        granary::app_pc value,
        hash_set<granary::app_pc>&
    ) throw() {
        typedef void (*scanner)(void *addr);
        if(is_active_watchpoint(value)){
            handle_watched_address(unsafe_cast<uintptr_t>(value));
         }
        scanner func = (scanner)value;
        if(nullptr != func){
            func(unsafe_cast<void*>(key));
        }
    }

    void print_collected_objects(
        uintptr_t key,
        bool value,
        hash_set<granary::app_pc>&
    ) throw() {
        granary::printf("key(%llx)\t", key);
        UNUSED(value);
    }

    void htable_stack_scanner(
        struct thread_info*,
        struct leak_detector_thread_info *my_thread_info,
        hash_set<granary::app_pc>&
    ) throw() {
        bool ret;
        uint64_t start = (my_thread_info->stack_current_ptr);
        uint64_t stack_base = my_thread_info->stack_start + THREAD_SIZE;
        while( start < stack_base){
            uint64_t *stack_ptr = (unsafe_cast<uint64_t*>)(start);
            ret = is_active_watchpoint((unsafe_cast<void*>)(*stack_ptr));
            start += 8;
        }
        UNUSED(ret);
    }

    void scan_descriptor(uintptr_t index){

        leak_detector_descriptor *desc = leak_detector_descriptor::access(index);
        leak_object_state state;

        if(is_valid_address(desc)){
            uintptr_t base = ((desc->base_address) | 0xffff000000000000ULL);

            for(unsigned offset=0; offset <= desc->size; offset = offset+8){
                uint64_t val = *(unsafe_cast<uintptr_t*>(base));
                if(is_watched_address(val)){
                    uintptr_t found_index = counter_index_of(val);
                    leak_detector_descriptor *found_desc = leak_detector_descriptor::access(found_index);
                    if(is_valid_address(found_desc)){
                        if(found_desc->state.is_reachable != true){
                            state.is_reachable = true;
                            found_desc->state.set_state(state);
                            scan_descriptor(found_index);
                        }
                    }
                }
                base += 8;
            }
        }
    }

    void watchpoint_callback(unsigned long addr){
        leak_object_state state;
        uintptr_t index = counter_index_of(addr);
        leak_detector_descriptor *desc = leak_detector_descriptor::access(index);

        if(desc != nullptr){
            uintptr_t base = desc->base_address;
            uintptr_t limit = desc->base_address + desc->size;
            uintptr_t watch_addr = (addr & 0xffffffffffffULL);
            if((base <= watch_addr) && ( watch_addr <= limit)) {
               // granary::printf("found index (%llx) : %llx\t", index, addr);
                if(desc->state.is_reachable != true){
                    state.is_reachable = true;
                    desc->state.set_state(state);
                    scan_descriptor(index);
                }
            }
        }
    }

    void leak_policy_scan_callback(void) throw(){
        uintptr_t index = 0;
        leak_object_state state;

        granary::printf("%s, %llx\n", __FUNCTION__, DESCRIPTORS);

        if(execution_state == false){
            granary::printf("No allocation in the last epoch\n");
            return;
        }

        {
            uint8_t old_bits;
            uint8_t new_bits;
            do {
                old_bits = execution_state;
                new_bits = 0x0;
            } while(!__sync_bool_compare_and_swap(&execution_state, old_bits, new_bits));
        }

        for(index = 0; index < client::wp::MAX_NUM_WATCHPOINTS; index++){
            leak_detector_descriptor *desc;
            desc = leak_detector_descriptor::access(index);

            if(is_valid_address(desc)){
                if(desc->accessed_in_last_epoch){
                    desc->accessed_in_last_epoch = false;
                    if(desc->state.is_reachable != true){
                        state.is_reachable = true;
                        desc->state.set_state(state);
                        scan_descriptor(index);
                    }
                  //  watchpoint_collect_list->add(index);
                   // watchpoint_list->append(index);
                    granary::printf("index (%llx)\t", index);
                }
            }
        }

        kernel_pagetable_walk(unsafe_cast<void*>(watchpoint_callback));

        granary::printf("reachable \t");
        for(index = 0; index < client::wp::MAX_NUM_WATCHPOINTS; index++){
            leak_detector_descriptor *desc;
            desc = leak_detector_descriptor::access(index);

            if(is_valid_address(desc)){
                if(desc->state.is_reachable == true){
                    granary::printf(" (%llx)\t", index);
                }else if(desc->state.is_active == true){
                    granary::printf(" \nleaked (%llx)\n", index);
                }
            }
        }

#if 0
        {
            handle_type list_item = watchpoint_list->first();

            while(list_item.is_valid()){
                index = unsafe_cast<uintptr_t>(*list_item);
                granary::printf("index2 (%llx)\n", index);
#if 1
                leak_detector_descriptor *desc = leak_detector_descriptor::access(index);
                if(desc != nullptr){
                    uintptr_t base = ((desc->base_address) | 0xffff000000000000ULL);

                    for(unsigned offset=0; offset <= desc->size; offset=offset+8){
                        uint64_t val = *(unsafe_cast<uintptr_t*>(base));
                        if(is_watched_address(val)){
                            uintptr_t found_index = counter_index_of(val);
                            if(!watchpoint_collect_list->contains(found_index)){
                                watchpoint_collect_list->add(found_index);
                              //  watchpoint_list->append(unsafe_cast<uintptr_t>(found_index));
                                granary::printf("found index (%llx)\t", found_index);
                            }
                            //watchpoint_collect_list->add(index);
                           // watchpoint_list->append(unsafe_cast<uintptr_t>(found_index));
                     //       granary::printf("watchpoint address %llx\n", val);
                        }
                        base += 8;
                    }
                }

               // uintptr_t limit = desc->base_address + desc->size;
               // granary::printf("base address : %llx\n", base);
#endif
                list_item = list_item.next();
            }
        }

#endif
#if 0


        hash_set<granary::app_pc> collect_list;
        watchpoint_collect_list->for_each_entry(print_collected_objects, collect_list);
        granary::printf("\n");
#endif
#if 0
        {
            handle_type list_item = watchpoint_list->first();
            while(list_item.is_valid()){
                index = unsafe_cast<uintptr_t>(*list_item);
                granary::printf("%llx\t", index);
                list_item = list_item.next();
            }

            granary::printf("\n");
        }
#endif
        // scan the kernel objects passed to the module

       // object_scan_list->for_each_entry(htable_scan_object, collect_list);

        // scan the kernel stack and look for the watched address;
       // thread_scan_list->for_each_entry(htable_stack_scanner, collect_list);

    }

    void htable_copy_task(
        struct thread_info *info,
        struct leak_detector_thread_info *my_thread_info,
        hash_set<granary::app_pc>&
    ) throw() {
        my_thread_info->stack_current_ptr = info->task->thread.sp;
        my_thread_info->stack_start = (unsafe_cast<uint64_t>)(info->task->stack);
        my_thread_info->tsk = info->task;
        memcpy(my_thread_info->stack, info->task->stack, THREAD_SIZE);
    }

    void leak_policy_update_rootset(void) throw(){
        granary::printf("%s\n", __FUNCTION__);
        //hash_set<granary::app_pc> collect_list;
        //thread_scan_list->for_each_entry(htable_copy_task, collect_list);
    }

}}

