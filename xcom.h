#ifndef XCOM_H
#define XCOM_H

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

#include <stdint.h>
#include <vector>
#include <string>
#include <array>
#include <memory>
#include <algorithm>
#include <exception>

namespace xcom
{
    // The supported saved game version(s)
    enum class xcom_version : uint32_t {
        invalid = 0,
        enemy_unknown = 0x0f,
        enemy_within = 0x10,
        enemy_within_android = 0x13,
        // xcom2 = 0x14 (not supported)
    };

    bool supported_version(xcom_version ver);

    // A buffer object for performing file IO. An xcom save may be read from
    // or written to a buffer instance or a file.
    template <typename T>
    struct buffer
    {
        std::unique_ptr<T[]> buf;
        size_t length;
    };

    // A string. This class is only used in very specific places - namely
    // string properties in actors. It appears that most of the strings
    // embedded in the save file, especially property names and types and other
    // metadata, are always in Latin-1 (ISO-8859-1) for INT games, and are
    // primarily all ASCII. Property strings may be in either Latin-1 or in
    // UTF-16: if the length of the string is a negative value, multiply the
    // length by -1 but interpret the string as UTF-16 characters, otherwise
    // treat it as containing Latin-1.
    //
    // Regardless of how they appear in the save, all strings are uniformly
    // represented in UTF-8 internally to this library. The conversion to and
    // from either Latin-1 or UTF-16 is done only when reading or writing the
    // save data.
    struct xcom_string
    {
        // A UTF-8 representation of the string
        std::string str;
        // If true, this string should be written in UTF-16 format in the 
        // save file. Otherwise, it's written in ISO-8859-1
        bool is_wide;
    };

    // The header occurs at the start of the save file and is the only
    // uncompressed part of the save. The first 1024 bytes of the save are the
    // header, although most of it is all zeros. On PC the header contains two
    // checksums: the first checksum occurs at the end of the main header data
    // blob (e.g. the data represented in this struct) and is the CRC32 of all
    // the compressed data in the file. E.g. everything from byte
    // 1024 on. The second checksum is the header checksum and is represented
    // at the end of the header, starting at offset 1016. The first value
    // is the 4-byte length of the header data checksummed. It's the first
    // N bytes of the header up to and including the first CRC and the
    // four zero bytes immediately following it. The second 4-byte integer
    // is the CRC32 of the header data itself. e.g.:
    //
    // 0        : Header Data (version through language)
    // N        : CRC32 of compressed data
    // N+4-1016 : Zeros
    // 1016     : Number of bytes of header checksummed (N + 4).
    // 1020     : CRC32 of the first N+4 bytes of header.
    // 1024-end : Compressed data
    //
    // In version 0x13 (Enemy Within for Android) there is only a single
    // compressed date checksum, no header checksum. It occurs in the same
    // place as on PC and checksums data from offset 1024 to the end of the
    // file.
    struct header
    {
        // XCom save version
        xcom_version version;

        // From the upk packages this looks like it should be the total
        // uncompressed size of the save data. In practice It's always all zeros.
        int32_t uncompressed_size;

        // The game number
        int32_t game_number;

        // The save number
        int32_t save_number;

        // The readable save description. This appears in the load game list.
        xcom_string save_description;

        // The date and time of the save
        xcom_string time;

        // The map to load for this save
        std::string map_command;

        // True if this is a tactical mission save, false if it's a geoscape save
        bool tactical_save;

        // True if the game is an ironman game
        bool ironman;

        // True if this is an autosave
        bool autosave;

        // A string containing the names of all installed DLC packs
        std::string dlc;

        // The current game language (e.g. INT for English/International)
        std::string language;

        int32_t profile_number; // Profile number (Android)
        xcom_string profile_date; // Profile date? (Android)
    };

    using actor_table = std::vector<std::string>;

    struct property_visitor;

