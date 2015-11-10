#include "xcom.h"
#include "xcomwriter.h"
#include "json11.hpp"
#include "util.h"

#include <iostream>
#include <cassert>

using namespace json11;

struct PropertyDispatch
{
	std::string name;
	XComPropertyPtr(*pFunc)(const Json& json);
};

XComPropertyPtr buildProperty(const Json& json);
XComPropertyList buildPropertyList(const Json& json);

XComPropertyPtr buildIntProperty(const Json& json);
XComPropertyPtr buildFloatProperty(const Json& json);
XComPropertyPtr buildBoolProperty(const Json& json);
XComPropertyPtr buildObjectProperty(const Json& json);
XComPropertyPtr buildStringProperty(const Json& json);
XComPropertyPtr buildByteProperty(const Json& json);
XComPropertyPtr buildStructProperty(const Json& json);
XComPropertyPtr buildArrayProperty(const Json& json);
XComPropertyPtr buildStaticArrayProperty(const Json& json);
XComPropertyPtr buildObjectArrayProperty(const Json& json);
XComPropertyPtr buildNumberArrayProperty(const Json& json);
XComPropertyPtr buildStructArrayProperty(const Json& json);


static PropertyDispatch propertyDispatch[] = {
	{ "IntProperty", buildIntProperty },
	{ "FloatProperty", buildFloatProperty },
	{ "BoolProperty", buildBoolProperty },
	{ "StrProperty", buildStringProperty },
	{ "ObjectProperty", buildObjectProperty },
	{ "ByteProperty", buildByteProperty },
	{ "StructProperty", buildStructProperty },
	{ "ArrayProperty", buildArrayProperty },
	{ "ObjectArrayPropety", buildObjectArrayProperty },
	{ "StaticArrayProperty", buildStaticArrayProperty }
};

