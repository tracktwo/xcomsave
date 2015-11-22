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
    class xcom_io
    {
    public:
        static const constexpr size_t initial_size = 1024 * 1024;

        xcom_io::xcom_io(buffer<unsigned char>&& b) :
            start_(std::move(b.buf)), length_(b.length)
        {
            ptr_ = start_.get();
        }

        xcom_io::xcom_io()
        {
            start_ = std::make_unique<unsigned char[]>(initial_size);
            ptr_ = start_.get();
            length_ = initial_size;
        }

    public:
        buffer<unsigned char> uncompressed_data() const;
        saved_game save_data();

    public:
        std::ptrdiff_t offset() const {
            return ptr_ - start_.get();
        }

        size_t size() const {
            return length_;
        }

        const unsigned char * pointer() const {
            return ptr_;
        }

        bool eof() const
        {
            return static_cast<size_t>(offset()) >= length_;
        }

        buffer<unsigned char> release() {
            buffer<unsigned char> b = { std::move(start_), length_ };
            start_.reset();
            length_ = 0;
            return b;
        }

        enum class seek_kind {
            start,
            current,
            end
        };

        uint32_t crc(size_t length);

        void seek(seek_kind k, size_t offset);
        int32_t read_int();
        float read_float();
        std::string read_string();
        xcom_string read_unicode_string();
        bool read_bool();
        unsigned char read_byte();
        std::unique_ptr<unsigned char[]> read_raw_bytes(size_t count);
        void read_raw_bytes(size_t count, unsigned char *outp);

        void ensure(size_t count);
        void write_string(const std::string& str);
        void write_unicode_string(const xcom_string &str);
        void write_int(int32_t val);
        void write_float(float val);
        void write_bool(bool b);
        void write_byte(unsigned char c);
        void write_raw(unsigned char *buf, size_t len);
    protected:
        std::unique_ptr<unsigned char[]> start_;
        unsigned char *ptr_;
        size_t length_;
    };

} // namespace xcom
#endif //XCOM_H
