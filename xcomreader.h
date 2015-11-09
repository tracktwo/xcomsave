#ifndef XCOMREADER_H
#define XCOMREADER_H

#include <stdint.h>
#include <string>

#include "xcom.h"
#include "util.h"

class XComReader
{
public:
	XComReader(Buffer&& b) :
		start_(std::move(b.buf))
	{
		length_ = b.len;
		ptr_ = start_.get();
	}

	XComSave getSaveData();

private:
	ptrdiff_t offset() const {
		return ptr_ - start_.get();
	}

	uint32_t readInt32();
	float readFloat();
	std::string readString();
	bool readBool();
	XComSaveHeader readHeader();
	XComActorTable readActorTable();
	XComCheckpointTable readCheckpointTable();
	std::vector<std::unique_ptr<XComProperty>> readProperties(uint32_t dataLen);
	XComPropertyPtr makeArrayProperty(const std::string& name, uint32_t propSize);
	XComActorTemplateTable readActorTemplateTable();
	XComNameTable readNameTable();
	int32_t getuncompressed_size();
	void getUncompressedData(unsigned char *);

private:
	std::unique_ptr<unsigned char[]> start_;
	unsigned char *ptr_;
	size_t length_;
};
#endif //XCOM_H
