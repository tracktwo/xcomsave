#include "jsonwriter.h"
#include <windows.h>
#include <wincrypt.h>

struct JsonPropertyVisitor : public XComPropertyVisitor
{
	// Inherited via XComPropertyVisitor
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

	virtual void visitString(XComStrProperty *prop) override
	{
		json = Json::object{
			{ "name", prop->getName() },
			{ "kind", "StrProperty" },
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

		unsigned long strLen;
		char *dataStr;
		//TODO memleak
		if (prop->nativeDataLen > 0) {
			CryptBinaryToString(prop->nativeData, prop->nativeDataLen, CRYPT_STRING_HEXRAW | CRYPT_STRING_NOCRLF, nullptr, &strLen);
			dataStr = new char[strLen];
			CryptBinaryToString(prop->nativeData, prop->nativeDataLen, CRYPT_STRING_HEXRAW | CRYPT_STRING_NOCRLF, dataStr, &strLen);
		}
		else {
			dataStr = "";
		}

		json = Json::object{
			{ "name", prop->getName() },
			{ "kind", "StructProperty" },
			{ "properties", jsonProps },
			{ "native_data", dataStr }
		};
	}

	virtual void visitArray(XComArrayProperty *prop) override
	{
		unsigned long strLen;
		char *dataStr;
		// TODO memleak
		if (prop->arrayBound > 0 && prop->elementSize > 0) {
			CryptBinaryToString(prop->data, prop->arrayBound*prop->elementSize, CRYPT_STRING_HEXRAW | CRYPT_STRING_NOCRLF, nullptr, &strLen);
			dataStr = new char[strLen];
			CryptBinaryToString(prop->data, prop->arrayBound*prop->elementSize, CRYPT_STRING_HEXRAW | CRYPT_STRING_NOCRLF, dataStr, &strLen);
		}
		else {
			dataStr = "";
		}
		json = Json::object{
			{ "name", prop->getName() },
			{ "kind", "ArrayProperty" },
			{ "array_bound", (int)prop->arrayBound },
			{ "element_size", (int)prop->elementSize },
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
	return Json::object{
		{ "instance_num", (int)actor.instanceNum },
		{ "name", actor.actorName }
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
			{ "unknown_str1", chk.unknownString1 },
			{ "checkpoint_table", checkpointTableJson },
			{ "unknown_int2", (int)chk.unknownInt2	},
			{ "unknown_str2", chk.unknownString2 },
			{ "actor_table", actorTableJson },
			{ "unknown_int3", (int)chk.unknownInt3 },
			{ "game_name", chk.gameName },
			{ "map_name", chk.mapName },
			{ "unknown_int4", (int)chk.unknownInt4 }
	};
}

static Json XComCheckpointChunkTableToJson(const XComCheckpointChunkTable & table)
{
	std::vector<Json> chunks(table.size());
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
		{ "uncompressed_size", (int)hdr.uncompressedSize },
		{ "game_number", (int) hdr.gameNumber },
		{ "save_number", (int) hdr.saveNumber },
		{ "save_description", hdr.saveDescription },
		{ "time", hdr.time },
		{ "map_command", hdr.mapCommand },
		{ "tactical_save", hdr.tacticalSave },
		{ "ironman", hdr.ironman },
		{ "autosave", hdr.autoSave },
		{ "dlc", hdr.dlcString },
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