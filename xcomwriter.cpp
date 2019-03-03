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

#include "xcomio.h"
#include "minilzo.h"
#include "zlib.h"
#include <cassert>
#include <cstring>
#include <tuple>

namespace xcom
{
    struct property_writer_visitor;

    static void write_property(xcom_io &w, const property_ptr& prop, int32_t array_index);

    static void write_header(xcom_io& w, const header& hdr)
    {
        w.write_int(static_cast<uint32_t>(hdr.version));
        w.write_int(0);
        w.write_int(hdr.game_number);
        w.write_int(hdr.save_number);
        w.write_unicode_string(hdr.save_description);
        w.write_unicode_string(hdr.time);
        w.write_string(hdr.map_command);
        w.write_bool(hdr.tactical_save);
        w.write_bool(hdr.ironman);
        w.write_bool(hdr.autosave);
        w.write_string(hdr.dlc);
        w.write_string(hdr.language);

        ptrdiff_t offset = w.offset();
        // Compute the CRC for the compressed data.
        w.seek(xcom_io::seek_kind::start, 1024);
        uint32_t compressed_crc = w.crc(w.size() - 1024);
        w.seek(xcom_io::seek_kind::start, offset);
        w.write_int(compressed_crc);

        // Write the profile information (android only)
        if (hdr.version == xcom_version::enemy_within_android) {
            // Profile info is 12 bytes after the CRC
            w.seek(xcom_io::seek_kind::current, 12);
            w.write_int(hdr.profile_number);
            w.write_unicode_string(hdr.profile_date);
        }

        // Compute the CRC for the header (except android)
        if (hdr.version != xcom_version::enemy_within_android) {
            int32_t hdr_length = static_cast<int32_t>(w.offset() + 4);

            w.seek(xcom_io::seek_kind::start, 0);
            uint32_t hdr_crc = w.crc(hdr_length);

            w.seek(xcom_io::seek_kind::start, 1016);
            w.write_int(hdr_length);
            w.write_int(hdr_crc);
        }
    }

    static void write_actor_table(xcom_io& w, const actor_table& actors)
    {
        // Each actorTable entry has 2 entries in the save table; names are split.
        w.write_int(static_cast<int32_t>(actors.size() * 2));
        for (const std::string& actor : actors) {
            std::tuple<std::string, std::string, int> tup = decompose_actor_name(actor);
            w.write_string(std::get<1>(tup));
            w.write_int(std::get<2>(tup));
            w.write_string(std::get<0>(tup));
            w.write_int(0);
        }
    }

    static void write_actor_table_EU(xcom_io& w, const actor_table& actors)
    {
        // Each actorTable entry has 2 entries in the save table; names are split.
        w.write_int(static_cast<int32_t>(actors.size() ) );
        for (const std::string& actor : actors) {
            std::tuple<std::string, int> tup = decompose_actor_name_EU(actor);
            w.write_string(std::get<0>(tup));
             w.write_int(std::get<1>(tup));
        }
    }
    
    
    struct property_writer_visitor : public property_visitor
    {
        property_writer_visitor(xcom_io& w) : io_(w) {}

        virtual void visit(int_property* prop) override
        {
            io_.write_int(prop->value);
        }

        virtual void visit(float_property *prop) override
        {
            io_.write_float(prop->value);
        }

        virtual void visit(bool_property *prop) override
        {
            io_.ensure(1);
            io_.write_byte(prop->value);
        }

        virtual void visit(string_property *prop) override
        {
            io_.write_unicode_string(prop->str);
        }

        virtual void visit(name_property *prop) override
        {
            io_.write_string(prop->str);
            io_.write_int(prop->number);
        }

        virtual void visit(object_property *prop) override
        {
            //enemy unknown
            if(4 == prop->size() )
            {
                io_.write_int(prop->actor);
            }
            else
            {
                if (prop->actor == -1) {
                    io_.write_int(prop->actor);
                    io_.write_int(prop->actor);
                }
                else {
                    io_.write_int(prop->actor * 2 + 1);
                    io_.write_int(prop->actor * 2);
                }
            }
        }

        virtual void visit(enum_property *prop) override
        {
            io_.write_string(prop->type);
            io_.write_int(0);
            if (prop->type == "None") {
                io_.write_byte(prop->value.number);
            }
            else {
                io_.write_string(prop->value.name);
                io_.write_int(prop->value.number);
            }
        }

        virtual void visit(struct_property *prop) override
        {
            io_.write_string(prop->struct_name);
            io_.write_int(0);
            if (prop->native_data_length > 0) {
                io_.write_raw(prop->native_data.get(), static_cast<int32_t>(prop->native_data_length));
            }
            else {
                for (unsigned int i = 0; i < prop->properties.size(); ++i) {
                    write_property(io_, prop->properties[i], 0);
                }
                io_.write_string("None");
                io_.write_int(0);
            }
        }

        virtual void visit(array_property *prop) override
        {
            io_.write_int(prop->array_bound);
            int32_t data_length = prop->size() - 4;
            io_.write_raw(prop->data.get(), data_length);
        }

