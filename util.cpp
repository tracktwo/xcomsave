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

#include <stdint.h>
#include <string>
#include <exception>
#include <memory>
#include <cassert>
#include <sstream>
#include <stdarg.h>

#ifdef _MSC_VER
#include <windows.h>
#else
#include <iconv.h>
#endif

#include "xcom.h"
#include "xcomio.h"

namespace xcom
{
    namespace util
    {
        // CRC Table for polynomial 0x04c11db7
        static const unsigned long crc32_table_b[256] =
        { 
            0x00000000UL, 0x04c11db7UL, 0x09823b6eUL, 0x0d4326d9UL, 0x130476dcUL,
            0x17c56b6bUL, 0x1a864db2UL, 0x1e475005UL, 0x2608edb8UL, 0x22c9f00fUL,
            0x2f8ad6d6UL, 0x2b4bcb61UL, 0x350c9b64UL, 0x31cd86d3UL, 0x3c8ea00aUL,
            0x384fbdbdUL, 0x4c11db70UL, 0x48d0c6c7UL, 0x4593e01eUL, 0x4152fda9UL,
            0x5f15adacUL, 0x5bd4b01bUL, 0x569796c2UL, 0x52568b75UL, 0x6a1936c8UL,
            0x6ed82b7fUL, 0x639b0da6UL, 0x675a1011UL, 0x791d4014UL, 0x7ddc5da3UL,
            0x709f7b7aUL, 0x745e66cdUL, 0x9823b6e0UL, 0x9ce2ab57UL, 0x91a18d8eUL,
            0x95609039UL, 0x8b27c03cUL, 0x8fe6dd8bUL, 0x82a5fb52UL, 0x8664e6e5UL,
            0xbe2b5b58UL, 0xbaea46efUL, 0xb7a96036UL, 0xb3687d81UL, 0xad2f2d84UL,
            0xa9ee3033UL, 0xa4ad16eaUL, 0xa06c0b5dUL, 0xd4326d90UL, 0xd0f37027UL,
            0xddb056feUL, 0xd9714b49UL, 0xc7361b4cUL, 0xc3f706fbUL, 0xceb42022UL,
            0xca753d95UL, 0xf23a8028UL, 0xf6fb9d9fUL, 0xfbb8bb46UL, 0xff79a6f1UL,
            0xe13ef6f4UL, 0xe5ffeb43UL, 0xe8bccd9aUL, 0xec7dd02dUL, 0x34867077UL,
            0x30476dc0UL, 0x3d044b19UL, 0x39c556aeUL, 0x278206abUL, 0x23431b1cUL,
            0x2e003dc5UL, 0x2ac12072UL, 0x128e9dcfUL, 0x164f8078UL, 0x1b0ca6a1UL,
            0x1fcdbb16UL, 0x018aeb13UL, 0x054bf6a4UL, 0x0808d07dUL, 0x0cc9cdcaUL,
            0x7897ab07UL, 0x7c56b6b0UL, 0x71159069UL, 0x75d48ddeUL, 0x6b93dddbUL,
            0x6f52c06cUL, 0x6211e6b5UL, 0x66d0fb02UL, 0x5e9f46bfUL, 0x5a5e5b08UL,
            0x571d7dd1UL, 0x53dc6066UL, 0x4d9b3063UL, 0x495a2dd4UL, 0x44190b0dUL,
            0x40d816baUL, 0xaca5c697UL, 0xa864db20UL, 0xa527fdf9UL, 0xa1e6e04eUL,
            0xbfa1b04bUL, 0xbb60adfcUL, 0xb6238b25UL, 0xb2e29692UL, 0x8aad2b2fUL,
            0x8e6c3698UL, 0x832f1041UL, 0x87ee0df6UL, 0x99a95df3UL, 0x9d684044UL,
            0x902b669dUL, 0x94ea7b2aUL, 0xe0b41de7UL, 0xe4750050UL, 0xe9362689UL,
            0xedf73b3eUL, 0xf3b06b3bUL, 0xf771768cUL, 0xfa325055UL, 0xfef34de2UL,
            0xc6bcf05fUL, 0xc27dede8UL, 0xcf3ecb31UL, 0xcbffd686UL, 0xd5b88683UL,
            0xd1799b34UL, 0xdc3abdedUL, 0xd8fba05aUL, 0x690ce0eeUL, 0x6dcdfd59UL,
            0x608edb80UL, 0x644fc637UL, 0x7a089632UL, 0x7ec98b85UL, 0x738aad5cUL,
            0x774bb0ebUL, 0x4f040d56UL, 0x4bc510e1UL, 0x46863638UL, 0x42472b8fUL,
            0x5c007b8aUL, 0x58c1663dUL, 0x558240e4UL, 0x51435d53UL, 0x251d3b9eUL,
            0x21dc2629UL, 0x2c9f00f0UL, 0x285e1d47UL, 0x36194d42UL, 0x32d850f5UL,
            0x3f9b762cUL, 0x3b5a6b9bUL, 0x0315d626UL, 0x07d4cb91UL, 0x0a97ed48UL,
            0x0e56f0ffUL, 0x1011a0faUL, 0x14d0bd4dUL, 0x19939b94UL, 0x1d528623UL,
            0xf12f560eUL, 0xf5ee4bb9UL, 0xf8ad6d60UL, 0xfc6c70d7UL, 0xe22b20d2UL,
            0xe6ea3d65UL, 0xeba91bbcUL, 0xef68060bUL, 0xd727bbb6UL, 0xd3e6a601UL,
            0xdea580d8UL, 0xda649d6fUL, 0xc423cd6aUL, 0xc0e2d0ddUL, 0xcda1f604UL,
            0xc960ebb3UL, 0xbd3e8d7eUL, 0xb9ff90c9UL, 0xb4bcb610UL, 0xb07daba7UL,
            0xae3afba2UL, 0xaafbe615UL, 0xa7b8c0ccUL, 0xa379dd7bUL, 0x9b3660c6UL,
            0x9ff77d71UL, 0x92b45ba8UL, 0x9675461fUL, 0x8832161aUL, 0x8cf30badUL,
            0x81b02d74UL, 0x857130c3UL, 0x5d8a9099UL, 0x594b8d2eUL, 0x5408abf7UL,
            0x50c9b640UL, 0x4e8ee645UL, 0x4a4ffbf2UL, 0x470cdd2bUL, 0x43cdc09cUL,
            0x7b827d21UL, 0x7f436096UL, 0x7200464fUL, 0x76c15bf8UL, 0x68860bfdUL,
            0x6c47164aUL, 0x61043093UL, 0x65c52d24UL, 0x119b4be9UL, 0x155a565eUL,
            0x18197087UL, 0x1cd86d30UL, 0x029f3d35UL, 0x065e2082UL, 0x0b1d065bUL,
            0x0fdc1becUL, 0x3793a651UL, 0x3352bbe6UL, 0x3e119d3fUL, 0x3ad08088UL,
            0x2497d08dUL, 0x2056cd3aUL, 0x2d15ebe3UL, 0x29d4f654UL, 0xc5a92679UL,
            0xc1683bceUL, 0xcc2b1d17UL, 0xc8ea00a0UL, 0xd6ad50a5UL, 0xd26c4d12UL,
            0xdf2f6bcbUL, 0xdbee767cUL, 0xe3a1cbc1UL, 0xe760d676UL, 0xea23f0afUL,
            0xeee2ed18UL, 0xf0a5bd1dUL, 0xf464a0aaUL, 0xf9278673UL, 0xfde69bc4UL,
            0x89b8fd09UL, 0x8d79e0beUL, 0x803ac667UL, 0x84fbdbd0UL, 0x9abc8bd5UL,
            0x9e7d9662UL, 0x933eb0bbUL, 0x97ffad0cUL, 0xafb010b1UL, 0xab710d06UL,
            0xa6322bdfUL, 0xa2f33668UL, 0xbcb4666dUL, 0xb8757bdaUL, 0xb5365d03UL,
            0xb1f740b4UL 
        };

