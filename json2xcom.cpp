#include "xcom.h"
#include "json11.hpp"
#include "util.h"

#include <iostream>
#include <cassert>
#include <cstring>
#include <string>
#include <sstream>

#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#error No <filesystem> support on this platform.
#endif

using namespace json11;
using namespace xcom;

struct property_dispatch
{
    std::string name;
    property_ptr(*func)(const Json& json, xcom_version version);
};

property_ptr build_property(const Json& json, xcom_version version);
property_list build_property_list(const Json& json, xcom_version version);

property_ptr build_int_property(const Json& json, xcom_version version);
property_ptr build_float_property(const Json& json, xcom_version version);
property_ptr build_bool_property(const Json& json, xcom_version version);
property_ptr build_object_property(const Json& json, xcom_version version);
property_ptr build_string_property(const Json& json, xcom_version version);
property_ptr build_name_property(const Json& json, xcom_version version);
property_ptr build_enum_property(const Json& json, xcom_version version);
property_ptr build_struct_property(const Json& json, xcom_version version);
property_ptr build_array_property(const Json& json, xcom_version version);
property_ptr build_static_array_property(const Json& json, xcom_version version);
property_ptr build_object_array_property(const Json& json, xcom_version version);
property_ptr build_number_array_property(const Json& json, xcom_version version);
property_ptr build_string_array_property(const Json& json, xcom_version version);
property_ptr build_enum_array_property(const Json& json, xcom_version version);
property_ptr build_struct_array_property(const Json& json, xcom_version version);

struct json_shape_exception : xcom::error::xcom_exception
{
    json_shape_exception(const std::string& n, const std::string& e) : node_ {n}, error_{e} {}
    virtual std::string what() const noexcept
    {
            std::ostringstream stream;
            stream << "Error: invalid json format in " << node_ << ": " << error_ << std::endl;
            return stream.str(); 
    }

private:
    std::string node_;
    std::string error_;
};

struct io_exception : xcom::error::xcom_exception
{
    io_exception(const std::string& s) : str_{s} {}
    virtual std::string what() const noexcept
    {
        std::ostringstream stream;
        stream << "Error: " << str_ << std::endl; 
        return stream.str();
    }

private:
    std::string str_;
};

static property_dispatch dispatch_table[] = {
    { "IntProperty", build_int_property },
    { "FloatProperty", build_float_property },
    { "BoolProperty", build_bool_property },
    { "StrProperty", build_string_property },
    { "NameProperty", build_name_property },
    { "ObjectProperty", build_object_property },
    { "ByteProperty", build_enum_property },
    { "StructProperty", build_struct_property },
    { "ArrayProperty", build_array_property },
    { "StaticArrayProperty", build_static_array_property }
};

xcom_string build_unicode_string(const Json& json, [[maybe_unused]] xcom_version version)
{
    Json::shape shape = {
        { "str", Json::STRING },
        { "is_wide", Json::BOOL }
    };

    std::string err;
    if (!json.has_shape(shape, err)) {
        throw json_shape_exception("unicode string", err);
    }

    return xcom_string{ json["str"].string_value(), json["is_wide"].bool_value() };
}

bool check_header_shape(xcom_version version, const Json& json, std::string &err)
{
    switch (version)
    {
    case xcom_version::enemy_within:
    case xcom_version::enemy_unknown:
        return json.has_shape({
            { std::string("version"), Json::NUMBER },
            { std::string("uncompressed_size"), Json::NUMBER },
            { "game_number", Json::NUMBER },
            { "save_number", Json::NUMBER },
            { "save_description", Json::OBJECT },
            { "time", Json::OBJECT },
            { "map_command", Json::STRING },
            { "tactical_save", Json::BOOL },
            { "ironman", Json::BOOL },
            { "autosave", Json::BOOL },
            { "dlc", Json::STRING },
            { "language", Json::STRING }
        }, err);
    case xcom_version::enemy_within_android:
        return json.has_shape({
            { "version", Json::NUMBER },
            { "uncompressed_size", Json::NUMBER },
            { "game_number", Json::NUMBER },
            { "save_number", Json::NUMBER },
            { "save_description", Json::OBJECT },
            { "time", Json::OBJECT },
            { "map_command", Json::STRING },
            { "tactical_save", Json::BOOL },
            { "ironman", Json::BOOL },
            { "autosave", Json::BOOL },
            { "dlc", Json::STRING },
            { "language", Json::STRING },
            { "profile_number", Json::NUMBER },
            { "profile_date", Json::OBJECT },
        }, err);
    default:
        throw xcom::error::unsupported_version(version);
    }
}

