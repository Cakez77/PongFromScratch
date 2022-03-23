#pragma once

#include "my_math.h"

enum AssetTypeID
{
    ASSET_SPRITE_WHITE,
    ASSET_SPRITE_BALL,
    ASSET_SPRITE_PADDLE,

    ASSET_COUNT
};

const char *get_asset(AssetTypeID assetTypeID);
Vec2 get_texture_size(AssetTypeID assetTypeID);