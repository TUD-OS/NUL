/*
 * (C) 2011 Alexander Boettcher
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include <nul/program.h>
#include <sigma0/console.h>
#include <wvtest.h>

namespace ab {
class IPCTest : public NovaProgram, public ProgramConsole
{
  private:
    unsigned pt_wo;

  public:

  void run(Utcb *utcb, Hip *hip)
    {

      init(hip);
      init_mem(hip);

      console_init("syscall tests", new Semaphore(alloc_cap(), true));
      _console_data.log = new LogProtocol(alloc_cap(LogProtocol::CAP_SERVER_PT + hip->cpu_desc_count()));

      Logging::printf("Hello\n");

      _virt_phys.debug_dump("");

      if (!create_setup(utcb->head.nul_cpunr))
        Logging::panic("failure - creation of setup\n");

      Logging::printf("\nTest output should look like that:\n");
      Logging::printf("\t 'tX: ... success/failure - reason: XYZ'\n\n");

      Logging::printf("          Test I  - IPC aborts\n");
      if (!test_ipc_aborts())
        Logging::printf("failure - test I  run\n");

      Logging::printf("\n          Test II - Lookup\n");
      if (!lookup_test())
        Logging::printf("failure - test II run\n");
    }

  static void portal_pf(IPCTest *tls, Utcb *utcb) __attribute__((regparm(0)))
  {
    //Logging::printf("got pagefault\n");

    *reinterpret_cast<unsigned long *>(0) = 0xaffe;

    utcb->head.mtr = 0; //transfer nothing back
    asmlinkage_protect("g"(tls), "g"(utcb));
  }

  static void portal_wo(IPCTest *tls, Utcb *utcb) __attribute__((regparm(0)))
  {
    //Logging::printf("got ipc\n");

    *reinterpret_cast<unsigned long *>(0) = 0xaffe;

    utcb->head.mtr = 0; //transfer nothing back
    asmlinkage_protect("g"(tls), "g"(utcb));
  }

  private:

  bool create_setup(unsigned long cpu) {

    unsigned exc_base_wo, exc_base_pf;
    unsigned pt_pf;
    unsigned res, i;

    exc_base_wo = alloc_cap(16);
    pt_wo       = alloc_cap();
    if (!exc_base_wo || !pt_wo) return false;

    unsigned cap_ec = create_ec4pt(this, cpu, exc_base_wo, 0, alloc_cap());
    if (!cap_ec) return false;

    unsigned long func_wo = reinterpret_cast<unsigned long>(portal_wo);
    res = nova_create_pt(pt_wo, cap_ec, func_wo, 0);
    if (res) return false;

    pt_pf       = exc_base_wo + 0xe;

    unsigned cap_pf[1];
    for (i=0; i < 1; i++)  {
      exc_base_pf = alloc_cap(16);
      if (!exc_base_pf) return false;

      cap_pf[i] = create_ec4pt(this, cpu, exc_base_pf, 0, alloc_cap());
      if (!cap_pf[i]) return false;

      unsigned long func_pf = reinterpret_cast<unsigned long>(portal_pf);
      res = nova_create_pt(pt_pf, cap_pf[i], func_pf, MTD_GPR_ACDB | MTD_GPR_BSD | MTD_QUAL | MTD_RIP_LEN | MTD_RSP);
      if (res) return false;

      pt_pf       = exc_base_pf + 0xe;
    }

    return true;
  }

  bool test_ipc_aborts() {
    unsigned res;

    Logging::printf("  <------ exc pf ---------   <------- ipc ------------ \n");
    Logging::printf("A                          B                           C*\n");
    Logging::printf(" 3. dies (unhand. exc)      2. cause pf exc             1. sends ipc to B\n");
    Logging::printf(" 4. kernel sets A dead      5. will die - unhand. pf    6. gets ipc abort \n");
    Logging::printf("                                                        7. sends ipc to B\n");
    Logging::printf("                                                        8. gets ipc timeout\n");

    Logging::printf("A: ... ");
    res = nova_call(pt_wo);
    Logging::printf("%s - reason: return code 0x%x ?= 0x%x\n", res == NOVA_EABORT ? "success" : "failure", NOVA_EABORT, res);
    WVPASSEQ(res, 0U + NOVA_EABORT);

    Logging::printf("B: ... ");
    res = nova_call(pt_wo);
    Logging::printf("%s - reason: return code 0x%x ?= 0x%x\n", res == NOVA_ETIMEOUT ? "success" : "failure", NOVA_ETIMEOUT, res);
    WVPASSEQ(res, 0U + NOVA_ETIMEOUT);

    Logging::printf("C: ... ");
    res = nova_call(pt_wo);
    Logging::printf("%s - reason: return code 0x%x ?= 0x%x\n", res == NOVA_ETIMEOUT ? "success" : "failure", NOVA_ETIMEOUT, res);
    WVPASSEQ(res, 0U + NOVA_ETIMEOUT);

    return true;
  }

  bool lookup_test() {
    unsigned res, crdout;
    unsigned sm = alloc_cap();
    if (!sm) return false;

    res = nova_create_sm(sm);
    Logging::printf("%s - create_sm     - reason: return code 0x%x ?= 0x%x -> sm=0x%x\n", res == NOVA_ESUCCESS ? "success" : "failure", NOVA_ESUCCESS, res, sm);
    WVPASSEQ(res, 0U + NOVA_ESUCCESS);
    if (res != NOVA_ESUCCESS) return false;

    res = nova_syscall(NOVA_LOOKUP,  Crd(sm, 0, DESC_CAP_ALL).value(), 0, 0, 0, &crdout);
    Logging::printf("%s - lookup(0x%x) - reason: crd.raw=0x%x should be != 0x0\n",
                    ((res == NOVA_ESUCCESS) && (crdout & DESC_RIGHTS_ALL)) ? "success" : "failure", sm, crdout);
    WVPASSEQ(res, 0U + NOVA_ESUCCESS);
    WVPASS(crdout & DESC_RIGHTS_ALL);

    res = nova_revoke(Crd(sm, 0, DESC_CAP_ALL), true);
    Logging::printf("%s - revoke(0x%x) - reason: return code 0x%x ?= 0x%x\n", res == NOVA_ESUCCESS ? "success" : "failure", sm, NOVA_ESUCCESS, res);
    WVPASSEQ(res, 0U + NOVA_ESUCCESS);
    if (res != NOVA_ESUCCESS) return false;

    res  = nova_syscall(NOVA_LOOKUP,  Crd(sm, 0, DESC_CAP_ALL).value(), 0, 0, 0, &crdout);
    Logging::printf("%s - lookup(0x%x) - reason: crd.raw=0x%x should be == 0x0\n",
                    ((res == NOVA_ESUCCESS) && (crdout == 0U)) ? "success" : "failure", sm, crdout);
    WVPASSEQ(res, 0U + NOVA_ESUCCESS);
    WVPASSEQ(crdout, 0U);
    if (res || crdout != 0U) {
      Logging::printf("          lookup(0x%x) -> cap=0x%x order=%u rights=0x%x\n",
                      sm, Crd(crdout).cap(), Crd(crdout).order(), crdout & DESC_RIGHTS_ALL);
      return false;
    }

    return true;
  }
};

} /* namespace */

ASMFUNCS(ab::IPCTest, WvTest)
