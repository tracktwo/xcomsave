#ifndef UTIL_H
#define UTIL_H

#include <memory>

#ifdef _DEBUG
#define DBG(msg, ...) fprintf(stderr, msg, __VA_ARGS__)
#else
#define DBG(msg, ...)
#endif

namespace xcom
{
	namespace util
	{
		unsigned int crc32b(const unsigned char *message, long len);

		std::string iso8859_1_to_utf8(const std::string& in);
		std::string utf8_to_iso8859_1(const std::string& in);

		std::string to_hex(const unsigned char *data, size_t dataLen);
		std::unique_ptr<unsigned char[]> from_hex(const std::string& str);
	}

	std::string build_actor_name(const std::string& package, const std::string& cls, int instance);
	std::tuple<std::string, std::string, int> decompose_actor_name(const std::string& actorName);
	std::string property_kind_to_string(property::kind_t kind);
}
#endif // UTIL_H
