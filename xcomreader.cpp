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
#include "xcomio.h"
#include "util.h"

#include <string>
#include <memory>
#include <cassert>
#include <cstdlib>
#include <cstring>

namespace xcom
{
    static const size_t compressed_data_start = 1024;

    property_list read_properties(xcom_io &r);

    header read_header(xcom_io &r)
    {
        header hdr;
        hdr.version = r.read_int();
        if (hdr.version != save_version) {
            fprintf(stderr, 
                "Error: Data does not appear to be an xcom save: expected file version %d but got %d\n", 
                save_version, hdr.version);
            return{ 0 };
        }
        hdr.uncompressed_size = r.read_int();
        hdr.game_number = r.read_int();
        hdr.save_number = r.read_int();
        hdr.save_description = r.read_string();
        hdr.time = r.read_string();
        hdr.map_command = r.read_string();
        hdr.tactical_save = r.read_bool();
        hdr.ironman = r.read_bool();
        hdr.autosave = r.read_bool();
        hdr.dlc = r.read_string();
        hdr.language = r.read_string();
        uint32_t compressed_crc = (uint32_t)r.read_int();

        // Compute the CRC of the header
        r.seek(xcom_io::seek_kind::start, 1016);
        int32_t hdr_size = r.read_int();
        uint32_t hdr_crc = (uint32_t)r.read_int();

        // CRC the first hdr_size bytes of the buffer
        r.seek(xcom_io::seek_kind::start, 0);
        uint32_t computed_hdr_crc = r.crc(hdr_size);
        if (hdr_crc != computed_hdr_crc)
        {
            throw std::runtime_error("CRC mismatch in header. Bad save?\n");
        }

        // CRC the compressed data
        r.seek(xcom_io::seek_kind::start, compressed_data_start);
        uint32_t computed_compressed_crc = r.crc(r.size() - 1024);
        if (computed_compressed_crc != compressed_crc)
        {
            throw std::runtime_error("CRC mismatch in compressed data. Bad save?\n");
        }
        return hdr;
    }

    actor_table read_actor_table(xcom_io &r)
    {
        actor_table actors;
        int32_t actor_count = r.read_int();

        // We expect all entries to be of the form <package> <0> <actor>
        // <instance>, or two entries per real actor.
        assert(actor_count % 2 == 0);

        for (int i = 0; i < actor_count; i += 2) {
            std::string actor_name = r.read_string();
            int32_t instance = r.read_int();
            if (instance == 0) {
                throw format_exception(r.offset(), 
                        "Malformed actor table entry: expected a non-zero instance\n");
            }
            std::string package = r.read_string();
            int32_t sentinel = r.read_int();
            if (sentinel != 0) {
                throw format_exception(r.offset(), 
                        "Malformed actor table entry: missing 0 instance\n");
            }
            actors.push_back(build_actor_name(package, actor_name, instance));
        }

        return actors;
    }

    property_ptr make_struct_property(xcom_io& r, const std::string &name)
    {
        std::string struct_name = r.read_string();
        int32_t inner_unknown = r.read_int();
        if (inner_unknown != 0) {
            throw format_exception(r.offset(), 
                    "Read non-zero prop unknown value in struct property: %x\n", 
                    inner_unknown);
        }

        // Special case certain structs
        if (struct_name.compare("Vector2D") == 0) {
            return std::make_unique<struct_property>(name, struct_name,
                    r.read_raw_bytes(8), 8);
        }
        else if (struct_name.compare("Vector") == 0) {
            return std::make_unique<struct_property>(name, struct_name, 
                    r.read_raw_bytes(12), 12);
        }
        else {
            property_list structProps = read_properties(r);
            return std::make_unique<struct_property>(name, struct_name, 
                    std::move(structProps));
        }
    }

