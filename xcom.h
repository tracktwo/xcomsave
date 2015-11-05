#ifndef XCOM_H
#define XCOM_H

#include <stdint.h>
#include <vector>
#include <array>
#include <memory>
#include <algorithm>


static const int Chunk_Magic = 0x9e2a83c1;

struct XComSaveHeader
{
	uint32_t version;
	uint32_t uncompressedSize;
	uint32_t gameNumber;
	uint32_t saveNumber;
	std::string saveDescription;
	std::string time;
	std::string mapCommand;
	bool tacticalSave;
	bool ironman;
	bool autoSave;
	std::string dlcString;
	std::string language;
	uint32_t crc;
};

struct XComActor
{
	std::pair<std::string, std::string> actorName;
	uint32_t instanceNum;
};

using XComActorTable = std::vector<XComActor>;

struct XComPropertyVisitor;

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

	XComProperty(const std::string &n, uint32_t s, Kind k) : name(n), size(s), kind(k) {}

	Kind getKind() const {
		return kind;
	}

	std::string getName() const {
		return name;
	}

	uint32_t getSize() const {
		return size;
	}

	virtual void accept(XComPropertyVisitor* v) = 0;

protected:
	std::string name;
	uint32_t size;
	Kind kind;
};

struct XComIntProperty;
struct XComFloatProperty;
struct XComBoolProperty;
struct XComStringProperty;
struct XComObjectProperty;
struct XComByteProperty;
struct XComStructProperty;
struct XComArrayProperty;
struct XComStaticArrayProperty;

struct XComPropertyVisitor
{
	virtual void visitInt(XComIntProperty*) = 0;
	virtual void visitFloat(XComFloatProperty*) = 0;
	virtual void visitBool(XComBoolProperty*) = 0;
	virtual void visitString(XComStringProperty*) = 0;
	virtual void visitObject(XComObjectProperty*) = 0;
	virtual void visitByte(XComByteProperty*) = 0;
	virtual void visitStruct(XComStructProperty*) = 0;
	virtual void visitArray(XComArrayProperty*) = 0;
	virtual void visitStaticArray(XComStaticArrayProperty*) = 0;
};

using XComPropertyPtr = std::unique_ptr<XComProperty>;
using XComPropertyList = std::vector<XComPropertyPtr>;

struct XComObjectProperty : public XComProperty
{
	XComObjectProperty(const std::string &n, uint32_t s, std::vector<unsigned char>&& d) :
		XComProperty(n, s, Kind::ObjectProperty), data(std::move(d)) {}

	std::vector<unsigned char> data;

	void accept(XComPropertyVisitor *v) {
		v->visitObject(this);
	}
};

struct XComIntProperty : public XComProperty
{
	XComIntProperty(const std::string& n, uint32_t s, uint32_t v) :
		XComProperty(n, s, Kind::IntProperty), value(v) {}

	uint32_t value;

	void accept(XComPropertyVisitor *v) {
		v->visitInt(this);
	}
};

struct XComBoolProperty : public XComProperty
{
	XComBoolProperty(const std::string &n, uint32_t s, bool v) :
		XComProperty(n, s, Kind::BoolProperty), value(v) {}

	bool value;

	void accept(XComPropertyVisitor *v) {
		v->visitBool(this);
	}
};

struct XComArrayProperty : public XComProperty
{
	XComArrayProperty(const std::string& n, uint32_t s, std::unique_ptr<unsigned char[]>&& a, uint32_t b) :
		XComProperty(n, s, Kind::ArrayProperty), data(std::move(a)), arrayBound(b) {}

	std::unique_ptr<unsigned char[]> data;
	uint32_t arrayBound;

	void accept(XComPropertyVisitor *v) {
		v->visitArray(this);
	}
};

struct XComByteProperty : public XComProperty
{
	XComByteProperty(const std::string& n, uint32_t s, const std::string &et, const std::string &ev, uint32_t i) :
		XComProperty(n, s, Kind::ByteProperty), enumType(et), enumVal(ev), extVal(i) {}

	std::string enumType;
	std::string enumVal;
	uint32_t extVal;

	void accept(XComPropertyVisitor *v) {
		v->visitByte(this);
	}
};

struct XComFloatProperty : public XComProperty
{
	XComFloatProperty(const std::string &n, uint32_t s, float f) :
		XComProperty(n, s, Kind::FloatProperty), value(f) {}

	float value;

	void accept(XComPropertyVisitor *v) {
		v->visitFloat(this);
	}
};

struct XComStructProperty : public XComProperty
{
	XComStructProperty(const std::string &n, uint32_t s, const std::string &sn, XComPropertyList &&propList) :
		XComProperty(n, s, Kind::StructProperty), structName(sn), structProps(std::move(propList)), nativeData(), nativeDataLen(0) {}

	XComStructProperty(const std::string& n, uint32_t s, const std::string &sn, std::unique_ptr<unsigned char[]> &&nd, uint32_t l) :
		XComProperty(n, s, Kind::StructProperty), structName(sn), nativeData(std::move(nd)), nativeDataLen(l) {}

	std::string structName;
	XComPropertyList structProps;
	std::unique_ptr<unsigned char[]> nativeData;
	uint32_t nativeDataLen;

	void accept(XComPropertyVisitor *v) {
		v->visitStruct(this);
	}
};

struct XComStringProperty : public XComProperty
{
	XComStringProperty(const std::string& n, uint32_t sz, const std::string& s) :
		XComProperty(n, sz, Kind::StrProperty), str(s) {}

	std::string str;

	void accept(XComPropertyVisitor *v) {
		v->visitString(this);
	}
};

struct XComStaticArrayProperty : public XComProperty
{
	XComStaticArrayProperty(const std::string& n) :
		XComProperty(n, 0, Kind::StaticArrayProperty) {}

	void addProperty(XComPropertyPtr&& ptr)
	{
		properties.push_back(std::move(ptr));
	}

	size_t size() const
	{
		return properties.size();
	}

	void accept(XComPropertyVisitor *v) {
		v->visitStaticArray(this);
	}

	XComPropertyList properties;
};

using UVector = std::array<float, 3>;
using URotator = std::array<int, 3>;

struct XComCheckpoint
{
	std::string name;
	std::string instanceName;
	UVector vector;
	URotator rotator;
	std::string className;
	uint32_t propLen;
	XComPropertyList properties;
	uint32_t templateIndex;
	uint32_t padSize;
};

using XComCheckpointTable = std::vector<XComCheckpoint>;

struct XComActorTemplate
{
	std::string actorClassPath;
	std::string archetypePath;
	unsigned char loadParams[64];
};

using XComActorTemplateTable = std::vector<XComActorTemplate>;

struct XComNameEntry
{
	std::string name;
	unsigned char zeros[8];
	uint32_t dataLen;
	unsigned char *data;
};

using XComNameTable = std::vector<XComNameEntry>;

struct XComCheckpointChunk
{
	uint32_t unknownInt1;
	std::string gameType;
	XComCheckpointTable checkpointTable;
	uint32_t unknownInt2;
	std::string className;
	XComActorTable actorTable;
	uint32_t unknownInt3;
	std::string displayName;
	std::string mapName;
	uint32_t unknownInt4;
};

using XComCheckpointChunkTable = std::vector<XComCheckpointChunk>;

struct XComSave
{
	XComSaveHeader header;
	XComActorTable actorTable;
	XComCheckpointChunkTable checkpoints;
};

#endif // XCOM_H
