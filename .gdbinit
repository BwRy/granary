set logging off
set breakpoint pending on
set print demangle on
set print asm-demangle on
set print object on
set print static-members on
set disassembly-flavor att
set language c++


# set-user-detect
#
# Uses Python support to set the variable `$__in_user_space`
# to `0` or `1` depending on whether we are instrumenting in
# user space or kernel space, respectively.
define set-user-detect
  python None ; \
    gdb.execute( \
      "set $__in_user_space = %d" % int(None is not gdb.current_progspace().filename), \
      from_tty=True, to_string=True)
end


# Detect if we're in user space and set `$__in_user_space`
# appropriately.
set-user-detect


# Kernel setup
if !$__in_user_space
  file ~/Code/linux-3.8.2/vmlinux
  target remote : 9999
  source ~/Code/Granary/granary.syms
end


# Granary breakpoints
b granary_fault
b granary_break_on_fault
b granary_break_on_predict


# Kernel breakpoints
if !$__in_user_space
  #b granary_break_on_interrupt
  b granary_break_on_nested_task
  b panic
  b show_fault_oops
  b do_invalid_op
  b do_general_protection
  b __schedule_bug
  b __stack_chk_fail
  b do_spurious_interrupt_bug
  b report_bug
end


# Just re-affirm it.
set language c++


# p-bb-info ADDR
#
# Print the basic block info structure information for the basic block
# whose meta-information is located at address ADDR.
define p-bb-info
  set language c++

  # Find the basic block.
  set $__addr = (unsigned long) $arg0
  if ($__addr % 4)
    set $__addr = $__addr + (4 - ($__addr % 4))
  end
  set $__int = (uint32_t *) $__addr
  while *$__int != 0xD4D5D682
    set $__int = $__int + 1
  end

  # Extract info.
  set $__bb = ((granary::basic_block_info *) $__int)
  set $__policy_addr = &($__bb->policy_bits)
  set $__policy = ((granary::instrumentation_policy *) $__policy_addr)

  # Print the info.
  printf "Basic block info:\n"
  printf "  App address: %p\n", $__bb->generating_pc
  printf "  Stub instructions: %p\n", ($arg0 - $__bb->num_bytes)
  printf "  Instructions: %p\n", ($arg0 - $__bb->num_bytes + $__bb->num_patch_bytes)
  printf "  Is indirect CTI target: %d\n", $__policy->u.is_indirect_target
  printf "  Is return target: %d\n", $__policy->u.is_return_target
  printf "  Is in XMM context: %d\n", $__policy->u.is_in_xmm_context
  printf "  Is in host context: %d\n", $__policy->u.is_in_host_context
  printf "  Policy ID: %d\n", $__policy->u.id
  printf "  Instrumentation Function: "
  if 1 == $__policy->u.is_in_host_context
    info sym granary::instrumentation_policy::HOST_VISITORS[$__policy->u.id]
  else
    info sym granary::instrumentation_policy::APP_VISITORS[$__policy->u.id]
  end
  dont-repeat
end


# x-ins START END
#
# Examine the instructions in the range [START, END).
define x-ins
  set $__start = (uint64_t) $arg0
  set $__end = (uint64_t) $arg1
  set $__dont_exit = 1
  set $__len = 0
  set $__in = (uint8_t *) 0
  set $num_ins = 0

  while $__dont_exit
    set $__orig_start = $__start

    python None ; \
      gdb.execute( \
        "x/2i $__start\n", \
        from_tty=True, to_string=True) ; \
      gdb.execute( \
        "set $__start = $_\n", \
        from_tty=True, to_string=True)

    set $__in = (uint8_t *) $__start
    if $__start == $__end || 0xEA == *$__in || 0xD4 == *$__in || 0x82 == *$__in
      set $__dont_exit = 0
    end 
    x/i $__orig_start
    set $num_ins = $num_ins + 1
  end
  dont-repeat
end


