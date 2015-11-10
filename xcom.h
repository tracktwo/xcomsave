#ifndef XCOM_H
#define XCOM_H

#include <stdint.h>
#include <vector>
#include <string>
#include <array>
#include <memory>
#include <algorithm>
#include <exception>

struct Buffer
{
	std::unique_ptr<unsigned char[]> buf;
	size_t len;
};

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
	int32_t version;

	// From the upk packages this looks like it should be the total
	// uncompressed size of the save data. In practice It's always all zeros.
	int32_t uncompressed_size;

	// The game number
	int32_t game_number;

	// The save number
	int32_t save_number;

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

using XComActorTable = std::vector<std::string>;

struct XComPropertyVisitor;

// An Unreal property. Saved actors and other objects are made up of property values.
// These can be simple primitive values (int, float, bool, string, enums), object values (actors),
// or aggregate types (arrays, structs, and static arrays of another property type).
//
// Most of the kinds are present directly in the saved game, but arrays have some special handling
// here to make them easier to deal with:
//
// Dynamic arrays are all represented identically in the save but have different representations
// in this library. They can be one of the following kinds:
//
// ObjectArrayProperty - An array of objects (actors). Contents are simply actor reference numbers.
// NumberArrayProperty - An array of numbers. Could be integers or floats, depending on the particular
// property. There is no unambiguous way to distinguish the two cases from analyzing the save file alone,
// you need to "know" the real type of the object by looking in the corresponding UPK file. Array data
// is represented as an array of ints, but may need to be cast to floats if appropriate.
// ArrayProperty - An array of 
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
		ObjectArrayProperty,
		NumberArrayProperty,
		StructArrayProperty,
		StaticArrayProperty,
		Max
	};

	XComProperty(const std::string &n, Kind k) : name(n), kind(k) {}

	std::string kind_string() const;
	virtual size_t size() const = 0;
	virtual size_t full_size() const;
	virtual void accept(XComPropertyVisitor* v) = 0;
	
	std::string name;
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
struct XComObjectArrayProperty;
struct XComNumberArrayProperty;
struct XComStructArrayProperty;
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
	virtual void visitObjectArray(XComObjectArrayProperty*) = 0;
	virtual void visitNumberArray(XComNumberArrayProperty*) = 0;
	virtual void visitStructArray(XComStructArrayProperty*) = 0;
	virtual void visitStaticArray(XComStaticArrayProperty*) = 0;
};

// A list of properties.
using XComPropertyList = std::vector<XComPropertyPtr>;


// An object property refers to an actor.
// TODO Replace the data with the actor references.
struct XComObjectProperty : public XComProperty
{
	XComObjectProperty(const std::string &n, int32_t a) :
		XComProperty(n, Kind::ObjectProperty), actor(a) {}

	virtual size_t size() const {
		return 8;
	}

	virtual void accept(XComPropertyVisitor *v) {
		v->visitObject(this);
	}

	int32_t actor;
};

// An int property contains a 32-bit signed integer value.
struct XComIntProperty : public XComProperty
{
	XComIntProperty(const std::string& n, int32_t v) :
		XComProperty(n, Kind::IntProperty), value(v) {}

	virtual size_t size() const {
		return 4;
	}

	virtual void accept(XComPropertyVisitor *v) {
		v->visitInt(this);
	}

	int32_t value;
};

// A bool property contains a boolean value.
struct XComBoolProperty : public XComProperty
{
	XComBoolProperty(const std::string &n, bool v) :
		XComProperty(n, Kind::BoolProperty), value(v) {}

	virtual size_t size() const {

		// Bool properties report as size 0
		return 0;
	}

	virtual size_t full_size() const {
		return XComProperty::full_size() + 1;
	}

	virtual void accept(XComPropertyVisitor *v) {
		v->visitBool(this);
	}

	bool value;
};

// A float property contains a single-precision floating point value.
struct XComFloatProperty : public XComProperty
{
	XComFloatProperty(const std::string &n, float f) :
		XComProperty(n, Kind::FloatProperty), value(f) {}

	virtual size_t size() const {
		return 4;
	}

	virtual void accept(XComPropertyVisitor *v) {
		v->visitFloat(this);
	}

	float value;
};

// A string property contains a string value. For INT saves, these are recorded in ISO-8859-1 format,
// but are translated by the library to and from UTF-8 for ease of interoperability with other libraries.
// Be careful not to store characters that cannot be represented in ISO-8859-1 or the game will most likely
// crash. Note that non-INT saves likely use some other encoding for the strings, but I do not know which.
struct XComStringProperty : public XComProperty
{
	XComStringProperty(const std::string& n, const std::string& s) :
		XComProperty(n, Kind::StrProperty), str(s) {}

	virtual size_t size() const;

	virtual void accept(XComPropertyVisitor *v) {
		v->visitString(this);
	}

	std::string str;
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
	XComArrayProperty(const std::string& n, std::unique_ptr<unsigned char[]>&& a, int32_t dl, int32_t b) :
		XComProperty(n, Kind::ArrayProperty), data(std::move(a)), data_length(dl), array_bound(b) {}

	virtual size_t size() const {
		return 4 + data_length;
	}

	virtual void accept(XComPropertyVisitor *v) {
		v->visitArray(this);
	}

	std::unique_ptr<unsigned char[]> data;
	int32_t array_bound;
	int32_t data_length;
};

struct XComObjectArrayProperty : public XComProperty
{
	XComObjectArrayProperty(const std::string& n, std::vector<int32_t> objs) :
		XComProperty(n, Kind::ObjectArrayProperty), elements(objs) {}

