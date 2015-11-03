#include "xcomwriter.h"
#include <cassert>

void XComWriter::ensureSpace(uint32_t count)
{
	ptrdiff_t currentCount = buf_ - start_;

	if ((currentCount + count) > bufLen_) {
		uint32_t newLen = bufLen_ * 2;
		unsigned char * newBuf = new unsigned char[newLen];
		memcpy(newBuf, start_, currentCount);
		delete[] buf_;
		start_ = newBuf;
		bufLen_ = newLen;
		buf_ = start_ + currentCount;
	}
}

void XComWriter::writeString(const std::string& str)
{
	// Ensure we have space for the string size + string data + trailing null
	ensureSpace(str.length() + 5);
	writeInt(str.length() + 1);
	memcpy(buf_, str.c_str(), str.length());
	buf_ += str.length();
	*buf_++ = 0;
}

void XComWriter::writeInt(uint32_t val)
{
	ensureSpace(4);
	*(reinterpret_cast<uint32_t*>(buf_)) = val;
	buf_ += 4;
}


void XComWriter::writeFloat(float val)
{
	ensureSpace(4);
	*(reinterpret_cast<float*>(buf_)) = val;
	buf_ += 4;
}

void XComWriter::writeBool(bool b)
{
	writeInt(b);
}

void XComWriter::writeActorTable(const XComActorTable& actorTable)
{
	// Each actorTable entry has 2 entries in the save table; names are split.
	writeInt(actorTable.size() * 2);
	for (const XComActor& actor : actorTable) {
		writeString(actor.actorName.second);
		writeInt(actor.instanceNum);
		writeString(actor.actorName.first);
		writeInt(0);
	}
}

struct PropertyWriterVisitor : public XComPropertyVisitor
{
	PropertyWriterVisitor(XComWriter* writer) : writer_(writer) {}

	virtual void visitInt(XComIntProperty *prop) override
	{
		writer_->writeInt(prop->value);
	}

	virtual void visitFloat(XComFloatProperty *prop) override
	{
		writer_->writeFloat(prop->value);
	}

	virtual void visitBool(XComBoolProperty *prop) override
	{
		writer_->writeBool(prop->value);
	}

	virtual void visitString(XComStringProperty *prop) override
	{
		writer_->writeString(prop->str);
	}

	virtual void visitObject(XComObjectProperty *prop) override
	{
		assert(prop->getSize() == prop->data.size());
		writer_->ensureSpace(prop->getSize());
		for (unsigned int i = 0; i < prop->getSize(); ++i) {
			*writer_->buf_++ = prop->data[i];
		}
	}

	virtual void visitByte(XComByteProperty *prop) override
	{
		writer_->writeString(prop->enumType);
		writer_->writeInt(0);
		writer_->writeString(prop->enumVal);
		writer_->writeInt(prop->extVal);
	}

	virtual void visitStruct(XComStructProperty *prop) override
	{
		writer_->writeString(prop->structName);
		writer_->writeInt(0);
		if (prop->nativeDataLen > 0) {
			writer_->ensureSpace(prop->nativeDataLen);
			memcpy(writer_->buf_, prop->nativeData, prop->nativeDataLen);
		}
		else {
			for (unsigned int i = 0; i < prop->structProps.size(); ++i) {
				writer_->writeProperty(prop->structProps[i], 0);
			}
		}
	}

	virtual void visitArray(XComArrayProperty *prop) override
	{
		writer_->writeInt(prop->arrayBound);
		assert(prop->getSize() == ((prop->arrayBound * prop->elementSize) - 4));
		writer_->ensureSpace(prop->arrayBound * prop->elementSize);
		memcpy(writer_->buf_, prop->data, prop->getSize() - 4);
	}

	virtual void visitStaticArray(XComStaticArrayProperty *) override
	{
		// This shouldn't happen: static arrays need special handling and can't be written normally as they don't
		// really exist in the save format.
		assert(false);
	}

private:
	XComWriter *writer_;
};