    // An Unreal property. Saved actors and other objects are made up of
    // property values.  These can be simple primitive values (int, float,
    // bool, string, enums), object values (actors), or aggregate types
    // (arrays, structs, and static arrays of another property type).
    //
    // Most of the kinds are present directly in the saved game, but arrays
    // have some special handling here to make them easier to deal with:
    //
    // Dynamic arrays are all represented identically in the save but have
    // different representations in this library. They can be one of the
    // following kinds:
    //
    // ObjectArrayProperty - An array of objects (actors). Contents are simply
    // actor reference numbers.
    // 
    // NumberArrayProperty - An array of numbers. Could be integers or floats,
    // depending on the particular property. There is no unambiguous way to
    // distinguish the two cases from analyzing the save file alone, you need
    // to "know" the real type of the object by looking in the corresponding
    // UPK file. Array data is represented as an array of ints, but may need to
    // be cast to floats if appropriate.
    //
    // StructArrayProperty - An array of structs. The name of the struct itself
    // isn't recorded in the save, it's only visible in the UPK, so it is represented
    // as an array of unnamed structs. Since structs may have default values, even 
    // though all elements of the array must have the same struct type the actual
    // properties present might be different for each element as some may be missing
    // and use the default value.
    //
    // ArrayProperty - An array of something else. Strings/enums are the typical
    // case but they are not yet completely handled.
    struct property
    {
        enum class kind_t
        {
            int_property,
            float_property,
            bool_property,
            string_property,
            object_property,
            name_property,
            enum_property,
            struct_property,
            array_property,
            object_array_property,
            number_array_property,
            struct_array_property,
            string_array_property,
            enum_array_property,
            static_array_property,
            last_property
        };

        property(const std::string &n, kind_t k) : name(n), kind(k) {}

        std::string kind_string() const;
        virtual int32_t size() const = 0;
        virtual int32_t full_size() const;
        virtual void accept(property_visitor * v) = 0;

        std::string name;
        kind_t kind;
    };

    // Properties are polymorphic types and are usually referenced by 
    // XComPropertyPtr values which are simply unique_ptrs to a property.
    using property_ptr = std::unique_ptr<property>;

    struct int_property;
    struct float_property;
    struct bool_property;
    struct string_property;
    struct object_property;
    struct name_property;
    struct enum_property;
    struct struct_property;
    struct array_property;
    struct object_array_property;
    struct number_array_property;
    struct struct_array_property;
    struct string_array_property;
    struct enum_array_property;
    struct static_array_property;

    // XCOM enum values are represented by a string enumerator value (e.g. eFoo_Value) and
    // a corresponding integer. The integer is always 0 for standard enums but LW extended
    // enums re-use existing value names with successively growing int values.
    struct enum_value
    {
        std::string name;
        int32_t number;
    };

    // Visit all property types.
    struct property_visitor
    {
        virtual void visit(int_property*) = 0;
        virtual void visit(float_property*) = 0;
        virtual void visit(bool_property*) = 0;
        virtual void visit(string_property*) = 0;
        virtual void visit(object_property*) = 0;
        virtual void visit(name_property*) = 0;
        virtual void visit(enum_property*) = 0;
        virtual void visit(struct_property*) = 0;
        virtual void visit(array_property*) = 0;
        virtual void visit(object_array_property*) = 0;
        virtual void visit(number_array_property*) = 0;
        virtual void visit(struct_array_property*) = 0;
        virtual void visit(string_array_property*) = 0;
        virtual void visit(enum_array_property*) = 0;
        virtual void visit(static_array_property*) = 0;
    };

    // A list of properties.
    using property_list = std::vector<property_ptr>;


    // An object property refers to an actor.
    struct object_property : public property
    {
        object_property(const std::string &n, int32_t a) :
            property(n, kind_t::object_property), actor(a) {}

        virtual int32_t size() const {
            return 8;
        }

        virtual void accept(property_visitor *v) {
            v->visit(this);
        }

        int32_t actor;
    };
    
    // An object property refers to an actor.
    struct object_property_EU : public object_property
    {
        object_property_EU(const std::string &n, int32_t a) :
            object_property(n, a) {}
        virtual int32_t size() const {
            return 4;
        }
    };
    

