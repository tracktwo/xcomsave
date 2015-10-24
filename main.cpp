#include <string>
#include <iostream>
#include <stdio.h>
#include "xcomreader.h"

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

	XComReader reader{ fileBuf, fileLen };
	XComSave save = reader.getSaveData();
}