# p-bb ADDR
#
# Print the info and instructions of a basic block given the address
# of an instruction in the basic block.
define p-bb
  set language c++
  set $__a = (uint64_t) $arg0
  set $__a = $__a + (($__a % 8) ? 8 - ($__a % 8): 0)
  set $__int = (uint32_t *) $__a
  set $__num_stub_ins = 0
  set $__num_trans_ins = 0
  set $__num_ins = 0

  while *$__int != 0xD4D5D682
    set $__int = $__int + 1
  end

  set $__bb_info = (granary::basic_block_info *) $__int
  set $__bb_info_addr = (uint64_t) $__int
  set $__bb_stub = $__bb_info_addr - $__bb_info->num_bytes
  set $__bb_start = $__bb_stub + $__bb_info->num_patch_bytes

  if $__bb_stub != $__bb_start
    printf "Stub instructions:\n"
    x-ins $__bb_stub $__bb_start
    set $__num_stub_ins = $num_ins
    printf "\n"
  end

  printf "Translated instructions:\n"
  x-ins $__bb_start $__bb_info_addr
  set $__num_trans_ins = $num_ins
  printf "\n"

  printf "Original instructions:\n"
  set $__in_start = $__bb_info->generating_pc
  set $__in_end = $__in_start + $__bb_info->generating_num_bytes
  x-ins $__in_start $__in_end
  set $__num_ins = $num_ins
  printf "\n"

  p-bb-info $__bb_info
  
  printf "  Number of stub instructions: %d\n", $__num_stub_ins
  printf "  Number of translated instructions: %d\n", $__num_trans_ins
  printf "  Number of original instructions: %d\n", $__num_ins
  dont-repeat
end


# p-wrapper ID
#
# Prints the information about a function wrapper with id ID.
define p-wrapper
  set language c++
  set $__w = &(granary::FUNCTION_WRAPPERS[(int) $arg0])
  printf "Function wrapper %s (%d):\n", $__w->name, (int) $arg0
  printf "  Original address: %p\n", $__w->original_address
  printf "  App Wrapper address: %p\n", $__w->app_wrapper_address
  printf "  Host Wrapper address: %p\n", $__w->host_wrapper_address
  dont-repeat
end


# p-trace N
#
# Prints at most N elements from the trace log.
define p-trace
  set language c++
  set $__i = (int) $arg0
  set $__j = 1
  set $__head = (granary::trace_log_item *) granary::TRACE._M_b._M_p
  printf "Global code cache lookup trace:\n"
  while $__i > 0 && 0 != $__head
    printf "  [%d] %p\n", $__j, $__head->code_cache_addr
    set $__j = $__j + 1
    set $__i = $__i - 1
    set $__head = $__head->prev
  end
  dont-repeat
end


# get-trace-entry N
#
# Gets a pointer to the Nth trace entry from the trace log and
# put its into the $trace_entry variable.
define get-trace-entry
  set language c++
  set $trace_entry = (granary::trace_log_item *) 0
  set $__i = (int) $arg0
  set $__head = (granary::trace_log_item *) granary::TRACE._M_b._M_p
  while $__i > 0 && 0 != $__head
    if $__i == 1
      set $trace_entry = $__head
    end
    set $__i = $__i - 1
    set $__head = $__head->prev
  end
  dont-repeat
end


# p-trace-entry N
#
# Prints a specific entry from the trace log. The most recent entry
# is entry 1.
define p-trace-entry
  set language c++
  get-trace-entry $arg0
  printf "Global code cache lookup trace:\n"
  printf "  [%d] %p\n", $arg0, $trace_entry->code_cache_addr
  dont-repeat
end


# p-trace-entry-bb N
#
# Prints the basic block for the Nth trace log entry.
define p-trace-entry-bb
  set language c++
  get-trace-entry $arg0
  p-bb $trace_entry->code_cache_addr
  dont-repeat
end


# get-trace-cond-reg <min_trace> <str_reg_name> <str_bin_op> <int_value>
#
# Gets a pointer to a trace entry whose register meets a condition
# specified by `str_reg_name str_bin_op int_value` and where the
# trace number (1-indexed) is greater-than or equal to the `min_trace`.
#
# Puts its results into `$trace_entry` and `$trace_entry_num`.
define get-trace-cond-reg
  set language c++
  set $__min_trace = (int) $arg0
  set $__str_reg_name = $arg1
  set $__str_bin_op = $arg2
  set $__int_value = (unsigned long) $arg3
  set $trace_entry = 0
  set $trace_entry_num = 0
  set $__i = 1
  set $__head = (granary::trace_log_item *) granary::TRACE._M_b._M_p
  set $__reg_value = 0
  set $__cond = 0

  while 0 != $__head
    if $__i >= $__min_trace

      # Evaluate the condition using Python
      python None ; \
        reg = str(gdb.parse_and_eval("$__str_reg_name")).lower()[1:-1] ; \
        bin_op = str(gdb.parse_and_eval("$__str_bin_op"))[1:-1] ; \
        gdb.execute( \
          "set $__reg_value = $__head->state.%s.value_64\n" % reg, \
          from_tty=True, to_string=True) ; \
        gdb.execute( \
          "set $__cond = !!($__reg_value %s $__int_value)\n" % bin_op, \
          from_tty=True, to_string=True)

      if $__cond
        set $trace_entry = $__head
        set $trace_entry_num = $__i
      end
    end

    # Either go to the next trace entry or bail out.
    set $__i = $__i + 1
    if !$trace_entry
      set $__head = $__head->prev
    else
      set $__head = 0
    end

  end
  dont-repeat
