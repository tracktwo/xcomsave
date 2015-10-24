#ifndef XCOM_H
#define XCOM_H

#include <stdint.h>
#include <string>

struct SaveHeader
{
	uint32_t version;
	uint32_t uncompressedSize;
	uint32_t gameNumber;
	uint32_t saveNumber;
	std::string saveDescription;
	std::string time;
	std::string mapCommand;
	bool tacticalSave;
	bool ironman;
	bool autoSave;
	std::string dlcString;
	std::string language;
	uint32_t crc;
};

struct XComSave
{
	SaveHeader header;
};


XComSave XSReadSave(const unsigned char *, long);

#endif //XCOM_H
