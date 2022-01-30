#pragma once

enum AssetTypeID
{
    ASSET_SPRITE_WHITE,
    ASSET_SPRITE_BALL,
    ASSET_SPRITE_PADDLE,

    ASSET_COUNT
};

const char *get_asset(AssetTypeID assetTypeID);