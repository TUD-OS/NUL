/*
 * Per CPU Timer service.
 *
 * Copyright (C) 2011, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Vancouver.
 *
 * Vancouver.nova is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * Vancouver.nova is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */


#include <nul/motherboard.h>
#include <nul/compiler.h>
#include <nul/capalloc.h>
#include <nul/baseprogram.h>
#include <sys/semaphore.h>
#include <host/hpet.h>
#include <host/rtc.h>
#include <nul/topology.h>
#include <nul/generic_service.h>
#include <nul/service_timer.h>
#include <service/lifo.h>
#include <service/time.h>
#include <nul/timer.h>

// Each CPU has a thread that maintains a CPU-local timeout list. From
// this list the next timeout on this CPU is calculated. If a CPU does
// not have its own timer, it adds this timeout to the timeout list of
// another CPU that has a timer. CPU-to-CPU mapping tries to respect
// CPU topology.

// CAVEATS:
// 
// - sessions are per-CPU only (will be fixed)

// TODO:
// - non-MSI mode has issues with IRQ sharing. Better disable this for
//   releases?
// - restructure code to allow helping
// - PIT mode is not as exact as it could be. Do we care? 

// Tick at least this often between overflows of the lower 32-bit of
// the HPET main counter. If we don't tick between overflows, the
// overflow might not be detected correctly.
#define MIN_TICKS_BETWEEN_HPET_WRAP 4

// Resolution of our TSC clocks per HPET clock measurement. Lower
// resolution mean larger error in HPET counter estimation.
#define CPT_RESOLUTION   /* 1 divided by */ (1U<<13) /* clocks per hpet tick */

#define CLIENTS ((1 << Config::MAX_CLIENTS_ORDER) + 32 + 10)

// PIT
#define PIT_FREQ            1193180ULL
#define PIT_DEFAULT_PERIOD  1000ULL /* us */
#define PIT_IRQ  2
#define PIT_PORT 0x40

static uint64 uninterruptible_read(volatile uint64 &v)
{
  uint32 lo = 0;
  uint32 hi = 0;
  // We don't need a lock prefix, because this is only meant to be
  // uninterruptible not atomic. So don't use this for SMP!
  asm ("cmpxchg8b %2\n"
       : "+a" (static_cast<uint32>(lo)), "+d" (static_cast<uint32>(hi))
       : "m" (v), "b" (static_cast<uint32>(lo)), "c" (static_cast<uint32>(hi)));
  return (static_cast<uint64>(hi) << 32) | lo;
}

static void uninterruptible_write(volatile uint64 &to, uint64 value)
{
  uint32 nlo = value;
  uint32 nhi = value >> 32;
  uint32 olo = 0;
  uint32 ohi = 0;

  // We iterate at least twice to get the correct old value and then
  // to substitute it.
  asm ("1: cmpxchg8b %0\n"
       "jne 1b"
       : "+m" (to),
         "+a" (static_cast<uint32>(olo)), "+d" (static_cast<uint32>(ohi))
       : "b" (static_cast<uint32>(nlo)), "c" (static_cast<uint32>(nhi)));
}

class ClockSyncInfo {
private:
  volatile uint64 last_hpet;

public:
  uint64 unsafe_hpet() const { return last_hpet; }

  explicit
  ClockSyncInfo(uint64 hpet = 0)
    : last_hpet(hpet)
  {}

  uint64 correct_overflow(uint64 last, uint32 newv)
  {
    bool of = (static_cast<uint32>(newv) < static_cast<uint32>(last));
    return (((last >> 32) + of) << 32) | newv;
  }

  // Safe to call from anywhere. Returns current (non-estimated) HPET
  // value.
  uint64 current_hpet(uint32 r)
  {
    return correct_overflow(uninterruptible_read(last_hpet), r);
  }

  // Fetch a new timer value.
  uint64 fetch(volatile uint32 &r)
  {
    uint64 newv = r;
    newv = correct_overflow(uninterruptible_read(last_hpet), newv);

    uninterruptible_write(last_hpet, newv);
    return newv;
  }
};

