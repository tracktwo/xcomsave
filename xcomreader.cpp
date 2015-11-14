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

#include "minilzo.h"
#include "xcomreader.h"
#include "util.h"

#include <string>
#include <memory>
#include <cassert>
#include <cstdlib>
#include <cstring>

namespace xcom
{
    int32_t reader::read_int()
    {
        int32_t v = *(reinterpret_cast<const int32_t*>(ptr_));
        ptr_ += 4;
        return v;
    }

    float reader::read_float()
    {
        float f = *(reinterpret_cast<const float*>(ptr_));
        ptr_ += 4;
        return f;
    }

    bool reader::read_bool()
    {
        return read_int() != 0;
    }

    std::string reader::read_string()
    {
        int32_t length = read_int();
        if (length == 0) {
            return "";
        }

        if (length < 0) {
            // A UTF-16 encoded string.
            length = -length;
            const wchar_t *str = reinterpret_cast<const wchar_t*>(ptr_);
            ptr_ += 2 * length;
            return util::utf16_to_utf8(str);
        }
        else {
            const char *str = reinterpret_cast<const char *>(ptr_);
            size_t actual_length = strlen(str);

            // Double check the length matches what we read from the file, considering the trailing null is counted in the length 
            // stored in the file.
            if (actual_length != (length - 1))
            {
                fprintf(stderr, "Error: String mismatch at offset %d. Expected length %d but got %d\n", ptr_ - start_.get(), length, actual_length);
                return "<error>";
            }
            ptr_ += length;
            return util::iso8859_1_to_utf8(str);
        }
    }

    header reader::read_header()
    {
        header hdr;
        hdr.version = read_int();
        if (hdr.version != save_version) {
            fprintf(stderr, "Error: Data does not appear to be an xcom save: expected file version %d but got %d\n", save_version, hdr.version);
            return{ 0 };
        }
        hdr.uncompressed_size = read_int();
        hdr.game_number = read_int();
        hdr.save_number = read_int();
        hdr.save_description = read_string();
        hdr.time = read_string();
        hdr.map_command = read_string();
        hdr.tactical_save = read_bool();
        hdr.ironman = read_bool();
        hdr.autosave = read_bool();
        hdr.dlc = read_string();
        hdr.language = read_string();
        uint32_t compressed_crc = (uint32_t)read_int();

        // Compute the CRC of the header
        ptr_ = start_.get() + 1016;
        int32_t hdr_size = read_int();
        uint32_t hdr_crc = (uint32_t)read_int();
        uint32_t computed_hdr_crc = util::crc32b(start_.get(), hdr_size);
        if (hdr_crc != computed_hdr_crc)
        {
            throw std::runtime_error("CRC mismatch in header. Bad save?");
        }

        uint32_t computed_compressed_crc = util::crc32b(start_.get() + 1024, length_ - 1024);
        if (computed_compressed_crc != compressed_crc)
        {
            throw std::runtime_error("CRC mismatch in compressed data. Bad save?");
        }
        return hdr;
    }

    actor_table reader::read_actor_table()
    {
        actor_table actors;
        int32_t actor_count = read_int();

        // We expect all entries to be of the form <package> <0> <actor> <instance>, or two entries
        // per real actor.
        assert(actor_count % 2 == 0);

        for (int i = 0; i < actor_count; i += 2) {
            std::string actor_name = read_string();
            int32_t instance = read_int();
            if (instance == 0) {
                throw format_exception(offset(), "Malformed actor table entry: expected a non-zero instance\n");
            }
            std::string package = read_string();
            int32_t sentinel = read_int();
            if (sentinel != 0) {
                throw format_exception(offset(), "Malformed actor table entry: missing 0 instance\n");
            }
            actors.push_back(build_actor_name(package, actor_name, instance));
        }

        return actors;
    }

