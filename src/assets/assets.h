#pragma once

enum AssetTypeID
{
    ASSET_SPRITE_WHITE,
    ASSET_SPRITE_CAKEZ,

    ASSET_COUNT
};

const char* get_asset(AssetTypeID assetTypeID);