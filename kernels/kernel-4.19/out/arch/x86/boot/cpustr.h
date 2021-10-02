static const char x86_cap_strs[] =
#if REQUIRED_MASK0 & (1 << 0)
	"\x00\x00""fpu\0"
#endif
#if REQUIRED_MASK0 & (1 << 1)
	"\x00\x01""vme\0"
#endif
#if REQUIRED_MASK0 & (1 << 2)
	"\x00\x02""de\0"
#endif
#if REQUIRED_MASK0 & (1 << 3)
	"\x00\x03""pse\0"
#endif
#if REQUIRED_MASK0 & (1 << 4)
	"\x00\x04""tsc\0"
#endif
#if REQUIRED_MASK0 & (1 << 5)
	"\x00\x05""msr\0"
#endif
#if REQUIRED_MASK0 & (1 << 6)
	"\x00\x06""pae\0"
#endif
#if REQUIRED_MASK0 & (1 << 7)
	"\x00\x07""mce\0"
#endif
#if REQUIRED_MASK0 & (1 << 8)
	"\x00\x08""cx8\0"
#endif
#if REQUIRED_MASK0 & (1 << 9)
	"\x00\x09""apic\0"
#endif
#if REQUIRED_MASK0 & (1 << 11)
	"\x00\x0b""sep\0"
#endif
#if REQUIRED_MASK0 & (1 << 12)
	"\x00\x0c""mtrr\0"
#endif
#if REQUIRED_MASK0 & (1 << 13)
	"\x00\x0d""pge\0"
#endif
#if REQUIRED_MASK0 & (1 << 14)
	"\x00\x0e""mca\0"
#endif
#if REQUIRED_MASK0 & (1 << 15)
	"\x00\x0f""cmov\0"
#endif
#if REQUIRED_MASK0 & (1 << 16)
	"\x00\x10""pat\0"
#endif
#if REQUIRED_MASK0 & (1 << 17)
	"\x00\x11""pse36\0"
#endif
#if REQUIRED_MASK0 & (1 << 18)
	"\x00\x12""pn\0"
#endif
#if REQUIRED_MASK0 & (1 << 19)
	"\x00\x13""clflush\0"
#endif
#if REQUIRED_MASK0 & (1 << 21)
	"\x00\x15""dts\0"
#endif
#if REQUIRED_MASK0 & (1 << 22)
	"\x00\x16""acpi\0"
#endif
#if REQUIRED_MASK0 & (1 << 23)
	"\x00\x17""mmx\0"
#endif
#if REQUIRED_MASK0 & (1 << 24)
	"\x00\x18""fxsr\0"
#endif
#if REQUIRED_MASK0 & (1 << 25)
	"\x00\x19""sse\0"
#endif
#if REQUIRED_MASK0 & (1 << 26)
	"\x00\x1a""sse2\0"
#endif
#if REQUIRED_MASK0 & (1 << 27)
	"\x00\x1b""ss\0"
#endif
#if REQUIRED_MASK0 & (1 << 28)
	"\x00\x1c""ht\0"
#endif
#if REQUIRED_MASK0 & (1 << 29)
	"\x00\x1d""tm\0"
#endif
#if REQUIRED_MASK0 & (1 << 30)
	"\x00\x1e""ia64\0"
#endif
#if REQUIRED_MASK0 & (1 << 31)
	"\x00\x1f""pbe\0"
#endif
#if REQUIRED_MASK1 & (1 << 11)
	"\x01\x0b""syscall\0"
#endif
#if REQUIRED_MASK1 & (1 << 19)
	"\x01\x13""mp\0"
#endif
#if REQUIRED_MASK1 & (1 << 20)
	"\x01\x14""nx\0"
#endif
#if REQUIRED_MASK1 & (1 << 22)
	"\x01\x16""mmxext\0"
#endif
#if REQUIRED_MASK1 & (1 << 25)
	"\x01\x19""fxsr_opt\0"
#endif
#if REQUIRED_MASK1 & (1 << 26)
	"\x01\x1a""pdpe1gb\0"