std::string propKindToString(XComProperty::Kind k)
{
	switch (k)
	{
	case XComProperty::Kind::IntProperty: return "IntProperty";
	case XComProperty::Kind::StrProperty: return "StrProperty";
	case XComProperty::Kind::BoolProperty: return "BoolProperty";
	case XComProperty::Kind::FloatProperty: return "FloatProperty";
	case XComProperty::Kind::ObjectProperty: return "ObjectProperty";
	case XComProperty::Kind::ByteProperty: return "ByteProperty";
	case XComProperty::Kind::StructProperty: return "StructProperty";
	case XComProperty::Kind::ArrayProperty: return "ArrayProperty";
	default:
		throw new std::exception("Unknown property kind");
	}
}

void XComWriter::writeProperty(const XComPropertyPtr& prop, uint32_t arrayIdx)
{
	// If this is a static array property we need to write only the contained properties, not the fake static array property created to contain it.
	if (prop->getKind() == XComProperty::Kind::StaticArrayProperty) {
		XComStaticArrayProperty* staticArray = dynamic_cast<XComStaticArrayProperty*>(prop.get());
		for (unsigned int idx = 0; idx < staticArray->size(); ++idx) {
			writeProperty(staticArray->properties[idx], idx);
		}
	}
	else {
		// Write the common part of a property
		writeString(prop->getName());
		writeInt(0);
		writeString("None");
		writeString(propKindToString(prop->getKind()));
		writeInt(0);
		writeInt(prop->getSize());
		writeInt(arrayIdx);

		// Write the specific part
		PropertyWriterVisitor v{ this };
		prop->accept(&v);
	}
}

void XComWriter::writeCheckpoint(const XComCheckpoint& chk)
{
	writeString(chk.name);
	writeString(chk.instanceName);
	writeFloat(chk.vector[0]);
	writeFloat(chk.vector[1]);
	writeFloat(chk.vector[2]);
	writeFloat(chk.rotator[0]);
	writeFloat(chk.rotator[1]);
	writeFloat(chk.rotator[2]);
	writeString(chk.className);
	writeInt(chk.propLen);
	for (unsigned int i = 0; i < chk.properties.size(); ++i) {
		writeProperty(chk.properties[i], 0);
	}
	// TODO Padsize isn't written properly
	ensureSpace(chk.padSize);
	for (unsigned int i = 0; i < chk.padSize; ++i) {
		*buf_++ = 0;
	}
	writeInt(chk.templateIndex);
}

void XComWriter::writeCheckpointTable(const XComCheckpointTable& table)
{
	writeInt(table.size());
	for (const XComCheckpoint& chk : table) {
		writeCheckpoint(chk);
	}
}

void XComWriter::writeCheckpointChunk(const XComCheckpointChunk& chunk)
{
	writeInt(chunk.unknownInt1);
	writeString(chunk.gameType);
	writeString("None");
	writeInt(chunk.unknownInt2);
	writeCheckpointTable(chunk.checkpointTable);
	writeInt(0); // name table length
	writeString(chunk.className);
	writeActorTable(chunk.actorTable);
	writeInt(chunk.unknownInt3);
	writeInt(0); // actor template table length
	writeString(chunk.displayName);
	writeString(chunk.mapName);
	writeInt(chunk.unknownInt4);
}

void XComWriter::writeCheckpointChunks(const XComCheckpointChunkTable& chunks)
{
	for (const XComCheckpointChunk& chunk : chunks) {
		writeCheckpointChunk(chunk);
	}
}

std::unique_ptr<unsigned char []> XComWriter::getSaveData()
{
	start_ = buf_ = new unsigned char[initial_size];
	bufLen_ = initial_size;

	// Write out the initial actor table
	writeActorTable(save_.actorTable);

	// Write the checkpoint chunks
	writeCheckpointChunks(save_.checkpoints);

	std::unique_ptr<unsigned char[]> ptr{ start_ };
	start_ = nullptr;
	buf_ = nullptr;
	return ptr;
}