class PerCpuTimerService : private BasicHpet,
                           public StaticReceiver<PerCpuTimerService>,
                           public CapAllocator
{
  Motherboard      &_mb;
  #include "host/simplehwioout.h"
  static const unsigned MAX_TIMERS = 24;
  HostHpetRegister *_reg;       // !=NULL in HPET mode only
  uint32            _timer_freq;
  uint64            _nominal_tsc_ticks_per_timer_tick;
  unsigned          _usable_timers;
  uint64            _pit_ticks;

  struct Timer {
    unsigned       _no;
    HostHpetTimer *_reg;

    Timer() : _reg(NULL) {}
  } _timer[MAX_TIMERS];

  struct ClientData : public GenericClientData {

    // This field has different semantics: When this ClientData
    // belongs to a client it contains an absolute TSC value. If it
    // belongs to a remote CPU it contains an absolute timer count.
    volatile uint64   abstimeout;

    // How often has the timeout triggered?
    volatile unsigned count;

    unsigned   nr;
    phy_cpu_no cpu;

    ClientData * volatile lifo_next;
  };

  ALIGNED(8)
  ClientDataStorage<ClientData, PerCpuTimerService> _storage;

  struct RemoteSlot {
    ClientData data;
  };

  struct PerCpu {
    ClockSyncInfo          clock_sync;
    bool                   has_timer;
    Timer                 *timer;
    AtomicLifo<ClientData> work_queue;
    TimeoutList<CLIENTS, ClientData> abstimeouts;

    unsigned        worker_pt;
    KernelSemaphore xcpu_sm;
    uint64          last_to;


    // Used by CPUs without timer
    KernelSemaphore remote_sm;   // for cross cpu wakeup
    RemoteSlot     *remote_slot; // where to store crosscpu timeouts

    // Used by CPUs with timer
    RemoteSlot *slots;          // Array
    unsigned    slot_count;     // with this many entries
    unsigned    irq;            // our irq number
    unsigned    ack;            // what do we have to write to ack? (0 if nothing)
  };

  KernelSemaphore  _xcpu_up;
  PerCpu          *_per_cpu[Config::MAX_CPUS];

  uint32           _assigned_irqs;
  bool             _verbose;
  bool             _slow_wallclock; // If true, wait for RTC update before fetching time

  bool attach_timer_irq(DBus<MessageHostOp> &bus_hostop, Timer *timer, phy_cpu_no cpu)
  {
    // Prefer MSIs. No sharing, no routing problems, always edge triggered.
    if (not (_reg->config & LEG_RT_CNF) and
        (timer->_reg->config & FSB_INT_DEL_CAP)) {


      MessageHostOp msg1 = MessageHostOp::attach_hpet_msi(cpu, false, _reg, "hpet msi");
      if (not bus_hostop.send(msg1)) Logging::panic("MSI allocation failed.");

      if (_verbose)
        Logging::printf("TIMER: Timer %u -> GSI %u CPU %u (%llx:%x)\n",
                        timer->_no, msg1.msi_gsi, cpu, msg1.msi_address, msg1.msi_value);
      
      _per_cpu[cpu]->irq = msg1.msi_gsi;
      _per_cpu[cpu]->ack = 0;

      timer->_reg->msi[0]  = msg1.msi_value;
      timer->_reg->msi[1]  = msg1.msi_address;
      timer->_reg->config |= FSB_INT_EN_CNF;

      return true;
    } else {
      // If legacy is enabled, only allow IRQ2
      uint32 allowed_irqs  = (_reg->config & LEG_RT_CNF) ? (1U << 2) : timer->_reg->int_route;
      uint32 possible_irqs = ~_assigned_irqs & allowed_irqs;

      if (possible_irqs == 0) {
        Logging::printf("TIMER: No IRQs left.\n");
        return false;
      }

      unsigned irq = Cpu::bsr(possible_irqs);
      _assigned_irqs |= (1U << irq);

      MessageHostOp msg = MessageHostOp::attach_irq(irq, cpu, false, "hpet");
      if (not bus_hostop.send(msg)) Logging::panic("Could not attach IRQ.\n");

      _per_cpu[cpu]->irq = irq;
      _per_cpu[cpu]->ack = (irq < 16) ? 0 : (1U << timer->_no);

      if (_verbose)
        Logging::printf("TIMER: Timer %u -> IRQ %u (assigned %x ack %x).\n",
                        timer->_no, irq, _assigned_irqs, _per_cpu[cpu]->ack);


      timer->_reg->config &= ~(0x1F << 9) | INT_TYPE_CNF;
      timer->_reg->config |= (irq << 9) |
        ((irq < 16) ? 0 /* Edge */: INT_TYPE_CNF /* Level */);
    }

    return true;
  }

  // Convert an absolute TSC value into an absolute time counter
  // value. Call only from per_cpu thread. Returns ZERO if result is
  // in the past.
  uint64 absolute_tsc_to_timer(PerCpu *per_cpu, uint64 tsc)
  {
    int64 diff = tsc - _mb.clock()->time();
    if (diff <= 0) {
      //Logging::printf("CPU%u %016llx too late\n", BaseProgram::mycpu(), -diff);
      return 0;
    }

    diff = Math::muldiv128(diff, CPT_RESOLUTION, _nominal_tsc_ticks_per_timer_tick);
    uint64 estimated_main;
    if (_reg)
      estimated_main = per_cpu->clock_sync.current_hpet(_reg->counter[0]);
    else
      estimated_main = _pit_ticks + 1; // Compute from next tick.
          
    return estimated_main + diff;
  }

  bool per_cpu_handle_xcpu(PerCpu *per_cpu)
  {
    bool reprogram = false;

    // Process cross-CPU timeouts. slot_count is zero for CPUs without
    // a timer.
    for (unsigned i = 0; i < per_cpu->slot_count; i++) {
      RemoteSlot &cur = per_cpu->slots[i];
      uint64 to;

      do {
        to = cur.data.abstimeout;
        if (to == 0ULL)
          // No need to program a timeout
          goto next;
      } while (not __sync_bool_compare_and_swap(&cur.data.abstimeout, to, 0));
      
      //Logging::printf(  "CPU%u: Remote timeout %u at     %016llx\n", BaseProgram::mycpu(), i, to);
      if (to < per_cpu->last_to) {
        //Logging::printf("CPU%u: Need to reprogram (last %016llx)\n", BaseProgram::mycpu(), per_cpu->last_to);
        reprogram = true;
      } else
        //Logging::printf("CPU%u: Don't reprogram   (last %016llx)\n", BaseProgram::mycpu(), per_cpu->last_to);

      per_cpu->abstimeouts.cancel(cur.data.nr);
      per_cpu->abstimeouts.request(cur.data.nr, to);

    next:
      ;
    }

    return reprogram;
  }

  bool per_cpu_client_request(PerCpu *per_cpu, ClientData *data)
  {
    unsigned nr = data->nr;
    per_cpu->abstimeouts.cancel(nr);

    uint64 t = absolute_tsc_to_timer(per_cpu, data->abstimeout);
    // XXX Set abstimeout to zero here?
    if (t == 0 /* timer in the past */) {
      COUNTER_INC("TO early");
      unsigned res = nova_semup(data->get_identity());
      (void)res;                // avoid compiler warning. nothing to
                                // be done in error case.
      return false;
    }

    per_cpu->abstimeouts.request(nr, t);

    // uint64 tsc = Cpu::rdtsc();
    // Logging::printf("CPU%u CLIENT now %016llx to %016llx diff %016llx\n", BaseProgram::mycpu(),
    //                 tsc, data->abstimeout, data->abstimeout - tsc);

    return (t < per_cpu->last_to);
  }

  // Returns the next timeout.
  uint64 handle_expired_timers(PerCpu *per_cpu, uint64 now)
  {
    ClientData *data;
    unsigned nr;
    while ((nr = per_cpu->abstimeouts.trigger(now, &data))) {
      per_cpu->abstimeouts.cancel(nr);
      assert(data);
      Cpu::atomic_xadd(&data->count, 1U);
      //Logging::printf("CPU%u semup\n", BaseProgram::mycpu());
      unsigned res = nova_semup(data->get_identity());
      if (res != NOVA_ESUCCESS) Logging::panic("ts: sem cap disappeared\n");
    }

    return per_cpu->abstimeouts.timeout();
  }

  // Update our clock estimation stuff, as a side effect we get a
  // new HPET main counter value.
  void update_hpet_estimation(PerCpu *per_cpu)
  {
    //ClockSyncInfo old = per_cpu->clock_sync;
    per_cpu->clock_sync.fetch(_reg->counter[0]);
  }

  struct WorkerMessage {
    enum WMType{
      XCPU_REQUEST = 1,
      CLIENT_REQUEST,
      TIMER_IRQ,
    } type;
    ClientData *data;
  };

  void per_cpu_worker(Utcb *u)
  {
    //Logging::printf("PT TLS %p UTCB %p %x: %08x %08x\n", this, u, u->head.mtr, u->msg[0], u->msg[1]);

    unsigned cpu     = u->head.nul_cpunr;
    PerCpu  *per_cpu = _per_cpu[cpu];

    WorkerMessage m;
    // XXX *u >> m; ???
    m.type = WorkerMessage::WMType(u->msg[0]);
    m.data = reinterpret_cast<ClientData *>(u->msg[1]);

    //Logging::printf("%08x %08x | %08x %08x\n", u->msg[0], u->msg[1], m.type, m.data);
    //Logging::printf("WORKER CPU%u %u %p\n", cpu, m.type, m.data);

    u->set_header(0, 0);

    bool reprogram = false;

    // We jump here if we were to late with timer
    // programming. reprogram stays true.
  again:

    switch (m.type) {
    case WorkerMessage::XCPU_REQUEST:
      reprogram = per_cpu_handle_xcpu(per_cpu);
      break;
    case WorkerMessage::CLIENT_REQUEST:
      reprogram = per_cpu_client_request(per_cpu, m.data);
      break;
    case WorkerMessage::TIMER_IRQ: 
      {
        if (_reg)
          update_hpet_estimation(per_cpu);
        
        uint64 now = _reg ? per_cpu->clock_sync.unsafe_hpet() : _pit_ticks;
        handle_expired_timers(per_cpu, now);
      
        reprogram = true;
 
        break;
      }
    default:
      Logging::printf("CPU%u Unknown type %u. %08x %08x\n", cpu, m.type, u->msg[0], u->msg[1]);
      return;
    }

    // Check if we don't need to reprogram or if we are in periodic
    // mode. Either way we are done.
    if (not reprogram or (not _reg and per_cpu->has_timer))
      return;
    
    // Okay, we need to program a new timeout.
    uint64 next_to = per_cpu->abstimeouts.timeout();

    uint64 estimated_now = per_cpu->clock_sync.unsafe_hpet();

    // Generate at least some IRQs between wraparound IRQs to make
    // overflow detection robust. Only needed with HPETs.
    if (_reg)
      if ((next_to == ~0ULL /* no next timeout */ ) or
          (static_cast<int64>(next_to - estimated_now) > 0x100000000LL/MIN_TICKS_BETWEEN_HPET_WRAP)) {
        next_to = estimated_now + 0x100000000ULL/MIN_TICKS_BETWEEN_HPET_WRAP;
      }

    // Logging::printf("CPU%u now %08x comp %08x next_to %08x last_to %08x %u\n",
    //                  cpu, static_cast<uint32>(per_cpu->clock_sync.unsafe_hpet()),
    //                  per_cpu->has_timer ? per_cpu->timer->_reg->comp[0] : 0,
    //                  static_cast<uint32>(next_to), static_cast<uint32>(per_cpu->last_to), per_cpu->has_timer);

    per_cpu->last_to = next_to;
        
    if (per_cpu->has_timer) {
      assert(_reg);
      // HPET timer programming. We don't get here for PIT mode.

      // Program a new timeout. Top 32-bits are discarded.
      per_cpu->timer->_reg->comp[0] = next_to;
      
      // Check whether we might have missed that interrupt.
      if ((static_cast<int32>(next_to - _reg->counter[0])) <= 8) {
        COUNTER_INC("TO lost/past");
        //Logging::printf("CPU%u: Next timeout too close!\n", cpu);
        m.type = WorkerMessage::TIMER_IRQ;
        goto again;
      }
    } else {
      // Tell timer_cpu that we have a new timeout.
      
      if (next_to == ~0ULL) return;
      //Logging::printf("CPU%u: Cross core wakeup at %llx!\n", cpu, next_to);
      // XXX Needs to be written atomically!
      per_cpu->remote_slot->data.abstimeout = next_to;
      MEMORY_BARRIER;
      per_cpu->remote_sm.up();
    }


  }


  bool hpet_init(bool hpet_force_legacy)
  {
    // Find and map HPET
    bool          legacy_only   = hpet_force_legacy;
    unsigned long hpet_addr     = get_hpet_address(_mb.bus_acpi);
    if (hpet_addr == 0) {
      Logging::printf("TIMER: No HPET found.\n");
      return false;
    }

    MessageHostOp msg1(MessageHostOp::OP_ALLOC_IOMEM, hpet_addr, 1024);
    if (!_mb.bus_hostop.send(msg1) || !msg1.ptr) {
      Logging::printf("TIMER: %s failed to allocate iomem %lx+0x400\n", __PRETTY_FUNCTION__, hpet_addr);
      return false;
    }

    _reg = reinterpret_cast<HostHpetRegister *>(msg1.ptr);
    if (_verbose)
      Logging::printf("TIMER: HPET at %08lx -> %p.\n", hpet_addr, _reg);

    // Check for old AMD HPETs and go home. :)
    uint8  hpet_rev    = _reg->cap & 0xFF;
    uint16 hpet_vendor = _reg->cap >> 16;
    Logging::printf("TIMER: HPET vendor %04x revision %02x:%s%s\n", hpet_vendor, hpet_rev,
                    (_reg->cap & LEG_RT_CAP) ? " LEGACY" : "",
                    (_reg->cap & BIT64_CAP) ? " 64BIT" : " 32BIT"
                    );
    switch (hpet_vendor) {
    case 0x8086:                // Intel
      // Everything OK.
      break;
    case 0x4353:                // AMD
      if (hpet_rev < 0x10) {
        Logging::printf("TIMER: It's one of those old broken AMD HPETs. Use legacy mode.\n");
        legacy_only = true;
      }
      break;
    default:
      // Before you blindly enable features for other HPETs, check
      // Linux and FreeBSD source for quirks!
      Logging::printf("TIMER: Unknown HPET vendor ID. We only trust legacy mode.\n");
      legacy_only = true;
    }

    if (legacy_only and not (_reg->cap & LEG_RT_CAP)) {
      // XXX Implement PIT mode
      Logging::printf("TIMER: We want legacy mode, but the timer doesn't support it.\n");
      return false;
    }

    // Figure out how many HPET timers are usable
    if (_verbose)
      Logging::printf("TIMER: HPET: cap %x config %x period %d\n", _reg->cap, _reg->config, _reg->period);
    unsigned timers = ((_reg->cap >> 8) & 0x1F) + 1;

    _usable_timers = 0;

    uint32 combined_irqs = ~0U;
    for (unsigned i=0; i < timers; i++) {
      if (_verbose)
        Logging::printf("TIMER: HPET Timer[%d]: config %x int %x\n", i, _reg->timer[i].config, _reg->timer[i].int_route);
      if ((_reg->timer[i].config | _reg->timer[i].int_route) == 0) {
        Logging::printf("TIMER:\tTimer[%d] seems bogus. Ignore.\n", i);
        continue;
      }

      if ((_reg->timer[i].config & FSB_INT_DEL_CAP) == 0) {
        // Comparator is not MSI capable.
        combined_irqs &= _reg->timer[i].int_route;
      }

      _timer[_usable_timers]._no   = i;
      _timer[_usable_timers]._reg = &_reg->timer[i];
      _usable_timers++;
    }

    // Reduce the number of comparators to ones we can assign
    // individual IRQs.
    // XXX This overcompensates and is suboptimal. We need
    // backtracking search. It's a bin packing problem.
    unsigned irqs = Cpu::popcount(combined_irqs);
    if (irqs < _usable_timers) {
      Logging::printf("TIMER: Reducing usable timers from %u to %u. Combined IRQ mask is %x\n",
                      _usable_timers, irqs, combined_irqs);
      _usable_timers = irqs;
    }


    if (_usable_timers == 0) {
      // XXX Can this happen?
      Logging::printf("TIMER: No suitable HPET timer.\n");
      return false;

    }

    if (legacy_only) {
      Logging::printf("TIMER: Use one timer in legacy mode.\n");
      _usable_timers = 1;
    } else
      Logging::printf("TIMER: Found %u usable timers.\n", _usable_timers);

    if (_usable_timers > _mb.hip()->cpu_count()) {
      _usable_timers = _mb.hip()->cpu_count();
      Logging::printf("TIMER: More timers than CPUs. (Good!) Use only %u timers.\n",
                      _usable_timers);
    }

    for (unsigned i = 0; i < _usable_timers; i++) {
      // Interrupts will be disabled now. Will be enabled when the
      // corresponding per_cpu thread comes up.
      _timer[i]._reg->config |= MODE32_CNF;
      _timer[i]._reg->config &= ~(INT_ENB_CNF | TYPE_CNF);
      _timer[i]._reg->comp64 = 0;
    }

    // Disable counting and IRQs. Program legacy mode as requested.
    _reg->isr     = ~0U;
    _reg->config &= ~(ENABLE_CNF | LEG_RT_CNF);
    _reg->main    = 0ULL;
    _reg->config |= (legacy_only ? LEG_RT_CNF : 0);

    // HPET configuration

    uint64 freq = 1000000000000000ULL;
    Math::div64(freq, _reg->period);
    _timer_freq = freq;
    Logging::printf("TIMER: HPET ticks with %u HZ.\n", _timer_freq);

    return true;
  }

  // Start HPET counter at value. HPET might be 32-bit. In this case,
  // the upper 32-bit of value are ignored.
  void hpet_start(uint64 value)
  {
    assert((_reg->config & ENABLE_CNF) == 0);
    _reg->main    = value;
    _reg->config |= ENABLE_CNF;
  }

  void start_thread(ServiceThreadFn fn,
                    unsigned prio, phy_cpu_no cpu)
  {
    MessageHostOp msg = MessageHostOp::alloc_service_thread(fn, this, "timer", prio, cpu);
    if (!_mb.bus_hostop.send(msg))
      Logging::panic("%s thread creation failed", __func__);
  }

  // Initialize PIT to tick every period_us microseconds.
  void pit_init(unsigned period_us)
  {
    MessageHostOp msg1(MessageHostOp::OP_ALLOC_IOIO_REGION, (PIT_PORT << 8) |  2);
    if (not _mb.bus_hostop.send(msg1))
      Logging::panic("TIMER: Couldn't grab PIT ports.\n");

    unsigned long long value = PIT_FREQ*period_us;
    Math::div64(value, 1000000);

    if ((value == 0) || (value > 65535)) {
      Logging::printf("TIMER: Bogus PIT period %uus. Set to default (%llu us)\n",
                      period_us, PIT_DEFAULT_PERIOD);
      period_us = PIT_DEFAULT_PERIOD;
      value = (PIT_FREQ*PIT_DEFAULT_PERIOD) / 1000000ULL;
    }

    outb(0x34, PIT_PORT + 3);
    outb(value, PIT_PORT);
    outb(value>>8, PIT_PORT);

    _timer_freq = 1000000U / period_us;

    Logging::printf("TIMER: PIT initalized. Ticks every %uus (period %llu, %uHZ).\n",
                    period_us, value, _timer_freq);
  }

  void xcpu_wakeup_thread() NORETURN
  {
    Utcb         *utcb = BaseProgram::myutcb();
    phy_cpu_no     cpu = BaseProgram::mycpu();
    PerCpu * const our = _per_cpu[cpu];

    _xcpu_up.up();

    WorkerMessage m;
    m.type = our->has_timer ? WorkerMessage::XCPU_REQUEST : WorkerMessage::TIMER_IRQ;
    m.data = NULL;

    while (1) {
      our->xcpu_sm.downmulti();

      //Logging::printf("TIMER: CPU%u %s wakeup\n", cpu, our->has_timer ? "XCPU" : "XIRQ");

      utcb->set_header(0, 0);
      *utcb << m;
      unsigned res = nova_call(our->worker_pt);
      if (res != NOVA_ESUCCESS) {
        Logging::printf("TIMER: CPU%u xcpu call error %u\n", cpu, res);
        // XXX Die?
      }
    }
  }

public:

  static void do_xcpu_wakeup_thread(void *t) REGPARM(0) NORETURN
  { reinterpret_cast<PerCpuTimerService *>(t)->xcpu_wakeup_thread(); }

  static void do_per_cpu_worker(void *t, Utcb *u) REGPARM(0)
  { 
    //Logging::printf("PT TLS %p UTCB %p %x: %08x %08x\n", t, u, u->head.mtr, u->msg[0], u->msg[1]);
    reinterpret_cast<PerCpuTimerService *>(t)->per_cpu_worker(u);
  }

  bool receive(MessageIrq &msg)
  {
    phy_cpu_no cpu = BaseProgram::mycpu();
    if ((msg.type == MessageIrq::ASSERT_IRQ) && (_per_cpu[cpu]->irq == msg.line)) {

      if (_reg) {
        // ACK the IRQ in non-MSI HPET mode.
        if (_per_cpu[cpu]->ack != 0)
          _reg->isr = _per_cpu[cpu]->ack;
      } else {
        // PIT mode. Increment our clock.
        while (not __sync_bool_compare_and_swap(&_pit_ticks, _pit_ticks, _pit_ticks+1))
          {}
      }

      MEMORY_BARRIER;

      WorkerMessage m;
      m.type = WorkerMessage::TIMER_IRQ;
      m.data = NULL;

      *BaseProgram::myutcb() << m;

      unsigned res = nova_call(_per_cpu[cpu]->worker_pt);
      if (res != NOVA_ESUCCESS)
        Logging::printf("TIMER: CPU%u irq call fail %u\n", cpu, res);

      return true;
    }

    return false;
  }

  unsigned alloc_crd() { return Crd(alloc_cap(), 0, DESC_CAP_ALL).value(); }

  unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap, cap_sel pid) {
    unsigned res = ENONE;
    unsigned op;

    check1(EPROTO, input.get_word(op));

    phy_cpu_no cpu = BaseProgram::mycpu();
    PerCpu * const our = _per_cpu[cpu];

    switch (op) {
    case ParentProtocol::TYPE_OPEN:
      {
        ClientData *data = 0;
        check1(res, res = _storage.alloc_client_data(utcb, data, input.received_cap(), this));
        free_cap = false;
        // XXX Allocate nr on all CPUs
        data->nr = our->abstimeouts.alloc(data);
        data->cpu = cpu;
        if (!data->nr) return EABORT;

        utcb << Utcb::TypedMapCap(data->get_identity());
        //Logging::printf("ts:: new client data %x parent %x\n", data->identity, data->pseudonym);
        return res;
      }
    case ParentProtocol::TYPE_CLOSE:
      {
        ClientData *data = 0;
        ClientDataStorage<ClientData, PerCpuTimerService>::Guard guard_c(&_storage, utcb, this);
        check1(res, res = _storage.get_client_data(utcb, data, input.identity()));

        Logging::printf("ts:: close session for %x\n", data->get_identity());
        // XXX We are leaking an abstimeout slot!! XXX
        return _storage.free_client_data(utcb, data, this);
      }
    case TimerProtocol::TYPE_REQUEST_TIMER:
      {
        ClientData *data = 0;
        ClientDataStorage<ClientData, PerCpuTimerService>::Guard guard_c(&_storage, utcb, this);
        if (res = _storage.get_client_data(utcb, data, input.identity())) return res;

        TimerProtocol::MessageTimer msg = TimerProtocol::MessageTimer(0);
        if (input.get_word(msg)) return EABORT;
        if (data->cpu != cpu)    return EABORT;

        COUNTER_INC("request to");
        assert(data->nr < CLIENTS);
        data->abstimeout = msg.abstime;
        // XXX Don't send request. Just send nr
        WorkerMessage m;
        m.type = WorkerMessage::CLIENT_REQUEST;
        m.data = data;

        utcb.add_frame() << m;
        unsigned res = nova_call(our->worker_pt);
        utcb.drop_frame();

        return (res == NOVA_ESUCCESS) ? ENONE : EABORT;
      }
    case TimerProtocol::TYPE_REQUEST_LAST_TIMEOUT:
      {
        ClientData *data = 0;
        ClientDataStorage<ClientData, PerCpuTimerService>::Guard guard_c(&_storage, utcb, this);
        if (res = _storage.get_client_data(utcb, data, input.identity())) return res;

        utcb << Cpu::xchg(&data->count, 0U);
        return ENONE;
      }
    case TimerProtocol::TYPE_REQUEST_TIME:
      {
        ClientData *data = 0;
        ClientDataStorage<ClientData, PerCpuTimerService>::Guard guard_c(&_storage, utcb, this);
        if (res = _storage.get_client_data(utcb, data, input.identity())) return res;

        MessageTime msg;
        uint64 counter = _reg ?
          our->clock_sync.current_hpet(_reg->counter[0]) :
          uninterruptible_read(_pit_ticks);

        msg.timestamp = _mb.clock()->clock(TimerProtocol::WALLCLOCK_FREQUENCY);
        msg.wallclocktime = Math::muldiv128(counter, TimerProtocol::WALLCLOCK_FREQUENCY, _timer_freq);
        utcb << msg;
        return ENONE;
      }
    default:
      return EPROTO;
    }
  }

  // Returns initial value of timecounter register.
  uint64
  wallclock_init()
  {
    unsigned iobase = 0x70;

    // RTC ports
    MessageHostOp msg1(MessageHostOp::OP_ALLOC_IOIO_REGION, (iobase << 8) |  1);
    if (not _mb.bus_hostop.send(msg1)) {
      Logging::printf("TIMER: %s failed to allocate ports %x+2\n", __PRETTY_FUNCTION__, iobase);
      return 0;
    }

    BasicRtc rtc(_mb.bus_hwioin, _mb.bus_hwioout, iobase);
    if (_slow_wallclock) rtc.rtc_sync(_mb.clock());
    uint64 msecs  = rtc.rtc_wallclock();
    uint64 ticks  = Math::muldiv128(msecs, _timer_freq, TimerProtocol::WALLCLOCK_FREQUENCY);

    tm_simple time;

    gmtime(msecs / TimerProtocol::WALLCLOCK_FREQUENCY, &time);
    if (_verbose)
      Logging::printf("TIMER: %d.%02d.%02d %d:%02d:%02d\n", time.mday, time.mon, time.year, time.hour, time.min, time.sec);

    return ticks;
  }


  PerCpuTimerService(Motherboard &mb, unsigned cap, unsigned cap_order,
                     bool hpet_force_legacy, bool force_pit, unsigned pit_period_us,
                     bool verbose, bool slow_wallclock)
    : CapAllocator(cap, cap, cap_order),
      _mb(mb), _bus_hwioout(mb.bus_hwioout), _assigned_irqs(0), _verbose(verbose),
      _slow_wallclock(slow_wallclock)
  {
    // XXX Lots of pointless log->phy cpu index conversions...

    log_cpu_no cpus = mb.hip()->cpu_count();
    assert(cpus <= Config::MAX_CPUS);

    for (phy_cpu_no i = 0; i < mb.hip()->cpu_desc_count(); i++) {
      const Hip_cpu &c = mb.hip()->cpus()[i];
      assert(i <  (sizeof(_per_cpu) / sizeof(_per_cpu[0])));
      _per_cpu[i] = c.enabled() ? (new(64) PerCpu) : NULL;
    }

    if (force_pit or not hpet_init(hpet_force_legacy)) {
      _reg = NULL;
      _usable_timers = 1;
      Logging::printf("TIMER: HPET initialization %s. Try PIT instead.\n", force_pit ? "skipped" : "failed");
      pit_init(pit_period_us);
    }

    // HPET: Counter is running, IRQs are off.
    // PIT:  PIT is programmed to run in periodic mode, if HPET didn't work for us.

    uint64 clocks_per_tick = static_cast<uint64>(mb.hip()->freq_tsc) * 1000 * CPT_RESOLUTION;
    Math::div64(clocks_per_tick, _timer_freq);
    if (verbose)
      Logging::printf("TIMER: %llu+%04llu/%u TSC ticks per timer tick.\n", clocks_per_tick/CPT_RESOLUTION, clocks_per_tick%CPT_RESOLUTION, CPT_RESOLUTION);
    _nominal_tsc_ticks_per_timer_tick = clocks_per_tick;

    // Get wallclock time
    uint64 initial_counter = wallclock_init();
    if (_reg)
      hpet_start(initial_counter);
    else
      _pit_ticks = initial_counter;

    log_cpu_no cpu_cpu[cpus];
    log_cpu_no part_cpu[cpus];

    size_t n = mb.hip()->cpu_desc_count();
    Topology::divide(mb.hip()->cpus(), n,
                     _usable_timers,
                     part_cpu,
                     cpu_cpu);

    // Create remote slot mapping and initialize per cpu data structure
    for (log_cpu_no i = 0; i < cpus; i++) {
      phy_cpu_no pcpu = mb.hip()->cpu_physical(i);

      // Provide initial hpet counter to get high 32-bit right.
      _per_cpu[pcpu]->clock_sync = ClockSyncInfo(initial_counter);
      _per_cpu[pcpu]->abstimeouts.init();
      _per_cpu[pcpu]->last_to = ~0ULL;

      // Create per CPU worker
      MessageHostOp msg = MessageHostOp::alloc_service_portal(&_per_cpu[pcpu]->worker_pt,
                                                              do_per_cpu_worker, this, Crd(0), pcpu);
      if (!_mb.bus_hostop.send(msg))
        Logging::panic("%s worker creation failed", __func__);
    }


    // Bootstrap IRQ handlers. IRQs are disabled. Each worker enables
    // its IRQ when it comes up.

    for (unsigned i = 0; i < _usable_timers; i++) {
      phy_cpu_no cpu = mb.hip()->cpu_physical(part_cpu[i]);
      if (_verbose)
        Logging::printf("TIMER: CPU%u owns Timer%u.\n", cpu, i);

      _per_cpu[cpu]->has_timer = true;
      if (_reg)
        _per_cpu[cpu]->timer = &_timer[i];

      // We allocate a couple of unused slots if there is an odd
      // combination of CPU count and usable timers. Who cares.
      _per_cpu[cpu]->slots = new RemoteSlot[mb.hip()->cpu_count() / _usable_timers];

      // Attach to IRQ
      if (_reg)
        attach_timer_irq(mb.bus_hostop, &_timer[i], cpu);
      else {
        // Attach to PIT instead.
        MessageHostOp msg = MessageHostOp::attach_irq(PIT_IRQ, cpu, false, "pit");
        if (not mb.bus_hostop.send(msg)) Logging::panic("Could not attach IRQ.\n");
        _per_cpu[cpu]->irq = PIT_IRQ;
      }
    }

    _xcpu_up = KernelSemaphore(alloc_cap(), true);
    for (log_cpu_no i = 0; i < cpus; i++)
      // Create wakeup semaphores
      _per_cpu[mb.hip()->cpu_physical(i)]->xcpu_sm = KernelSemaphore(alloc_cap(), true);

    for (log_cpu_no i = 0; i < cpus; i++) {
      phy_cpu_no cpu = mb.hip()->cpu_physical(i);
      if (not _per_cpu[cpu]->has_timer) {
        PerCpu &remote = *_per_cpu[mb.hip()->cpu_physical(cpu_cpu[i])];

        _per_cpu[cpu]->remote_sm   = remote.xcpu_sm;
        _per_cpu[cpu]->remote_slot = &remote.slots[remote.slot_count];

        // Fake a ClientData for this CPU.
        _per_cpu[cpu]->remote_slot->data.set_identity(_per_cpu[cpu]->xcpu_sm.sm());
        _per_cpu[cpu]->remote_slot->data.abstimeout = 0;
        _per_cpu[cpu]->remote_slot->data.nr = remote.abstimeouts.alloc(&_per_cpu[cpu]->remote_slot->data);

        if (verbose)
          Logging::printf("TIMER: CPU%u maps to CPU%u slot %u.\n",
                          i, mb.hip()->cpu_physical(cpu_cpu[i]), remote.slot_count);

        remote.slot_count ++;
      }
    }

    unsigned xcpu_threads_started = 0;
    for (log_cpu_no i = 0; i < cpus; i++) {
      if (_reg) {
        _per_cpu[i]->clock_sync.fetch(_reg->counter[0]);
        if (_per_cpu[i]->has_timer) {
          if (_verbose)
            Logging::printf("TIMER: Enable interrupts for CPU%u.\n", i);
          _per_cpu[i]->timer->_reg->config |= INT_ENB_CNF;
        }
      }

      // Enable XCPU threads for CPUs that either have to serve or
      // need to query other CPUs.
      if ((_per_cpu[i]->slot_count > 0) or not _per_cpu[i]->has_timer) {
        // Create wakeup thread
        start_thread(PerCpuTimerService::do_xcpu_wakeup_thread, 1, mb.hip()->cpu_physical(i));
        xcpu_threads_started++;
      }
    }

    // XXX Do we need those when we have enough timers for all CPUs?
    Logging::printf("TIMER: Waiting for %u XCPU threads to come up.\n", xcpu_threads_started);
    while (xcpu_threads_started-- > 0)
      _xcpu_up.down();

    mb.bus_hostirq.add(this, receive_static<MessageIrq>);
    Logging::printf("TIMER: Initialized!\n");
  }

};

