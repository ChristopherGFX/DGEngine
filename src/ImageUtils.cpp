#include "ImageUtils.h"
#include "CachedImagePack.h"
#include "Pcx.h"
#include "PhysFSStream.h"
#include "rectpack2D/finders_interface.h"
#include "Utils/NumberVector.h"
#include "Utils/Utils.h"

namespace ImageUtils
{
	void applyMask(sf::Image& image, const sf::Color& transparencyMask)
	{
		if (transparencyMask != sf::Color::Transparent)
		{
			auto imgSize = image.getSize();
			auto size = imgSize.x * imgSize.y * 4;
			if (size > 0)
			{
				auto pixelPtr = (sf::Uint8*)image.getPixelsPtr();
				auto pixelPtrEnd = pixelPtr + size;
				while (pixelPtr < pixelPtrEnd)
				{
					if ((pixelPtr[0] == transparencyMask.r) &&
						(pixelPtr[1] == transparencyMask.g) &&
						(pixelPtr[2] == transparencyMask.b) &&
						(pixelPtr[3] == transparencyMask.a))
					{
						pixelPtr[0] = pixelPtr[1] = pixelPtr[2] = pixelPtr[3] = 0;
					}
					pixelPtr += 4;
				}
			}
		}
	}

	sf::Image createPackedImage(const ImageContainer& imgContainer,
		const std::shared_ptr<Palette>& pal, std::vector<sf::IntRect>& rects)
	{
		if (imgContainer.size() == 1)
		{
			return imgContainer.get(0, (pal == nullptr ? nullptr : &pal->palette));
		}

		using spaces_t = rectpack2D::empty_spaces<false, rectpack2D::default_empty_spaces>;
		using rect_t = rectpack2D::output_rect_t<spaces_t>;

		bool success = true;

		auto reportSuccessful = [](rect_t&) {
			return rectpack2D::callback_result::CONTINUE_PACKING;
		};

		auto reportUnsuccessful = [&success](rect_t&) {
			success = false;
			return rectpack2D::callback_result::ABORT_PACKING;
		};

		const auto maxSide = (int)std::min(sf::Texture::getMaximumSize(), 3072u);
		const auto discardStep = 2;

		CachedImagePack imgCache(&imgContainer, pal);

		std::vector<rect_t> imgRects;

		for (size_t i = 0; i < imgCache.size(); i++)
		{
			auto frameSize = imgCache[i].getSize();
			imgRects.push_back(rect_t(0, 0, (int)frameSize.x, (int)frameSize.y));
		}

		const auto resultSize = rectpack2D::find_best_packing<spaces_t>(
			imgRects,
			make_finder_input(
				maxSide,
				discardStep,
				reportSuccessful,
				reportUnsuccessful,
				rectpack2D::flipping_option::DISABLED
			)
		);

		if (success == false)
		{
			return {};
		}

		sf::Image img;
		img.create(resultSize.w, resultSize.h, sf::Color::Transparent);

		for (size_t i = 0; i < imgRects.size(); i++)
		{
			const auto& r = imgRects[i];
			rects.push_back({ r.x, r.y, r.w, r.h });
			img.copy(imgCache[i], r.x, r.y);
		}
		return img;
	}

	sf::Image loadImage(const std::string_view fileName, const sf::Color& transparencyMask)
	{
		sf::Image image;

		if (Utils::endsWith(Utils::toLower(fileName), ".pcx"))
		{
			image = LoadImagePCX(fileName.data());
		}
		else
		{
			sf::PhysFSStream file(fileName.data());
			if (file.hasError() == true)
			{
				return image;
			}
			image.loadFromStream(file);
		}
		applyMask(image, transparencyMask);
		return image;
	}