    property_ptr reader::make_struct_property(const std::string &name)
    {
        std::string struct_name = read_string();
        int32_t inner_unknown = read_int();
        if (inner_unknown != 0) {
            throw format_exception(offset(), "Read non-zero prop unknown value in struct property: %x\n", inner_unknown);
        }

        // Special case certain structs
        if (struct_name.compare("Vector2D") == 0) {
            std::unique_ptr<unsigned char[]> nativeData = std::make_unique<unsigned char[]>(8);
            memcpy(nativeData.get(), ptr_, 8);
            ptr_ += 8;
            return std::make_unique<struct_property>(name, struct_name, std::move(nativeData), 8);
        }
        else if (struct_name.compare("Vector") == 0) {
            std::unique_ptr<unsigned char[]> nativeData = std::make_unique<unsigned char[]>(12);
            memcpy(nativeData.get(), ptr_, 12);
            ptr_ += 12;
            return std::make_unique<struct_property>(name, struct_name, std::move(nativeData), 12);
        }
        else {
            property_list structProps = read_properties();
            return std::make_unique<struct_property>(name, struct_name, std::move(structProps));
        }
    }

    // Determine if this is a struct or string property. Returns struct_array_property if it's a struct
    // array, string_array_property if it's a string/enum array, or last_array_property if it's neither.
    static property::kind_t is_struct_or_string_property(const unsigned char *ptr, uint32_t array_data_size)
    {
        // Sniff the first part of the array data to see if it looks like a string.
        int32_t str_length = *reinterpret_cast<const int*>(ptr);
        if (str_length > 0 && static_cast<size_t>(str_length) < array_data_size) {
            const char *str = reinterpret_cast<const char *>(ptr) + 4;
            if (str[str_length-1] == 0 && strlen(str) == (str_length -1))
            {
                // Ok, looks like a name string. Distinguish between a struct
                // array and a enum array by looking at the next string. if it's a property
                // kind, then we have a property list and this is a struct. If it's not, then
                // we must have an enum array.

                // Skip over the string length, string data, and zero int.
                ptr += 4 + str_length + 4;
                str_length = *reinterpret_cast<const int*>(ptr);
                if (str_length > 0 && static_cast<size_t>(str_length) < array_data_size) {
                    str = reinterpret_cast<const char *>(ptr + 4);
                    if (str[str_length-1] == 0 && strlen(str) == (str_length - 1))
                    {
                        for (int i = 0; i < static_cast<int>(property::kind_t::last_property); ++i) {
                            if (property_kind_to_string(static_cast<property::kind_t>(i)) == str) {
                                return property::kind_t::struct_array_property;
                            }
                        }
                    }
                }

                // Doesn't look like a struct. This is likely an enum (or string?)
                return property::kind_t::string_array_property;
            }
        }

        return property::kind_t::last_property;
    }


    property_ptr reader::make_array_property(const std::string &name, int32_t property_size)
    {
        int32_t array_bound = read_int();
        std::unique_ptr<unsigned char[]> array_data;
        int array_data_size = property_size - 4;
        if (array_data_size > 0) {
            if (array_bound * 8 == array_data_size) {
                // Looks like an array of objects
                std::vector<int32_t> elements;
                for (int32_t i = 0; i < array_bound; ++i) {
                    int32_t actor1 = read_int();
                    int32_t actor2 = read_int();
                    if (actor1 == -1 && actor2 == -1) {
                        elements.push_back(actor1);
                    }
                    else if (actor1 != (actor2 + 1)) {
                        throw format_exception(offset(), "Assertion failed: expected related actor numbers in object array\n");
                    }
                    else {
                        elements.push_back(actor1 / 2);
                    }
                }
                return std::make_unique<object_array_property>(name, elements);
            }
            else if (array_bound * 4 == array_data_size) {
                // Looks like an array of ints or floats
                std::vector<int32_t> elems;
                for (int i = 0; i < array_bound; ++i) {
                    elems.push_back(read_int());
                }

                return std::make_unique<number_array_property>(name, elems);
            }
            else {
                property::kind_t kind = is_struct_or_string_property(ptr_, array_data_size);
                if (kind == property::kind_t::struct_array_property) {
                    std::vector<property_list> elements;
                    for (int32_t i = 0; i < array_bound; ++i) {
                        elements.push_back(read_properties());
                    }

                    return std::make_unique<struct_array_property>(name, std::move(elements));
                }
#if 0
                else if (kind == property::kind_t::string_array_property) {
                    std::vector<std::string> elements;
                    for (int32_t i = 0; i < array_bound; ++i) {
                        elements.push_back(read_string());
                        int32_t tmp = read_int();
                        if (tmp != 0) {
                            throw format_exception(offset(), "Read non-zero value after string/enum array element");
                        }
                    }

                    return std::make_unique<string_array_property>(name, std::move(elements));
                }
#endif
                else {
                    // Nope, dunno what this thing is.
                    array_data = std::make_unique<unsigned char[]>(array_data_size);
                    memcpy(array_data.get(), ptr_, array_data_size);
                    ptr_ += array_data_size;
                }
            }
        }
        return std::make_unique<array_property>(name, std::move(array_data), array_data_size, array_bound);
    }

