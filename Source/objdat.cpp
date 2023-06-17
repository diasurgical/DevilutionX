/**
 * @file objdat.cpp
 *
 * Implementation of all object data.
 */
#include "objdat.h"

namespace devilution {

/** Maps from dun_object_id to object_id. */
const _object_id ObjTypeConv[] = {
	OBJ_NULL,
	OBJ_LEVER,
	OBJ_CRUX1,
	OBJ_CRUX2,
	OBJ_CRUX3,
	OBJ_ANGEL,
	OBJ_BANNERL,
	OBJ_BANNERM,
	OBJ_BANNERR,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_BOOK2L,
	OBJ_BOOK2R,
	OBJ_BCROSS,
	OBJ_NULL,
	OBJ_CANDLE1,
	OBJ_CANDLE2,
	OBJ_CANDLEO,
	OBJ_CAULDRON,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_FLAMEHOLE,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_MCIRCLE1,
	OBJ_MCIRCLE2,
	OBJ_SKFIRE,
	OBJ_SKPILE,
	OBJ_SKSTICK1,
	OBJ_SKSTICK2,
	OBJ_SKSTICK3,
	OBJ_SKSTICK4,
	OBJ_SKSTICK5,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_SWITCHSKL,
	OBJ_NULL,
	OBJ_TRAPL,
	OBJ_TRAPR,
	OBJ_TORTURE1,
	OBJ_TORTURE2,
	OBJ_TORTURE3,
	OBJ_TORTURE4,
	OBJ_TORTURE5,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NUDEW2R,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_TNUDEM1,
	OBJ_TNUDEM2,
	OBJ_TNUDEM3,
	OBJ_TNUDEM4,
	OBJ_TNUDEW1,
	OBJ_TNUDEW2,
	OBJ_TNUDEW3,
	OBJ_CHEST1,
	OBJ_CHEST1,
	OBJ_CHEST1,
	OBJ_CHEST2,
	OBJ_CHEST2,
	OBJ_CHEST2,
	OBJ_CHEST3,
	OBJ_CHEST3,
	OBJ_CHEST3,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_PEDESTAL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_ALTBOY,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_WARARMOR,
	OBJ_WARWEAP,
	OBJ_TORCHR2,
	OBJ_TORCHL2,
	OBJ_MUSHPATCH,
	OBJ_STAND,
	OBJ_TORCHL,
	OBJ_TORCHR,
	OBJ_FLAMELVR,
	OBJ_SARC,
	OBJ_BARREL,
	OBJ_BARRELEX,
	OBJ_BOOKSHELF,
	OBJ_BOOKCASEL,
	OBJ_BOOKCASER,
	OBJ_ARMORSTANDN,
	OBJ_WEAPONRACKN,
	OBJ_BLOODFTN,
	OBJ_PURIFYINGFTN,
	OBJ_SHRINEL,
	OBJ_SHRINER,
	OBJ_GOATSHRINE,
	OBJ_MURKYFTN,
	OBJ_TEARFTN,
	OBJ_DECAP,
	OBJ_TCHEST1,
	OBJ_TCHEST2,
	OBJ_TCHEST3,
	OBJ_LAZSTAND,
	OBJ_BOOKSTAND,
	OBJ_BOOKSHELFR,
	OBJ_POD,
	OBJ_PODEX,
	OBJ_URN,
	OBJ_URNEX,
	OBJ_L5BOOKS,
	OBJ_L5CANDLE,
	OBJ_L5LEVER,
	OBJ_L5SARC,
};

namespace {
constexpr auto Animated = ObjectDataFlags::Animated;
constexpr auto Solid = ObjectDataFlags::Solid;
constexpr auto MissilesPassThrough = ObjectDataFlags::MissilesPassThrough;
constexpr auto Light = ObjectDataFlags::Light;
constexpr auto Trap = ObjectDataFlags::Trap;
constexpr auto Breakable = ObjectDataFlags::Breakable;
} // namespace

/** Contains the data related to each object ID. */
const ObjectData AllObjects[109] = {
	// clang-format off
// _object_id          ofindex,         minlvl,  maxlvl, olvltype,        otheme,                  oquest,     flags,                                            animDelay,  animLen,  animWidth,  selFlag
/*OBJ_L1LIGHT*/      { OFILE_L1BRAZ,         0,       0, DTYPE_CATHEDRAL, THEME_NONE,              Q_INVALID,  Animated | Solid | MissilesPassThrough,                   1,       26,         64,        0 },
/*OBJ_L1LDOOR*/      { OFILE_L1DOORS,        0,       0, DTYPE_CATHEDRAL, THEME_NONE,              Q_INVALID,  Light | Trap,                                             1,        0,         64,        3 },
/*OBJ_L1RDOOR*/      { OFILE_L1DOORS,        0,       0, DTYPE_CATHEDRAL, THEME_NONE,              Q_INVALID,  Light | Trap,                                             2,        0,         64,        3 },
/*OBJ_SKFIRE*/       { OFILE_SKULFIRE,       0,       0, DTYPE_NONE,      THEME_SKELROOM,          Q_INVALID,  Animated | Solid | MissilesPassThrough,                   2,       11,         96,        0 },
/*OBJ_LEVER*/        { OFILE_LEVER,          0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light | Trap,               1,        1,         96,        1 },
/*OBJ_CHEST1*/       { OFILE_CHEST1,         1,      24, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light | Trap,               1,        0,         96,        1 },
/*OBJ_CHEST2*/       { OFILE_CHEST2,         1,      24, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light | Trap,               1,        0,         96,        1 },
/*OBJ_CHEST3*/       { OFILE_CHEST3,         1,      24, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light | Trap,               1,        0,         96,        1 },
/*OBJ_CANDLE1*/      { OFILE_L1BRAZ,         0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  {},                                                       0,        0,          0,        0 },
/*OBJ_CANDLE2*/      { OFILE_CANDLE2,        0,       0, DTYPE_NONE,      THEME_SHRINE,            Q_PWATER,   Animated | Solid | MissilesPassThrough | Light,           2,        4,         96,        0 },
/*OBJ_CANDLEO*/      { OFILE_L1BRAZ,         0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  {},                                                       0,        0,          0,        0 },
/*OBJ_BANNERL*/      { OFILE_BANNER,         0,       0, DTYPE_NONE,      THEME_SKELROOM,          Q_INVALID,  Solid | MissilesPassThrough | Light,                      2,        0,         96,        0 },
/*OBJ_BANNERM*/      { OFILE_BANNER,         0,       0, DTYPE_NONE,      THEME_SKELROOM,          Q_INVALID,  Solid | MissilesPassThrough | Light,                      1,        0,         96,        0 },
/*OBJ_BANNERR*/      { OFILE_BANNER,         0,       0, DTYPE_NONE,      THEME_SKELROOM,          Q_INVALID,  Solid | MissilesPassThrough | Light,                      3,        0,         96,        0 },
/*OBJ_SKPILE*/       { OFILE_SKULPILE,       0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light,                      1,        1,         96,        0 },
/*OBJ_SKSTICK1*/     { OFILE_L1BRAZ,         0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  {},                                                       0,        0,          0,        0 },
/*OBJ_SKSTICK2*/     { OFILE_L1BRAZ,         0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  {},                                                       0,        0,          0,        0 },
/*OBJ_SKSTICK3*/     { OFILE_L1BRAZ,         0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  {},                                                       0,        0,          0,        0 },
/*OBJ_SKSTICK4*/     { OFILE_L1BRAZ,         0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  {},                                                       0,        0,          0,        0 },
/*OBJ_SKSTICK5*/     { OFILE_L1BRAZ,         0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  {},                                                       0,        0,          0,        0 },
/*OBJ_CRUX1*/        { OFILE_CRUXSK1,        0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | Light | Breakable,                                1,       15,         96,        3 },
/*OBJ_CRUX2*/        { OFILE_CRUXSK2,        0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | Light | Breakable,                                1,       15,         96,        3 },
/*OBJ_CRUX3*/        { OFILE_CRUXSK3,        0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | Light | Breakable,                                1,       15,         96,        3 },
/*OBJ_STAND*/        { OFILE_ROCKSTAN,       5,       5, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light,                      1,        0,         96,        0 },
/*OBJ_ANGEL*/        { OFILE_ANGEL,          0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | Light,                                            1,        0,         96,        0 },
/*OBJ_BOOK2L*/       { OFILE_BOOK2,          0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light,                      1,        0,         96,        3 },
/*OBJ_BCROSS*/       { OFILE_BURNCROS,       0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Animated | Solid,                                         0,       10,        160,        0 },
/*OBJ_NUDEW2R*/      { OFILE_NUDE2,          0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Animated | Solid | Light,                                 3,        6,        128,        0 },
/*OBJ_SWITCHSKL*/    { OFILE_SWITCH4,       16,      16, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light | Trap,               1,        0,         96,        1 },
/*OBJ_TNUDEM1*/      { OFILE_TNUDEM,        13,      15, DTYPE_NONE,      THEME_NONE,              Q_BUTCHER,  Solid | Light,                                            1,        0,        128,        0 },
/*OBJ_TNUDEM2*/      { OFILE_TNUDEM,        13,      15, DTYPE_NONE,      THEME_TORTURE,           Q_BUTCHER,  Solid | Light,                                            2,        0,        128,        0 },
/*OBJ_TNUDEM3*/      { OFILE_TNUDEM,        13,      15, DTYPE_NONE,      THEME_TORTURE,           Q_BUTCHER,  Solid | Light,                                            3,        0,        128,        0 },
/*OBJ_TNUDEM4*/      { OFILE_TNUDEM,        13,      15, DTYPE_NONE,      THEME_TORTURE,           Q_BUTCHER,  Solid | Light,                                            4,        0,        128,        0 },
/*OBJ_TNUDEW1*/      { OFILE_TNUDEW,        13,      15, DTYPE_NONE,      THEME_TORTURE,           Q_BUTCHER,  Solid | Light,                                            1,        0,        128,        0 },
/*OBJ_TNUDEW2*/      { OFILE_TNUDEW,        13,      15, DTYPE_NONE,      THEME_TORTURE,           Q_BUTCHER,  Solid | Light,                                            2,        0,        128,        0 },
/*OBJ_TNUDEW3*/      { OFILE_TNUDEW,        13,      15, DTYPE_NONE,      THEME_TORTURE,           Q_BUTCHER,  Solid | Light,                                            3,        0,        128,        0 },
/*OBJ_TORTURE1*/     { OFILE_TSOUL,         13,      15, DTYPE_NONE,      THEME_NONE,              Q_BUTCHER,  MissilesPassThrough | Light,                              1,        0,        128,        0 },
/*OBJ_TORTURE2*/     { OFILE_TSOUL,         13,      15, DTYPE_NONE,      THEME_NONE,              Q_BUTCHER,  MissilesPassThrough | Light,                              2,        0,        128,        0 },
/*OBJ_TORTURE3*/     { OFILE_TSOUL,         13,      15, DTYPE_NONE,      THEME_NONE,              Q_BUTCHER,  MissilesPassThrough | Light,                              3,        0,        128,        0 },
/*OBJ_TORTURE4*/     { OFILE_TSOUL,         13,      15, DTYPE_NONE,      THEME_NONE,              Q_BUTCHER,  MissilesPassThrough | Light,                              4,        0,        128,        0 },
/*OBJ_TORTURE5*/     { OFILE_TSOUL,         13,      15, DTYPE_NONE,      THEME_NONE,              Q_BUTCHER,  MissilesPassThrough | Light,                              5,        0,        128,        0 },
/*OBJ_BOOK2R*/       { OFILE_BOOK2,          6,       6, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light,                      4,        0,         96,        3 },
/*OBJ_L2LDOOR*/      { OFILE_L2DOORS,        0,       0, DTYPE_CATACOMBS, THEME_NONE,              Q_INVALID,  Light | Trap,                                             1,        0,         64,        3 },
/*OBJ_L2RDOOR*/      { OFILE_L2DOORS,        0,       0, DTYPE_CATACOMBS, THEME_NONE,              Q_INVALID,  Light | Trap,                                             2,        0,         64,        3 },
/*OBJ_TORCHL*/       { OFILE_WTORCH4,        5,       8, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Animated | MissilesPassThrough,                           1,        9,         96,        0 },
/*OBJ_TORCHR*/       { OFILE_WTORCH3,        5,       8, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Animated | MissilesPassThrough,                           1,        9,         96,        0 },
/*OBJ_TORCHL2*/      { OFILE_WTORCH1,        5,       8, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Animated | MissilesPassThrough,                           1,        9,         96,        0 },
/*OBJ_TORCHR2*/      { OFILE_WTORCH2,        5,       8, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Animated | MissilesPassThrough,                           1,        9,         96,        0 },
/*OBJ_SARC*/         { OFILE_SARC,           1,       4, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light | Trap,               1,        5,        128,        3 },
/*OBJ_FLAMEHOLE*/    { OFILE_FLAME1,         0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  MissilesPassThrough | Light,                              1,       20,         96,        0 },
/*OBJ_FLAMELVR*/     { OFILE_LEVER,          0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light | Trap,               1,        2,         96,        1 },
/*OBJ_WATER*/        { OFILE_MINIWATR,       0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Animated | Solid | Light,                                 1,       10,         64,        0 },
/*OBJ_BOOKLVR*/      { OFILE_BOOK1,          0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light,                      1,        0,         96,        3 },
/*OBJ_TRAPL*/        { OFILE_TRAPHOLE,       1,      24, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  MissilesPassThrough | Light,                              1,        0,         64,        0 },
/*OBJ_TRAPR*/        { OFILE_TRAPHOLE,       1,      24, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  MissilesPassThrough | Light,                              2,        0,         64,        0 },
/*OBJ_BOOKSHELF*/    { OFILE_BCASE,          0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | Light,                                            1,        0,         96,        0 },
/*OBJ_WEAPRACK*/     { OFILE_WEAPSTND,       0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | Light,                                            1,        0,         96,        0 },
/*OBJ_BARREL*/       { OFILE_BARREL,         1,      15, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light | Breakable,          1,        9,         96,        3 },
/*OBJ_BARRELEX*/     { OFILE_BARRELEX,       1,      15, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light | Breakable,          1,       10,         96,        3 },
/*OBJ_SHRINEL*/      { OFILE_LSHRINEG,       0,       0, DTYPE_NONE,      THEME_SHRINE,            Q_INVALID,  Light,                                                    1,       11,        128,        3 },
/*OBJ_SHRINER*/      { OFILE_RSHRINEG,       0,       0, DTYPE_NONE,      THEME_SHRINE,            Q_INVALID,  Light,                                                    1,       11,        128,        3 },
/*OBJ_SKELBOOK*/     { OFILE_BOOK2,          0,       0, DTYPE_NONE,      THEME_SKELROOM,          Q_INVALID,  Solid | MissilesPassThrough | Light,                      4,        0,         96,        3 },
/*OBJ_BOOKCASEL*/    { OFILE_BCASE,          0,       0, DTYPE_NONE,      THEME_LIBRARY,           Q_INVALID,  Solid | Light,                                            3,        0,         96,        3 },
/*OBJ_BOOKCASER*/    { OFILE_BCASE,          0,       0, DTYPE_NONE,      THEME_LIBRARY,           Q_INVALID,  Solid | Light,                                            4,        0,         96,        3 },
/*OBJ_BOOKSTAND*/    { OFILE_BOOK2,          0,       0, DTYPE_NONE,      THEME_LIBRARY,           Q_INVALID,  Solid | MissilesPassThrough | Light,                      1,        0,         96,        3 },
/*OBJ_BOOKCANDLE*/   { OFILE_CANDLE2,        0,       0, DTYPE_NONE,      THEME_LIBRARY,           Q_INVALID,  Animated | Solid | MissilesPassThrough | Light,           2,        4,         96,        0 },
/*OBJ_BLOODFTN*/     { OFILE_BLOODFNT,       0,       0, DTYPE_NONE,      THEME_BLOODFOUNTAIN,     Q_INVALID,  Animated | Solid | MissilesPassThrough | Light,           2,       10,         96,        3 },
/*OBJ_DECAP*/        { OFILE_DECAP,         13,      15, DTYPE_NONE,      THEME_DECAPITATED,       Q_INVALID,  Solid | MissilesPassThrough | Light,                      1,        0,         96,        1 },
/*OBJ_TCHEST1*/      { OFILE_CHEST1,         1,      24, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light | Trap,               1,        0,         96,        1 },
/*OBJ_TCHEST2*/      { OFILE_CHEST2,         1,      24, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light | Trap,               1,        0,         96,        1 },
/*OBJ_TCHEST3*/      { OFILE_CHEST3,         1,      24, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light | Trap,               1,        0,         96,        1 },
/*OBJ_BLINDBOOK*/    { OFILE_BOOK1,          0,       0, DTYPE_NONE,      THEME_NONE,              Q_BLIND,    Solid | MissilesPassThrough | Light,                      1,        0,         96,        3 },
/*OBJ_BLOODBOOK*/    { OFILE_BOOK1,          0,       0, DTYPE_NONE,      THEME_NONE,              Q_BLOOD,    Solid | MissilesPassThrough | Light,                      4,        0,         96,        3 },
/*OBJ_PEDESTAL*/     { OFILE_PEDISTL,        0,       0, DTYPE_NONE,      THEME_NONE,              Q_BLOOD,    Solid | MissilesPassThrough | Light,                      1,        0,         96,        3 },
/*OBJ_L3LDOOR*/      { OFILE_L3DOORS,        0,       0, DTYPE_CAVES,     THEME_NONE,              Q_INVALID,  Light | Trap,                                             1,        0,         64,        3 },
/*OBJ_L3RDOOR*/      { OFILE_L3DOORS,        0,       0, DTYPE_CAVES,     THEME_NONE,              Q_INVALID,  Light | Trap,                                             2,        0,         64,        3 },
/*OBJ_PURIFYINGFTN*/ { OFILE_PFOUNTN,        0,       0, DTYPE_NONE,      THEME_PURIFYINGFOUNTAIN, Q_INVALID,  Animated | Solid | MissilesPassThrough | Light,           2,       10,        128,        3 },
/*OBJ_ARMORSTAND*/   { OFILE_ARMSTAND,       0,       0, DTYPE_NONE,      THEME_ARMORSTAND,        Q_INVALID,  Solid | Light,                                            1,        0,         96,        3 },
/*OBJ_ARMORSTANDN*/  { OFILE_ARMSTAND,       0,       0, DTYPE_NONE,      THEME_ARMORSTAND,        Q_INVALID,  Solid | Light,                                            2,        0,         96,        0 },
/*OBJ_GOATSHRINE*/   { OFILE_GOATSHRN,       0,       0, DTYPE_NONE,      THEME_GOATSHRINE,        Q_INVALID,  Animated | Solid | MissilesPassThrough | Light,           2,       10,         96,        3 },
/*OBJ_CAULDRON*/     { OFILE_CAULDREN,      13,      15, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | Light,                                            1,        0,         96,        3 },
/*OBJ_MURKYFTN*/     { OFILE_MFOUNTN,        0,       0, DTYPE_NONE,      THEME_MURKYFOUNTAIN,     Q_INVALID,  Animated | Solid | MissilesPassThrough | Light,           2,       10,        128,        3 },
/*OBJ_TEARFTN*/      { OFILE_TFOUNTN,        0,       0, DTYPE_NONE,      THEME_TEARFOUNTAIN,      Q_INVALID,  Animated | Solid | MissilesPassThrough | Light,           2,        4,        128,        3 },
/*OBJ_ALTBOY*/       { OFILE_ALTBOY,         0,       0, DTYPE_NONE,      THEME_NONE,              Q_BETRAYER, Solid | MissilesPassThrough | Light,                      1,        0,        128,        0 },
/*OBJ_MCIRCLE1*/     { OFILE_MCIRL,          0,       0, DTYPE_NONE,      THEME_NONE,              Q_BETRAYER, MissilesPassThrough | Light,                              1,        0,         96,        0 },
/*OBJ_MCIRCLE2*/     { OFILE_MCIRL,          0,       0, DTYPE_NONE,      THEME_NONE,              Q_BETRAYER, MissilesPassThrough | Light,                              1,        0,         96,        0 },
/*OBJ_STORYBOOK*/    { OFILE_BKSLBRNT,       0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light,                      1,        0,         96,        3 },
/*OBJ_STORYCANDLE*/  { OFILE_CANDLE2,        0,       0, DTYPE_NONE,      THEME_NONE,              Q_BETRAYER, Animated | Solid | MissilesPassThrough | Light,           2,        4,         96,        0 },
/*OBJ_STEELTOME*/    { OFILE_BOOK1,          0,       0, DTYPE_NONE,      THEME_NONE,              Q_WARLORD,  Solid | MissilesPassThrough | Light,                      4,        0,         96,        3 },
/*OBJ_WARARMOR*/     { OFILE_ARMSTAND,       0,       0, DTYPE_NONE,      THEME_NONE,              Q_WARLORD,  Solid | Light,                                            1,        0,         96,        3 },
/*OBJ_WARWEAP*/      { OFILE_WEAPSTND,       0,       0, DTYPE_NONE,      THEME_NONE,              Q_WARLORD,  Solid | Light,                                            1,        0,         96,        3 },
/*OBJ_TBCROSS*/      { OFILE_BURNCROS,       0,       0, DTYPE_NONE,      THEME_BRNCROSS,          Q_INVALID,  Animated | Solid,                                         0,       10,        160,        0 },
/*OBJ_WEAPONRACK*/   { OFILE_WEAPSTND,       0,       0, DTYPE_NONE,      THEME_WEAPONRACK,        Q_INVALID,  Solid | Light,                                            1,        0,         96,        3 },
/*OBJ_WEAPONRACKN*/  { OFILE_WEAPSTND,       0,       0, DTYPE_NONE,      THEME_WEAPONRACK,        Q_INVALID,  Solid | Light,                                            2,        0,         96,        0 },
/*OBJ_MUSHPATCH*/    { OFILE_MUSHPTCH,       0,       0, DTYPE_NONE,      THEME_NONE,              Q_MUSHROOM, Solid | MissilesPassThrough | Light | Trap,               1,        0,         96,        3 },
/*OBJ_LAZSTAND*/     { OFILE_LZSTAND,        0,       0, DTYPE_NONE,      THEME_NONE,              Q_BETRAYER, Solid | Light,                                            1,        0,        128,        3 },
/*OBJ_SLAINHERO*/    { OFILE_DECAP,          9,       9, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light,                      2,        0,         96,        1 },
/*OBJ_SIGNCHEST*/    { OFILE_CHEST3,         0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light | Trap,               1,        0,         96,        1 },
/*OBJ_BOOKSHELFR*/   { OFILE_BCASE,          0,       0, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | Light,                                            2,        0,         96,        0 },
/*OBJ_POD*/          { OFILE_POD,           17,      20, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light | Breakable,          1,        9,         96,        3 },
/*OBJ_PODEX*/        { OFILE_PODEX,         17,      20, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light | Breakable,          1,       10,         96,        3 },
/*OBJ_URN*/          { OFILE_URN,           21,      24, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light | Breakable,          1,        9,         96,        3 },
/*OBJ_URNEX*/        { OFILE_URNEX,         21,      24, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light | Breakable,          1,       10,         96,        3 },
/*OBJ_L5BOOKS*/      { OFILE_L5BOOKS,       21,      24, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light,                      1,        0,         96,        3 },
/*OBJ_L5CANDLE*/     { OFILE_L5CANDLE,      21,      23, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Animated | Solid | MissilesPassThrough | Light,           2,        4,         96,        0 },
/*OBJ_L5LDOOR*/      { OFILE_L5DOORS,        0,       0, DTYPE_CRYPT,     THEME_NONE,              Q_INVALID,  Light | Trap,                                             1,        0,         64,        3 },
/*OBJ_L5RDOOR*/      { OFILE_L5DOORS,        0,       0, DTYPE_CRYPT,     THEME_NONE,              Q_INVALID,  Light | Trap,                                             2,        0,         64,        3 },
/*OBJ_L5LEVER*/      { OFILE_L5LEVER,       24,      24, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light | Trap,               1,        1,         96,        1 },
/*OBJ_L5SARC*/       { OFILE_L5SARC,        21,      24, DTYPE_NONE,      THEME_NONE,              Q_INVALID,  Solid | MissilesPassThrough | Light | Trap,               1,        5,        128,        3 },
	// clang-format on
};
/** Maps from object_graphic_id to object CEL name. */
const char *const ObjMasterLoadList[] = {
	"l1braz",
	"l1doors",
	"lever",
	"chest1",
	"chest2",
	"banner",
	"skulpile",
	"skulfire",
	"skulstik",
	"cruxsk1",
	"cruxsk2",
	"cruxsk3",
	"book1",
	"book2",
	"rockstan",
	"angel",
	"chest3",
	"burncros",
	"candle2",
	"nude2",
	"switch4",
	"tnudem",
	"tnudew",
	"tsoul",
	"l2doors",
	"wtorch4",
	"wtorch3",
	"sarc",
	"flame1",
	"prsrplt1",
	"traphole",
	"miniwatr",
	"wtorch2",
	"wtorch1",
	"bcase",
	"bshelf",
	"weapstnd",
	"barrel",
	"barrelex",
	"lshrineg",
	"rshrineg",
	"bloodfnt",
	"decap",
	"pedistl",
	"l3doors",
	"pfountn",
	"armstand",
	"goatshrn",
	"cauldren",
	"mfountn",
	"tfountn",
	"altboy",
	"mcirl",
	"bkslbrnt",
	"mushptch",
	"lzstand",
	"l6pod1",
	"l6pod2",
	"l5door",
	"l5lever",
	"l5light",
	"l5sarco",
	"urn",
	"urnexpld",
	"l5books",
};

} // namespace devilution
