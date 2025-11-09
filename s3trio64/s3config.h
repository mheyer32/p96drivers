#ifndef S3CONFIG_H
#define S3CONFIG_H

#if defined(CONFIG_VISION864)

// Vision864 cannot read from MMIO, just write
// This define indicates multiple things, like external RAMDAC; should probably find a different name
#define BUILD_VISION864 1

#define MMIO_ONLY       0
#define BIGENDIAN_MMIO  0
#define BIGENDIAN_IO    0
#define HAS_PACKED_MMIO 0

#elif defined(CONFIG_S3TRIO3264)

#define MMIO_ONLY       0
#define BIGENDIAN_MMIO  0
#define BIGENDIAN_IO    0
#define HAS_PACKED_MMIO 1
#define BUILD_VISION864 0

#elif defined(CONFIG_S3TRIO64PLUS)

#define MMIO_ONLY       0
#define BIGENDIAN_MMIO  1
#define BIGENDIAN_IO    (BIGENDIAN_MMIO && MMIO_ONLY)
#define HAS_PACKED_MMIO 1
#define BUILD_VISION864 0

#elif defined(CONFIG_S3TRIO64V2)

#define MMIO_ONLY       1
#define BIGENDIAN_MMIO  1
#define BIGENDIAN_IO    (BIGENDIAN_MMIO && MMIO_ONLY)
#define HAS_PACKED_MMIO 1
#define BUILD_VISION864 0

#else
#pragma GCC error "no CONFIG_xxx defined"
#endif

#endif  // S3CONFIG_H