    property_list reader::read_properties()
    {
        property_list properties;
        for (;;)
        {
            std::string name = read_string();
            int32_t unknown1 = read_int();
            if (unknown1 != 0) {
                throw format_exception(offset(), "Read non-zero property unknown value: %x\n", unknown1);
            }

            if (name.compare("None") == 0) {
                break;
            }
            std::string prop_type = read_string();
            int32_t unknown2 = read_int();
            if (unknown2 != 0) {
                throw format_exception(offset(), "Read non-zero property unknown2 value: %x\n", unknown2);
            }
            int32_t prop_size = read_int();
            int32_t array_index = read_int();

            property_ptr prop;
            if (prop_type.compare("ObjectProperty") == 0) {
                assert(prop_size == 8);
                int32_t actor1 = read_int();
                int32_t actor2 = read_int();
                if (actor1 != -1 && actor1 != (actor2 + 1)) {
                    throw format_exception(offset(), "Assertion failed: actor references in object property not related.\n");
                }

                prop = std::make_unique<object_property>(name, (actor1 == -1) ? actor1 : (actor1 / 2));
            }
            else if (prop_type.compare("IntProperty") == 0) {
                assert(prop_size == 4);
                int32_t val = read_int();
                prop = std::make_unique<int_property>(name, val);
            }
            else if (prop_type.compare("ByteProperty") == 0) {
                std::string enum_type = read_string();
                int32_t inner_unknown = read_int();
                if (inner_unknown != 0) {
                    throw format_exception(offset(), "Read non-zero enum property unknown value: %x\n", inner_unknown);
                }
                std::string enum_val = read_string();
                int32_t extra_val = read_int();
                prop = std::make_unique<enum_property>(name, enum_type, enum_val, extra_val);
            }
            else if (prop_type.compare("BoolProperty") == 0) {
                assert(prop_size == 0);
                bool val = *ptr_++ != 0;
                prop = std::make_unique<bool_property>(name, val);
            }
            else if (prop_type.compare("ArrayProperty") == 0) {
                prop = make_array_property(name, prop_size);
            }
            else if (prop_type.compare("FloatProperty") == 0) {
                float f = read_float();
                prop = std::make_unique<float_property>(name, f);
            }
            else if (prop_type.compare("StructProperty") == 0) {
                prop = make_struct_property(name);
            }
            else if (prop_type.compare("StrProperty") == 0) {
                std::string str = read_string();
                prop = std::make_unique<string_property>(name, str);
            }
            else
            {
                throw format_exception(offset(), "Unknown property type %s\n", prop_type.c_str());
            }

            if (prop.get() != nullptr) {
                assert(prop->size() == prop_size);
                if (array_index == 0) {
                    properties.push_back(std::move(prop));
                }
                else {
                    if (properties.back()->name.compare(name) != 0) {
                        throw format_exception(offset(), "Static array index found but doesn't match previous property\n");
                    }

                    if (properties.back()->kind == property::kind_t::static_array_property) {
                        // We already have a static array. Sanity check the array index and add it
                        static_array_property *static_array = static_cast<static_array_property*>(properties.back().get());
                        assert(array_index == static_array->properties.size());
                        static_array->properties.push_back(std::move(prop));
                    }
                    else {
                        // Not yet a static array. This new property should have index 1.
                        assert(array_index == 1);

                        // Pop off the old property
                        property_ptr last_property = std::move(properties.back());
                        properties.pop_back();

                        // And replace it with a new static array
                        std::unique_ptr<static_array_property> static_array = std::make_unique<static_array_property>(name);
                        static_array->properties.push_back(std::move(last_property));
                        static_array->properties.push_back(std::move(prop));
                        properties.push_back(std::move(static_array));
                    }
                }
            }
        }

        return properties;
    }