	sf::Image loadImage(const ImageContainer& imgContainer,
		const std::shared_ptr<Palette>& pal)
	{
		if (imgContainer.size() == 1)
		{
			return imgContainer.get(0, (pal == nullptr ? nullptr : &pal->palette));
		}

		CachedImagePack imgCache(&imgContainer, pal);

		size_t imgWidth = 0;
		size_t imgHeight = 0;
		for (size_t i = 0; i < imgCache.size(); i++)
		{
			auto frameSize = imgCache[i].getSize();
			if (imgWidth < frameSize.x)
			{
				imgWidth = frameSize.x;
			}
			imgHeight += frameSize.y;
		}

		sf::Image img;
		img.create(imgWidth, imgHeight, sf::Color::Transparent);

		size_t maxHeight = 0;
		for (size_t fr = 0; fr < imgCache.size(); fr++)
		{
			const auto& frame = imgCache[fr];
			auto frameSize = frame.getSize();
			for (size_t j = 0; j < frameSize.y; j++)
			{
				for (size_t i = 0; i < frameSize.x; i++)
				{
					img.setPixel(i, maxHeight + j, frame.getPixel(i, j));
				}
			}
			maxHeight += frameSize.y;
		}
		return img;
	}

	sf::Image loadImageFrame(const ImageContainer& imgContainer,
		const PaletteArray* pal, size_t frameIdx)
	{
		if (imgContainer.size() > 0 && frameIdx < imgContainer.size())
		{
			return imgContainer.get(frameIdx, pal);
		}
		return {};
	}

	sf::Image loadBitmapFontImage(const ImageContainer& imgContainer,
		const std::string_view fileNameCharMap, const std::shared_ptr<Palette>& pal)
	{
		CachedImagePack imgCache(&imgContainer, pal);

		auto cacheSize = imgCache.size();
		if (cacheSize == 0)
		{
			return {};
		}

		auto firstImgSize = imgCache[0].getSize();
		sf::Image img;
		img.create(firstImgSize.x * 16, firstImgSize.y * 16, sf::Color::Transparent);
		NumberVector<uint8_t> charMapping(fileNameCharMap, 0, 256);

		size_t xx = 0;
		size_t yy = 0;
		for (auto charMap : charMapping.getContainer())
		{
			if (charMap != 0xFF && charMap < cacheSize)
			{
				const auto& frame = imgCache[charMap];
				auto frameSize = frame.getSize();

				for (size_t j = 0; j < frameSize.y; j++)
				{
					for (size_t i = 0; i < frameSize.x; i++)
					{
						img.setPixel(
							frameSize.x * xx + i,
							frameSize.y * yy + j,
							frame.getPixel(i, j)
						);
					}
				}
			}
			xx++;
			if (xx == 16)
			{
				xx = 0;
				yy++;
			}
		}
		return img;
	}

	sf::Image splitImageHorizontal(const sf::Image& img, unsigned pieces)
	{
		if (pieces <= 1)
		{
			return img;
		}

		auto size = img.getSize();

		if (pieces > size.x)
		{
			return img;
		}

		auto newX = size.x / pieces;

		if (size.x % pieces != 0)
		{
			return img;
		}

		auto newY = size.y * pieces;

		sf::Image newImg;
		newImg.create(newX, newY);

		sf::IntRect rect(0, 0, newX, size.y);

		for (auto i = 0u; i < pieces; i++)
		{
			rect.left = i * newX;
			newImg.copy(img, 0, i * size.y, rect);
		}

		return newImg;
	}

	sf::Image splitImageVertical(const sf::Image& img, unsigned pieces)
	{
		if (pieces <= 1)
		{
			return img;
		}

		auto size = img.getSize();

		if (pieces > size.y)
		{
			return img;
		}

		auto newY = size.y / pieces;

		if (size.y % pieces != 0)
		{
			return img;
		}

		auto newX = size.x * pieces;

		sf::Image newImg;
		newImg.create(newX, newY);

		sf::IntRect rect(0, 0, size.x, newY);

		for (auto i = 0u; i < pieces; i++)
		{
			rect.top = i * newY;
			newImg.copy(img, i * size.x, 0, rect);
		}

		return newImg;
	}
}
