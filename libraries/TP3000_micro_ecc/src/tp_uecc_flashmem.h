/* TP-3000 placement extension; upstream algorithm remains unchanged. */
#ifndef TP_UECC_FLASHMEM
  #if defined(__IMXRT1062__)
    #define TP_UECC_FLASHMEM __attribute__((section(".flashmem")))
  #else
    #define TP_UECC_FLASHMEM
  #endif
#endif
