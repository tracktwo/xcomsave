#include "minilzo.h"
#include "xcomreader.h"
#include <string>

uint32_t XComReader::readInt32()
{
	uint32_t v = *(reinterpret_cast<const uint32_t*>(ptr_));
	ptr_ += 4;
	return v;
}

bool XComReader::readBool()
{
	return readInt32() != 0;
}

const char* XComReader::readString()
{
	uint32_t len = readInt32();
	const char *str = reinterpret_cast<const char *>(ptr_);
	size_t actualLen = strlen(str);
	
	// Double check the length matches what we read from the file, considering the trailing null is counted in the length 
	// stored in the file.
	if (actualLen != (len-1))
	{
		fprintf(stderr, "Error: String mismatch at offset %d. Expected length %d but got %d\n", ptr_ - start_, len, actualLen);
		return "<error>";
	}

	ptr_ += len;
	return str;
}

XComSaveHeader XComReader::readHeader()
{
	XComSaveHeader hdr;
	hdr.version = readInt32();
	if (hdr.version != 0x10) {
		fprintf(stderr, "Error: Data does not appear to be an xcom save: expected file version 16 but got %d\n", hdr.version);
		return {0};
	}
	hdr.uncompressedSize = readInt32();
	hdr.gameNumber = readInt32();
	hdr.saveNumber = readInt32();
	hdr.saveDescription = readString();
	hdr.time = readString();
	hdr.mapCommand = readString();
	hdr.tacticalSave = readBool();
	hdr.ironman = readBool();
	hdr.autoSave = readBool();
	hdr.dlcString = readString();
	hdr.language = readString();
	hdr.crc = readInt32();
	return hdr;
}

XComActorTable XComReader::readActorTable()
{
	XComActorTable actorTable;
	actorTable.actorCount = readInt32();
	actorTable.actors = new XComActor[actorTable.actorCount];

	for (unsigned int i = 0; i < actorTable.actorCount; ++i) {
		actorTable.actors[i].actorName = strdup(readString());
		actorTable.actors[i].instanceNum = readInt32();
	}
	return actorTable;
}

XComCheckpointTable XComReader::readCheckpointTable()
{
	XComCheckpointTable checkpointTable;
	checkpointTable.checkpointCount = readInt32();
	checkpointTable.checkpoints = new XComCheckpoint[checkpointTable.checkpointCount];

	for (unsigned int i = 0; i < checkpointTable.checkpointCount; ++i) {
		XComCheckpoint *chk = &checkpointTable.checkpoints[i];
		chk->fullName = strdup(readString());
		chk->instanceName = strdup(readString());
		memcpy(chk->unknown1, ptr_, 24);
		ptr_ += 24;
		chk->className = strdup(readString());
		chk->dataLen = readInt32();
		chk->data = new unsigned char[chk->dataLen];
		memcpy(chk->data, ptr_, chk->dataLen);
		ptr_ += chk->dataLen;
		if (readInt32() != 0xffffffff) {
			fprintf(stderr, "Error: Checkpoint entry %d doesn't end with -1 at offset 0x%08x\n", i, ptr_ - start_);
			return{};
		}
	}

	return checkpointTable;
}

int32_t XComReader::getUncompressedSize()
{
	// The compressed data begins 1024 bytes into the file.
	const unsigned char* p = start_ + 1024;
	uint32_t compressedSize;
	uint32_t uncompressedSize = 0;
	do
	{
		// Expect the magic header value 0x9e2a83c1 at the start of each chunk
		if (*(reinterpret_cast<const uint32_t*>(p)) != 0x9e2a83c1) {
			fprintf(stderr, "Failed to find compressed chunk at offset 0x%08x", (p - start_));
			return -1;
		}

		// Compressed size is at p+8
		compressedSize = *(reinterpret_cast<const unsigned long*>(p + 8));

		// Uncompressed size is at p+12
		uncompressedSize += *(reinterpret_cast<const unsigned long*>(p + 12));

		// Skip to next chunk - 24 bytes of this chunk header + compressedSize bytes later.
		p += (compressedSize + 24);
	} while (p - start_ < length_);

	return uncompressedSize;
}

void XComReader::getUncompressedData(unsigned char *buf)
{
	// Start back at the beginning of the compressed data.
	const unsigned char *p = start_ + 1024;
	unsigned char *outp = buf;
	do
	{
		// Expect the magic header value 0x9e2a83c1 at the start of each chunk
		if (*(reinterpret_cast<const uint32_t*>(p)) != 0x9e2a83c1) {
			fprintf(stderr, "Failed to find compressed chunk at offset 0x%08x", (p - start_));
			return;
		}

		// Compressed size is at p+8
		uint32_t compressedSize = *(reinterpret_cast<const unsigned long*>(p + 8));

		// Uncompressed size is at p+12
		uint32_t uncompressedSize = *(reinterpret_cast<const unsigned long*>(p + 12));
		
		unsigned long decompSize = uncompressedSize;

		if (lzo1x_decompress_safe(p + 24, compressedSize, outp, &decompSize, nullptr) != LZO_E_OK) {
			fprintf(stderr, "LZO decompress of save data failed\n");
			return;
		}

		// Skip to next chunk - 24 bytes of this chunk header + compressedSize bytes later.
		p += (compressedSize + 24);
		outp += uncompressedSize;
	} while (p - start_ < length_);
}

XComSave XComReader::getSaveData()
{
	XComSave save;
	const unsigned char *p = ptr_ + 1024;
	FILE *outFile = fopen("output.dat", "wb");
	int chunkCount = 0;
	int totalCompressed = 0;
	int totalUncompressed = 0;
	save.header = readHeader();

	uint32_t uncompressedSize = getUncompressedSize();
	if (uncompressedSize < 0) {
		return {};
	}
	
	unsigned char *uncompressedData = new unsigned char[uncompressedSize];
	getUncompressedData(uncompressedData);

	// We're now done with the compressed file. Swap over to the uncompressed data
	start_ = ptr_ = uncompressedData;
	length_ = uncompressedSize;
	save.actorTable = readActorTable();
	printf("Finished reading actor table at offset %x\n", ptr_ - start_);
	save.unknownInt1 = readInt32();
	save.unknownStr1 = readString();
	const char *none = readString();
	if (strcmp(none, "None") != 0) {
		fprintf(stderr, "Error locating 'None' after actor table.\n");
		return{};
	}
	save.unknownInt2 = readInt32();
	save.checkpointTable = readCheckpointTable();
	printf("Finished reading checkpoint table at offset %x\n", ptr_ - start_);

	save.unknownInt3 = readInt32();
	save.unknownStr2 = readString();
	save.actorTable2 = readActorTable();
	printf("Finished reading second actor table at offset %x\n", ptr_ - start_);
	fwrite(uncompressedData, 1, uncompressedSize, outFile);
	fclose(outFile);
	return save;
}