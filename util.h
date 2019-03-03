/*
    XCom EW Saved Game Reader
    Copyright(C) 2015

    This program is free software; you can redistribute it and / or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110 - 1301 USA.
*/
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
    // The magic number that occurs at the begining of each compressed chunk.
    static const int UPK_Magic = 0x9e2a83c1;

    namespace util
    {
        unsigned int crc32b(const unsigned char *message, size_t len);

        std::string iso8859_1_to_utf8(const std::string& in);
        std::string utf8_to_iso8859_1(const std::string& in);

        std::u16string utf8_to_utf16(const std::string& in);
        std::string utf16_to_utf8(const std::u16string& in);

        std::string to_hex(const unsigned char *data, size_t dataLen);
        std::unique_ptr<unsigned char[]> from_hex(const std::string& str);
    }

    std::string build_actor_name(const std::string& package, const std::string& cls, int instance);
    std::string build_actor_name_EU(const std::string& cls, int instance);
    std::tuple<std::string, std::string, int> decompose_actor_name(const std::string& actorName);
    std::tuple<std::string, int> decompose_actor_name_EU(const std::string& actorName);
    std::string property_kind_to_string(property::kind_t kind);
}
#endif // UTIL_H