    // An int property contains a 32-bit signed integer value.
    struct int_property : public property
    {
        int_property(const std::string& n, int32_t v) :
            property(n, kind_t::int_property), value(v) {}

        virtual int32_t size() const {
            return 4;
        }

        virtual void accept(property_visitor *v) {
            v->visit(this);
        }

        int32_t value;
    };

    // A bool property contains a boolean value.
    struct bool_property : public property
    {
        bool_property(const std::string &n, bool v) :
            property(n, kind_t::bool_property), value(v) {}

        virtual int32_t size() const {

            // Bool properties report as size 0
            return 0;
        }

        virtual int32_t full_size() const {
            return property::full_size() + 1;
        }

        virtual void accept(property_visitor *v) {
            v->visit(this);
        }

        bool value;
    };

    // A float property contains a single-precision floating point value.
    struct float_property : public property
    {
        float_property(const std::string &n, float f) :
            property(n, kind_t::float_property), value(f) {}

        virtual int32_t size() const {
            return 4;
        }

        virtual void accept(property_visitor *v) {
            v->visit(this);
        }

        float value;
    };

    // A string property contains a string value. For INT saves, these are
    // recorded in either ISO-8859-1 or UTF-16 format, but are translated by
    // the library to and from UTF-8 for ease of interoperability with other
    // libraries.  Note that non-INT saves likely use some other encoding for
    // the strings, but I do not know which. 
    //
    // When modifying strings, be sure to adjust the "is_wide" flag if you
    // inject characters that cannot be represented in Latin-1. It's likely
    // always save to mark strings as wide even if not necessary, but the
    // converse will likely crash the game.
    struct string_property : public property
    {
        string_property(const std::string& n, const xcom_string& s) :
            property(n, kind_t::string_property), str(s) {}

        virtual int32_t size() const;

        virtual void accept(property_visitor *v) {
            v->visit(this);
        }

        xcom_string str;
    };

    // A name property contains a string value referencing an Unreal name entry, and
    // a number whose use is currently unknown.
    //
    // Be careful: the name field of properties refers to the name of the property,
    // the contents of a name property is in the str field, similar to a string
    // property.
    struct name_property : public property
    {
        name_property(const std::string& n, const std::string& s, int32_t d) :
            property(n, kind_t::name_property), str(s), number(d) {}

        virtual void accept(property_visitor *v) {
            v->visit(this);
        }

        virtual int32_t size() const {
            // Length of the string + null byte + the string size integer + the
            // number value.
            // Note: This is making the (reasonable?) assumption that name strings
            // can never be empty and the (unreasonable?) assumption that names
            // are always ASCII.
            return static_cast<int32_t>(str.length()) + 1 + 4 + 4;
        }

        std::string str;
        int32_t number;
    };

    // A dynamic array property. Represents an Unreal dynamic array. This type
    // is used for "raw" arrays where we have not correctly determined the
    // contents of the array. The saved array format doesn't explicitly say
    // what the type of objects are in a dynamic array, this information is
    // read from the UPK file containing the class that defines this
    // checkpoint. However, we can use some heuristics to guess the broad kind
    // of object. One of the specific array property types will be used when we
    // can determine it (object_array_property, number_array_property,
    // struct_array_property). This type is used as a fallback if the heuristic
    // fails, and it just treats the entire array contents as a big blob of
    // binary data - we can't even necessarily determine where each element
    // begins and ends inside the blob.
    struct array_property : public property
    {
        array_property(const std::string& n, 
            std::unique_ptr<unsigned char[]>&& a, int32_t dl, int32_t b) :
                property(n, kind_t::array_property), 
                data(std::move(a)),
                array_bound(b), 
                data_length(dl) 
                {}

        virtual int32_t size() const {
            return 4 + data_length;
        }

        virtual void accept(property_visitor *v) {
            v->visit(this);
        }

        std::unique_ptr<unsigned char[]> data;
        int32_t array_bound;
        int32_t data_length;
    };

