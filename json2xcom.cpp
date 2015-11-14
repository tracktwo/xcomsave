#include "xcom.h"
#include "xcomwriter.h"
#include "json11.hpp"
#include "util.h"

#include <iostream>
#include <cassert>
#include <cstring>

using namespace json11;
using namespace xcom;

struct property_dispatch
{
    std::string name;
    property_ptr(*func)(const Json& json);
};

property_ptr build_property(const Json& json);
property_list build_property_list(const Json& json);

property_ptr build_int_property(const Json& json);
property_ptr build_float_property(const Json& json);
property_ptr build_bool_property(const Json& json);
property_ptr build_object_property(const Json& json);
property_ptr build_string_property(const Json& json);
property_ptr build_enum_property(const Json& json);
property_ptr build_struct_property(const Json& json);
property_ptr build_array_property(const Json& json);
property_ptr build_static_array_property(const Json& json);
property_ptr build_object_array_property(const Json& json);
property_ptr build_number_array_property(const Json& json);
property_ptr build_string_array_property(const Json& json);
property_ptr build_struct_array_property(const Json& json);


static property_dispatch dispatch_table[] = {
    { "IntProperty", build_int_property },
    { "FloatProperty", build_float_property },
    { "BoolProperty", build_bool_property },
    { "StrProperty", build_string_property },
    { "ObjectProperty", build_object_property },
    { "ByteProperty", build_enum_property },
    { "StructProperty", build_struct_property },
    { "ArrayProperty", build_array_property },
    { "StaticArrayProperty", build_static_array_property }
};

header build_header(const Json& json)
{
    header hdr;
    Json::shape shape = {
        { "version", Json::NUMBER },
        { "uncompressed_size", Json::NUMBER },
        { "game_number", Json::NUMBER },
        { "save_number", Json::NUMBER },
        { "save_description", Json::STRING },
        { "time", Json::STRING },
        { "map_command", Json::STRING },
        { "tactical_save", Json::BOOL },
        { "ironman", Json::BOOL },
        { "autosave", Json::BOOL },
        { "dlc", Json::STRING },
        { "language", Json::STRING }
    };

    std::string err;
    if (!json.has_shape(shape, err)) {
        throw std::runtime_error(
                "Error reading json file: header format does not match\n");
    }

    hdr.version = json["version"].int_value();
    hdr.uncompressed_size = json["uncompressed_size"].int_value();
    hdr.game_number = json["game_number"].int_value();
    hdr.save_number = json["save_number"].int_value();
    hdr.save_description = json["save_description"].string_value();
    hdr.time = json["time"].string_value();
    hdr.map_command = json["map_command"].string_value();
    hdr.tactical_save = json["tactical_save"].bool_value();
    hdr.ironman = json["ironman"].bool_value();
    hdr.autosave = json["autosave"].bool_value();
    hdr.dlc = json["dlc"].string_value();
    hdr.language = json["language"].string_value();

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
std::array<T, 3> build_array(const Json& json)
{
    std::array<T, 3> arr;
    if (json.array_items().size() != 3) {
        throw std::runtime_error(
            "Error reading json file: format mismatch in vector/rotator array");
    }

    for (int i = 0; i < 3; ++i) {
        arr[i] = value_from_json<T>(json.array_items()[i]);
    }

    return arr;
}



property_ptr build_int_property(const Json& json)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "value", Json::NUMBER }
    };

    if (!json.has_shape(shape, err)) {
        throw std::runtime_error(
            "Error reading json file: format mismatch in int property");
    }

    return std::make_unique<int_property>(json["name"].string_value(), 
        json["value"].int_value());
}

property_ptr build_float_property(const Json& json)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "value", Json::NUMBER }
    };

    if (!json.has_shape(shape, err)) {
        throw std::runtime_error(
            "Error reading json file: format mismatch in float property");
    }

    return std::make_unique<float_property>(json["name"].string_value(), 
            static_cast<float>(json["value"].number_value()));
}

property_ptr build_bool_property(const Json& json)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "value", Json::BOOL }
    };

    if (!json.has_shape(shape, err)) {
        throw std::runtime_error(
            "Error reading json file: format mismatch in bool property");
    }

    return std::make_unique<bool_property>(json["name"].string_value(), 
        json["value"].bool_value());
}

property_ptr build_string_property(const Json& json)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "value", Json::STRING }
    };

    if (!json.has_shape(shape, err)) {
        throw std::runtime_error(
            "Error reading json file: format mismatch in string property");
    }

    bool wide = (json["wide"] != Json() && json["wide"].bool_value());
  
    return std::make_unique<string_property>(json["name"].string_value(), 
        xcom_string{ json["value"].string_value(), wide });
}