	virtual size_t size() const {
		return 4 + 8 * elements.size();
	}

	virtual void accept(XComPropertyVisitor* v) {
		v->visitObjectArray(this);
	}

	std::vector<int32_t> elements;
};

struct XComNumberArrayProperty : public XComProperty
{
	XComNumberArrayProperty(const std::string& n, std::vector<int32_t> objs) :
		XComProperty(n, Kind::NumberArrayProperty), elements(objs) {}
	
	virtual size_t size() const {
		return 4 + 4 * elements.size();
	}

	virtual void accept(XComPropertyVisitor *v) {
		v->visitNumberArray(this);
	}

	std::vector<int32_t> elements;
};

struct XComStructArrayProperty : public XComProperty
{
	XComStructArrayProperty(const std::string &n, std::vector<XComPropertyList> props) :
		XComProperty(n, Kind::StructArrayProperty), elements(std::move(props)) {}

	virtual size_t size() const {
		// A struct array has the array bound plus 9 bytes per element for the terminating "None" string 
		// plus 4 bytes per element for the unknown 0 integer that follows the none.
		size_t total = 4 + 13 * elements.size();
		std::for_each(elements.begin(), elements.end(), [&total](const XComPropertyList &pl) {
			std::for_each(pl.begin(), pl.end(), [&total](const XComPropertyPtr& prop) {
				total += prop->full_size();
			});
		});

		return total;
	}

	virtual void accept(XComPropertyVisitor *v) {
		v->visitStructArray(this);
	}

	std::vector<XComPropertyList> elements;
};

// A "byte" property, e.g. an enum value. Contains the enum type and enum value strings, as well as an "extra" value that I
// am not entirely sure how to interpret, but is commonly used in LW extended enums. Vanilla enums generally have '0' for this
// extra value for all members. LW extended enums typically re-use the names of an existing enum member, but each successive
// additional entry with the same name has an extra value larger than the previous.
struct XComByteProperty : public XComProperty
{
	XComByteProperty(const std::string& n,const std::string &et, const std::string &ev, int32_t i) :
		XComProperty(n, Kind::ByteProperty), enumType(et), enumVal(ev), extVal(i) {}

	size_t size() const {
		// size does not include the size of the enum type string
		return enumVal.length() + 5 + 4;
	}

	virtual size_t full_size() const {
		// full size must also include the length of the inner "unknown" value and the enum Type string length.
		return XComProperty::full_size() + enumType.length() + 5  + 4;
	}

	void accept(XComPropertyVisitor *v) {
		v->visitByte(this);
	}

	std::string enumType;
	std::string enumVal;
	int32_t extVal;
};

// A struct property. Contains a nested list of properties for the struct elements.
struct XComStructProperty : public XComProperty
{
	XComStructProperty(const std::string &n, const std::string &sn, XComPropertyList &&propList) :
		XComProperty(n, Kind::StructProperty), structName(sn), structProps(std::move(propList)), nativeData(), nativeDataLen(0) {}

	XComStructProperty(const std::string& n, const std::string &sn, std::unique_ptr<unsigned char[]> &&nd, size_t l) :
		XComProperty(n, Kind::StructProperty), structName(sn), nativeData(std::move(nd)), nativeDataLen(l) {}

	size_t size() const;

	virtual size_t full_size() const;
	void accept(XComPropertyVisitor *v) {
		v->visitStruct(this);
	}

	std::string structName;
	XComPropertyList structProps;
	std::unique_ptr<unsigned char[]> nativeData;
	size_t nativeDataLen;
};

// A static array property. Static arrays are not first-class objects in the save, they are instead represented by
// the elements themselves listed sequentially in the save, each with an increasing array index value. This property type
// is a pseudo-property used as a container for all the array elements.
struct XComStaticArrayProperty : public XComProperty
{
	XComStaticArrayProperty(const std::string& n) :
		XComProperty(n, Kind::StaticArrayProperty) {}

	void addProperty(XComPropertyPtr&& ptr) {
		properties.push_back(std::move(ptr));
	}

	size_t length() const {
		return properties.size();
	}

	virtual size_t size() const {
		size_t total = 0;
		std::for_each(properties.begin(), properties.end(), [&total](const XComPropertyPtr &prop) {
			total += prop->size();
		});
		return total;
	}

	virtual size_t full_size() const {
		size_t total = 0;
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
	int32_t templateIndex;

	// The number of padding bytes (all zeros) appended to the checkpoint.
	size_t padSize;
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
	size_t dataLen;
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
	int32_t unknownInt1;
	
	// The "game type". For strategy saves, this is "Command1".
	std::string gameType;

	// The table of checkpoint entries for this save.
	XComCheckpointTable checkpointTable;

	// Unknown
	int32_t unknownInt2;

	// The class for this chunk. E.g. a top-level game type, like XComStrategyGame.XComHeadquartersGame.
	std::string className;

	// An actor table for actors in this chunk
	XComActorTable actorTable;

	// Unknown
	int32_t unknownInt3;

	// The display name for this save chunk. Unknown.
	std::string displayName;

	// The map name for this chunk. Unknown.
	std::string mapName;

	// Unknown.
	int32_t unknownInt4;
};

using XComCheckpointChunkTable = std::vector<XComCheckpointChunk>;

struct XComSave
{
	XComSaveHeader header;
	XComActorTable actorTable;
	XComCheckpointChunkTable checkpoints;
};

std::string build_actor_name(const std::string& package, const std::string& cls, int instance);
std::tuple<std::string, std::string, int> decompose_actor_name(const std::string& actorName);

#endif // XCOM_H
