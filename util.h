#ifndef UTIL_H
#define UTIL_H

#ifdef _DEBUG
#define DBG(msg, ...) printf(msg, __VA_ARGS__)
#else
#define DBG(msg, ...)
#endif

#endif // UTIL_H
