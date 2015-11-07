#ifndef XCOM_H
#define XCOM_H

#include <stdint.h>
#include <vector>
#include <array>
#include <memory>
#include <algorithm>
#include <exception>

// The magic number that occurs at the begining of each compressed chunk.
static const int Chunk_Magic = 0x9e2a83c1;

// The header occurs at the start of the save file and is the only uncompressed part
// of the save. The first 1024 bytes of the save are the header, although most of
// it is all zeros. The header contains two checksums: the first checksum occurs 
// at the end of the main header data blob (e.g. the data represented in this struct)
// and is the CRC32 of all the compressed data in the file. E.g. everything from byte
// 1024 on. The second checksum is the header checksum and is represented at the end
// of the header, starting at offset 1016. The first value is the 4-byte length of the
// header data checksummed. It's the first N bytes of the header up to and including the
// first CRC and the four zero bytes immediately following it. The second 4-byte integer
// is the CRC32 of the header data itself. e.g.:
//
// 0		: Header Data (version through language)
// N		: CRC32 of compressed data
// N+4-1016 : Zeros
// 1016		: Number of bytes of header checksummed (N + 4).
// 1020		: CRC32 of the first N+4 bytes of header.
// 1024-end	: Compressed data
struct XComSaveHeader
{
	// XCom save version (always 0x10)
	uint32_t version;

	// From the upk packages this looks like it should be the total
	// uncompressed size of the save data. In practice It's always all zeros.
	uint32_t uncompressed_size;

	// The game number
	uint32_t game_number;

	// The save number
	uint32_t save_number;

	// The readable save description. This appears in the load game list.
	std::string save_description;

	// The date and time of the save
	std::string time;

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
};

// An actor instance. Most saved objects in the game are instances of
// the Unreal Actor class. These are represented in the save by a pair
// of strings (map name, class name) and an instance number.
struct XComActor
{
	// Map name followed by actor class name
	std::pair<std::string, std::string> actorName;

	// The instance number. Always non-zero
	uint32_t instanceNum;
};

using XComActorTable = std::vector<XComActor>;

struct XComPropertyVisitor;

// An Unreal property. Saved actors and other objects are made up of property values.
// These can be simple primitive values (int, float, bool, string, enums), object values (actors),
// or aggregate types (arrays, structs, and static arrays of another property type)
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

// Properties are polymorphic types and are usually referenced by 
// XComPropertyPtr values which are simply unique_ptrs to a property.
using XComPropertyPtr = std::unique_ptr<XComProperty>;

struct XComIntProperty;
struct XComFloatProperty;
struct XComBoolProperty;
struct XComStringProperty;
struct XComObjectProperty;
struct XComByteProperty;
struct XComStructProperty;
struct XComArrayProperty;
struct XComStaticArrayProperty;

// Visit all property types.
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

// A list of properties.
using XComPropertyList = std::vector<XComPropertyPtr>;


// An object property refers to an actor.
// TODO Replace the data with the actor references.
struct XComObjectProperty : public XComProperty
{
	XComObjectProperty(const std::string &n, uint32_t s, std::vector<unsigned char>&& d) :
		XComProperty(n, s, Kind::ObjectProperty), data(std::move(d)) {}

	std::vector<unsigned char> data;

	void accept(XComPropertyVisitor *v) {
		v->visitObject(this);
	}
};

// An int property contains a 32-bit signed integer value.
struct XComIntProperty : public XComProperty
{
	XComIntProperty(const std::string& n, uint32_t s, int32_t v) :
		XComProperty(n, s, Kind::IntProperty), value(v) {}

	int32_t value;

	void accept(XComPropertyVisitor *v) {
		v->visitInt(this);
	}
};

// A bool property contains a boolean value.
struct XComBoolProperty : public XComProperty
{
	XComBoolProperty(const std::string &n, uint32_t s, bool v) :
		XComProperty(n, s, Kind::BoolProperty), value(v) {}

	bool value;

	void accept(XComPropertyVisitor *v) {
		v->visitBool(this);
	}
};

// A float property contains a single-precision floating point value.
struct XComFloatProperty : public XComProperty
{
	XComFloatProperty(const std::string &n, uint32_t s, float f) :
		XComProperty(n, s, Kind::FloatProperty), value(f) {}

	float value;

	void accept(XComPropertyVisitor *v) {
		v->visitFloat(this);
	}
};