    // A dynamic array of some object (actor) type. Each element is an index to an actor
    // in the actor table.
    struct object_array_property : public property
    {
        object_array_property(const std::string& n, std::vector<int32_t>&& objs) :
            property(n, kind_t::object_array_property), elements(std::move(objs)) {}

        virtual int32_t size() const {
            return 4 + 8 * static_cast<int32_t>(elements.size());
        }

        virtual void accept(property_visitor* v) {
            v->visit(this);
        }

        std::vector<int32_t> elements;
    };

    // A number array property. This can be either an array of ints or an array
    // of floats, and it's not in general possible to determine which without
    // looking at the UPK checkpoint definition. The elements are represented
    // as an array of integers, but these may actually hold floating point
    // values.
    struct number_array_property : public property
    {
        number_array_property(const std::string& n, std::vector<int32_t> && objs) :
            property(n, kind_t::number_array_property), elements(std::move(objs)) {}

        virtual int32_t size() const {
            return 4 + 4 * static_cast<int32_t>(elements.size());
        }

        virtual void accept(property_visitor *v) {
            v->visit(this);
        }

        std::vector<int32_t> elements;
    };

    // A struct array property. Each element is a struct instance and all
    // elements share the same struct type but the actual name of the struct is
    // unknown (it can be found in the UPK).
    //
    // Note that structs listed here may have default properties, in which case
    // some properties may be missing for some elements. It's impossible to
    // determine the full list of properties in a struct array without looking
    // in the UPK to get the full struct definition, but a good approximation
    // is the set union of all properties across all the elements in the array.
    struct struct_array_property : public property
    {
        struct_array_property(const std::string &n, std::vector<property_list>&& props) :
            property(n, kind_t::struct_array_property), elements(std::move(props)) {}

        virtual int32_t size() const {
            // A struct array has the array bound plus 9 bytes per element for the terminating "None" string 
            // plus 4 bytes per element for the unknown 0 integer that follows the none.
            int32_t total = 4 + 13 * static_cast<int32_t>(elements.size());
            std::for_each(elements.begin(), elements.end(), 
                [&total](const property_list &pl) {
                    std::for_each(pl.begin(), pl.end(), 
                        [&total](const property_ptr& prop) {
                            total += prop->full_size();
                        });
                });

            return total;
        }

        virtual void accept(property_visitor *v) {
            v->visit(this);
        }

        std::vector<property_list> elements;
    };

    // A string array property. Elements are all strings, each individual string
    // in the array may be narrow or wide. Like all strings, they are always internally
    // represented in UTF-8, though.
    struct string_array_property : public property
    {
        string_array_property(const std::string& n, std::vector<xcom_string> &&objs) :
            property(n, kind_t::string_array_property), elements(std::move(objs)) {}

        virtual int32_t size() const;

        virtual void accept(property_visitor *v) {
            v->visit(this);
        }

        std::vector<xcom_string> elements;
    };

    // An enum array property. Elements are string values consisting of enum member
    // names for some unknown enum type. The enum type is not recorded by the array
    // and is only known by looking in the UPK.
    struct enum_array_property : public property
    {
        enum_array_property(const std::string& n, std::vector<enum_value>&& objs) :
            property(n, kind_t::enum_array_property), elements(std::move(objs)) {}

        virtual int32_t size() const;

        virtual void accept(property_visitor *v) {
            v->visit(this);
        }

        std::vector<enum_value> elements;
    };

    // An enum property. Contains the enum type and enum value strings, as well
    // as an "extra" value that I am not entirely sure how to interpret, but is
    // commonly used in LW extended enums. Vanilla enums generally have '0' for
    // this extra value for all members. LW extended enums typically re-use the
    // names of an existing enum member, but each successive additional entry
    // with the same name has an extra value larger than the previous.
    struct enum_property : public property
    {
        enum_property(const std::string& n, const std::string &et, 
                        const std::string &ev, int32_t i) :
            property(n, kind_t::enum_property), type(et), 
            value{ ev, i } {}

        virtual int32_t size() const {
            // Handle the special "None" byte type which is just a single byte.
            if (type == "None") {
                return 1;
            }
            // size does not include the size of the enum type string
            return static_cast<int32_t>(value.name.length()) + 5 + 4;
        }

