#ifndef BYTESWAP_H
#define BYTESWAP_H

#include <machine/endian.h>

#ifndef bswap_16
#define bswap_16(x) __bswap16(x)
#endif

#ifndef bswap_32
#define bswap_32(x) __bswap32(x)
#endif

#ifndef bswap_64
#define bswap_64(x) __bswap64(x)
#endif

#if _BYTE_ORDER == _LITTLE_ENDIAN

#define htobe16(x) __bswap16(x)
#define htole16(x) (x)
#define be16toh(x) __bswap16(x)
#define le16toh(x) (x)

#define htobe32(x) __bswap32(x)
#define htole32(x) (x)
#define be32toh(x) __bswap32(x)
#define le32toh(x) (x)

#define htobe64(x) __bswap64(x)
#define htole64(x) (x)
#define be64toh(x) __bswap64(x)
#define le64toh(x) (x)

#else

#define htobe16(x) (x)
#define htole16(x) __bswap16(x)
#define be16toh(x) (x)
#define le16toh(x) __bswap16(x)

#define htobe32(x) (x)
#define htole32(x) __bswap32(x)
#define be32toh(x) (x)
#define le32toh(x) __bswap32(x)

#define htobe64(x) (x)
#define htole64(x) __bswap64(x)
#define be64toh(x) (x)
#define le64toh(x) __bswap64(x)

#endif

#endif // BYTESWAP_H
