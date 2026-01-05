/**
 * @file objects.h
 *
 * Interface of object functionality, interaction, spawning, loading, etc.
 */
#pragma once

#include <cmath>
#include <cstdint>
#include <string>

#include <expected.hpp>

#include "cursor.h"
#include "engine/clx_sprite.hpp"
#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/world_tile.hpp"
#include "levels/dun_tile.hpp"
#include "monster.h"
#include "tables/itemdat.h"
#include "tables/objdat.h"
#include "tables/textdat.h"
#include "utils/attributes.h"
#include "utils/is_of.hpp"
#include "utils/string_or_view.hpp"

namespace devilution {

#define MAXOBJECTS 127

struct Object {
	_object_id _otype = OBJ_NULL;
	bool applyLighting = false;
	bool _oTrapFlag = false;
	bool _oDoorFlag = false;

	Point position;
	uint32_t _oAnimFlag = 0;
	OptionalClxSpriteList _oAnimData;
	int _oAnimDelay = 0;      // Tick length of each frame in the current animation
	int _oAnimCnt = 0;        // Increases by one each game tick, counting how close we are to _pAnimDelay
	uint32_t _oAnimLen = 0;   // Number of frames in current animation
	uint32_t _oAnimFrame = 0; // Current frame of animation.

	// TODO: Remove this field, it is unused and always equal to:
	// (*_oAnimData)[0].width()
	uint16_t _oAnimWidth = 0;

	bool _oDelFlag = false;
	int8_t _oBreak = 0;
	bool _oSolidFlag = false;
	/** True if the object allows missiles to pass through, false if it collides with missiles */
	bool _oMissFlag = false;
	SelectionRegion selectionRegion = SelectionRegion::None;
	bool _oPreFlag = false;
	int _olid = 0;
	/**
	 * Saves the absolute value of the engine state (typically from a call to AdvanceRndSeed()) to later use when spawning items from a container object
	 * This is an unsigned value to avoid implementation defined behaviour when reading from this variable.
	 */
	uint32_t _oRndSeed = 0;
	int _oVar1 = 0;
	int _oVar2 = 0;
	int _oVar3 = 0;
	int _oVar4 = 0;
	int _oVar5 = 0;
	uint32_t _oVar6 = 0;
	int _oVar8 = 0;

	/**
	 * @brief ID of a quest message to play when this object is activated.
	 *
	 * Used by spell book objects which trigger quest progress for Halls of the Blind, Valor, or Warlord of Blood
	 */
	_speech_id bookMessage = TEXT_NONE;

	/**
	 * @brief Returns the network identifier for this object
	 *
	 * This is currently the index into the Objects array, but may change in the future.
	 */
	[[nodiscard]] unsigned int GetId() const;

	/**
	 * @brief Marks the map region to be refreshed when the player interacts with the object.
	 *
	 * Some objects will cause a map region to change when a player interacts with them (e.g. Skeleton King
	 * antechamber levers). The coordinates used for this region are based on a 40*40 grid overlaying the central
	 * 80*80 region of the dungeon.
	 *
	 * @param topLeftPosition corner of the map region closest to the origin.
	 * @param bottomRightPosition corner of the map region furthest from the origin.
	 */
	constexpr void SetMapRange(WorldTilePosition topLeftPosition, WorldTilePosition bottomRightPosition)
	{
		_oVar1 = topLeftPosition.x;
		_oVar2 = topLeftPosition.y;
		_oVar3 = bottomRightPosition.x;
		_oVar4 = bottomRightPosition.y;
	}

	/**
	 * @brief Convenience function for SetMapRange(Point, Point).
	 * @param mapRange A rectangle defining the top left corner and size of the affected region.
	 */
	constexpr void SetMapRange(WorldTileRectangle mapRange)
	{
		SetMapRange(mapRange.position, mapRange.position + DisplacementOf<uint8_t>(mapRange.size));
	}

	/**
	 * @brief Sets up a generic quest book which will trigger a change in the map when activated.
	 *
	 * Books of this type use a generic message (see OperateSChambBook()) compared to the more specific quest books
	 * initialized by IntializeQuestBook().
	 *
	 * @param mapRange The region to be updated when this object is activated.
	 */
	constexpr void InitializeBook(WorldTileRectangle mapRange)
	{
		SetMapRange(mapRange);
		_oVar6 = _oAnimFrame + 1; // Save the frame number for the open book frame
	}

	/**
	 * @brief Initializes this object as a quest book which will cause further changes and play a message when activated.
	 * @param mapRange The region to be updated when this object is activated.
	 * @param leverID An ID (distinct from the object index) to identify the new objects spawned after updating the map.
	 * @param message The quest text to play when this object is activated.
	 */
	constexpr void InitializeQuestBook(WorldTileRectangle mapRange, int leverID, _speech_id message)
	{
		InitializeBook(mapRange);
		_oVar8 = leverID;
		bookMessage = message;
	}