// A string property contains a string value. For INT saves, these are recorded in ISO-8859-1 format,
// but are translated by the library to and from UTF-8 for ease of interoperability with other libraries.
// Be careful not to store characters that cannot be represented in ISO-8859-1 or the game will most likely
// crash. Note that non-INT saves likely use some other encoding for the strings, but I do not know which.
struct XComStringProperty : public XComProperty
{
	XComStringProperty(const std::string& n, uint32_t sz, const std::string& s) :
		XComProperty(n, sz, Kind::StrProperty), str(s) {}

	std::string str;

	void accept(XComPropertyVisitor *v) {
		v->visitString(this);
	}
};

// A dynamic array property. Represents an Unreal dynamic array. They have a known array bound, but 
// it is impossible to unambiguously interpret the data within the array without knowing the actual type
// of data in the array. This is part of the UPK that contains the object type containing the array, but
// is not recorded in the save itself. Some heuristics are used to attempt to figure out roughly what's in
// the array, based on the total size of the array property and the array bound:
//
// 1) If the array bound does not evenly divide the total array size (e.g. the elements have different sizes)
//    it must be an array of structs or strings. We can determine which by checking if the element data contains
//    nested properties. If so, it is an array of structs, otherwise it is strings.
// 2) If the size of each element is 4 bytes, it's an array of either ints or floats.
// 3) If the size of each element is 1 byte, it's an array of bools.
// 4) If the size of each element is 8 bytes, it's an array of objects and the elements are actor indices that can be
//    looked up in the actor table.
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

// A "byte" property, e.g. an enum value. Contains the enum type and enum value strings, as well as an "extra" value that I
// am not entirely sure how to interpret, but is commonly used in LW extended enums.
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

// A struct property. Contains a nested list of properties for the struct elements.
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

// A static array property. Static arrays are not first-class objects in the save, they are instead represented by
// the elements themselves listed sequentially in the save, each with an increasing array index value. This property type
// is a pseudo-property used as a container for all the array elements.
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

// A 3-d vector.
using UVector = std::array<float, 3>;

// A 3-d rotator. Note that rotators are ints, not floats.
using URotator = std::array<int, 3>;

// A single checkpoint record. Checkpoint records are defined for each
// actor that is serialized by the game.
struct XComCheckpoint
{
	// The fully-qualified name of the actor, e.g. Command1.TheWorld.PersistentLevel.XGStrategy_0
	std::string name;

	// The instance name of the actor, e.g. XGStrategy_0
	std::string instanceName;

	// A vector representing the position of the actor in the world
	UVector vector;

	// A rotator representing the rotation of the actor in the world
	URotator rotator;

	// The class name of the class this actor is an instance of
	std::string className;

	// The full length of all properties in this checkpoint
	uint32_t propLen;

	// A list of properties (e.g. the member variables of the actor instance)
	XComPropertyList properties;

	// The index for this actor into the actor template table. Appears to be unused in strategy saves.
	uint32_t templateIndex;

	// The number of padding bytes (all zeros) appended to the checkpoint.
	uint32_t padSize;
};

using XComCheckpointTable = std::vector<XComCheckpoint>;

// An actor template. Unused by strategy saves.
struct XComActorTemplate
{
	std::string actorClassPath;
	std::string archetypePath;
	unsigned char loadParams[64];
};

using XComActorTemplateTable = std::vector<XComActorTemplate>;

// An entry in the name table. Unused by strategy saves.
struct XComNameEntry
{
	std::string name;
	unsigned char zeros[8];
	uint32_t dataLen;
	unsigned char *data;
};

using XComNameTable = std::vector<XComNameEntry>;

// A checkpoint "chunk". The xcom strategy save is primarily a list of these 
// (see XComCheckpointChunkTable). It is not entirely known what the various chunks
// are for, and most of the fields are unknown. In fact the unknown integer type fields
// may not be integers at all, but rather some other kind of table that is always empty
// in strategy saves.
struct XComCheckpointChunk
{
	uint32_t unknownInt1;
	
	// The "game type". For strategy saves, this is "Command1".
	std::string gameType;

	// The table of checkpoint entries for this save.
	XComCheckpointTable checkpointTable;

	// Unknown
	uint32_t unknownInt2;

	// The class for this chunk. E.g. a top-level game type, like XComStrategyGame.XComHeadquartersGame.
	std::string className;

	// An actor table for actors in this chunk
	XComActorTable actorTable;

	// Unknown
	uint32_t unknownInt3;

	// The display name for this save chunk. Unknown.
	std::string displayName;

	// The map name for this chunk. Unknown.
	std::string mapName;

	// Unknown.
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
