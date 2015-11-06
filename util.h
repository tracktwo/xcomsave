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

std::string iso8859_1toutf8(const std::string& in);
std::string utf8toiso8859_1(const std::string& in);

std::string toHex(const unsigned char *data, uint32_t dataLen);
std::unique_ptr<unsigned char[]> fromHex(const std::string& str);


#endif // UTIL_H
