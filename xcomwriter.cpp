#include "xcomwriter.h"

std::unique_ptr<unsigned char *> XComWriter::getSaveData()
{
	auto ptr = std::make_unique<unsigned char *>(buf_);
	buf_ = nullptr;
	return ptr;
}