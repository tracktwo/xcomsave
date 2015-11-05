#ifndef UTIL_H
#define UTIL_H

#include <memory>

#ifdef _DEBUG
#define DBG(msg, ...) fprintf(stderr, msg, __VA_ARGS__)
#else
#define DBG(msg, ...)
#endif


struct Buffer
{
	std::unique_ptr<unsigned char[]> buf;
	size_t len;
};

unsigned int crc32b(const unsigned char *message, long len);

std::string toHex(const unsigned char *data, uint32_t dataLen);
std::unique_ptr<unsigned char[]> fromHex(const std::string& str);


#endif // UTIL_H
