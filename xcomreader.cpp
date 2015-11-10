#include "minilzo.h"
#include "xcomreader.h"
#include "util.h"
#include "xslib_internal.h"

#include <string>
#include <memory>
#include <cassert>

int32_t XComReader::readInt()
{
	int32_t v = *(reinterpret_cast<const int32_t*>(ptr_));
	ptr_ += 4;
	return v;
}

float XComReader::readFloat()
{
	float f = *(reinterpret_cast<const float*>(ptr_));
	ptr_ += 4;
	return f;
}

bool XComReader::readBool()
{
	return readInt() != 0;
}

std::string XComReader::readString()
{
	int32_t len = readInt();
	if (len == 0) {
		return "";
	}
	const char *str = reinterpret_cast<const char *>(ptr_);
	size_t actualLen = strlen(str);
	
	// Double check the length matches what we read from the file, considering the trailing null is counted in the length 
	// stored in the file.
	if (actualLen != (len-1))
	{
		fprintf(stderr, "Error: String mismatch at offset %d. Expected length %d but got %d\n", ptr_ - start_.get(), len, actualLen);
		return "<error>";
	}

	ptr_ += len;
	return iso8859_1toutf8(str);
}

XComSaveHeader XComReader::readHeader()
{
	XComSaveHeader hdr;
	hdr.version = readInt();
	if (hdr.version != 0x10) {
		fprintf(stderr, "Error: Data does not appear to be an xcom save: expected file version 16 but got %d\n", hdr.version);
		return {0};
	}
	hdr.uncompressed_size = readInt();
	hdr.game_number = readInt();
	hdr.save_number = readInt();
	hdr.save_description = readString();
	hdr.time = readString();
	hdr.map_command = readString();
	hdr.tactical_save = readBool();
	hdr.ironman = readBool();
	hdr.autosave = readBool();
	hdr.dlc = readString();
	hdr.language = readString();
	uint32_t compressedCrc = (uint32_t)readInt();

	// Compute the CRC of the header
	ptr_ = start_.get() + 1016;
	int32_t hdrSize = readInt();
	uint32_t hdrCrc = (uint32_t)readInt();
	uint32_t computedHeaderCrc = crc32b(start_.get(), hdrSize);
	if (hdrCrc != computedHeaderCrc)
	{
		throw std::exception("CRC mismatch in header. Bad save?");
	}

	uint32_t computedCompressedCrc = crc32b(start_.get() + 1024, length_ - 1024);
	if (computedCompressedCrc != compressedCrc)
	{
		throw std::exception("CRC mismatch in compressed data. Bad save?");
	}
	return hdr;
}

XComActorTable XComReader::readActorTable()
{
	XComActorTable actorTable;
	int32_t actorCount = readInt();

	// We expect all entries to be of the form <package> <0> <actor> <instance>, or two entries
	// per real actor.
	assert(actorCount % 2 == 0);

	for (int i = 0; i < actorCount; i += 2) {
		std::string actorName = readString();
		int32_t instanceNum = readInt();
		if (instanceNum == 0) {
			throw std::exception("Error: malformed actor table entry: expected a non-zero instance\n");
		}
		std::string package = readString();
		int32_t sentinel = readInt();
		if (sentinel != 0) {
			throw std::exception("Error: malformed actor table entry: missing 0 instance\n");
		}
		actorTable.push_back(build_actor_name(package, actorName, instanceNum));
	}

	return actorTable;
}

