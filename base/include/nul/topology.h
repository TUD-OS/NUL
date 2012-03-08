#pragma once

#include <service/quicksort.h>
#include <sys/hip.h>

class Topology {

  static bool cpu_smallereq(Hip_cpu const &a, Hip_cpu const &b)
  {
    unsigned a_v = (a.package << 16) | (a.core << 8) | (a.thread);
    unsigned b_v = (b.package << 16) | (b.core << 8) | (b.thread);
    return (a_v <= b_v);
  }

public:
  static void
  divide(const Hip_cpu top[],
         size_t &n,           // IN: Number of Hip_cpu descriptors
         // OUT: Number of CPUs in mappings
         unsigned &parts,     // IN: Desired parts
         // OUT: Actual parts (<= n)
         log_cpu_no part_cpu[], // Part -> responsible CPU mapping
         log_cpu_no cpu_cpu[]   // CPU -> responsible CPU mapping
         )
  {
    Hip_cpu local[n];

    // Copy CPU descriptors into consecutive list.
    unsigned cpus = 0;
    for (unsigned i = 0; i < n; i++) {
      if (top[i].enabled()) {
        local[cpus] = top[i];
        local[cpus].reserved = cpus; // Logical CPU number
        cpus++;
      }
    }

    if (cpus < parts)
      parts = cpus;

    // Logging::printf("top_divide: Divide %u CPU(s) (%u dead) into %u part(s).\n",
    //                 cpus, n - cpus, parts);
    n = cpus;

    Quicksort<Hip_cpu>::quicksort(cpu_smallereq, local, 0, n - 1);

    // Divide list into parts. Update mappings.
    unsigned cpus_per_part = n / parts;
    unsigned cpus_rest     = n % parts;
    log_cpu_no cur_cpu     = 0;
    for (unsigned i = 0; i < parts; i++) {
      unsigned cpus_to_spend = cpus_per_part;
      if (cpus_rest) { cpus_to_spend++; cpus_rest--; }
      part_cpu[i] = local[cur_cpu].reserved;
      for (unsigned j = 0; j < cpus_to_spend; j++, cur_cpu++) {
        cpu_cpu[local[cur_cpu].reserved] = part_cpu[i];
      }
    }

  }
};

// EOF
