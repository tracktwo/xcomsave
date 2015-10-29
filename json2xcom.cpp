#include "xcom.h"
#include "xcomwriter.h"
#include "json11.hpp"

#include <iostream>

#include <windows.h>
#include <wincrypt.h>

using namespace json11;

XComSaveHeader buildHeader(const Json& json)
{
	XComSaveHeader hdr;
	Json::shape shape = {
		{ "version", Json::NUMBER },
		{ "uncompressed_size", Json::NUMBER },
		{ "game_number", Json::NUMBER },
		{ "save_number", Json::NUMBER },
		{ "save_description", Json::STRING },
		{ "time", Json::STRING },
		{ "map_command", Json::STRING },
		{ "tactical_save", Json::BOOL },
		{ "ironman", Json::BOOL },
		{ "autosave", Json::BOOL },
		{ "dlc", Json::STRING },
		{ "language", Json::STRING }
	};

	std::string err;
	if (!json.has_shape(shape, err)) {
		throw std::exception("Error reading json file: header format does not match\n");
	}

	hdr.version = json["version"].int_value();
	hdr.uncompressedSize = json["uncompressed_size"].int_value();
	hdr.gameNumber = json["game_number"].int_value();
	hdr.saveNumber = json["save_number"].int_value();
	hdr.saveDescription = json["save_description"].string_value();
	hdr.time = json["time"].string_value();
	hdr.mapCommand = json["map_command"].string_value();
	hdr.tacticalSave = json["tactical_save"].bool_value();
	hdr.ironman = json["ironman"].bool_value();
	hdr.autoSave = json["autosave"].bool_value();
	hdr.dlcString = json["dlc"].string_value();
	hdr.language = json["language"].string_value();

	return hdr;
}

XComSave buildSave(const Json& json)
{
	XComSave save;
	std::string err;
	Json::shape shape = {
		{ "header", Json::OBJECT },
		{ "actor_table", Json::ARRAY },
		{ "checkpoints", Json::ARRAY }
	};

	if (!json.has_shape(shape, err)) {
		throw std::exception("Error reading json file: format mismatch at root\n");
		return XComSave{};
	}

	save.header = buildHeader(json["header"]);
	return save;
}

void usage(const char * name)
{
	printf("Usage: %s -i <infile> -o <outfile>\n", name);
}

char * read_file(const std::string& filename, long *fileLen)
{
	FILE *fp = fopen(filename.c_str(), "rb");
	if (fp == nullptr) {
		fprintf(stderr, "Error opening file %s\n", filename.c_str());
		return nullptr;
	}

	if (fseek(fp, 0, SEEK_END) != 0) {
		fprintf(stderr, "Error determining file length\n");
		return nullptr;
	}

	*fileLen = ftell(fp);

	if (fseek(fp, 0, SEEK_SET) != 0) {
		fprintf(stderr, "Error determining file length\n");
		return nullptr;
	}

	char* filebuf = new char[(*fileLen) + 1];
	if (fread(filebuf, 1, *fileLen, fp) != *fileLen) {
		fprintf(stderr, "Error reading file contents\n");
		return nullptr;
	}
	filebuf[(*fileLen)] = 0;
	fclose(fp);
	return filebuf;
}

int main(int argc, char *argv[])
{
	bool writesave = false;
	const char *infile = nullptr;
	const char *outfile = nullptr;

	if (argc <= 1) {
		usage(argv[0]);
		return 1;
	}

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-i") == 0) {
			infile = argv[++i];
		}
		else if (strcmp(argv[i], "-o") == 0) {
			outfile = argv[++i];
		}
	}

	if (infile == nullptr || outfile == nullptr) {
		usage(argv[0]);
		return 1;
	}

	long fileLen = 0;
	auto fileBuf = read_file(infile, &fileLen);

	if (fileBuf == nullptr) {
		return 1;
	}

	std::string errStr;
	Json jsonsave = Json::parse(fileBuf, errStr);
	delete[] fileBuf;

	try {
		XComSave save = buildSave(jsonsave);
	}
	catch (std::exception e)
	{
		fprintf(stderr, e.what());
		return 1;
	}
}