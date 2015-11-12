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

#include "xcomwriter.h"
#include "minilzo.h"
#include "xslib_internal.h"
#include <cassert>
#include <cstring>
#include <tuple>

namespace xcom
{
	void writer::ensure(size_t count)
	{
		ptrdiff_t current_count = offset();

		if ((current_count + count) > length_) {
			size_t new_length = length_ * 2;
			unsigned char * new_buffer = new unsigned char[new_length];
			memcpy(new_buffer, start_.get(), current_count);
			start_.reset(new_buffer);
			length_ = new_length;
			ptr_ = start_.get() + current_count;
		}

	}

	void writer::write_string(const std::string& str)
	{
		if (str.empty()) {
			// An empty string is just written as size 0
			write_int(0);
		}
		else {
			// Ensure we have space for the string size + string data + trailing null
			std::string conv = util::utf8_to_iso8859_1(str);
			ensure(conv.length() + 5);
			write_int(conv.length() + 1);
			memcpy(ptr_, conv.c_str(), conv.length());
			ptr_ += conv.length();
			*ptr_++ = 0;
		}
	}

	void writer::write_int(int32_t val)
	{
		ensure(4);
		*(reinterpret_cast<int32_t*>(ptr_)) = val;
		ptr_ += 4;
	}


	void writer::write_float(float val)
	{
		ensure(4);
		*(reinterpret_cast<float*>(ptr_)) = val;
		ptr_ += 4;
	}

	void writer::write_bool(bool b)
	{
		write_int(b);
	}

	void writer::write_raw(unsigned char *buf, size_t len)
	{
		ensure(len);
		memcpy(ptr_, buf, len);
		ptr_ += len;
	}

	void writer::write_header(const header& hdr)
	{
		write_int(hdr.version);
		write_int(0);
		write_int(hdr.game_number);
		write_int(hdr.save_number);
		write_string(hdr.save_description);
		write_string(hdr.time);
		write_string(hdr.map_command);
		write_bool(hdr.tactical_save);
		write_bool(hdr.ironman);
		write_bool(hdr.autosave);
		write_string(hdr.dlc);
		write_string(hdr.language);

		// Compute the CRC for the compressed data.
		uint32_t compressed_crc = util::crc32b(start_.get() + 1024, length_ - 1024);
		write_int(compressed_crc);

		int32_t hdr_length = ptr_ - start_.get() + 4;
		ptr_ = start_.get() + 1016;
		write_int(hdr_length);
		uint32_t hdr_crc = util::crc32b(start_.get(), hdr_length);
		write_int(hdr_crc);
	}

	void writer::write_actor_table(const actor_table& actors)
	{
		// Each actorTable entry has 2 entries in the save table; names are split.
		write_int(actors.size() * 2);
		for (const std::string& actor : actors) {
			std::tuple<std::string, std::string, int> tup = decompose_actor_name(actor);
			write_string(std::get<1>(tup));
			write_int(std::get<2>(tup));
			write_string(std::get<0>(tup));
			write_int(0);
		}
	}

	struct property_writer_visitor : public property_visitor
	{
		property_writer_visitor(writer* writer) : writer_(writer) {}

		virtual void visit(int_property* prop) override
		{
			writer_->write_int(prop->value);
		}

		virtual void visit(float_property *prop) override
		{
			writer_->write_float(prop->value);
		}

		virtual void visit(bool_property *prop) override
		{
			writer_->ensure(1);
			*writer_->ptr_++ = prop->value;
		}

		virtual void visit(string_property *prop) override
		{
			writer_->write_string(prop->str);
		}

		virtual void visit(object_property *prop) override
		{
			if (prop->actor == 0xffffffff) {
				writer_->write_int(prop->actor);
				writer_->write_int(prop->actor);
			}
			else {
				writer_->write_int(prop->actor * 2 + 1);
				writer_->write_int(prop->actor * 2);
			}
		}

		virtual void visit(enum_property *prop) override
		{
			writer_->write_string(prop->enum_type);
			writer_->write_int(0);
			writer_->write_string(prop->enum_value);
			writer_->write_int(prop->extra_value);
		}