    // Try to determine what the element type of this array is. Returns one of:
    // struct_array_property, string_array_property, or enum_array property if it
    // can successfully determine what kind of array it is, or last_array_property
    // if it cannot determine.
    static property::kind_t determine_array_property_kind(xcom_io &r,
            uint32_t array_data_size)
    {
        // Save the current position so we can rewind to it
        struct reset_position {
            reset_position(xcom_io &r) : r_(r), ofs_(r.offset()) {}
            ~reset_position() {
                r_.seek(xcom_io::seek_kind::start, ofs_);
            }
            xcom_io& r_;
            size_t ofs_;
        } reset_position(r);

        // Sniff the first part of the array data to see if it looks like a string.
        int32_t str_length = r.read_int();
        if (str_length > 0 && static_cast<size_t>(str_length) < array_data_size) {
            // Skip over the string data.
            r.seek(xcom_io::seek_kind::current, str_length);
            // Look at the next value as an integer. For structs and enums, we expect
            // a zero byte here. For string arrays, the next element begins immediately.
            // If we found a non-zero value this must be an array of strings.
            //
            // Note: embedded empty strings in the array may screw up this detection logic!
            // I haven't encountered this in a real save yet, though. In that case we'll
            // probably have to walk the entire array to distinguish an array of empty
            // strings from an array of enum values.
            int32_t tmp_int = r.read_int();
            if (tmp_int != 0) {
                return property::kind_t::string_array_property;
            }
            str_length = r.read_int();
            if (str_length > 0 && static_cast<size_t>(str_length) < array_data_size) {
                const char * str = reinterpret_cast<const char *>(r.pointer());
                if (str[str_length - 1] == 0 && strlen(str) == (str_length - 1))
                {
                    for (int i = 0; i < static_cast<int>(property::kind_t::last_property); ++i) {
                        if (property_kind_to_string(static_cast<property::kind_t>(i)) == str) {
                            return property::kind_t::struct_array_property;
                        }
                    }
                }
            }

            // Doesn't look like a struct or string. This is likely an enum
            return property::kind_t::enum_array_property;
        }

        // Unknown kind.
        return property::kind_t::last_property;
    }


    property_ptr make_array_property(xcom_io &r, const std::string &name, 
            int32_t property_size)
    {
        int32_t array_bound = r.read_int();
        std::unique_ptr<unsigned char[]> array_data;
        int array_data_size = property_size - 4;
        if (array_data_size > 0) {
            // Try to figure out what's in the array. Some kinds are easy to determine without inspecting
            // the array contents, failing that we need to inspect the contents to try to determine the
            // kind. 
            
            if (array_bound * 8 == array_data_size) {
                // If the array data size is exactly 8x the array bound, we have an array of objects where
                // each element is an actor id.
                std::vector<int32_t> elements;
                for (int32_t i = 0; i < array_bound; ++i) {
                    int32_t actor1 = r.read_int();
                    int32_t actor2 = r.read_int();
                    if (actor1 == -1 && actor2 == -1) {
                        elements.push_back(actor1);
                    }
                    else if (actor1 != (actor2 + 1)) {
                        throw format_exception(r.offset(), 
                            "Assertion failed: expected related actor numbers in object array\n");
                    }
                    else {
                        elements.push_back(actor1 / 2);
                    }
                }
                return std::make_unique<object_array_property>(name, std::move(elements));
            }
            else if (array_bound * 4 == array_data_size) {
                // If the array data size is exactly 4x the number of elements this is an array
                // of numbers. We can't tell if they're ints or floats without looking at the UPK, though.
                // Even guessing based on the numbers themselves is ambiguous for an array of all zeros.
                std::vector<int32_t> elems;
                for (int i = 0; i < array_bound; ++i) {
                    elems.push_back(r.read_int());
                }

                return std::make_unique<number_array_property>(name, std::move(elems));
            }
            else {
                property::kind_t kind = determine_array_property_kind(r, array_data_size);
                switch (kind) {
                case property::kind_t::struct_array_property:
                {
                    std::vector<property_list> elements;
                    for (int32_t i = 0; i < array_bound; ++i) {
                        elements.push_back(read_properties(r));
                    }

                    return std::make_unique<struct_array_property>(name,
                        std::move(elements));
                }
                case property::kind_t::enum_array_property:
                {
                    std::vector<std::string> elements;
                    for (int32_t i = 0; i < array_bound; ++i) {
                        elements.push_back(r.read_string());
                        int32_t tmp = r.read_int();
                        if (tmp != 0) {
                            throw format_exception(r.offset(), "Read non-zero value after string/enum array element\n");
                        }
                    }

                    return std::make_unique<enum_array_property>(name, std::move(elements));
                }
                case property::kind_t::string_array_property:
                {
                    std::vector<xcom_string> elements;
                    for (int32_t i = 0; i < array_bound; ++i) {
                        elements.push_back(r.read_unicode_string());
                    }

                    return std::make_unique<string_array_property>(name, std::move(elements));
                }
                default:
                    // Nope, dunno what this thing is.
                    array_data = r.read_raw_bytes(array_data_size);
                }
            }
        }
        return std::make_unique<array_property>(name, std::move(array_data),
            array_data_size, array_bound);
    }

