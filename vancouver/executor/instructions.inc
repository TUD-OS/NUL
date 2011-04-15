// -*- Mode: C++ -*-
// Automagically generated. Do not touch.
static void __attribute__((regparm(3))) exec_00_add_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al;lock addb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_01_add_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax;lock addw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_01_add_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax;lock addl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_02_add_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; addb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_03_add_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; addw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_03_add_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; addl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_04_add_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; addb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_05_add_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; addw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_05_add_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; addl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_06_push__es_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_PUSH<1>(&cache->_cpu->es.sel); }
static void __attribute__((regparm(3))) exec_06_push__es_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_PUSH<2>(&cache->_cpu->es.sel); }
static void __attribute__((regparm(3))) exec_07_pop__es_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned sel;cache->helper_POP<1>(&sel) || cache->set_segment(&cache->_cpu->es, sel);; }
static void __attribute__((regparm(3))) exec_07_pop__es_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned sel;cache->helper_POP<2>(&sel) || cache->set_segment(&cache->_cpu->es, sel);; }
static void __attribute__((regparm(3))) exec_08_or_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al;lock orb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_09_or_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax;lock orw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_09_or_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax;lock orl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0a_or_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; orb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0b_or_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; orw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0b_or_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; orl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0c_or_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; orb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0d_or_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; orw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0d_or_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; orl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0e_push__cs_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_PUSH<1>(&cache->_cpu->cs.sel); }
static void __attribute__((regparm(3))) exec_0e_push__cs_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_PUSH<2>(&cache->_cpu->cs.sel); }
static void __attribute__((regparm(3))) exec_0f0000_sldt_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 move<1>(tmp_dst, &cache->_cpu->ld.sel); }
static void __attribute__((regparm(3))) exec_0f0008_str_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 move<1>(tmp_dst, &cache->_cpu->tr.sel); }
static void __attribute__((regparm(3))) exec_0f0010_lldt_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LLDT(*reinterpret_cast<unsigned short *>(tmp_src)); }
static void __attribute__((regparm(3))) exec_0f0018_ltr_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LTR(*reinterpret_cast<unsigned short *>(tmp_src)); }
static void __attribute__((regparm(3))) exec_0f0100_sgdt_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_SGDT<1>(); }
static void __attribute__((regparm(3))) exec_0f0100_sgdt_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_SGDT<2>(); }
static void __attribute__((regparm(3))) exec_0f0108_sidt_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_SIDT<1>(); }
static void __attribute__((regparm(3))) exec_0f0108_sidt_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_SIDT<2>(); }
static void __attribute__((regparm(3))) exec_0f0110_lgdt_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LGDT<1>(); }
static void __attribute__((regparm(3))) exec_0f0110_lgdt_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LGDT<2>(); }
static void __attribute__((regparm(3))) exec_0f0118_lidt_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LIDT<1>(); }
static void __attribute__((regparm(3))) exec_0f0118_lidt_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LIDT<2>(); }
static void __attribute__((regparm(3))) exec_0f0120_smsw_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 move<1>(tmp_dst, &cache->_cpu->cr0); }
static void __attribute__((regparm(3))) exec_0f0130_lmsw_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 if (cache->cpl0_test()) return;cache->helper_LMSW(*reinterpret_cast<unsigned short *>(tmp_src)); }
static void __attribute__((regparm(3))) exec_0f0138_invlpg_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 if (cache->cpl0_test()) return;cache->helper_INVLPG(); }
static void __attribute__((regparm(3))) exec_0f06_clts_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 if (cache->cpl0_test()) return;cache->helper_CLTS(); }
static void __attribute__((regparm(3))) exec_0f08_invd_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 if (cache->cpl0_test()) return;cache->helper_INVD(); }
static void __attribute__((regparm(3))) exec_0f09_wbinvd_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 if (cache->cpl0_test()) return;cache->helper_WBINVD(); }
static void __attribute__((regparm(3))) exec_0f0b_ud2a_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_UD2A(); }
static void __attribute__((regparm(3))) exec_0f1f_nopl___eax__0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		  ; }
static void __attribute__((regparm(3))) exec_0f20_mov__cr0__edx_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 if (cache->cpl0_test()) return;cache->helper_MOV__CR0__EDX(); }
static void __attribute__((regparm(3))) exec_0f21_mov__db0__edx_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 if (cache->cpl0_test()) return;cache->helper_MOV__DB0__EDX(); }
static void __attribute__((regparm(3))) exec_0f22_mov__edx__cr0_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 if (cache->cpl0_test()) return;cache->helper_MOV__EDX__CR0(); }
static void __attribute__((regparm(3))) exec_0f23_mov__edx__db0_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 if (cache->cpl0_test()) return;cache->helper_MOV__EDX__DB0(); }
static void __attribute__((regparm(3))) exec_0f30_wrmsr_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->send_message(CpuMessage::TYPE_WRMSR); }
static void __attribute__((regparm(3))) exec_0f30_wrmsr_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->send_message(CpuMessage::TYPE_WRMSR); }
static void __attribute__((regparm(3))) exec_0f31_rdtsc_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->send_message(CpuMessage::TYPE_RDTSC); }
static void __attribute__((regparm(3))) exec_0f31_rdtsc_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->send_message(CpuMessage::TYPE_RDTSC); }
static void __attribute__((regparm(3))) exec_0f32_rdmsr_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->send_message(CpuMessage::TYPE_RDMSR); }
static void __attribute__((regparm(3))) exec_0f32_rdmsr_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->send_message(CpuMessage::TYPE_RDMSR); }
static void __attribute__((regparm(3))) exec_0f34_sysenter_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_SYSENTER(); }
static void __attribute__((regparm(3))) exec_0f35_sysexit_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_SYSEXIT(); }
static void __attribute__((regparm(3))) exec_0f40_cmovo_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("jno 1f;mov (%%edx), %%eax;mov %%eax, (%%ecx);1:" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f41_cmovno_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("jo 1f;mov (%%edx), %%eax;mov %%eax, (%%ecx);1:" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f42_cmovb_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("jae 1f;mov (%%edx), %%eax;mov %%eax, (%%ecx);1:" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f43_cmovae_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("jb 1f;mov (%%edx), %%eax;mov %%eax, (%%ecx);1:" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f44_cmove_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("jne 1f;mov (%%edx), %%eax;mov %%eax, (%%ecx);1:" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f45_cmovne_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("je 1f;mov (%%edx), %%eax;mov %%eax, (%%ecx);1:" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f46_cmovbe_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("ja 1f;mov (%%edx), %%eax;mov %%eax, (%%ecx);1:" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f47_cmova_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("jbe 1f;mov (%%edx), %%eax;mov %%eax, (%%ecx);1:" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f48_cmovs_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("jns 1f;mov (%%edx), %%eax;mov %%eax, (%%ecx);1:" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f49_cmovns_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("js 1f;mov (%%edx), %%eax;mov %%eax, (%%ecx);1:" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f4a_cmovp_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("jnp 1f;mov (%%edx), %%eax;mov %%eax, (%%ecx);1:" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f4b_cmovnp_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("jp 1f;mov (%%edx), %%eax;mov %%eax, (%%ecx);1:" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f4c_cmovl_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("jge 1f;mov (%%edx), %%eax;mov %%eax, (%%ecx);1:" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f4d_cmovge_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("jl 1f;mov (%%edx), %%eax;mov %%eax, (%%ecx);1:" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f4e_cmovle_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("jg 1f;mov (%%edx), %%eax;mov %%eax, (%%ecx);1:" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f4f_cmovg_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("jle 1f;mov (%%edx), %%eax;mov %%eax, (%%ecx);1:" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f80_jo_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jno 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f80_jo_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jno 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f81_jno_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jo 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f81_jno_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jo 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f82_jb_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jae 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f82_jb_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jae 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f83_jae_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jb 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f83_jae_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jb 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f84_je_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jne 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f84_je_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jne 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f85_jne_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("je 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f85_jne_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("je 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f86_jbe_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("ja 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f86_jbe_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("ja 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f87_ja_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jbe 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f87_ja_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jbe 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f88_js_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jns 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f88_js_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jns 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f89_jns_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("js 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f89_jns_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("js 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f8a_jp_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jnp 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f8a_jp_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jnp 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f8b_jnp_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jp 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f8b_jnp_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jp 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f8c_jl_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jge 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f8c_jl_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jge 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f8d_jge_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jl 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f8d_jge_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jl 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f8e_jle_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jg 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f8e_jle_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jg 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f8f_jg_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jle 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f8f_jg_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jle 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_0f9000_seto_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("seto (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f9100_setno_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("setno (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f9200_setb_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("setb (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f9300_setae_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("setae (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f9400_sete_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("sete (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f9500_setne_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("setne (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f9600_setbe_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("setbe (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f9700_seta_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("seta (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f9800_sets_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("sets (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f9900_setns_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("setns (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f9a00_setp_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("setp (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f9b00_setnp_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("setnp (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f9c00_setl_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("setl (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f9d00_setge_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("setge (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f9e00_setle_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("setle (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0f9f00_setg_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("setg (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fa0_push__fs_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_PUSH<1>(&cache->_cpu->fs.sel); }
static void __attribute__((regparm(3))) exec_0fa0_push__fs_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_PUSH<2>(&cache->_cpu->fs.sel); }
static void __attribute__((regparm(3))) exec_0fa1_pop__fs_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned sel;cache->helper_POP<1>(&sel) || cache->set_segment(&cache->_cpu->fs, sel);; }
static void __attribute__((regparm(3))) exec_0fa1_pop__fs_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned sel;cache->helper_POP<2>(&sel) || cache->set_segment(&cache->_cpu->fs, sel);; }
static void __attribute__((regparm(3))) exec_0fa2_cpuid_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->send_message(CpuMessage::TYPE_CPUID); }
static void __attribute__((regparm(3))) exec_0fa2_cpuid_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->send_message(CpuMessage::TYPE_CPUID); }
static void __attribute__((regparm(3))) exec_0fa3_bt_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%eax;and  $(8<<1)-1, %%eax; bt %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fa3_bt_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%eax;and  $(8<<2)-1, %%eax; bt %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fa4_shld_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned count;if (1) count = cache->_entry->immediate; else count = cache->_cpu->ecx;tmp_src = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);asm volatile ("xchg %%eax, %%ecx; mov (%%edx), %%edx; data16 shld %%cl, %%edx, (%%eax); pushf; pop %%eax" : "+a"(count), "+d"(tmp_src), "+c"(tmp_dst));cache->_cpu->efl = (cache->_cpu->efl & ~0x8d5) | (count  & 0x8d5); }
static void __attribute__((regparm(3))) exec_0fa4_shld_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned count;if (1) count = cache->_entry->immediate; else count = cache->_cpu->ecx;tmp_src = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);asm volatile ("xchg %%eax, %%ecx; mov (%%edx), %%edx;  shld %%cl, %%edx, (%%eax); pushf; pop %%eax" : "+a"(count), "+d"(tmp_src), "+c"(tmp_dst));cache->_cpu->efl = (cache->_cpu->efl & ~0x8d5) | (count  & 0x8d5); }
static void __attribute__((regparm(3))) exec_0fa5_shld_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned count;if (0) count = cache->_entry->immediate; else count = cache->_cpu->ecx;tmp_src = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);asm volatile ("xchg %%eax, %%ecx; mov (%%edx), %%edx; data16 shld %%cl, %%edx, (%%eax); pushf; pop %%eax" : "+a"(count), "+d"(tmp_src), "+c"(tmp_dst));cache->_cpu->efl = (cache->_cpu->efl & ~0x8d5) | (count  & 0x8d5); }
static void __attribute__((regparm(3))) exec_0fa5_shld_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned count;if (0) count = cache->_entry->immediate; else count = cache->_cpu->ecx;tmp_src = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);asm volatile ("xchg %%eax, %%ecx; mov (%%edx), %%edx;  shld %%cl, %%edx, (%%eax); pushf; pop %%eax" : "+a"(count), "+d"(tmp_src), "+c"(tmp_dst));cache->_cpu->efl = (cache->_cpu->efl & ~0x8d5) | (count  & 0x8d5); }
static void __attribute__((regparm(3))) exec_0fa8_push__gs_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_PUSH<1>(&cache->_cpu->gs.sel); }
static void __attribute__((regparm(3))) exec_0fa8_push__gs_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_PUSH<2>(&cache->_cpu->gs.sel); }
static void __attribute__((regparm(3))) exec_0fa9_pop__gs_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned sel;cache->helper_POP<1>(&sel) || cache->set_segment(&cache->_cpu->gs, sel);; }
static void __attribute__((regparm(3))) exec_0fa9_pop__gs_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned sel;cache->helper_POP<2>(&sel) || cache->set_segment(&cache->_cpu->gs, sel);; }
static void __attribute__((regparm(3))) exec_0fab_bts_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%eax;and  $(8<<1)-1, %%eax;lock bts %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fab_bts_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%eax;and  $(8<<2)-1, %%eax;lock bts %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fac_shrd_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned count;if (1) count = cache->_entry->immediate; else count = cache->_cpu->ecx;tmp_src = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);asm volatile ("xchg %%eax, %%ecx; mov (%%edx), %%edx; data16 shrd %%cl, %%edx, (%%eax); pushf; pop %%eax" : "+a"(count), "+d"(tmp_src), "+c"(tmp_dst));cache->_cpu->efl = (cache->_cpu->efl & ~0x8d5) | (count  & 0x8d5); }
static void __attribute__((regparm(3))) exec_0fac_shrd_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned count;if (1) count = cache->_entry->immediate; else count = cache->_cpu->ecx;tmp_src = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);asm volatile ("xchg %%eax, %%ecx; mov (%%edx), %%edx;  shrd %%cl, %%edx, (%%eax); pushf; pop %%eax" : "+a"(count), "+d"(tmp_src), "+c"(tmp_dst));cache->_cpu->efl = (cache->_cpu->efl & ~0x8d5) | (count  & 0x8d5); }
static void __attribute__((regparm(3))) exec_0fad_shrd_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned count;if (0) count = cache->_entry->immediate; else count = cache->_cpu->ecx;tmp_src = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);asm volatile ("xchg %%eax, %%ecx; mov (%%edx), %%edx; data16 shrd %%cl, %%edx, (%%eax); pushf; pop %%eax" : "+a"(count), "+d"(tmp_src), "+c"(tmp_dst));cache->_cpu->efl = (cache->_cpu->efl & ~0x8d5) | (count  & 0x8d5); }
static void __attribute__((regparm(3))) exec_0fad_shrd_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned count;if (0) count = cache->_entry->immediate; else count = cache->_cpu->ecx;tmp_src = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);asm volatile ("xchg %%eax, %%ecx; mov (%%edx), %%edx;  shrd %%cl, %%edx, (%%eax); pushf; pop %%eax" : "+a"(count), "+d"(tmp_src), "+c"(tmp_dst));cache->_cpu->efl = (cache->_cpu->efl & ~0x8d5) | (count  & 0x8d5); }
static void __attribute__((regparm(3))) exec_0fae00_fxsave_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_FXSAVE(); }
static void __attribute__((regparm(3))) exec_0faf_imul_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned param, result;tmp_dst = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);if (0) param = cache->_entry->immediate; else if (0) param = cache->_cpu->eax; else move<1>(&param, tmp_dst);;asm volatile ("imulw (%%ecx); pushf; pop %%ecx" : "+a"(param), "=d"(result), "+c"(tmp_src));cache->_cpu->efl = (cache->_cpu->efl & ~0x8d5) | (reinterpret_cast<unsigned>(tmp_src)  & 0x8d5);if (0) move<1 ? 1 : 1>(&cache->_cpu->eax, &param);if (0 && 1) move<1>(&cache->_cpu->edx, &result);if (!0) move<1>(tmp_dst, &param); }
static void __attribute__((regparm(3))) exec_0faf_imul_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned param, result;tmp_dst = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);if (0) param = cache->_entry->immediate; else if (0) param = cache->_cpu->eax; else move<2>(&param, tmp_dst);;asm volatile ("imull (%%ecx); pushf; pop %%ecx" : "+a"(param), "=d"(result), "+c"(tmp_src));cache->_cpu->efl = (cache->_cpu->efl & ~0x8d5) | (reinterpret_cast<unsigned>(tmp_src)  & 0x8d5);if (0) move<2 ? 2 : 1>(&cache->_cpu->eax, &param);if (0 && 2) move<2>(&cache->_cpu->edx, &result);if (!0) move<2>(tmp_dst, &param); }
static void __attribute__((regparm(3))) exec_0fb0_cmpxchg_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 char res; asm volatile("mov (%2), %2; lock cmpxchg %%dl, (%3); setz %1" : "+a"(cache->_cpu->eax), "=d"(res) : "d"(tmp_src), "c"(tmp_dst));if (res) cache->_cpu->efl |= EFL_ZF; else cache->_cpu->efl &= EFL_ZF; }
static void __attribute__((regparm(3))) exec_0fb1_cmpxchg_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 char res; asm volatile("mov (%2), %2; lock cmpxchg %%dx, (%3); setz %1" : "+a"(cache->_cpu->eax), "=d"(res) : "d"(tmp_src), "c"(tmp_dst));if (res) cache->_cpu->efl |= EFL_ZF; else cache->_cpu->efl &= EFL_ZF; }
static void __attribute__((regparm(3))) exec_0fb1_cmpxchg_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 char res; asm volatile("mov (%2), %2; lock cmpxchg %%edx, (%3); setz %1" : "+a"(cache->_cpu->eax), "=d"(res) : "d"(tmp_src), "c"(tmp_dst));if (res) cache->_cpu->efl |= EFL_ZF; else cache->_cpu->efl &= EFL_ZF; }
static void __attribute__((regparm(3))) exec_0fb2_lss_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_loadsegment<1>(&cache->_cpu->ss); }
static void __attribute__((regparm(3))) exec_0fb2_lss_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_loadsegment<2>(&cache->_cpu->ss); }
static void __attribute__((regparm(3))) exec_0fb3_btr_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%eax;and  $(8<<1)-1, %%eax;lock btr %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fb3_btr_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%eax;and  $(8<<2)-1, %%eax;lock btr %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fb4_lfs_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_loadsegment<1>(&cache->_cpu->fs); }
static void __attribute__((regparm(3))) exec_0fb4_lfs_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_loadsegment<2>(&cache->_cpu->fs); }
static void __attribute__((regparm(3))) exec_0fb5_lgs_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_loadsegment<1>(&cache->_cpu->gs); }
static void __attribute__((regparm(3))) exec_0fb5_lgs_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_loadsegment<2>(&cache->_cpu->gs); }
static void __attribute__((regparm(3))) exec_0fb6_movzxb_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned dummy;tmp_dst = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);asm volatile("movl (%2), %0; data16 movzxb (%1), %0; mov %0, (%2)" : "=a"(dummy), "+d"(tmp_src), "+c"(tmp_dst)); }
static void __attribute__((regparm(3))) exec_0fb6_movzxb_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned dummy;tmp_dst = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);asm volatile("movl (%2), %0;  movzxb (%1), %0; mov %0, (%2)" : "=a"(dummy), "+d"(tmp_src), "+c"(tmp_dst)); }
static void __attribute__((regparm(3))) exec_0fb7_movzxw_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned dummy;tmp_dst = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);asm volatile("movl (%2), %0; data16 movzxw (%1), %0; mov %0, (%2)" : "=a"(dummy), "+d"(tmp_src), "+c"(tmp_dst)); }
static void __attribute__((regparm(3))) exec_0fb7_movzxw_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned dummy;tmp_dst = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);asm volatile("movl (%2), %0;  movzxw (%1), %0; mov %0, (%2)" : "=a"(dummy), "+d"(tmp_src), "+c"(tmp_dst)); }
static void __attribute__((regparm(3))) exec_0fbae2_bt_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%eax;and  $(8<<1)-1, %%eax; bt %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fbae2_bt_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%eax;and  $(8<<2)-1, %%eax; bt %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fbaea_bts_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%eax;and  $(8<<1)-1, %%eax;lock bts %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fbaea_bts_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%eax;and  $(8<<2)-1, %%eax;lock bts %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fbaf2_btr_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%eax;and  $(8<<1)-1, %%eax;lock btr %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fbaf2_btr_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%eax;and  $(8<<2)-1, %%eax;lock btr %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fbafa_btc_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%eax;and  $(8<<1)-1, %%eax;lock btc %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fbafa_btc_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%eax;and  $(8<<2)-1, %%eax;lock btc %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fbb_btc_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%eax;and  $(8<<1)-1, %%eax;lock btc %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fbb_btc_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%eax;and  $(8<<2)-1, %%eax;lock btc %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fbc_bsf_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("bsfw (%%edx), %%ax;mov %%ax, (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fbc_bsf_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("bsfl (%%edx), %%eax;mov %%eax, (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fbd_bsr_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("bsrw (%%edx), %%ax;mov %%ax, (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fbd_bsr_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("bsrl (%%edx), %%eax;mov %%eax, (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fbe_movsxb_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned dummy;tmp_dst = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);asm volatile("movl (%2), %0; data16 movsxb (%1), %0; mov %0, (%2)" : "=a"(dummy), "+d"(tmp_src), "+c"(tmp_dst)); }
static void __attribute__((regparm(3))) exec_0fbe_movsxb_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned dummy;tmp_dst = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);asm volatile("movl (%2), %0;  movsxb (%1), %0; mov %0, (%2)" : "=a"(dummy), "+d"(tmp_src), "+c"(tmp_dst)); }
static void __attribute__((regparm(3))) exec_0fbf_movsxw_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned dummy;tmp_dst = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);asm volatile("movl (%2), %0; data16 movsxw (%1), %0; mov %0, (%2)" : "=a"(dummy), "+d"(tmp_src), "+c"(tmp_dst)); }
static void __attribute__((regparm(3))) exec_0fbf_movsxw_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned dummy;tmp_dst = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);asm volatile("movl (%2), %0;  movsxw (%1), %0; mov %0, (%2)" : "=a"(dummy), "+d"(tmp_src), "+c"(tmp_dst)); }
static void __attribute__((regparm(3))) exec_0fc0_xadd_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%al;lock xadd %%al, (%%ecx);mov %%al, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fc1_xadd_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%ax;lock xadd %%ax, (%%ecx);mov %%ax, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fc1_xadd_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%eax;lock xadd %%eax, (%%ecx);mov %%eax, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_0fc9_bswap_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%ecx), %%eax;bswap %%eax;mov %%eax, (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_10_adc_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al;lock adcb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_11_adc_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax;lock adcw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_11_adc_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax;lock adcl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_12_adc_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; adcb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_13_adc_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; adcw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_13_adc_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; adcl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_14_adc_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; adcb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_15_adc_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; adcw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_15_adc_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; adcl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_16_push__ss_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_PUSH<1>(&cache->_cpu->ss.sel); }
static void __attribute__((regparm(3))) exec_16_push__ss_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_PUSH<2>(&cache->_cpu->ss.sel); }
static void __attribute__((regparm(3))) exec_17_pop__ss_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned sel;cache->helper_POP<1>(&sel) || cache->set_segment(&cache->_cpu->ss, sel);cache->_cpu->intr_state |= 2; }
static void __attribute__((regparm(3))) exec_17_pop__ss_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned sel;cache->helper_POP<2>(&sel) || cache->set_segment(&cache->_cpu->ss, sel);cache->_cpu->intr_state |= 2; }
static void __attribute__((regparm(3))) exec_18_sbb_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al;lock sbbb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_19_sbb_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax;lock sbbw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_19_sbb_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax;lock sbbl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_1a_sbb_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; sbbb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_1b_sbb_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; sbbw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_1b_sbb_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; sbbl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_1c_sbb_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; sbbb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_1d_sbb_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; sbbw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_1d_sbb_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; sbbl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_1e_push__ds_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_PUSH<1>(&cache->_cpu->ds.sel); }
static void __attribute__((regparm(3))) exec_1e_push__ds_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_PUSH<2>(&cache->_cpu->ds.sel); }
static void __attribute__((regparm(3))) exec_1f_pop__ds_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned sel;cache->helper_POP<1>(&sel) || cache->set_segment(&cache->_cpu->ds, sel);; }
static void __attribute__((regparm(3))) exec_1f_pop__ds_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned sel;cache->helper_POP<2>(&sel) || cache->set_segment(&cache->_cpu->ds, sel);; }
static void __attribute__((regparm(3))) exec_20_and_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al;lock andb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_21_and_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax;lock andw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_21_and_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax;lock andl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_22_and_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; andb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_23_and_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; andw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_23_and_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; andl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_24_and_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; andb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_25_and_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; andw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_25_and_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; andl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_27_daa_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%ecx), %%eax;daa;mov %%eax, (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_28_sub_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al;lock subb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_29_sub_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax;lock subw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_29_sub_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax;lock subl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_2a_sub_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; subb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_2b_sub_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; subw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_2b_sub_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; subl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_2c_sub_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; subb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_2d_sub_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; subw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_2d_sub_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; subl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_2f_das_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%ecx), %%eax;das;mov %%eax, (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_30_xor_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al;lock xorb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_31_xor_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax;lock xorw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_31_xor_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax;lock xorl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_32_xor_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; xorb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_33_xor_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; xorw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_33_xor_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; xorl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_34_xor_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; xorb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_35_xor_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; xorw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_35_xor_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; xorl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_37_aaa_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%ecx), %%eax;aaa;mov %%eax, (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_38_cmp_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; cmpb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_39_cmp_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; cmpw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_39_cmp_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; cmpl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_3a_cmp_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; cmpb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_3b_cmp_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; cmpw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_3b_cmp_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; cmpl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_3c_cmp_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; cmpb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_3d_cmp_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; cmpw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_3d_cmp_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; cmpl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_3f_aas_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%ecx), %%eax;aas;mov %%eax, (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_41_inc_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile(" incw (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_41_inc_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile(" incl (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_49_dec_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile(" decw (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_49_dec_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile(" decl (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_51_push_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_PUSH<1>(tmp_src); }
static void __attribute__((regparm(3))) exec_51_push_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_PUSH<2>(tmp_src); }
static void __attribute__((regparm(3))) exec_59_pop_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_POP<1>(tmp_dst); }
static void __attribute__((regparm(3))) exec_59_pop_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_POP<2>(tmp_dst); }
static void __attribute__((regparm(3))) exec_60_pusha_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 for (unsigned i=0; i<8; i++) {;if (i == 4) { if (cache->helper_PUSH<1>(&cache->_oesp)) return; }else if (cache->helper_PUSH<1>(cache->get_reg32(i))) return; }; }
static void __attribute__((regparm(3))) exec_60_pusha_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 for (unsigned i=0; i<8; i++) {;if (i == 4) { if (cache->helper_PUSH<2>(&cache->_oesp)) return; }else if (cache->helper_PUSH<2>(cache->get_reg32(i))) return; }; }
static void __attribute__((regparm(3))) exec_61_popa_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned values[8];for (unsigned i=8; i; i--)  if (cache->helper_POP<1>(values+i-1)) return;;for (unsigned i=0; i < 8; i++) if (i!=4) move<1>(cache->get_reg32(i), values+i); }
static void __attribute__((regparm(3))) exec_61_popa_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned values[8];for (unsigned i=8; i; i--)  if (cache->helper_POP<2>(values+i-1)) return;;for (unsigned i=0; i < 8; i++) if (i!=4) move<2>(cache->get_reg32(i), values+i); }
static void __attribute__((regparm(3))) exec_68_push_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_PUSH<1>(tmp_src); }
static void __attribute__((regparm(3))) exec_68_push_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_PUSH<2>(tmp_src); }
static void __attribute__((regparm(3))) exec_69_imul_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned param, result;tmp_dst = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);if (1) param = cache->_entry->immediate; else if (0) param = cache->_cpu->eax; else move<1>(&param, tmp_dst);;asm volatile ("imulw (%%ecx); pushf; pop %%ecx" : "+a"(param), "=d"(result), "+c"(tmp_src));cache->_cpu->efl = (cache->_cpu->efl & ~0x8d5) | (reinterpret_cast<unsigned>(tmp_src)  & 0x8d5);if (0) move<1 ? 1 : 1>(&cache->_cpu->eax, &param);if (0 && 1) move<1>(&cache->_cpu->edx, &result);if (!0) move<1>(tmp_dst, &param); }
static void __attribute__((regparm(3))) exec_69_imul_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned param, result;tmp_dst = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);if (1) param = cache->_entry->immediate; else if (0) param = cache->_cpu->eax; else move<2>(&param, tmp_dst);;asm volatile ("imull (%%ecx); pushf; pop %%ecx" : "+a"(param), "=d"(result), "+c"(tmp_src));cache->_cpu->efl = (cache->_cpu->efl & ~0x8d5) | (reinterpret_cast<unsigned>(tmp_src)  & 0x8d5);if (0) move<2 ? 2 : 1>(&cache->_cpu->eax, &param);if (0 && 2) move<2>(&cache->_cpu->edx, &result);if (!0) move<2>(tmp_dst, &param); }
static void __attribute__((regparm(3))) exec_6a_push_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_PUSH<1>(tmp_src); }
static void __attribute__((regparm(3))) exec_6a_push_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_PUSH<2>(tmp_src); }
static void __attribute__((regparm(3))) exec_6b_imul_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned param, result;tmp_dst = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);if (1) param = cache->_entry->immediate; else if (0) param = cache->_cpu->eax; else move<1>(&param, tmp_dst);;asm volatile ("imulw (%%ecx); pushf; pop %%ecx" : "+a"(param), "=d"(result), "+c"(tmp_src));cache->_cpu->efl = (cache->_cpu->efl & ~0x8d5) | (reinterpret_cast<unsigned>(tmp_src)  & 0x8d5);if (0) move<1 ? 1 : 1>(&cache->_cpu->eax, &param);if (0 && 1) move<1>(&cache->_cpu->edx, &result);if (!0) move<1>(tmp_dst, &param); }
static void __attribute__((regparm(3))) exec_6b_imul_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned param, result;tmp_dst = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);if (1) param = cache->_entry->immediate; else if (0) param = cache->_cpu->eax; else move<2>(&param, tmp_dst);;asm volatile ("imull (%%ecx); pushf; pop %%ecx" : "+a"(param), "=d"(result), "+c"(tmp_src));cache->_cpu->efl = (cache->_cpu->efl & ~0x8d5) | (reinterpret_cast<unsigned>(tmp_src)  & 0x8d5);if (0) move<2 ? 2 : 1>(&cache->_cpu->eax, &param);if (0 && 2) move<2>(&cache->_cpu->edx, &result);if (!0) move<2>(tmp_dst, &param); }
static void __attribute__((regparm(3))) exec_6c_ins_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->string_helper<SH_SAVE_EDI | SH_DOOP_IN, 0>(); }
static void __attribute__((regparm(3))) exec_6d_ins_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->string_helper<SH_SAVE_EDI | SH_DOOP_IN, 1>(); }
static void __attribute__((regparm(3))) exec_6d_ins_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->string_helper<SH_SAVE_EDI | SH_DOOP_IN, 2>(); }
static void __attribute__((regparm(3))) exec_6e_outs_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->string_helper<SH_LOAD_ESI | SH_DOOP_OUT, 0>(); }
static void __attribute__((regparm(3))) exec_6f_outs_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->string_helper<SH_LOAD_ESI | SH_DOOP_OUT, 1>(); }
static void __attribute__((regparm(3))) exec_6f_outs_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->string_helper<SH_LOAD_ESI | SH_DOOP_OUT, 2>(); }
static void __attribute__((regparm(3))) exec_70_jo_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jno 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_70_jo_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jno 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_71_jno_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jo 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_71_jno_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jo 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_72_jb_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jae 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_72_jb_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jae 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_73_jae_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jb 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_73_jae_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jb 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_74_je_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jne 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_74_je_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jne 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_75_jne_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("je 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_75_jne_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("je 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_76_jbe_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("ja 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_76_jbe_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("ja 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_77_ja_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jbe 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_77_ja_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jbe 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_78_js_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jns 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_78_js_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jns 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_79_jns_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("js 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_79_jns_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("js 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_7a_jp_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jnp 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_7a_jp_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jnp 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_7b_jnp_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jp 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_7b_jnp_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jp 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_7c_jl_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jge 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_7c_jl_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jge 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_7d_jge_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jl 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_7d_jge_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jl 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_7e_jle_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jg 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_7e_jle_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jg 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_7f_jg_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<1>;asm volatile ("jle 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_7f_jg_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<2>;asm volatile ("jle 1f;call %c0; 1:" : : "i"(foo)); }
static void __attribute__((regparm(3))) exec_80c2_add_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al;lock addb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_80ca_or_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al;lock orb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_80d2_adc_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al;lock adcb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_80da_sbb_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al;lock sbbb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_80e2_and_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al;lock andb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_80ea_sub_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al;lock subb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_80f2_xor_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al;lock xorb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_80fa_cmp_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; cmpb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_81c2_add_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax;lock addw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_81c2_add_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax;lock addl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_81ca_or_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax;lock orw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_81ca_or_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax;lock orl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_81d2_adc_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax;lock adcw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_81d2_adc_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax;lock adcl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_81da_sbb_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax;lock sbbw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_81da_sbb_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax;lock sbbl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_81e2_and_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax;lock andw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_81e2_and_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax;lock andl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_81ea_sub_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax;lock subw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_81ea_sub_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax;lock subl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_81f2_xor_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax;lock xorw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_81f2_xor_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax;lock xorl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_81fa_cmp_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; cmpw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_81fa_cmp_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; cmpl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_83c2_add_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax;lock addw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_83c2_add_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax;lock addl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_83ca_or_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax;lock orw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_83ca_or_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax;lock orl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_83d2_adc_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax;lock adcw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_83d2_adc_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax;lock adcl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_83da_sbb_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax;lock sbbw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_83da_sbb_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax;lock sbbl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_83e2_and_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax;lock andw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_83e2_and_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax;lock andl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_83ea_sub_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax;lock subw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_83ea_sub_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax;lock subl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_83f2_xor_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax;lock xorw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_83f2_xor_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax;lock xorl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_83fa_cmp_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; cmpw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_83fa_cmp_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; cmpl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_84_test_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; testb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_85_test_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; testw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_85_test_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; testl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_86_xchg_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%al;lock xchgb %%al, (%%ecx);mov %%al, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_87_xchg_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%ax;lock xchgw %%ax, (%%ecx);mov %%ax, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_87_xchg_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%eax;lock xchgl %%eax, (%%ecx);mov %%eax, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_88_mov_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; movb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_89_mov_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; movw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_89_mov_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; movl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_8a_mov_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; movb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_8b_mov_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; movw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_8b_mov_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; movl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_8c_mov__es__edx_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 CpuState::Descriptor *dsc = &cache->_cpu->es + ((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);move<1>(tmp_dst, &(dsc->sel)); }
static void __attribute__((regparm(3))) exec_8c_mov__es__edx_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 CpuState::Descriptor *dsc = &cache->_cpu->es + ((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);move<2>(tmp_dst, &(dsc->sel)); }
static void __attribute__((regparm(3))) exec_8d_lea_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LEA<1>(); }
static void __attribute__((regparm(3))) exec_8d_lea_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LEA<2>(); }
static void __attribute__((regparm(3))) exec_8e_mov__edx__es_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 if (((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7) == 2) cache->_cpu->intr_state |= 2;cache->set_segment(&cache->_cpu->es + ((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7), *reinterpret_cast<unsigned short *>(tmp_src)); }
static void __attribute__((regparm(3))) exec_8e_mov__edx__es_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 if (((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7) == 2) cache->_cpu->intr_state |= 2;cache->set_segment(&cache->_cpu->es + ((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7), *reinterpret_cast<unsigned short *>(tmp_src)); }
static void __attribute__((regparm(3))) exec_8f00_pop_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_POP<1>(tmp_dst); }
static void __attribute__((regparm(3))) exec_8f00_pop_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_POP<2>(tmp_dst); }
static void __attribute__((regparm(3))) exec_92_xchg_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%ax; xchgw %%ax, (%%ecx);mov %%ax, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_92_xchg_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%eax; xchgl %%eax, (%%ecx);mov %%eax, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_98_cwtl_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%ecx), %%ax;data16 cwde;mov %%ax, (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_98_cwtl_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%ecx), %%eax; cwde;mov %%eax, (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_99_cltd_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%eax;data16 cltd;mov %%edx, (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_99_cltd_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("mov (%%edx), %%eax; cltd;mov %%edx, (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_9a_lcall_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LCALL<1>(tmp_src); }
static void __attribute__((regparm(3))) exec_9a_lcall_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LCALL<2>(tmp_src); }
static void __attribute__((regparm(3))) exec_9b_fwait_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_FWAIT(); }
static void __attribute__((regparm(3))) exec_9c_pushf_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_PUSHF<1>(); }
static void __attribute__((regparm(3))) exec_9c_pushf_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_PUSHF<2>(); }
static void __attribute__((regparm(3))) exec_9d_popf_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_POPF<1>(); }
static void __attribute__((regparm(3))) exec_9d_popf_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_POPF<2>(); }
static void __attribute__((regparm(3))) exec_9e_sahf_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->_cpu->efl = (cache->_cpu->efl & ~0xd5) | (cache->_cpu->ah  & 0xd5); }
static void __attribute__((regparm(3))) exec_9f_lahf_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->_cpu->ah = (cache->_cpu->efl & 0xd5) | 2; }
static void __attribute__((regparm(3))) exec_a0_mov_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; movb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_a1_mov_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; movw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_a1_mov_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; movl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_a2_mov_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; movb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_a3_mov_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; movw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_a3_mov_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; movl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_a4_movs_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->string_helper<SH_LOAD_ESI | SH_SAVE_EDI, 0>(); }
static void __attribute__((regparm(3))) exec_a5_movs_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->string_helper<SH_LOAD_ESI | SH_SAVE_EDI, 1>(); }
static void __attribute__((regparm(3))) exec_a5_movs_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->string_helper<SH_LOAD_ESI | SH_SAVE_EDI, 2>(); }
static void __attribute__((regparm(3))) exec_a6_cmps_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->string_helper<SH_LOAD_ESI | SH_LOAD_EDI | SH_DOOP_CMP, 0>(); }
static void __attribute__((regparm(3))) exec_a7_cmps_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->string_helper<SH_LOAD_ESI | SH_LOAD_EDI | SH_DOOP_CMP, 1>(); }
static void __attribute__((regparm(3))) exec_a7_cmps_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->string_helper<SH_LOAD_ESI | SH_LOAD_EDI | SH_DOOP_CMP, 2>(); }
static void __attribute__((regparm(3))) exec_a8_test_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; testb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_a9_test_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; testw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_a9_test_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; testl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_aa_stos_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->string_helper<SH_SAVE_EDI, 0>(); }
static void __attribute__((regparm(3))) exec_ab_stos_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->string_helper<SH_SAVE_EDI, 1>(); }
static void __attribute__((regparm(3))) exec_ab_stos_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->string_helper<SH_SAVE_EDI, 2>(); }
static void __attribute__((regparm(3))) exec_ac_lods_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->string_helper<SH_LOAD_ESI | SH_SAVE_EAX, 0>(); }
static void __attribute__((regparm(3))) exec_ad_lods_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->string_helper<SH_LOAD_ESI | SH_SAVE_EAX, 1>(); }
static void __attribute__((regparm(3))) exec_ad_lods_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->string_helper<SH_LOAD_ESI | SH_SAVE_EAX, 2>(); }
static void __attribute__((regparm(3))) exec_ae_scas_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->string_helper<SH_LOAD_EDI | SH_DOOP_CMP, 0>(); }
static void __attribute__((regparm(3))) exec_af_scas_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->string_helper<SH_LOAD_EDI | SH_DOOP_CMP, 1>(); }
static void __attribute__((regparm(3))) exec_af_scas_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->string_helper<SH_LOAD_EDI | SH_DOOP_CMP, 2>(); }
static void __attribute__((regparm(3))) exec_b2_mov_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; movb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_ba_mov_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; movw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_ba_mov_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; movl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c0c2_rol_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rolb %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c0ca_ror_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rorb %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c0d2_rcl_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rclb %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c0da_rcr_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rcrb %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c0e2_shl_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;shlb %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c0ea_shr_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;shrb %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c0fa_sar_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;sarb %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c1c2_rol_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rolw %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c1c2_rol_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;roll %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c1ca_ror_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rorw %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c1ca_ror_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rorl %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c1d2_rcl_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rclw %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c1d2_rcl_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rcll %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c1da_rcr_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rcrw %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c1da_rcr_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rcrl %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c1e2_shl_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;shlw %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c1e2_shl_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;shll %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c1ea_shr_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;shrw %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c1ea_shr_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;shrl %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c1fa_sar_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;sarw %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c1fa_sar_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;sarl %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c2_ret_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_RET<1>(tmp_src); }
static void __attribute__((regparm(3))) exec_c2_ret_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_RET<2>(tmp_src); }
static void __attribute__((regparm(3))) exec_c3_ret_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_RET<1>(tmp_src); }
static void __attribute__((regparm(3))) exec_c3_ret_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_RET<2>(tmp_src); }
static void __attribute__((regparm(3))) exec_c4_les_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_loadsegment<1>(&cache->_cpu->es); }
static void __attribute__((regparm(3))) exec_c4_les_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_loadsegment<2>(&cache->_cpu->es); }
static void __attribute__((regparm(3))) exec_c5_lds_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_loadsegment<1>(&cache->_cpu->ds); }
static void __attribute__((regparm(3))) exec_c5_lds_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_loadsegment<2>(&cache->_cpu->ds); }
static void __attribute__((regparm(3))) exec_c602_mov_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; movb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c702_mov_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; movw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c702_mov_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; movl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_c8_enter_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_ENTER<1>(reinterpret_cast<unsigned *>(tmp_src)); }
static void __attribute__((regparm(3))) exec_c8_enter_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_ENTER<2>(reinterpret_cast<unsigned *>(tmp_src)); }
static void __attribute__((regparm(3))) exec_c9_leave_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LEAVE<1>(); }
static void __attribute__((regparm(3))) exec_c9_leave_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LEAVE<2>(); }
static void __attribute__((regparm(3))) exec_ca_lret_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LRET<1>(tmp_src); }
static void __attribute__((regparm(3))) exec_ca_lret_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LRET<2>(tmp_src); }
static void __attribute__((regparm(3))) exec_cb_lret_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LRET<1>(tmp_src); }
static void __attribute__((regparm(3))) exec_cb_lret_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LRET<2>(tmp_src); }
static void __attribute__((regparm(3))) exec_cc_int3_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_INT3(); }
static void __attribute__((regparm(3))) exec_cd_int_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_INT(*reinterpret_cast<unsigned char *>(tmp_src)); }
static void __attribute__((regparm(3))) exec_ce_into_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_INTO(); }
static void __attribute__((regparm(3))) exec_cf_iret_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_IRET<1>(); }
static void __attribute__((regparm(3))) exec_cf_iret_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_IRET<2>(); }
static void __attribute__((regparm(3))) exec_d000_rol_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rolb %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d008_ror_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rorb %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d010_rcl_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rclb %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d018_rcr_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rcrb %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d020_shl_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;shlb %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d028_shr_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;shrb %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d038_sar_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;sarb %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d100_rol_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rolw %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d100_rol_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;roll %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d108_ror_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rorw %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d108_ror_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rorl %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d110_rcl_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rclw %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d110_rcl_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rcll %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d118_rcr_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rcrw %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d118_rcr_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rcrl %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d120_shl_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;shlw %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d120_shl_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;shll %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d128_shr_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;shrw %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d128_shr_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;shrl %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d138_sar_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;sarw %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d138_sar_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;sarl %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d200_rol_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rolb %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d208_ror_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rorb %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d210_rcl_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rclb %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d218_rcr_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rcrb %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d220_shl_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;shlb %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d228_shr_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;shrb %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d238_sar_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;sarb %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d300_rol_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rolw %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d300_rol_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;roll %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d308_ror_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rorw %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d308_ror_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rorl %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d310_rcl_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rclw %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d310_rcl_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rcll %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d318_rcr_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rcrw %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d318_rcr_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;rcrl %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d320_shl_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;shlw %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d320_shl_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;shll %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d328_shr_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;shrw %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d328_shr_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;shrl %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d338_sar_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;sarw %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d338_sar_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("xchg %%edx, %%ecx;movb (%%ecx),%%cl;sarl %%cl, (%%edx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_d4_aam_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_AAM(*reinterpret_cast<unsigned char *>(tmp_src)); }
static void __attribute__((regparm(3))) exec_d5_aad_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_AAD(*reinterpret_cast<unsigned char *>(tmp_src)); }
static void __attribute__((regparm(3))) exec_d7_xlat_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_XLAT(); }
static void __attribute__((regparm(3))) exec_d938_fnstcw_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 if (cache->_cpu->cr0 & 0xc) EXCEPTION(cache, 0x7, 0);asm volatile("fxrstor (%%eax);fnstcw (%%ecx); fxsave (%%eax);" : "+d"(tmp_src), "+c"(tmp_dst) : "a"(cache->_fpustate)); }
static void __attribute__((regparm(3))) exec_da10_ficom_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 if (cache->_cpu->cr0 & 0xc) EXCEPTION(cache, 0x7, 0);asm volatile("fxrstor (%%eax);ficom (%%ecx); fxsave (%%eax);" : "+d"(tmp_src), "+c"(tmp_dst) : "a"(cache->_fpustate)); }
static void __attribute__((regparm(3))) exec_da18_ficomp_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 if (cache->_cpu->cr0 & 0xc) EXCEPTION(cache, 0x7, 0);asm volatile("fxrstor (%%eax);ficomp (%%ecx); fxsave (%%eax);" : "+d"(tmp_src), "+c"(tmp_dst) : "a"(cache->_fpustate)); }
static void __attribute__((regparm(3))) exec_dbe3_fninit_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 if (cache->_cpu->cr0 & 0xc) EXCEPTION(cache, 0x7, 0);asm volatile("fninit; fxsave (%%eax);" : "+d"(tmp_src), "+c"(tmp_dst) : "a"(cache->_fpustate)); }
static void __attribute__((regparm(3))) exec_dbe4__byte_0xdb__0xe4__0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 /* fnsetpm, on 287 only, noop afterwards */; }
static void __attribute__((regparm(3))) exec_dd20_frstor_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_FRSTOR(); }
static void __attribute__((regparm(3))) exec_dd38_fnstsw_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 if (cache->_cpu->cr0 & 0xc) EXCEPTION(cache, 0x7, 0);asm volatile("fxrstor (%%eax);fnstsw (%%ecx); fxsave (%%eax);" : "+d"(tmp_src), "+c"(tmp_dst) : "a"(cache->_fpustate)); }
static void __attribute__((regparm(3))) exec_de10_ficom_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 if (cache->_cpu->cr0 & 0xc) EXCEPTION(cache, 0x7, 0);asm volatile("fxrstor (%%eax);ficom (%%ecx); fxsave (%%eax);" : "+d"(tmp_src), "+c"(tmp_dst) : "a"(cache->_fpustate)); }
static void __attribute__((regparm(3))) exec_de18_ficomp_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 if (cache->_cpu->cr0 & 0xc) EXCEPTION(cache, 0x7, 0);asm volatile("fxrstor (%%eax);ficomp (%%ecx); fxsave (%%eax);" : "+d"(tmp_src), "+c"(tmp_dst) : "a"(cache->_fpustate)); }
static void __attribute__((regparm(3))) exec_dfe0_fnstsw__ax_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 if (cache->_cpu->cr0 & 0xc) EXCEPTION(cache, 0x7, 0);asm volatile("fxrstor (%%eax);fnstsw (%%ecx); fxsave (%%eax);" : "+d"(tmp_src), "+c"(tmp_dst) : "a"(cache->_fpustate)); }
static void __attribute__((regparm(3))) exec_e0_loopne_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LOOPNE<1>(tmp_src); }
static void __attribute__((regparm(3))) exec_e0_loopne_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LOOPNE<2>(tmp_src); }
static void __attribute__((regparm(3))) exec_e1_loope_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LOOPE<1>(tmp_src); }
static void __attribute__((regparm(3))) exec_e1_loope_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LOOPE<2>(tmp_src); }
static void __attribute__((regparm(3))) exec_e2_loop_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LOOP<1>(tmp_src); }
static void __attribute__((regparm(3))) exec_e2_loop_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LOOP<2>(tmp_src); }
static void __attribute__((regparm(3))) exec_e3_jecxz_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_JECXZ<1>(tmp_src); }
static void __attribute__((regparm(3))) exec_e3_jecxz_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_JECXZ<2>(tmp_src); }
static void __attribute__((regparm(3))) exec_e4_in_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_IN<0>(*reinterpret_cast<unsigned char *>(tmp_src), &cache->_cpu->eax); }
static void __attribute__((regparm(3))) exec_e5_in_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_IN<1>(*reinterpret_cast<unsigned char *>(tmp_src), &cache->_cpu->eax); }
static void __attribute__((regparm(3))) exec_e5_in_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_IN<2>(*reinterpret_cast<unsigned char *>(tmp_src), &cache->_cpu->eax); }
static void __attribute__((regparm(3))) exec_e6_out_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_OUT<0>(*reinterpret_cast<unsigned char *>(tmp_src), &cache->_cpu->eax); }
static void __attribute__((regparm(3))) exec_e7_out_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_OUT<1>(*reinterpret_cast<unsigned char *>(tmp_src), &cache->_cpu->eax); }
static void __attribute__((regparm(3))) exec_e7_out_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_OUT<2>(*reinterpret_cast<unsigned char *>(tmp_src), &cache->_cpu->eax); }
static void __attribute__((regparm(3))) exec_e8_call_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_CALL<1>(tmp_src); }
static void __attribute__((regparm(3))) exec_e8_call_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_CALL<2>(tmp_src); }
static void __attribute__((regparm(3))) exec_e9_jmp_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_JMP<1>(tmp_src); }
static void __attribute__((regparm(3))) exec_e9_jmp_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_JMP<2>(tmp_src); }
static void __attribute__((regparm(3))) exec_ea_ljmp_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LJMP<1>(tmp_src); }
static void __attribute__((regparm(3))) exec_ea_ljmp_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LJMP<2>(tmp_src); }
static void __attribute__((regparm(3))) exec_eb_jmp_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_JMP<1>(tmp_src); }
static void __attribute__((regparm(3))) exec_eb_jmp_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_JMP<2>(tmp_src); }
static void __attribute__((regparm(3))) exec_ec_in_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_IN<0>(*reinterpret_cast<unsigned *>(tmp_src), &cache->_cpu->eax); }
static void __attribute__((regparm(3))) exec_ed_in_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_IN<1>(*reinterpret_cast<unsigned *>(tmp_src), &cache->_cpu->eax); }
static void __attribute__((regparm(3))) exec_ed_in_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_IN<2>(*reinterpret_cast<unsigned *>(tmp_src), &cache->_cpu->eax); }
static void __attribute__((regparm(3))) exec_ee_out_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_OUT<0>(*reinterpret_cast<unsigned *>(tmp_src), &cache->_cpu->eax); }
static void __attribute__((regparm(3))) exec_ef_out_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_OUT<1>(*reinterpret_cast<unsigned *>(tmp_src), &cache->_cpu->eax); }
static void __attribute__((regparm(3))) exec_ef_out_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_OUT<2>(*reinterpret_cast<unsigned *>(tmp_src), &cache->_cpu->eax); }
static void __attribute__((regparm(3))) exec_f4_hlt_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 if (cache->cpl0_test()) return;cache->helper_HLT(); }
static void __attribute__((regparm(3))) exec_f5_cmc_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->_cpu->efl ^=  1; }
static void __attribute__((regparm(3))) exec_f610_not_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("lock notb (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_f618_neg_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("lock negb (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_f620_mul_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned edx = cache->_cpu->edx, eax = cache->_cpu->eax;;asm volatile ("1: ;mulb (%2);xorl %2, %2;2: ; .section .data.fixup2; .long 1b, 2b, 2b-1b; .previous;" : "+a"(eax), "+d"(edx), "+c"(tmp_src));if (tmp_src) DE0(cache);cache->_cpu->eax = eax;cache->_cpu->edx = edx; }
static void __attribute__((regparm(3))) exec_f628_imul_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned param, result;tmp_dst = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);if (0) param = cache->_entry->immediate; else if (1) param = cache->_cpu->eax; else move<0>(&param, tmp_dst);;asm volatile ("imulb (%%ecx); pushf; pop %%ecx" : "+a"(param), "=d"(result), "+c"(tmp_src));cache->_cpu->efl = (cache->_cpu->efl & ~0x8d5) | (reinterpret_cast<unsigned>(tmp_src)  & 0x8d5);if (1) move<0 ? 0 : 1>(&cache->_cpu->eax, &param);if (1 && 0) move<0>(&cache->_cpu->edx, &result);if (!1) move<0>(tmp_dst, &param); }
static void __attribute__((regparm(3))) exec_f630_div_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned edx = cache->_cpu->edx, eax = cache->_cpu->eax;;asm volatile ("1: ;divb (%2);xorl %2, %2;2: ; .section .data.fixup2; .long 1b, 2b, 2b-1b; .previous;" : "+a"(eax), "+d"(edx), "+c"(tmp_src));if (tmp_src) DE0(cache);cache->_cpu->eax = eax;cache->_cpu->edx = edx; }
static void __attribute__((regparm(3))) exec_f638_idiv_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned edx = cache->_cpu->edx, eax = cache->_cpu->eax;;asm volatile ("1: ;idivb (%2);xorl %2, %2;2: ; .section .data.fixup2; .long 1b, 2b, 2b-1b; .previous;" : "+a"(eax), "+d"(edx), "+c"(tmp_src));if (tmp_src) DE0(cache);cache->_cpu->eax = eax;cache->_cpu->edx = edx; }
static void __attribute__((regparm(3))) exec_f6c2_test_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movb (%%edx), %%al; testb %%al,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_f710_not_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("lock notw (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_f710_not_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("lock notl (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_f718_neg_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("lock negw (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_f718_neg_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("lock negl (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_f720_mul_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned edx = cache->_cpu->edx, eax = cache->_cpu->eax;;asm volatile ("1: ;mulw (%2);xorl %2, %2;2: ; .section .data.fixup2; .long 1b, 2b, 2b-1b; .previous;" : "+a"(eax), "+d"(edx), "+c"(tmp_src));if (tmp_src) DE0(cache);cache->_cpu->eax = eax;cache->_cpu->edx = edx; }
static void __attribute__((regparm(3))) exec_f720_mul_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned edx = cache->_cpu->edx, eax = cache->_cpu->eax;;asm volatile ("1: ;mull (%2);xorl %2, %2;2: ; .section .data.fixup2; .long 1b, 2b, 2b-1b; .previous;" : "+a"(eax), "+d"(edx), "+c"(tmp_src));if (tmp_src) DE0(cache);cache->_cpu->eax = eax;cache->_cpu->edx = edx; }
static void __attribute__((regparm(3))) exec_f728_imul_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned param, result;tmp_dst = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);if (0) param = cache->_entry->immediate; else if (1) param = cache->_cpu->eax; else move<1>(&param, tmp_dst);;asm volatile ("imulw (%%ecx); pushf; pop %%ecx" : "+a"(param), "=d"(result), "+c"(tmp_src));cache->_cpu->efl = (cache->_cpu->efl & ~0x8d5) | (reinterpret_cast<unsigned>(tmp_src)  & 0x8d5);if (1) move<1 ? 1 : 1>(&cache->_cpu->eax, &param);if (1 && 1) move<1>(&cache->_cpu->edx, &result);if (!1) move<1>(tmp_dst, &param); }
static void __attribute__((regparm(3))) exec_f728_imul_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned param, result;tmp_dst = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7);if (0) param = cache->_entry->immediate; else if (1) param = cache->_cpu->eax; else move<2>(&param, tmp_dst);;asm volatile ("imull (%%ecx); pushf; pop %%ecx" : "+a"(param), "=d"(result), "+c"(tmp_src));cache->_cpu->efl = (cache->_cpu->efl & ~0x8d5) | (reinterpret_cast<unsigned>(tmp_src)  & 0x8d5);if (1) move<2 ? 2 : 1>(&cache->_cpu->eax, &param);if (1 && 2) move<2>(&cache->_cpu->edx, &result);if (!1) move<2>(tmp_dst, &param); }
static void __attribute__((regparm(3))) exec_f730_div_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned edx = cache->_cpu->edx, eax = cache->_cpu->eax;;asm volatile ("1: ;divw (%2);xorl %2, %2;2: ; .section .data.fixup2; .long 1b, 2b, 2b-1b; .previous;" : "+a"(eax), "+d"(edx), "+c"(tmp_src));if (tmp_src) DE0(cache);cache->_cpu->eax = eax;cache->_cpu->edx = edx; }
static void __attribute__((regparm(3))) exec_f730_div_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned edx = cache->_cpu->edx, eax = cache->_cpu->eax;;asm volatile ("1: ;divl (%2);xorl %2, %2;2: ; .section .data.fixup2; .long 1b, 2b, 2b-1b; .previous;" : "+a"(eax), "+d"(edx), "+c"(tmp_src));if (tmp_src) DE0(cache);cache->_cpu->eax = eax;cache->_cpu->edx = edx; }
static void __attribute__((regparm(3))) exec_f738_idiv_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned edx = cache->_cpu->edx, eax = cache->_cpu->eax;;asm volatile ("1: ;idivw (%2);xorl %2, %2;2: ; .section .data.fixup2; .long 1b, 2b, 2b-1b; .previous;" : "+a"(eax), "+d"(edx), "+c"(tmp_src));if (tmp_src) DE0(cache);cache->_cpu->eax = eax;cache->_cpu->edx = edx; }
static void __attribute__((regparm(3))) exec_f738_idiv_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 unsigned edx = cache->_cpu->edx, eax = cache->_cpu->eax;;asm volatile ("1: ;idivl (%2);xorl %2, %2;2: ; .section .data.fixup2; .long 1b, 2b, 2b-1b; .previous;" : "+a"(eax), "+d"(edx), "+c"(tmp_src));if (tmp_src) DE0(cache);cache->_cpu->eax = eax;cache->_cpu->edx = edx; }
static void __attribute__((regparm(3))) exec_f7c2_test_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movw (%%edx), %%ax; testw %%ax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_f7c2_test_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("movl (%%edx), %%eax; testl %%eax,(%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_f8_clc_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->_cpu->efl &= ~1; }
static void __attribute__((regparm(3))) exec_f9_stc_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->_cpu->efl |=  1; }
static void __attribute__((regparm(3))) exec_fa_cli_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_CLI(); }
static void __attribute__((regparm(3))) exec_fb_sti_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_STI(); }
static void __attribute__((regparm(3))) exec_fc_cld_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->_cpu->efl &= ~0x400; }
static void __attribute__((regparm(3))) exec_fd_std_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->_cpu->efl |=  0x400; }
static void __attribute__((regparm(3))) exec_fe00_inc_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("lock incb (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_fe08_dec_0(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("lock decb (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_ff00_inc_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("lock incw (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_ff00_inc_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("lock incl (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_ff08_dec_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("lock decw (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_ff08_dec_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 asm volatile("lock decl (%%ecx)" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax"); }
static void __attribute__((regparm(3))) exec_ff15_call_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_CALL<1>(tmp_src); }
static void __attribute__((regparm(3))) exec_ff15_call_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_CALL<2>(tmp_src); }
static void __attribute__((regparm(3))) exec_ff1d_lcall_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LCALL<1>(tmp_src); }
static void __attribute__((regparm(3))) exec_ff1d_lcall_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LCALL<2>(tmp_src); }
static void __attribute__((regparm(3))) exec_ff25_jmp_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_JMP<1>(tmp_src); }
static void __attribute__((regparm(3))) exec_ff25_jmp_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_JMP<2>(tmp_src); }
static void __attribute__((regparm(3))) exec_ff2d_ljmp_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LJMP<1>(tmp_src); }
static void __attribute__((regparm(3))) exec_ff2d_ljmp_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_LJMP<2>(tmp_src); }
static void __attribute__((regparm(3))) exec_ff30_push_1(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_PUSH<1>(tmp_src); }
static void __attribute__((regparm(3))) exec_ff30_push_2(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 cache->helper_PUSH<2>(tmp_src); }
int handle_code_byte(InstructionCacheEntry *entry, unsigned char code, int &op_mode) {
entry->offset_opcode = entry->inst_len;
switch (op_mode) {
case 0x0:
	{
	  switch(code) {
case 0x00:
{
/* instruction2 'add' ['00'] ['ASM', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'BYTE', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_00_add_0; } ;
break; }
case 0x01:
{
/* instruction2 'add' ['01'] ['ASM', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_01_add_1; } else {entry->execute = exec_01_add_2; } ;
break; }
case 0x02:
{
/* instruction2 'add' ['02'] ['ASM', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'DIRECTION', 'BYTE'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_DIRECTION|IC_BYTE ;
{entry->execute = exec_02_add_0; } ;
break; }
case 0x03:
{
/* instruction2 'add' ['03'] ['ASM', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_03_add_1; } else {entry->execute = exec_03_add_2; } ;
break; }
case 0x04:
{
/* instruction2 'add' ['04'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMM1', 'EAX', 'BYTE'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_BYTE ;
{entry->execute = exec_04_add_0; } ;
break; }
case 0x05:
{
/* instruction2 'add' ['05'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMMO', 'EAX'] */ ;
;entry->src = &entry->immediate ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_05_add_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_05_add_2; } ;
break; }
case 0x06:
{
/* instruction2 'push %es' ['06'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_06_push__es_1; } else {entry->execute = exec_06_push__es_2; } ;
break; }
case 0x07:
{
/* instruction2 'pop %es' ['07'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_07_pop__es_1; } else {entry->execute = exec_07_pop__es_2; } ;
break; }
case 0x08:
{
/* instruction2 'or' ['08'] ['ASM', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'BYTE', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_08_or_0; } ;
break; }
case 0x09:
{
/* instruction2 'or' ['09'] ['ASM', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_09_or_1; } else {entry->execute = exec_09_or_2; } ;
break; }
case 0x0a:
{
/* instruction2 'or' ['0a'] ['ASM', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'DIRECTION', 'BYTE'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_DIRECTION|IC_BYTE ;
{entry->execute = exec_0a_or_0; } ;
break; }
case 0x0b:
{
/* instruction2 'or' ['0b'] ['ASM', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_0b_or_1; } else {entry->execute = exec_0b_or_2; } ;
break; }
case 0x0c:
{
/* instruction2 'or' ['0c'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMM1', 'EAX', 'BYTE'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_BYTE ;
{entry->execute = exec_0c_or_0; } ;
break; }
case 0x0d:
{
/* instruction2 'or' ['0d'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMMO', 'EAX'] */ ;
;entry->src = &entry->immediate ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_0d_or_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_0d_or_2; } ;
break; }
case 0x0e:
{
/* instruction2 'push %cs' ['0e'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_0e_push__cs_1; } else {entry->execute = exec_0e_push__cs_2; } ;
break; }
case 0x0f:
{
op_mode = 1 ;
break; }
case 0x10:
{
/* instruction2 'adc' ['10'] ['ASM', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'BYTE', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_10_adc_0; } ;
break; }
case 0x11:
{
/* instruction2 'adc' ['11'] ['ASM', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_11_adc_1; } else {entry->execute = exec_11_adc_2; } ;
break; }
case 0x12:
{
/* instruction2 'adc' ['12'] ['ASM', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'DIRECTION', 'BYTE'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_DIRECTION|IC_BYTE ;
{entry->execute = exec_12_adc_0; } ;
break; }
case 0x13:
{
/* instruction2 'adc' ['13'] ['ASM', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_13_adc_1; } else {entry->execute = exec_13_adc_2; } ;
break; }
case 0x14:
{
/* instruction2 'adc' ['14'] ['ASM', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'IMM1', 'EAX', 'BYTE'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_BYTE ;
{entry->execute = exec_14_adc_0; } ;
break; }
case 0x15:
{
/* instruction2 'adc' ['15'] ['ASM', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'IMMO', 'EAX'] */ ;
;entry->src = &entry->immediate ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_15_adc_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_15_adc_2; } ;
break; }
case 0x16:
{
/* instruction2 'push %ss' ['16'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_16_push__ss_1; } else {entry->execute = exec_16_push__ss_2; } ;
break; }
case 0x17:
{
/* instruction2 'pop %ss' ['17'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_17_pop__ss_1; } else {entry->execute = exec_17_pop__ss_2; } ;
break; }
case 0x18:
{
/* instruction2 'sbb' ['18'] ['ASM', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'BYTE', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_18_sbb_0; } ;
break; }
case 0x19:
{
/* instruction2 'sbb' ['19'] ['ASM', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_19_sbb_1; } else {entry->execute = exec_19_sbb_2; } ;
break; }
case 0x1a:
{
/* instruction2 'sbb' ['1a'] ['ASM', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'DIRECTION', 'BYTE'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_DIRECTION|IC_BYTE ;
{entry->execute = exec_1a_sbb_0; } ;
break; }
case 0x1b:
{
/* instruction2 'sbb' ['1b'] ['ASM', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_1b_sbb_1; } else {entry->execute = exec_1b_sbb_2; } ;
break; }
case 0x1c:
{
/* instruction2 'sbb' ['1c'] ['ASM', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'IMM1', 'EAX', 'BYTE'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_BYTE ;
{entry->execute = exec_1c_sbb_0; } ;
break; }
case 0x1d:
{
/* instruction2 'sbb' ['1d'] ['ASM', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'IMMO', 'EAX'] */ ;
;entry->src = &entry->immediate ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_1d_sbb_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_1d_sbb_2; } ;
break; }
case 0x1e:
{
/* instruction2 'push %ds' ['1e'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_1e_push__ds_1; } else {entry->execute = exec_1e_push__ds_2; } ;
break; }
case 0x1f:
{
/* instruction2 'pop %ds' ['1f'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_1f_pop__ds_1; } else {entry->execute = exec_1f_pop__ds_2; } ;
break; }
case 0x20:
{
/* instruction2 'and' ['20'] ['ASM', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'BYTE', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_20_and_0; } ;
break; }
case 0x21:
{
/* instruction2 'and' ['21'] ['ASM', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_21_and_1; } else {entry->execute = exec_21_and_2; } ;
break; }
case 0x22:
{
/* instruction2 'and' ['22'] ['ASM', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'DIRECTION', 'BYTE'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_DIRECTION|IC_BYTE ;
{entry->execute = exec_22_and_0; } ;
break; }
case 0x23:
{
/* instruction2 'and' ['23'] ['ASM', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_23_and_1; } else {entry->execute = exec_23_and_2; } ;
break; }
case 0x24:
{
/* instruction2 'and' ['24'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMM1', 'EAX', 'BYTE'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_BYTE ;
{entry->execute = exec_24_and_0; } ;
break; }
case 0x25:
{
/* instruction2 'and' ['25'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMMO', 'EAX'] */ ;
;entry->src = &entry->immediate ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_25_and_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_25_and_2; } ;
break; }
case 0x26:
{
/* instruction2 'es' ['26'] ['PREFIX'] */ ;
entry->prefixes = (entry->prefixes & ~0xff00) | (0 << 8) ;
break; }
case 0x27:
{
/* instruction2 'daa' ['27'] ['ASM', 'EAX', 'NO_OS', 'SAVEFLAGS'] */ ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM|IC_SAVEFLAGS ;
{entry->execute = exec_27_daa_0; } ;
break; }
case 0x28:
{
/* instruction2 'sub' ['28'] ['ASM', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'BYTE', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_28_sub_0; } ;
break; }
case 0x29:
{
/* instruction2 'sub' ['29'] ['ASM', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_29_sub_1; } else {entry->execute = exec_29_sub_2; } ;
break; }
case 0x2a:
{
/* instruction2 'sub' ['2a'] ['ASM', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'DIRECTION', 'BYTE'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_DIRECTION|IC_BYTE ;
{entry->execute = exec_2a_sub_0; } ;
break; }
case 0x2b:
{
/* instruction2 'sub' ['2b'] ['ASM', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_2b_sub_1; } else {entry->execute = exec_2b_sub_2; } ;
break; }
case 0x2c:
{
/* instruction2 'sub' ['2c'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMM1', 'EAX', 'BYTE'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_BYTE ;
{entry->execute = exec_2c_sub_0; } ;
break; }
case 0x2d:
{
/* instruction2 'sub' ['2d'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMMO', 'EAX'] */ ;
;entry->src = &entry->immediate ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_2d_sub_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_2d_sub_2; } ;
break; }
case 0x2e:
{
/* instruction2 'cs' ['2e'] ['PREFIX'] */ ;
entry->prefixes = (entry->prefixes & ~0xff00) | (1 << 8) ;
break; }
case 0x2f:
{
/* instruction2 'das' ['2f'] ['ASM', 'EAX', 'NO_OS', 'SAVEFLAGS'] */ ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM|IC_SAVEFLAGS ;
{entry->execute = exec_2f_das_0; } ;
break; }
case 0x30:
{
/* instruction2 'xor' ['30'] ['ASM', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'BYTE', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_30_xor_0; } ;
break; }
case 0x31:
{
/* instruction2 'xor' ['31'] ['ASM', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_31_xor_1; } else {entry->execute = exec_31_xor_2; } ;
break; }
case 0x32:
{
/* instruction2 'xor' ['32'] ['ASM', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'DIRECTION', 'BYTE'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_DIRECTION|IC_BYTE ;
{entry->execute = exec_32_xor_0; } ;
break; }
case 0x33:
{
/* instruction2 'xor' ['33'] ['ASM', 'SAVEFLAGS', 'RMW', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_33_xor_1; } else {entry->execute = exec_33_xor_2; } ;
break; }
case 0x34:
{
/* instruction2 'xor' ['34'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMM1', 'EAX', 'BYTE'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_BYTE ;
{entry->execute = exec_34_xor_0; } ;
break; }
case 0x35:
{
/* instruction2 'xor' ['35'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMMO', 'EAX'] */ ;
;entry->src = &entry->immediate ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_35_xor_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_35_xor_2; } ;
break; }
case 0x36:
{
/* instruction2 'ss' ['36'] ['PREFIX'] */ ;
entry->prefixes = (entry->prefixes & ~0xff00) | (2 << 8) ;
break; }
case 0x37:
{
/* instruction2 'aaa' ['37'] ['ASM', 'EAX', 'NO_OS', 'LOADFLAGS', 'SAVEFLAGS'] */ ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS ;
{entry->execute = exec_37_aaa_0; } ;
break; }
case 0x38:
{
/* instruction2 'cmp' ['38'] ['ASM', 'READONLY', 'SAVEFLAGS', 'MODRMMEM', 'MODRM', 'BYTE'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_READONLY|IC_SAVEFLAGS|IC_MODRM|IC_BYTE ;
{entry->execute = exec_38_cmp_0; } ;
break; }
case 0x39:
{
/* instruction2 'cmp' ['39'] ['ASM', 'READONLY', 'SAVEFLAGS', 'MODRMMEM', 'MODRM'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_READONLY|IC_SAVEFLAGS|IC_MODRM ;
if (entry->operand_size == 1) {entry->execute = exec_39_cmp_1; } else {entry->execute = exec_39_cmp_2; } ;
break; }
case 0x3a:
{
/* instruction2 'cmp' ['3a'] ['ASM', 'READONLY', 'SAVEFLAGS', 'MODRMMEM', 'MODRM', 'DIRECTION', 'BYTE'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_READONLY|IC_SAVEFLAGS|IC_MODRM|IC_DIRECTION|IC_BYTE ;
{entry->execute = exec_3a_cmp_0; } ;
break; }
case 0x3b:
{
/* instruction2 'cmp' ['3b'] ['ASM', 'READONLY', 'SAVEFLAGS', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_READONLY|IC_SAVEFLAGS|IC_MODRM|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_3b_cmp_1; } else {entry->execute = exec_3b_cmp_2; } ;
break; }
case 0x3c:
{
/* instruction2 'cmp' ['3c'] ['ASM', 'READONLY', 'SAVEFLAGS', 'IMM1', 'EAX', 'BYTE'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM|IC_READONLY|IC_SAVEFLAGS|IC_BYTE ;
{entry->execute = exec_3c_cmp_0; } ;
break; }
case 0x3d:
{
/* instruction2 'cmp' ['3d'] ['ASM', 'READONLY', 'SAVEFLAGS', 'IMMO', 'EAX'] */ ;
;entry->src = &entry->immediate ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM|IC_READONLY|IC_SAVEFLAGS ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_3d_cmp_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_3d_cmp_2; } ;
break; }
case 0x3e:
{
/* instruction2 'ds' ['3e'] ['PREFIX'] */ ;
entry->prefixes = (entry->prefixes & ~0xff00) | (3 << 8) ;
break; }
case 0x3f:
{
/* instruction2 'aas' ['3f'] ['ASM', 'EAX', 'NO_OS', 'LOADFLAGS', 'SAVEFLAGS'] */ ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS ;
{entry->execute = exec_3f_aas_0; } ;
break; }
case 0x40 ... 0x47:
{
/* instruction2 'inc' ['41'] ['ASM', 'SAVEFLAGS', 'LOADFLAGS', 'RMW', 'IMPL'] */ ;
entry->dst = get_reg<0>(entry->data[entry->offset_opcode-1] & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_LOADFLAGS|IC_RMW ;
if (entry->operand_size == 1) {entry->execute = exec_41_inc_1; } else {entry->execute = exec_41_inc_2; } ;
break; }
case 0x48 ... 0x4f:
{
/* instruction2 'dec' ['49'] ['ASM', 'SAVEFLAGS', 'LOADFLAGS', 'RMW', 'IMPL'] */ ;
entry->dst = get_reg<0>(entry->data[entry->offset_opcode-1] & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_LOADFLAGS|IC_RMW ;
if (entry->operand_size == 1) {entry->execute = exec_49_dec_1; } else {entry->execute = exec_49_dec_2; } ;
break; }
case 0x50 ... 0x57:
{
/* instruction2 'push' ['51'] ['DIRECTION', 'IMPL'] */ ;
entry->dst = get_reg<0>(entry->data[entry->offset_opcode-1] & 0x7) ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_51_push_1; } else {entry->execute = exec_51_push_2; } ;
break; }
case 0x58 ... 0x5f:
{
/* instruction2 'pop' ['59'] ['IMPL'] */ ;
entry->dst = get_reg<0>(entry->data[entry->offset_opcode-1] & 0x7) ;
if (entry->operand_size == 1) {entry->execute = exec_59_pop_1; } else {entry->execute = exec_59_pop_2; } ;
break; }
case 0x60:
{
/* instruction2 'pusha' ['60'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_60_pusha_1; } else {entry->execute = exec_60_pusha_2; } ;
break; }
case 0x61:
{
/* instruction2 'popa' ['61'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_61_popa_1; } else {entry->execute = exec_61_popa_2; } ;
break; }
case 0x62:
{
/* instruction2 'bound' ['62'] ['MODRMMEM', 'MODRM'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
UNIMPLEMENTED(this) ;
break; }
case 0x63:
{
/* instruction2 'arpl' ['63'] ['MODRMREG', 'MODRM', 'WORD'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
UNIMPLEMENTED(this) ;
break; }
case 0x64:
{
/* instruction2 'fs' ['64'] ['PREFIX'] */ ;
entry->prefixes = (entry->prefixes & ~0xff00) | (4 << 8) ;
break; }
case 0x65:
{
/* instruction2 'gs' ['65'] ['PREFIX'] */ ;
entry->prefixes = (entry->prefixes & ~0xff00) | (5 << 8) ;
break; }
case 0x66:
{
/* instruction2 'data16' ['66'] ['PREFIX'] */ ;
entry->prefixes = (entry->prefixes & ~(0xff0000)) | (code << 16) ;
entry->operand_size = (((entry->cs_ar >> 10) & 1) + 1) ^ 3 ;
break; }
case 0x67:
{
/* instruction2 'addr16' ['67'] ['PREFIX'] */ ;
entry->prefixes = (entry->prefixes & ~(0xff000000)) | (code << 24) ;
entry->address_size = (((entry->cs_ar >> 10) & 1) + 1) ^ 3 ;
break; }
case 0x68:
{
/* instruction2 'push' ['68'] ['DIRECTION', 'IMMO'] */ ;
;entry->dst = &entry->immediate ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_68_push_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_68_push_2; } ;
break; }
case 0x69:
{
/* instruction2 'imul' ['69'] ['DIRECTION', 'MODRMMEM', 'MODRM', 'IMMO', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
;entry->dst = &entry->immediate ;
entry->flags = IC_DIRECTION|IC_MODRM|IC_DIRECTION ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_69_imul_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_69_imul_2; } ;
break; }
case 0x6a:
{
/* instruction2 'push' ['6a'] ['DIRECTION', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_6a_push_1; } else {entry->execute = exec_6a_push_2; } ;
break; }
case 0x6b:
{
/* instruction2 'imul' ['6b'] ['DIRECTION', 'MODRMMEM', 'MODRM', 'IMM1', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_DIRECTION|IC_MODRM|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_6b_imul_1; } else {entry->execute = exec_6b_imul_2; } ;
break; }
case 0x6c:
{
/* instruction2 'ins' ['6c'] ['BYTE'] */ ;
entry->flags = IC_BYTE ;
{entry->execute = exec_6c_ins_0; } ;
break; }
case 0x6d:
{
/* instruction2 'ins' ['6d'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_6d_ins_1; } else {entry->execute = exec_6d_ins_2; } ;
break; }
case 0x6e:
{
/* instruction2 'outs' ['6e'] ['BYTE'] */ ;
entry->flags = IC_BYTE ;
{entry->execute = exec_6e_outs_0; } ;
break; }
case 0x6f:
{
/* instruction2 'outs' ['6f'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_6f_outs_1; } else {entry->execute = exec_6f_outs_2; } ;
break; }
case 0x70:
{
/* instruction2 'jo' ['70'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_70_jo_1; } else {entry->execute = exec_70_jo_2; } ;
break; }
case 0x71:
{
/* instruction2 'jno' ['71'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_71_jno_1; } else {entry->execute = exec_71_jno_2; } ;
break; }
case 0x72:
{
/* instruction2 'jb' ['72'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_72_jb_1; } else {entry->execute = exec_72_jb_2; } ;
break; }
case 0x73:
{
/* instruction2 'jae' ['73'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_73_jae_1; } else {entry->execute = exec_73_jae_2; } ;
break; }
case 0x74:
{
/* instruction2 'je' ['74'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_74_je_1; } else {entry->execute = exec_74_je_2; } ;
break; }
case 0x75:
{
/* instruction2 'jne' ['75'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_75_jne_1; } else {entry->execute = exec_75_jne_2; } ;
break; }
case 0x76:
{
/* instruction2 'jbe' ['76'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_76_jbe_1; } else {entry->execute = exec_76_jbe_2; } ;
break; }
case 0x77:
{
/* instruction2 'ja' ['77'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_77_ja_1; } else {entry->execute = exec_77_ja_2; } ;
break; }
case 0x78:
{
/* instruction2 'js' ['78'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_78_js_1; } else {entry->execute = exec_78_js_2; } ;
break; }
case 0x79:
{
/* instruction2 'jns' ['79'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_79_jns_1; } else {entry->execute = exec_79_jns_2; } ;
break; }
case 0x7a:
{
/* instruction2 'jp' ['7a'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_7a_jp_1; } else {entry->execute = exec_7a_jp_2; } ;
break; }
case 0x7b:
{
/* instruction2 'jnp' ['7b'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_7b_jnp_1; } else {entry->execute = exec_7b_jnp_2; } ;
break; }
case 0x7c:
{
/* instruction2 'jl' ['7c'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_7c_jl_1; } else {entry->execute = exec_7c_jl_2; } ;
break; }
case 0x7d:
{
/* instruction2 'jge' ['7d'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_7d_jge_1; } else {entry->execute = exec_7d_jge_2; } ;
break; }
case 0x7e:
{
/* instruction2 'jle' ['7e'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_7e_jle_1; } else {entry->execute = exec_7e_jle_2; } ;
break; }
case 0x7f:
{
/* instruction2 'jg' ['7f'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_7f_jg_1; } else {entry->execute = exec_7f_jg_2; } ;
break; }
case 0x80:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0xfa & 0x38: { ;
/* instruction 'cmp' ['80', 'fa'] ['ASM', 'READONLY', 'SAVEFLAGS', 'IMM1', 'MODRM', 'GRP', 'BYTE'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_READONLY|IC_SAVEFLAGS|IC_MODRM|IC_BYTE ;
{entry->execute = exec_80fa_cmp_0; } ;
break; } ;
case 0xf2 & 0x38: { ;
/* instruction 'xor' ['80', 'f2'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_80f2_xor_0; } ;
break; } ;
case 0xea & 0x38: { ;
/* instruction 'sub' ['80', 'ea'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_80ea_sub_0; } ;
break; } ;
case 0xe2 & 0x38: { ;
/* instruction 'and' ['80', 'e2'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_80e2_and_0; } ;
break; } ;
case 0xda & 0x38: { ;
/* instruction 'sbb' ['80', 'da'] ['ASM', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_80da_sbb_0; } ;
break; } ;
case 0xd2 & 0x38: { ;
/* instruction 'adc' ['80', 'd2'] ['ASM', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_80d2_adc_0; } ;
break; } ;
case 0xca & 0x38: { ;
/* instruction 'or' ['80', 'ca'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_80ca_or_0; } ;
break; } ;
case 0xc2 & 0x38: { ;
/* instruction 'add' ['80', 'c2'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_80c2_add_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0x81:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0xfa & 0x38: { ;
/* instruction 'cmp' ['81', 'fa'] ['ASM', 'READONLY', 'SAVEFLAGS', 'IMMO', 'MODRM', 'GRP'] */ ;
;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_READONLY|IC_SAVEFLAGS|IC_MODRM ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_81fa_cmp_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_81fa_cmp_2; } ;
break; } ;
case 0xf2 & 0x38: { ;
/* instruction 'xor' ['81', 'f2'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMMO', 'MODRM', 'GRP', 'LOCK'] */ ;
;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_81f2_xor_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_81f2_xor_2; } ;
break; } ;
case 0xea & 0x38: { ;
/* instruction 'sub' ['81', 'ea'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMMO', 'MODRM', 'GRP', 'LOCK'] */ ;
;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_81ea_sub_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_81ea_sub_2; } ;
break; } ;
case 0xe2 & 0x38: { ;
/* instruction 'and' ['81', 'e2'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMMO', 'MODRM', 'GRP', 'LOCK'] */ ;
;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_81e2_and_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_81e2_and_2; } ;
break; } ;
case 0xda & 0x38: { ;
/* instruction 'sbb' ['81', 'da'] ['ASM', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'IMMO', 'MODRM', 'GRP', 'LOCK'] */ ;
;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_81da_sbb_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_81da_sbb_2; } ;
break; } ;
case 0xd2 & 0x38: { ;
/* instruction 'adc' ['81', 'd2'] ['ASM', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'IMMO', 'MODRM', 'GRP', 'LOCK'] */ ;
;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_81d2_adc_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_81d2_adc_2; } ;
break; } ;
case 0xca & 0x38: { ;
/* instruction 'or' ['81', 'ca'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMMO', 'MODRM', 'GRP', 'LOCK'] */ ;
;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_81ca_or_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_81ca_or_2; } ;
break; } ;
case 0xc2 & 0x38: { ;
/* instruction 'add' ['81', 'c2'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMMO', 'MODRM', 'GRP', 'LOCK'] */ ;
;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_81c2_add_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_81c2_add_2; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0x83:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0xfa & 0x38: { ;
/* instruction 'cmp' ['83', 'fa'] ['ASM', 'READONLY', 'SAVEFLAGS', 'IMM1', 'MODRM', 'GRP'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_READONLY|IC_SAVEFLAGS|IC_MODRM ;
if (entry->operand_size == 1) {entry->execute = exec_83fa_cmp_1; } else {entry->execute = exec_83fa_cmp_2; } ;
break; } ;
case 0xf2 & 0x38: { ;
/* instruction 'xor' ['83', 'f2'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'LOCK'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_83f2_xor_1; } else {entry->execute = exec_83f2_xor_2; } ;
break; } ;
case 0xea & 0x38: { ;
/* instruction 'sub' ['83', 'ea'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'LOCK'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_83ea_sub_1; } else {entry->execute = exec_83ea_sub_2; } ;
break; } ;
case 0xe2 & 0x38: { ;
/* instruction 'and' ['83', 'e2'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'LOCK'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_83e2_and_1; } else {entry->execute = exec_83e2_and_2; } ;
break; } ;
case 0xda & 0x38: { ;
/* instruction 'sbb' ['83', 'da'] ['ASM', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'LOCK'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_83da_sbb_1; } else {entry->execute = exec_83da_sbb_2; } ;
break; } ;
case 0xd2 & 0x38: { ;
/* instruction 'adc' ['83', 'd2'] ['ASM', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'LOCK'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_83d2_adc_1; } else {entry->execute = exec_83d2_adc_2; } ;
break; } ;
case 0xca & 0x38: { ;
/* instruction 'or' ['83', 'ca'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'LOCK'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_83ca_or_1; } else {entry->execute = exec_83ca_or_2; } ;
break; } ;
case 0xc2 & 0x38: { ;
/* instruction 'add' ['83', 'c2'] ['ASM', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'LOCK'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_83c2_add_1; } else {entry->execute = exec_83c2_add_2; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0x84:
{
/* instruction2 'test' ['84'] ['ASM', 'READONLY', 'SAVEFLAGS', 'MODRMMEM', 'MODRM', 'BYTE'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_READONLY|IC_SAVEFLAGS|IC_MODRM|IC_BYTE ;
{entry->execute = exec_84_test_0; } ;
break; }
case 0x85:
{
/* instruction2 'test' ['85'] ['ASM', 'READONLY', 'SAVEFLAGS', 'MODRMMEM', 'MODRM'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_READONLY|IC_SAVEFLAGS|IC_MODRM ;
if (entry->operand_size == 1) {entry->execute = exec_85_test_1; } else {entry->execute = exec_85_test_2; } ;
break; }
case 0x86:
{
/* instruction2 'xchg' ['86'] ['ASM', 'RMW', 'MODRMMEM', 'MODRM', 'BYTE', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_86_xchg_0; } ;
break; }
case 0x87:
{
/* instruction2 'xchg' ['87'] ['ASM', 'RMW', 'MODRMMEM', 'MODRM', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_87_xchg_1; } else {entry->execute = exec_87_xchg_2; } ;
break; }
case 0x88:
{
/* instruction2 'mov' ['88'] ['ASM', 'MODRMMEM', 'MODRM', 'BYTE'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_MODRM|IC_BYTE ;
{entry->execute = exec_88_mov_0; } ;
break; }
case 0x89:
{
/* instruction2 'mov' ['89'] ['ASM', 'MODRMMEM', 'MODRM'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_MODRM ;
if (entry->operand_size == 1) {entry->execute = exec_89_mov_1; } else {entry->execute = exec_89_mov_2; } ;
break; }
case 0x8a:
{
/* instruction2 'mov' ['8a'] ['ASM', 'MODRMMEM', 'MODRM', 'DIRECTION', 'BYTE'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_MODRM|IC_DIRECTION|IC_BYTE ;
{entry->execute = exec_8a_mov_0; } ;
break; }
case 0x8b:
{
/* instruction2 'mov' ['8b'] ['ASM', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_MODRM|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_8b_mov_1; } else {entry->execute = exec_8b_mov_2; } ;
break; }
case 0x8c:
{
/* instruction2 'mov %es,%edx' ['8c'] ['MODRM', 'DROP1'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_MODRM ;
if (entry->operand_size == 1) {entry->execute = exec_8c_mov__es__edx_1; } else {entry->execute = exec_8c_mov__es__edx_2; } ;
break; }
case 0x8d:
{
/* instruction2 'lea' ['8d'] ['MEMONLY', 'DIRECTION', 'SKIPMODRM', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
if (~entry->modrminfo & MRM_REG) { ;
entry->flags = IC_DIRECTION|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_8d_lea_1; } else {entry->execute = exec_8d_lea_2; } ;
} else  {  ;
Logging::printf("lea not implemented at %x - %x instr %02x%02x%02x\n", _cpu->eip, entry->modrminfo, entry->data[0], entry->data[1], entry->data[2]);  ;
UNIMPLEMENTED(this); } ;
break; }
case 0x8e:
{
/* instruction2 'mov %edx,%es' ['8e'] ['MODRM', 'DROP1', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_MODRM|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_8e_mov__edx__es_1; } else {entry->execute = exec_8e_mov__edx__es_2; } ;
break; }
case 0x8f:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x00 & 0x38: { ;
/* instruction 'pop' ['8f', '00'] ['OP1', 'MODRM', 'GRP'] */ ;
entry->flags = IC_MODRM ;
if (entry->operand_size == 1) {entry->execute = exec_8f00_pop_1; } else {entry->execute = exec_8f00_pop_2; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0x90 ... 0x97:
{
/* instruction2 'xchg' ['92'] ['ASM', 'RMW', 'EAX', 'IMPL', 'DIRECTION'] */ ;
entry->dst = get_reg<0>(entry->data[entry->offset_opcode-1] & 0x7) ;
entry->src = &_cpu->eax ;
entry->flags = IC_ASM|IC_RMW|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_92_xchg_1; } else {entry->execute = exec_92_xchg_2; } ;
break; }
case 0x98:
{
/* instruction2 'cwtl' ['98'] ['ASM', 'EAX'] */ ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM ;
if (entry->operand_size == 1) {entry->execute = exec_98_cwtl_1; } else {entry->execute = exec_98_cwtl_2; } ;
break; }
case 0x99:
{
/* instruction2 'cltd' ['99'] ['ASM', 'EAX', 'EDX', 'DIRECTION'] */ ;
entry->dst = &_cpu->edx ;
entry->src = &_cpu->eax ;
entry->flags = IC_ASM ;
if (entry->operand_size == 1) {entry->execute = exec_99_cltd_1; } else {entry->execute = exec_99_cltd_2; } ;
break; }
case 0x9a:
{
/* instruction2 'lcall' ['9a'] ['JMP', 'DIRECTION', 'LONGJMP'] */ ;
fetch_code(entry, 2 + (1 << entry->operand_size)) ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_9a_lcall_1; } else {entry->execute = exec_9a_lcall_2; } ;
break; }
case 0x9b:
{
/* instruction2 'fwait' ['9b'] ['NO_OS'] */ ;
{entry->execute = exec_9b_fwait_0; } ;
break; }
case 0x9c:
{
/* instruction2 'pushf' ['9c'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_9c_pushf_1; } else {entry->execute = exec_9c_pushf_2; } ;
break; }
case 0x9d:
{
/* instruction2 'popf' ['9d'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_9d_popf_1; } else {entry->execute = exec_9d_popf_2; } ;
break; }
case 0x9e:
{
/* instruction2 'sahf' ['9e'] ['NO_OS'] */ ;
{entry->execute = exec_9e_sahf_0; } ;
break; }
case 0x9f:
{
/* instruction2 'lahf' ['9f'] ['NO_OS'] */ ;
{entry->execute = exec_9f_lahf_0; } ;
break; }
case 0xa0:
{
/* instruction2 'mov' ['a0'] ['ASM', 'MOFS', 'DIRECTION', 'BYTE'] */ ;
fetch_code(entry, 1 << entry->address_size); entry->src = &_cpu->eax ;
entry->flags = IC_ASM|IC_MOFS|IC_DIRECTION|IC_BYTE ;
{entry->execute = exec_a0_mov_0; } ;
break; }
case 0xa1:
{
/* instruction2 'mov' ['a1'] ['ASM', 'MOFS', 'DIRECTION'] */ ;
fetch_code(entry, 1 << entry->address_size); entry->src = &_cpu->eax ;
entry->flags = IC_ASM|IC_MOFS|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_a1_mov_1; } else {entry->execute = exec_a1_mov_2; } ;
break; }
case 0xa2:
{
/* instruction2 'mov' ['a2'] ['ASM', 'MOFS', 'BYTE'] */ ;
fetch_code(entry, 1 << entry->address_size); entry->src = &_cpu->eax ;
entry->flags = IC_ASM|IC_MOFS|IC_BYTE ;
{entry->execute = exec_a2_mov_0; } ;
break; }
case 0xa3:
{
/* instruction2 'mov' ['a3'] ['ASM', 'MOFS'] */ ;
fetch_code(entry, 1 << entry->address_size); entry->src = &_cpu->eax ;
entry->flags = IC_ASM|IC_MOFS ;
if (entry->operand_size == 1) {entry->execute = exec_a3_mov_1; } else {entry->execute = exec_a3_mov_2; } ;
break; }
case 0xa4:
{
/* instruction2 'movs' ['a4'] ['BYTE'] */ ;
entry->flags = IC_BYTE ;
{entry->execute = exec_a4_movs_0; } ;
break; }
case 0xa5:
{
/* instruction2 'movs' ['a5'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_a5_movs_1; } else {entry->execute = exec_a5_movs_2; } ;
break; }
case 0xa6:
{
/* instruction2 'cmps' ['a6'] ['BYTE'] */ ;
entry->flags = IC_BYTE ;
{entry->execute = exec_a6_cmps_0; } ;
break; }
case 0xa7:
{
/* instruction2 'cmps' ['a7'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_a7_cmps_1; } else {entry->execute = exec_a7_cmps_2; } ;
break; }
case 0xa8:
{
/* instruction2 'test' ['a8'] ['ASM', 'READONLY', 'SAVEFLAGS', 'IMM1', 'EAX', 'BYTE'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM|IC_READONLY|IC_SAVEFLAGS|IC_BYTE ;
{entry->execute = exec_a8_test_0; } ;
break; }
case 0xa9:
{
/* instruction2 'test' ['a9'] ['ASM', 'READONLY', 'SAVEFLAGS', 'IMMO', 'EAX'] */ ;
;entry->src = &entry->immediate ;
entry->dst = &_cpu->eax ;
entry->flags = IC_ASM|IC_READONLY|IC_SAVEFLAGS ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_a9_test_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_a9_test_2; } ;
break; }
case 0xaa:
{
/* instruction2 'stos' ['aa'] ['BYTE'] */ ;
entry->flags = IC_BYTE ;
{entry->execute = exec_aa_stos_0; } ;
break; }
case 0xab:
{
/* instruction2 'stos' ['ab'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_ab_stos_1; } else {entry->execute = exec_ab_stos_2; } ;
break; }
case 0xac:
{
/* instruction2 'lods' ['ac'] ['BYTE'] */ ;
entry->flags = IC_BYTE ;
{entry->execute = exec_ac_lods_0; } ;
break; }
case 0xad:
{
/* instruction2 'lods' ['ad'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_ad_lods_1; } else {entry->execute = exec_ad_lods_2; } ;
break; }
case 0xae:
{
/* instruction2 'scas' ['ae'] ['BYTE'] */ ;
entry->flags = IC_BYTE ;
{entry->execute = exec_ae_scas_0; } ;
break; }
case 0xaf:
{
/* instruction2 'scas' ['af'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_af_scas_1; } else {entry->execute = exec_af_scas_2; } ;
break; }
case 0xb0 ... 0xb7:
{
/* instruction2 'mov' ['b2'] ['ASM', 'IMM1', 'IMPL', 'BYTE'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->dst = get_reg<1>(entry->data[entry->offset_opcode-1] & 0x7) ;
entry->flags = IC_ASM|IC_BYTE ;
{entry->execute = exec_b2_mov_0; } ;
break; }
case 0xb8 ... 0xbf:
{
/* instruction2 'mov' ['ba'] ['ASM', 'IMMO', 'IMPL'] */ ;
;entry->src = &entry->immediate ;
entry->dst = get_reg<0>(entry->data[entry->offset_opcode-1] & 0x7) ;
entry->flags = IC_ASM ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_ba_mov_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_ba_mov_2; } ;
break; }
case 0xc0:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0xfa & 0x38: { ;
/* instruction 'sar' ['c0', 'fa'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_c0fa_sar_0; } ;
break; } ;
case 0xea & 0x38: { ;
/* instruction 'shr' ['c0', 'ea'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_c0ea_shr_0; } ;
break; } ;
case 0xe2 & 0x38: { ;
/* instruction 'shl' ['c0', 'e2'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_c0e2_shl_0; } ;
break; } ;
case 0xda & 0x38: { ;
/* instruction 'rcr' ['c0', 'da'] ['ASM', 'CONST1', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_c0da_rcr_0; } ;
break; } ;
case 0xd2 & 0x38: { ;
/* instruction 'rcl' ['c0', 'd2'] ['ASM', 'CONST1', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_c0d2_rcl_0; } ;
break; } ;
case 0xca & 0x38: { ;
/* instruction 'ror' ['c0', 'ca'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_c0ca_ror_0; } ;
break; } ;
case 0xc2 & 0x38: { ;
/* instruction 'rol' ['c0', 'c2'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_c0c2_rol_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0xc1:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0xfa & 0x38: { ;
/* instruction 'sar' ['c1', 'fa'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_c1fa_sar_1; } else {entry->execute = exec_c1fa_sar_2; } ;
break; } ;
case 0xea & 0x38: { ;
/* instruction 'shr' ['c1', 'ea'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_c1ea_shr_1; } else {entry->execute = exec_c1ea_shr_2; } ;
break; } ;
case 0xe2 & 0x38: { ;
/* instruction 'shl' ['c1', 'e2'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_c1e2_shl_1; } else {entry->execute = exec_c1e2_shl_2; } ;
break; } ;
case 0xda & 0x38: { ;
/* instruction 'rcr' ['c1', 'da'] ['ASM', 'CONST1', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_c1da_rcr_1; } else {entry->execute = exec_c1da_rcr_2; } ;
break; } ;
case 0xd2 & 0x38: { ;
/* instruction 'rcl' ['c1', 'd2'] ['ASM', 'CONST1', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_c1d2_rcl_1; } else {entry->execute = exec_c1d2_rcl_2; } ;
break; } ;
case 0xca & 0x38: { ;
/* instruction 'ror' ['c1', 'ca'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_c1ca_ror_1; } else {entry->execute = exec_c1ca_ror_2; } ;
break; } ;
case 0xc2 & 0x38: { ;
/* instruction 'rol' ['c1', 'c2'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'IMM1', 'MODRM', 'GRP', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_c1c2_rol_1; } else {entry->execute = exec_c1c2_rol_2; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0xc2:
{
/* instruction2 'ret' ['c2'] ['DIRECTION', 'IMM2'] */ ;
fetch_code(entry, 2); entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->dst = &entry->immediate ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_c2_ret_1; } else {entry->execute = exec_c2_ret_2; } ;
break; }
case 0xc3:
{
/* instruction2 'ret' ['c3'] ['DIRECTION'] */ ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_c3_ret_1; } else {entry->execute = exec_c3_ret_2; } ;
break; }
case 0xc4:
{
/* instruction2 'les' ['c4'] ['SKIPMODRM', 'MODRM', 'MEMONLY', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
if (~entry->modrminfo & MRM_REG) { ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_c4_les_1; } else {entry->execute = exec_c4_les_2; } ;
} else  {  ;
Logging::printf("les not implemented at %x - %x instr %02x%02x%02x\n", _cpu->eip, entry->modrminfo, entry->data[0], entry->data[1], entry->data[2]);  ;
UNIMPLEMENTED(this); } ;
break; }
case 0xc5:
{
/* instruction2 'lds' ['c5'] ['SKIPMODRM', 'MODRM', 'MEMONLY', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
if (~entry->modrminfo & MRM_REG) { ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_c5_lds_1; } else {entry->execute = exec_c5_lds_2; } ;
} else  {  ;
Logging::printf("lds not implemented at %x - %x instr %02x%02x%02x\n", _cpu->eip, entry->modrminfo, entry->data[0], entry->data[1], entry->data[2]);  ;
UNIMPLEMENTED(this); } ;
break; }
case 0xc6:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x02 & 0x38: { ;
/* instruction 'mov' ['c6', '02'] ['ASM', 'OP1', 'MODRM', 'GRP', 'IMM1', 'BYTE'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_MODRM|IC_BYTE ;
{entry->execute = exec_c602_mov_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0xc7:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x02 & 0x38: { ;
/* instruction 'mov' ['c7', '02'] ['ASM', 'OP1', 'MODRM', 'GRP', 'IMMO'] */ ;
;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_MODRM ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_c702_mov_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_c702_mov_2; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0xc8:
{
/* instruction2 'enter' ['c8'] ['IMM3'] */ ;
fetch_code(entry, 3); entry->immediate = *reinterpret_cast<unsigned *>(entry->data+entry->inst_len - 3);entry->src = &entry->immediate ;
if (entry->operand_size == 1) {entry->execute = exec_c8_enter_1; } else {entry->execute = exec_c8_enter_2; } ;
break; }
case 0xc9:
{
/* instruction2 'leave' ['c9'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_c9_leave_1; } else {entry->execute = exec_c9_leave_2; } ;
break; }
case 0xca:
{
/* instruction2 'lret' ['ca'] ['DIRECTION', 'IMM2'] */ ;
fetch_code(entry, 2); entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->dst = &entry->immediate ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_ca_lret_1; } else {entry->execute = exec_ca_lret_2; } ;
break; }
case 0xcb:
{
/* instruction2 'lret' ['cb'] ['DIRECTION'] */ ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_cb_lret_1; } else {entry->execute = exec_cb_lret_2; } ;
break; }
case 0xcc:
{
/* instruction2 'int3' ['cc'] ['NO_OS'] */ ;
{entry->execute = exec_cc_int3_0; } ;
break; }
case 0xcd:
{
/* instruction2 'int' ['cd'] ['NO_OS', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
{entry->execute = exec_cd_int_0; } ;
break; }
case 0xce:
{
/* instruction2 'into' ['ce'] ['NO_OS'] */ ;
{entry->execute = exec_ce_into_0; } ;
break; }
case 0xcf:
{
/* instruction2 'iret' ['cf'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_cf_iret_1; } else {entry->execute = exec_cf_iret_2; } ;
break; }
case 0xd0:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x38 & 0x38: { ;
/* instruction 'sar' ['d0', '38'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_d038_sar_0; } ;
break; } ;
case 0x28 & 0x38: { ;
/* instruction 'shr' ['d0', '28'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_d028_shr_0; } ;
break; } ;
case 0x20 & 0x38: { ;
/* instruction 'shl' ['d0', '20'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_d020_shl_0; } ;
break; } ;
case 0x18 & 0x38: { ;
/* instruction 'rcr' ['d0', '18'] ['ASM', 'CONST1', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_d018_rcr_0; } ;
break; } ;
case 0x10 & 0x38: { ;
/* instruction 'rcl' ['d0', '10'] ['ASM', 'CONST1', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_d010_rcl_0; } ;
break; } ;
case 0x08 & 0x38: { ;
/* instruction 'ror' ['d0', '08'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_d008_ror_0; } ;
break; } ;
case 0x00 & 0x38: { ;
/* instruction 'rol' ['d0', '00'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_d000_rol_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0xd1:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x38 & 0x38: { ;
/* instruction 'sar' ['d1', '38'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_d138_sar_1; } else {entry->execute = exec_d138_sar_2; } ;
break; } ;
case 0x28 & 0x38: { ;
/* instruction 'shr' ['d1', '28'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_d128_shr_1; } else {entry->execute = exec_d128_shr_2; } ;
break; } ;
case 0x20 & 0x38: { ;
/* instruction 'shl' ['d1', '20'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_d120_shl_1; } else {entry->execute = exec_d120_shl_2; } ;
break; } ;
case 0x18 & 0x38: { ;
/* instruction 'rcr' ['d1', '18'] ['ASM', 'CONST1', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_d118_rcr_1; } else {entry->execute = exec_d118_rcr_2; } ;
break; } ;
case 0x10 & 0x38: { ;
/* instruction 'rcl' ['d1', '10'] ['ASM', 'CONST1', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_d110_rcl_1; } else {entry->execute = exec_d110_rcl_2; } ;
break; } ;
case 0x08 & 0x38: { ;
/* instruction 'ror' ['d1', '08'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_d108_ror_1; } else {entry->execute = exec_d108_ror_2; } ;
break; } ;
case 0x00 & 0x38: { ;
/* instruction 'rol' ['d1', '00'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_d100_rol_1; } else {entry->execute = exec_d100_rol_2; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0xd2:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x38 & 0x38: { ;
/* instruction 'sar' ['d2', '38'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'ECX', 'BYTE', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->src = &_cpu->ecx ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_d238_sar_0; } ;
break; } ;
case 0x28 & 0x38: { ;
/* instruction 'shr' ['d2', '28'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'ECX', 'BYTE', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->src = &_cpu->ecx ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_d228_shr_0; } ;
break; } ;
case 0x20 & 0x38: { ;
/* instruction 'shl' ['d2', '20'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'ECX', 'BYTE', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->src = &_cpu->ecx ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_d220_shl_0; } ;
break; } ;
case 0x18 & 0x38: { ;
/* instruction 'rcr' ['d2', '18'] ['ASM', 'CONST1', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'ECX', 'BYTE', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->src = &_cpu->ecx ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_d218_rcr_0; } ;
break; } ;
case 0x10 & 0x38: { ;
/* instruction 'rcl' ['d2', '10'] ['ASM', 'CONST1', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'ECX', 'BYTE', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->src = &_cpu->ecx ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_d210_rcl_0; } ;
break; } ;
case 0x08 & 0x38: { ;
/* instruction 'ror' ['d2', '08'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'ECX', 'BYTE', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->src = &_cpu->ecx ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_d208_ror_0; } ;
break; } ;
case 0x00 & 0x38: { ;
/* instruction 'rol' ['d2', '00'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'ECX', 'BYTE', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->src = &_cpu->ecx ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_d200_rol_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0xd3:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x38 & 0x38: { ;
/* instruction 'sar' ['d3', '38'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'ECX', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->src = &_cpu->ecx ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_d338_sar_1; } else {entry->execute = exec_d338_sar_2; } ;
break; } ;
case 0x28 & 0x38: { ;
/* instruction 'shr' ['d3', '28'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'ECX', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->src = &_cpu->ecx ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_d328_shr_1; } else {entry->execute = exec_d328_shr_2; } ;
break; } ;
case 0x20 & 0x38: { ;
/* instruction 'shl' ['d3', '20'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'ECX', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->src = &_cpu->ecx ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_d320_shl_1; } else {entry->execute = exec_d320_shl_2; } ;
break; } ;
case 0x18 & 0x38: { ;
/* instruction 'rcr' ['d3', '18'] ['ASM', 'CONST1', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'ECX', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->src = &_cpu->ecx ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_d318_rcr_1; } else {entry->execute = exec_d318_rcr_2; } ;
break; } ;
case 0x10 & 0x38: { ;
/* instruction 'rcl' ['d3', '10'] ['ASM', 'CONST1', 'LOADFLAGS', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'ECX', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->src = &_cpu->ecx ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_d310_rcl_1; } else {entry->execute = exec_d310_rcl_2; } ;
break; } ;
case 0x08 & 0x38: { ;
/* instruction 'ror' ['d3', '08'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'ECX', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->src = &_cpu->ecx ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_d308_ror_1; } else {entry->execute = exec_d308_ror_2; } ;
break; } ;
case 0x00 & 0x38: { ;
/* instruction 'rol' ['d3', '00'] ['ASM', 'CONST1', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'ECX', 'LOCK'] */ ;
entry->immediate = 1;entry->src = &entry->immediate ;
entry->src = &_cpu->ecx ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_d300_rol_1; } else {entry->execute = exec_d300_rol_2; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0xd4:
{
/* instruction2 'aam' ['d4'] ['NO_OS', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
{entry->execute = exec_d4_aam_0; } ;
break; }
case 0xd5:
{
/* instruction2 'aad' ['d5'] ['NO_OS', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
{entry->execute = exec_d5_aad_0; } ;
break; }
case 0xd7:
{
/* instruction2 'xlat' ['d7'] ['NO_OS', 'BYTE'] */ ;
entry->flags = IC_BYTE ;
{entry->execute = exec_d7_xlat_0; } ;
break; }
case 0xd9:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x38 & 0x38: { ;
/* instruction 'fnstcw' ['d9', '38'] ['FPU', 'NO_OS', 'OP1', 'MODRM', 'GRP', 'WORD'] */ ;
entry->flags = IC_MODRM ;
{entry->execute = exec_d938_fnstcw_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0xda:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x18 & 0x38: { ;
/* instruction 'ficomp' ['da', '18'] ['FPU', 'NO_OS', 'OP1', 'MODRM', 'GRP'] */ ;
entry->flags = IC_MODRM ;
{entry->execute = exec_da18_ficomp_0; } ;
break; } ;
case 0x10 & 0x38: { ;
/* instruction 'ficom' ['da', '10'] ['FPU', 'NO_OS', 'OP1', 'MODRM', 'GRP'] */ ;
entry->flags = IC_MODRM ;
{entry->execute = exec_da10_ficom_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0xdb:
{
op_mode = 3 ;
break; }
case 0xdd:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x38 & 0x38: { ;
/* instruction 'fnstsw' ['dd', '38'] ['FPU', 'NO_OS', 'OP1', 'MODRM', 'GRP', 'WORD'] */ ;
entry->flags = IC_MODRM ;
{entry->execute = exec_dd38_fnstsw_0; } ;
break; } ;
case 0x20 & 0x38: { ;
/* instruction 'frstor' ['dd', '20'] ['SKIPMODRM', 'NO_OS', 'OP1', 'MODRM', 'GRP'] */ ;
{entry->execute = exec_dd20_frstor_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0xde:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x18 & 0x38: { ;
/* instruction 'ficomp' ['de', '18'] ['FPU', 'NO_OS', 'OP1', 'MODRM', 'GRP'] */ ;
entry->flags = IC_MODRM ;
{entry->execute = exec_de18_ficomp_0; } ;
break; } ;
case 0x10 & 0x38: { ;
/* instruction 'ficom' ['de', '10'] ['FPU', 'NO_OS', 'OP1', 'MODRM', 'GRP'] */ ;
entry->flags = IC_MODRM ;
{entry->execute = exec_de10_ficom_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0xdf:
{
op_mode = 4 ;
break; }
case 0xe0:
{
/* instruction2 'loopne' ['e0'] ['JMP', 'DIRECTION', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_e0_loopne_1; } else {entry->execute = exec_e0_loopne_2; } ;
break; }
case 0xe1:
{
/* instruction2 'loope' ['e1'] ['JMP', 'DIRECTION', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_e1_loope_1; } else {entry->execute = exec_e1_loope_2; } ;
break; }
case 0xe2:
{
/* instruction2 'loop' ['e2'] ['JMP', 'DIRECTION', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_e2_loop_1; } else {entry->execute = exec_e2_loop_2; } ;
break; }
case 0xe3:
{
/* instruction2 'jecxz' ['e3'] ['JMP', 'DIRECTION', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_e3_jecxz_1; } else {entry->execute = exec_e3_jecxz_2; } ;
break; }
case 0xe4:
{
/* instruction2 'in' ['e4'] ['IMM1', 'EAX', 'BYTE'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->dst = &_cpu->eax ;
entry->flags = IC_BYTE ;
{entry->execute = exec_e4_in_0; } ;
break; }
case 0xe5:
{
/* instruction2 'in' ['e5'] ['IMM1', 'PORT'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
if (entry->operand_size == 1) {entry->execute = exec_e5_in_1; } else {entry->execute = exec_e5_in_2; } ;
break; }
case 0xe6:
{
/* instruction2 'out' ['e6'] ['IMM1', 'EAX', 'BYTE', 'DIRECTION'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->src = &_cpu->eax ;
entry->flags = IC_BYTE|IC_DIRECTION ;
{entry->execute = exec_e6_out_0; } ;
break; }
case 0xe7:
{
/* instruction2 'out' ['e7'] ['IMM1', 'PORT'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
if (entry->operand_size == 1) {entry->execute = exec_e7_out_1; } else {entry->execute = exec_e7_out_2; } ;
break; }
case 0xe8:
{
/* instruction2 'call' ['e8'] ['JMP', 'DIRECTION', 'IMMO'] */ ;
;entry->dst = &entry->immediate ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_e8_call_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_e8_call_2; } ;
break; }
case 0xe9:
{
/* instruction2 'jmp' ['e9'] ['JMP', 'DIRECTION', 'IMMO'] */ ;
;entry->dst = &entry->immediate ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_e9_jmp_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_e9_jmp_2; } ;
break; }
case 0xea:
{
/* instruction2 'ljmp' ['ea'] ['JMP', 'DIRECTION', 'LONGJMP'] */ ;
fetch_code(entry, 2 + (1 << entry->operand_size)) ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_ea_ljmp_1; } else {entry->execute = exec_ea_ljmp_2; } ;
break; }
case 0xeb:
{
/* instruction2 'jmp' ['eb'] ['JMP', 'DIRECTION', 'IMM1'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->dst = &entry->immediate ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_eb_jmp_1; } else {entry->execute = exec_eb_jmp_2; } ;
break; }
case 0xec:
{
/* instruction2 'in' ['ec'] ['PORT', 'BYTE', 'EDX'] */ ;
entry->src = &_cpu->edx ;
entry->flags = IC_BYTE ;
{entry->execute = exec_ec_in_0; } ;
break; }
case 0xed:
{
/* instruction2 'in' ['ed'] ['PORT', 'EDX'] */ ;
entry->src = &_cpu->edx ;
if (entry->operand_size == 1) {entry->execute = exec_ed_in_1; } else {entry->execute = exec_ed_in_2; } ;
break; }
case 0xee:
{
/* instruction2 'out' ['ee'] ['PORT', 'BYTE', 'EDX'] */ ;
entry->src = &_cpu->edx ;
entry->flags = IC_BYTE ;
{entry->execute = exec_ee_out_0; } ;
break; }
case 0xef:
{
/* instruction2 'out' ['ef'] ['PORT', 'EDX'] */ ;
entry->src = &_cpu->edx ;
if (entry->operand_size == 1) {entry->execute = exec_ef_out_1; } else {entry->execute = exec_ef_out_2; } ;
break; }
case 0xf0:
{
/* instruction2 'lock' ['f0'] ['PREFIX'] */ ;
entry->prefixes = (entry->prefixes & ~(0xff)) | (code << 0) ;
break; }
case 0xf2:
{
/* instruction2 'repnz' ['f2'] ['PREFIX'] */ ;
entry->prefixes = (entry->prefixes & ~(0xff)) | (code << 0) ;
break; }
case 0xf3:
{
/* instruction2 'repz' ['f3'] ['PREFIX'] */ ;
entry->prefixes = (entry->prefixes & ~(0xff)) | (code << 0) ;
break; }
case 0xf4:
{
/* instruction2 'hlt' ['f4'] ['NO_OS', 'CPL0'] */ ;
{entry->execute = exec_f4_hlt_0; } ;
break; }
case 0xf5:
{
/* instruction2 'cmc' ['f5'] ['NO_OS'] */ ;
{entry->execute = exec_f5_cmc_0; } ;
break; }
case 0xf6:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0xc2 & 0x38: { ;
/* instruction 'test' ['f6', 'c2'] ['ASM', 'READONLY', 'SAVEFLAGS', 'IMM1', 'MODRM', 'GRP', 'BYTE'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_READONLY|IC_SAVEFLAGS|IC_MODRM|IC_BYTE ;
{entry->execute = exec_f6c2_test_0; } ;
break; } ;
case 0x38 & 0x38: { ;
/* instruction 'idiv' ['f6', '38'] ['DIRECTION', 'OP1', 'MODRM', 'GRP', 'BYTE'] */ ;
entry->flags = IC_DIRECTION|IC_MODRM|IC_BYTE ;
{entry->execute = exec_f638_idiv_0; } ;
break; } ;
case 0x30 & 0x38: { ;
/* instruction 'div' ['f6', '30'] ['DIRECTION', 'OP1', 'MODRM', 'GRP', 'BYTE'] */ ;
entry->flags = IC_DIRECTION|IC_MODRM|IC_BYTE ;
{entry->execute = exec_f630_div_0; } ;
break; } ;
case 0x28 & 0x38: { ;
/* instruction 'imul' ['f6', '28'] ['DIRECTION', 'OP1', 'MODRM', 'GRP', 'BYTE'] */ ;
entry->flags = IC_DIRECTION|IC_MODRM|IC_BYTE ;
{entry->execute = exec_f628_imul_0; } ;
break; } ;
case 0x20 & 0x38: { ;
/* instruction 'mul' ['f6', '20'] ['DIRECTION', 'OP1', 'MODRM', 'GRP', 'BYTE'] */ ;
entry->flags = IC_DIRECTION|IC_MODRM|IC_BYTE ;
{entry->execute = exec_f620_mul_0; } ;
break; } ;
case 0x18 & 0x38: { ;
/* instruction 'neg' ['f6', '18'] ['ASM', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_f618_neg_0; } ;
break; } ;
case 0x10 & 0x38: { ;
/* instruction 'not' ['f6', '10'] ['ASM', 'RMW', 'OP1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
entry->flags = IC_ASM|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_f610_not_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0xf7:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0xc2 & 0x38: { ;
/* instruction 'test' ['f7', 'c2'] ['ASM', 'READONLY', 'SAVEFLAGS', 'IMMO', 'MODRM', 'GRP'] */ ;
;entry->src = &entry->immediate ;
entry->flags = IC_ASM|IC_READONLY|IC_SAVEFLAGS|IC_MODRM ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_f7c2_test_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_f7c2_test_2; } ;
break; } ;
case 0x38 & 0x38: { ;
/* instruction 'idiv' ['f7', '38'] ['DIRECTION', 'OP1', 'MODRM', 'GRP'] */ ;
entry->flags = IC_DIRECTION|IC_MODRM ;
if (entry->operand_size == 1) {entry->execute = exec_f738_idiv_1; } else {entry->execute = exec_f738_idiv_2; } ;
break; } ;
case 0x30 & 0x38: { ;
/* instruction 'div' ['f7', '30'] ['DIRECTION', 'OP1', 'MODRM', 'GRP'] */ ;
entry->flags = IC_DIRECTION|IC_MODRM ;
if (entry->operand_size == 1) {entry->execute = exec_f730_div_1; } else {entry->execute = exec_f730_div_2; } ;
break; } ;
case 0x28 & 0x38: { ;
/* instruction 'imul' ['f7', '28'] ['DIRECTION', 'OP1', 'MODRM', 'GRP'] */ ;
entry->flags = IC_DIRECTION|IC_MODRM ;
if (entry->operand_size == 1) {entry->execute = exec_f728_imul_1; } else {entry->execute = exec_f728_imul_2; } ;
break; } ;
case 0x20 & 0x38: { ;
/* instruction 'mul' ['f7', '20'] ['DIRECTION', 'OP1', 'MODRM', 'GRP'] */ ;
entry->flags = IC_DIRECTION|IC_MODRM ;
if (entry->operand_size == 1) {entry->execute = exec_f720_mul_1; } else {entry->execute = exec_f720_mul_2; } ;
break; } ;
case 0x18 & 0x38: { ;
/* instruction 'neg' ['f7', '18'] ['ASM', 'SAVEFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'LOCK'] */ ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_f718_neg_1; } else {entry->execute = exec_f718_neg_2; } ;
break; } ;
case 0x10 & 0x38: { ;
/* instruction 'not' ['f7', '10'] ['ASM', 'RMW', 'OP1', 'MODRM', 'GRP', 'LOCK'] */ ;
entry->flags = IC_ASM|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_f710_not_1; } else {entry->execute = exec_f710_not_2; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0xf8:
{
/* instruction2 'clc' ['f8'] ['NO_OS'] */ ;
{entry->execute = exec_f8_clc_0; } ;
break; }
case 0xf9:
{
/* instruction2 'stc' ['f9'] ['NO_OS'] */ ;
{entry->execute = exec_f9_stc_0; } ;
break; }
case 0xfa:
{
/* instruction2 'cli' ['fa'] ['NO_OS'] */ ;
{entry->execute = exec_fa_cli_0; } ;
break; }
case 0xfb:
{
/* instruction2 'sti' ['fb'] ['NO_OS'] */ ;
{entry->execute = exec_fb_sti_0; } ;
break; }
case 0xfc:
{
/* instruction2 'cld' ['fc'] ['NO_OS'] */ ;
{entry->execute = exec_fc_cld_0; } ;
break; }
case 0xfd:
{
/* instruction2 'std' ['fd'] ['NO_OS'] */ ;
{entry->execute = exec_fd_std_0; } ;
break; }
case 0xfe:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x08 & 0x38: { ;
/* instruction 'dec' ['fe', '08'] ['ASM', 'SAVEFLAGS', 'LOADFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_LOADFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_fe08_dec_0; } ;
break; } ;
case 0x00 & 0x38: { ;
/* instruction 'inc' ['fe', '00'] ['ASM', 'SAVEFLAGS', 'LOADFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'BYTE', 'LOCK'] */ ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_LOADFLAGS|IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_fe00_inc_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0xff:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x30 & 0x38: { ;
/* instruction 'push' ['ff', '30'] ['DIRECTION', 'OP1', 'MODRM', 'GRP'] */ ;
entry->flags = IC_DIRECTION|IC_MODRM ;
if (entry->operand_size == 1) {entry->execute = exec_ff30_push_1; } else {entry->execute = exec_ff30_push_2; } ;
break; } ;
case 0x2d & 0x38: { ;
/* instruction 'ljmp' ['ff', '2d'] ['JMP', 'DIRECTION', 'MODRM', 'GRP'] */ ;
entry->flags = IC_DIRECTION|IC_MODRM ;
if (entry->operand_size == 1) {entry->execute = exec_ff2d_ljmp_1; } else {entry->execute = exec_ff2d_ljmp_2; } ;
break; } ;
case 0x25 & 0x38: { ;
/* instruction 'jmp' ['ff', '25'] ['JMP', 'DIRECTION', 'MODRM', 'GRP'] */ ;
entry->flags = IC_DIRECTION|IC_MODRM ;
if (entry->operand_size == 1) {entry->execute = exec_ff25_jmp_1; } else {entry->execute = exec_ff25_jmp_2; } ;
break; } ;
case 0x1d & 0x38: { ;
/* instruction 'lcall' ['ff', '1d'] ['JMP', 'DIRECTION', 'MODRM', 'GRP'] */ ;
entry->flags = IC_DIRECTION|IC_MODRM ;
if (entry->operand_size == 1) {entry->execute = exec_ff1d_lcall_1; } else {entry->execute = exec_ff1d_lcall_2; } ;
break; } ;
case 0x15 & 0x38: { ;
/* instruction 'call' ['ff', '15'] ['JMP', 'DIRECTION', 'MODRM', 'GRP'] */ ;
entry->flags = IC_DIRECTION|IC_MODRM ;
if (entry->operand_size == 1) {entry->execute = exec_ff15_call_1; } else {entry->execute = exec_ff15_call_2; } ;
break; } ;
case 0x08 & 0x38: { ;
/* instruction 'dec' ['ff', '08'] ['ASM', 'SAVEFLAGS', 'LOADFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'LOCK'] */ ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_LOADFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_ff08_dec_1; } else {entry->execute = exec_ff08_dec_2; } ;
break; } ;
case 0x00 & 0x38: { ;
/* instruction 'inc' ['ff', '00'] ['ASM', 'SAVEFLAGS', 'LOADFLAGS', 'RMW', 'OP1', 'MODRM', 'GRP', 'LOCK'] */ ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_LOADFLAGS|IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_ff00_inc_1; } else {entry->execute = exec_ff00_inc_2; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
default:
	      fetch_code(entry, 4);
	      Logging::printf("unimplemented case %x at line %d code %02x%02x%02x\n", code, __LINE__, entry->data[0], entry->data[1], entry->data[2]);
	      UNIMPLEMENTED(this);
	  }
	}
	break;
case 0x1:
	{
	  switch(code) {
case 0x00:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x28 & 0x38: { ;
/* instruction 'verw' ['0f', '00', '28'] ['OP1', 'MODRM', 'GRP', 'WORD'] */ ;
UNIMPLEMENTED(this) ;
break; } ;
case 0x20 & 0x38: { ;
/* instruction 'verr' ['0f', '00', '20'] ['OP1', 'MODRM', 'GRP', 'WORD'] */ ;
UNIMPLEMENTED(this) ;
break; } ;
case 0x18 & 0x38: { ;
/* instruction 'ltr' ['0f', '00', '18'] ['NO_OS', 'OS1', 'DIRECTION', 'OP1', 'MODRM', 'GRP', 'WORD'] */ ;
entry->operand_size = 1 ;
entry->flags = IC_DIRECTION|IC_MODRM ;
{entry->execute = exec_0f0018_ltr_0; } ;
break; } ;
case 0x10 & 0x38: { ;
/* instruction 'lldt' ['0f', '00', '10'] ['NO_OS', 'OS1', 'DIRECTION', 'OP1', 'MODRM', 'GRP', 'WORD'] */ ;
entry->operand_size = 1 ;
entry->flags = IC_DIRECTION|IC_MODRM ;
{entry->execute = exec_0f0010_lldt_0; } ;
break; } ;
case 0x08 & 0x38: { ;
/* instruction 'str' ['0f', '00', '08'] ['NO_OS', 'OS1', 'OP1', 'MODRM', 'GRP', 'WORD'] */ ;
entry->operand_size = 1 ;
entry->flags = IC_MODRM ;
{entry->execute = exec_0f0008_str_0; } ;
break; } ;
case 0x00 & 0x38: { ;
/* instruction 'sldt' ['0f', '00', '00'] ['NO_OS', 'OS1', 'OP1', 'MODRM', 'GRP', 'WORD'] */ ;
entry->operand_size = 1 ;
entry->flags = IC_MODRM ;
{entry->execute = exec_0f0000_sldt_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0x01:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x38 & 0x38: { ;
/* instruction 'invlpg' ['0f', '01', '38'] ['NO_OS', 'MEMONLY', 'SKIPMODRM', 'CPL0', 'OP1', 'MODRM', 'GRP'] */ ;
if (~entry->modrminfo & MRM_REG) { ;
{entry->execute = exec_0f0138_invlpg_0; } ;
} else  {  ;
Logging::printf("invlpg not implemented at %x - %x instr %02x%02x%02x\n", _cpu->eip, entry->modrminfo, entry->data[0], entry->data[1], entry->data[2]);  ;
UNIMPLEMENTED(this); } ;
break; } ;
case 0x30 & 0x38: { ;
/* instruction 'lmsw' ['0f', '01', '30'] ['NO_OS', 'OS1', 'DIRECTION', 'CPL0', 'OP1', 'MODRM', 'GRP', 'WORD'] */ ;
entry->operand_size = 1 ;
entry->flags = IC_DIRECTION|IC_MODRM ;
{entry->execute = exec_0f0130_lmsw_0; } ;
break; } ;
case 0x20 & 0x38: { ;
/* instruction 'smsw' ['0f', '01', '20'] ['NO_OS', 'OS1', 'OP1', 'MODRM', 'GRP', 'WORD'] */ ;
entry->operand_size = 1 ;
entry->flags = IC_MODRM ;
{entry->execute = exec_0f0120_smsw_0; } ;
break; } ;
case 0x18 & 0x38: { ;
/* instruction 'lidt' ['0f', '01', '18'] ['MEMONLY', 'DIRECTION', 'SKIPMODRM', 'OP1', 'MODRM', 'GRP'] */ ;
if (~entry->modrminfo & MRM_REG) { ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_0f0118_lidt_1; } else {entry->execute = exec_0f0118_lidt_2; } ;
} else  {  ;
Logging::printf("lidt not implemented at %x - %x instr %02x%02x%02x\n", _cpu->eip, entry->modrminfo, entry->data[0], entry->data[1], entry->data[2]);  ;
UNIMPLEMENTED(this); } ;
break; } ;
case 0x10 & 0x38: { ;
/* instruction 'lgdt' ['0f', '01', '10'] ['MEMONLY', 'DIRECTION', 'SKIPMODRM', 'OP1', 'MODRM', 'GRP'] */ ;
if (~entry->modrminfo & MRM_REG) { ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_0f0110_lgdt_1; } else {entry->execute = exec_0f0110_lgdt_2; } ;
} else  {  ;
Logging::printf("lgdt not implemented at %x - %x instr %02x%02x%02x\n", _cpu->eip, entry->modrminfo, entry->data[0], entry->data[1], entry->data[2]);  ;
UNIMPLEMENTED(this); } ;
break; } ;
case 0x08 & 0x38: { ;
/* instruction 'sidt' ['0f', '01', '08'] ['MEMONLY', 'SKIPMODRM', 'OP1', 'MODRM', 'GRP'] */ ;
if (~entry->modrminfo & MRM_REG) { ;
if (entry->operand_size == 1) {entry->execute = exec_0f0108_sidt_1; } else {entry->execute = exec_0f0108_sidt_2; } ;
} else  {  ;
Logging::printf("sidt not implemented at %x - %x instr %02x%02x%02x\n", _cpu->eip, entry->modrminfo, entry->data[0], entry->data[1], entry->data[2]);  ;
UNIMPLEMENTED(this); } ;
break; } ;
case 0x00 & 0x38: { ;
/* instruction 'sgdt' ['0f', '01', '00'] ['MEMONLY', 'SKIPMODRM', 'OP1', 'MODRM', 'GRP'] */ ;
if (~entry->modrminfo & MRM_REG) { ;
if (entry->operand_size == 1) {entry->execute = exec_0f0100_sgdt_1; } else {entry->execute = exec_0f0100_sgdt_2; } ;
} else  {  ;
Logging::printf("sgdt not implemented at %x - %x instr %02x%02x%02x\n", _cpu->eip, entry->modrminfo, entry->data[0], entry->data[1], entry->data[2]);  ;
UNIMPLEMENTED(this); } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0x02:
{
/* instruction2 'lar' ['0f', '02'] ['MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
UNIMPLEMENTED(this) ;
break; }
case 0x03:
{
/* instruction2 'lsl' ['0f', '03'] ['MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
UNIMPLEMENTED(this) ;
break; }
case 0x06:
{
/* instruction2 'clts' ['0f', '06'] ['NO_OS', 'CPL0'] */ ;
{entry->execute = exec_0f06_clts_0; } ;
break; }
case 0x08:
{
/* instruction2 'invd' ['0f', '08'] ['NO_OS', 'CPL0'] */ ;
{entry->execute = exec_0f08_invd_0; } ;
break; }
case 0x09:
{
/* instruction2 'wbinvd' ['0f', '09'] ['NO_OS', 'CPL0'] */ ;
{entry->execute = exec_0f09_wbinvd_0; } ;
break; }
case 0x0b:
{
/* instruction2 'ud2a' ['0f', '0b'] ['NO_OS'] */ ;
{entry->execute = exec_0f0b_ud2a_0; } ;
break; }
case 0x1f:
{
/* instruction2 'nopl (%eax)' ['0f', '1f'] ['NO_OS', 'SKIPMODRM', 'MODRM', 'DROP1'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
{entry->execute = exec_0f1f_nopl___eax__0; } ;
break; }
case 0x20:
{
/* instruction2 'mov %cr0,%edx' ['0f', '20'] ['MODRM', 'DROP1', 'REGONLY', 'NO_OS', 'CPL0'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
if (entry->modrminfo & MRM_REG) { ;
entry->flags = IC_MODRM ;
{entry->execute = exec_0f20_mov__cr0__edx_0; } ;
} else  {  ;
Logging::printf("mov %%cr0,%%edx not implemented at %x - %x instr %02x%02x%02x\n", _cpu->eip, entry->modrminfo, entry->data[0], entry->data[1], entry->data[2]);  ;
UNIMPLEMENTED(this); } ;
break; }
case 0x21:
{
/* instruction2 'mov %db0,%edx' ['0f', '21'] ['MODRM', 'DROP1', 'REGONLY', 'NO_OS', 'CPL0'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
if (entry->modrminfo & MRM_REG) { ;
entry->flags = IC_MODRM ;
{entry->execute = exec_0f21_mov__db0__edx_0; } ;
} else  {  ;
Logging::printf("mov %%db0,%%edx not implemented at %x - %x instr %02x%02x%02x\n", _cpu->eip, entry->modrminfo, entry->data[0], entry->data[1], entry->data[2]);  ;
UNIMPLEMENTED(this); } ;
break; }
case 0x22:
{
/* instruction2 'mov %edx,%cr0' ['0f', '22'] ['MODRM', 'DROP1', 'REGONLY', 'NO_OS', 'CPL0'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
if (entry->modrminfo & MRM_REG) { ;
entry->flags = IC_MODRM ;
{entry->execute = exec_0f22_mov__edx__cr0_0; } ;
} else  {  ;
Logging::printf("mov %%edx,%%cr0 not implemented at %x - %x instr %02x%02x%02x\n", _cpu->eip, entry->modrminfo, entry->data[0], entry->data[1], entry->data[2]);  ;
UNIMPLEMENTED(this); } ;
break; }
case 0x23:
{
/* instruction2 'mov %edx,%db0' ['0f', '23'] ['MODRM', 'DROP1', 'REGONLY', 'NO_OS', 'CPL0'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
if (entry->modrminfo & MRM_REG) { ;
entry->flags = IC_MODRM ;
{entry->execute = exec_0f23_mov__edx__db0_0; } ;
} else  {  ;
Logging::printf("mov %%edx,%%db0 not implemented at %x - %x instr %02x%02x%02x\n", _cpu->eip, entry->modrminfo, entry->data[0], entry->data[1], entry->data[2]);  ;
UNIMPLEMENTED(this); } ;
break; }
case 0x30:
{
/* instruction2 'wrmsr' ['0f', '30'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_0f30_wrmsr_1; } else {entry->execute = exec_0f30_wrmsr_2; } ;
break; }
case 0x31:
{
/* instruction2 'rdtsc' ['0f', '31'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_0f31_rdtsc_1; } else {entry->execute = exec_0f31_rdtsc_2; } ;
break; }
case 0x32:
{
/* instruction2 'rdmsr' ['0f', '32'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_0f32_rdmsr_1; } else {entry->execute = exec_0f32_rdmsr_2; } ;
break; }
case 0x33:
{
/* instruction2 'rdpmc' ['0f', '33'] [] */ ;
UNIMPLEMENTED(this) ;
break; }
case 0x34:
{
/* instruction2 'sysenter' ['0f', '34'] ['NO_OS'] */ ;
{entry->execute = exec_0f34_sysenter_0; } ;
break; }
case 0x35:
{
/* instruction2 'sysexit' ['0f', '35'] ['NO_OS'] */ ;
{entry->execute = exec_0f35_sysexit_0; } ;
break; }
case 0x37:
{
/* instruction2 'getsec' ['0f', '37'] [] */ ;
UNIMPLEMENTED(this) ;
break; }
case 0x40:
{
/* instruction2 'cmovo' ['0f', '40'] ['NO_OS', 'ASM', 'LOADFLAGS', 'OS2', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->operand_size = 2 ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_DIRECTION ;
{entry->execute = exec_0f40_cmovo_0; } ;
break; }
case 0x41:
{
/* instruction2 'cmovno' ['0f', '41'] ['NO_OS', 'ASM', 'LOADFLAGS', 'OS2', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->operand_size = 2 ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_DIRECTION ;
{entry->execute = exec_0f41_cmovno_0; } ;
break; }
case 0x42:
{
/* instruction2 'cmovb' ['0f', '42'] ['NO_OS', 'ASM', 'LOADFLAGS', 'OS2', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->operand_size = 2 ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_DIRECTION ;
{entry->execute = exec_0f42_cmovb_0; } ;
break; }
case 0x43:
{
/* instruction2 'cmovae' ['0f', '43'] ['NO_OS', 'ASM', 'LOADFLAGS', 'OS2', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->operand_size = 2 ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_DIRECTION ;
{entry->execute = exec_0f43_cmovae_0; } ;
break; }
case 0x44:
{
/* instruction2 'cmove' ['0f', '44'] ['NO_OS', 'ASM', 'LOADFLAGS', 'OS2', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->operand_size = 2 ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_DIRECTION ;
{entry->execute = exec_0f44_cmove_0; } ;
break; }
case 0x45:
{
/* instruction2 'cmovne' ['0f', '45'] ['NO_OS', 'ASM', 'LOADFLAGS', 'OS2', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->operand_size = 2 ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_DIRECTION ;
{entry->execute = exec_0f45_cmovne_0; } ;
break; }
case 0x46:
{
/* instruction2 'cmovbe' ['0f', '46'] ['NO_OS', 'ASM', 'LOADFLAGS', 'OS2', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->operand_size = 2 ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_DIRECTION ;
{entry->execute = exec_0f46_cmovbe_0; } ;
break; }
case 0x47:
{
/* instruction2 'cmova' ['0f', '47'] ['NO_OS', 'ASM', 'LOADFLAGS', 'OS2', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->operand_size = 2 ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_DIRECTION ;
{entry->execute = exec_0f47_cmova_0; } ;
break; }
case 0x48:
{
/* instruction2 'cmovs' ['0f', '48'] ['NO_OS', 'ASM', 'LOADFLAGS', 'OS2', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->operand_size = 2 ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_DIRECTION ;
{entry->execute = exec_0f48_cmovs_0; } ;
break; }
case 0x49:
{
/* instruction2 'cmovns' ['0f', '49'] ['NO_OS', 'ASM', 'LOADFLAGS', 'OS2', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->operand_size = 2 ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_DIRECTION ;
{entry->execute = exec_0f49_cmovns_0; } ;
break; }
case 0x4a:
{
/* instruction2 'cmovp' ['0f', '4a'] ['NO_OS', 'ASM', 'LOADFLAGS', 'OS2', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->operand_size = 2 ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_DIRECTION ;
{entry->execute = exec_0f4a_cmovp_0; } ;
break; }
case 0x4b:
{
/* instruction2 'cmovnp' ['0f', '4b'] ['NO_OS', 'ASM', 'LOADFLAGS', 'OS2', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->operand_size = 2 ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_DIRECTION ;
{entry->execute = exec_0f4b_cmovnp_0; } ;
break; }
case 0x4c:
{
/* instruction2 'cmovl' ['0f', '4c'] ['NO_OS', 'ASM', 'LOADFLAGS', 'OS2', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->operand_size = 2 ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_DIRECTION ;
{entry->execute = exec_0f4c_cmovl_0; } ;
break; }
case 0x4d:
{
/* instruction2 'cmovge' ['0f', '4d'] ['NO_OS', 'ASM', 'LOADFLAGS', 'OS2', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->operand_size = 2 ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_DIRECTION ;
{entry->execute = exec_0f4d_cmovge_0; } ;
break; }
case 0x4e:
{
/* instruction2 'cmovle' ['0f', '4e'] ['NO_OS', 'ASM', 'LOADFLAGS', 'OS2', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->operand_size = 2 ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_DIRECTION ;
{entry->execute = exec_0f4e_cmovle_0; } ;
break; }
case 0x4f:
{
/* instruction2 'cmovg' ['0f', '4f'] ['NO_OS', 'ASM', 'LOADFLAGS', 'OS2', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->operand_size = 2 ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_DIRECTION ;
{entry->execute = exec_0f4f_cmovg_0; } ;
break; }
case 0x78:
{
/* instruction2 'vmread' ['0f', '78'] ['MODRMMEM', 'MODRM'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
UNIMPLEMENTED(this) ;
break; }
case 0x79:
{
/* instruction2 'vmwrite' ['0f', '79'] ['MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
UNIMPLEMENTED(this) ;
break; }
case 0x80:
{
/* instruction2 'jo' ['0f', '80'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMMO'] */ ;
;entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_0f80_jo_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_0f80_jo_2; } ;
break; }
case 0x81:
{
/* instruction2 'jno' ['0f', '81'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMMO'] */ ;
;entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_0f81_jno_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_0f81_jno_2; } ;
break; }
case 0x82:
{
/* instruction2 'jb' ['0f', '82'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMMO'] */ ;
;entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_0f82_jb_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_0f82_jb_2; } ;
break; }
case 0x83:
{
/* instruction2 'jae' ['0f', '83'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMMO'] */ ;
;entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_0f83_jae_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_0f83_jae_2; } ;
break; }
case 0x84:
{
/* instruction2 'je' ['0f', '84'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMMO'] */ ;
;entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_0f84_je_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_0f84_je_2; } ;
break; }
case 0x85:
{
/* instruction2 'jne' ['0f', '85'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMMO'] */ ;
;entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_0f85_jne_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_0f85_jne_2; } ;
break; }
case 0x86:
{
/* instruction2 'jbe' ['0f', '86'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMMO'] */ ;
;entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_0f86_jbe_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_0f86_jbe_2; } ;
break; }
case 0x87:
{
/* instruction2 'ja' ['0f', '87'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMMO'] */ ;
;entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_0f87_ja_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_0f87_ja_2; } ;
break; }
case 0x88:
{
/* instruction2 'js' ['0f', '88'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMMO'] */ ;
;entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_0f88_js_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_0f88_js_2; } ;
break; }
case 0x89:
{
/* instruction2 'jns' ['0f', '89'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMMO'] */ ;
;entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_0f89_jns_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_0f89_jns_2; } ;
break; }
case 0x8a:
{
/* instruction2 'jp' ['0f', '8a'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMMO'] */ ;
;entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_0f8a_jp_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_0f8a_jp_2; } ;
break; }
case 0x8b:
{
/* instruction2 'jnp' ['0f', '8b'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMMO'] */ ;
;entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_0f8b_jnp_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_0f8b_jnp_2; } ;
break; }
case 0x8c:
{
/* instruction2 'jl' ['0f', '8c'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMMO'] */ ;
;entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_0f8c_jl_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_0f8c_jl_2; } ;
break; }
case 0x8d:
{
/* instruction2 'jge' ['0f', '8d'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMMO'] */ ;
;entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_0f8d_jge_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_0f8d_jge_2; } ;
break; }
case 0x8e:
{
/* instruction2 'jle' ['0f', '8e'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMMO'] */ ;
;entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_0f8e_jle_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_0f8e_jle_2; } ;
break; }
case 0x8f:
{
/* instruction2 'jg' ['0f', '8f'] ['JMP', 'ASM', 'LOADFLAGS', 'DIRECTION', 'IMMO'] */ ;
;entry->dst = &entry->immediate ;
entry->flags = IC_ASM|IC_LOADFLAGS|IC_DIRECTION ;
if (entry->operand_size == 1) {fetch_code(entry, 2);entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);entry->execute = exec_0f8f_jg_1; } else {fetch_code(entry, 4);entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);entry->execute = exec_0f8f_jg_2; } ;
break; }
case 0x90:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x00 & 0x38: { ;
/* instruction 'seto' ['0f', '90', '00'] ['BYTE', 'ASM', 'LOADFLAGS', 'OP1', 'MODRM', 'GRP', 'BYTE'] */ ;
entry->flags = IC_BYTE|IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_BYTE ;
{entry->execute = exec_0f9000_seto_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0x91:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x00 & 0x38: { ;
/* instruction 'setno' ['0f', '91', '00'] ['BYTE', 'ASM', 'LOADFLAGS', 'OP1', 'MODRM', 'GRP', 'BYTE'] */ ;
entry->flags = IC_BYTE|IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_BYTE ;
{entry->execute = exec_0f9100_setno_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0x92:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x00 & 0x38: { ;
/* instruction 'setb' ['0f', '92', '00'] ['BYTE', 'ASM', 'LOADFLAGS', 'OP1', 'MODRM', 'GRP', 'BYTE'] */ ;
entry->flags = IC_BYTE|IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_BYTE ;
{entry->execute = exec_0f9200_setb_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0x93:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x00 & 0x38: { ;
/* instruction 'setae' ['0f', '93', '00'] ['BYTE', 'ASM', 'LOADFLAGS', 'OP1', 'MODRM', 'GRP', 'BYTE'] */ ;
entry->flags = IC_BYTE|IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_BYTE ;
{entry->execute = exec_0f9300_setae_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0x94:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x00 & 0x38: { ;
/* instruction 'sete' ['0f', '94', '00'] ['BYTE', 'ASM', 'LOADFLAGS', 'OP1', 'MODRM', 'GRP', 'BYTE'] */ ;
entry->flags = IC_BYTE|IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_BYTE ;
{entry->execute = exec_0f9400_sete_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0x95:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x00 & 0x38: { ;
/* instruction 'setne' ['0f', '95', '00'] ['BYTE', 'ASM', 'LOADFLAGS', 'OP1', 'MODRM', 'GRP', 'BYTE'] */ ;
entry->flags = IC_BYTE|IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_BYTE ;
{entry->execute = exec_0f9500_setne_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0x96:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x00 & 0x38: { ;
/* instruction 'setbe' ['0f', '96', '00'] ['BYTE', 'ASM', 'LOADFLAGS', 'OP1', 'MODRM', 'GRP', 'BYTE'] */ ;
entry->flags = IC_BYTE|IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_BYTE ;
{entry->execute = exec_0f9600_setbe_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0x97:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x00 & 0x38: { ;
/* instruction 'seta' ['0f', '97', '00'] ['BYTE', 'ASM', 'LOADFLAGS', 'OP1', 'MODRM', 'GRP', 'BYTE'] */ ;
entry->flags = IC_BYTE|IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_BYTE ;
{entry->execute = exec_0f9700_seta_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0x98:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x00 & 0x38: { ;
/* instruction 'sets' ['0f', '98', '00'] ['BYTE', 'ASM', 'LOADFLAGS', 'OP1', 'MODRM', 'GRP', 'BYTE'] */ ;
entry->flags = IC_BYTE|IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_BYTE ;
{entry->execute = exec_0f9800_sets_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0x99:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x00 & 0x38: { ;
/* instruction 'setns' ['0f', '99', '00'] ['BYTE', 'ASM', 'LOADFLAGS', 'OP1', 'MODRM', 'GRP', 'BYTE'] */ ;
entry->flags = IC_BYTE|IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_BYTE ;
{entry->execute = exec_0f9900_setns_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0x9a:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x00 & 0x38: { ;
/* instruction 'setp' ['0f', '9a', '00'] ['BYTE', 'ASM', 'LOADFLAGS', 'OP1', 'MODRM', 'GRP', 'BYTE'] */ ;
entry->flags = IC_BYTE|IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_BYTE ;
{entry->execute = exec_0f9a00_setp_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0x9b:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x00 & 0x38: { ;
/* instruction 'setnp' ['0f', '9b', '00'] ['BYTE', 'ASM', 'LOADFLAGS', 'OP1', 'MODRM', 'GRP', 'BYTE'] */ ;
entry->flags = IC_BYTE|IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_BYTE ;
{entry->execute = exec_0f9b00_setnp_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0x9c:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x00 & 0x38: { ;
/* instruction 'setl' ['0f', '9c', '00'] ['BYTE', 'ASM', 'LOADFLAGS', 'OP1', 'MODRM', 'GRP', 'BYTE'] */ ;
entry->flags = IC_BYTE|IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_BYTE ;
{entry->execute = exec_0f9c00_setl_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0x9d:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x00 & 0x38: { ;
/* instruction 'setge' ['0f', '9d', '00'] ['BYTE', 'ASM', 'LOADFLAGS', 'OP1', 'MODRM', 'GRP', 'BYTE'] */ ;
entry->flags = IC_BYTE|IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_BYTE ;
{entry->execute = exec_0f9d00_setge_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0x9e:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x00 & 0x38: { ;
/* instruction 'setle' ['0f', '9e', '00'] ['BYTE', 'ASM', 'LOADFLAGS', 'OP1', 'MODRM', 'GRP', 'BYTE'] */ ;
entry->flags = IC_BYTE|IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_BYTE ;
{entry->execute = exec_0f9e00_setle_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0x9f:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x00 & 0x38: { ;
/* instruction 'setg' ['0f', '9f', '00'] ['BYTE', 'ASM', 'LOADFLAGS', 'OP1', 'MODRM', 'GRP', 'BYTE'] */ ;
entry->flags = IC_BYTE|IC_ASM|IC_LOADFLAGS|IC_MODRM|IC_BYTE ;
{entry->execute = exec_0f9f00_setg_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0xa0:
{
/* instruction2 'push %fs' ['0f', 'a0'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_0fa0_push__fs_1; } else {entry->execute = exec_0fa0_push__fs_2; } ;
break; }
case 0xa1:
{
/* instruction2 'pop %fs' ['0f', 'a1'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_0fa1_pop__fs_1; } else {entry->execute = exec_0fa1_pop__fs_2; } ;
break; }
case 0xa2:
{
/* instruction2 'cpuid' ['0f', 'a2'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_0fa2_cpuid_1; } else {entry->execute = exec_0fa2_cpuid_2; } ;
break; }
case 0xa3:
{
/* instruction2 'bt' ['0f', 'a3'] ['READONLY', 'SAVEFLAGS', 'BITS', 'ASM', 'MODRMMEM', 'MODRM'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_READONLY|IC_SAVEFLAGS|IC_BITS|IC_ASM|IC_MODRM ;
if (entry->operand_size == 1) {entry->execute = exec_0fa3_bt_1; } else {entry->execute = exec_0fa3_bt_2; } ;
break; }
case 0xa4:
{
/* instruction2 'shld' ['0f', 'a4'] ['RMW', 'MODRMMEM', 'MODRM', 'IMM1', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_0fa4_shld_1; } else {entry->execute = exec_0fa4_shld_2; } ;
break; }
case 0xa5:
{
/* instruction2 'shld' ['0f', 'a5'] ['RMW', 'MODRMMEM', 'MODRM', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_0fa5_shld_1; } else {entry->execute = exec_0fa5_shld_2; } ;
break; }
case 0xa8:
{
/* instruction2 'push %gs' ['0f', 'a8'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_0fa8_push__gs_1; } else {entry->execute = exec_0fa8_push__gs_2; } ;
break; }
case 0xa9:
{
/* instruction2 'pop %gs' ['0f', 'a9'] [] */ ;
if (entry->operand_size == 1) {entry->execute = exec_0fa9_pop__gs_1; } else {entry->execute = exec_0fa9_pop__gs_2; } ;
break; }
case 0xaa:
{
/* instruction2 'rsm' ['0f', 'aa'] [] */ ;
UNIMPLEMENTED(this) ;
break; }
case 0xab:
{
/* instruction2 'bts' ['0f', 'ab'] ['RMW', 'SAVEFLAGS', 'BITS', 'ASM', 'MODRMMEM', 'MODRM', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_RMW|IC_SAVEFLAGS|IC_BITS|IC_ASM|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_0fab_bts_1; } else {entry->execute = exec_0fab_bts_2; } ;
break; }
case 0xac:
{
/* instruction2 'shrd' ['0f', 'ac'] ['RMW', 'MODRMMEM', 'MODRM', 'IMM1', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_0fac_shrd_1; } else {entry->execute = exec_0fac_shrd_2; } ;
break; }
case 0xad:
{
/* instruction2 'shrd' ['0f', 'ad'] ['RMW', 'MODRMMEM', 'MODRM', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_0fad_shrd_1; } else {entry->execute = exec_0fad_shrd_2; } ;
break; }
case 0xae:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x00 & 0x38: { ;
/* instruction 'fxsave' ['0f', 'ae', '00'] ['SKIPMODRM', 'NO_OS', 'OP1', 'MODRM', 'GRP'] */ ;
{entry->execute = exec_0fae00_fxsave_0; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0xaf:
{
/* instruction2 'imul' ['0f', 'af'] ['DIRECTION', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_DIRECTION|IC_MODRM|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_0faf_imul_1; } else {entry->execute = exec_0faf_imul_2; } ;
break; }
case 0xb0:
{
/* instruction2 'cmpxchg' ['0f', 'b0'] ['RMW', 'MODRMMEM', 'MODRM', 'BYTE', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_RMW|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_0fb0_cmpxchg_0; } ;
break; }
case 0xb1:
{
/* instruction2 'cmpxchg' ['0f', 'b1'] ['RMW', 'MODRMMEM', 'MODRM', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_RMW|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_0fb1_cmpxchg_1; } else {entry->execute = exec_0fb1_cmpxchg_2; } ;
break; }
case 0xb2:
{
/* instruction2 'lss' ['0f', 'b2'] ['SKIPMODRM', 'MODRM', 'MEMONLY', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
if (~entry->modrminfo & MRM_REG) { ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_0fb2_lss_1; } else {entry->execute = exec_0fb2_lss_2; } ;
} else  {  ;
Logging::printf("lss not implemented at %x - %x instr %02x%02x%02x\n", _cpu->eip, entry->modrminfo, entry->data[0], entry->data[1], entry->data[2]);  ;
UNIMPLEMENTED(this); } ;
break; }
case 0xb3:
{
/* instruction2 'btr' ['0f', 'b3'] ['RMW', 'SAVEFLAGS', 'BITS', 'ASM', 'MODRMMEM', 'MODRM', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_RMW|IC_SAVEFLAGS|IC_BITS|IC_ASM|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_0fb3_btr_1; } else {entry->execute = exec_0fb3_btr_2; } ;
break; }
case 0xb4:
{
/* instruction2 'lfs' ['0f', 'b4'] ['SKIPMODRM', 'MODRM', 'MEMONLY', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
if (~entry->modrminfo & MRM_REG) { ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_0fb4_lfs_1; } else {entry->execute = exec_0fb4_lfs_2; } ;
} else  {  ;
Logging::printf("lfs not implemented at %x - %x instr %02x%02x%02x\n", _cpu->eip, entry->modrminfo, entry->data[0], entry->data[1], entry->data[2]);  ;
UNIMPLEMENTED(this); } ;
break; }
case 0xb5:
{
/* instruction2 'lgs' ['0f', 'b5'] ['SKIPMODRM', 'MODRM', 'MEMONLY', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
if (~entry->modrminfo & MRM_REG) { ;
entry->flags = IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_0fb5_lgs_1; } else {entry->execute = exec_0fb5_lgs_2; } ;
} else  {  ;
Logging::printf("lgs not implemented at %x - %x instr %02x%02x%02x\n", _cpu->eip, entry->modrminfo, entry->data[0], entry->data[1], entry->data[2]);  ;
UNIMPLEMENTED(this); } ;
break; }
case 0xb6:
{
/* instruction2 'movzxb' ['0f', 'b6'] ['BYTE', 'HAS_OS', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_BYTE|IC_MODRM|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_0fb6_movzxb_1; } else {entry->execute = exec_0fb6_movzxb_2; } ;
break; }
case 0xb7:
{
/* instruction2 'movzxw' ['0f', 'b7'] ['HAS_OS', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_MODRM|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_0fb7_movzxw_1; } else {entry->execute = exec_0fb7_movzxw_2; } ;
break; }
case 0xba:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0xfa & 0x38: { ;
/* instruction 'btc' ['0f', 'ba', 'fa'] ['RMW', 'SAVEFLAGS', 'BITS', 'ASM', 'IMM1', 'MODRM', 'GRP', 'LOCK'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_RMW|IC_SAVEFLAGS|IC_ASM|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_0fbafa_btc_1; } else {entry->execute = exec_0fbafa_btc_2; } ;
break; } ;
case 0xf2 & 0x38: { ;
/* instruction 'btr' ['0f', 'ba', 'f2'] ['RMW', 'SAVEFLAGS', 'BITS', 'ASM', 'IMM1', 'MODRM', 'GRP', 'LOCK'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_RMW|IC_SAVEFLAGS|IC_ASM|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_0fbaf2_btr_1; } else {entry->execute = exec_0fbaf2_btr_2; } ;
break; } ;
case 0xea & 0x38: { ;
/* instruction 'bts' ['0f', 'ba', 'ea'] ['RMW', 'SAVEFLAGS', 'BITS', 'ASM', 'IMM1', 'MODRM', 'GRP', 'LOCK'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_RMW|IC_SAVEFLAGS|IC_ASM|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_0fbaea_bts_1; } else {entry->execute = exec_0fbaea_bts_2; } ;
break; } ;
case 0xe2 & 0x38: { ;
/* instruction 'bt' ['0f', 'ba', 'e2'] ['READONLY', 'SAVEFLAGS', 'BITS', 'ASM', 'IMM1', 'MODRM', 'GRP'] */ ;
fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1);entry->src = &entry->immediate ;
entry->flags = IC_READONLY|IC_SAVEFLAGS|IC_ASM|IC_MODRM ;
if (entry->operand_size == 1) {entry->execute = exec_0fbae2_bt_1; } else {entry->execute = exec_0fbae2_bt_2; } ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0xbb:
{
/* instruction2 'btc' ['0f', 'bb'] ['RMW', 'SAVEFLAGS', 'BITS', 'ASM', 'MODRMMEM', 'MODRM', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_RMW|IC_SAVEFLAGS|IC_BITS|IC_ASM|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_0fbb_btc_1; } else {entry->execute = exec_0fbb_btc_2; } ;
break; }
case 0xbc:
{
/* instruction2 'bsf' ['0f', 'bc'] ['ASM', 'SAVEFLAGS', 'DIRECTION', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_DIRECTION|IC_MODRM|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_0fbc_bsf_1; } else {entry->execute = exec_0fbc_bsf_2; } ;
break; }
case 0xbd:
{
/* instruction2 'bsr' ['0f', 'bd'] ['ASM', 'SAVEFLAGS', 'DIRECTION', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_ASM|IC_SAVEFLAGS|IC_DIRECTION|IC_MODRM|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_0fbd_bsr_1; } else {entry->execute = exec_0fbd_bsr_2; } ;
break; }
case 0xbe:
{
/* instruction2 'movsxb' ['0f', 'be'] ['BYTE', 'HAS_OS', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_BYTE|IC_MODRM|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_0fbe_movsxb_1; } else {entry->execute = exec_0fbe_movsxb_2; } ;
break; }
case 0xbf:
{
/* instruction2 'movsxw' ['0f', 'bf'] ['HAS_OS', 'MODRMMEM', 'MODRM', 'DIRECTION'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_MODRM|IC_DIRECTION ;
if (entry->operand_size == 1) {entry->execute = exec_0fbf_movsxw_1; } else {entry->execute = exec_0fbf_movsxw_2; } ;
break; }
case 0xc0:
{
/* instruction2 'xadd' ['0f', 'c0'] ['RMW', 'ASM', 'SAVEFLAGS', 'MODRMMEM', 'MODRM', 'BYTE', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<1>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_RMW|IC_ASM|IC_SAVEFLAGS|IC_MODRM|IC_BYTE|IC_LOCK ;
{entry->execute = exec_0fc0_xadd_0; } ;
break; }
case 0xc1:
{
/* instruction2 'xadd' ['0f', 'c1'] ['RMW', 'ASM', 'SAVEFLAGS', 'MODRMMEM', 'MODRM', 'LOCK'] */ ;
get_modrm() ;
entry->src = get_reg<0>((entry->data[entry->offset_opcode] >> 3) & 0x7) ;
entry->flags = IC_RMW|IC_ASM|IC_SAVEFLAGS|IC_MODRM|IC_LOCK ;
if (entry->operand_size == 1) {entry->execute = exec_0fc1_xadd_1; } else {entry->execute = exec_0fc1_xadd_2; } ;
break; }
case 0xc7:
{
get_modrm() ;
switch (entry->data[entry->offset_opcode] & 0x38) { ;
case 0x38 & 0x38: { ;
/* instruction 'vmptrst' ['0f', 'c7', '38'] ['OP1', 'MODRM', 'GRP'] */ ;
UNIMPLEMENTED(this) ;
break; } ;
case 0x30 & 0x38: { ;
/* instruction 'vmptrld' ['0f', 'c7', '30'] ['OP1', 'MODRM', 'GRP'] */ ;
UNIMPLEMENTED(this) ;
break; } ;
case 0x08 & 0x38: { ;
/* instruction 'cmpxchg8b' ['0f', 'c7', '08'] ['RMW', 'OP1', 'MODRM', 'GRP', 'LOCK'] */ ;
UNIMPLEMENTED(this) ;
break; } ;
default: ;
Logging::printf("unimpl GRP case %02x%02x%02x at %d\n", entry->data[0], entry->data[1], entry->data[2], __LINE__) ;
UNIMPLEMENTED(this) ;
} ;
break; }
case 0xc8 ... 0xcf:
{
/* instruction2 'bswap' ['0f', 'c9'] ['NO_OS', 'ASM', 'IMPL'] */ ;
entry->dst = get_reg<0>(entry->data[entry->offset_opcode-1] & 0x7) ;
entry->flags = IC_ASM ;
{entry->execute = exec_0fc9_bswap_0; } ;
break; }
default:
	      fetch_code(entry, 4);
	      Logging::printf("unimplemented case %x at line %d code %02x%02x%02x\n", code, __LINE__, entry->data[0], entry->data[1], entry->data[2]);
	      UNIMPLEMENTED(this);
	  }
	}
	break;
case 0x2:
	{
	  switch(code) {
case 0xc1:
{
/* instruction2 'vmcall' ['0f', '01', 'c1'] [] */ ;
UNIMPLEMENTED(this) ;
break; }
case 0xc2:
{
/* instruction2 'vmlaunch' ['0f', '01', 'c2'] [] */ ;
UNIMPLEMENTED(this) ;
break; }
case 0xc3:
{
/* instruction2 'vmresume' ['0f', '01', 'c3'] [] */ ;
UNIMPLEMENTED(this) ;
break; }
case 0xc4:
{
/* instruction2 'vmxoff' ['0f', '01', 'c4'] [] */ ;
UNIMPLEMENTED(this) ;
break; }
default:
	      fetch_code(entry, 4);
	      Logging::printf("unimplemented case %x at line %d code %02x%02x%02x\n", code, __LINE__, entry->data[0], entry->data[1], entry->data[2]);
	      UNIMPLEMENTED(this);
	  }
	}
	break;
case 0x3:
	{
	  switch(code) {
case 0xe3:
{
/* instruction2 'fninit' ['db', 'e3'] ['FPU', 'FPUNORESTORE', 'NO_OS'] */ ;
{entry->execute = exec_dbe3_fninit_0; } ;
break; }
case 0xe4:
{
/* instruction2 '.byte 0xdb, 0xe4 ' ['db', 'e4'] ['NO_OS', 'COMPLETE'] */ ;
{entry->execute = exec_dbe4__byte_0xdb__0xe4__0; } ;
break; }
default:
	      fetch_code(entry, 4);
	      Logging::printf("unimplemented case %x at line %d code %02x%02x%02x\n", code, __LINE__, entry->data[0], entry->data[1], entry->data[2]);
	      UNIMPLEMENTED(this);
	  }
	}
	break;
case 0x4:
	{
	  switch(code) {
case 0xe0:
{
/* instruction2 'fnstsw %ax' ['df', 'e0'] ['FPU', 'NO_OS', 'EAX'] */ ;
entry->dst = &_cpu->eax ;
{entry->execute = exec_dfe0_fnstsw__ax_0; } ;
break; }
default:
	      fetch_code(entry, 4);
	      Logging::printf("unimplemented case %x at line %d code %02x%02x%02x\n", code, __LINE__, entry->data[0], entry->data[1], entry->data[2]);
	      UNIMPLEMENTED(this);
	  }
	}
	break;
default:  assert(0); }
return _fault; }
