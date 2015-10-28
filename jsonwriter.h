#ifndef JSONWRITER_H
#define JSONWRITER_H

#include "xcom.h"
#include "json11.hpp"

using namespace json11;

Json buildJson(const XComSave& save);
#endif //JSONWRITER_H