#endif
#if REQUIRED_MASK1 & (1 << 27)
	"\x01\x1b""rdtscp\0"
#endif
#if REQUIRED_MASK1 & (1 << 29)
	"\x01\x1d""lm\0"
#endif
#if REQUIRED_MASK1 & (1 << 30)
	"\x01\x1e""3dnowext\0"
#endif
#if REQUIRED_MASK1 & (1 << 31)
	"\x01\x1f""3dnow\0"
#endif
#if REQUIRED_MASK2 & (1 << 0)
	"\x02\x00""recovery\0"
#endif
#if REQUIRED_MASK2 & (1 << 1)
	"\x02\x01""longrun\0"
#endif
#if REQUIRED_MASK2 & (1 << 3)
	"\x02\x03""lrti\0"
#endif
#if REQUIRED_MASK3 & (1 << 0)
	"\x03\x00""cxmmx\0"
#endif
#if REQUIRED_MASK3 & (1 << 1)
	"\x03\x01""k6_mtrr\0"
#endif
#if REQUIRED_MASK3 & (1 << 2)
	"\x03\x02""cyrix_arr\0"
#endif
#if REQUIRED_MASK3 & (1 << 3)
	"\x03\x03""centaur_mcr\0"
#endif
#if REQUIRED_MASK3 & (1 << 8)
	"\x03\x08""constant_tsc\0"
#endif
#if REQUIRED_MASK3 & (1 << 9)
	"\x03\x09""up\0"
#endif
#if REQUIRED_MASK3 & (1 << 10)
	"\x03\x0a""art\0"
#endif
#if REQUIRED_MASK3 & (1 << 11)
	"\x03\x0b""arch_perfmon\0"
#endif
#if REQUIRED_MASK3 & (1 << 12)
	"\x03\x0c""pebs\0"
#endif
#if REQUIRED_MASK3 & (1 << 13)
	"\x03\x0d""bts\0"
#endif
#if REQUIRED_MASK3 & (1 << 16)
	"\x03\x10""rep_good\0"
#endif
#if REQUIRED_MASK3 & (1 << 19)
	"\x03\x13""acc_power\0"
#endif
#if REQUIRED_MASK3 & (1 << 20)
	"\x03\x14""nopl\0"
#endif
#if REQUIRED_MASK3 & (1 << 22)
	"\x03\x16""xtopology\0"
#endif
#if REQUIRED_MASK3 & (1 << 23)
	"\x03\x17""tsc_reliable\0"
#endif
#if REQUIRED_MASK3 & (1 << 24)
	"\x03\x18""nonstop_tsc\0"
#endif
#if REQUIRED_MASK3 & (1 << 25)
	"\x03\x19""cpuid\0"
#endif
#if REQUIRED_MASK3 & (1 << 26)
	"\x03\x1a""extd_apicid\0"
#endif
#if REQUIRED_MASK3 & (1 << 27)
	"\x03\x1b""amd_dcm\0"
#endif
#if REQUIRED_MASK3 & (1 << 28)
	"\x03\x1c""aperfmperf\0"
#endif
#if REQUIRED_MASK3 & (1 << 30)
	"\x03\x1e""nonstop_tsc_s3\0"
#endif
#if REQUIRED_MASK3 & (1 << 31)
	"\x03\x1f""tsc_known_freq\0"
#endif
#if REQUIRED_MASK4 & (1 << 0)
	"\x04\x00""pni\0"
#endif
#if REQUIRED_MASK4 & (1 << 1)
	"\x04\x01""pclmulqdq\0"
#endif
#if REQUIRED_MASK4 & (1 << 2)
	"\x04\x02""dtes64\0"
#endif
#if REQUIRED_MASK4 & (1 << 3)
	"\x04\x03""monitor\0"
#endif
#if REQUIRED_MASK4 & (1 << 4)
	"\x04\x04""ds_cpl\0"
