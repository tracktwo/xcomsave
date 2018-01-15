#include "xcom.h"
#include <sstream>
#include <string>
#include <stdarg.h>

namespace xcom {
    namespace error {
        std::string unsupported_version::what() const noexcept {
            std::ostringstream stream;
            stream << "Error: unsupported version: " << version_ << std::endl;
            return stream.str();
        }

        std::string crc_mismatch::what() const noexcept {
            std::ostringstream stream;
            stream << "Error: corrupted save (CRC mismatch)" << std::endl;
            return stream.str();
        }

        format_exception::format_exception(std::ptrdiff_t offset, const char *msg, ...)
        {
            va_list args;
            va_start(args, msg);
            vsnprintf(buf_, sizeof buf_, msg, args);
            va_end(args);
            offset_ = offset;
        }

        std::string format_exception::what() const noexcept {
            std::ostringstream stream;
            stream << "Error: invalid save format at 0x" 
                << std::hex << offset_ 
                << ": "
                << buf_ 
                << std::endl;
            return stream.str();
        }
    }
}