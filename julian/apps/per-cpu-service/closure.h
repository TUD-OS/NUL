#pragma once

#include <nul/types.h>

struct Closure {
  uint8  mov_edx_opcode;
  uint32 mov_edx_value;
  uint8  jmp_opcode;
  uint32 jmp_target;
  uint8  end[0];

  mword value() const { return reinterpret_cast<mword>(this); }
  void set(mword tls, mword method) {
    // eax == pt_id
    // mov tls, %edx
    // jmp somewhere
    mov_edx_opcode = 0xBA;
    mov_edx_value  = tls;
    jmp_opcode     = 0xE9;
    jmp_target     = method - reinterpret_cast<mword>(end);
  };
} __attribute__((packed));

// EOF
