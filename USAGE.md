# General
In order to edit a save, you need to:
1. Locate your savegame folder, which will be something like:
    - Linux: `~/.local/share/feral-interactive/XCOM/XEW/savedata/`
    - Windows: `~\Documents\My Games\XCOM - Enemy Unknown\XComGame\SaveData`
2. Convert the target save file to json with xcom2json.
3. Edit the file with your favorite text editor (Atom, gedit, KWrite etc.).
4. Convert the save file back to xcom format with json2xcom.

# xcom2json
Use `xcom2json <savegame_file>`.

The result will be an editable text file in JSON format. This file can then be edited to reflect the desired values (see [KNOWN](KNOWN.md) for currently known entities).

# json2xcom
Use `json2xcom <savegame_file>.json`.

You may use the "o" option to define the output file name: `json2xcom -o <output> <savegame_file>.json`

**Note**: 
1. XCOM:EW savegame files have no extension.
2. DO NOT use MS notepad on an international installation. File encoding is utf-8 and MS notepad might have a problem with that.
3. Currently only geoscape saves can be edited. 