#endif
#if REQUIRED_MASK4 & (1 << 5)
	"\x04\x05""vmx\0"
#endif
#if REQUIRED_MASK4 & (1 << 6)
	"\x04\x06""smx\0"
#endif
#if REQUIRED_MASK4 & (1 << 7)
	"\x04\x07""est\0"
#endif
#if REQUIRED_MASK4 & (1 << 8)
	"\x04\x08""tm2\0"
#endif
#if REQUIRED_MASK4 & (1 << 9)
	"\x04\x09""ssse3\0"
#endif
#if REQUIRED_MASK4 & (1 << 10)
	"\x04\x0a""cid\0"
#endif
#if REQUIRED_MASK4 & (1 << 11)
	"\x04\x0b""sdbg\0"
#endif
#if REQUIRED_MASK4 & (1 << 12)
	"\x04\x0c""fma\0"
#endif
#if REQUIRED_MASK4 & (1 << 13)
	"\x04\x0d""cx16\0"
#endif
#if REQUIRED_MASK4 & (1 << 14)
	"\x04\x0e""xtpr\0"
#endif
#if REQUIRED_MASK4 & (1 << 15)
	"\x04\x0f""pdcm\0"
#endif
#if REQUIRED_MASK4 & (1 << 17)
	"\x04\x11""pcid\0"
#endif
#if REQUIRED_MASK4 & (1 << 18)
	"\x04\x12""dca\0"
#endif
#if REQUIRED_MASK4 & (1 << 19)
	"\x04\x13""sse4_1\0"
#endif
#if REQUIRED_MASK4 & (1 << 20)
	"\x04\x14""sse4_2\0"
#endif
#if REQUIRED_MASK4 & (1 << 21)
	"\x04\x15""x2apic\0"
#endif
#if REQUIRED_MASK4 & (1 << 22)
	"\x04\x16""movbe\0"
#endif
#if REQUIRED_MASK4 & (1 << 23)
	"\x04\x17""popcnt\0"
#endif
#if REQUIRED_MASK4 & (1 << 24)
	"\x04\x18""tsc_deadline_timer\0"
#endif
#if REQUIRED_MASK4 & (1 << 25)
	"\x04\x19""aes\0"
#endif
#if REQUIRED_MASK4 & (1 << 26)
	"\x04\x1a""xsave\0"
#endif
#if REQUIRED_MASK4 & (1 << 28)
	"\x04\x1c""avx\0"
#endif
#if REQUIRED_MASK4 & (1 << 29)
	"\x04\x1d""f16c\0"
#endif
#if REQUIRED_MASK4 & (1 << 30)
	"\x04\x1e""rdrand\0"
#endif
#if REQUIRED_MASK4 & (1 << 31)
	"\x04\x1f""hypervisor\0"
#endif
#if REQUIRED_MASK5 & (1 << 2)
	"\x05\x02""rng\0"
#endif
#if REQUIRED_MASK5 & (1 << 3)
	"\x05\x03""rng_en\0"
#endif
#if REQUIRED_MASK5 & (1 << 6)
	"\x05\x06""ace\0"
#endif
#if REQUIRED_MASK5 & (1 << 7)
	"\x05\x07""ace_en\0"
#endif
#if REQUIRED_MASK5 & (1 << 8)
	"\x05\x08""ace2\0"
#endif
#if REQUIRED_MASK5 & (1 << 9)
	"\x05\x09""ace2_en\0"
#endif
#if REQUIRED_MASK5 & (1 << 10)
	"\x05\x0a""phe\0"
#endif
#if REQUIRED_MASK5 & (1 << 11)
	"\x05\x0b""phe_en\0"
#endif
#if REQUIRED_MASK5 & (1 << 12)
	"\x05\x0c""pmm\0"
#endif
#if REQUIRED_MASK5 & (1 << 13)
	"\x05\x0d""pmm_en\0"
#endif
#if REQUIRED_MASK6 & (1 << 0)
	"\x06\x00""lahf_lm\0"
