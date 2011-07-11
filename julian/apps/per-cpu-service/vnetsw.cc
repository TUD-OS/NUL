#include "closure.h"
#include "client.h"
#include "service.h"

#include <service/net.h>

struct VirtualNetMessage {
  enum {
    SET_OPTIONS,
    SEND_PACKET,
  } op;

  union {
    struct {
      EthernetAddr mac;
      bool         promisc;
    } options;
    uint8 packet[];
  };
};


struct ReceiveBufferDescriptor {
  EthernetAddr addr;
  cap_sel      sm;
  uint8       *data;
};

class VirtualNetService : public ServiceProgram
{
protected:
  static const unsigned MAXCLIENTS  = 1024;
  static const unsigned BUFFER_SIZE = 1024*128;

  ReceiveBufferDescriptor _rb[MAXCLIENTS];

  struct Session : public BaseSession {


    // Used to notify client about new packet data.
    const cap_sel sm_notify;

    void clear()
    {
      promisc       = false;
    }

    virtual void cleanup()
    {
      
    }

    explicit Session(cap_sel pt, cap_sel sm) : BaseSession(pt), sm_notify(sm) {
      data = new (4096) uint8[SHARED_MEMORY_SIZE];
    }
  };

  BaseSession *new_session()
  {
    mword func = reinterpret_cast<mword>(&VirtualNetService::client_func_s);
    Session *s = static_cast<Session *>(_free_sessions.dequeue());
    if (s == NULL) {
      s = new Session(alloc_cap(), alloc_cap());
      Logging::printf("New session object allocated: %p\n", s);
    }

    nova_create_sm(s->sm_notify());

    // Update closure
    s->closure.set(reinterpret_cast<mword>(s), func);
    return s;
  }

  static unsigned client_func_s(unsigned pt_id, Session *s,
                                VirtualNetService *tls, Utcb *utcb) REGPARM(2)
  {
    return tls->client_func(pt_id, s, utcb);
  }

  unsigned client_func(unsigned pt_id, Session *session, Utcb *utcb)
  {
    Logging::printf("Got %u bytes of data\n", utcb->head.untyped*sizeof(utcb->msg[0]));


    return ENONE;
  }

public:

  VirtualNetService()
  {
    Logging::printf("Hello. This is your switch speaking.\n");
    register_service("/vnet");
  };

  NORETURN
  void run(Utcb *utcb, Hip *hip)
  {
    block_forever();
  }

};

ASMFUNCS(VirtualNetService, NovaProgram)

// EOF
