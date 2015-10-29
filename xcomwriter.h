#ifndef XCOMWRITER_H
#define XCOMWRITER_H

#include <stdint.h>
#include <string>

#include "xcom.h"

class XComWriter
{
public:
	XComWriter(XComSave && save) :
		save_(std::move(save)) {
		buf_ = new unsigned char[initial_size];
		bufLen_ = initial_size;
	}

	std::unique_ptr<unsigned char *> getSaveData();

	static const constexpr uint32_t initial_size = 1024 * 1024;

private:
	XComSave save_;
	unsigned char *buf_;
	uint32_t bufLen_;
};

#endif // XCOMWRITER_H
