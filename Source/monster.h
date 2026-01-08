/**
 * @file monster.h
 *
 * Interface of monster functionality, AI, actions, spawning, loading, etc.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <functional>
#include <string>

#include <expected.hpp>
#include <function_ref.hpp>

#include "engine/actor_position.hpp"
#include "engine/animationinfo.h"
#include "engine/clx_sprite.hpp"
#include "engine/point.hpp"
#include "engine/sound.h"
#include "engine/world_tile.hpp"
#include "game_mode.hpp"
#include "levels/dun_tile.hpp"
#include "tables/misdat.h"
#include "tables/monstdat.h"
#include "tables/spelldat.h"
#include "tables/textdat.h"
#include "utils/language.h"

namespace devilution {

struct Missile;
struct Player;

constexpr size_t MaxMonsters = 200;
constexpr size_t MaxLvlMTypes = 24;

enum monster_flag : uint16_t {
	// clang-format off
	MFLAG_HIDDEN          = 1 << 0,
	MFLAG_LOCK_ANIMATION  = 1 << 1,
	MFLAG_ALLOW_SPECIAL   = 1 << 2,
	MFLAG_TARGETS_MONSTER = 1 << 4,
	MFLAG_GOLEM           = 1 << 5,
	MFLAG_QUEST_COMPLETE  = 1 << 6,
	MFLAG_KNOCKBACK       = 1 << 7,
	MFLAG_SEARCH          = 1 << 8,
	MFLAG_CAN_OPEN_DOOR   = 1 << 9,
	MFLAG_NO_ENEMY        = 1 << 10,
	MFLAG_BERSERK         = 1 << 11,
	MFLAG_NOLIFESTEAL     = 1 << 12,
	// clang-format on
};

/** Indexes from UniqueMonstersData array for special unique monsters (usually quest related) */
enum class UniqueMonsterType : uint8_t {
	Garbud,
	SkeletonKing,
	Zhar,
	SnotSpill,
	Lazarus,
	RedVex,
	BlackJade,
	Lachdan,
	WarlordOfBlood,
	Butcher,
	HorkDemon,
	Defiler,
	NaKrul,
	None = static_cast<uint8_t>(-1),
};

enum class MonsterMode : uint8_t {
	Stand,
	/** Movement towards N, NW, or NE */
	MoveNorthwards,
	/** Movement towards S, SW, or SE */
	MoveSouthwards,
	/** Movement towards W or E */
	MoveSideways,
	MeleeAttack,
	HitRecovery,
	Death,
	SpecialMeleeAttack,
	FadeIn,
	FadeOut,
	RangedAttack,
	SpecialStand,
	SpecialRangedAttack,
	Delay,
	Charge,
	Petrified,
	Heal,
	Talk,
};

inline bool IsMonsterModeMove(MonsterMode mode)
{
	switch (mode) {
	case MonsterMode::MoveNorthwards:
	case MonsterMode::MoveSouthwards:
	case MonsterMode::MoveSideways:
		return true;
	default:
		return false;
	}
}

enum class MonsterGraphic : uint8_t {
	Stand,
	Walk,
	Attack,
	GotHit,
	Death,
	Special,
};

enum class MonsterGoal : uint8_t {
	None,
	Normal,
	Retreat,
	Healing,
	Move,
	Attack,
	Inquiring,
	Talking,
};

enum placeflag : uint8_t {
	// clang-format off
	PLACE_SCATTER = 1 << 0,
	PLACE_SPECIAL = 1 << 1,
	PLACE_UNIQUE  = 1 << 2,
	// clang-format on
};

/**
 * @brief Defines the relation of the monster to a monster pack.
 *        If value is different from Individual Monster, the leader must also be set
 */
enum class LeaderRelation : uint8_t {
	None,
	/**
	 * @brief Minion that sticks to the leader
	 */
	Leashed,
	/**
	 * @brief Minion that was separated from the leader and acts individually until it reaches the leader again
	 */
	Separated,
};