        virtual void visit(object_array_property *prop) override
        {
            io_.write_int(static_cast<int32_t>(prop->elements.size()));
            for (size_t i = 0; i < prop->elements.size(); ++i) {
                if (prop->elements[i] == -1) {
                    io_.write_int(prop->elements[i]);
                    io_.write_int(prop->elements[i]);
                }
                else {
                    io_.write_int(prop->elements[i] * 2 + 1);
                    io_.write_int(prop->elements[i] * 2);
                }
            }
        }

        virtual void visit(number_array_property *prop) override
        {
            io_.write_int(static_cast<int32_t>(prop->elements.size()));
            for (size_t i = 0; i < prop->elements.size(); ++i) {
                io_.write_int(prop->elements[i]);
            }
        }

        virtual void visit(string_array_property *prop) override
        {
            io_.write_int(static_cast<int32_t>(prop->elements.size()));
            for (size_t i = 0; i < prop->elements.size(); ++i) {
                io_.write_unicode_string(prop->elements[i]);
            }
        }

        virtual void visit(enum_array_property* prop) override
        {
            io_.write_int(static_cast<int32_t>(prop->elements.size()));
            for (size_t i = 0; i < prop->elements.size(); ++i) {
                io_.write_string(prop->elements[i].name);
                io_.write_int(prop->elements[i].number);
            }
        }

        virtual void visit(struct_array_property *prop) override
        {
            io_.write_int(static_cast<int32_t>(prop->elements.size()));
            std::for_each(prop->elements.begin(), prop->elements.end(),
                [this](const property_list &pl) {
                    std::for_each(pl.begin(), pl.end(),
                        [this](const property_ptr& p) {
                            write_property(io_, p, 0);
                        });

                    // Write the "None" to indicate the end of this struct.
                    io_.write_string("None");
                    io_.write_int(0);
                });
        }

        virtual void visit(static_array_property *) override
        {
            // This shouldn't happen: static arrays need special handling and
            // can't be written normally as they don't really exist in the save
            // format.
            throw xcom::error::general_exception("attempted to write a static array property");
        }

    private:
        xcom_io& io_;
    };

    static void write_property(xcom_io &w, const property_ptr& prop, int32_t array_index)
    {
        // If this is a static array property we need to write only the
        // contained properties, not the fake static array property created to
        // contain it.
        if (prop->kind == property::kind_t::static_array_property) {
            static_array_property* static_array =
                dynamic_cast<static_array_property*>(prop.get());
            for (unsigned int idx = 0; idx < static_array->properties.size(); ++idx) {
                write_property(w, static_array->properties[idx], idx);
            }
        }
        else {
            // Write the common part of a property
            w.write_string(prop->name);
            w.write_int(0);
            w.write_string(prop->kind_string());
            w.write_int(0);
            w.write_int(prop->size());
            w.write_int(array_index);

            // Write the specific part
            property_writer_visitor v{ w };
            prop->accept(&v);
        }
    }

    static void write_checkpoint(xcom_io& w, const checkpoint& chk)
    {
        w.write_string(chk.name);
        w.write_string(chk.instance_name);
        w.write_float(chk.vector[0]);
        w.write_float(chk.vector[1]);
        w.write_float(chk.vector[2]);
        w.write_int(chk.rotator[0]);
        w.write_int(chk.rotator[1]);
        w.write_int(chk.rotator[2]);
        w.write_string(chk.class_name);
        int32_t total_property_size = 0;
        std::for_each(chk.properties.begin(), chk.properties.end(),
            [&total_property_size](const property_ptr& prop) {
                total_property_size += prop->full_size();
            });
        // length of trailing "None" to terminate the list + the unknown int.
        total_property_size += 9 + 4;
        total_property_size += chk.pad_size;
        w.write_int(total_property_size);
        for (unsigned int i = 0; i < chk.properties.size(); ++i) {
            write_property(w, chk.properties[i], 0);
        }
        w.write_string("None");
        w.write_int(0);
        w.ensure(chk.pad_size);
        for (unsigned int i = 0; i < chk.pad_size; ++i) {
            w.write_byte(0);
        }
        w.write_int(chk.template_index);
    }

    static void write_checkpoint_table(xcom_io &w, const checkpoint_table& table)
    {
        w.write_int(static_cast<int32_t>(table.size()));
        for (const checkpoint& chk : table) {
            write_checkpoint(w, chk);
        }
    }

    static void write_checkpoint_chunk(xcom_io & w, const checkpoint_chunk& chunk, xcom_version version)
    {
        w.write_int(chunk.unknown_int1);
        w.write_string(chunk.game_type);
        w.write_string("None");
        w.write_int(chunk.unknown_int2);
        write_checkpoint_table(w, chunk.checkpoints);
        w.write_int(0); // name table length
        w.write_string(chunk.class_name);
        if(xcom_version::enemy_unknown != version)
        {
            write_actor_table(w, chunk.actors);
        }
        else
        {
            write_actor_table_EU(w, chunk.actors);
        }
        w.write_int(chunk.unknown_int3);
        w.write_int(0); // actor template table length
        w.write_string(chunk.display_name);
        w.write_string(chunk.map_name);
        w.write_int(chunk.unknown_int4);
    }

