#ifndef MACH64CONFIG_H
#define MACH64CONFIG_H

#define OPENPCI 1

/*
 * Two chip builds:
 *   CONFIG_ATIMACH64_GX — GX/CT: poll FIFO_STAT in waitFifo
 *   CONFIG_ATIMACH64_VT — VT, GT, GM: waitFifo is a no-op (BUS_PCI_RETRY_EN)
 * Card / TestMach64Card use CONFIG_ATIMACH64 (both families; opens the matching chip).
 */
#if defined(CONFIG_ATIMACH64_GX)
#define MACH64_PCI_RETRY 0
#elif defined(CONFIG_ATIMACH64_VT)
#define MACH64_PCI_RETRY 1
#elif defined(CONFIG_ATIMACH64)
#define MACH64_PCI_RETRY 1
#else
#error "Define CONFIG_ATIMACH64_GX, CONFIG_ATIMACH64_VT, or CONFIG_ATIMACH64"
#endif

#endif  // MACH64CONFIG_H
