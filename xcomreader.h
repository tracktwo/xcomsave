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
		int32_t uncompressed_size();
		void uncompressed_data(unsigned char *);

	private:
		std::unique_ptr<unsigned char[]> start_;
		unsigned char *ptr_;
		size_t length_;
	};

} // namespace xcom
#endif //XCOM_H
