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

XComSave XComReader::getSaveData()
{
	XComSave save;
	const unsigned char *p = ptr_ + 1024;
	FILE *outFile = fopen("output.dat", "wb");
	int chunkCount = 0;
	save.header = readHeader();
	do
	{
		++chunkCount;
		unsigned long uncompressedSize = *(reinterpret_cast<const unsigned long*>(p + 4));
		unsigned long compressedSize = *(reinterpret_cast<const unsigned long*>(p + 8));
		printf("Next chunk size: %d (%d) bytes at offset 0x%08x\n", uncompressedSize, compressedSize, (p - start_));

		unsigned char *data = new unsigned char[uncompressedSize];
		unsigned long decompSize = uncompressedSize;
		if (lzo1x_decompress_safe(p + 24, compressedSize, data, &decompSize, nullptr) != LZO_E_OK) {
			fprintf(stderr, "LZO decompress failed\n");
		}

		fwrite(data, 1, decompSize, outFile);
		if (uncompressedSize != decompSize) {
			fprintf(stderr, "Expected to decompress %d bytes but only got %d\n", uncompressedSize, decompSize);
		}
		p += compressedSize;
		p += 24;
	} while ((p - start_) < length_);
	fclose(outFile);
	printf("%d chunks found\n", chunkCount);

	return save;
}