#endif
#if REQUIRED_MASK6 & (1 << 1)
	"\x06\x01""cmp_legacy\0"
#endif
#if REQUIRED_MASK6 & (1 << 2)
	"\x06\x02""svm\0"
#endif
#if REQUIRED_MASK6 & (1 << 3)
	"\x06\x03""extapic\0"
#endif
#if REQUIRED_MASK6 & (1 << 4)
	"\x06\x04""cr8_legacy\0"
#endif
#if REQUIRED_MASK6 & (1 << 5)
	"\x06\x05""abm\0"
#endif
#if REQUIRED_MASK6 & (1 << 6)
	"\x06\x06""sse4a\0"
#endif
#if REQUIRED_MASK6 & (1 << 7)
	"\x06\x07""misalignsse\0"
#endif
#if REQUIRED_MASK6 & (1 << 8)
	"\x06\x08""3dnowprefetch\0"
#endif
#if REQUIRED_MASK6 & (1 << 9)
	"\x06\x09""osvw\0"
#endif
#if REQUIRED_MASK6 & (1 << 10)
	"\x06\x0a""ibs\0"
#endif
#if REQUIRED_MASK6 & (1 << 11)
	"\x06\x0b""xop\0"
#endif
#if REQUIRED_MASK6 & (1 << 12)
	"\x06\x0c""skinit\0"
#endif
#if REQUIRED_MASK6 & (1 << 13)
	"\x06\x0d""wdt\0"
#endif
#if REQUIRED_MASK6 & (1 << 15)
	"\x06\x0f""lwp\0"
#endif
#if REQUIRED_MASK6 & (1 << 16)
	"\x06\x10""fma4\0"
#endif
#if REQUIRED_MASK6 & (1 << 17)
	"\x06\x11""tce\0"
#endif
#if REQUIRED_MASK6 & (1 << 19)
	"\x06\x13""nodeid_msr\0"
#endif
#if REQUIRED_MASK6 & (1 << 21)
	"\x06\x15""tbm\0"
#endif
#if REQUIRED_MASK6 & (1 << 22)
	"\x06\x16""topoext\0"
#endif
#if REQUIRED_MASK6 & (1 << 23)
	"\x06\x17""perfctr_core\0"
#endif
#if REQUIRED_MASK6 & (1 << 24)
	"\x06\x18""perfctr_nb\0"
#endif
#if REQUIRED_MASK6 & (1 << 26)
	"\x06\x1a""bpext\0"
#endif
#if REQUIRED_MASK6 & (1 << 27)
	"\x06\x1b""ptsc\0"
#endif
#if REQUIRED_MASK6 & (1 << 28)
	"\x06\x1c""perfctr_llc\0"
#endif
#if REQUIRED_MASK6 & (1 << 29)
	"\x06\x1d""mwaitx\0"
#endif
#if REQUIRED_MASK7 & (1 << 0)
	"\x07\x00""ring3mwait\0"
#endif
#if REQUIRED_MASK7 & (1 << 1)
	"\x07\x01""cpuid_fault\0"
#endif
#if REQUIRED_MASK7 & (1 << 2)
	"\x07\x02""cpb\0"
#endif
#if REQUIRED_MASK7 & (1 << 3)
	"\x07\x03""epb\0"
#endif
#if REQUIRED_MASK7 & (1 << 4)
	"\x07\x04""cat_l3\0"
#endif
#if REQUIRED_MASK7 & (1 << 5)
	"\x07\x05""cat_l2\0"
#endif
#if REQUIRED_MASK7 & (1 << 6)
	"\x07\x06""cdp_l3\0"
#endif
#if REQUIRED_MASK7 & (1 << 7)
	"\x07\x07""invpcid_single\0"
#endif
#if REQUIRED_MASK7 & (1 << 8)
	"\x07\x08""hw_pstate\0"
#endif
#if REQUIRED_MASK7 & (1 << 9)
	"\x07\x09""proc_feedback\0"
#endif
#if REQUIRED_MASK7 & (1 << 10)
	"\x07\x0a""sme\0"