property_ptr build_object_property(const Json& json)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "actor", Json::NUMBER }
    };

    if (!json.has_shape(shape, err)) {
        throw std::runtime_error(
            "Error reading json file: format mismatch in object property");
    }

    return std::make_unique<object_property>(json["name"].string_value(), 
        json["actor"].int_value());
}

property_ptr build_enum_property(const Json& json)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "type", Json::STRING },
        { "value", Json::STRING },
        { "extra_value", Json::NUMBER }
    };

    if (!json.has_shape(shape, err)) {
        throw std::runtime_error(
            "Error reading json file: format mismatch in enum property");
    }

    return std::make_unique<enum_property>(json["name"].string_value(), 
        json["type"].string_value(), json["value"].string_value(), 
        json["extra_value"].int_value());
}

property_ptr build_struct_property(const Json& json)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "struct_name", Json::STRING },
        { "properties", Json::ARRAY },
        { "native_data", Json::STRING },
    };

    if (!json.has_shape(shape, err)) {
        throw std::runtime_error(
            "Error reading json file: format mismatch in struct property");
    }

    std::unique_ptr<unsigned char[]> data;
    const std::string & native_data_str = json["native_data"].string_value();
    if (native_data_str != "") {
        size_t data_len = native_data_str.length() / 2;
        data = util::from_hex(native_data_str);
        return std::make_unique<struct_property>(json["name"].string_value(), 
            json["struct_name"].string_value(), std::move(data), data_len);
    }
    else {
        property_list props = build_property_list(json["properties"]);
        return std::make_unique<struct_property>(json["name"].string_value(), 
            json["struct_name"].string_value(), std::move(props));
    }
}

property_ptr build_array_property(const Json& json)
{
    // Handle array sub-types
    if (json["actors"] != Json()) {
        return build_object_array_property(json);
    }
    else if (json["elements"] != Json()) {
        return build_number_array_property(json);
    }
    else if (json["structs"] != Json()) {
        return build_struct_array_property(json);
    }
    else if (json["strings"] != Json()) {
        return build_string_array_property(json);
    }

    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "data_length", Json::NUMBER },
        { "array_bound", Json::NUMBER },
        { "data", Json::STRING },
    };

    if (!json.has_shape(shape, err)) {
        throw std::runtime_error(
            "Error reading json file: format mismatch in array property");
    }

    const std::string & data_str = json["data"].string_value();
    std::unique_ptr<unsigned char[]> data;

    if (data_str.length() > 0) {
        assert((data_str.length() / 2) == (json["data_length"].int_value()));
        data = util::from_hex(data_str);
    }

    return std::make_unique<array_property>(json["name"].string_value(), std::move(data),
        json["data_length"].int_value(), json["array_bound"].int_value());
}

property_ptr build_object_array_property(const Json& json)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "actors", Json::ARRAY },
    };

    if (!json.has_shape(shape, err)) {
        throw std::runtime_error(
            "Error reading json file: format mismatch in object array property");
    }

    std::vector<int32_t> elements;

    for (const Json& elem : json["actors"].array_items()) {
        elements.push_back(elem.int_value());
    }

    return std::make_unique<object_array_property>(json["name"].string_value(), elements);
}

property_ptr build_number_array_property(const Json& json)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "elements", Json::ARRAY },
    };

    if (!json.has_shape(shape, err)) {
        throw std::runtime_error(
            "Error reading json file: format mismatch in object array property");
    }

    std::vector<int32_t> elements;

    for (const Json& elem : json["elements"].array_items()) {
        elements.push_back(elem.int_value());
    }

    return std::make_unique<number_array_property>(json["name"].string_value(), elements);
}

property_ptr build_string_array_property(const Json& json)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "strings", Json::ARRAY },
    };

    if (!json.has_shape(shape, err)) {
        throw std::runtime_error(
            "Error reading json file: format mismatch in string array property");
    }

    std::vector<std::string> elements;

    for (const Json& elem : json["elements"].array_items()) {
        elements.push_back(elem.string_value());
    }

    return std::make_unique<string_array_property>(json["name"].string_value(), elements);
}

property_ptr build_struct_array_property(const Json& json)
{
    std::string err;

    Json::shape shape = {
        { "name", Json::STRING },
        { "structs", Json::ARRAY },
    };

    if (!json.has_shape(shape, err)) {
        throw std::runtime_error(
            "Error reading json file: format mismatch in object array property");
    }

    std::vector<property_list> elements;

    for (const Json& elem : json["structs"].array_items()) {
        elements.push_back(build_property_list(elem));
    }

    return std::make_unique<struct_array_property>(json["name"].string_value(), 
        std::move(elements));
}

