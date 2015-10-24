#ifndef XCOM_H
#define XCOM_H

#include <stdint.h>

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

#endif // XCOM_H