XComSaveHeader buildHeader(const Json& json)
{
	XComSaveHeader hdr;
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
		throw std::exception("Error reading json file: header format does not match\n");
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

XComActorTable buildActorTable(const Json& json)
{
	XComActorTable table;
	for (const Json& elem : json.array_items()) {
		table.push_back(elem.string_value());
	}

	return table;
}

template <typename T>
static T getValueFromJson(const Json& json);

template <>
static int getValueFromJson(const Json& json)
{
	return json.int_value();
}

template <>
static float getValueFromJson(const Json& json)
{
	return static_cast<float>(json.number_value());
}


template <typename T>
std::array<T, 3> buildArray(const Json& json)
{
	std::array<T, 3> arr;
	if (json.array_items().size() != 3) {
		throw std::exception("Error reading json file: format mismatch in vector/rotator array");
	}

	for (int i = 0; i < 3; ++i) {
		arr[i] = getValueFromJson<T>(json.array_items()[i]);
	}

	return arr;
}



XComPropertyPtr buildIntProperty(const Json& json)
{
	std::string err;
	Json::shape shape = {
		{ "name", Json::STRING },
		{ "value", Json::NUMBER }
	};

	if (!json.has_shape(shape, err)) {
		throw std::exception("Error reading json file: format mismatch in int property");
	}

	return std::make_unique<XComIntProperty>(json["name"].string_value(), json["value"].int_value());
}

XComPropertyPtr buildFloatProperty(const Json& json)
{
	std::string err;
	Json::shape shape = {
		{ "name", Json::STRING },
		{ "value", Json::NUMBER }
	};

	if (!json.has_shape(shape, err)) {
		throw std::exception("Error reading json file: format mismatch in float property");
	}

	return std::make_unique<XComFloatProperty>(json["name"].string_value(), static_cast<float>(json["value"].number_value()));
}

XComPropertyPtr buildBoolProperty(const Json& json)
{
	std::string err;
	Json::shape shape = {
		{ "name", Json::STRING },
		{ "value", Json::BOOL }
	};

	if (!json.has_shape(shape, err)) {
		throw std::exception("Error reading json file: format mismatch in bool property");
	}

	return std::make_unique<XComBoolProperty>(json["name"].string_value(), json["value"].bool_value());
}

XComPropertyPtr buildStringProperty(const Json& json)
{
	std::string err;
	Json::shape shape = {
		{ "name", Json::STRING },
		{ "value", Json::STRING }
	};

	if (!json.has_shape(shape, err)) {
		throw std::exception("Error reading json file: format mismatch in string property");
	}

	return std::make_unique<XComStringProperty>(json["name"].string_value(), json["value"].string_value());
}

XComPropertyPtr buildObjectProperty(const Json& json)
{
	std::string err;
	Json::shape shape = {
		{ "name", Json::STRING },
		{ "actor", Json::NUMBER }
	};

	if (!json.has_shape(shape, err)) {
		throw std::exception("Error reading json file: format mismatch in object property");
	}

	return std::make_unique<XComObjectProperty>(json["name"].string_value(), json["actor"].int_value());
}

XComPropertyPtr buildByteProperty(const Json& json)
{
	std::string err;
	Json::shape shape = {
		{ "name", Json::STRING },
		{ "type", Json::STRING },
		{ "value", Json::STRING },
		{ "extra_value", Json::NUMBER }
	};

	if (!json.has_shape(shape, err)) {
		throw std::exception("Error reading json file: format mismatch in byte property");
	}

	return std::make_unique<XComByteProperty>(json["name"].string_value(), json["type"].string_value(), json["value"].string_value(), json["extra_value"].int_value());
}

XComPropertyPtr buildStructProperty(const Json& json)
{
	std::string err;
	Json::shape shape = {
		{ "name", Json::STRING },
		{ "struct_name", Json::STRING },
		{ "properties", Json::ARRAY },
		{ "native_data", Json::STRING },
	};

	if (!json.has_shape(shape, err)) {
		throw std::exception("Error reading json file: format mismatch in struct property");
	}

	std::unique_ptr<unsigned char[]> data;
	const std::string & nativeDataStr = json["native_data"].string_value();
	if (nativeDataStr != "") {
		size_t dataLen = nativeDataStr.length() / 2;
		data = fromHex(nativeDataStr);
		return std::make_unique<XComStructProperty>(json["name"].string_value(), json["struct_name"].string_value(), std::move(data), dataLen);
	}
	else {
		XComPropertyList props = buildPropertyList(json["properties"]);
		return std::make_unique<XComStructProperty>(json["name"].string_value(), json["struct_name"].string_value(), std::move(props));
	}
}

XComPropertyPtr buildArrayProperty(const Json& json)
{
	// Handle array sub-types
	if (json["actors"] != Json()) {
		return buildObjectArrayProperty(json);
	} 
	else if (json["elements"] != Json()) {
		return buildNumberArrayProperty(json);
	}
	else if (json["structs"] != Json()) {
		return buildStructArrayProperty(json);
	}

	std::string err;
	Json::shape shape = {
		{ "name", Json::STRING },
		{ "data_length", Json::NUMBER },
		{ "array_bound", Json::NUMBER },
		{ "data", Json::STRING },
	};

	if (!json.has_shape(shape, err)) {
		throw std::exception("Error reading json file: format mismatch in array property");
	}

	const std::string & dataStr = json["data"].string_value();
	std::unique_ptr<unsigned char[]> data;

	if (dataStr.length() > 0) {
		assert((dataStr.length() / 2) == (json["data_length"].int_value()));
		data = fromHex(dataStr);
	}

	return std::make_unique<XComArrayProperty>(json["name"].string_value(), std::move(data), json["data_length"].int_value(), json["array_bound"].int_value());
}

XComPropertyPtr buildObjectArrayProperty(const Json& json)
{
	std::string err;
	Json::shape shape = {
		{ "name", Json::STRING },
		{ "actors", Json::ARRAY },
	};

	if (!json.has_shape(shape, err)) {
		throw std::exception("Error reading json file: format mismatch in object array property");
	}

	std::vector<int32_t> elements;

	for (const Json& elem : json["actors"].array_items()) {
		elements.push_back(elem.int_value());
	}

	return std::make_unique<XComObjectArrayProperty>(json["name"].string_value(), elements);
}

XComPropertyPtr buildNumberArrayProperty(const Json& json)
{
	std::string err;
	Json::shape shape = {
		{ "name", Json::STRING },
		{ "elements", Json::ARRAY },
	};

	if (!json.has_shape(shape, err)) {
		throw std::exception("Error reading json file: format mismatch in object array property");
	}

	std::vector<int32_t> elements;

	for (const Json& elem : json["elements"].array_items()) {
		elements.push_back(elem.int_value());
	}

	return std::make_unique<XComNumberArrayProperty>(json["name"].string_value(), elements);
}

XComPropertyPtr buildStructArrayProperty(const Json& json)
{
	std::string err;

	Json::shape shape = {
		{ "name", Json::STRING },
		{ "structs", Json::ARRAY },
	};

	if (!json.has_shape(shape, err)) {
		throw std::exception("Error reading json file: format mismatch in object array property");
	}

	std::vector<XComPropertyList> elements;

	for (const Json& elem : json["structs"].array_items()) {
		elements.push_back(buildPropertyList(elem));
	}

	return std::make_unique<XComStructArrayProperty>(json["name"].string_value(), std::move(elements));
}

XComPropertyPtr buildStaticArrayProperty(const Json& json)
{
	std::string err;
	Json::shape shape = {
		{ "name", Json::STRING },
		{ "properties", Json::ARRAY },
	};

	if (!json.has_shape(shape, err)) {
		throw std::exception("Error reading json file: format mismatch in static array property");
	}

	std::unique_ptr<XComStaticArrayProperty> prop = std::make_unique<XComStaticArrayProperty>(json["name"].string_value());

	for (const Json& elem : json["properties"].array_items()) {
		prop->addProperty(buildProperty(elem));
	}

	return XComPropertyPtr{ prop.release() };
}

XComPropertyPtr buildProperty(const Json& json)
{

	std::string kind = json["kind"].string_value();
	for (const PropertyDispatch &dispatch : propertyDispatch) {
		if (dispatch.name.compare(kind) == 0) {
			return dispatch.pFunc(json);
		}
	}

	std::string err = "Error reading json file: Unknown property kind: ";
	err.append(kind);
	throw std::exception(err.c_str());
}

XComPropertyList buildPropertyList(const Json& json) 
{
	XComPropertyList props;
	for (const Json& elem : json.array_items()) {
		props.push_back(buildProperty(elem));
	}

	return props;
}

XComCheckpoint buildCheckpoint(const Json& json)
{
	XComCheckpoint chk;
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
		throw std::exception("Error reading json file: format mismatch in checkpoint");
	}

	chk.name = json["name"].string_value();
	chk.instanceName = json["instance_name"].string_value();
	chk.vector = buildArray<float>(json["vector"]);
	chk.rotator = buildArray<int>(json["rotator"]);
	chk.className = json["class_name"].string_value();
	chk.properties = buildPropertyList(json["properties"]);
	chk.templateIndex = json["template_index"].int_value();
	chk.padSize = json["pad_size"].int_value();
	return chk;
}

