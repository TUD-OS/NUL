// -*- Mode: C++ -*-

#include <nul/motherboard.h>

static const unsigned queue_size = 64;

PARAM(riotest, {
    QueueContext *ctx   = new QueueContext(INTEL82576);
    uint64       *queue = new(128) uint64[queue_size*2];
    MessageQueueOp qop(0, queue, sizeof(uint64[queue_size*2]), ctx);
    if (!mb.bus_queueop.send(qop))
      Logging::printf("Could not register queue.");
  },
  "riotest - Julian knows what this does.")

// EOF