property_ptr build_static_array_property(const Json& json)
{
    std::string err;
    Json::shape shape = {
        { "name", Json::STRING },
        { "properties", Json::ARRAY },
    };

    if (!json.has_shape(shape, err)) {
        throw std::runtime_error(
            "Error reading json file: format mismatch in static array property");
    }

    std::unique_ptr<static_array_property> static_array = 
        std::make_unique<static_array_property>(json["name"].string_value());

    for (const Json& elem : json["properties"].array_items()) {
        static_array->properties.push_back(build_property(elem));
    }

    return property_ptr{ static_array.release() };
}

property_ptr build_property(const Json& json)
{
    std::string kind = json["kind"].string_value();
    for (const property_dispatch &dispatch : dispatch_table) {
        if (dispatch.name.compare(kind) == 0) {
            return dispatch.func(json);
        }
    }

    std::string err = "Error reading json file: Unknown property kind: ";
    err.append(kind);
    throw std::runtime_error(err.c_str());
}

property_list build_property_list(const Json& json)
{
    property_list props;
    for (const Json& elem : json.array_items()) {
        props.push_back(build_property(elem));
    }

    return props;
}

checkpoint build_checkpoint(const Json& json)
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
        throw std::runtime_error("Error reading json file: format mismatch in checkpoint");
    }

    chk.name = json["name"].string_value();
    chk.instance_name = json["instance_name"].string_value();
    chk.vector = build_array<float>(json["vector"]);
    chk.rotator = build_array<int>(json["rotator"]);
    chk.class_name = json["class_name"].string_value();
    chk.properties = build_property_list(json["properties"]);
    chk.template_index = json["template_index"].int_value();
    chk.pad_size = json["pad_size"].int_value();
    return chk;
}

checkpoint_table build_checkpoint_table(const Json& json)
{
    checkpoint_table table;
    for (const Json& elem : json.array_items()) {
        table.push_back(build_checkpoint(elem));
    }
    return table;
}

checkpoint_chunk build_checkpoint_chunk(const Json& json)
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
        throw std::runtime_error(
            "Error reading json file: format mismatch in checkpoint chunk");
    }

    chunk.unknown_int1 = json["unknown_int1"].int_value();
    chunk.game_type = json["game_type"].string_value();
    chunk.checkpoints = build_checkpoint_table(json["checkpoint_table"]);
    chunk.unknown_int2 = json["unknown_int2"].int_value();
    chunk.class_name = json["class_name"].string_value();
    chunk.actors = build_actor_table(json["actor_table"]);
    chunk.unknown_int3 = json["unknown_int3"].int_value();
    chunk.display_name = json["display_name"].string_value();
    chunk.map_name = json["map_name"].string_value();
    chunk.unknown_int4 = json["unknown_int4"].int_value();
    return chunk;
}

checkpoint_chunk_table build_checkpoint_chunk_table(const Json& json)
{
    checkpoint_chunk_table table;
    for (const Json& elem : json.array_items()) {
        table.push_back(build_checkpoint_chunk(elem));
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
        throw std::runtime_error("Error reading json file: format mismatch at root\n");
    }

    save.hdr = build_header(json["header"]);
    save.actors = build_actor_table(json["actor_table"]);
    save.checkpoints = build_checkpoint_chunk_table(json["checkpoints"]);
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
        fprintf(stderr, "Error opening file %s\n", filename.c_str());
        return{};
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fprintf(stderr, "Error determining file length\n");
        return{};
    }

    size_t file_length = ftell(fp);

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fprintf(stderr, "Error determining file length\n");
        return{};
    }

    std::unique_ptr<char[]> file_buf = std::make_unique<char[]>(file_length + 1);
    if (fread(file_buf.get(), 1, file_length, fp) != file_length) {
        fprintf(stderr, "Error reading file contents\n");
        return{};
    }
    file_buf[file_length] = 0;
    fclose(fp);
    return buffer<char>{std::move(file_buf), file_length};
}

int main(int argc, char *argv[])
{
    bool writesave = false;
    std::string  infile;
    std::string outfile;

    if (argc <= 1) {
        usage(argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0) {
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
        size_t pos = infile.find_last_of(".json");
        if (pos != std::string::npos) {
            outfile = infile.substr(0, pos);
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
        writer w(std::move(save));
        buffer<unsigned char> save_buffer = w.save_data();

        FILE *fp = fopen(outfile.c_str(), "wb");
        fwrite(save_buffer.buf.get(), 1, save_buffer.length, fp);
        fclose(fp);
    }
    catch (format_exception e) {
        fprintf(stderr, "Error (0x%08x): ", e.offset());
        fprintf(stderr, e.what());
    }
    catch (std::exception e) {
        fprintf(stderr, e.what());
        return 1;
    }
}