XComCheckpointTable buildCheckpointTable(const Json& json)
{
	XComCheckpointTable table;
	for (const Json& elem : json.array_items()) {
		table.push_back(buildCheckpoint(elem));
	}

	return table;
}

XComCheckpointChunk buildCheckpointChunk(const Json& json)
{
	XComCheckpointChunk chunk;
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
		throw std::exception("Error reading json file: format mismatch in checkpoint chunk");
	}

	chunk.unknownInt1 = json["unknown_int1"].int_value();
	chunk.gameType = json["game_type"].string_value();
	chunk.checkpointTable = buildCheckpointTable(json["checkpoint_table"]);
	chunk.unknownInt2 = json["unknown_int2"].int_value();
	chunk.className = json["class_name"].string_value();
	chunk.actorTable = buildActorTable(json["actor_table"]);
	chunk.unknownInt3 = json["unknown_int3"].int_value();
	chunk.displayName = json["display_name"].string_value();
	chunk.mapName = json["map_name"].string_value();
	chunk.unknownInt4 = json["unknown_int4"].int_value();
	return chunk;
}

XComCheckpointChunkTable buildCheckpoints(const Json& json)
{
	XComCheckpointChunkTable table;
	for (const Json& elem : json.array_items()) {
		table.push_back(buildCheckpointChunk(elem));
	}

	return table;
}

XComSave buildSave(const Json& json)
{
	XComSave save;
	std::string err;
	Json::shape shape = {
		{ "header", Json::OBJECT },
		{ "actor_table", Json::ARRAY },
		{ "checkpoints", Json::ARRAY }
	};

	if (!json.has_shape(shape, err)) {
		throw std::exception("Error reading json file: format mismatch at root\n");
	}

	save.header = buildHeader(json["header"]);
	save.actorTable = buildActorTable(json["actor_table"]);
	save.checkpoints = buildCheckpoints(json["checkpoints"]);
	return save;
}

void usage(const char * name)
{
	printf("Usage: %s -i <infile> -o <outfile>\n", name);
}

char * read_file(const std::string& filename, long *fileLen)
{
	FILE *fp = fopen(filename.c_str(), "rb");
	if (fp == nullptr) {
		fprintf(stderr, "Error opening file %s\n", filename.c_str());
		return nullptr;
	}

	if (fseek(fp, 0, SEEK_END) != 0) {
		fprintf(stderr, "Error determining file length\n");
		return nullptr;
	}

	*fileLen = ftell(fp);

	if (fseek(fp, 0, SEEK_SET) != 0) {
		fprintf(stderr, "Error determining file length\n");
		return nullptr;
	}

	char* filebuf = new char[(*fileLen) + 1];
	if (fread(filebuf, 1, *fileLen, fp) != *fileLen) {
		fprintf(stderr, "Error reading file contents\n");
		return nullptr;
	}
	filebuf[(*fileLen)] = 0;
	fclose(fp);
	return filebuf;
}

int main(int argc, char *argv[])
{
	bool writesave = false;
	const char *infile = nullptr;
	const char *outfile = nullptr;

	if (argc <= 1) {
		usage(argv[0]);
		return 1;
	}

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-i") == 0) {
			infile = argv[++i];
		}
		else if (strcmp(argv[i], "-o") == 0) {
			outfile = argv[++i];
		}
	}

	if (infile == nullptr || outfile == nullptr) {
		usage(argv[0]);
		return 1;
	}

	long fileLen = 0;
	auto fileBuf = read_file(infile, &fileLen);

	if (fileBuf == nullptr) {
		return 1;
	}

	std::string errStr;
	Json jsonsave = Json::parse(fileBuf, errStr);
	delete[] fileBuf;

	try {
		XComSave save = buildSave(jsonsave);
		XComWriter writer(std::move(save));
		Buffer b = writer.getSaveData();

		FILE *fp = fopen(outfile, "wb");
		fwrite(b.buf.get(), 1, b.len, fp);
		fclose(fp);
	}
	catch (std::exception e)
	{
		fprintf(stderr, e.what());
		return 1;
	}
}