    checkpoint_table reader::read_checkpoint_table()
    {
        checkpoint_table checkpoints;
        int32_t checkpoint_count = read_int();

        for (int i = 0; i < checkpoint_count; ++i) {
            checkpoint chk;
            chk.name = read_string();
            chk.instance_name = read_string();
            chk.vector[0] = read_float();
            chk.vector[1] = read_float();
            chk.vector[2] = read_float();
            chk.rotator[0] = read_int();
            chk.rotator[1] = read_int();
            chk.rotator[2] = read_int();
            chk.class_name = read_string();
            int32_t prop_length = read_int();
            if (prop_length < 0) {
                throw format_exception(offset(), "Found negative property length\n");
            }
            chk.pad_size = 0;
            const unsigned char* start_ptr = ptr_;

            chk.properties = read_properties();
            if ((ptr_ - start_ptr) < prop_length) {
                chk.pad_size = prop_length - (ptr_ - start_ptr);

                for (unsigned int i = 0; i < chk.pad_size; ++i) {
                    if (*ptr_++ != 0) {
                        throw format_exception(offset(), "Found non-zero padding byte\n");
                    }
                }
            }
            size_t total_prop_size = 0;
            std::for_each(chk.properties.begin(), chk.properties.end(), [&total_prop_size](const property_ptr& prop) {
                total_prop_size += prop->full_size();
            });
            total_prop_size += 9 + 4; // length of trailing "None" to terminate the list + the unknown int.
            assert((uint32_t)prop_length == (total_prop_size + chk.pad_size));
            chk.template_index = read_int();
            checkpoints.push_back(std::move(chk));
        }

        return checkpoints;
    }

    actor_template_table reader::read_actor_template_table()
    {
        actor_template_table template_table;
        int32_t templateCount = read_int();

        for (int i = 0; i < templateCount; ++i) {
            actor_template tmpl;
            tmpl.actor_class_path = read_string();
            memcpy(tmpl.load_params, ptr_, 64);
            ptr_ += 64;
            tmpl.archetype_path = read_string();
            template_table.push_back(std::move(tmpl));
        }

        return template_table;
    }

    name_table reader::read_name_table()
    {
        static unsigned char all_zeros[8] = { 0 };
        name_table names;
        int32_t name_count = read_int();

        for (int i = 0; i < name_count; ++i) {
            name_entry entry;
            entry.name = read_string();
            memcpy(entry.zeros, ptr_, 8);
            ptr_ += 8;
            if (memcmp(entry.zeros, all_zeros, 8) != 0) {
                throw format_exception(offset(), "Expected all zeros in name table entry\n");
                return{};
            }
            entry.data_length = read_int();
            entry.data = new unsigned char[entry.data_length];
            memcpy(entry.data, ptr_, entry.data_length);
            ptr_ += entry.data_length;
            names.push_back(std::move(entry));
        }

        return names;
    }

