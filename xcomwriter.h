#ifndef XCOMWRITER_H
#define XCOMWRITER_H

#include <stdint.h>
#include <string>

#include "xcom.h"

struct PropertyWriterVisitor;

class XComWriter
{
public:
	XComWriter(XComSave && save) :
		save_(std::move(save)), buf_{}, bufLen_{ 0 }
	{}

	std::unique_ptr<unsigned char[]> getSaveData();

	static const constexpr uint32_t initial_size = 1024 * 1024;

	friend PropertyWriterVisitor;

private:
	void ensureSpace(uint32_t count);
	void writeString(const std::string& str);
	void writeInt(uint32_t val);
	void writeFloat(float val);
	void writeBool(bool b);
	void writeActorTable(const XComActorTable& actorTable);
	void writeCheckpointChunks(const XComCheckpointChunkTable& chunks);
	void writeCheckpointChunk(const XComCheckpointChunk& chunk);
	void writeCheckpointTable(const XComCheckpointTable& table);
	void writeCheckpoint(const XComCheckpoint& chk);
	void writeProperty(const XComPropertyPtr& prop, uint32_t arrayIdx);
private:
	XComSave save_;
	unsigned char *start_;
	unsigned char* buf_;
	uint32_t bufLen_;
};

#endif // XCOMWRITER_H