XComPropertyPtr XComReader::makeStructProperty(const std::string &name)
{
	std::string structName = readString();
	int32_t innerUnknown = readInt();
	if (innerUnknown != 0) {
		DBG("Read non-zero prop unknown3 value: %x\n", innerUnknown);
	}
	// Special case certain structs
	if (structName.compare("Vector2D") == 0) {
		std::unique_ptr<unsigned char[]> nativeData = std::make_unique<unsigned char[]>(8);
		memcpy(nativeData.get(), ptr_, 8);
		ptr_ += 8;
		return std::make_unique<XComStructProperty>(name, structName, std::move(nativeData), 8);
	}
	else if (structName.compare("Vector") == 0) {
		std::unique_ptr<unsigned char[]> nativeData = std::make_unique<unsigned char[]>(12);
		memcpy(nativeData.get(), ptr_, 12);
		ptr_ += 12;
		return std::make_unique<XComStructProperty>(name, structName, std::move(nativeData), 12);
	}
	else {
		XComPropertyList structProps = readProperties();
		return std::make_unique<XComStructProperty>(name, structName, std::move(structProps));
	}
}

bool isStructArray(const unsigned char *ptr)
{
	// Sniff the first part of the array data to see if it looks like a string.

	int32_t strLen = *reinterpret_cast<const int*>(ptr);
	if (strLen > 0 && strLen < 100) {
		const char *str = reinterpret_cast<const char *>(ptr) + 4;
		if (str[strLen] == 0 && strlen(str) == (strLen - 1))
		{
			// Ok, looks like a name string. Distinguish between a struct
			// array and a enum array by looking at the next string. if it's a property
			// kind, then we have a property list and this is a struct. If it's not, then
			// we must have an enum array.

			// Skip over the string length, string data, and zero int.
			ptr += 4 + strLen + 4;
			strLen = *reinterpret_cast<const int*>(ptr);
			if (strLen > 0 && strLen < 100) {
				str = reinterpret_cast<const char *>(ptr + 4);
				if (str[strLen] == 0 && strlen(str) == (strLen - 1))
				{
					for (int i = 0; i < static_cast<int>(XComProperty::Kind::Max); ++i) {
						if (property_kind_to_string(static_cast<XComProperty::Kind>(i)) == str) {
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}


XComPropertyPtr XComReader::makeArrayProperty(const std::string &name, int32_t propSize)
{
	int32_t arrayBound = readInt();
	std::unique_ptr<unsigned char[]> arrayData;
	int array_data_size = propSize - 4;
	if (array_data_size > 0) {
		if (arrayBound * 8 == array_data_size) {
			// Looks like an array of objects
			std::vector<int32_t> elems;
			for (int32_t i = 0; i < arrayBound; ++i) {
				int32_t actor1 = readInt();
				int32_t actor2 = readInt();
				if (actor1 == -1 && actor2 == -1) {
					elems.push_back(actor1);
				}
				else if (actor1 != (actor2 + 1)) {
					throw std::exception("Error: malformed object array: expected related actor numbers\n");
				}
				else {
					elems.push_back(actor1 / 2);
				}
			}
			return std::make_unique<XComObjectArrayProperty>(name, elems);
		}
		else if (arrayBound * 4 == array_data_size) {
			// Looks like an array of ints or floats
			std::vector<int32_t> elems;
			for (int i = 0; i < arrayBound; ++i) {
				elems.push_back(readInt());
			}

			return std::make_unique<XComNumberArrayProperty>(name, elems);
		}
		else if (isStructArray(ptr_)) {
			std::vector<XComPropertyList> elems;
			for (int32_t i = 0; i < arrayBound; ++i) {
				elems.push_back(readProperties());
			}

			return std::make_unique<XComStructArrayProperty>(name, std::move(elems));
		}
		else {
			// Nope, dunno what this thing is.
			arrayData = std::make_unique<unsigned char[]>(array_data_size);
			memcpy(arrayData.get(), ptr_, array_data_size);
			ptr_ += array_data_size;
		}
	}
	return std::make_unique<XComArrayProperty>(name, std::move(arrayData), array_data_size, arrayBound);
}

XComPropertyList XComReader::readProperties()
{
	XComPropertyList properties;
	for (;;)
	{
		std::string name = readString();
		int32_t unknown1 = readInt();
		if (unknown1 != 0) {
			DBG("Read non-zero prop unknown value: %x\n", unknown1);
		}

		if (name.compare("None") == 0) {
			break;
		}
		std::string propType = readString();
		int32_t unknown2 = readInt();
		if (unknown2 != 0) {
			DBG("Read non-zero prop unknown2 value: %x\n", unknown2);
		}
		int32_t propSize = readInt();
		int32_t arrayIdx = readInt();

		XComPropertyPtr prop;
		if (propType.compare("ObjectProperty") == 0) {
			assert(propSize == 8);
			int32_t objRef1 = readInt();
			int32_t objRef2 = readInt();
			if (objRef1 != -1 && objRef1 != (objRef2 + 1)) {
				throw std::exception("Assertion failed: object references not related.\n");
			}

			prop = std::make_unique<XComObjectProperty>(name, (objRef1 == -1) ? objRef1 : (objRef1 / 2));
		}
		else if (propType.compare("IntProperty") == 0) {
			assert(propSize == 4);
			int32_t intVal = readInt();
			prop = std::make_unique<XComIntProperty>(name, intVal);
		}
		else if (propType.compare("ByteProperty") == 0) {
			std::string enumType = readString();
			int32_t innerUnknown = readInt();
			if (innerUnknown != 0) {
				DBG("Read non-zero prop unknown3 value: %x\n", innerUnknown);
			}
			std::string enumVal = readString();
			int32_t extVal = readInt();
			prop = std::make_unique<XComByteProperty>(name, enumType, enumVal, extVal);
		}
		else if (propType.compare("BoolProperty") == 0) {
			assert(propSize == 0);
			bool boolVal = *ptr_++ != 0;
			prop = std::make_unique<XComBoolProperty>(name, boolVal);
		}
		else if (propType.compare("ArrayProperty") == 0) {
			prop = makeArrayProperty(name, propSize);
		}
		else if (propType.compare("FloatProperty") == 0) {
			float f = readFloat();
			prop = std::make_unique<XComFloatProperty>(name, f);
		}
		else if (propType.compare("StructProperty") == 0) {
			prop = makeStructProperty(name);
		}
		else if (propType.compare("StrProperty") == 0) {
			std::string str = readString();
			prop = std::make_unique<XComStringProperty>(name, str);
		}
		else
		{
			DBG("Error: unknown property type %s\n", propType.c_str());
		}

		if (prop.get() != nullptr) {
			assert(prop->size() == propSize);
			if (arrayIdx == 0) {
				properties.push_back(std::move(prop));
			}
			else {
				if (properties.back()->name.compare(name) != 0) {
					DBG("Static array index found but doesn't match last property at offset 0x%x\n", ptr_ - start_.get());
				}

				if (properties.back()->kind == XComProperty::Kind::StaticArrayProperty) {
					// We already have a static array. Sanity check the array index and add it
					assert(arrayIdx == static_cast<XComStaticArrayProperty*>(properties.back().get())->length());
					static_cast<XComStaticArrayProperty*>(properties.back().get())->addProperty(std::move(prop));
				}
				else {
					// Not yet a static array. This new property should have index 1.
					assert(arrayIdx == 1);

					// Pop off the old property
					XComPropertyPtr lastProp = std::move(properties.back());
					properties.pop_back();

					// And replace it with a new static array
					std::unique_ptr<XComStaticArrayProperty> staticArrayProp = std::make_unique<XComStaticArrayProperty>(name);
					staticArrayProp->addProperty(std::move(lastProp));
					staticArrayProp->addProperty(std::move(prop));
					properties.push_back(std::move(staticArrayProp));
				}
			}
		}
	}

	return properties;
}

XComCheckpointTable XComReader::readCheckpointTable()
{
	XComCheckpointTable checkpointTable;
	int32_t checkpointCount = readInt();

	for (int i = 0; i < checkpointCount; ++i) {
		XComCheckpoint chk;
		chk.name = readString();
		chk.instanceName = readString();
		chk.vector[0] = readFloat();
		chk.vector[1] = readFloat();
		chk.vector[2] = readFloat();
		chk.rotator[0] = readInt();
		chk.rotator[1] = readInt();
		chk.rotator[2] = readInt();
		chk.className = readString();
		int32_t propLen = readInt();
		if (propLen < 0) {
			throw std::exception("Error: Found negative property length\n");
		}
		chk.padSize = 0;
		const unsigned char* startPtr = ptr_;

		chk.properties = readProperties();
		if ((ptr_ - startPtr) < propLen) {
			chk.padSize = propLen - (ptr_ - startPtr);

			for (unsigned int i = 0; i < chk.padSize; ++i) {
				if (*ptr_++ != 0) {
					DBG("Found non-zero padding byte at offset %x", (offset() - 1));
				}
			}
		}
		size_t totalPropSize = 0;
		std::for_each(chk.properties.begin(), chk.properties.end(), [&totalPropSize](const XComPropertyPtr& prop) {
			totalPropSize += prop->full_size();
		});
		totalPropSize += 9 + 4; // length of trailing "None" to terminate the list + the unknown int.
		assert((uint32_t)propLen == (totalPropSize + chk.padSize));
		chk.templateIndex = readInt();
		checkpointTable.push_back(std::move(chk));
	}

	return checkpointTable;
}

XComActorTemplateTable XComReader::readActorTemplateTable()
{
	XComActorTemplateTable templateTable;
	int32_t templateCount = readInt();

	for (int i = 0; i < templateCount; ++i) {
		XComActorTemplate tmpl;
		tmpl.actorClassPath = readString();
		memcpy(tmpl.loadParams, ptr_, 64);
		ptr_ += 64;
		tmpl.archetypePath = readString();
		templateTable.push_back(std::move(tmpl));
	}

	return templateTable;
}

XComNameTable XComReader::readNameTable()
{
	static unsigned char allZeros[8] = { 0 };
	XComNameTable nameTable;
	int32_t nameCount = readInt();

	for (int i = 0; i < nameCount; ++i) {
		XComNameEntry entry;
		entry.name = readString();
		memcpy(entry.zeros, ptr_, 8);
		ptr_ += 8;
		if (memcmp(entry.zeros, allZeros, 8) != 0) {
			fprintf(stderr, "Error: Expected all zeros in name table entry");
			return{};
		}
		entry.dataLen = readInt();
		entry.data = new unsigned char[entry.dataLen];
		memcpy(entry.data, ptr_, entry.dataLen);
		ptr_ += entry.dataLen;
		nameTable.push_back(std::move(entry));
	}

	return nameTable;
}

int32_t XComReader::getuncompressed_size()
{
	// The compressed data begins 1024 bytes into the file.
	const unsigned char* p = start_.get() + 1024;
	uint32_t compressedSize;
	uint32_t uncompressed_size = 0;
	do
	{
		// Expect the magic header value 0x9e2a83c1 at the start of each chunk
		if (*(reinterpret_cast<const int32_t*>(p)) != UPK_Magic) {
			fprintf(stderr, "Failed to find compressed chunk at offset 0x%08x", offset());
			return -1;
		}

		// Compressed size is at p+8
		compressedSize = *(reinterpret_cast<const unsigned long*>(p + 8));

		// Uncompressed size is at p+12
		uncompressed_size += *(reinterpret_cast<const unsigned long*>(p + 12));

		// Skip to next chunk - 24 bytes of this chunk header + compressedSize bytes later.
		p += (compressedSize + 24);
	} while (static_cast<size_t>(p - start_.get()) < length_);

	return uncompressed_size;
}

void XComReader::getUncompressedData(unsigned char *buf)
{
	// Start back at the beginning of the compressed data.
	const unsigned char *p = start_.get() + 1024;
	unsigned char *outp = buf;
	do
	{
		// Expect the magic header value 0x9e2a83c1 at the start of each chunk
		if (*(reinterpret_cast<const uint32_t*>(p)) != UPK_Magic) {
			fprintf(stderr, "Failed to find compressed chunk at offset 0x%08x", (p - start_.get()));
			return;
		}

		// Compressed size is at p+8
		int32_t compressedSize = *(reinterpret_cast<const int32_t *>(p + 8));

		// Uncompressed size is at p+12
		int32_t uncompressed_size = *(reinterpret_cast<const int32_t*>(p + 12));
		
		unsigned long decompSize = uncompressed_size;

		if (lzo1x_decompress_safe(p + 24, compressedSize, outp, &decompSize, nullptr) != LZO_E_OK) {
			fprintf(stderr, "LZO decompress of save data failed\n");
			return;
		}

		unsigned char * wrkbuf = new unsigned char[LZO1X_1_MEM_COMPRESS];
		
		unsigned long tmpSize = decompSize + decompSize / 16  + 64 + 3;
		unsigned char * tmpBuf = new unsigned char[tmpSize];

		if (decompSize != uncompressed_size)
		{
			fprintf(stderr, "Failed to decompress chunk!");
			return;
		}

		// Skip to next chunk - 24 bytes of this chunk header + compressedSize bytes later.
		p += (compressedSize + 24);
		outp += uncompressed_size;
	} while (static_cast<size_t>(p - start_.get()) < length_);
}

XComSave XComReader::getSaveData()
{
	XComSave save;
	const unsigned char *p = ptr_ + 1024;
	FILE *outFile = fopen("output.dat", "wb");
	if (outFile == nullptr) {
		fprintf(stderr, "Failed to open output file: %d", errno);
	}
	int chunkCount = 0;
	int totalCompressed = 0;
	int totalUncompressed = 0;
	save.header = readHeader();

	int32_t uncompressed_size = getuncompressed_size();
	if (uncompressed_size < 0) {
		return {};
	}
	
	unsigned char *uncompressedData = new unsigned char[uncompressed_size];
	getUncompressedData(uncompressedData);

	// We're now done with the compressed file. Swap over to the uncompressed data
	ptr_ = uncompressedData;
	start_.reset(ptr_);
	length_ = uncompressed_size;
	fwrite(uncompressedData, 1, uncompressed_size, outFile);
	fclose(outFile);
	save.actorTable = readActorTable();
	DBG("Finished reading actor table (%x) at offset %x\n", save.actorTable.size(), offset());
	// Jump back to here after each chunk
	do {
		XComCheckpointChunk chunk;
		chunk.unknownInt1 = readInt();
		chunk.gameType = readString();
		std::string none = readString();
		if (none != "None") {
			fprintf(stderr, "Error locating 'None' after actor table.\n");
			return{};
		}
		
		chunk.unknownInt2 = readInt();
		chunk.checkpointTable = readCheckpointTable();
		DBG("Finished reading checkpoint table at offset %x\n", offset());

		int32_t nameTableLen = readInt();
		assert(nameTableLen == 0);

#if 0
		if (unknownInt3 > 0) { // only seems to be present for tactical saves?
			XComNameTable nameTable = readNameTable();
			DBG("Finished reading name table at offset %x\n", ptr_ - start_);
		}
#endif

		chunk.className = readString();
		chunk.actorTable = readActorTable();
		DBG("Finished reading second actor table (%x) at offset %x\n", chunk.actorTable.size(), offset());

		chunk.unknownInt3 = readInt();

		XComActorTemplateTable actorTemplateTable = readActorTemplateTable(); // (only seems to be present for tactical saves?)
		assert(actorTemplateTable.size() == 0);
		DBG("Finished reading actor template table at offset %x\n", offset());

		chunk.displayName = readString(); //unknown (game name)
		chunk.mapName = readString(); //unknown (map name)
		chunk.unknownInt4 = readInt(); //unknown  (checksum?)

		save.checkpoints.push_back(std::move(chunk));
	}
	while ((static_cast<size_t>(offset()) < length_));
	return save;
}