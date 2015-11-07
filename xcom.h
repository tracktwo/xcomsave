#ifndef XCOM_H
#define XCOM_H

#include <stdint.h>
#include <vector>
#include <array>
#include <memory>
#include <algorithm>
#include <exception>

#include "util.h"

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

	std::string kind_string() const
	{
		switch (kind)
		{
		case Kind::IntProperty: return "IntProperty";
		case Kind::FloatProperty: return "FloatProperty";
		case Kind::BoolProperty: return "BoolProperty";
		case Kind::StrProperty: return "StrProperty";
		case Kind::ObjectProperty: return "ObjectProperty";
		case Kind::ByteProperty: return "ByteProperty";
		case Kind::StructProperty: return "StructProperty";
		case Kind::ArrayProperty: return "ArrayProperty";
		case Kind::StaticArrayProperty: return "StaticArrayProperty";
		default:
			throw std::exception("getPropertyKindString: Invalid property kind\n");
		}
	}


	XComProperty(const std::string &n, Kind k) : name(n), kind(k) {}

	Kind getKind() const {
		return kind;
	}

	std::string getName() const {
		return name;
	}

	virtual uint32_t size() const = 0;

	virtual uint32_t full_size() const;

	virtual void accept(XComPropertyVisitor* v) = 0;

protected:
	std::string name;
	Kind kind;
};

std::string getXcomKindString(XComProperty::Kind kind);


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
	XComObjectProperty(const std::string &n, uint32_t a1, uint32_t a2) :
		XComProperty(n, Kind::ObjectProperty), actor1(a1), actor2(a2) {}

	uint32_t size() const {
		return 8;
	}

	uint32_t actor1;
	uint32_t actor2;

	void accept(XComPropertyVisitor *v) {
		v->visitObject(this);
	}
};

// An int property contains a 32-bit signed integer value.
struct XComIntProperty : public XComProperty
{
	XComIntProperty(const std::string& n, int32_t v) :
		XComProperty(n, Kind::IntProperty), value(v) {}

	int32_t value;

	uint32_t size() const {
		return 4;
	}

	void accept(XComPropertyVisitor *v) {
		v->visitInt(this);
	}
};

// A bool property contains a boolean value.
struct XComBoolProperty : public XComProperty
{
	XComBoolProperty(const std::string &n, bool v) :
		XComProperty(n, Kind::BoolProperty), value(v) {}

	bool value;

	uint32_t size() const {

		// Bool properties report as size 0
		return 0;
	}

	virtual uint32_t full_size() const {
		return XComProperty::full_size() + 1;
	}

	void accept(XComPropertyVisitor *v) {
		v->visitBool(this);
	}
};

// A float property contains a single-precision floating point value.
struct XComFloatProperty : public XComProperty
{
	XComFloatProperty(const std::string &n, float f) :
		XComProperty(n, Kind::FloatProperty), value(f) {}

	float value;

	uint32_t size() const {
		return 4;
	}

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
	XComStringProperty(const std::string& n, const std::string& s) :
		XComProperty(n, Kind::StrProperty), str(s) {}

	std::string str;

	uint32_t size() const {
		if (str.empty()) {
			return 4;
		}
		// 4 for string length + 1 for terminating null. Make sure to use the ISO-8859-1 encoded length!
		std::string tmp = utf8toiso8859_1(str);
		return tmp.length() + 5;
	}

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
	XComArrayProperty(const std::string& n, std::unique_ptr<unsigned char[]>&& a, uint32_t dl, uint32_t b) :
		XComProperty(n, Kind::ArrayProperty), data(std::move(a)), data_length(dl), arrayBound(b) {}

	std::unique_ptr<unsigned char[]> data;
	uint32_t arrayBound;
	uint32_t data_length;

	uint32_t size() const {
		return 4 + data_length;
	}

	void accept(XComPropertyVisitor *v) {
		v->visitArray(this);
	}
};

// A "byte" property, e.g. an enum value. Contains the enum type and enum value strings, as well as an "extra" value that I
// am not entirely sure how to interpret, but is commonly used in LW extended enums.
struct XComByteProperty : public XComProperty
{
	XComByteProperty(const std::string& n,const std::string &et, const std::string &ev, uint32_t i) :
		XComProperty(n, Kind::ByteProperty), enumType(et), enumVal(ev), extVal(i) {}

	std::string enumType;
	std::string enumVal;
	uint32_t extVal;

	uint32_t size() const {
		// 2 * string length + 2 * trailing null + int for extVal = 8 + 2 + 4 = 14.
		return /*enumType.length() + */enumVal.length() + 5 + 4;
	}

	virtual uint32_t full_size() const {
		// full size must also include the length of the inner "unknown" value and the enum Type string length.
		return XComProperty::full_size() + enumType.length() + 5  + 4;
	}

	void accept(XComPropertyVisitor *v) {
		v->visitByte(this);
	}
};

// A struct property. Contains a nested list of properties for the struct elements.
struct XComStructProperty : public XComProperty
{
	XComStructProperty(const std::string &n, const std::string &sn, XComPropertyList &&propList) :
		XComProperty(n, Kind::StructProperty), structName(sn), structProps(std::move(propList)), nativeData(), nativeDataLen(0) {}

	XComStructProperty(const std::string& n, const std::string &sn, std::unique_ptr<unsigned char[]> &&nd, uint32_t l) :
		XComProperty(n, Kind::StructProperty), structName(sn), nativeData(std::move(nd)), nativeDataLen(l) {}

	std::string structName;
	XComPropertyList structProps;
	std::unique_ptr<unsigned char[]> nativeData;
	uint32_t nativeDataLen;

	uint32_t size() const {
		// Name length + name data + trailing null + unknown 0 int value = len + 4 + 1 + 4 = len + 9 bytes.
		uint32_t total = 0;//structName.length() + 5 + 4;
		if (nativeDataLen > 0) {
			return total + nativeDataLen;
		}
		else {
			std::for_each(structProps.begin(), structProps.end(), [&total](const XComPropertyPtr &prop) {
				total += prop->full_size();
			});
			
			// Add the size of the "None" property terminating the list: 9 for "None" and 4 for the unknown int
			total += 9 + 4;
			return total;
		}
	}

	virtual uint32_t full_size() const {
		uint32_t total = XComProperty::full_size();
		total += structName.length() + 5 + 4;
		return total;
	}

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
		XComProperty(n, Kind::StaticArrayProperty) {}

	void addProperty(XComPropertyPtr&& ptr)
	{
		properties.push_back(std::move(ptr));
	}

	size_t length() const {
		return properties.size();
	}

	uint32_t size() const {
		uint32_t total = 0;
		std::for_each(properties.begin(), properties.end(), [&total](const XComPropertyPtr &prop) {
			total += prop->size();
		});
		return total;
	}

	virtual uint32_t full_size() const
	{
		uint32_t total = 0;
		std::for_each(properties.begin(), properties.end(), [&total](const XComPropertyPtr &prop) {
			total += prop->full_size();
		});
		return total;
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
