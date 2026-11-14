#pragma once

enum class FontOutlineCap {
	Butt = 0,
	Round = 1,
	Square = 2,
};

enum class FontOutlineJoin {
	Round = 0,
	Bevel = 1,
	MiterVariable = 2,
	MiterFixed = 3,
};

enum class FontRasterization {
	Blended,
	Solid,
};

enum class FontTextureFiltering {
	Linear,
	Nearest,
};

struct FontRenderOptions {
	FontOutlineCap outline_cap = FontOutlineCap::Round;
	FontOutlineJoin outline_join = FontOutlineJoin::Round;
	FontRasterization rasterization = FontRasterization::Blended;
	FontTextureFiltering texture_filtering = FontTextureFiltering::Linear;

	static FontRenderOptions pixel() {
		FontRenderOptions options;
		options.outline_cap = FontOutlineCap::Square;
		options.outline_join = FontOutlineJoin::MiterFixed;
		options.rasterization = FontRasterization::Solid;
		options.texture_filtering = FontTextureFiltering::Nearest;
		return options;
	}
};
