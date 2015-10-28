#include "jsonwriter.h"

Json buildJson(const XComSave& save)
{
	const XComSaveHeader &hdr = save.header;

	Json header = Json::object{ 
		{ "version", (int)hdr.version },
		{ "uncompressed_size", (int)hdr.uncompressedSize },
		{ "game_number", (int) hdr.gameNumber },
		{ "save_number", (int) hdr.saveNumber },
		{ "save_description", hdr.saveDescription },
		{ "time", hdr.time },
		{ "map_command", hdr.mapCommand },
		{ "tactical_save", hdr.tacticalSave },
		{ "ironman", hdr.ironman },
		{ "autosave", hdr.autoSave },
		{ "dlc", hdr.dlcString },
		{ "language", hdr.language }
	};


	Json jsonSave = Json::object{
		{ "header", header },
		{ "actor_table", save.actorTable },
		{ "checkpoints", save.checkpoints },
		
	};

	return jsonSave;
}