#endif
#if REQUIRED_MASK7 & (1 << 11)
	"\x07\x0b""pti\0"
#endif
#if REQUIRED_MASK7 & (1 << 14)
	"\x07\x0e""intel_ppin\0"
#endif
#if REQUIRED_MASK7 & (1 << 15)
	"\x07\x0f""cdp_l2\0"
#endif
#if REQUIRED_MASK7 & (1 << 17)
	"\x07\x11""ssbd\0"
#endif
#if REQUIRED_MASK7 & (1 << 18)
	"\x07\x12""mba\0"
#endif
#if REQUIRED_MASK7 & (1 << 20)
	"\x07\x14""sev\0"
#endif
#if REQUIRED_MASK7 & (1 << 25)
	"\x07\x19""ibrs\0"
#endif
#if REQUIRED_MASK7 & (1 << 26)
	"\x07\x1a""ibpb\0"
#endif
#if REQUIRED_MASK7 & (1 << 27)
	"\x07\x1b""stibp\0"
#endif
#if REQUIRED_MASK7 & (1 << 30)
	"\x07\x1e""ibrs_enhanced\0"
#endif
#if REQUIRED_MASK8 & (1 << 0)
	"\x08\x00""tpr_shadow\0"
#endif
#if REQUIRED_MASK8 & (1 << 1)
	"\x08\x01""vnmi\0"
#endif
#if REQUIRED_MASK8 & (1 << 2)
	"\x08\x02""flexpriority\0"
#endif
#if REQUIRED_MASK8 & (1 << 3)
	"\x08\x03""ept\0"
#endif
#if REQUIRED_MASK8 & (1 << 4)
	"\x08\x04""vpid\0"
#endif
#if REQUIRED_MASK8 & (1 << 15)
	"\x08\x0f""vmmcall\0"
#endif
#if REQUIRED_MASK8 & (1 << 17)
	"\x08\x11""ept_ad\0"
#endif
#if REQUIRED_MASK9 & (1 << 0)
	"\x09\x00""fsgsbase\0"
#endif
#if REQUIRED_MASK9 & (1 << 1)
	"\x09\x01""tsc_adjust\0"
#endif
#if REQUIRED_MASK9 & (1 << 3)
	"\x09\x03""bmi1\0"
#endif
#if REQUIRED_MASK9 & (1 << 4)
	"\x09\x04""hle\0"
#endif
#if REQUIRED_MASK9 & (1 << 5)
	"\x09\x05""avx2\0"
#endif
#if REQUIRED_MASK9 & (1 << 7)
	"\x09\x07""smep\0"
#endif
#if REQUIRED_MASK9 & (1 << 8)
	"\x09\x08""bmi2\0"
#endif
#if REQUIRED_MASK9 & (1 << 9)
	"\x09\x09""erms\0"
#endif
#if REQUIRED_MASK9 & (1 << 10)
	"\x09\x0a""invpcid\0"
#endif
#if REQUIRED_MASK9 & (1 << 11)
	"\x09\x0b""rtm\0"
#endif
#if REQUIRED_MASK9 & (1 << 12)
	"\x09\x0c""cqm\0"
#endif
#if REQUIRED_MASK9 & (1 << 14)
	"\x09\x0e""mpx\0"
#endif
#if REQUIRED_MASK9 & (1 << 15)
	"\x09\x0f""rdt_a\0"
#endif
#if REQUIRED_MASK9 & (1 << 16)
	"\x09\x10""avx512f\0"
#endif
#if REQUIRED_MASK9 & (1 << 17)
	"\x09\x11""avx512dq\0"
#endif
#if REQUIRED_MASK9 & (1 << 18)
	"\x09\x12""rdseed\0"
#endif
#if REQUIRED_MASK9 & (1 << 19)
	"\x09\x13""adx\0"
#endif
#if REQUIRED_MASK9 & (1 << 20)
	"\x09\x14""smap\0"
#endif
#if REQUIRED_MASK9 & (1 << 21)
	"\x09\x15""avx512ifma\0"
