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

struct XComActor
{
	uint32_t instanceNum;
	char *actorName;
};

struct XComActorTable
{
	uint32_t actorCount;
	XComActor *actors;
};

struct XComCheckpoint
{
	char *fullName;
	char *instanceName;
	char unknown1[24];
	char *className;
	uint32_t dataLen;
	unsigned char *data;
};

struct XComCheckpointTable
{
	uint32_t checkpointCount;
	XComCheckpoint *checkpoints;
};

struct XComSave
{
	XComSaveHeader header;
	XComActorTable actorTable;
	uint32_t unknownInt1; // 4 unknown bytes immediately following the actor table.
	const char *unknownStr1; // unknown string
	uint32_t unknownInt2; // 4 unknown bytes after the "None" string.
	XComCheckpointTable checkpointTable;
	uint32_t unknownInt3; // 4 unknown bytes after the checkpoint table
	const char *unknownStr2; // Unknown string 2 
	XComActorTable actorTable2;
};


unsigned int crc32b(const unsigned char *message, long len);

#endif // XCOM_H
