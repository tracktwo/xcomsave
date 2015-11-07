
#include "xcom.h"
#include "xcomreader.h"
#include "json11.hpp"
#include "util.h"

#include <iostream>

using namespace json11;

struct JsonPropertyVisitor : public XComPropertyVisitor
{
	virtual void visitInt(XComIntProperty *prop) override
	{
		json = Json::object {
			{ "name", prop->getName() },
			{ "kind", "IntProperty" },
			{ "value", (int)prop->value }
		};
	}

	virtual void visitFloat(XComFloatProperty *prop) override
	{
		json = Json::object{
			{ "name", prop->getName() },
			{ "kind", "FloatProperty" },
			{ "value", prop->value }
		};
	}

	virtual void visitBool(XComBoolProperty *prop) override
	{
		json = Json::object{
			{ "name", prop->getName() },
			{ "kind", "BoolProperty" },
			{ "value", prop->value }
		};
	}

	virtual void visitString(XComStringProperty *prop) override
	{
		json = Json::object{
			{ "name", prop->getName() },
			{ "kind", "StringProperty" },
			{ "value", prop->str }
		};
	}

	virtual void visitObject(XComObjectProperty *prop) override
	{
		json = Json::object{
			{ "name", prop->getName() },
			{ "kind", "ObjectProperty" },
			{ "data", prop->data }
		};
	}

	virtual void visitByte(XComByteProperty *prop) override
	{
		json = Json::object{
			{ "name", prop->getName() },
			{ "kind", "ByteProperty" },
			{ "type", prop->enumType },
			{ "value", prop->enumVal },
			{ "extra_value", (int)prop->extVal }
		};
	}

	virtual void visitStruct(XComStructProperty *prop) override
	{
		std::vector<Json> jsonProps;
		std::for_each(prop->structProps.begin(), prop->structProps.end(),
			[&jsonProps](const XComPropertyPtr& v) { 
				JsonPropertyVisitor visitor;
				v->accept(&visitor);
				jsonProps.push_back(visitor.json); 
		});

		std::string dataStr;
		if (prop->nativeDataLen > 0) {
			uint32_t strLen = prop->nativeDataLen * 2 + 1;
			dataStr = toHex(prop->nativeData.get(), prop->nativeDataLen);
		}
		else {
			dataStr = "";
		}

		json = Json::object{
			{ "name", prop->getName() },
			{ "struct_name", prop->structName },
			{ "kind", "StructProperty" },
			{ "properties", jsonProps },
			{ "native_data", dataStr }
		};
	}

	virtual void visitArray(XComArrayProperty *prop) override
	{
		std::string dataStr = (prop->arrayBound > 0) ? toHex(prop->data.get(), prop->data_length) : "";
		json = Json::object{
			{ "name", prop->getName() },
			{ "data_length", (int)prop->data_length },
			{ "kind", "ArrayProperty" },
			{ "array_bound", (int)prop->arrayBound },
			{ "data", dataStr }
		};
	}

	virtual void visitStaticArray(XComStaticArrayProperty *prop) override
	{
		std::vector<Json> jsonProps;
		std::for_each(prop->properties.begin(), prop->properties.end(),
			[&jsonProps](const XComPropertyPtr& v) { 
				JsonPropertyVisitor visitor;
				v->accept(&visitor);
				jsonProps.push_back(visitor.json); 
		});

		// Static array properties don't really exist in the save file and don't have a size.
		json = Json::object{
			{ "name", prop->getName() },
			{ "kind", "StaticArrayProperty" },
			{ "properties", jsonProps }
		};
	}

	Json json;
};

static Json XComActorToJson(const XComActor& actor)
{
	std::string actorName = actor.actorName.first;
	actorName.append(".").append(actor.actorName.second);
	return Json::object{
		{ "instance_num", (int)actor.instanceNum },
		{ "name", actorName }
	};
}

static Json XComCheckpointToJson(const XComCheckpoint& chk)
{
	std::vector<Json> propertyJson;
	std::for_each(chk.properties.begin(), chk.properties.end(),
		[&propertyJson](const XComPropertyPtr& v) { 
			JsonPropertyVisitor visitor;
			v->accept(&visitor);
			propertyJson.push_back(visitor.json); 
	});

	return Json::object{
		{ "name", chk.name },
		{ "instance_name", chk.instanceName },
		{ "vector", chk.vector },
		{ "rotator", chk.rotator },
		{ "class_name", chk.className },
		{ "properties", propertyJson },
		{ "template_index", (int)chk.templateIndex },
		{ "pad_size", (int)chk.padSize }
	};
}