struct AnimStruct {
	/**
	 * @brief Sprite lists for each of the 8 directions.
	 */
	OptionalClxSpriteListOrSheet sprites;

	[[nodiscard]] OptionalClxSpriteList spritesForDirection(Direction direction) const
	{
		if (!sprites)
			return std::nullopt;
		return sprites->isSheet() ? (*sprites).sheet()[static_cast<size_t>(direction)] : (*sprites).list();
	}

	uint16_t width;
	int8_t frames;
	int8_t rate;
};

enum class MonsterSound : uint8_t {
	Attack,
	Hit,
	Death,
	Special
};

struct MonsterSpritesData {
	static constexpr size_t MaxAnims = 6;
	std::unique_ptr<std::byte[]> data;
	std::array<uint32_t, MaxAnims + 1> offsets;
};

struct CMonster {
	std::unique_ptr<std::byte[]> animData;
	AnimStruct anims[6];
	std::unique_ptr<TSnd> sounds[4][2];

	_monster_id type;
	/** placeflag enum as a flags*/
	uint8_t placeFlags;
	int8_t corpseId = 0;

	const MonsterData &data() const
	{
		return MonstersData[type];
	}

	/**
	 * @brief Returns AnimStruct for specified graphic
	 */
	[[nodiscard]] const AnimStruct &getAnimData(MonsterGraphic graphic) const
	{
		return anims[static_cast<int>(graphic)];
	}
};

extern CMonster LevelMonsterTypes[MaxLvlMTypes];

struct Monster { // note: missing field _mAFNum
	std::unique_ptr<uint8_t[]> uniqueMonsterTRN;
	/**
	 * @brief Contains information for current animation
	 */
	AnimationInfo animInfo;
	int maxHitPoints;
	int hitPoints;
	uint32_t flags;
	/** Seed used to determine item drops on death */
	uint32_t rndItemSeed;
	/** Seed used to determine AI behaviour/sync sounds in multiplayer games? */
	uint32_t aiSeed;
	uint16_t golemToHit;
	uint16_t resistance;
	_speech_id talkMsg;

	/** @brief Specifies monster's behaviour regarding moving and changing goals. */
	int16_t goalVar1;

	/**
	 * @brief Specifies turning direction for @p RoundWalk in most cases.
	 * Used in custom way by @p FallenAi, @p SnakeAi, @p M_FallenFear and @p FallenAi.
	 */
	int8_t goalVar2;

	/**
	 * @brief Controls monster's behaviour regarding special actions.
	 * Used only by @p ScavengerAi, @p MegaAi and @p GolemAi.
	 */
	int8_t goalVar3;

	int16_t var1;
	int16_t var2;
	int8_t var3;

	ActorPosition position;

	/** Specifies current goal of the monster */
	MonsterGoal goal;

	/** Usually corresponds to the enemy's future position */
	WorldTilePosition enemyPosition;
	uint8_t levelType;
	MonsterMode mode;
	uint8_t pathCount;
	/** Direction faced by monster (direction enum) */
	Direction direction;
	/** The current target of the monster. An index in to either the player or monster array based on the _meflag value. */
	uint8_t enemy;
	bool isInvalid;
	MonsterAIID ai;
	/**
	 * @brief Specifies monster's behaviour across various actions.
	 * Generally, when monster thinks it decides what to do based on this value, among other things.
	 * Higher values should result in more aggressive behaviour (e.g. some monsters use this to calculate the @p AiDelay).
	 */
	uint8_t intelligence;
	/** Stores information for how many ticks the monster will remain active */
	uint8_t activeForTicks;
	UniqueMonsterType uniqueType;
	uint8_t uniqTrans;
	int8_t corpseId;
	int8_t whoHit;
	uint8_t minDamage;
	uint8_t maxDamage;
	uint8_t minDamageSpecial;
	uint8_t maxDamageSpecial;
	uint8_t armorClass;
	uint8_t leader;
	LeaderRelation leaderRelation;
	uint8_t packSize;
	int8_t lightId;