    property_list read_properties(xcom_io &r)
    {
        property_list properties;
        for (;;)
        {
            std::string name = r.read_string();
            int32_t unknown1 = r.read_int();
            if (unknown1 != 0) {
                throw format_exception(r.offset(), 
                        "Read non-zero property unknown value: %x\n", unknown1);
            }

            if (name.compare("None") == 0) {
                break;
            }
            std::string prop_type = r.read_string();
            int32_t unknown2 = r.read_int();
            if (unknown2 != 0) {
                throw format_exception(r.offset(), 
                        "Read non-zero property unknown2 value: %x\n", unknown2);
            }
            int32_t prop_size = r.read_int();
            int32_t array_index = r.read_int();

            property_ptr prop;
            if (prop_type.compare("ObjectProperty") == 0) {
                assert(prop_size == 8);
                int32_t actor1 = r.read_int();
                int32_t actor2 = r.read_int();
                if (actor1 != -1 && actor1 != (actor2 + 1)) {
                    throw format_exception(r.offset(), 
                            "Assertion failed: actor references in object property not related.\n");
                }

                prop = std::make_unique<object_property>(name, 
                        (actor1 == -1) ? actor1 : (actor1 / 2));
            }
            else if (prop_type.compare("IntProperty") == 0) {
                assert(prop_size == 4);
                int32_t val = r.read_int();
                prop = std::make_unique<int_property>(name, val);
            }
            else if (prop_type.compare("ByteProperty") == 0) {
                std::string enum_type = r.read_string();
                int32_t inner_unknown = r.read_int();
                if (inner_unknown != 0) {
                    throw format_exception(r.offset(), 
                            "Read non-zero enum property unknown value: %x\n", 
                            inner_unknown);
                }
                std::string enum_val = r.read_string();
                int32_t extra_val = r.read_int();
                prop = std::make_unique<enum_property>(name, enum_type, 
                        enum_val, extra_val);
            }
            else if (prop_type.compare("BoolProperty") == 0) {
                assert(prop_size == 0);
                bool val = r.read_byte() != 0;
                prop = std::make_unique<bool_property>(name, val);
            }
            else if (prop_type.compare("ArrayProperty") == 0) {
                prop = make_array_property(r, name, prop_size);
            }
            else if (prop_type.compare("FloatProperty") == 0) {
                float f = r.read_float();
                prop = std::make_unique<float_property>(name, f);
            }
            else if (prop_type.compare("StructProperty") == 0) {
                prop = make_struct_property(r, name);
            }
            else if (prop_type.compare("StrProperty") == 0) {
                xcom_string str = r.read_unicode_string();
                prop = std::make_unique<string_property>(name, str);
            }
            else
            {
                throw format_exception(r.offset(), 
                        "Unknown property type %s\n", prop_type.c_str());
            }

            if (prop.get() != nullptr) {
                assert(prop->size() == prop_size);
                if (array_index == 0) {
                    properties.push_back(std::move(prop));
                }
                else {
                    if (properties.back()->name.compare(name) != 0) {
                        throw format_exception(r.offset(), 
                                "Static array index found but doesn't match previous property\n");
                    }

                    if (properties.back()->kind == property::kind_t::static_array_property) {
                        // We already have a static array. Sanity check the
                        // array index and add it
                        static_array_property *static_array = 
                                static_cast<static_array_property*>(properties.back().get());
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
                        std::unique_ptr<static_array_property> static_array = 
                                std::make_unique<static_array_property>(name);
                        static_array->properties.push_back(std::move(last_property));
                        static_array->properties.push_back(std::move(prop));
                        properties.push_back(std::move(static_array));
                    }
                }
            }
        }

        return properties;
    }

