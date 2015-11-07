
#include "xcom.h"
#include "xcomreader.h"
#include "util.h"

#include <string>
#include <iostream>
#include <fstream>

static std::string escape(const std::string& str) {
	std::string ret;

	for (size_t i = 0; i < str.length(); ++i) {
		switch (str[i])
		{
		case '"':
			ret += "\\\"";
			break;
		case '\\':
			ret += "\\\\";
			break;
		case '\n':
			ret += "\\n";
			break;
		case '\r':
			ret += "\\r";
			break;
		case '\t':
			ret += "\\t";
			break;
		default:
			if (str[i] > 0 && str[i] < ' ') {
				std::string hex = toHex(reinterpret_cast<const unsigned char *>(&str[i]), 1);
				ret += "\\u00";
				ret += hex;
			}
			else {
				ret += str[i];
			}
		}
	}

	return ret;
}

struct JsonWriter
{
	JsonWriter(const std::string& filename)
	{
		out = std::ofstream{ filename };
		out.setf(std::ofstream::boolalpha);
		indent_level = 0;
		needs_comma = false;
		skip_indent = true;
	}

	void indent()
	{
		if (needs_comma) {
			out << ", ";

		}
		if (!skip_indent) {
			out << std::endl;
			std::string ind( 2*indent_level, ' ');
			out << ind;
		}
	}

	void beginObject(bool omitNewline = false)
	{
		indent();
		out << "{ ";
		++indent_level;
		needs_comma = false;
		skip_indent = omitNewline;
	}

	void endObject()
	{
		--indent_level;
		if (needs_comma) {
			out << " ";
		}
		needs_comma = false;
		//skip_indent = false;
		indent();
		out << "}";
		needs_comma = true;
		skip_indent = false;
	}

	void beginArray(bool omitNewline = false)
	{
		indent();
		out << "[ ";
		++indent_level;
		needs_comma = false;
		skip_indent = omitNewline;
	}

	void endArray()
	{
		--indent_level;
		if (needs_comma) {
			out << " ";
		}
		needs_comma = false;
		//skip_indent = false;
		indent();
		out << "]";
		needs_comma = true;
		skip_indent = false;
	}

	void enditem(bool omitNewline)
	{
		if (!omitNewline) {
			skip_indent = false;
		}
		else {
			skip_indent = true;
		}

		needs_comma = true;
	}

	void writeKey(const std::string &name)
	{
		indent();
		out << "\"" << name << "\": ";
		skip_indent = true;
		needs_comma = false;
	}

	void writeInt(const std::string &name, uint32_t val, bool omitNewline = false)
	{
		writeKey(name);
		out << (int)val;
		enditem(omitNewline);
	}

	void writeBareInt(int val, bool omitNewLine = false)
	{
		indent();
		out << val;
		enditem(omitNewLine);
	}

	void writeFloat(const std::string &name, float val, bool omitNewline = false)
	{
		writeKey(name);
		out << val;
		enditem(omitNewline);
	}

	void writeBareFloat(float val, bool omitNewLine = false)
	{
		indent();
		out << val;
		enditem(omitNewLine);
	}

	void writeString(const std::string &name, const std::string &val, bool omitNewline = false)
	{
		writeKey(name);
		out << "\"" << escape(val) << "\"";
		enditem(omitNewline);
	}

	void writeBool(const std::string &name, bool val, bool omitNewline = false)
	{
		writeKey(name);
		out << val;
		enditem(omitNewline);
	}


private:
	std::ofstream out;
	size_t indent_level;
	bool skip_indent;
	bool needs_comma;
};



struct JsonPropertyVisitor : public XComPropertyVisitor
{
	JsonPropertyVisitor(JsonWriter &writer) : w(writer) {}

	void writeCommon(XComProperty* prop, bool omitNewlines = false)
	{
		w.writeString("name", prop->getName(), omitNewlines);
		w.writeString("kind", prop->kind_string(), omitNewlines);
	}

	virtual void visitInt(XComIntProperty *prop) override
	{
		w.beginObject(true);
		writeCommon(prop, true);
		w.writeInt("value", prop->value, true);
		w.endObject();
	}

	virtual void visitFloat(XComFloatProperty *prop) override
	{
		w.beginObject(true);
		writeCommon(prop, true);
		w.writeFloat("value", prop->value, true);
		w.endObject();
	}

	virtual void visitBool(XComBoolProperty *prop) override
	{
		w.beginObject(true);
		writeCommon(prop, true);
		w.writeBool("value", prop->value, true);
		w.endObject();
	}

	virtual void visitString(XComStringProperty *prop) override
	{
		w.beginObject(true);
		writeCommon(prop, true);
		w.writeString("value", prop->str, true);
		w.endObject();
	}

	virtual void visitObject(XComObjectProperty *prop) override
	{
		w.beginObject(true);
		writeCommon(prop, true);
		w.writeInt("actor1", prop->actor1, true);
		w.writeInt("actor2", prop->actor2, true);
		w.endObject();
	}

	virtual void visitByte(XComByteProperty *prop) override
	{
		w.beginObject();
		writeCommon(prop);
		w.writeString("type", prop->enumType);
		w.writeString("value", prop->enumVal);
		w.writeInt("extra_value", prop->extVal);
		w.endObject();
	}