        unsigned int crc32b(const unsigned char *message, size_t len)
        {
            unsigned int crc = 0xffffffff;
            for (size_t i = 0; i < len; ++i) {
                unsigned char tmp = (crc >> 24) ^ message[i];
                crc = (crc << 8) ^ (crc32_table_b[tmp]);
            }
            return ~crc;
        }

        static char to_hex_nibble(unsigned char nib)
        {
            return nib < 10 ? nib + '0' : nib - 10 + 'a';
        }

        static unsigned char from_hex_nibble(char c)
        {
            switch (c)
            {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                return c - '0';
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
                return c - 'a' + 10;
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
                return c - 'F' + 10;
            }

            std::string str = "unexpected hex character: ";
            str.append(1, c);
            throw xcom::error::general_exception(str);
        }

        std::string to_hex(const unsigned char *data, size_t length)
        {
            std::string str;
            for (size_t i = 0; i < length; ++i) {
                str += to_hex_nibble(data[i] >> 4);
                str += to_hex_nibble(data[i] & 0x0F);
            }

            return str;
        }

        std::unique_ptr<unsigned char[]> from_hex(const std::string &str)
        {
            std::unique_ptr<unsigned char[]> data = 
                std::make_unique<unsigned char[]>(str.length() / 2);
            for (size_t i = 0; i < str.length(); i += 2) {
                data[i / 2] = from_hex_nibble(str[i]) << 4;
                data[i / 2] |= from_hex_nibble(str[i + 1]);
            }

            return data;
        }