static bool default_force_hpet_legacy = false;
static bool default_force_pit         = false;
static bool default_verbose           = false;
static bool default_slow_wallclock    = false;

PARAM_HANDLER(timer_hpet_legacy)    { default_force_hpet_legacy = true; }
PARAM_HANDLER(timer_force_pit)      { default_force_pit         = true; }
PARAM_HANDLER(timer_verbose)        { default_verbose 	        = true; }
PARAM_HANDLER(timer_slow_wallclock) { default_slow_wallclock    = false; }


PARAM_HANDLER(service_per_cpu_timer,
	      "service_per_cpu_timer:[slow_wallclock=0][,hpet_force_legacy=0][,force_pit=0][,pit_period_us]")
{
  unsigned cap_region = alloc_cap_region(1 << 12, 12);
  bool     slow_wallclock = (argv[0] == ~0U) ? default_slow_wallclock : argv[0];
  bool     hpet_legacy    = (argv[1] == ~0U) ? default_force_hpet_legacy : argv[1];
  bool     force_pit      = (argv[2] == ~0U) ? default_force_pit : argv[2];
  unsigned pit_period_us  = (argv[3] == ~0U) ? PIT_DEFAULT_PERIOD : argv[3];

  PerCpuTimerService *h = new(16) PerCpuTimerService(mb, cap_region, 12, hpet_legacy, force_pit,
                                                     pit_period_us, default_verbose, slow_wallclock);

  MessageHostOp msg(h, "/timer", reinterpret_cast<unsigned long>(StaticPortalFunc<PerCpuTimerService>::portal_func));
  msg.crd_t = Crd(cap_region, 12, DESC_TYPE_CAP).value();
  if (!cap_region || !mb.bus_hostop.send(msg))
    Logging::panic("starting of timer service failed");
}