header build_header(const Json& json)
{
    header hdr;

    hdr.version = static_cast<xcom_version>(json["version"].int_value());
    if (!supported_version(hdr.version)) {
        throw xcom::error::unsupported_version(hdr.version);
    }

    // The header shape depends on the version.
    std::string err;
    if (!check_header_shape(hdr.version, json, err)) {
        throw json_shape_exception("header", err);
    }

    hdr.uncompressed_size = json["uncompressed_size"].int_value();
    hdr.game_number = json["game_number"].int_value();
    hdr.save_number = json["save_number"].int_value();
    hdr.save_description = build_unicode_string(json["save_description"], hdr.version);
    hdr.time = build_unicode_string(json["time"], hdr.version);
    hdr.map_command = json["map_command"].string_value();
    hdr.tactical_save = json["tactical_save"].bool_value();
    hdr.ironman = json["ironman"].bool_value();
    hdr.autosave = json["autosave"].bool_value();
    hdr.dlc = json["dlc"].string_value();
    hdr.language = json["language"].string_value();

    if (hdr.version == xcom_version::enemy_within_android) {
        hdr.profile_number = json["profile_number"].int_value();
        hdr.profile_date = build_unicode_string(json["profile_date"], hdr.version);
    }

    return hdr;
}

actor_table build_actor_table(const Json& json)
{
    actor_table table;
    for (const Json& elem : json.array_items()) {
        table.push_back(elem.string_value());
    }
    return table;
}

template <typename T>
static T value_from_json(const Json& json);

template <>
int value_from_json(const Json& json)
{
    return json.int_value();
}

template <>
float value_from_json(const Json& json)
{
    return static_cast<float>(json.number_value());
}


template <typename T>
std::array<T, 3> build_array(const Json& json, [[maybe_unused]] xcom_version version)
{
    std::array<T, 3> arr;
    if (json.array_items().size() != 3) {
        std::ostringstream stream;
        stream << "expected 3 items but got " << json.array_items().size() << ": " << json.dump();
        throw json_shape_exception("vector/rotator array", stream.str());
    }

    for (int i = 0; i < 3; ++i) {
        arr[i] = value_from_json<T>(json.array_items()[i]);
    }

    return arr;
}



property_ptr build_int_property(const Json& json, [[maybe_unused]] xcom_version version)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "value", Json::NUMBER }
    };

    if (!json.has_shape(shape, err)) {
        throw json_shape_exception("int property", err);
    }

    return std::make_unique<int_property>(json["name"].string_value(), 
        json["value"].int_value());
}

property_ptr build_float_property(const Json& json, [[maybe_unused]] xcom_version version)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "value", Json::NUMBER }
    };

    if (!json.has_shape(shape, err)) {
        throw json_shape_exception("float property", err);
    }

    return std::make_unique<float_property>(json["name"].string_value(), 
            static_cast<float>(json["value"].number_value()));
}

property_ptr build_bool_property(const Json& json, [[maybe_unused]] xcom_version version)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "value", Json::BOOL }
    };

    if (!json.has_shape(shape, err)) {
        throw json_shape_exception("bool property", err);
    }

    return std::make_unique<bool_property>(json["name"].string_value(), 
        json["value"].bool_value());
}

property_ptr build_string_property(const Json& json, xcom_version version)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "value", Json::OBJECT }
    };

    if (!json.has_shape(shape, err)) {
        throw json_shape_exception("string property", err);
    }
    return std::make_unique<string_property>(json["name"].string_value(),
        build_unicode_string(json["value"], version));
}

property_ptr build_name_property(const Json& json, [[maybe_unused]] xcom_version version)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "string", Json::STRING },
        { "number", Json::NUMBER }
    };

    if (!json.has_shape(shape, err)) {
        throw json_shape_exception("name property", err);
    }

    return std::make_unique<name_property>(json["name"].string_value(),
        json["string"].string_value(), json["number"].int_value());
}

