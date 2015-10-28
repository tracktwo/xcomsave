#ifndef XCOM_H
#define XCOM_H

#include <stdint.h>
#include <vector>
#include <array>
#include <memory>
#include <algorithm>
#include <windows.h>
#include <wincrypt.h>
#include "json11.hpp"


using namespace json11;

struct XComSaveHeader
{
	uint32_t version;
	uint32_t uncompressedSize;
	uint32_t gameNumber;
	uint32_t saveNumber;
	const char* saveDescription;
	const char* time;
	const char* mapCommand;
	bool tacticalSave;
	bool ironman;
	bool autoSave;
	const char* dlcString;
	const char* language;
	uint32_t crc;
};

struct XComActor
{
	std::string actorName;
	uint32_t instanceNum;
	
	Json to_json() const {
		return Json::object {
			{ "instance_num", (int)instanceNum },
			{ "name", actorName }
		};
	}
};

using XComActorTable = std::vector<XComActor>;

struct XComProperty
{
	enum class Kind
	{
		IntProperty,
		FloatProperty,
		BoolProperty,
		StrProperty,
		ObjectProperty,
		ByteProperty,
		StructProperty,
		ArrayProperty,
		StaticArrayProperty
	};

	XComProperty(const std::string &n, Kind k) : name(n), kind(k) {}
	Kind getKind() const {
		return kind;
	}

	std::string getName() const {
		return name;
	}

	virtual Json to_json() const = 0;

protected:
	std::string name;
	Kind kind;
};

using XComPropertyPtr = std::unique_ptr<XComProperty>;
using XComPropertyList = std::vector<XComPropertyPtr>;

struct XComObjectProperty : public XComProperty
{
	XComObjectProperty(const std::string &n, std::vector<unsigned char> d) :
		XComProperty(n, Kind::ObjectProperty), data(d) {}

	//unsigned char *data;
	std::vector<unsigned char> data;

	Json to_json() const
	{
		return Json::object{
			{ "name", name },
			{ "kind", "ObjectProperty" },
			{ "data", data }
		};
	}
};

struct XComIntProperty : public XComProperty
{
	XComIntProperty(const std::string& n, uint32_t v) :
		XComProperty(n, Kind::IntProperty), value(v) {}

	uint32_t value;

	Json to_json() const
	{
		return Json::object{
			{ "name", name },
			{ "kind", "IntProperty" },
			{ "value", (int) value }
		};
	}
};

struct XComBoolProperty : public XComProperty
{
	XComBoolProperty(const std::string &n, bool v) :
		XComProperty(n, Kind::BoolProperty), value(v) {}

	bool value;

	Json to_json() const
	{
		return Json::object{
			{ "name", name },
			{ "kind", "BoolProperty" },
			{ "value", value }
		};
	}
};

struct XComArrayProperty : public XComProperty
{
	XComArrayProperty(const std::string& n, unsigned char *a, uint32_t b, uint32_t elem) :
		XComProperty(n, Kind::ArrayProperty), data(a), arrayBound(b), elementSize(elem) {}

	unsigned char *data;
	uint32_t arrayBound;
	uint32_t elementSize;

	Json to_json() const
	{
		unsigned long strLen;
		char *dataStr;
		if (arrayBound > 0 && elementSize > 0) {
			CryptBinaryToString(data, arrayBound*elementSize, CRYPT_STRING_HEXRAW | CRYPT_STRING_NOCRLF, nullptr, &strLen);
			dataStr = new char[strLen];
			CryptBinaryToString(data, arrayBound*elementSize, CRYPT_STRING_HEXRAW | CRYPT_STRING_NOCRLF, dataStr, &strLen);
		}
		else {
			dataStr = "";
		}
		return Json::object{
			{ "name", name },
			{ "kind", "ArrayProperty" },
			{ "array_bound", (int)arrayBound },
			{ "element_size", (int)elementSize },
			{ "data", dataStr }
		};
	}
};

struct XComByteProperty : public XComProperty
{
	XComByteProperty(const std::string& n, const std::string &et, const std::string &ev, uint32_t i) :
		XComProperty(n, Kind::ByteProperty), enumType(et), enumVal(ev), extVal(i) {}

	std::string enumType;
	std::string enumVal;
	uint32_t extVal;

	Json to_json() const
	{
		return Json::object{
			{ "name", name },
			{ "kind", "ByteProperty" },
			{ "type", enumType },
			{ "value", enumVal },
			{ "extra_value", (int) extVal }
		};
	}
};

struct XComFloatProperty : public XComProperty
{
	XComFloatProperty(const std::string &n, float f) :
		XComProperty(n, Kind::FloatProperty), value(f) {}

	float value;

	Json to_json() const
	{
		return Json::object{
			{ "name", name },
			{ "kind", "FloatProperty" },
			{ "value", value }
		};
	}
};

struct XComStructProperty : public XComProperty
{
	XComStructProperty(const std::string &n, const std::string &sn, XComPropertyList &&propList) :
		XComProperty(n, Kind::StructProperty), structName(sn), structProps(std::move(propList)), nativeData(nullptr), nativeDataLen(0) {}

	XComStructProperty(const std::string& n, const std::string &sn, unsigned char* nd, uint32_t l) :
		XComProperty(n, Kind::StructProperty), structName(sn), nativeData(nd), nativeDataLen(l) {}

	std::string structName;
	XComPropertyList structProps;
	unsigned char *nativeData;
	uint32_t nativeDataLen;

