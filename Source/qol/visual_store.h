/**
 * @file qol/visual_store.h
 *
 * Interface of visual grid-based store UI.
 */
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "engine/point.hpp"
#include "engine/surface.hpp"
#include "items.h"

namespace devilution {

enum class VisualStoreVendor : uint8_t {
	Smith,
	Witch,
	Healer,
	Boy
};

enum class VisualStoreTab : uint8_t {
	Regular = 0,
	Premium = 1
};

// Grid: 5x5 = 25 items per page (sufficient for all vendors)
inline constexpr int VisualStoreGridWidth = 10;
inline constexpr int VisualStoreGridHeight = 9;
inline constexpr int VisualStoreItemsPerPage = VisualStoreGridWidth * VisualStoreGridHeight;

struct VisualStoreItem {
	uint16_t index; // Index in the vendor's item list
	Point position; // Top-left position in the grid
};

struct VisualStorePage {
	std::vector<VisualStoreItem> items;
	uint16_t grid[VisualStoreGridWidth][VisualStoreGridHeight];
};

struct VisualStoreState {
	VisualStoreVendor vendor;
	VisualStoreTab activeTab; // For Smith: Regular vs Premium
	unsigned currentPage;
	std::vector<VisualStorePage> pages;
};

extern bool IsVisualStoreOpen;
extern VisualStoreState VisualStore;
extern int16_t pcursstoreitem; // Currently highlighted store item index (-1 if none)
extern int16_t pcursstorebtn; 

/**
 * @brief Load visual store graphics.
 */
void InitVisualStore();

/**
 * @brief Free visual store graphics.
 */
void FreeVisualStoreGFX();

/**
 * @brief Open the visual store for a vendor.
 * Opens both the store panel (left) and inventory panel (right).
 * @param vendor The vendor to open the store for.
 */
void OpenVisualStore(VisualStoreVendor vendor);

/**
 * @brief Close the visual store and inventory panels.
 */
void CloseVisualStore();

/**
 * @brief Set the active tab for Smith (Regular/Premium).
 * @param tab The tab to switch to.
 */
void SetVisualStoreTab(VisualStoreTab tab);

/**
 * @brief Navigate to the next page of store items.
 */
void VisualStoreNextPage();

/**
 * @brief Navigate to the previous page of store items.
 */
void VisualStorePreviousPage();

/**
 * @brief Render the visual store panel to the given buffer.
 */
void DrawVisualStore(const Surface &out);

/**
 * @brief Handle a click on the visual store panel.
 * @param mousePosition The mouse position.
 */
void CheckVisualStoreItem(Point mousePosition);

/**
 * @brief Handle dropping an item on the visual store to sell.
 * @param mousePosition The mouse position.
 */
void CheckVisualStorePaste(Point mousePosition);

/**
 * @brief Check for item highlight under the cursor.
 * @param mousePosition The mouse position.
 * @return The index of the highlighted item, or -1 if none.
 */
int16_t CheckVisualStoreHLight(Point mousePosition);

/**
 * @brief Handle button press in the visual store.
 * @param mousePosition The mouse position.
 */
void CheckVisualStoreButtonPress(Point mousePosition);

/**
 * @brief Handle button release in the visual store.
 * @param mousePosition The mouse position.
 */
void CheckVisualStoreButtonRelease(Point mousePosition);

/**
 * @brief Check if an item can be sold to the current vendor.
 * @param item The item to check.
 * @return true if the item can be sold.
 */
bool CanSellToCurrentVendor(const Item &item);

/**
 * @brief Sell an item from the player's inventory to the current vendor.
 * @param invIndex The inventory index of the item.
 */
void SellItemToVisualStore(int invIndex);

/**
 * @brief Get the number of items for the current vendor/tab.
 * @return The item count.
 */
int GetVisualStoreItemCount();

/**
 * @brief Get the items array for the current vendor/tab.
 * @return A span of items.
 */
std::span<Item> GetVisualStoreItems();

/**
 * @brief Get the total number of pages for the current vendor/tab.
 * @return The page count.
 */
int GetVisualStorePageCount();

/**
 * @brief Convert a grid slot position to screen coordinates.
 * @param slot The grid slot position.
 * @return The screen coordinates.
 */
Point GetVisualStoreSlotCoord(Point slot);

/**
 * @brief Gets the point for a btn on the panel.
 * @param slot Btn id.
 * @return The screen coordinates.
 */
Rectangle GetVisualBtnCoord(int btnId);

/**
 * @brief Calculate the cost to repair an item.
 * @param item The item to repair.
 * @return The cost in gold.
 */
int GetRepairCost(const Item &item);

/**
 * @brief Repair a specific item from the player's inventory/body.
 * @param invIndex The inventory index of the item.
 */
void VisualStoreRepairItem(int invIndex);

} // namespace devilution