property_ptr build_object_property(const Json& json, xcom_version version)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "actor", Json::NUMBER }
    };

    if (!json.has_shape(shape, err)) {
        throw json_shape_exception("object property", err);
    }
    if (version == xcom_version::enemy_unknown)
    {
        return std::make_unique<object_property_EU>(json["name"].string_value(),
            json["actor"].int_value());
    }
    else
    {
        return std::make_unique<object_property>(json["name"].string_value(),
            json["actor"].int_value());
    }
}

property_ptr build_enum_property(const Json& json, [[maybe_unused]] xcom_version version)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "type", Json::STRING },
        { "value", Json::STRING },
        { "number", Json::NUMBER }
    };

    if (!json.has_shape(shape, err)) {
        throw json_shape_exception("enum property", err);
    }

    return std::make_unique<enum_property>(json["name"].string_value(), 
        json["type"].string_value(), json["value"].string_value(), 
        json["number"].int_value());
}

property_ptr build_struct_property(const Json& json, xcom_version version)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "struct_name", Json::STRING },
        { "properties", Json::ARRAY },
        { "native_data", Json::STRING },
    };

    if (!json.has_shape(shape, err)) {
        throw json_shape_exception("struct property", err);
    }

    std::unique_ptr<unsigned char[]> data;
    const std::string & native_data_str = json["native_data"].string_value();
    if (native_data_str != "") {
        int32_t data_len = static_cast<int32_t>(native_data_str.length() / 2);
        data = util::from_hex(native_data_str);
        return std::make_unique<struct_property>(json["name"].string_value(), 
            json["struct_name"].string_value(), std::move(data), data_len);
    }
    else {
        property_list props = build_property_list(json["properties"], version);
        return std::make_unique<struct_property>(json["name"].string_value(), 
            json["struct_name"].string_value(), std::move(props));
    }
}

property_ptr build_array_property(const Json& json, xcom_version version)
{
    // Handle array sub-types
    if (json["actors"] != Json()) {
        return build_object_array_property(json, version);
    }
    else if (json["elements"] != Json()) {
        return build_number_array_property(json, version);
    }
    else if (json["structs"] != Json()) {
        return build_struct_array_property(json, version);
    }
    else if (json["strings"] != Json()) {
        return build_string_array_property(json, version);
    }
    else if (json["enum_values"] != Json()) {
        return build_enum_array_property(json, version);
    }

    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "data_length", Json::NUMBER },
        { "array_bound", Json::NUMBER },
        { "data", Json::STRING },
    };

    if (!json.has_shape(shape, err)) {
        throw json_shape_exception("array property", err);
    }

    const std::string & data_str = json["data"].string_value();
    std::unique_ptr<unsigned char[]> data;

    if (data_str.length() > 0) {
        assert(static_cast<int32_t>(data_str.length() / 2) == (json["data_length"].int_value()));
        data = util::from_hex(data_str);
    }

    return std::make_unique<array_property>(json["name"].string_value(), std::move(data),
        json["data_length"].int_value(), json["array_bound"].int_value());
}

property_ptr build_object_array_property(const Json& json, [[maybe_unused]] xcom_version version)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "actors", Json::ARRAY },
    };

    if (!json.has_shape(shape, err)) {
        throw json_shape_exception("object array property", err);
    }

    std::vector<int32_t> elements;

    for (const Json& elem : json["actors"].array_items()) {
        elements.push_back(elem.int_value());
    }

    return std::make_unique<object_array_property>(json["name"].string_value(), std::move(elements));
}

property_ptr build_number_array_property(const Json& json, [[maybe_unused]] xcom_version version)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "elements", Json::ARRAY },
    };

    if (!json.has_shape(shape, err)) {
        throw json_shape_exception("number array property", err);
    }

    std::vector<int32_t> elements;

    for (const Json& elem : json["elements"].array_items()) {
        elements.push_back(elem.int_value());
    }

    return std::make_unique<number_array_property>(json["name"].string_value(), std::move(elements));
}

property_ptr build_string_array_property(const Json& json, xcom_version version)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "strings", Json::ARRAY },
    };

    if (!json.has_shape(shape, err)) {
        throw json_shape_exception("string array property", err);
    }

    std::vector<xcom_string> elements;

    for (const Json& elem : json["strings"].array_items()) {
        elements.push_back(build_unicode_string(elem, version));
    }

    return std::make_unique<string_array_property>(json["name"].string_value(), std::move(elements));
}