end


# internal-bb-by-reg-cond
#
# Internal command only; prints out the basic block for a condition-
# based register lookup.
define internal-bb-by-reg-cond
  if $trace_entry
    set $__str_reg_name = $arg0
    set $__str_bin_op = $arg1
    set $__int_value = (unsigned long) $arg2

    python None ; \
      reg = str(gdb.parse_and_eval("$__str_reg_name")).lower()[1:-1] ; \
      bin_op = str(gdb.parse_and_eval("$__str_bin_op"))[1:-1] ; \
      val = str(gdb.parse_and_eval("$__int_value")).lower() ; \
      print "Global code cache lookup where %s %s %s:" % ( \
        reg, bin_op, val)

    printf "  [%d] %p\n\n", $trace_entry_num, $trace_entry->code_cache_addr
    printf "Regs:\n"
    printf "  r15: 0x%lx\n", $trace_entry->state.r15.value_64
    printf "  r14: 0x%lx\n", $trace_entry->state.r14.value_64
    printf "  r13: 0x%lx\n", $trace_entry->state.r13.value_64
    printf "  r12: 0x%lx\n", $trace_entry->state.r12.value_64
    printf "  r11: 0x%lx\n", $trace_entry->state.r11.value_64
    printf "  r10: 0x%lx\n", $trace_entry->state.r10.value_64
    printf "  r9:  0x%lx\n", $trace_entry->state.r9.value_64
    printf "  r8:  0x%lx\n", $trace_entry->state.r8.value_64
    printf "  rdi: 0x%lx\n", $trace_entry->state.rdi.value_64
    printf "  rsi: 0x%lx\n", $trace_entry->state.rsi.value_64
    printf "  rbp: 0x%lx\n", $trace_entry->state.rbp.value_64
    printf "  rbx: 0x%lx\n", $trace_entry->state.rbx.value_64
    printf "  rdx: 0x%lx\n", $trace_entry->state.rdx.value_64
    printf "  rcx: 0x%lx\n", $trace_entry->state.rcx.value_64
    printf "  rax: 0x%lx\n", $trace_entry->state.rax.value_64
    printf "  "
    printf "\n"
    p-bb $trace_entry->code_cache_addr
  end
end


# p-bb-where-reg <str_reg_name> <str_bin_op> <int_value>
#
# Prints out the basic block where the register condition specified
# by `str_reg_name str_bin_op int_value` is satisfied.
define p-bb-where-reg
  set $__str_reg_name = $arg0
  set $__str_bin_op = $arg1
  set $__int_value = (unsigned long) $arg2
  get-trace-cond-reg 1 $arg0 $arg1 $arg2
  internal-bb-by-reg-cond $__str_reg_name $__str_bin_op $__int_value
end


# p-next-bb-where-reg <str_reg_name> <str_bin_op> <int_value>
#
# Prints out the next basic block where the register condition
# specified by `str_reg_name str_bin_op int_value` is satisfied.
# This assumes that either `p-bb-by-reg-cond` or
# `p-next-bb-by-reg-cond` has been issued.
define p-next-bb-where-reg
  set $__str_reg_name = $arg0
  set $__str_bin_op = $arg1
  set $__int_value = (unsigned long) $arg2
  if 0 < $trace_entry_num
    set $__i = $trace_entry_num + 1
    get-trace-cond-reg $__i $__str_reg_name $__str_bin_op $__int_value
    internal-bb-by-reg-cond $__str_reg_name $__str_bin_op $__int_value
  end
end