	/**
	 * @brief Marks an object which was spawned from a sublevel in response to a lever activation.
	 * @param mapRange The region which was updated to spawn this object.
	 * @param leverID The id (*not* an object ID/index) of the lever responsible for the map change.
	 */
	constexpr void InitializeLoadedObject(WorldTileRectangle mapRange, int leverID)
	{
		SetMapRange(mapRange);
		_oVar8 = leverID;
	}

	/**
	 * @brief Check if the object can be broken (is an intact barrel or crux)
	 * @return True if the object is intact and breakable, false if already broken or not a breakable object.
	 */
	[[nodiscard]] constexpr bool IsBreakable() const
	{
		return _oBreak == 1;
	}

	/**
	 * @brief Check if the object has been broken
	 * @return True if the object is breakable and has been broken, false if unbroken or not a breakable object.
	 */
	[[nodiscard]] constexpr bool IsBroken() const
	{
		return _oBreak == -1;
	}

	/**
	 * Returns true if the object is a harmful shrine and the player has disabled permanent shrine effects.
	 */
	[[nodiscard]] bool IsDisabled() const;

	[[nodiscard]] constexpr bool canInteractWith() const
	{
		return selectionRegion != SelectionRegion::None;
	}

	/**
	 * @brief Check if this object is barrel (or explosive barrel)
	 * @return True if the object is one of the barrel types (see _object_id)
	 */
	[[nodiscard]] constexpr bool IsBarrel() const
	{
		return IsAnyOf(_otype, _object_id::OBJ_BARREL, _object_id::OBJ_BARRELEX, _object_id::OBJ_POD, _object_id::OBJ_PODEX, _object_id::OBJ_URN, _object_id::OBJ_URNEX);
	}

	/**
	 * @brief Check if this object contains explosives or caustic material
	 */
	[[nodiscard]] constexpr bool isExplosive() const
	{
		return IsAnyOf(_otype, _object_id::OBJ_BARRELEX, _object_id::OBJ_PODEX, _object_id::OBJ_URNEX);
	}

	/**
	 * @brief Check if this object is a chest (or trapped chest).
	 *
	 * Trapped chests get their base type change in addition to having the trap flag set, but if they get "refilled" by
	 * a Thaumaturgic shrine the base type is not reverted. This means you need to consider both the base type and the
	 * trap flag to differentiate between chests that are currently trapped and chests which have never been trapped.
	 *
	 * @return True if the object is any of the chest types (see _object_id)
	 */
	[[nodiscard]] constexpr bool IsChest() const
	{
		return IsAnyOf(_otype, _object_id::OBJ_CHEST1, _object_id::OBJ_CHEST2, _object_id::OBJ_CHEST3, _object_id::OBJ_TCHEST1, _object_id::OBJ_TCHEST2, _object_id::OBJ_TCHEST3);
	}

	/**
	 * @brief Check if this object is a trapped chest (specifically a chest which is currently trapped).
	 * @return True if the object is one of the trapped chest types (see _object_id) and has an active trap.
	 */
	[[nodiscard]] constexpr bool IsTrappedChest() const
	{
		return IsAnyOf(_otype, _object_id::OBJ_TCHEST1, _object_id::OBJ_TCHEST2, _object_id::OBJ_TCHEST3) && _oTrapFlag;
	}

	/**
	 * @brief Check if this object is an untrapped chest (specifically a chest which has not been trapped).
	 * @return True if the object is one of the untrapped chest types (see _object_id) and has no active trap.
	 */
	[[nodiscard]] constexpr bool IsUntrappedChest() const
	{
		return IsAnyOf(_otype, _object_id::OBJ_CHEST1, _object_id::OBJ_CHEST2, _object_id::OBJ_CHEST3) && !_oTrapFlag;
	}

	/**
	 * @brief Check if this object is a crucifix
	 * @return True if the object is one of the crux types (see _object_id)
	 */
	[[nodiscard]] constexpr bool IsCrux() const
	{
		return IsAnyOf(_otype, _object_id::OBJ_CRUX1, _object_id::OBJ_CRUX2, _object_id::OBJ_CRUX3);
	}

	/**
	 * @brief Check if this object is a door
	 * @return True if the object is one of the door types (see _object_id)
	 */
	[[nodiscard]] constexpr bool isDoor() const
	{
		return IsAnyOf(_otype, _object_id::OBJ_L1LDOOR, _object_id::OBJ_L1RDOOR, _object_id::OBJ_L2LDOOR, _object_id::OBJ_L2RDOOR, _object_id::OBJ_L3LDOOR, _object_id::OBJ_L3RDOOR, _object_id::OBJ_L5LDOOR, _object_id::OBJ_L5RDOOR);
	}