    checkpoint_table read_checkpoint_table(xcom_io &r)
    {
        checkpoint_table checkpoints;
        int32_t checkpoint_count = r.read_int();

        for (int i = 0; i < checkpoint_count; ++i) {
            checkpoint chk;
            chk.name = r.read_string();
            chk.instance_name = r.read_string();
            chk.vector[0] = r.read_float();
            chk.vector[1] = r.read_float();
            chk.vector[2] = r.read_float();
            chk.rotator[0] = r.read_int();
            chk.rotator[1] = r.read_int();
            chk.rotator[2] = r.read_int();
            chk.class_name = r.read_string();
            int32_t prop_length = r.read_int();
            if (prop_length < 0) {
                throw format_exception(r.offset(), "Found negative property length\n");
            }
            chk.pad_size = 0;
            size_t start_offset = r.offset();

            chk.properties = read_properties(r);
            if ((r.offset() - static_cast<int32_t>(start_offset)) < prop_length) {
                chk.pad_size = prop_length - (r.offset() - start_offset);

                for (unsigned int i = 0; i < chk.pad_size; ++i) {
                    if (r.read_byte() != 0) {
                        throw format_exception(r.offset(), "Found non-zero padding byte\n");
                    }
                }
            }
            size_t total_prop_size = 0;
            std::for_each(chk.properties.begin(), chk.properties.end(), 
                    [&total_prop_size](const property_ptr& prop) {
                        total_prop_size += prop->full_size();
                    });

            // length of trailing "None" to terminate the list + the unknown int.
            total_prop_size += 9 + 4; 
            assert((uint32_t)prop_length == (total_prop_size + chk.pad_size));
            chk.template_index = r.read_int();
            checkpoints.push_back(std::move(chk));
        }

        return checkpoints;
    }

    actor_template_table read_actor_template_table(xcom_io &r)
    {
        actor_template_table template_table;
        int32_t templateCount = r.read_int();

        for (int i = 0; i < templateCount; ++i) {
            actor_template tmpl;
            tmpl.actor_class_path = r.read_string();
            r.read_raw_bytes(64, tmpl.load_params);
            tmpl.archetype_path = r.read_string();
            template_table.push_back(std::move(tmpl));
        }

        return template_table;
    }

    name_table read_name_table(xcom_io &r)
    {
        static unsigned char all_zeros[8] = { 0 };
        name_table names;
        int32_t name_count = r.read_int();

        for (int i = 0; i < name_count; ++i) {
            name_entry entry;
            entry.name = r.read_string();
            r.read_raw_bytes(8, entry.zeros);
            if (memcmp(entry.zeros, all_zeros, 8) != 0) {
                throw format_exception(r.offset(), 
                        "Expected all zeros in name table entry\n");
                return{};
            }
            entry.data_length = r.read_int();

            entry.data = r.read_raw_bytes(entry.data_length);
            names.push_back(std::move(entry));
        }

        return names;
    }

    checkpoint_chunk_table read_checkpoint_chunk_table(xcom_io &r)
    {
        checkpoint_chunk_table checkpoints;

        // Read the checkpoint chunks
        do {
            checkpoint_chunk chunk;
            chunk.unknown_int1 = r.read_int();
            chunk.game_type = r.read_string();
            std::string none = r.read_string();
            if (none != "None") {
                throw format_exception(r.offset(),
                    "Failed to locate 'None' after actor table\n");
                return{};
            }

            chunk.unknown_int2 = r.read_int();
            chunk.checkpoints = read_checkpoint_table(r);
            int32_t name_table_length = r.read_int();
            assert(name_table_length == 0);
            chunk.class_name = r.read_string();
            chunk.actors = read_actor_table(r);
            chunk.unknown_int3 = r.read_int();
            // (only seems to be present for tactical saves?)
            actor_template_table actor_templates = read_actor_template_table(r);
            assert(actor_templates.size() == 0);
            chunk.display_name = r.read_string(); //unknown (game name)
            chunk.map_name = r.read_string(); //unknown (map name)
            chunk.unknown_int4 = r.read_int(); //unknown  (checksum?)

            checkpoints.push_back(std::move(chunk));
        } while (!r.eof());

        return checkpoints;
    }