    static void write_checkpoint_chunks(xcom_io &w, const checkpoint_chunk_table& chunks, xcom_version version)
    {
        for (const checkpoint_chunk& chunk : chunks) {
            write_checkpoint_chunk(w, chunk, version);
        }
    }

    static char lzo_work_buffer[LZO1X_1_MEM_COMPRESS];

    unsigned long compress_one_chunk(xcom_version version, const unsigned char *chunk_start, unsigned long chunk_size, unsigned char *output_start, unsigned long output_size)
    {
        switch (version)
        {
            case xcom_version::enemy_within:
            {
                unsigned long bytes_compressed = output_size;
                std::unique_ptr<char[]> wrkMem = std::make_unique<char[]>(LZO1X_1_MEM_COMPRESS);

                lzo_init();
                lzo_uint out_compressed_size = bytes_compressed;
                if (lzo1x_1_compress(chunk_start, chunk_size,
                    output_start, &out_compressed_size, lzo_work_buffer) != LZO_E_OK) {
                    throw xcom::error::general_exception("failed to compress chunk");
                }
                return static_cast<unsigned long>(out_compressed_size);
            }

            case xcom_version::enemy_within_android:
            {
                z_stream stream;
                stream.zalloc = Z_NULL;
                stream.zfree = Z_NULL;
                stream.opaque = Z_NULL;
                stream.avail_in = chunk_size;
                stream.next_in = (Bytef*)chunk_start;
                stream.avail_out = output_size;
                stream.next_out = (Bytef*)(output_start);
                deflateInit(&stream, Z_BEST_COMPRESSION);
                deflate(&stream, Z_FINISH);
                deflateEnd(&stream);
                return stream.total_out;
            }

            default:
                throw xcom::error::unsupported_version(version);
        }
    }

    buffer<unsigned char> compress(xcom_io &w, xcom_version version)
    {
        int32_t total_in_size = static_cast<int32_t>(w.offset());
        // Allocate a new buffer to hold the compressed data. Just allocate as
        // much as the uncompressed buffer since we don't know how big it will
        // be, but it'll presumably be smaller.
        buffer<unsigned char> b;
        b.buf = std::make_unique<unsigned char[]>(total_in_size);

        // Compress the data in 128k chunks
        static const int max_chunk_size = 0x20000;

        // The "flags" (?) value is always 20000, even for trailing chunks
        static const int chunk_flags = 0x20000;
        w.seek(xcom_io::seek_kind::start, 0);
        const unsigned char *chunk_start = w.pointer();
        // Reserve 1024 bytes at the start of the compressed buffer for the header.
        unsigned char *output_ptr = b.buf.get() + 1024;
        int bytes_left = total_in_size;
        int total_out_size = 1024;

        do
        {
            int chunk_size = (bytes_left < max_chunk_size) ? bytes_left : max_chunk_size;

            // Compress the next chunk, reserving 24 bytes from the current output position for the chunk header.
            unsigned long bytes_compressed = compress_one_chunk(version, chunk_start, chunk_size, output_ptr + 24, bytes_left);

            // Write the magic number
            *reinterpret_cast<int*>(output_ptr) = UPK_Magic;
            output_ptr += 4;
            // Write the "flags" (?)
            *reinterpret_cast<int*>(output_ptr) = chunk_flags;
            output_ptr += 4;
            // Write the compressed size
            *reinterpret_cast<int*>(output_ptr) = bytes_compressed;
            output_ptr += 4;
            // Write the uncompressed size of this chunk
            *reinterpret_cast<int*>(output_ptr) = chunk_size;
            output_ptr += 4;
            // Write the compressed size
            *reinterpret_cast<int*>(output_ptr) = bytes_compressed;
            output_ptr += 4;
            // Write the uncompressed size
            *reinterpret_cast<int*>(output_ptr) = chunk_size;
            output_ptr += 4;

            // Skip over the compressed chunk we wrote
            output_ptr += bytes_compressed;

            bytes_left -= chunk_size;
            chunk_start += chunk_size;
            total_out_size += bytes_compressed + 24;
        } while (bytes_left > 0);

        b.length = total_out_size;
        return b;
    }

    buffer<unsigned char> write_xcom_save(const saved_game &save)
    {
        xcom_io w{};

        if (!supported_version(save.hdr.version)) {
            throw xcom::error::unsupported_version(save.hdr.version);
        }
        if(xcom_version::enemy_unknown == save.hdr.version)
        {
            write_actor_table_EU(w, save.actors);
        }
        else
        {
            write_actor_table(w, save.actors);
        }
        write_checkpoint_chunks(w, save.checkpoints, save.hdr.version);
        xcom_io compressed{ compress(w, save.hdr.version) };
        write_header(compressed, save.hdr);
        return compressed.release();
    }

    void write_xcom_save(const saved_game &save, const std::string& outfile)
    {
        buffer<unsigned char> b = write_xcom_save(save);
        FILE *fp = fopen(outfile.c_str(), "wb");
        fwrite(b.buf.get(), 1, b.length, fp);
        fclose(fp);
    }
}