	static constexpr uint8_t NoLeader = -1;

	/**
	 * @brief Sets the current cell sprite to match the desired desiredDirection and animation sequence
	 * @param graphic Animation sequence of interest
	 * @param desiredDirection Desired desiredDirection the monster should be visually facing
	 */
	void changeAnimationData(MonsterGraphic graphic, Direction desiredDirection)
	{
		const AnimStruct &animationData = type().getAnimData(graphic);

		// Passing the frames and rate properties here is only relevant when initialising a monster, but doesn't cause any harm when switching animations.
		this->animInfo.changeAnimationData(animationData.spritesForDirection(desiredDirection), animationData.frames, animationData.rate);
	}

	/**
	 * @brief Sets the current cell sprite to match the desired animation sequence using the direction the monster is currently facing
	 * @param graphic Animation sequence of interest
	 */
	void changeAnimationData(MonsterGraphic graphic)
	{
		this->changeAnimationData(graphic, this->direction);
	}

	/**
	 * @brief Check if the correct stand Animation is loaded. This is needed if direction is changed (monster stands and looks at the player).
	 * @param dir direction of the monster
	 */
	void checkStandAnimationIsLoaded(Direction dir);

	/**
	 * @brief Sets mode to MonsterMode::Petrified
	 */
	void petrify();

	const CMonster &type() const
	{
		return LevelMonsterTypes[levelType];
	}

	const MonsterData &data() const
	{
		return type().data();
	}

	/**
	 * @brief Returns monster's name
	 * Internally it returns a name stored in global array of monsters' data.
	 * @return Monster's name
	 */
	std::string_view name() const
	{
		if (uniqueType != UniqueMonsterType::None)
			return pgettext("monster", UniqueMonstersData[static_cast<size_t>(uniqueType)].mName);

		return pgettext("monster", data().name);
	}

	/**
	 * @brief Calculates monster's experience points.
	 * Fetches base exp value from @p MonstersData array.
	 * @param difficulty - difficulty on which calculation is performed
	 * @return Monster's experience points, including bonuses from difficulty and monster being unique
	 */
	unsigned int exp(_difficulty difficulty) const
	{
		unsigned int monsterExp = data().exp;

		if (difficulty == DIFF_NIGHTMARE) {
			monsterExp = 2 * (monsterExp + 1000);
		} else if (difficulty == DIFF_HELL) {
			monsterExp = 4 * (monsterExp + 1000);
		}

		if (isUnique()) {
			monsterExp *= 2;
		}

		return monsterExp;
	}

	/**
	 * @brief Calculates monster's chance to hit with normal attack.
	 * Fetches base value from @p MonstersData array or @p UniqueMonstersData.
	 * @param difficulty - difficulty on which calculation is performed
	 * @return Monster's chance to hit with normal attack, including bonuses from difficulty and monster being unique
	 */
	unsigned int toHit(_difficulty difficulty) const;

	/**
	 * @brief Calculates monster's chance to hit with special attack.
	 * Fetches base value from @p MonstersData array or @p UniqueMonstersData.
	 * @param difficulty - difficulty on which calculation is performed
	 * @return Monster's chance to hit with special attack, including bonuses from difficulty and monster being unique
	 */
	unsigned int toHitSpecial(_difficulty difficulty) const;

	/**
	 * @brief Calculates monster's level.
	 * Fetches base level value from @p MonstersData array or @p UniqueMonstersData.
	 * @param difficulty - difficulty on which calculation is performed
	 * @return Monster's level, including bonuses from difficulty and monster being unique
	 */
	unsigned int level(_difficulty difficulty) const
	{
		unsigned int baseLevel = data().level;
		if (isUnique()) {
			baseLevel = UniqueMonstersData[static_cast<size_t>(uniqueType)].mlevel;
			if (baseLevel != 0) {
				baseLevel *= 2;
			} else {
				baseLevel = data().level + 5;
			}
		}

		if (difficulty == DIFF_NIGHTMARE) {
			baseLevel += 15;
		} else if (difficulty == DIFF_HELL) {
			baseLevel += 30;
		}

		return baseLevel;
	}

