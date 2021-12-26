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

    case ASSET_SPRITE_CAKEZ:
    {
        uint32_t size;
        const char *data = platform_read_file("assets/textures/cakez.DDS", &size);
        return data;
    }
    break;

    default:
        CAKEZ_ASSERT(0, "Unrecognized Asset Type ID: %d", assetTypeID);
    }

    return 0;
}