	/**
	 * @brief Check if this object is a shrine
	 * @return True if the object is one of the shrine types (see _object_id)
	 */
	[[nodiscard]] constexpr bool IsShrine() const
	{
		return IsAnyOf(_otype, _object_id::OBJ_SHRINEL, _object_id::OBJ_SHRINER);
	}

	/**
	 * @brief Check if this object is a trap source
	 * @return True if the object is one of the trap types (see _object_id)
	 */
	[[nodiscard]] constexpr bool IsTrap() const
	{
		return IsAnyOf(_otype, _object_id::OBJ_TRAPL, _object_id::OBJ_TRAPR);
	}

	/**
	 * @brief Returns the name of the object as shown in the info box
	 */
	[[nodiscard]] StringOrView name() const;

	[[nodiscard]] ClxSprite currentSprite() const
	{
		return (*_oAnimData)[_oAnimFrame - 1];
	}
	[[nodiscard]] Displacement getRenderingOffset(const ClxSprite sprite, Point tilePosition) const
	{
		Displacement offset = Displacement { -CalculateSpriteTileCenterX(sprite.width()), 0 };
		if (position != tilePosition) {
			// drawing a large or offset object, calculate the correct position for the center of the sprite
			Displacement worldOffset = position - tilePosition;
			offset -= worldOffset.worldToScreen();
		}
		return offset;
	}
};

extern DVL_API_FOR_TEST Object Objects[MAXOBJECTS];
extern int AvailableObjects[MAXOBJECTS];
extern int ActiveObjects[MAXOBJECTS];
extern int ActiveObjectCount;
/** @brief Indicates that objects are being loaded during gameplay and pre calculated data should be updated. */
extern bool LoadingMapObjects;
/** Tracks progress through the tome sequence that spawns Na-Krul (see OperateNakrulBook()) */
extern int NaKrulTomeSequence;

/**
 * @brief Find an object given a point in map coordinates
 *
 * @param position The map coordinate to test
 * @param considerLargeObjects Default behaviour will return a pointer to a large object that covers this tile, set
 *                             this param to false if you only want the object whose base position matches this tile
 * @return A pointer to the object or nullptr if no object exists at this location
 */
Object *FindObjectAtPosition(Point position, bool considerLargeObjects = true);

/**
 * @brief Check whether an item occupies this tile position
 * @param position The map coordinate to test
 * @return true if the tile is occupied
 */
inline bool IsObjectAtPosition(Point position)
{
	return FindObjectAtPosition(position) != nullptr;
}

/**
 * @brief Get a reference to the object located at this tile
 *
 * This function is unchecked. Trying to access an invalid position will result in out of bounds memory access
 * @param position The map coordinate of the object
 * @return a reference to the object
 */
inline Object &ObjectAtPosition(Point position)
{
	return Objects[std::abs(dObject[position.x][position.y]) - 1];
}

/**
 * @brief Check whether an item blocking object (solid object or open door) is located at this tile position
 * @param position The map coordinate to test
 * @return true if the tile is blocked
 */
bool IsItemBlockingObjectAtPosition(Point position);

tl::expected<void, std::string> InitObjectGFX();
void FreeObjectGFX();
void AddL1Objs(int x1, int y1, int x2, int y2);
void AddL2Objs(int x1, int y1, int x2, int y2);
void AddL3Objs(int x1, int y1, int x2, int y2);
void AddCryptObjects(int x1, int y1, int x2, int y2);
void InitObjects();
void SetMapObjects(const uint16_t *dunData, int startx, int starty);
/**
 * @brief Spawns an object of the given type at the map coordinates provided
 * @param objType Type specifier
 * @param objPos tile coordinates
 */
Object *AddObject(_object_id objType, Point objPos);
bool UpdateTrapState(Object &trap);
void OperateTrap(Object &trap);
void ProcessObjects();
void RedoPlayerVision();
void MonstCheckDoors(const Monster &monster);
void ObjChangeMap(int x1, int y1, int x2, int y2);
void ObjChangeMapResync(int x1, int y1, int x2, int y2);
_item_indexes ItemMiscIdIdx(item_misc_id imiscid);
void OperateObject(Player &player, Object &object);
void SyncOpObject(Player &player, int cmd, Object &object);
void BreakObjectMissile(const Player *player, Object &object);
void BreakObject(const Player &player, Object &object);
void DeltaSyncOpObject(Object &object);
void DeltaSyncCloseObj(Object &object);
void DeltaSyncBreakObj(Object &object);
void SyncBreakObj(const Player &player, Object &object);
void SyncObjectAnim(Object &object);
/**
 * @brief Updates the text drawn in the info box to describe the given object
 * @param object The currently highlighted object
 */
void GetObjectStr(const Object &object);
void SyncNakrulRoom();

} // namespace devilution