	Json to_json() const
	{
		std::vector<Json> jsonProps;
		std::for_each(structProps.begin(), structProps.end(),
			[&jsonProps](const XComPropertyPtr& v) { jsonProps.push_back(v->to_json()); });

		unsigned long strLen;
		char *dataStr;
		if (nativeDataLen > 0) {
			CryptBinaryToString(nativeData, nativeDataLen, CRYPT_STRING_HEXRAW | CRYPT_STRING_NOCRLF, nullptr, &strLen);
			dataStr = new char[strLen];
			CryptBinaryToString(nativeData, nativeDataLen, CRYPT_STRING_HEXRAW | CRYPT_STRING_NOCRLF, dataStr, &strLen);
		}
		else {
			dataStr = "";
		}
		return Json::object{
			{ "name", name },
			{ "kind", "StructProperty" },
			{ "properties", jsonProps },
			{ "native_data", dataStr }
		};
	}
};

struct XComStrProperty : public XComProperty
{
	XComStrProperty(const std::string& n, const std::string& s) :
		XComProperty(n, Kind::StrProperty), str(s) {}

	std::string str;

	Json to_json() const
	{
		return Json::object{
			{ "name", name },
			{ "kind", "StrProperty" },
			{ "value", str }
		};
	}
};

struct XComStaticArrayProperty : public XComProperty
{
	XComStaticArrayProperty(const std::string& n) :
		XComProperty(n, Kind::StaticArrayProperty) {}

	void addProperty(XComPropertyPtr&& ptr)
	{
		properties.push_back(std::move(ptr));
	}

	size_t size() const
	{
		return properties.size();
	}

	Json to_json() const
	{
		std::vector<Json> jsonProps;
		std::for_each(properties.begin(), properties.end(),
			[&jsonProps](const XComPropertyPtr& v) { jsonProps.push_back(v->to_json()); });

		return Json::object{
			{ "name", name },
			{ "kind", "StaticArrayProperty" },
			{ "properties", jsonProps }
		};
	}
private:
	XComPropertyList properties;
};

using UVector = std::array<float, 3>;
using URotator = std::array<float, 3>;

struct XComCheckpoint
{
	std::string name;
	std::string instanceName;
	UVector vector;
	URotator rotator;
	std::string className;
	XComPropertyList properties;
	uint32_t templateIndex;
	uint32_t padSize;

	Json to_json() const
	{
		std::vector<Json> propertyJson;
		std::for_each(properties.begin(), properties.end(), 
			[&propertyJson](const XComPropertyPtr& v) { propertyJson.push_back(v->to_json()); });

		return Json::object {
			{ "name", name },
			{ "instance_name", instanceName },
			{ "vector", vector },
			{ "rotator", rotator },
			{ "class_name", className },
			{ "properties", propertyJson },
			{ "template_index", (int)templateIndex },
			{ "pad_size", (int) padSize }
		};
	}
};

using XComCheckpointTable = std::vector<XComCheckpoint>;


struct XComActorTemplate
{
	char *actorClassPath;
	char *archetypePath;
	unsigned char loadParams[64];
};

struct XComActorTemplateTable
{
	uint32_t templateCount;
	XComActorTemplate *templates;
};

struct XComNameEntry
{
	std::string name;
	unsigned char zeros[8];
	uint32_t dataLen;
	unsigned char *data;
};

struct XComNameTable
{
	uint32_t nameCount;
	XComNameEntry *names;
};

struct XComCheckpointChunk
{
	uint32_t unknownInt1;
	std::string unknownString1;
	XComCheckpointTable checkpointTable;
	uint32_t unknownInt2;
	std::string unknownString2;
	XComActorTable actorTable;
	uint32_t unknownInt3;
	std::string gameName;
	std::string mapName;
	uint32_t unknownInt4;

	Json to_json() const {
		return Json::object{
			{ "unknown_int1", (int)unknownInt1 },
			{ "unknown_str1", unknownString1 },
			{ "checkpoint_table", checkpointTable },
			{ "unknown_int2", (int)unknownInt2	},
			{ "unknown_str2", unknownString2 },
			{ "actor_table", actorTable },
			{ "unknown_int3", (int)unknownInt3 },
			{ "game_name", gameName },
			{ "map_name", mapName },
			{ "unknown_int4", (int)unknownInt4 }
		};
	}
};

using XComCheckpointChunkTable = std::vector<XComCheckpointChunk>;
struct XComSave
{
	XComSaveHeader header;
	XComActorTable actorTable;
	XComCheckpointChunkTable checkpoints;
#if 0
	uint32_t unknownInt1; // 4 unknown bytes immediately following the actor table.
	const char *unknownStr1; // unknown string
	uint32_t unknownInt2; // 4 unknown bytes after the "None" string.
	XComCheckpointTable checkpointTable;
	uint32_t unknownInt3; // 4 unknown bytes after the checkpoint table
	XComNameTable nameTable;
	const char *unknownStr2; // Unknown string 2 
	XComActorTable actorTable2;
	XComActorTemplateTable actorTemplateTable;
	uint32_t unknownInt4; // 4 unknown bytes after 2nd actor table
	uint32_t unknownInt5; // 4 unknown bytes after 2nd actor table
	const char *unknownStr3; // Unknown string 3
	const char *unknownStr4; // Unknown string 4
	uint32_t unknownInt6;	// unknown 4 bytes after 2nd actor table
#endif
};

#endif // XCOM_H