#endif
#if REQUIRED_MASK9 & (1 << 23)
	"\x09\x17""clflushopt\0"
#endif
#if REQUIRED_MASK9 & (1 << 24)
	"\x09\x18""clwb\0"
#endif
#if REQUIRED_MASK9 & (1 << 25)
	"\x09\x19""intel_pt\0"
#endif
#if REQUIRED_MASK9 & (1 << 26)
	"\x09\x1a""avx512pf\0"
#endif
#if REQUIRED_MASK9 & (1 << 27)
	"\x09\x1b""avx512er\0"
#endif
#if REQUIRED_MASK9 & (1 << 28)
	"\x09\x1c""avx512cd\0"
#endif
#if REQUIRED_MASK9 & (1 << 29)
	"\x09\x1d""sha_ni\0"
#endif
#if REQUIRED_MASK9 & (1 << 30)
	"\x09\x1e""avx512bw\0"
#endif
#if REQUIRED_MASK9 & (1 << 31)
	"\x09\x1f""avx512vl\0"
#endif
#if REQUIRED_MASK10 & (1 << 0)
	"\x0a\x00""xsaveopt\0"
#endif
#if REQUIRED_MASK10 & (1 << 1)
	"\x0a\x01""xsavec\0"
#endif
#if REQUIRED_MASK10 & (1 << 2)
	"\x0a\x02""xgetbv1\0"
#endif
#if REQUIRED_MASK10 & (1 << 3)
	"\x0a\x03""xsaves\0"
#endif
#if REQUIRED_MASK11 & (1 << 0)
	"\x0b\x00""cqm_llc\0"
#endif
#if REQUIRED_MASK11 & (1 << 1)
	"\x0b\x01""cqm_occup_llc\0"
#endif
#if REQUIRED_MASK11 & (1 << 2)
	"\x0b\x02""cqm_mbm_total\0"
#endif
#if REQUIRED_MASK11 & (1 << 3)
	"\x0b\x03""cqm_mbm_local\0"
#endif
#if REQUIRED_MASK13 & (1 << 0)
	"\x0d\x00""clzero\0"
#endif
#if REQUIRED_MASK13 & (1 << 1)
	"\x0d\x01""irperf\0"
#endif
#if REQUIRED_MASK13 & (1 << 2)
	"\x0d\x02""xsaveerptr\0"
#endif
#if REQUIRED_MASK13 & (1 << 25)
	"\x0d\x19""virt_ssbd\0"
#endif
#if REQUIRED_MASK14 & (1 << 0)
	"\x0e\x00""dtherm\0"
#endif
#if REQUIRED_MASK14 & (1 << 1)
	"\x0e\x01""ida\0"
#endif
#if REQUIRED_MASK14 & (1 << 2)
	"\x0e\x02""arat\0"
#endif
#if REQUIRED_MASK14 & (1 << 4)
	"\x0e\x04""pln\0"
#endif
#if REQUIRED_MASK14 & (1 << 6)
	"\x0e\x06""pts\0"
#endif
#if REQUIRED_MASK14 & (1 << 7)
	"\x0e\x07""hwp\0"
#endif
#if REQUIRED_MASK14 & (1 << 8)
	"\x0e\x08""hwp_notify\0"
#endif
#if REQUIRED_MASK14 & (1 << 9)
	"\x0e\x09""hwp_act_window\0"
#endif
#if REQUIRED_MASK14 & (1 << 10)
	"\x0e\x0a""hwp_epp\0"
#endif
#if REQUIRED_MASK14 & (1 << 11)
	"\x0e\x0b""hwp_pkg_req\0"
#endif
#if REQUIRED_MASK15 & (1 << 0)
	"\x0f\x00""npt\0"
#endif
#if REQUIRED_MASK15 & (1 << 1)
	"\x0f\x01""lbrv\0"
#endif
#if REQUIRED_MASK15 & (1 << 2)
	"\x0f\x02""svm_lock\0"