static Json XComCheckpointChunkToJson(const XComCheckpointChunk& chk)
{
	std::vector<Json> checkpointTableJson;
	std::for_each(chk.checkpointTable.begin(), chk.checkpointTable.end(),
		[&checkpointTableJson](const XComCheckpoint& v) { checkpointTableJson.push_back(XComCheckpointToJson(v)); }
	);

	std::vector<Json> actorTableJson;
	std::for_each(chk.actorTable.begin(), chk.actorTable.end(),
		[&actorTableJson](const XComActor& v) { actorTableJson.push_back(XComActorToJson(v)); }
	);

	return Json::object{
			{ "unknown_int1", (int)chk.unknownInt1 },
			{ "game_type", chk.gameType },
			{ "checkpoint_table", checkpointTableJson },
			{ "unknown_int2", (int)chk.unknownInt2	},
			{ "class_name", chk.className },
			{ "actor_table", actorTableJson },
			{ "unknown_int3", (int)chk.unknownInt3 },
			{ "display_name", chk.displayName },
			{ "map_name", chk.mapName },
			{ "unknown_int4", (int)chk.unknownInt4 }
	};
}

static Json XComCheckpointChunkTableToJson(const XComCheckpointChunkTable & table)
{
	std::vector<Json> chunks;
	for (size_t i = 0; i < table.size(); ++i) {
		chunks.push_back(XComCheckpointChunkToJson(table[i]));
	}

	return Json(std::move(chunks));
}

Json buildJson(const XComSave& save)
{
	const XComSaveHeader &hdr = save.header;

	Json header = Json::object{ 
		{ "version", (int)hdr.version },
		{ "uncompressed_size", (int)hdr.uncompressed_size },
		{ "game_number", (int) hdr.game_number },
		{ "save_number", (int) hdr.save_number },
		{ "save_description", hdr.save_description },
		{ "time", hdr.time },
		{ "map_command", hdr.map_command },
		{ "tactical_save", hdr.tactical_save },
		{ "ironman", hdr.ironman },
		{ "autosave", hdr.autosave },
		{ "dlc", hdr.dlc },
		{ "language", hdr.language }
	};

	std::vector<Json> actorTableJson;
	std::for_each(save.actorTable.begin(), save.actorTable.end(),
		[&actorTableJson](const XComActor& v) { actorTableJson.push_back(XComActorToJson(v)); }
	);

	Json jsonSave = Json::object{
		{ "header", header },
		{ "actor_table", actorTableJson },
		{ "checkpoints", XComCheckpointChunkTableToJson(save.checkpoints) },
	};

	return jsonSave;
}

void usage(const char * name)
{
	printf("Usage: %s -i <infile> -o <outfile>\n", name);
}

Buffer read_file(const std::string& filename)
{
	Buffer buffer;
	FILE *fp = fopen(filename.c_str(), "rb");
	if (fp == nullptr) {
		fprintf(stderr, "Error opening file\n");
		return{};
	}

	if (fseek(fp, 0, SEEK_END) != 0) {
		fprintf(stderr, "Error determining file length\n");
		return{};
	}

	buffer.len = ftell(fp);

	if (fseek(fp, 0, SEEK_SET) != 0) {
		fprintf(stderr, "Error determining file length\n");
		return{};
	}

	buffer.buf = std::make_unique<unsigned char[]>(buffer.len);
	if (fread(buffer.buf.get(), 1, buffer.len, fp) != buffer.len) {
		fprintf(stderr, "Error reading file contents\n");
		return{};
	}

	fclose(fp);
	return buffer;
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
	Buffer fileBuf = read_file(infile);

	if (fileBuf.len == 0) {
		return 1;
	}

	try {
		XComReader reader{ std::move(fileBuf) };
		XComSave save = reader.getSaveData();
		Json jsonsave = buildJson(save);
		std::string str = jsonsave.dump();
		FILE *fp = fopen(outfile, "w");
		fwrite(str.c_str(), 1, str.length(), fp);
		fclose(fp);
	}
	catch (std::exception e)
	{
		fprintf(stderr, e.what());
		return 1;
	}
}
