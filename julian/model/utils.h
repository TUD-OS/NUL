// -*- Mode: C++ -*-

#pragma once

#include <nul/motherboard.h>

static inline void *alloc_from_guest(DBus<MessageHostOp> &hostop, size_t size)
{
  MessageHostOp alloc(MessageHostOp::OP_ALLOC_FROM_GUEST, size);
  if (not hostop.send(alloc))
    Logging::panic("Could not allocate register window.\n");

  // Cannot translate alloc.phys directly, because it is beyond
  // "normal" physical memory now and we would get NULL back.
  MessageHostOp conv(MessageHostOp::OP_GUEST_MEM, 0UL);
  if (not hostop.send(conv) or (conv.ptr == NULL))
    Logging::panic("Could not convert VM pointer?\n");

  return conv.ptr + alloc.phys;
}

// EOF
