#ifndef XCOM_H
#define XCOM_H

#include <stdint.h>
#include <string>

struct XComSaveHeader
{
	uint32_t version;
	uint32_t uncompressedSize;
	uint32_t gameNumber;
	uint32_t saveNumber;
	const char* saveDescription;
	const char* time;
	const char* mapCommand;
	bool tacticalSave;
	bool ironman;
	bool autoSave;
	const char* dlcString;
	const char* language;
	uint32_t crc;
};

struct XComSave
{
	XComSaveHeader header;
};


class XComReader
{
public:
	XComReader(const unsigned char *ptr, long len) :
		ptr_(ptr), start_(ptr), length_(len) {}

	XComSave getSaveData();
private:

	uint32_t readInt32();
	const char* readString();
	bool readBool();
	XComSaveHeader readHeader();
	const unsigned char *ptr_;
	const unsigned char *start_;
	long length_;
};
#endif //XCOM_H
