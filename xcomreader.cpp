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
#include "zlib.h"
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

    property_list read_properties(xcom_io &r, xcom_version version);

    header read_header(xcom_io &r)
    {
        header hdr;
        hdr.version = static_cast<xcom_version>(r.read_int());
        if (!supported_version(hdr.version)) {
            throw error::unsupported_version(hdr.version);
        }

        hdr.uncompressed_size = r.read_int();
        hdr.game_number = r.read_int();
        hdr.save_number = r.read_int();
        hdr.save_description = r.read_unicode_string();
        hdr.time = r.read_unicode_string();
        hdr.map_command = r.read_string();
        hdr.tactical_save = r.read_bool();
        hdr.ironman = r.read_bool();
        hdr.autosave = r.read_bool();
        hdr.dlc = r.read_string();
        hdr.language = r.read_string();
        uint32_t compressed_crc = (uint32_t)r.read_int();

        // The Android version has two additional fields in the header, 12 bytes after the checksum
        if (hdr.version == xcom_version::enemy_within_android) {
            r.seek(xcom_io::seek_kind::current, 12);
            hdr.profile_number = r.read_int();
            hdr.profile_date = r.read_unicode_string();
        }

        // Compute the CRC of the header
        // Note the android version includes only a single checksum over the compressed data,
        // the header is not checked.
        if (hdr.version != xcom_version::enemy_within_android) {
            r.seek(xcom_io::seek_kind::start, 1016);
            int32_t hdr_size = r.read_int();
            uint32_t hdr_crc = (uint32_t)r.read_int();

            // CRC the first hdr_size bytes of the buffer
            r.seek(xcom_io::seek_kind::start, 0);
            uint32_t computed_hdr_crc = r.crc(hdr_size);
            if (hdr_crc != computed_hdr_crc)
            {
                throw error::crc_mismatch(hdr_crc, computed_hdr_crc, true);
            }
        }

        // CRC the compressed data
        r.seek(xcom_io::seek_kind::start, compressed_data_start);
        uint32_t computed_compressed_crc = r.crc(r.size() - compressed_data_start);
        if (computed_compressed_crc != compressed_crc)
        {
            throw error::crc_mismatch(compressed_crc, computed_compressed_crc, false);
        }
        return hdr;
    }

    actor_table read_actor_table(xcom_io &r, xcom_version version)
    {
        actor_table actors;
        int32_t actor_count = r.read_int();

        // We expect all entries to be of the form <package> <0> <actor>
        // <instance>, or two entries per real actor.
        //can this assert be on the EU version? asssuming no:
        if(xcom_version::enemy_unknown != version)
        {
            assert(actor_count % 2 == 0);
        }

        const int increment= (xcom_version::enemy_unknown == version)? 1 : 2;
        
        for (int i = 0; i < actor_count; i += increment) {
            std::string actor_name = r.read_string();
            int32_t instance = r.read_int();

            if(xcom_version::enemy_unknown == version)
            {
                actors.push_back(build_actor_name_EU(actor_name, instance));
            }
            else
            {
                if (instance == 0) {
                    throw error::format_exception(r.offset(),
                            "malformed actor table entry: expected a non-zero instance");
                }
                std::string package = r.read_string();
                int32_t sentinel = r.read_int();
                if (sentinel != 0) {
                    throw error::format_exception(r.offset(),
                            "malformed actor table entry: missing 0 instance");
                }
                actors.push_back(build_actor_name(package, actor_name, instance));
            }
        }

        return actors;
    }

    property_ptr make_struct_property(xcom_io& r, const std::string &name, xcom_version version)
    {
        std::string struct_name = r.read_string();
        int32_t inner_unknown = r.read_int();
        if (inner_unknown != 0) {
            throw error::format_exception(r.offset(),
                    "read non-zero prop unknown value in struct property: %x",
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
        else if (struct_name.compare("Rotator") == 0) {
            return std::make_unique<struct_property>(name, struct_name,
                r.read_raw_bytes(12), 12);
        }
        else if (struct_name.compare("Box") == 0) {
            // A "box" type. Unknown contents but always 25 bytes long
            return std::make_unique<struct_property>(name, struct_name,
                r.read_raw_bytes(25), 25);
        }
        else if (struct_name.compare("Color") == 0) {
            // A Color type. Unknown contents (4 bytes)
            return std::make_unique<struct_property>(name, struct_name,
                r.read_raw_bytes(4), 4);
        }
        else {
            property_list structProps = read_properties(r, version);
            return std::make_unique<struct_property>(name, struct_name,
                    std::move(structProps));
        }
    }

    // Try to determine what the element type of this array is. Returns one of:
    // struct_array_property, string_array_property, or enum_array property if it
    // can successfully determine what kind of array it is, or last_array_property
    // if it cannot determine.
    static property::kind_t determine_array_property_kind(xcom_io &r)
    {
        // Save the current position so we can rewind to it
        struct reset_position {
            reset_position(xcom_io &r) : r_(r), ofs_(r.offset()) {}
            ~reset_position() {
                r_.seek(xcom_io::seek_kind::start, static_cast<int32_t>(ofs_));
            }
            xcom_io& r_;
            size_t ofs_;
        } reset_position(r);

        // Sniff the first part of the array data to see if it looks like a string.
        xcom_string s = r.read_unicode_string(false);

        if (s.str.length() == 0)
        {
            // Not a string, we don't know what this thing is.
            return property::kind_t::last_property;
        }

        // If the first thing we get is a "None", we have an ambiguity. This could be
        // a struct array (as in XGExaltSimulation.m_arrCellData) where the "None"
        // indicates a struct element with all default property values. Or it could be
        // an enum array (as in XGExaltSimulation.m_arrCellLastVisibilityData) where "None"
        // presumably indicates the 0 value of the enumeration?
        //
        // Either way, we don't know how to parse this yet. There should be a 0 int after
        // the "None" to complete this element. Read that and then recursively call this
        // function to try to determine based on the next array element.
        //
        // I'm not sure if it's possible to have an array element here with "None" in every
        // element. If so this will eventually run off the end of the array and we'd probably
        // fail to parse the rest of the file. If that happens though we are hosed anyway
        // as we still won't know whether this is an struct or enum array.
        if (s.str.compare("None") == 0)
        {
            r.read_int();
            return determine_array_property_kind(r);
           // return property::kind_t::struct_array_property;
        }


        // Try to read another string. If we find a non-zero length string this must be
        // an array of strings. Otherwise it's likely an array of enums or structs.
        {
            struct reset_position reset(r);
            xcom_string s = r.read_unicode_string(false);
            if (s.str.length() > 0) {
                return property::kind_t::string_array_property;
            }
        }

        // We didn't find a string. It should've been an int (0 for structs, or an enum value for
        // enums).
        (void)r.read_int();

        // Now we should have a string: Either the next enum value for an enum or a property kind
        // string for a struct property.
        s = r.read_unicode_string(false);
        if (s.str.length() > 0) {
            for (int i = 0; i < static_cast<int>(property::kind_t::last_property); ++i) {
                if (property_kind_to_string(static_cast<property::kind_t>(i)) == s.str) {
                    return property::kind_t::struct_array_property;
                }
            }

            // Not a property name, this should be an enum.
            return property::kind_t::enum_array_property;
        }

        // Not a string, we don't know what this thing is.
        return property::kind_t::last_property;

    }


    property_ptr make_array_property(xcom_io &r, const std::string &name,
            int32_t property_size, xcom_version version)
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
                        throw error::format_exception(r.offset(),
                            "expected related actor numbers in object array");
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
                property::kind_t kind = determine_array_property_kind(r);
                switch (kind) {
                case property::kind_t::struct_array_property:
                {
                    std::vector<property_list> elements;
                    for (int32_t i = 0; i < array_bound; ++i) {
                        elements.push_back(read_properties(r, version));
                    }

                    return std::make_unique<struct_array_property>(name,
                        std::move(elements));
                }
                case property::kind_t::enum_array_property:
                {
                    std::vector<enum_value> elements;
                    for (int32_t i = 0; i < array_bound; ++i) {
                        std::string name = r.read_string();
                        int32_t value = r.read_int();
                        elements.push_back({ name, value });
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

    property_list read_properties(xcom_io &r, xcom_version version)
    {
        property_list properties;
        for (;;)
        {
            std::string name = r.read_string();
            int32_t unknown1 = r.read_int();
            if (unknown1 != 0) {
                throw error::format_exception(r.offset(),
                        "read non-zero property unknown value: %x", unknown1);
            }

            if (name.compare("None") == 0) {
                break;
            }

            std::string prop_type = r.read_string();
            int32_t unknown2 = r.read_int();
            if (unknown2 != 0) {
                throw error::format_exception(r.offset(),
                        "read non-zero property unknown2 value: %x", unknown2);
            }
            int32_t prop_size = r.read_int();
            int32_t array_index = r.read_int();

            property_ptr prop;
            if (prop_type.compare("ObjectProperty") == 0) {
                if(xcom_version::enemy_unknown == version)
                {
                    assert(prop_size == 4);
                     int32_t actor = r.read_int();
                     prop = std::make_unique<object_property_EU>(name, actor);
                }
                else
                {
                    assert(prop_size == 8);
                    int32_t actor1 = r.read_int();
                    int32_t actor2 = r.read_int();
                    if (actor1 != -1 && actor1 != (actor2 + 1)) {
                        throw error::format_exception(r.offset(),
                                "actor references in object property not related");
                    }
                    prop = std::make_unique<object_property>(name,
                            (actor1 == -1) ? actor1 : (actor1 / 2));
                }
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
                    throw error::format_exception(r.offset(),
                            "read non-zero enum property unknown value: %x",
                            inner_unknown);
                }
                if (enum_type == "None") {
                    // Sigh. ByteProperty can be an enum value, or can just
                    // be a raw byte if the "type" field is "None". Read just
                    // a single byte and use that as the "extra" value.
                    unsigned char c = r.read_byte();
                    prop = std::make_unique<enum_property>(name, enum_type,
                        "None", c);
                }
                else {
                    std::string enum_val = r.read_string();
                    int32_t extra_val = r.read_int();
                    prop = std::make_unique<enum_property>(name, enum_type,
                        enum_val, extra_val);
                }
            }
            else if (prop_type.compare("BoolProperty") == 0) {
                assert(prop_size == 0);
                bool val = r.read_byte() != 0;
                prop = std::make_unique<bool_property>(name, val);
            }
            else if (prop_type.compare("ArrayProperty") == 0) {
                prop = make_array_property(r, name, prop_size, version);
            }
            else if (prop_type.compare("FloatProperty") == 0) {
                float f = r.read_float();
                prop = std::make_unique<float_property>(name, f);
            }
            else if (prop_type.compare("StructProperty") == 0) {
                prop = make_struct_property(r, name, version);
            }
            else if (prop_type.compare("StrProperty") == 0) {
                xcom_string str = r.read_unicode_string();
                prop = std::make_unique<string_property>(name, str);
            }
            else if (prop_type.compare("NameProperty") == 0) {
                std::string str = r.read_string();
                int32_t number = r.read_int();
                prop = std::make_unique<name_property>(name, str, number);
            }
            else
            {
                throw error::format_exception(r.offset(),
                        "unknown property type %s", prop_type.c_str());
            }

            if (prop.get() != nullptr) {
                assert(prop->size() == prop_size);
                if (array_index == 0) {
                    properties.push_back(std::move(prop));
                }
                else {
#if 0
                    if (properties.back()->name.compare(name) != 0) {
                        throw format_exception(r.offset(),
                                "Static array index found but doesn't match previous property\n");
                    }
#endif

                    if (properties.back()->kind == property::kind_t::static_array_property) {
                        // We already have a static array. Sanity check the
                        // array index and add it
                        static_array_property *static_array =
                                static_cast<static_array_property*>(properties.back().get());
#if 0
                        assert(array_index == static_array->properties.size());
#endif
                        static_array->properties.push_back(std::move(prop));
                    }
                    else {
                        // Not yet a static array. This new property should have index 1.
#if 0
                        assert(array_index == 1);
#endif

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

    checkpoint_table read_checkpoint_table(xcom_io &r, xcom_version version)
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
                throw error::format_exception(r.offset(), "found negative property length");
            }
            chk.pad_size = 0;
            size_t start_offset = r.offset();

            chk.properties = read_properties(r, version);
            if ((r.offset() - static_cast<int32_t>(start_offset)) < prop_length) {
                chk.pad_size = static_cast<int32_t>(prop_length - (r.offset() - start_offset));

                for (unsigned int i = 0; i < chk.pad_size; ++i) {
                    if (r.read_byte() != 0) {
                        throw error::format_exception(r.offset(), "found non-zero padding byte");
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
                throw error::format_exception(r.offset(),
                        "expected all zeros in name table entry");
            }
            entry.data_length = r.read_int();

            entry.data = r.read_raw_bytes(entry.data_length);
            names.push_back(std::move(entry));
        }

        return names;
    }

    checkpoint_chunk_table read_checkpoint_chunk_table(xcom_io &r, xcom_version version)
    {
        checkpoint_chunk_table checkpoints;
        std::vector<name_table> name_tables;
        std::vector<actor_template_table> actor_templates;
        // Read the checkpoint chunks
        do {
            checkpoint_chunk chunk;
            chunk.unknown_int1 = r.read_int();
            chunk.game_type = r.read_string();
            std::string none = r.read_string();
            if (none != "None") {
                throw error::format_exception(r.offset(),
                    "failed to locate 'None' after actor table");
            }

            chunk.unknown_int2 = r.read_int();
            chunk.checkpoints = read_checkpoint_table(r, version);
            int32_t name_table_length = r.read_int();
           // assert(name_table_length == 0);
            //TODO
            if (name_table_length > 0) {
                name_tables.push_back(read_name_table(r));
            }
            chunk.class_name = r.read_string();
            chunk.actors = read_actor_table(r, version);
            chunk.unknown_int3 = r.read_int();
            // (only seems to be present for tactical saves?)
            actor_templates.push_back(read_actor_template_table(r));
           // assert(actor_templates.size() == 0);
            chunk.display_name = r.read_string(); //unknown (game name)
            chunk.map_name = r.read_string(); //unknown (map name)
            chunk.unknown_int4 = r.read_int(); //unknown  (checksum?)

            checkpoints.push_back(std::move(chunk));
        } while (!r.eof());

        return checkpoints;
    }

    int32_t calculate_uncompressed_size(xcom_io &r)
    {
        // The compressed data begins 1024 bytes into the file.
        //const unsigned char* p = r.start_.get() + 1024;

        int32_t compressed_size;
        int32_t uncompressed_size = 0;
        r.seek(xcom_io::seek_kind::start, compressed_data_start);

        do
        {
            // Expect the magic header value 0x9e2a83c1 at the start of each chunk
            if (r.read_int() != UPK_Magic) {
                throw error::format_exception(r.offset(),
                        "failed to find compressed chunk header");
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

    uint32_t decompress_one_chunk(xcom_version version, const unsigned char *compressed_start, unsigned long compressed_size, unsigned char *decompressed_start, unsigned long decompressed_size)
    {
        switch (version)
        {
            case xcom_version::enemy_within:
            {
                lzo_init();
                lzo_uint out_decompressed_size = decompressed_size;
                if (lzo1x_decompress_safe(compressed_start, compressed_size, decompressed_start,
                    &out_decompressed_size, nullptr) != LZO_E_OK) {
                        throw xcom::error::general_exception("LZO decompress of save data failed");
                }

                return static_cast<uint32_t>(out_decompressed_size);
            }

            case xcom_version::enemy_within_android:
            {
                z_stream stream;
                stream.zalloc = Z_NULL;
                stream.zfree = Z_NULL;
                stream.opaque = Z_NULL;
                stream.avail_in = compressed_size;
                stream.next_in = (Bytef*)compressed_start;
                stream.avail_out = decompressed_size;
                stream.next_out = (Bytef*)decompressed_start;
                inflateInit(&stream);
                inflate(&stream, Z_NO_FLUSH);
                inflateEnd(&stream);
                return stream.total_out;
            }
            break;

            default:
                throw error::unsupported_version(version);
        }
    }

    buffer<unsigned char> decompress(xcom_io &r, xcom_version version)
    {
        int32_t total_uncompressed_size = calculate_uncompressed_size(r);
        if (total_uncompressed_size < 0) {
            throw error::format_exception(r.offset(), "found no uncompressed data in save");
        }

        std::unique_ptr<unsigned char[]> buf = std::make_unique<unsigned char[]>(total_uncompressed_size);
        // Start back at the beginning of the compressed data.
        r.seek(xcom_io::seek_kind::start, compressed_data_start);

        unsigned char *outp = buf.get();
        int32_t bytes_remaining = total_uncompressed_size;

        do
        {
            // Expect the magic header value 0x9e2a83c1 at the start of each chunk
            if (r.read_int() != UPK_Magic) {
                throw error::format_exception(r.offset(),
                        "failed to find compressed chunk header");
            }

            // Skip unknown int (flags?)
            (void)r.read_int();

            // Compressed size is at p+8
            int32_t compressed_size = r.read_int();

            // Uncompressed size is at p+12
            int32_t uncompressed_size = r.read_int();
            uint32_t decomp_size = decompress_one_chunk(version, r.pointer() + 8, compressed_size, outp, bytes_remaining);
            if (static_cast<int32_t>(decomp_size) != uncompressed_size)
            {
                throw error::format_exception(r.offset(), "failed to decompress chunk");
            }

            // Skip to next chunk - 24 bytes of this chunk header +
            // compressedSize bytes later.
            r.seek(xcom_io::seek_kind::current, compressed_size + 8);
            outp += uncompressed_size;
            bytes_remaining -= uncompressed_size;
        } while (!r.eof());

        return{ std::move(buf), static_cast<size_t>(total_uncompressed_size) };
    }

    buffer<unsigned char> read_file(const std::string& filename)
    {
        buffer<unsigned char> buffer;
        FILE *fp = fopen(filename.c_str(), "rb");
        if (fp == nullptr) {
            throw xcom::error::general_exception("error opening file");
        }

        if (fseek(fp, 0, SEEK_END) != 0) {
            throw xcom::error::general_exception("error determining file length");
        }

        buffer.length = ftell(fp);

        if (fseek(fp, 0, SEEK_SET) != 0) {
            throw xcom::error::general_exception("error determining file length");
        }

        buffer.buf = std::make_unique<unsigned char[]>(buffer.length);
        if (fread(buffer.buf.get(), 1, buffer.length, fp) != buffer.length) {
            throw xcom::error::general_exception("error reading file contents");
        }

        fclose(fp);
        return buffer;
    }

    saved_game read_xcom_save(buffer<unsigned char>&& b)
    {
        saved_game save;

        xcom_io rdr{ std::move(b) };
        save.hdr = read_header(rdr);
        buffer<unsigned char> uncompressed_buf = decompress(rdr, static_cast<xcom_version>(save.hdr.version));
#ifdef _DEBUG
        FILE *fp = fopen("output.dat", "wb");
        fwrite(uncompressed_buf.buf.get(), 1, uncompressed_buf.length, fp);
        fclose(fp);
#endif
        xcom_io uncompressed(std::move(uncompressed_buf));
        save.actors = read_actor_table(uncompressed, save.hdr.version);
        save.checkpoints = read_checkpoint_chunk_table(uncompressed, save.hdr.version);

        return save;
    }

    saved_game read_xcom_save(const std::string &infile)
    {
        return read_xcom_save(read_file(infile));
    }

} //namespace xcom
