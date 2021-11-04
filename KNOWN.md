This is a list of known strings in the savegame file.

# Funds

Look for the line containing `"name": "m_iCash", "kind": "IntProperty",` and change `"value":` to the desired amount of money.

# Scientists

Look for the line containing `"name": "m_iNumScientists", "kind": "IntProperty",` and change `"value":` to the desired number of scientists.

# Engineers

Look for the line containing `"name": "m_iNumEngineers", "kind": "IntProperty",` and change `"value":` to the desired number of engineers.

# Items

Most of the items (Weapons, Alloys etc.) are located in an array called **m_arrItems**. To modify the quantity of a given item, change the associated array element number to the desired quantity. The following array elements have been identified within Enemy Unknown:

|Element Number|Description|
|----|----|
|145|Sectoid Corpse|
|146|Sectoid Commander Corpse|
|147|Floater Corpse|
|149|Thin Man Corpse|
|150|Muton Corpse|
|151|?|
|152|Berserker Corpse|
|153|Cyberdisc Wreck|
|154|?|
|155|Chryssalid Corpse|
|156|?|
|157|?|
|158|Drone Wreck|
|159|?|
|160|?|
|161|?|
|162|?|
|163|?|
|164|?|
|165|?|
|166|?|
|167|?|
|168|?|
|169|?|
|170|?|
|171|?|
|172|Elerium|
|173|Alien Alloys|
|174|Weapon Fragments|
|175|Alien Entertainment|
|176|Alien Food|
|177|Alien Stasis Tank|
|178|UFO Flight Computer|
|179|Alien Surgery|
|180|UFO Power Source|
|181|?|
|182|?|
|183|Alien Food (damaged)|
|184|?|
|185|?|
|186|?|
|187|?|
|188|?|
|189|Fusion Core|

# Meld 

Meld is located in an array after `Meld "Command1.TheWorld:PersistentLevel.XGBattleDesc_0"`. 
Please note that this array hold the amount you found since the beginning of the game and not your current amount. 

# Team Members

TODO