#endif
#if REQUIRED_MASK15 & (1 << 3)
	"\x0f\x03""nrip_save\0"
#endif
#if REQUIRED_MASK15 & (1 << 4)
	"\x0f\x04""tsc_scale\0"
#endif
#if REQUIRED_MASK15 & (1 << 5)
	"\x0f\x05""vmcb_clean\0"
#endif
#if REQUIRED_MASK15 & (1 << 6)
	"\x0f\x06""flushbyasid\0"
#endif
#if REQUIRED_MASK15 & (1 << 7)
	"\x0f\x07""decodeassists\0"
#endif
#if REQUIRED_MASK15 & (1 << 10)
	"\x0f\x0a""pausefilter\0"
#endif
#if REQUIRED_MASK15 & (1 << 12)
	"\x0f\x0c""pfthreshold\0"
#endif
#if REQUIRED_MASK15 & (1 << 13)
	"\x0f\x0d""avic\0"
#endif
#if REQUIRED_MASK15 & (1 << 15)
	"\x0f\x0f""v_vmsave_vmload\0"
#endif
#if REQUIRED_MASK15 & (1 << 16)
	"\x0f\x10""vgif\0"
#endif
#if REQUIRED_MASK16 & (1 << 1)
	"\x10\x01""avx512vbmi\0"
#endif
#if REQUIRED_MASK16 & (1 << 2)
	"\x10\x02""umip\0"
#endif
#if REQUIRED_MASK16 & (1 << 3)
	"\x10\x03""pku\0"
#endif
#if REQUIRED_MASK16 & (1 << 4)
	"\x10\x04""ospke\0"
#endif
#if REQUIRED_MASK16 & (1 << 6)
	"\x10\x06""avx512_vbmi2\0"
#endif
#if REQUIRED_MASK16 & (1 << 8)
	"\x10\x08""gfni\0"
#endif
#if REQUIRED_MASK16 & (1 << 9)
	"\x10\x09""vaes\0"
#endif
#if REQUIRED_MASK16 & (1 << 10)
	"\x10\x0a""vpclmulqdq\0"
#endif
#if REQUIRED_MASK16 & (1 << 11)
	"\x10\x0b""avx512_vnni\0"
#endif
#if REQUIRED_MASK16 & (1 << 12)
	"\x10\x0c""avx512_bitalg\0"
#endif
#if REQUIRED_MASK16 & (1 << 13)
	"\x10\x0d""tme\0"
#endif
#if REQUIRED_MASK16 & (1 << 14)
	"\x10\x0e""avx512_vpopcntdq\0"
#endif
#if REQUIRED_MASK16 & (1 << 16)
	"\x10\x10""la57\0"
#endif
#if REQUIRED_MASK16 & (1 << 22)
	"\x10\x16""rdpid\0"
#endif
#if REQUIRED_MASK16 & (1 << 25)
	"\x10\x19""cldemote\0"
#endif
#if REQUIRED_MASK17 & (1 << 0)
	"\x11\x00""overflow_recov\0"
#endif
#if REQUIRED_MASK17 & (1 << 1)
	"\x11\x01""succor\0"
#endif
#if REQUIRED_MASK17 & (1 << 3)
	"\x11\x03""smca\0"
#endif
#if REQUIRED_MASK18 & (1 << 2)
	"\x12\x02""avx512_4vnniw\0"
#endif
#if REQUIRED_MASK18 & (1 << 3)
	"\x12\x03""avx512_4fmaps\0"
#endif
#if REQUIRED_MASK18 & (1 << 10)
	"\x12\x0a""md_clear\0"
#endif
#if REQUIRED_MASK18 & (1 << 18)
	"\x12\x12""pconfig\0"
#endif
#if REQUIRED_MASK18 & (1 << 28)
	"\x12\x1c""flush_l1d\0"
#endif
#if REQUIRED_MASK18 & (1 << 29)
	"\x12\x1d""arch_capabilities\0"
#endif
	"\x12\x1f"""
	;
