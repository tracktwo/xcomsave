This is a list of known strings in the savegame file.

# Funds:
look for the line containing `"name": "m_iCash", "kind": "IntProperty",`.

# Items:
Most of the items (Weapons, Alloys etc.) are located in an array called **m_arrItems**. 

Meld is located in an array after `Meld "Command1.TheWorld:PersistentLevel.XGBattleDesc_0"`. 
Please note that this array hold the amount you found since the beginning of the game and not your current amount. 

# Team members
Look for the entry: '"name": "Command1.TheWorld:PersistentLevel.XGStrategySoldier_0", ' were the 0 is the first soldier in your roster, 1 is the second, etc (keep in mind that soldiers may be sorted differently in the game because usually they are sorted by: number of mission>number of kills>etc)
Stats: My best guest is this
look for an array called:
"name": "aStats", 
                  "kind": "StaticArrayProperty", 
                  "int_values": [ 4, 103, 0, 13, 1, 0, 0, 165, 0, 0, 27, 0, 0, 0, 0, 0, 0, 0, 0 ] 
This array is roughly 60 lines after the "name" entry
First position (4 in above example) is hitpoints (default 4 in game for a rookie)
Second position (103 in above example) is aim
8th position is Will (165 in above example)

Make a psychic:
Look for '"name": "bHasPsiGift", "kind": "BoolProperty", "value": false },'
Change "false" to "true"
This will NOT immendiately grant the class, only allow you to unlock later through the psy lab
To grant it now, change the value "bHasPsiGift" AND the value
'"name": "bForcePsiGift", "kind": "BoolProperty", "value": true }'

