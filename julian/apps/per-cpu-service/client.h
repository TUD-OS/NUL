#pragma once

#include <nul/capalloc.h>
#include <nul/baseprogram.h>
#include <nul/parent.h>

class NewClient {
protected:
  Utcb           &_utcb;
  CapAllocator   *_cap_alloc;
  const char     *_service;
  
  unsigned _session;
  unsigned _pseudonym;
  unsigned _portal;
  
public:

  unsigned call()
  {
    return nova_call(_session);
  }

  // Create session for the calling CPU.
  NewClient(Utcb &utcb, CapAllocator *cap, const char *service, unsigned instance, bool blocking = true)
    : _utcb(utcb), _cap_alloc(cap), _service(service)

  {
    unsigned res;
    _pseudonym = cap->alloc_cap();

    res = ParentProtocol::get_pseudonym(utcb, service, instance, _pseudonym);
    if (res != ENONE) Logging::panic("Couldn't get pseudonym for service '%s'.",
                                     service);
    _portal = cap->alloc_cap();

    res = ParentProtocol::get_portal(utcb, _pseudonym, _portal, blocking);
    if (res != ENONE) Logging::panic("Couldn't get service portal for service '%s'.",
                                     service);
    
    utcb.add_frame()
      << ParentProtocol::TYPE_OPEN << Utcb::TypedMapCap(_pseudonym)
      << Crd(_session, 0, DESC_CAP_ALL);
    res = ParentProtocol::call(utcb, _portal, true, false);
    if (res != ENONE) Logging::panic("Couldn't open session to '%s': %u",
                                     service, res);
  }


  ~NewClient() {
    assert(BaseProgram::myutcb() == &_utcb);

    // XXX uh?! why is this not called in release_pseudonym? How does
    // the normal client cause a TYPE_CLOSE message to be sent to the
    // server?
    _utcb.add_frame()
      << ParentProtocol::TYPE_CLOSE << Utcb::TypedMapCap(_pseudonym);
    unsigned res = ParentProtocol::call(_utcb, _portal, true, false);
    // XXX
    assert(res == ENONE);

    // XXX Do we need this?
    res = ParentProtocol::release_pseudonym(_utcb, _pseudonym);
    // XXX
    assert(res == ENONE);

    nova_revoke(Crd(_pseudonym, 0, DESC_CAP_ALL), true);
    nova_revoke(Crd(_portal, 0, DESC_CAP_ALL),    true);
    nova_revoke(Crd(_session, 0, DESC_CAP_ALL),   true);
    _cap_alloc->dealloc_cap(_pseudonym);
    _cap_alloc->dealloc_cap(_portal);
    _cap_alloc->dealloc_cap(_session);

  }
};

// EOF
