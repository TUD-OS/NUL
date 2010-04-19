// -*- Mode: C++ -*-

#include <nul/types.h>

// Size of DMA descriptor and layout of context area depends on type
// of queue.
enum QueueType {
  TX = 0,
  RX = 1,
  RXTX_MASK  = 1,
  SIMPLE     = (1<<1),
  INTEL82576 = (2<<1),
  // foo = (3<<1),
};

// Terminology:
// driver - the thing that drives the hardware and DMA queues
// client - the thing that wants to send/receive packets (IP stack, VM, ...)

struct QueueContext {
  QueueType queue_type;
  unsigned  queue_irq; // Driver up's this semaphore if DMA descriptors were consumed. Filled in by client.
  unsigned  queue_sem; // Notify driver that client prepared DMA descriptors and updated tail pointer. Filled in by driver.

  union {
    struct {
      
    } simple;
    struct {
      uint64 context[8];
    } intel82576;
  };
  
  QueueContext(QueueType qt) : queue_type(qt) {}
};

// Comments on new network interface:
// - register DMA queues at host driver
// - use old-style synchronous MessageNetwork for meta-info (setting MAC, up, down, modify head pointer)

struct MessageQueueOp
{
  enum ops {
    ANNOUNCE,
    SET_MAC,
  };
  
  unsigned op;
  bool success;

  union {
    struct {
      unsigned      vnet;       // Virtual Net (1 per host driver -> is used to address host device)

      // The following two are here, because the addresses might need to be translated?
      void         *queue;	// Filled in by client.
      size_t        queue_len;	// Bytes

      QueueContext *context;    // Stores state of the queue. Must include tail pointer.
    };
    struct {
      uint64        mac;
    };
  };

  MessageQueueOp(unsigned vnet, void *queue, unsigned queue_len, QueueContext *context) :
    op(ANNOUNCE), vnet(vnet), queue(queue), queue_len(queue_len),
    context(context)
  {};

  explicit
  MessageQueueOp(uint64 mac) :
    op(SET_MAC), mac(mac)
  {};
};


// EOF