        virtual int32_t full_size() const {
            // full size must also include the length of the inner "unknown"
            // value and the enum Type string length.
            return property::full_size() + static_cast<int32_t>(type.length()) + 5 + 4;
        }

        void accept(property_visitor *v) {
            v->visit(this);
        }

        std::string type;
        enum_value value;
    };

    // A struct property. Contains a nested list of properties for the struct elements.
    struct struct_property : public property
    {
        struct_property(const std::string &n, const std::string &sn, 
            property_list &&props) :
                property(n, kind_t::struct_property), 
                struct_name(sn), 
                properties(std::move(props)),
                native_data(), 
                native_data_length(0) {}

        struct_property(const std::string& n, const std::string &sn, 
            std::unique_ptr<unsigned char[]> &&nd, int32_t l) :
                property(n, kind_t::struct_property), 
                struct_name(sn), 
                properties{}, 
                native_data(std::move(nd)), 
                native_data_length(l) {}

        virtual int32_t size() const;
        virtual int32_t full_size() const;

        void accept(property_visitor *v) {
            v->visit(this);
        }

        std::string struct_name;
        property_list properties;
        std::unique_ptr<unsigned char[]> native_data;
        int32_t native_data_length;
    };

    // A static array property. Static arrays are not first-class objects in
    // the save, they are instead represented by the elements themselves listed
    // sequentially in the save, each with an increasing array index value.
    // This property type is a pseudo-property used as a container for all the
    // array elements.
    struct static_array_property : public property
    {
        static_array_property(const std::string& n) :
            property(n, kind_t::static_array_property), properties() {}

        virtual int32_t size() const {
            int32_t total = 0;
            std::for_each(properties.begin(), properties.end(), 
                [&total](const property_ptr &prop) {
                    total += prop->size();
                });
            return total;
        }

        virtual int32_t full_size() const {
            int32_t total = 0;
            std::for_each(properties.begin(), properties.end(), 
                [&total](const property_ptr &prop) {
                    total += prop->full_size();
            });
            return total;
        }

        void accept(property_visitor *v) {
            v->visit(this);
        }

        property_list properties;
    };

    // A 3-d vector.
    using uvector = std::array<float, 3>;

    // A 3-d rotator. Note that rotators are ints, not floats.
    using urotator = std::array<int, 3>;

    // A single checkpoint record. Checkpoint records are defined for each
    // actor that is serialized by the game.
    struct checkpoint
    {
        // The fully-qualified name of the actor, e.g.
        // Command1.TheWorld.PersistentLevel.XGStrategy_0
        std::string name;

        // The instance name of the actor, e.g. XGStrategy_0
        std::string instance_name;

        // A vector representing the position of the actor in the world
        uvector vector;

        // A rotator representing the rotation of the actor in the world
        urotator rotator;

        // The class name of the class this actor is an instance of
        std::string class_name;

        // A list of properties (e.g. the member variables of the actor instance)
        property_list properties;

        // The index for this actor into the actor template table. Appears to
        // be unused in strategy saves.
        int32_t template_index;

        // The number of padding bytes (all zeros) appended to the checkpoint.
        uint32_t pad_size;
    };

    using checkpoint_table = std::vector<checkpoint>;

    // An actor template. Unused by strategy saves.
    struct actor_template
    {
        std::string actor_class_path;
        std::string archetype_path;
        unsigned char load_params[64];
    };

    using actor_template_table = std::vector<actor_template>;

    // An entry in the name table. Unused by strategy saves.
    struct name_entry
    {
        std::string name;
        unsigned char zeros[8];
        int32_t data_length;
        std::unique_ptr<unsigned char[]> data;
    };

    using name_table = std::vector<name_entry>;

    // A checkpoint "chunk". The xcom strategy save is primarily a list of
    // these (see XComCheckpointChunkTable). It is not entirely known what the
    // various chunks are for, and most of the fields are unknown. In fact the
    // unknown integer type fields may not be integers at all, but rather some
    // other kind of table that is always empty in strategy saves.
    struct checkpoint_chunk
    {
        int32_t unknown_int1;