	virtual void visitStruct(XComStructProperty *prop) override
	{
		w.beginObject();
		writeCommon(prop);
		w.writeString("struct_name", prop->structName);

		if (prop->nativeDataLen > 0) {
			uint32_t strLen = prop->nativeDataLen * 2 + 1;
			w.writeString("native_data", toHex(prop->nativeData.get(), prop->nativeDataLen));
			w.writeKey("properties");
			w.beginArray(true);
			w.endArray();
		}
		else {
			w.writeString("native_data", "");
			w.writeKey("properties");
			w.beginArray();
			std::for_each(prop->structProps.begin(), prop->structProps.end(),
				[this](const XComPropertyPtr& v) {
				JsonPropertyVisitor visitor{ w };
				v->accept(&visitor);
			});
			w.endArray();
		}
		w.endObject();
	}

	virtual void visitArray(XComArrayProperty *prop) override
	{
		w.beginObject();
		writeCommon(prop);
		w.writeInt("data_length", prop->data_length);
		w.writeInt("array_bound", prop->arrayBound);
		std::string dataStr = (prop->arrayBound > 0) ? toHex(prop->data.get(), prop->data_length) : "";
		w.writeString("data", dataStr);
		w.endObject();
	}

	virtual void visitStaticArray(XComStaticArrayProperty *prop) override
	{
		w.beginObject();
		writeCommon(prop);
		w.writeKey("properties");
		w.beginArray();

		std::for_each(prop->properties.begin(), prop->properties.end(),
			[this](const XComPropertyPtr& v) { 
				JsonPropertyVisitor visitor{ w };
				v->accept(&visitor);
		});

		w.endArray();
		w.endObject();
	}

	JsonWriter& w;
};

static void XComActorToJson(const XComActor& actor, JsonWriter& w)
{
	std::string actorName = actor.actorName.first;
	actorName.append(".").append(actor.actorName.second);

	w.beginObject(true);
	w.writeString("name", actorName, true);
	w.writeInt("instance_num", actor.instanceNum, true);
	w.endObject();
}

static void XComCheckpointToJson(const XComCheckpoint& chk, JsonWriter& w)
{
	w.beginObject();
	w.writeString("name", chk.name);
	w.writeString("instance_name", chk.instanceName);
	w.writeString("class_name", chk.className);
	w.writeKey("vector");
	w.beginArray(true);
	for (const auto& i : chk.vector) {
		w.writeBareFloat(i, true);
	}
	w.endArray();
	w.writeKey("rotator");
	w.beginArray(true);
	for (const auto& i : chk.rotator) {
		w.writeBareInt(i, true);
	}
	w.endArray();

	w.writeKey("properties");
	w.beginArray();
	std::for_each(chk.properties.begin(), chk.properties.end(),
		[&w](const XComPropertyPtr& v) { 
			JsonPropertyVisitor visitor{ w };
			v->accept(&visitor);
	});
	w.endArray();

	w.writeInt("template_index", chk.templateIndex);
	w.writeInt("pad_size", chk.padSize);
	w.endObject();
}

static void XComCheckpointChunkToJson(const XComCheckpointChunk& chk, JsonWriter &w)
{
	w.beginObject();
	w.writeInt("unknown_int1", chk.unknownInt1);
	w.writeString("game_type", chk.gameType);
	w.writeKey("checkpoint_table");
	w.beginArray();
	std::for_each(chk.checkpointTable.begin(), chk.checkpointTable.end(),
		[&w](const XComCheckpoint& v) { XComCheckpointToJson(v, w); }
	);
	w.endArray();

	w.writeInt("unknown_int2", chk.unknownInt2);
	w.writeString("class_name", chk.className);

	w.writeKey("actor_table");
	w.beginArray();
	std::for_each(chk.actorTable.begin(), chk.actorTable.end(),
		[&w](const XComActor& v) { XComActorToJson(v, w); }
	);
	w.endArray();

	w.writeInt("unknown_int3", chk.unknownInt3);
	w.writeString("display_name", chk.displayName);
	w.writeString("map_name", chk.mapName);
	w.writeInt("unknown_int4", chk.unknownInt4);
	w.endObject();
}

static void XComCheckpointChunkTableToJson(const XComCheckpointChunkTable & table, JsonWriter& w)
{
	for (size_t i = 0; i < table.size(); ++i) {
		XComCheckpointChunkToJson(table[i], w);
	}
}

void buildJson(const XComSave& save, JsonWriter& w)
{
	w.beginObject();

	// Write the header
	const XComSaveHeader &hdr = save.header;

	w.writeKey("header");
	w.beginObject();
	w.writeInt("version", hdr.version);
	w.writeInt("uncompressed_size", hdr.uncompressed_size);
	w.writeInt("game_number", hdr.game_number);
	w.writeInt("save_number", hdr.save_number);
	w.writeString("save_description", hdr.save_description);
	w.writeString("time", hdr.time);
	w.writeString("map_command", hdr.map_command);
	w.writeBool("tactical_save", hdr.tactical_save);
	w.writeBool("ironman", hdr.ironman);
	w.writeBool("autosave", hdr.autosave);
	w.writeString("dlc", hdr.dlc);
	w.writeString("language", hdr.language);
	w.endObject();

	w.writeKey("actor_table");
	w.beginArray();
	std::for_each(save.actorTable.begin(), save.actorTable.end(),
		[&w](const XComActor& v) { XComActorToJson(v, w); w.enditem(false); }
	);
	w.endArray();

	w.writeKey("checkpoints");
	w.beginArray();
	std::for_each(save.checkpoints.begin(), save.checkpoints.end(),
		[&w](const XComCheckpointChunk& v) { XComCheckpointChunkToJson(v, w); w.enditem(false); }
	);
	w.endArray();
	w.endObject();
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
		JsonWriter writer{ outfile };
		buildJson(save, writer);
	}
	catch (std::exception e)
	{
		fprintf(stderr, e.what());
		return 1;
	}
}