property_ptr build_enum_array_property(const Json& json, [[maybe_unused]] xcom_version version)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "enum_values", Json::ARRAY },
    };

    if (!json.has_shape(shape, err)) {
        throw json_shape_exception("enum array property", err);
    }

    std::vector<enum_value> elements;

    for (const Json& elem : json["enum_values"].array_items()) {
        std::string name = elem["value"].string_value();
        int32_t number = elem["number"].int_value();
        elements.push_back({ name, number });
    }

    return std::make_unique<enum_array_property>(json["name"].string_value(), std::move(elements));
}

property_ptr build_struct_array_property(const Json& json, xcom_version version)
{
    std::string err;

    Json::shape shape = {
        { "name", Json::STRING },
        { "structs", Json::ARRAY },
    };

    if (!json.has_shape(shape, err)) {
        throw json_shape_exception("object array property", err);
    }

    std::vector<property_list> elements;

    for (const Json& elem : json["structs"].array_items()) {
        elements.push_back(build_property_list(elem, version));
    }

    return std::make_unique<struct_array_property>(json["name"].string_value(), 
        std::move(elements));
}

property_ptr build_static_array_property(const Json& json, xcom_version version)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
    };

    if (!json.has_shape(shape, err)) {
        throw json_shape_exception("static array property", err);
    }

    std::unique_ptr<static_array_property> static_array =
        std::make_unique<static_array_property>(json["name"].string_value());

    if (json["int_values"] != Json()) {
        // An array of integers.
        for (const Json &v : json["int_values"].array_items()) {
            static_array->properties.push_back(
                std::make_unique<int_property>(json["name"].string_value(),
                    v.int_value()));
        }
    }
    else if (json["string_values"] != Json()) {
        // An array of (narrow) strings
        for (const Json &v : json["string_values"].array_items()) {
            static_array->properties.push_back(
                std::make_unique<string_property>(json["name"].string_value(),
                    xcom_string{ v.string_value(), false }));
        }
    }
    else
    {
        for (const Json& elem : json["properties"].array_items()) {
            static_array->properties.push_back(build_property(elem, version));
        }
    }
    return property_ptr{ static_array.release() };
}

property_ptr build_property(const Json& json, xcom_version version)
{
    std::string kind = json["kind"].string_value();
    for (const property_dispatch &dispatch : dispatch_table) {
        if (dispatch.name.compare(kind) == 0) {
            return dispatch.func(json, version);
        }
    }

    std::string err = "Error reading json file: Unknown property kind: ";
    err.append(kind);
    throw xcom::error::general_exception(err);
}

property_list build_property_list(const Json& json, xcom_version version)
{
    property_list props;
    for (const Json& elem : json.array_items()) {
        props.push_back(build_property(elem, version));
    }

    return props;
}

checkpoint build_checkpoint(const Json& json, xcom_version version)
{
    checkpoint chk;
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "instance_name", Json::STRING },
        { "vector", Json::ARRAY },
        { "rotator", Json::ARRAY },
        { "class_name", Json::STRING },
        { "properties", Json::ARRAY },
        { "template_index", Json::NUMBER },
        { "pad_size", Json::NUMBER }
    };

    if (!json.has_shape(shape, err)) {
        throw json_shape_exception("checkpoint", err);
    }

    chk.name = json["name"].string_value();
    chk.instance_name = json["instance_name"].string_value();
    chk.vector = build_array<float>(json["vector"], version);
    chk.rotator = build_array<int>(json["rotator"], version);
    chk.class_name = json["class_name"].string_value();
    chk.properties = build_property_list(json["properties"], version);
    chk.template_index = json["template_index"].int_value();
    chk.pad_size = json["pad_size"].int_value();
    return chk;
}

checkpoint_table build_checkpoint_table(const Json& json, xcom_version version)
{
    checkpoint_table table;
    for (const Json& elem : json.array_items()) {
        table.push_back(build_checkpoint(elem, version));
    }
    return table;
}

