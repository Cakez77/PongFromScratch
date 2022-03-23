#include "assets.h"

#include "dds_strcuts.h"
#include "logger.h"
#include "platform.h"

const char *get_asset(AssetTypeID assetTypeID)
{
    switch (assetTypeID)
    {
    case ASSET_SPRITE_WHITE:
    {
        char *white = new char[4];
        white[0] = 255;
        white[1] = 255;
        white[2] = 255;
        white[3] = 255;

        return (const char *)white;
    }
    break;

    case ASSET_SPRITE_BALL:
    {
        uint32_t size;
        const char *data = platform_read_file("assets/textures/ball.DDS", &size);
        return data;
    }
    break;

    case ASSET_SPRITE_PADDLE:
    {
        uint32_t size;
        const char *data = platform_read_file("assets/textures/paddle.DDS", &size);
        return data;
    }
    break;

    default:
        CAKEZ_ASSERT(0, "Unrecognized Asset Type ID: %d", assetTypeID);
    }

    return 0;
}

Vec2 get_texture_size(AssetTypeID assetTypeID)
{
    Vec2 size = {};
    switch (assetTypeID)
    {
    case ASSET_SPRITE_WHITE:
    {
        size =  {1.0f, 1.0f};
    }
    break;

    case ASSET_SPRITE_BALL:
    {
        size = {50.0f, 50.0f};
    }
    break;

    case ASSET_SPRITE_PADDLE:
    {
        size = {50.0f, 100.0f};
    }
    break;

    default:
        CAKEZ_ASSERT(0, "Unrecognized Asset Type ID: %d", assetTypeID);
    }

    return size;
}