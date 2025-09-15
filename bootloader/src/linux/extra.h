#ifndef _EXTRA_H
#define _EXTRA_H

#define __force
#ifndef __aligned
#define __aligned(...)
#endif
#define WARN_ONCE(x, ...) ({x;})
#define WARN_ON_ONCE(x, ...) (x)
#define likely(x) (x)
#define unlikely(x) (x)
#define min_t(type, x, y) ((type)(x) < (type)(y) ? (type)(x) : (type)(y))
#define clamp(val, min, max) ((val) < (min) ? (min) : (val) > (max) ? (max) : (val))

#endif /* _EXTRA_H */
