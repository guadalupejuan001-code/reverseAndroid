  else if ((instruction&0xffffffe0u)==0xd50b7520u && machine_.enabled && !machine_.el) {
    // IC IVAU, Xt: the same coherent instruction-cache operation already
    // supported at EL1. Only this exact SYS encoding is admitted at EL0.
    // Preserve the architectural UCI trap, including the complete SYS ISS.
    if(!(machine_.sctlr&(UINT64_C(1)<<26)))
      throw MachineFault{guest_sys64_syndrome((instruction>>5)&65535,rd,false),0};
    // This CPU has coherent synchronous memory and no persistent I-cache.
    // Like the original EL1 model, the operation performs no data access or
    // address-translation probe (IC IVAU permission faults are optional).
    // Flush only the host's derived fetch hints; no XI/Ether bytes change.
    clear_code_cache();
  }