		virtual void visit(struct_property *prop) override
		{
			writer_->write_string(prop->struct_name);
			writer_->write_int(0);
			if (prop->native_data_length > 0) {
				writer_->write_raw(prop->native_data.get(), prop->native_data_length);
			}
			else {
				for (unsigned int i = 0; i < prop->properties.size(); ++i) {
					writer_->write_property(prop->properties[i], 0);
				}
				writer_->write_string("None");
				writer_->write_int(0);
			}
		}

		virtual void visit(array_property *prop) override
		{
			writer_->write_int(prop->array_bound);
			size_t data_length = prop->size() - 4;
			writer_->write_raw(prop->data.get(), data_length);
		}

		virtual void visit(object_array_property *prop) override
		{
			writer_->write_int(prop->elements.size());
			for (size_t i = 0; i < prop->elements.size(); ++i) {
				if (prop->elements[i] == -1) {
					writer_->write_int(prop->elements[i]);
					writer_->write_int(prop->elements[i]);
				}
				else {
					writer_->write_int(prop->elements[i] * 2 + 1);
					writer_->write_int(prop->elements[i] * 2);
				}
			}
		}

		virtual void visit(number_array_property *prop) override
		{
			writer_->write_int(prop->elements.size());
			for (size_t i = 0; i < prop->elements.size(); ++i) {
				writer_->write_int(prop->elements[i]);
			}
		}

		virtual void visit(struct_array_property *prop) override
		{
			writer_->write_int(prop->elements.size());
			std::for_each(prop->elements.begin(), prop->elements.end(), [this](const property_list &pl) {
				std::for_each(pl.begin(), pl.end(), [this](const property_ptr& p) {
					writer_->write_property(p, 0);
				});

				// Write the "None" to indicate the end of this struct.
				writer_->write_string("None");
				writer_->write_int(0);
			});
		}

		virtual void visit(static_array_property *) override
		{
			// This shouldn't happen: static arrays need special handling and can't be written normally as they don't
			// really exist in the save format.
			throw std::runtime_error("Attempted to write a static array property\n");
		}

	private:
		writer *writer_;
	};

	void writer::write_property(const property_ptr& prop, int32_t array_index)
	{
		// If this is a static array property we need to write only the contained properties, not the fake static array property created to contain it.
		if (prop->kind == property::kind_t::static_array_property) {
			static_array_property* static_array = dynamic_cast<static_array_property*>(prop.get());
			for (unsigned int idx = 0; idx < static_array->properties.size(); ++idx) {
				write_property(static_array->properties[idx], idx);
			}
		}
		else {
			// Write the common part of a property
			write_string(prop->name);
			write_int(0);
			write_string(prop->kind_string());
			write_int(0);
			write_int(prop->size());
			write_int(array_index);

			// Write the specific part
			property_writer_visitor v{ this };
			prop->accept(&v);
		}
	}

	void writer::write_checkpoint(const checkpoint& chk)
	{
		write_string(chk.name);
		write_string(chk.instance_name);
		write_float(chk.vector[0]);
		write_float(chk.vector[1]);
		write_float(chk.vector[2]);
		write_int(chk.rotator[0]);
		write_int(chk.rotator[1]);
		write_int(chk.rotator[2]);
		write_string(chk.class_name);
		size_t total_property_size = 0;
		std::for_each(chk.properties.begin(), chk.properties.end(), [&total_property_size](const property_ptr& prop) {
			total_property_size += prop->full_size();
		});
		total_property_size += 9 + 4; // length of trailing "None" to terminate the list + the unknown int.
		total_property_size += chk.pad_size;
		write_int(total_property_size);
		for (unsigned int i = 0; i < chk.properties.size(); ++i) {
			write_property(chk.properties[i], 0);
		}
		write_string("None");
		write_int(0);
		ensure(chk.pad_size);
		for (unsigned int i = 0; i < chk.pad_size; ++i) {
			*ptr_++ = 0;
		}
		write_int(chk.template_index);
	}

	void writer::write_checkpoint_table(const checkpoint_table& table)
	{
		write_int(table.size());
		for (const checkpoint& chk : table) {
			write_checkpoint(chk);
		}
	}

