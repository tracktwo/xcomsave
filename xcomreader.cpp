#include "minilzo.h"
#include "xcomreader.h"
#include "util.h"
#include <string>
#include <memory>
#include <cassert>

uint32_t XComReader::readInt32()
{
	uint32_t v = *(reinterpret_cast<const uint32_t*>(ptr_));
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
	return readInt32() != 0;
}

std::string XComReader::readString()
{
	uint32_t len = readInt32();
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
	hdr.version = readInt32();
	if (hdr.version != 0x10) {
		fprintf(stderr, "Error: Data does not appear to be an xcom save: expected file version 16 but got %d\n", hdr.version);
		return {0};
	}
	hdr.uncompressed_size = readInt32();
	hdr.game_number = readInt32();
	hdr.save_number = readInt32();
	hdr.save_description = readString();
	hdr.time = readString();
	hdr.map_command = readString();
	hdr.tactical_save = readBool();
	hdr.ironman = readBool();
	hdr.autosave = readBool();
	hdr.dlc = readString();
	hdr.language = readString();
	uint32_t compressedCrc = readInt32();

	// Compute the CRC of the header
	ptr_ = start_.get() + 1016;
	uint32_t hdrSize = readInt32();
	uint32_t hdrCrc = readInt32();
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
	uint32_t actorCount = readInt32();

	// We expect all entries to be of the form <package> <0> <actor> <instance>, or two entries
	// per real actor.
	assert(actorCount % 2 == 0);

	for (unsigned int i = 0; i < actorCount; i += 2) {
		std::string actorName = readString();
		uint32_t instanceNum = readInt32();
		if (instanceNum == 0) {
			throw std::exception("Error: malformed actor table entry: expected a non-zero instance\n");
		}
		std::string package = readString();
		uint32_t sentinel = readInt32();
		if (sentinel != 0) {
			throw std::exception("Error: malformed actor table entry: missing 0 instance\n");
		}
		actorTable.push_back({std::make_pair(package, actorName), instanceNum });
	}

	return actorTable;
}