    size_t reader::uncompressed_size()
    {
        // The compressed data begins 1024 bytes into the file.
        const unsigned char* p = start_.get() + 1024;
        size_t compressed_size;
        size_t uncompressed_size = 0;
        do
        {
            // Expect the magic header value 0x9e2a83c1 at the start of each chunk
            if (*(reinterpret_cast<const int32_t*>(p)) != UPK_Magic) {
                throw format_exception(offset(), "Failed to find compressed chunk header\n");
                return -1;
            }

            // Compressed size is at p+8
            compressed_size = *(reinterpret_cast<const unsigned long*>(p + 8));

            // Uncompressed size is at p+12
            uncompressed_size += *(reinterpret_cast<const unsigned long*>(p + 12));

            // Skip to next chunk - 24 bytes of this chunk header + compressedSize bytes later.
            p += (compressed_size + 24);
        } while (static_cast<size_t>(p - start_.get()) < length_);

        return uncompressed_size;
    }

    void reader::uncompressed_data(unsigned char *buf)
    {
        // Start back at the beginning of the compressed data.
        const unsigned char *p = start_.get() + 1024;
        unsigned char *outp = buf;

        do
        {
            // Expect the magic header value 0x9e2a83c1 at the start of each chunk
            if (*(reinterpret_cast<const uint32_t*>(p)) != UPK_Magic) {
                throw format_exception(p - start_.get(), "Failed to find compressed chunk header\n");
                return;
            }

            // Compressed size is at p+8
            int32_t compressed_size = *(reinterpret_cast<const int32_t *>(p + 8));

            // Uncompressed size is at p+12
            int32_t uncompressed_size = *(reinterpret_cast<const int32_t*>(p + 12));

            unsigned long decomp_size = uncompressed_size;

            if (lzo1x_decompress_safe(p + 24, compressed_size, outp, &decomp_size, nullptr) != LZO_E_OK) {
                throw format_exception(p - start_.get(), "LZO decompress of save data failed\n");
                return;
            }

            unsigned long tmpSize = decomp_size + decomp_size / 16 + 64 + 3;
            unsigned char * tmpBuf = new unsigned char[tmpSize];

            if (decomp_size != uncompressed_size)
            {
                throw format_exception(p - start_.get(), "Failed to decompress chunk\n");
                return;
            }

            // Skip to next chunk - 24 bytes of this chunk header + compressedSize bytes later.
            p += (compressed_size + 24);
            outp += uncompressed_size;
        } while (static_cast<size_t>(p - start_.get()) < length_);
    }

    saved_game reader::save_data()
    {
        saved_game save;
        save.hdr = read_header();

        size_t uncompressed_length = uncompressed_size();
        if (uncompressed_length < 0) {
            return{};
        }

        unsigned char *data = new unsigned char[uncompressed_length];
        uncompressed_data(data);

        // We're now done with the compressed file. Swap over to the uncompressed data
        ptr_ = data;
        start_.reset(ptr_);
        length_ = uncompressed_length;
#if 1
        FILE *out_file = fopen("output.dat", "wb");
        if (out_file == nullptr) {
            throw std::exception("Failed to open output file");
        }
        fwrite(data, 1, length_, out_file);
        fclose(out_file);
#endif
        save.actors = read_actor_table();

        // Jump back to here after each chunk
        do {
            checkpoint_chunk chunk;
            chunk.unknown_int1 = read_int();
            chunk.game_type = read_string();
            std::string none = read_string();
            if (none != "None") {
                throw format_exception(offset(), "Failed to locate 'None' after actor table\n");
                return{};
            }

            chunk.unknown_int2 = read_int();
            chunk.checkpoints = read_checkpoint_table();
            int32_t name_table_length = read_int();
            assert(name_table_length == 0);
            chunk.class_name = read_string();
            chunk.actors = read_actor_table();
            chunk.unknown_int3 = read_int();
            actor_template_table actor_templates = read_actor_template_table(); // (only seems to be present for tactical saves?)
            assert(actor_templates.size() == 0);
            chunk.display_name = read_string(); //unknown (game name)
            chunk.map_name = read_string(); //unknown (map name)
            chunk.unknown_int4 = read_int(); //unknown  (checksum?)

            save.checkpoints.push_back(std::move(chunk));
        } while ((static_cast<size_t>(offset()) < length_));
        return save;
    }

} //namespace xcom
