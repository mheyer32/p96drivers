#ifndef AT3DCONFIG_H
#define AT3DCONFIG_H

// AT3D configuration
// AT3D doesn't have MMIO_ONLY, nor Packed MMIO
// BIGENDIAN_MMIO and BIGENDIAN_IO will both be 0

#define OPENPCI         1

#define MMIO_ONLY       0
#define BIGENDIAN_MMIO  0
#define BIGENDIAN_IO    0
#define HAS_PACKED_MMIO 0


#endif  // AT3DCONFIG_H

