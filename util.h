#ifndef UTIL_H
#define UTIL_H

#ifdef _DEBUG
#define DBG(msg, ...) printf(msg, __VA_ARGS__)
#else
#define DBG(msg, ...)
#endif

unsigned int crc32b(const unsigned char *message, long len);


#endif // UTIL_H
