#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace devilution {

class BidiText {
public:
	explicit BidiText(std::string_view text);

	std::string_view visual() const { return visual_; }

	size_t logicalToVisual(size_t logicalPos) const;
	size_t visualToLogical(size_t visualPos) const;
	bool isVisualPositionRTL(size_t visualPos) const;

private:
	std::string text_;
	std::string visual_;
	std::vector<int> logicalToVisualMap_;
	std::vector<int> visualToLogicalMap_;
	std::vector<bool> isRTLVisualMap_;
};

} // namespace devilution