	/**
	 * @brief Returns the network identifier for this monster
	 *
	 * This is currently the index into the Monsters array, but may change in the future.
	 */
	[[nodiscard]] size_t getId() const;

	[[nodiscard]] Monster *getLeader() const;
	void setLeader(const Monster *leader);

	[[nodiscard]] bool hasLeashedMinions() const
	{
		return isUnique() && UniqueMonstersData[static_cast<size_t>(uniqueType)].monsterPack == UniqueMonsterPack::Leashed;
	}

	/**
	 * @brief Calculates the distance in tiles between this monster and its current target
	 *
	 * The distance is not calculated as the euclidean distance, but rather as
	 * the longest number of tiles in the coordinate system.
	 *
	 * @return The distance in tiles
	 */
	[[nodiscard]] unsigned distanceToEnemy() const;

	/**
	 * @brief Is the monster currently walking?
	 */
	[[nodiscard]] bool isWalking() const;
	[[nodiscard]] bool isImmune(MissileID mitype, DamageType missileElement) const;
	[[nodiscard]] bool isResistant(MissileID mitype, DamageType missileElement) const;

	/**
	 * Is this a player's golem?
	 */
	[[nodiscard]] bool isPlayerMinion() const;

	bool isPossibleToHit() const;
	void tag(const Player &tagger);

	[[nodiscard]] bool isUnique() const
	{
		return uniqueType != UniqueMonsterType::None;
	}

	bool tryLiftGargoyle();

	/**
	 * @brief Gets the visual/shown monster mode.
	 *
	 * When a monster is petrified it's monster mode is changed to MonsterMode::Petrified.
	 * But for graphics and rendering we show the old/real mode.
	 */
	[[nodiscard]] MonsterMode getVisualMonsterMode() const;

	[[nodiscard]] Displacement getRenderingOffset(const ClxSprite sprite) const
	{
		Displacement offset = { -CalculateSpriteTileCenterX(sprite.width()), 0 };
		if (isWalking())
			offset += GetOffsetForWalking(animInfo, direction);
		return offset;
	}

	/**
	 * @brief Sets a tile/dMonster to be occupied by the monster
	 * @param position tile to update
	 * @param isMoving specifies whether the monster is moving or not (true/moving results in a negative index in dMonster)
	 */
	void occupyTile(Point tile, bool isMoving) const;

	bool hasNoLife() const
	{
		return hitPoints >> 6 <= 0;
	}
};

extern size_t LevelMonsterTypeCount;
extern Monster Monsters[MaxMonsters];
extern unsigned ActiveMonsters[MaxMonsters];
extern size_t ActiveMonsterCount;
extern int MonsterKillCounts[NUM_MAX_MTYPES];
extern bool sgbSaveSoundOn;

tl::expected<void, std::string> PrepareUniqueMonst(Monster &monster, UniqueMonsterType monsterType, size_t miniontype, int bosspacksize, const UniqueMonsterData &uniqueMonsterData);
void InitLevelMonsters();
tl::expected<void, std::string> GetLevelMTypes();
tl::expected<size_t, std::string> AddMonsterType(_monster_id type, placeflag placeflag);
inline tl::expected<size_t, std::string> AddMonsterType(UniqueMonsterType uniqueType, placeflag placeflag)
{
	return AddMonsterType(UniqueMonstersData[static_cast<size_t>(uniqueType)].mtype, placeflag);
}
tl::expected<void, std::string> InitMonsterSND(CMonster &monsterType);
tl::expected<void, std::string> InitMonsterGFX(CMonster &monsterType, MonsterSpritesData &&spritesData = {});
tl::expected<void, std::string> InitAllMonsterGFX();
void WeakenNaKrul();
void InitGolems();
tl::expected<void, std::string> InitMonsters();
tl::expected<void, std::string> SetMapMonsters(const uint16_t *dunData, Point startPosition);
Monster *AddMonster(Point position, Direction dir, size_t typeIndex, bool inMap);
/**
 * @brief Spawns a new monsters (dynamically/not on level load).
 * The command is only executed for the level owner, to prevent desyncs in multiplayer.
 * The level owner sends a CMD_SPAWNMONSTER-message to the other players.
 */