    size_t calculate_uncompressed_size(xcom_io &r)
    {
        // The compressed data begins 1024 bytes into the file.
        //const unsigned char* p = r.start_.get() + 1024;

        size_t compressed_size;
        size_t uncompressed_size = 0;
        r.seek(xcom_io::seek_kind::start, compressed_data_start);

        do
        {
            // Expect the magic header value 0x9e2a83c1 at the start of each chunk
            if (r.read_int() != UPK_Magic) {
                throw format_exception(r.offset(), 
                        "Failed to find compressed chunk header\n");
                return -1;
            }

            // Skip flags
            (void)r.read_int();

            // Compressed size is at p+8
            compressed_size = r.read_int();

            // Uncompressed size is at p+12
            uncompressed_size += r.read_int();

            // Skip to next chunk: include the 8 bytes of header in this chunk we didn't
            // read (which sould be the compressed and uncompressed sizes repeated).
            r.seek(xcom_io::seek_kind::current, compressed_size + 8);
        } while (!r.eof());

        return uncompressed_size;
    }

    buffer<unsigned char> decompress(xcom_io &r)
    {
        size_t uncompressed_size = calculate_uncompressed_size(r);
        if (uncompressed_size < 0) {
            throw format_exception(r.offset(), "Found no uncompressed data in save.\n");
        }

        std::unique_ptr<unsigned char[]> buf = std::make_unique<unsigned char[]>(uncompressed_size);
        // Start back at the beginning of the compressed data.
        r.seek(xcom_io::seek_kind::start, compressed_data_start);
        
        unsigned char *outp = buf.get();
        do
        {
            // Expect the magic header value 0x9e2a83c1 at the start of each chunk
            if (r.read_int() != UPK_Magic) {
                throw format_exception(r.offset(), 
                        "Failed to find compressed chunk header\n");
            }

            // Skip unknown int (flags?)
            (void)r.read_int();

            // Compressed size is at p+8
            int32_t compressed_size = r.read_int();

            // Uncompressed size is at p+12
            int32_t uncompressed_size = r.read_int();

            unsigned long decomp_size = uncompressed_size;

            if (lzo1x_decompress_safe(r.pointer() + 8, compressed_size, outp, 
                                      &decomp_size, nullptr) != LZO_E_OK) {
                throw format_exception(r.offset(), 
                        "LZO decompress of save data failed\n");
            }

            if (decomp_size != uncompressed_size)
            {
                throw format_exception(r.offset(), "Failed to decompress chunk\n");
            }

            // Skip to next chunk - 24 bytes of this chunk header +
            // compressedSize bytes later.
            r.seek(xcom_io::seek_kind::current, compressed_size + 8);
            outp += uncompressed_size;
        } while (!r.eof());

        return{ std::move(buf), uncompressed_size };
    }

    buffer<unsigned char> read_file(const std::string& filename)
    {
        buffer<unsigned char> buffer;
        FILE *fp = fopen(filename.c_str(), "rb");
        if (fp == nullptr) {
            throw std::runtime_error("Error opening file\n");
        }

        if (fseek(fp, 0, SEEK_END) != 0) {
            throw std::runtime_error("Error determining file length\n");
        }

        buffer.length = ftell(fp);

        if (fseek(fp, 0, SEEK_SET) != 0) {
            throw std::runtime_error("Error determining file length\n");
        }

        buffer.buf = std::make_unique<unsigned char[]>(buffer.length);
        if (fread(buffer.buf.get(), 1, buffer.length, fp) != buffer.length) {
            throw std::runtime_error("Error reading file contents\n");
        }

        fclose(fp);
        return buffer;
    }

    saved_game read_xcom_save(buffer<unsigned char>&& b)
    {
        saved_game save;

        xcom_io rdr{ std::move(b) };
        save.hdr = read_header(rdr);
        xcom_io uncompressed(decompress(rdr));
        save.actors = read_actor_table(uncompressed);
        save.checkpoints = read_checkpoint_chunk_table(uncompressed);

        return save;
    }

    saved_game read_xcom_save(const std::string &infile)
    {
        return read_xcom_save(read_file(infile));
    }



} //namespace xcom
