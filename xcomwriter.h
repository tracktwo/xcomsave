#ifndef XCOMWRITER_H
#define XCOMWRITER_H

#include <stdint.h>
#include <string>

#include "xcom.h"
#include "util.h"

namespace xcom
{
	struct property_writer_visitor;

	class writer
	{
	public:
		writer(saved_game && save) :
			save_(std::move(save)), ptr_{}, length_{ 0 }
		{}

		buffer<unsigned char> save_data();

		static const constexpr size_t initial_size = 1024 * 1024;

		friend property_writer_visitor;

	private:
		ptrdiff_t offset() const {
			return ptr_ - start_.get();
		}

		void ensure(size_t count);
		void write_string(const std::string& str);
		void write_int(int32_t val);
		void write_float(float val);
		void write_bool(bool b);
		void write_raw(unsigned char *buf, size_t len);
		void write_header(const header& header);
		void write_actor_table(const actor_table& actors);
		void write_checkpoint_chunks(const checkpoint_chunk_table& chunks);
		void write_checkpoint_chunk(const checkpoint_chunk& chunk);
		void write_checkpoint_table(const checkpoint_table& checkpoints);
		void write_checkpoint(const checkpoint& chk);
		void write_property(const property_ptr& prop, int32_t array_index);
		buffer<unsigned char> compress();

	private:
		saved_game save_;
		std::unique_ptr<unsigned char[]> start_;
		unsigned char* ptr_;
		size_t length_;
	};
} //namespace xcom
#endif // XCOMWRITER_H
