#include <string>
#include <iostream>
#include "minilzo.h"
#include "platform.h"
#include <stdio.h>

void usage(const std::string& name)
{
	printf("Usage: %s <save file>\n", name.c_str());
}

const unsigned char * read_file(const std::string& filename, long *fileLen)
{
	FILE *fp = fopen(filename.c_str(), "rb");
	if (fp == nullptr) {
		fprintf(stderr, "Error opening file\n");
		return nullptr;
	}

	if (fseek(fp, 0, SEEK_END) != 0) {
		fprintf(stderr, "Error determining file length\n");
		return nullptr;
	}

	*fileLen = ftell(fp);
	
	if (fseek(fp, 0, SEEK_SET) != 0) {
		fprintf(stderr, "Error determining file length\n");
		return nullptr;
	}

	unsigned char * filebuf = new unsigned char[*fileLen];
	if (fread(filebuf, 1, *fileLen, fp) != *fileLen) {
		fprintf(stderr, "Error reading file contents\n");
		return nullptr;
	}

	fclose(fp);
	return filebuf;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        usage(argv[0]);
        return 1;
    }

	long fileLen = 0;
    const unsigned char *fileBuf = read_file(argv[1], &fileLen);

	if (fileBuf == nullptr) {
		return 1;
	}

	const unsigned char *p = fileBuf + 1024;
	FILE *outFile = fopen("output.dat", "wb");
	do
	{
		unsigned long uncompressedSize = *(reinterpret_cast<const unsigned long*>(p + 4));
		unsigned long compressedSize = *(reinterpret_cast<const unsigned long*>(p + 8));
		printf("Next chunk size: %d (%d) bytes at offset 0x%08x\n", uncompressedSize, compressedSize, (p - fileBuf));

		unsigned char *data = new unsigned char[uncompressedSize];
		unsigned long decompSize = uncompressedSize;
		if (lzo1x_decompress(p + 24, compressedSize, data, &decompSize, nullptr) != LZO_E_OK) {
			fprintf(stderr, "LZO decompress failed\n");
		}

		fwrite(data, 1, decompSize, outFile);
		if (uncompressedSize != decompSize) {
			fprintf(stderr, "Expected to decompress %d bytes but only got %d\n", uncompressedSize, decompSize);
		}
		p += compressedSize;
		p += 24;
	} while ((p - fileBuf) < fileLen);
	fclose(outFile);
}
