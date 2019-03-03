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

#ifndef XCOMREADER_H
#define XCOMREADER_H

#include <stdint.h>
#include <string>

#include "xcom.h"
#include "util.h"

namespace xcom
{
    // A low-level i/o class for managing an xcom save. Provides basic
    // read and write functionality of primitive types (ints, strings,
    // bools, byte arrays, etc.).
    class xcom_io
    {
    public:
        static const constexpr int32_t initial_size = 1024 * 1024;

        // Construct an xcom_io object from an existing buffer (e.g.
        // a raw save file).
        xcom_io(buffer<unsigned char>&& b) :
            start_(std::move(b.buf)), length_(b.length)
        {
            ptr_ = start_.get();
        }

        // Construct an empty xcom_io object, e.g. for writing a save.
        xcom_io()
        {
            start_ = std::make_unique<unsigned char[]>(initial_size);
            ptr_ = start_.get();
            length_ = initial_size;
        }

    public:

        // Return the current offset of the cursor within the buffer.
        std::ptrdiff_t offset() const {
            return ptr_ - start_.get();
        }

        // Return the current size of the buffer.
        size_t size() const {
            return length_;
        }

        // Return a pointer to the current byte at the cursor.
        const unsigned char * pointer() const {
            return ptr_;
        }

        // Are we at the end of the file?
        bool eof() const
        {
            return static_cast<size_t>(offset()) >= length_;
        }

        // Extract the raw buffer from the io object. After this the
        // io object is empty (length 0 and holds no pointer).
        buffer<unsigned char> release() {
            buffer<unsigned char> b = { std::move(start_), length_ };
            start_.reset();
            ptr_ = nullptr;
            length_ = 0;
            return b;
        }

        enum class seek_kind {
            start,
            current,
            end
        };

        // Check at least 'count' bytes remain in buffer.
        bool bounds_check(size_t count) const;

        // Seek to a position within the buffer based on the seek_kind
        void seek(seek_kind k, std::ptrdiff_t offset);

        // Compute a crc over the next length bytes from the cursor.
        uint32_t crc(size_t length);

        // Read a 32-bit signed integer
        int32_t read_int();

        // Read a single-precision float
        float read_float();

        // Read a string. Returned string is converted to UTF-8. The
        // string is expected to be a Latin-1 string in the save file.
        std::string read_string();

        // Read a (possibly) unicode string from the save file. Returned
        // string is in UTF-8, but has a corresponding flag indicating
        // whether or not the string must be written to the save as a
        // UTF-16 string.
        xcom_string read_unicode_string(bool throw_on_error = true);

        // Read a boolean value. Note bools take up 4 bytes in xcom saves.
        bool read_bool();

        // Read a single byte
        unsigned char read_byte();

        // Read count bytes from the save
        std::unique_ptr<unsigned char[]> read_raw_bytes(int32_t count);

        // Read count bytes from the save and store them in the buffer pointed
        // to by outp.
        void read_raw_bytes(int32_t count, unsigned char *outp);

        // Ensure enough space exists in the internal buffer to hold count bytes.
        void ensure(size_t count);

        // Write a string. Str is expected to be in UTF-8 format but will be converted
        // to Latin-1 on write.
        void write_string(const std::string& str);

        // Write a (possibly) unicode string. If the provided string is wide the string
        // will be converted to UTF-16 before writing, otherwise it will be converted to Latin-1.
        void write_unicode_string(const xcom_string &str);

        // Write an integer
        void write_int(int32_t val);

        int16_t read_int16();

        // Write a single-precision float
        void write_float(float val);

        // Write a boolean value
        void write_bool(bool b);

        // Write a single byte
        void write_byte(unsigned char c);

        // Write len bytes pointed to by buf
        void write_raw(unsigned char *buf, int32_t len);

    protected:
        // The buffer itself, always points to the start of the owned memory.
        std::unique_ptr<unsigned char[]> start_;

        // The current cursor into the buffer.
        unsigned char *ptr_;

        // The number of bytes in the buffer pointed to by start_
        size_t length_;
    };

} // namespace xcom
#endif //XCOM_H