	void writer::write_checkpoint_chunk(const checkpoint_chunk& chunk)
	{
		write_int(chunk.unknown_int1);
		write_string(chunk.game_type);
		write_string("None");
		write_int(chunk.unknown_int2);
		write_checkpoint_table(chunk.checkpoints);
		write_int(0); // name table length
		write_string(chunk.class_name);
		write_actor_table(chunk.actors);
		write_int(chunk.unknown_int3);
		write_int(0); // actor template table length
		write_string(chunk.display_name);
		write_string(chunk.map_name);
		write_int(chunk.unknown_int4);
	}

	void writer::write_checkpoint_chunks(const checkpoint_chunk_table& chunks)
	{
		for (const checkpoint_chunk& chunk : chunks) {
			write_checkpoint_chunk(chunk);
		}
	}

	buffer<unsigned char> writer::compress()
	{
		int totalBufSize = offset();
		// Allocate a new buffer to hold the compressed data. Just allocate
		// as much as the uncompressed buffer since we don't know how big
		// it will be, but it'll presumably be smaller.
		buffer<unsigned char> b;
		b.buf = std::make_unique<unsigned char[]>(totalBufSize);

		// Compress the data in 128k chunks
		int idx = 0;
		static const int chunk_size = 0x20000;

		// The "flags" (?) value is always 20000, even for trailing chunks
		static const int chunk_flags = 0x20000;
		unsigned char *buf_ptr = start_.get();
		// Reserve 1024 bytes at the start of the compressed buffer for the header.
		unsigned char *compressed_ptr = b.buf.get() + 1024;
		int bytes_left = totalBufSize;
		int total_out_size = 1024;

		lzo_init();

		std::unique_ptr<char[]> wrkMem = std::make_unique<char[]>(LZO1X_1_MEM_COMPRESS);

		do
		{
			int uncompressed_size = (bytes_left < chunk_size) ? bytes_left : chunk_size;
			unsigned long bytes_compressed = bytes_left - 24;
			// Compress the chunk
			int ret = lzo1x_1_compress(buf_ptr, uncompressed_size, compressed_ptr + 24, &bytes_compressed, wrkMem.get());
			if (ret != LZO_E_OK) {
				fprintf(stderr, "Error compressing data: %d", ret);
			}
			// Write the magic number
			*reinterpret_cast<int*>(compressed_ptr) = UPK_Magic;
			compressed_ptr += 4;
			// Write the "flags" (?)
			*reinterpret_cast<int*>(compressed_ptr) = chunk_flags;
			compressed_ptr += 4;
			// Write the compressed size
			*reinterpret_cast<int*>(compressed_ptr) = bytes_compressed;
			compressed_ptr += 4;
			// Write the uncompressed size
			*reinterpret_cast<int*>(compressed_ptr) = uncompressed_size;
			compressed_ptr += 4;
			// Write the compressed size
			*reinterpret_cast<int*>(compressed_ptr) = bytes_compressed;
			compressed_ptr += 4;
			// Write the uncompressed size
			*reinterpret_cast<int*>(compressed_ptr) = uncompressed_size;
			compressed_ptr += 4;

			compressed_ptr += bytes_compressed;
			bytes_left -= chunk_size;
			buf_ptr += chunk_size;
			total_out_size += bytes_compressed + 24;
		} while (bytes_left > 0);

		b.length = total_out_size;
		return b;
	}

	buffer<unsigned char> writer::save_data()
	{
		start_ = std::make_unique<unsigned char[]>(initial_size);
		ptr_ = start_.get();
		length_ = initial_size;

		// Write out the initial actor table
		write_actor_table(save_.actors);

		// Write the checkpoint chunks
		write_checkpoint_chunks(save_.checkpoints);

#if 0
		// Write the raw data file
		FILE *out_file = fopen("newoutput.dat", "wb");
		if (out_file == nullptr) {
			throw std::runtime_error("Failed to open output file\n");
		}
		fwrite(start_.get(), 1, offset(), out_file);
		fclose(out_file);
#endif
		buffer<unsigned char> b = compress();

		// Reset the internal buffer to the compressed data to write the header
		start_ = std::move(b.buf);
		ptr_ = start_.get();
		length_ = b.length;

		write_header(save_.hdr);

		// Move the save data back into the buffer to return
		b.buf = std::move(start_);
		ptr_ = nullptr;
		length_ = 0;
		return b;
	}
}