        // The "game type". For strategy saves, this is "Command1".
        std::string game_type;

        // The table of checkpoint entries for this save.
        checkpoint_table checkpoints;

        // Unknown
        int32_t unknown_int2;

        // The class for this chunk. E.g. a top-level game type, like
        // XComStrategyGame.XComHeadquartersGame.
        std::string class_name;

        // An actor table for actors in this chunk
        actor_table actors;

        // Unknown
        int32_t unknown_int3;

        // The display name for this save chunk. Unknown.
        std::string display_name;

        // The map name for this chunk. Unknown.
        std::string map_name;

        // Unknown.
        int32_t unknown_int4;
    };

    using checkpoint_chunk_table = std::vector<checkpoint_chunk>;

    // An xcom save. The save consists of three main parts:
    // 1) The header. This part is always uncompressed (parts 2 and 3 are
    //    stored compressed in the file) and contains basic information about
    //    the save: the save numbers and identifiers, crc values to detect save
    //    corruption, and miscellaneous flags (ironman, autosave, tactical
    //    save).
    //
    // 2) The global actor table. This is a bit list of actor identifiers as
    //    strings in the format GameName.Class_Instance. The game name is
    //    always "Command1" for strategy level saves (the name of the strategy
    //    "map"). The Class name is the name of the actor class (e.g.
    //    XGStrategySoldier) and the Instance is an integer uniquely identifying 
    //    a particular instance of that class. E.g. Command1.XGStrategySoldier_3
    //    is the 4th (0 is the first) soldier instance.
    //
    // 3) The checkpoint chunk table. This is a list of several "chunks". It's
    //    not really known at this point what these chunks represent, but they
    //    do contain all main information in the saved game. I have a suspicion
    //    that I haven't fully looked into that these are corresponding to the
    //    "ActorClassesToRecord" and "ActorClassesToDestroy" lists in
    //    XComGame.Checkpoint but I haven't confirmed this yet. Note that each 
    //    checkpoint "chunk" has its own actor table in addition to the global 
    //    actor table in 2. I also haven't fully explored how these tables are 
    //    different.
    struct saved_game
    {
        header hdr;
        actor_table actors;
        checkpoint_chunk_table checkpoints;
    };

    saved_game read_xcom_save(const std::string &infile);
    saved_game read_xcom_save(buffer<unsigned char>&& buf);
    void write_xcom_save(const saved_game &save, const std::string &outfile);
    buffer<unsigned char> write_xcom_save(const saved_game &save);

    // Errors
    namespace error {
        struct xcom_exception {
            virtual std::string what() const noexcept = 0;
        };

        struct general_exception : xcom_exception
        {
            general_exception(const std::string& s) : str_{s} {}
            virtual std::string what() const noexcept { return str_; }
        private:
            std::string str_;
        };

        struct unsupported_version : xcom_exception
        {
            unsupported_version(int32_t v) : version_{ v } {}
            unsupported_version(xcom_version v) : version_{static_cast<int32_t>(v)} {}

            virtual std::string what() const noexcept;
        private:
            int32_t version_;
        };

        struct crc_mismatch : xcom_exception
        {
            crc_mismatch(uint32_t expected, uint32_t actual, bool is_header_crc) :
                expected_{expected}, actual_{actual}, is_header_crc_{is_header_crc}
            {}

            virtual std::string what() const noexcept;
        private:
            uint32_t expected_; // the expected CRC from the save
            uint32_t actual_; // computed CRC
            bool is_header_crc_; // header (true) or payload (false) CRC
        };

        // An exception reading or writing the file
        struct format_exception : xcom_exception
        {
            format_exception(std::ptrdiff_t ofs, const char *fmt, ...);
            std::ptrdiff_t offset() const noexcept { return offset_; };
            virtual std::string what() const noexcept;

        protected:
            std::ptrdiff_t offset_;
            char buf_[1024];
        };
    }
} // namespace xcom
#endif // XCOM_H