void SpawnMonster(Point position, Direction dir, size_t typeIndex);
/**
 * @brief Loads data for a dynamically spawned monster when entering a level in multiplayer.
 */
void LoadDeltaSpawnedMonster(size_t typeIndex, size_t monsterId, uint32_t seed, uint8_t golemOwnerPlayerId, int16_t golemSpellLevel);
/**
 * @brief Initialize a spanwed monster (from a network message or from SpawnMonster-function).
 */
void InitializeSpawnedMonster(Point position, Direction dir, size_t typeIndex, size_t monsterId, uint32_t seed, uint8_t golemOwnerPlayerId, int16_t golemSpellLevel);
void AddDoppelganger(Monster &monster);
void ApplyMonsterDamage(DamageType damageType, Monster &monster, int damage);
bool M_Talker(const Monster &monster);
void M_StartStand(Monster &monster, Direction md);
void M_ClearSquares(const Monster &monster);
void M_GetKnockback(Monster &monster, WorldTilePosition attackerStartPos);
void M_StartHit(Monster &monster, int dam);
void M_StartHit(Monster &monster, const Player &player, int dam);
void StartMonsterDeath(Monster &monster, const Player &player, bool sendmsg);
void MonsterDeath(Monster &monster, Direction md, bool sendmsg);
void KillGolem(Monster &golem);
void M_StartKill(Monster &monster, const Player &player);
void M_SyncStartKill(Monster &monster, Point position, const Player &player);
void M_UpdateRelations(const Monster &monster);
void DoEnding();
void PrepDoEnding();
bool Walk(Monster &monster, Direction md);
void GolumAi(Monster &golem);
void DeleteMonsterList();
void RemoveEnemyReferences(const Player &player);
void ProcessMonsters();
void FreeMonsters();
bool DirOK(const Monster &monster, Direction mdir);
bool LineClearMissile(Point startPoint, Point endPoint);
/**
 * @brief Checks for same missile obstructions as CheckMissileCol() for missiles that move along a path between two points
 */
bool LineClearMovingMissile(Point startPoint, Point endPoint);
tl::expected<void, std::string> SyncMonsterAnim(Monster &monster);
void M_FallenFear(Point position);
void PrintMonstHistory(int mt);
void PrintUniqueHistory();
void PlayEffect(Monster &monster, MonsterSound mode);
void MissToMonst(Missile &missile, Point position);

Monster *FindMonsterAtPosition(Point position, bool ignoreMovingMonsters = false);
Monster *FindUniqueMonster(UniqueMonsterType monsterType);
Monster *FindGolemForPlayer(const Player &player);

/**
 * @brief Check that the given tile is available to the monster
 */
bool IsTileAvailable(const Monster &monster, Point position);
bool IsSkel(_monster_id mt);
bool IsGoat(_monster_id mt);
/**
 * @brief Reveals a monster that was hiding in a container
 * @param monster instance returned from a previous call to PreSpawnSkeleton
 * @param position tile to try spawn the monster at, neighboring tiles will be used as a fallback
 */
void ActivateSkeleton(Monster &monster, Point position);
Monster *PreSpawnSkeleton();
void TalktoMonster(Player &player, Monster &monster);
void SpawnGolem(const Player &player, Point position, uint8_t spellLevel);
bool CanTalkToMonst(const Monster &monster);
uint8_t encode_enemy(Monster &monster);
void decode_enemy(Monster &monster, uint8_t enemyId);

} // namespace devilution