        std::string iso8859_1_to_utf8(const std::string& in)
        {
            std::string out;

            for (size_t i = 0; i < in.length(); ++i) {
                unsigned char p = static_cast<unsigned char>(in[i]);
                if (p < 0x80) {
                    out += static_cast<char>(p);
                }
                else {
                    out += static_cast<char>(0xc0 | (p >> 6));
                    out += static_cast<char>(0x80 | (p & 0x3f));
                }
            }
            return out;
        }

        std::string utf8_to_iso8859_1(const std::string& in)
        {
            std::string out;

            for (size_t i = 0; i < in.length(); ++i) {
                unsigned char p = static_cast<unsigned char>(in[i]);

                if (p < 0x80) {
                    out += static_cast<char>(p);
                }
                else {
                    assert((p & 0xc0) == 0xc0);
                    unsigned char p2 = static_cast<unsigned char>(in[++i]);
                    out += static_cast<char>((p << 6) | (p2 & 0x3f));
                }
            }

            return out;
        }

#ifdef _MSC_VER
        std::u16string utf8_to_utf16(const std::string& in)
        {
            int encoded_size = MultiByteToWideChar(CP_UTF8, 0, in.c_str(), -1, 
                    nullptr, 0);
            std::unique_ptr<char16_t[]> buf = std::make_unique<char16_t[]>(encoded_size);
            LPWSTR out_buf = reinterpret_cast<wchar_t*>(buf.get());
            MultiByteToWideChar(CP_UTF8, 0, in.c_str(), -1, out_buf, encoded_size);
            return std::u16string{ buf.get() };
        }

        std::string utf16_to_utf8(const std::u16string& in)
        {
            LPCWSTR in_buf = reinterpret_cast<const wchar_t*>(in.c_str());
            int encoded_size = WideCharToMultiByte(CP_UTF8, 0, in_buf, -1, 
                    nullptr, 0, nullptr, nullptr);
            std::unique_ptr<char[]> buf = std::make_unique<char[]>(encoded_size);
            WideCharToMultiByte(CP_UTF8, 0, in_buf, -1, buf.get(), 
                    encoded_size, nullptr, nullptr);
            return std::string{ buf.get() };
        }
#else
        std::u16string utf8_to_utf16(const std::string& in)
        {
            iconv_t cd = iconv_open("UTF-16LE", "UTF-8");
            const char *in_buf = in.c_str();
            std::size_t in_length = in.length();
            std::size_t out_length = in.length() * 2 + 1;
            std::unique_ptr<char16_t[]> out_ptr = 
                    std::make_unique<char16_t[]>(out_length);
            char *out_buf = reinterpret_cast<char *>(out_ptr.get());
            int32_t result = iconv(cd, const_cast<char **>(&in_buf), &in_length, 
                    &out_buf, &out_length);
            if (result < 0) {
                throw xcom::error::general_exception(
                        "failed to convert all characters from utf-8 to utf-16");
            }
            return std::u16string{ out_ptr.get() };
        }

        std::string utf16_to_utf8(const std::u16string& in)
        {
            iconv_t cd = iconv_open("UTF-8", "UTF-16LE");
            const char *in_buf = reinterpret_cast<const char *>(in.c_str());
            std::size_t in_length = in.length() * sizeof(char16_t);
            std::size_t out_length = in.length() * 4 + 1;
            std::unique_ptr<char []> out_ptr = std::make_unique<char []>(out_length);
            char *out_buf = out_ptr.get();
            int32_t result = iconv(cd, const_cast<char **>(&in_buf), &in_length,
                    &out_buf, &out_length);
            if (result < 0) {
                throw xcom::error::general_exception(
                        "failed to convert all characters from utf-16 to utf-8");
            }
            return std::string{ out_ptr.get() };
        }
#endif

    } // namespace util

    int32_t property::full_size() const
    {
        int32_t total = size();
        total += static_cast<int32_t>(name.length()) + 5;
        total += 4; //unknown 1
        total += static_cast<int32_t>(kind_string().length()) + 5;
        total += 4; //unknown 2
        total += 4; //propsize
        total += 4; //array index
        return total;
    }

