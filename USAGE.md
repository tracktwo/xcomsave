# General
In order to edit a save, you need to:
1. Locate your savegame folder. In steam on Linux it's: `~/.local/share/feral-interactive/XCOM/XEW/savedata/`
2. Convert the target save file to json with xcom2json.
3. Edit the file with your favorite text editor (Atom, gedit, KWrite etc.).
4. Convert the save file back to xcom format with json2xcom.

# xcom2json
Use `xcom2json <savegame_file>`.

The result will be an editable text file.

# json2xcom
Use `json2xcom <savegame_file>.json`.
You may use the "o" option to define the output file name: `json2xcom -o <output> <savegame_file>.json`

**Note**: XCOM:EW savegame files have no extension.