checkpoint_chunk build_checkpoint_chunk(const Json& json, xcom_version version)
{
    checkpoint_chunk chunk;
    std::string err;
    Json::shape shape = {
        { "unknown_int1", Json::NUMBER },
        { "game_type", Json::STRING },
        { "checkpoint_table", Json::ARRAY },
        { "unknown_int2", Json::NUMBER },
        { "class_name", Json::STRING },
        { "actor_table", Json::ARRAY },
        { "unknown_int3", Json::NUMBER },
        { "display_name", Json::STRING },
        { "map_name", Json::STRING },
        { "unknown_int4", Json::NUMBER }
    };

    if (!json.has_shape(shape, err)) {
        throw json_shape_exception("checkpoint chunk", err);
    }

    chunk.unknown_int1 = json["unknown_int1"].int_value();
    chunk.game_type = json["game_type"].string_value();
    chunk.checkpoints = build_checkpoint_table(json["checkpoint_table"], version);
    chunk.unknown_int2 = json["unknown_int2"].int_value();
    chunk.class_name = json["class_name"].string_value();
    chunk.actors = build_actor_table(json["actor_table"]);
    chunk.unknown_int3 = json["unknown_int3"].int_value();
    chunk.display_name = json["display_name"].string_value();
    chunk.map_name = json["map_name"].string_value();
    chunk.unknown_int4 = json["unknown_int4"].int_value();
    return chunk;
}

checkpoint_chunk_table build_checkpoint_chunk_table(const Json& json, xcom_version version)
{
    checkpoint_chunk_table table;
    for (const Json& elem : json.array_items()) {
        table.push_back(build_checkpoint_chunk(elem, version));
    }
    return table;
}

saved_game build_save(const Json& json)
{
    saved_game save;
    std::string err;
    Json::shape shape = {
        { "header", Json::OBJECT },
        { "actor_table", Json::ARRAY },
        { "checkpoints", Json::ARRAY }
    };

    if (!json.has_shape(shape, err)) {
        throw json_shape_exception("root", err);
    }

    save.hdr = build_header(json["header"]);
    save.actors = build_actor_table(json["actor_table"]);
    save.checkpoints = build_checkpoint_chunk_table(json["checkpoints"], save.hdr.version);
    return save;
}

void usage(const char * name)
{
    printf("Usage: %s [-o <outfile>] <infile>\n", name);
}

buffer<char> read_file(const std::string& filename)
{
    FILE *fp = fopen(filename.c_str(), "rb");
    if (fp == nullptr) {
        throw io_exception("error opening file");
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        throw io_exception("error determining file length");
    }

    size_t file_length = ftell(fp);

    if (fseek(fp, 0, SEEK_SET) != 0) {
        throw io_exception("error determining file length");
    }

    std::unique_ptr<char[]> file_buf = std::make_unique<char[]>(file_length + 1);
    if (fread(file_buf.get(), 1, file_length, fp) != file_length) {
        throw io_exception("error reading file contents");
        return{};
    }
    file_buf[file_length] = 0;
    fclose(fp);
    return buffer<char>{std::move(file_buf), file_length};
}

int main(int argc, char *argv[])
{
    std::string  infile;
    std::string outfile;

    if (argc <= 1) {
        usage(argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0) {
            if (argc <= (i+1)) {
                usage(argv[0]);
                return 1;
            }
            outfile = argv[++i];
        }
        else {
            if (!infile.empty()) {
                usage(argv[0]);
                exit(1);
            }
            infile = argv[i];
        }
    }

    if (infile.empty()) {
        usage(argv[0]);
        return 1;
    }

    if (outfile.empty()) {
        size_t pos = infile.rfind(".json");
        if (pos != std::string::npos) {
            outfile = infile.substr(0, pos);
            if (fs::exists(outfile)) {
                outfile += ".out";
            }
        }
        else {
            outfile = infile + ".out";
        }
    }

    buffer<char> buf = read_file(infile);

    if (buf.length == 0) {
        return 1;
    }

    std::string errStr;
    Json jsonsave = Json::parse(buf.buf.get(), errStr);

    try {
        saved_game save = build_save(jsonsave);
        write_xcom_save(save, outfile);
        return 0;
    }
    catch (const error::xcom_exception& e) {
        fprintf(stderr, "%s", e.what().c_str());
        return 1;
    }
    
    fprintf(stderr, "Error: unknown error\n");
    return 1;
}