XComPropertyList XComReader::readProperties(uint32_t dataLen)
{
	const unsigned char *endpos = ptr_ + dataLen;
	XComPropertyList properties;
	while (ptr_ < endpos)
	{
		std::string name = readString();
		uint32_t unknown1 = readInt32();
		if (unknown1 != 0) {
			DBG("Read non-zero prop unknown value: %x\n", unknown1);
		}

		if (name.compare("None") == 0) {
			break;
		}
		std::string propType = readString();
		uint32_t unknown2 = readInt32();
		if (unknown2 != 0) {
			DBG("Read non-zero prop unknown2 value: %x\n", unknown2);
		}
		uint32_t propSize = readInt32();
		uint32_t arrayIdx = readInt32();

		XComPropertyPtr prop;
		if (propType.compare("ObjectProperty") == 0) {
			std::vector<unsigned char> data;
			for (unsigned int i = 0; i < propSize; ++i) {
				data.push_back(*ptr_++);
			}
			prop = std::make_unique<XComObjectProperty>(name, std::move(data));
		}
		else if (propType.compare("IntProperty") == 0) {
			assert(propSize == 4);
			uint32_t intVal = readInt32();
			prop = std::make_unique<XComIntProperty>(name, intVal);
		}
		else if (propType.compare("ByteProperty") == 0) {
			std::string enumType = readString();
			uint32_t innerUnknown = readInt32();
			if (innerUnknown != 0) {
				DBG("Read non-zero prop unknown3 value: %x\n", innerUnknown);
			}
			std::string enumVal = readString();
			uint32_t extVal = readInt32();
			prop = std::make_unique<XComByteProperty>(name, enumType, enumVal, extVal);
		}
		else if (propType.compare("BoolProperty") == 0) {
			assert(propSize == 0);
			bool boolVal = *ptr_++ != 0;
			prop = std::make_unique<XComBoolProperty>(name, boolVal);
		}
		else if (propType.compare("ArrayProperty") == 0) {
			uint32_t arrayBound = readInt32();
			std::unique_ptr<unsigned char[]> arrayData;
			if (propSize > 4) {
				arrayData = std::make_unique<unsigned char[]>(propSize - 4);
				memcpy(arrayData.get(), ptr_, propSize - 4);
				ptr_ += (propSize - 4);
			}
			prop = std::make_unique<XComArrayProperty>(name, std::move(arrayData), propSize - 4, arrayBound);
		}
		else if (propType.compare("FloatProperty") == 0) {
			float f = readFloat();
			prop = std::make_unique<XComFloatProperty>(name, f);
		}
		else if (propType.compare("StructProperty") == 0) {
			std::string structName = readString();
			uint32_t innerUnknown = readInt32();
			if (innerUnknown != 0) {
				DBG("Read non-zero prop unknown3 value: %x\n", innerUnknown);
			}
			// Special case certain structs
			if (structName.compare("Vector2D") == 0) {
				assert(propSize == 8);
				std::unique_ptr<unsigned char[]> nativeData = std::make_unique<unsigned char[]>(8);
				memcpy(nativeData.get(), ptr_, 8);
				ptr_ += 8;
				prop = std::make_unique<XComStructProperty>(name, structName, std::move(nativeData), 8);
			} 
			else if (structName.compare("Vector") == 0) {
				assert(propSize == 12);
				std::unique_ptr<unsigned char[]> nativeData = std::make_unique<unsigned char[]>(12);
				memcpy(nativeData.get(), ptr_, 12);
				ptr_ += 12;
				prop = std::make_unique<XComStructProperty>(name, structName, std::move(nativeData), 12);
			}
			else {
				XComPropertyList structProps = readProperties(propSize);
				prop = std::make_unique<XComStructProperty>(name, structName, std::move(structProps));
			}
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
				if (properties.back()->getName().compare(name) != 0) {
					DBG("Static array index found but doesn't match last property at offset 0x%x\n", ptr_ - start_.get());
				}

				if (properties.back()->getKind() == XComProperty::Kind::StaticArrayProperty) {
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
	uint32_t checkpointCount = readInt32();

	for (unsigned int i = 0; i < checkpointCount; ++i) {
		XComCheckpoint chk;
		chk.name = readString();
		chk.instanceName = readString();
		chk.vector[0] = readFloat();
		chk.vector[1] = readFloat();
		chk.vector[2] = readFloat();
		chk.rotator[0] = readInt32();
		chk.rotator[1] = readInt32();
		chk.rotator[2] = readInt32();
		chk.className = readString();
		uint32_t propLen = readInt32();
		chk.padSize = 0;
		const unsigned char* startPtr = ptr_;
		chk.properties = readProperties(propLen);
		if (uint32_t(ptr_ - startPtr) < propLen) {
			chk.padSize = propLen - (ptr_ - startPtr);

			for (unsigned int i = 0; i < chk.padSize; ++i) {
				if (*ptr_++ != 0) {
					DBG("Found non-zero padding byte at offset %x", (offset() - 1));
				}
			}
		}
		uint32_t totalPropSize = 0;
		std::for_each(chk.properties.begin(), chk.properties.end(), [&totalPropSize](const XComPropertyPtr& prop) {
			totalPropSize += prop->full_size();
		});
		totalPropSize += 9 + 4; // length of trailing "None" to terminate the list + the unknown int.
		assert(propLen == (totalPropSize + chk.padSize));
		chk.templateIndex = readInt32();
		checkpointTable.push_back(std::move(chk));
	}

	return checkpointTable;
}

XComActorTemplateTable XComReader::readActorTemplateTable()
{
	XComActorTemplateTable templateTable;
	uint32_t templateCount = readInt32();

	for (unsigned int i = 0; i < templateCount; ++i) {
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
	uint32_t nameCount = readInt32();

	for (unsigned int i = 0; i < nameCount; ++i) {
		XComNameEntry entry;
		entry.name = readString();
		memcpy(entry.zeros, ptr_, 8);
		ptr_ += 8;
		if (memcmp(entry.zeros, allZeros, 8) != 0) {
			fprintf(stderr, "Error: Expected all zeros in name table entry");
			return{};
		}
		entry.dataLen = readInt32();
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
		if (*(reinterpret_cast<const uint32_t*>(p)) != 0x9e2a83c1) {
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
		if (*(reinterpret_cast<const uint32_t*>(p)) != Chunk_Magic) {
			fprintf(stderr, "Failed to find compressed chunk at offset 0x%08x", (p - start_.get()));
			return;
		}

		printf("Found chunk at offset 0x%x\n", (p - start_.get()));
		// Compressed size is at p+8
		uint32_t compressedSize = *(reinterpret_cast<const unsigned long*>(p + 8));

		// Uncompressed size is at p+12
		uint32_t uncompressed_size = *(reinterpret_cast<const unsigned long*>(p + 12));
		
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

	uint32_t uncompressed_size = getuncompressed_size();
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
	DBG("Finished reading actor table at offset %x\n", offset());
	// Jump back to here after each chunk
	do {
		XComCheckpointChunk chunk;
		chunk.unknownInt1 = readInt32();
		chunk.gameType = readString();
		std::string none = readString();
		if (none != "None") {
			fprintf(stderr, "Error locating 'None' after actor table.\n");
			return{};
		}
		
		chunk.unknownInt2 = readInt32();
		chunk.checkpointTable = readCheckpointTable();
		DBG("Finished reading checkpoint table at offset %x\n", offset());

		uint32_t nameTableLen = readInt32();
		assert(nameTableLen == 0);

#if 0
		if (unknownInt3 > 0) { // only seems to be present for tactical saves?
			XComNameTable nameTable = readNameTable();
			DBG("Finished reading name table at offset %x\n", ptr_ - start_);
		}
#endif

		chunk.className = readString();
		chunk.actorTable = readActorTable();
		DBG("Finished reading second actor table at offset %x\n", offset());

		chunk.unknownInt3 = readInt32();

		XComActorTemplateTable actorTemplateTable = readActorTemplateTable(); // (only seems to be present for tactical saves?)
		assert(actorTemplateTable.size() == 0);
		DBG("Finished reading actor template table at offset %x\n", offset());

		chunk.displayName = readString(); //unknown (game name)
		chunk.mapName = readString(); //unknown (map name)
		chunk.unknownInt4 = readInt32(); //unknown  (checksum?)

		save.checkpoints.push_back(std::move(chunk));
	}
	while ((static_cast<size_t>(offset()) < length_));
	return save;
}