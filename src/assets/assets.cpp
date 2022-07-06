#include "assets.h"

#include "dds_strcuts.h"
#include "logger.h"
#include "platform.h"

const char *get_asset(AssetTypeID assetTypeID)
{
    uint32_t size;

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
        const char *data = platform_read_file("assets/textures/ball.DDS", &size);
        return data;
    }
    break;

    case ASSET_SPRITE_PADDLE:
    {
        const char *data = platform_read_file("assets/textures/paddle.DDS", &size);
        return data;
    }
    break;

    case ASSET_SPRITE_FONT_ATLAS:
    {
        const char *data = platform_read_file("assets/textures/Font_Atlas_mono_5_10.DDS", &size);
        return data;
    }
    break;

    case ASSET_SPRITE_BUTTON_64_16:
    {
        const char *data = platform_read_file("assets/textures/Button_64_16.DDS", &size);
        return data;
    }

    case ASSET_SPRITE_PONG_FROM_SCRATCH_LOGO:
    {
        const char *data = platform_read_file("assets/textures/Pong_from_Scratch_Logo.DDS", &size);
        return data;
    }

    default:
        CAKEZ_ASSERT(0, "Unrecognized Asset Type ID: %d", assetTypeID);
    }

    return 0;
}

Texture get_texture(AssetTypeID assetTypeID)
{
    Texture texture = {};
    // Scaled by 3x
    switch (assetTypeID)
    {
    case ASSET_SPRITE_WHITE:
    {
        texture.size = {1.0f, 1.0f};
        texture.subSize = texture.size;
    }
    break;

    case ASSET_SPRITE_BALL:
    {
        texture.size = {48.0f, 48.0f};
        texture.subSize = texture.size;
    }
    break;

    case ASSET_SPRITE_PADDLE:
    {
        texture.size = {48.0f, 96.0f};
        texture.subSize = texture.size;
    }
    break;

    case ASSET_SPRITE_FONT_ATLAS:
    {
        texture.size = {240.0f, 240.0f};
        texture.subSize = {15.0f, 30.0f};
    }
    break;

    case ASSET_SPRITE_BUTTON_64_16:
        texture.size = {576.0f, 48.0f};
        texture.subSize = {192.0f, 48.0f};
        break;

    case ASSET_SPRITE_PONG_FROM_SCRATCH_LOGO:
        texture.size = {900.0f, 600.0f};
        texture.subSize = texture.size;
        break;

    default:
        CAKEZ_ASSERT(0, "Unrecognized Asset Type ID: %d", assetTypeID);
    }

    return texture;
}