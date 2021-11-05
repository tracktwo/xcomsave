This is a list of known strings in the savegame file.

# Funds

Look for the line containing `"name": "m_iCash", "kind": "IntProperty",` and change the number after `"value":` to the desired amount of money.

# Scientists

Look for the line containing `"name": "m_iNumScientists", "kind": "IntProperty",` and change the number after `"value":` to the desired number of scientists.

# Engineers

Look for the line containing `"name": "m_iNumEngineers", "kind": "IntProperty",` and change the number after `"value":` to the desired number of engineers.

# Items

Most of the items (Weapons, Alloys etc.) are located in an array called **m_arrItems**. To modify the quantity of a given item, change the associated array element number to the desired quantity. The following array elements have been identified within Enemy Unknown:

|Element Number|Description|
|----|----|
|59|Carapace Armor|
|60|Skeleton Suit|
|61|Titan Armor|
|62|Archangel Armor|
|63|Ghost Armor|
|64|Psi Armor|
|65-76|UNDEFINED|
|77|Medikit|
|78|Combat Stims|
|79|Mind Shield|
|80|Chitin Plating|
|81|Arc Thrower|
|82|S.C.O.P.E.|
|83|Nano-Fiber Vest|
|84-88|UNDEFINED|
|89|Alien Grenade|
|90-99|UNDEFINED|
|100|Battle Scanner|
|101|?|
|102|?|
|103|S.H.I.V.|
|104|Alloy S.H.I.V.|
|105|Hover S.H.I.V.|
|106|UNDEFINED|
|107|Firestorm|
|108|UNDEFINED|
|109|Satellite|
|110-123|UNDEFINED|
|124|Phoenix Cannon|
|125|UNDEFINED|
|126|Laser Cannon|
|127|Plasma Cannon|
|128|EMP Cannon|
|129|Fusion Lance|
|130-133|UNDEFINED|
|134|Defense Matrix (Dodge)|
|135|UFO Tracking (Boost)|
|136|Uplink Targeting (Aim)|
|137-144|UNDEFINED|
|145|Sectoid Corpse|
|146|Sectoid Commander Corpse|
|147|Floater Corpse|
|149|Thin Man Corpse|
|150|Muton Corpse|
|151|Muton Elite Corpse|
|152|Berserker Corpse|
|153|Cyberdisc Wreck|
|154|Ethereal Corpse|
|155|Chryssalid Corpse|
|156|UNDEFINED|
|157|Sectopod Wreck|
|158|Drone Wreck|
|159-171|UNDEFINED|
|172|Elerium|
|173|Alien Alloys|
|174|Weapon Fragments|
|175|Alien Entertainment|
|176|Alien Food|
|177|Alien Stasis Tank|
|178|UFO Flight Computer|
|179|Alien Surgery|
|180|UFO Power Source|
|181|Hyperwave Beacon|
|182|Alien Entertainment (Damaged)|
|183|Alien Food (Damaged)|
|184|Alien Stasis Tank (Damaged)|
|185|UFO Flight Computer (Damaged)|
|186|Alien Surgery (Damaged)|
|187|UFO Power Source (Damaged)|
|188|Hyperwave Beacon (Damaged)|
|189|Fusion Core|
|190|Ethereal Device|
|191|UNDEFINED|
|192|Outsider Shard|
|193|Skeleton Key|
|194|UNDEFINED|

# Meld 

Meld is located in an array after `Meld "Command1.TheWorld:PersistentLevel.XGBattleDesc_0"`. 
Please note that this array stores the amount you found since the beginning of the game and not your current amount. 

# Team Members

TODO