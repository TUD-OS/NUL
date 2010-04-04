// -*- Mode: C++ -*-

#include <nul/types.h>

// Size of DMA descriptor and layout of context area depends on type
// of queue.
enum QueueType {
  SIMPLE_TX,
  INTEL82576_TX,
};


struct QueueContext {
  QueueType queue_type;

  union {
    struct {
      
    } simple_tx;
    struct {
      
    } intel82576_tx;
  };
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
      unsigned      vnet;       // Virtual Net (1 per host driver)
      void         *queue;
      unsigned      queue_sem;	// Notify driver that client prepared DMA descriptors and updated tail pointer.
      unsigned      queue_irq;	// Driver up's this semaphore if DMA descriptors were consumed.
      size_t        queue_len;	// Bytes
      QueueContext *context;    // Stores state of the queue. Must include tail pointer.
    };
    struct {
      uint64        mac;
    };
  };

  MessageQueueOp(unsigned vnet, void *queue, unsigned queue_irq, unsigned queue_len,
                 QueueContext *context) :
    op(ANNOUNCE), vnet(vnet), queue(queue), queue_irq(queue_irq),
    queue_len(queue_len), context(context)
  {};

  explicit
  MessageQueueOp(uint64 mac) :
    op(SET_MAC), mac(mac)
  {};
};


// EOF
