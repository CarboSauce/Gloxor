#pragma once
#include "glox/pair.hpp"
#include "glox/utilalgs.hpp"
#include "gloxor/types.hpp"

namespace glox
{
	struct framebuffer
	{
		colorT* fbBeg;
		colorT* fbEnd;
		size_t width;
		size_t height;
		size_t pitch;
	};

	template <typename iter, typename T>
	void drawRectangle(iter beg, size_t fbwidth, glox::vec2<T> pos, glox::vec2<T> size, T color)
	{
		for (auto i = beg + pos.y; i < beg + size.y * fbwidth; i += fbwidth)
		{
			glox::setRange(i, i + size.x, color);
		}
	}
} // namespace glox
