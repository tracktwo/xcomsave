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
	class reader
	{
	public:
		reader(buffer<unsigned char>&& b) :
			start_(std::move(b.buf))
		{
			length_ = b.length;
			ptr_ = start_.get();
		}

		saved_game save_data();

	private:
		std::ptrdiff_t offset() const {
			return ptr_ - start_.get();
		}

		int32_t read_int();
		float read_float();
		std::string read_string();
		bool read_bool();
		header read_header();
		actor_table read_actor_table();
		checkpoint_table read_checkpoint_table();
		std::vector<std::unique_ptr<property>> read_properties();
		property_ptr make_array_property(const std::string& name, int32_t propSize);
		property_ptr make_struct_property(const std::string& name);
		actor_template_table read_actor_template_table();
		name_table read_name_table();
		size_t uncompressed_size();
		void uncompressed_data(unsigned char *);

	private:
		std::unique_ptr<unsigned char[]> start_;
		unsigned char *ptr_;
		size_t length_;
	};

} // namespace xcom
#endif //XCOM_H
