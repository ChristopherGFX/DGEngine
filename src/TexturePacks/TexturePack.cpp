#include "TexturePack.h"
#include "TextureInfo.h"

void TexturePack::updateTextureRect(TextureInfo& ti)
{
	auto size = ti.texture->getSize();
	ti.textureRect.left = 0;
	ti.textureRect.top = 0;
	ti.textureRect.width = (int)size.x;
	ti.textureRect.height = (int)size.y;
}

std::pair<uint32_t, uint32_t> TexturePack::getRange(uint32_t startIdx,
	uint32_t stopIdx, int32_t directionIdx, uint32_t directions)
{
	if (directions > 1 && directionIdx >= 0 && (uint32_t)directionIdx < directions)
	{
		auto animSize = stopIdx - startIdx;
		if (animSize % directions == 0)
		{
			auto frameSize = animSize / directions;
			return std::make_pair(
				startIdx + (frameSize * directionIdx),
				startIdx + (frameSize * (directionIdx + 1)) - 1
			);
		}
	}
	return std::make_pair(startIdx, stopIdx - 1);
}

std::pair<uint32_t, uint32_t> TexturePack::getRange(
	int32_t groupIdx, int32_t directionIdx, AnimationType& animType) const
{
	animType = {};
	return std::make_pair((uint32_t)0, size() - 1);
}