    std::string property_kind_to_string(property::kind_t kind)
    {
        switch (kind)
        {
        case property::kind_t::int_property: return "IntProperty";
        case property::kind_t::float_property: return "FloatProperty";
        case property::kind_t::bool_property: return "BoolProperty";
        case property::kind_t::string_property: return "StrProperty";
        case property::kind_t::object_property: return "ObjectProperty";
        case property::kind_t::enum_property: return "ByteProperty";
        case property::kind_t::struct_property: return "StructProperty";
        case property::kind_t::name_property: return "NameProperty";
        case property::kind_t::array_property:
        case property::kind_t::object_array_property:
        case property::kind_t::number_array_property:
        case property::kind_t::struct_array_property:
        case property::kind_t::string_array_property:
        case property::kind_t::enum_array_property:
            return "ArrayProperty";
        case property::kind_t::static_array_property: return "StaticArrayProperty";
        default:
            std::stringstream err;
            err << "invalid property kind: " << static_cast<int>(kind) << std::endl;
            throw xcom::error::general_exception(err.str());
        }
    }

    std::string property::kind_string() const
    {
        return property_kind_to_string(kind);
    }

    static int32_t xcom_string_size(const xcom_string& s)
    {
        if (s.str.empty()) {
            // Empty strings are always 4 bytes (for the length)
            return 4;
        }

        if (s.is_wide) {
            // Wide string: convert from UTF-8 to UTF16 and return the length of the string * 2 for
            // the character data, plus 4 for the length int, plus 2 for the trailing null character.
            std::u16string tmp16 = util::utf8_to_utf16(s.str);
            return 6 + 2 * static_cast<int32_t>(tmp16.length());
        }
        else {
            // Narrow string: convert from UTF-8 to ISO-8859-1 and return the length of the string
            // plus 4 for the length int plus 1 for the trailing null character.
            std::string tmp = util::utf8_to_iso8859_1(s.str);
            return static_cast<int32_t>(tmp.length()) + 5;
        }
    }

    int32_t string_property::size() const
    {
        return xcom_string_size(str);
    }

    int32_t struct_property::full_size() const
    {
        int32_t total = property::full_size();
        total += static_cast<int32_t>(struct_name.length()) + 5 + 4;
        return total;
    }

    int32_t struct_property::size() const
    {
        // Size does not include the struct name itself
        if (native_data_length > 0) {
            return native_data_length;
        }
        else {
            int32_t total = 0;

            std::for_each(properties.begin(), properties.end(), [&total](const property_ptr &prop) {
                total += prop->full_size();
            });

            // Add the size of the "None" property terminating the list: 9 for "None" and 4 for the unknown int
            total += 9 + 4;
            return total;
        }
    }

    int32_t enum_array_property::size() const
    {
        int32_t total = 4; // the array bound

        for (const enum_value &e : elements) {
            if (e.name.length() == 0) {
                total += 8; // empty strings have size 4 for the count + 4 for the number following the string
            }
            else {
                std::string tmp = util::utf8_to_iso8859_1(e.name);
                // non-empty strings have length 4 + string length for the string + 1 for
                // the terminating null + 4 for the number following the string.
                total += 9 + static_cast<int32_t>(tmp.length()); 
            }
        }

        return total;
    }

    int32_t string_array_property::size() const
    {
        int32_t total = 4; // the array bound

        for (const xcom_string &s : elements) {
            total += xcom_string_size(s);
        }

        return total;
    }


    std::string build_actor_name(const std::string& package, const std::string& cls, int instance)
    {
        std::stringstream ret;
        ret << package << "." << cls << "_" << (instance - 1);
        return ret.str();
    }
    
    std::string build_actor_name_EU(const std::string& cls, int instance)
    {
        std::stringstream ret;
        ret << cls << "_" << (instance - 1);
        return ret.str();
    }
    
    std::tuple<std::string, std::string, int> decompose_actor_name(const std::string& actorName)
    {
        size_t underscore;
        size_t dot;

        dot = actorName.find_first_of('.');
        underscore = actorName.find_last_of('_');

        std::string package = actorName.substr(0, dot);
        std::string cls = actorName.substr(dot + 1, underscore - dot - 1);
        int instance = std::stoi(actorName.substr(underscore + 1));
        return std::make_tuple(package, cls, instance + 1);
    }

    std::tuple<std::string, int> decompose_actor_name_EU(const std::string& actorName)
    {
        size_t underscore = actorName.find_last_of('_');

        std::string cls = actorName.substr(0, underscore);
        int instance = std::stoi(actorName.substr(underscore + 1) );
        return std::make_tuple(cls, instance + 1);
    }
}
