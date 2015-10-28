#ifndef XCOM_H
#define XCOM_H

#include <stdint.h>
#include <vector>
#include <memory>
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

protected:
	std::string name;
	Kind kind;
};

using XComPropertyPtr = std::unique_ptr<XComProperty>;
using XComPropertyList = std::vector<XComPropertyPtr>;

struct XComObjectProperty : public XComProperty
{
	XComObjectProperty(const std::string &n, unsigned char* d, uint32_t l) :
		XComProperty(n, Kind::ObjectProperty), data(d), length(l) {}

	unsigned char *data;
	uint32_t length;
};

struct XComIntProperty : public XComProperty
{
	XComIntProperty(const std::string& n, uint32_t v) :
		XComProperty(n, Kind::IntProperty), value(v) {}

	uint32_t value;
};

struct XComBoolProperty : public XComProperty
{
	XComBoolProperty(const std::string &n, bool v) :
		XComProperty(n, Kind::BoolProperty), value(v) {}

	bool value;
};

struct XComArrayProperty : public XComProperty
{
	XComArrayProperty(const std::string& n, unsigned char *a, uint32_t b, uint32_t elem) :
		XComProperty(n, Kind::ArrayProperty), data(a), arrayBound(b), elementSize(elem) {}

	unsigned char *data;
	uint32_t arrayBound;
	uint32_t elementSize;
};

struct XComByteProperty : public XComProperty
{
	XComByteProperty(const std::string& n, const std::string &et, const std::string &ev, uint32_t i) :
		XComProperty(n, Kind::ByteProperty), enumType(et), enumVal(ev), extVal(i) {}

	std::string enumType;
	std::string enumVal;
	uint32_t extVal;
};

struct XComFloatProperty : public XComProperty
{
	XComFloatProperty(const std::string &n, float f) :
		XComProperty(n, Kind::FloatProperty), value(f) {}

	float value;
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
};

struct XComStrProperty : public XComProperty
{
	XComStrProperty(const std::string& n, const std::string& s) :
		XComProperty(n, Kind::StrProperty), str(s) {}

	std::string str;
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
private:
	XComPropertyList properties;
};

struct XComCheckpoint
{
	char *fullName;
	char *instanceName;
	char unknown1[24];
	char *className;
	XComPropertyList properties;
	uint32_t templateIndex;
};

struct XComCheckpointTable
{
	uint32_t checkpointCount;
	XComCheckpoint *checkpoints;
};

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

struct XComSave
{
	XComSaveHeader header;
	XComActorTable actorTable;
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

};

#endif // XCOM_H
