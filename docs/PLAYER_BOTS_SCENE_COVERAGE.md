# Player Bots — scene coverage policy

Generated from the uploaded `scene_table.h` and the implemented `BotScenePolicy.h`.
This is a **code-policy inventory, not a record of in-game map tests**.

There are 102 defined scenes in this table. The reinforcement whitelist has
22 scene/state entries. Thirty explicitly excluded scenes remain unchanged.
Every other defined scene permits peaceful or combat-enabled bots according to
the policy below, subject to legal navigation, space, simulation state and capacity.
Undefined and unknown scene IDs get no enemy reinforcements.

All four Clock Town sections, houses, shops, and ordinary safe interiors have **no
added enemies**. Scene-wide dungeon approval does not by itself allow an empty
puzzle room to receive enemies: a supported original ordinary enemy must remain
in the current room. Full bosses and protected scripted actors are not bot targets.

Population is per loaded navigable area. A permitted map can still have fewer
than the configured 10–20 bots, or no bot at all when no legal spawn can be found.
The maximum is not a simultaneous persistent population across unloaded maps.

| ID | Scene name from source | Identifier | Bot policy | Added enemies |
| --- | --- | --- | --- | --- |
| 0x00 | Southern Swamp (Clear) | `SCENE_20SICHITAI2` | Bots, subject to safe navigation | Approved combat scene |
| 0x07 | Lone Peak Shrine & Grottos | `SCENE_KAKUSIANA` | Peaceful bots, subject to space | No |
| 0x08 | Cutscene Scene | `SCENE_SPOT00` | Unchanged / no bots | No |
| 0x0A | Magic Hags' Potion Shop | `SCENE_WITCH_SHOP` | Peaceful bots, subject to space | No |
| 0x0B | Majora's Lair | `SCENE_LAST_BS` | Unchanged / no bots | No |
| 0x0C | Beneath the Graveyard | `SCENE_HAKASHITA` | Peaceful bots, subject to space | No |
| 0x0D | Curiosity Shop | `SCENE_AYASHIISHOP` | Peaceful bots, subject to space | No |
| 0x10 | Mama's House & Barn | `SCENE_OMOYA` | Peaceful bots, subject to space | No |
| 0x11 | Honey & Darling's Shop | `SCENE_BOWLING` | Unchanged / no bots | No |
| 0x12 | The Mayor's Residence | `SCENE_SONCHONOIE` | Peaceful bots, subject to space | No |
| 0x13 | Ikana Canyon | `SCENE_IKANA` | Bots, subject to safe navigation | Approved combat scene |
| 0x14 | Pirates' Fortress | `SCENE_KAIZOKU` | Unchanged / no bots | No |
| 0x15 | Milk Bar | `SCENE_MILK_BAR` | Peaceful bots, subject to space | No |
| 0x16 | Stone Tower Temple | `SCENE_INISIE_N` | Bots, subject to safe navigation | Conditional: native enemy in room |
| 0x17 | Treasure Chest Shop | `SCENE_TAKARAYA` | Peaceful bots, subject to space | No |
| 0x18 | Inverted Stone Tower Temple | `SCENE_INISIE_R` | Bots, subject to safe navigation | Conditional: native enemy in room |
| 0x19 | Clock Tower Rooftop | `SCENE_OKUJOU` | Unchanged / no bots | No |
| 0x1A | Before Clock Town | `SCENE_OPENINGDAN` | Unchanged / no bots | No |
| 0x1B | Woodfall Temple | `SCENE_MITURIN` | Bots, subject to safe navigation | Conditional: native enemy in room |
| 0x1C | Path to Mountain Village | `SCENE_13HUBUKINOMITI` | Bots, subject to safe navigation | Approved combat scene |
| 0x1D | Ancient Castle of Ikana | `SCENE_CASTLE` | Bots, subject to safe navigation | Conditional: native enemy in room |
| 0x1E | Deku Scrub Playground | `SCENE_DEKUTES` | Unchanged / no bots | No |
| 0x1F | Odolwa's Lair | `SCENE_MITURIN_BS` | Unchanged / no bots | No |
| 0x20 | Town Shooting Gallery | `SCENE_SYATEKI_MIZU` | Unchanged / no bots | No |
| 0x21 | Snowhead Temple | `SCENE_HAKUGIN` | Bots, subject to safe navigation | Conditional: native enemy in room |
| 0x22 | Milk Road | `SCENE_ROMANYMAE` | Peaceful bots, subject to space | No |
| 0x23 | Pirates' Fortress Interior | `SCENE_PIRATE` | Unchanged / no bots | No |
| 0x24 | Swamp Shooting Gallery | `SCENE_SYATEKI_MORI` | Unchanged / no bots | No |
| 0x25 | Pinnacle Rock | `SCENE_SINKAI` | Peaceful bots, subject to space | No |
| 0x26 | Fairy's Fountain | `SCENE_YOUSEI_IZUMI` | Peaceful bots, subject to space | No |
| 0x27 | Swamp Spider House | `SCENE_KINSTA1` | Peaceful bots, subject to space | No |
| 0x28 | Oceanside Spider House | `SCENE_KINDAN2` | Peaceful bots, subject to space | No |
| 0x29 | Astral Observatory | `SCENE_TENMON_DAI` | Peaceful bots, subject to space | No |
| 0x2A | Moon Deku Trial | `SCENE_LAST_DEKU` | Unchanged / no bots | No |
| 0x2B | Deku Palace | `SCENE_22DEKUCITY` | Peaceful bots, subject to space | No |
| 0x2C | Mountain Smithy | `SCENE_KAJIYA` | Peaceful bots, subject to space | No |
| 0x2D | Termina Field | `SCENE_00KEIKOKU` | Bots, subject to safe navigation | Approved combat scene |
| 0x2E | Post Office | `SCENE_POSTHOUSE` | Peaceful bots, subject to space | No |
| 0x2F | Marine Research Lab | `SCENE_LABO` | Peaceful bots, subject to space | No |
| 0x30 | Beneath Graveyard and Dampe's House | `SCENE_DANPEI2TEST` | Peaceful bots, subject to space | No |
| 0x32 | Goron Shrine | `SCENE_16GORON_HOUSE` | Peaceful bots, subject to space | No |
| 0x33 | Zora Hall | `SCENE_33ZORACITY` | Peaceful bots, subject to space | No |
| 0x34 | Trading Post | `SCENE_8ITEMSHOP` | Peaceful bots, subject to space | No |
| 0x35 | Romani Ranch | `SCENE_F01` | Peaceful bots, subject to space | No |
| 0x36 | Twinmold's Lair | `SCENE_INISIE_BS` | Unchanged / no bots | No |
| 0x37 | Great Bay Coast | `SCENE_30GYOSON` | Bots, subject to safe navigation | Approved combat scene |
| 0x38 | Zora Cape | `SCENE_31MISAKI` | Bots, subject to safe navigation | Approved combat scene |
| 0x39 | Lottery Shop | `SCENE_TAKARAKUJI` | Peaceful bots, subject to space | No |
| 0x3B | Pirates' Fortress Moat | `SCENE_TORIDE` | Unchanged / no bots | No |
| 0x3C | Fisherman's Hut | `SCENE_FISHERMAN` | Peaceful bots, subject to space | No |
| 0x3D | Goron Shop | `SCENE_GORONSHOP` | Peaceful bots, subject to space | No |
| 0x3E | Deku King's Chamber | `SCENE_DEKU_KING` | Peaceful bots, subject to space | No |
| 0x3F | Moon Goron Trial | `SCENE_LAST_GORON` | Unchanged / no bots | No |
| 0x40 | Road to Southern Swamp | `SCENE_24KEMONOMITI` | Bots, subject to safe navigation | Approved combat scene |
| 0x41 | Doggy Racetrack | `SCENE_F01_B` | Unchanged / no bots | No |
| 0x42 | Cucco Shack | `SCENE_F01C` | Peaceful bots, subject to space | No |
| 0x43 | Ikana Graveyard | `SCENE_BOTI` | Peaceful bots, subject to space | No |
| 0x44 | Goht's Lair | `SCENE_HAKUGIN_BS` | Unchanged / no bots | No |
| 0x45 | Southern Swamp (poison) | `SCENE_20SICHITAI` | Bots, subject to safe navigation | Approved combat scene |
| 0x46 | Woodfall | `SCENE_21MITURINMAE` | Bots, subject to safe navigation | Approved combat scene |
| 0x47 | Moon Zora Trial | `SCENE_LAST_ZORA` | Unchanged / no bots | No |
| 0x48 | Goron Village (spring) | `SCENE_11GORONNOSATO2` | Peaceful bots, subject to space | No |
| 0x49 | Great Bay Temple | `SCENE_SEA` | Bots, subject to safe navigation | Conditional: native enemy in room |
| 0x4A | Waterfall Rapids | `SCENE_35TAKI` | Peaceful bots, subject to space | No |
| 0x4B | Beneath the Well | `SCENE_REDEAD` | Peaceful bots, subject to space | No |
| 0x4C | Zora Hall Rooms | `SCENE_BANDROOM` | Peaceful bots, subject to space | No |
| 0x4D | Goron Village (winter) | `SCENE_11GORONNOSATO` | Peaceful bots, subject to space | No |
| 0x4E | Goron Graveyard | `SCENE_GORON_HAKA` | Peaceful bots, subject to space | No |
| 0x4F | Sakon's Hideout | `SCENE_SECOM` | Unchanged / no bots | No |
| 0x50 | Mountain Village (winter) | `SCENE_10YUKIYAMANOMURA` | Peaceful bots, subject to space | No |
| 0x51 | Ghost Hut | `SCENE_TOUGITES` | Unchanged / no bots | No |
| 0x52 | Deku Shrine | `SCENE_DANPEI` | Unchanged / no bots | No |
| 0x53 | Road to Ikana | `SCENE_IKANAMAE` | Bots, subject to safe navigation | Approved combat scene |
| 0x54 | Swordsman's School | `SCENE_DOUJOU` | Peaceful bots, subject to space | No |
| 0x55 | Music Box House | `SCENE_MUSICHOUSE` | Peaceful bots, subject to space | No |
| 0x56 | Igos du Ikana's Lair | `SCENE_IKNINSIDE` | Unchanged / no bots | No |
| 0x57 | Tourist Information | `SCENE_MAP_SHOP` | Peaceful bots, subject to space | No |
| 0x58 | Stone Tower | `SCENE_F40` | Bots, subject to safe navigation | Approved combat scene |
| 0x59 | Inverted Stone Tower | `SCENE_F41` | Bots, subject to safe navigation | Approved combat scene |
| 0x5A | Mountain Village (spring) | `SCENE_10YUKIYAMANOMURA2` | Peaceful bots, subject to space | No |
| 0x5B | Path to Snowhead | `SCENE_14YUKIDAMANOMITI` | Bots, subject to safe navigation | Approved combat scene |
| 0x5C | Snowhead | `SCENE_12HAKUGINMAE` | Peaceful bots, subject to space | No |
| 0x5D | Path to Goron Village (winter) | `SCENE_17SETUGEN` | Bots, subject to safe navigation | Approved combat scene |
| 0x5E | Path to Goron Village (spring) | `SCENE_17SETUGEN2` | Bots, subject to safe navigation | Approved combat scene |
| 0x5F | Gyorg's Lair | `SCENE_SEA_BS` | Unchanged / no bots | No |
| 0x60 | Secret Shrine | `SCENE_RANDOM` | Unchanged / no bots | No |
| 0x61 | Stock Pot Inn | `SCENE_YADOYA` | Peaceful bots, subject to space | No |
| 0x62 | Great Bay Cutscene | `SCENE_KONPEKI_ENT` | Unchanged / no bots | No |
| 0x63 | Clock Tower Interior | `SCENE_INSIDETOWER` | Peaceful bots, subject to space | No |
| 0x64 | Woods of Mystery | `SCENE_26SARUNOMORI` | Bots, subject to safe navigation | Approved combat scene |
| 0x65 | Lost Woods (Intro) | `SCENE_LOST_WOODS` | Unchanged / no bots | No |
| 0x66 | Moon Link Trial | `SCENE_LAST_LINK` | Unchanged / no bots | No |
| 0x67 | The Moon | `SCENE_SOUGEN` | Peaceful bots, subject to space | No |
| 0x68 | Bomb Shop | `SCENE_BOMYA` | Peaceful bots, subject to space | No |
| 0x69 | Giants' Chamber | `SCENE_KYOJINNOMA` | Unchanged / no bots | No |
| 0x6A | Gorman Track | `SCENE_KOEPONARACE` | Unchanged / no bots | No |
| 0x6B | Goron Racetrack | `SCENE_GORONRACE` | Unchanged / no bots | No |
| 0x6C | East Clock Town | `SCENE_TOWN` | Peaceful bots, subject to space | No |
| 0x6D | West Clock Town | `SCENE_ICHIBA` | Peaceful bots, subject to space | No |
| 0x6E | North Clock Town | `SCENE_BACKTOWN` | Peaceful bots, subject to space | No |
| 0x6F | South Clock Town | `SCENE_CLOCKTOWER` | Peaceful bots, subject to space | No |
| 0x70 | Laundry Pool | `SCENE_ALLEY` | Peaceful bots, subject to space | No |
