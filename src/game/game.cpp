
#include "assets/assets.h"
#include "defines.h"
#include "logger.h"
#include "my_math.h"

#include "renderer/shared_render_types.h"

uint32_t constexpr MAX_ENTITIES = 100;
uint32_t constexpr MAX_MATERIALS = 100;

struct Entity
{
    Transform transform;
};

struct Material
{
    AssetTypeID assetTypeID;
    MaterialData materialData;
};

struct GameState
{
    uint32_t entityCount;
    Entity entities[MAX_ENTITIES];

    uint32_t materialCount;
    Material materials[MAX_MATERIALS];
};

internal Entity *create_entity(GameState *gameState, Transform transform)
{
    Entity *e = 0;

    if (gameState->entityCount < MAX_ENTITIES)
    {
        e = &gameState->entities[gameState->entityCount++];
        e->transform = transform;
    }
    else
    {
        CAKEZ_ASSERT(0, "Reached Maximum amount of Entities!");
    }

    return e;
}

internal uint32_t create_material(GameState *gameState, AssetTypeID assetTypeID,
                                  Vec4 color = {1.0f, 1.0f, 1.0f, 1.0f})
{
    uint32_t materialIdx = INVALID_IDX;

    if (gameState->materialCount < MAX_MATERIALS)
    {
        materialIdx = gameState->materialCount;
        Material *m = &gameState->materials[gameState->materialCount++];
        m->assetTypeID = assetTypeID;
        m->materialData.color = color;
    }
    else
    {
        CAKEZ_ASSERT(0, "Reachted Maximum amount of Materials");
    }

    return materialIdx;
}

internal uint32_t get_material(GameState *gameState, AssetTypeID assetTypeID,
                               Vec4 color = {1.0f, 1.0f, 1.0f, 1.0f})
{
    uint32_t materialIdx = INVALID_IDX;

    for (uint32_t i = 0; i < gameState->materialCount; i++)
    {
        Material *m = &gameState->materials[i];

        if (m->assetTypeID == assetTypeID &&
            m->materialData.color == color)
        {
            materialIdx = i;
            break;
        }
    }

    if (materialIdx == INVALID_IDX)
    {
        materialIdx = create_material(gameState, assetTypeID, color);
    }

    return materialIdx;
}

internal Material *get_material(GameState *gameState, uint32_t materialIdx)
{
    CAKEZ_ASSERT(materialIdx < gameState->materialCount, "MaterialIdx out of bounds");

    Material *m = 0;

    if (materialIdx < gameState->materialCount)
    {
        m = &gameState->materials[materialIdx];
    }
    else
    {
        // By default we return the first Material, this will default to ASSET_SPRITE_WHITE
        m = &gameState->materials[0];
    }

    return m;
}

bool init_game(GameState *gameState)
{
    float counter = 0.0f;
    for (uint32_t i = 0; i < 10; i++)
    {
        for (uint32_t j = 0; j < 10; j++)
        {
            float r = counter / 100.0f;
            float g = 1.0f - r;
            float b = r;
            float a = 1.0f - r;

            Entity *e = create_entity(gameState, {i * 120.0f, j * 60.0f, 70.0f, 70.0f});
            e->transform.materialIdx = create_material(gameState, ASSET_SPRITE_CAKEZ,
                                                       {r, g, b, a});
            counter += 1.0f;
        }
    }

    return true;
}

void update_game(GameState *gameState)
{
    // Does nothing
    for (uint32_t i = 0; i < gameState->entityCount; i++)
    {
        Entity *e = &gameState->entities[i];

        // This is frame rate dependent
        e->transform.xPos += 0.01f;
    }
}