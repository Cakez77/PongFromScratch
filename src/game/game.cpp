
#include "assets/assets.h"
#include "defines.h"
#include "logger.h"
#include "input.h"
#include "my_math.h"

#include "renderer/shared_render_types.h"

uint32_t constexpr MAX_ENTITIES = 100;
uint32_t constexpr MAX_MATERIALS = 100;

enum Components
{
    COMPONENT_BALL = BIT(1),
    COMPONENT_LEFT_PADDLE = BIT(2),
    COMPONENT_RIGHT_PADDLE = BIT(3),
};

struct Entity
{
    uint32_t compMask;
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

internal bool has_component(Entity *e, Components c)
{
    return e->compMask & c;
}

internal void add_component(Entity *e, Components c)
{
    e->compMask |= c;
}

internal void remove_component(Entity *e, Components c)
{
    /**
     * AND with NOT c, this turns all bits to 1,
     * except for the one BIT that c references.
     */
    e->compMask &= ~c;
}

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
    float paddleSizeX = 50.0f, paddleSizeY = 100.0f, ballSize = 50.0f;

    Entity *e = create_entity(gameState, {10.0f, 10.0f, paddleSizeX, paddleSizeY});
    add_component(e, COMPONENT_LEFT_PADDLE);
    e->transform.materialIdx = get_material(gameState, ASSET_SPRITE_PADDLE);

    e = create_entity(gameState, {1000.0f - paddleSizeX - 20.0f, 10.0f, paddleSizeX, paddleSizeY});
    add_component(e, COMPONENT_RIGHT_PADDLE);
    e->transform.materialIdx = get_material(gameState, ASSET_SPRITE_PADDLE);

    e = create_entity(gameState, {1000.0f / 2.0f, 400.0f, ballSize, ballSize});
    add_component(e, COMPONENT_BALL);
    e->transform.materialIdx = get_material(gameState, ASSET_SPRITE_BALL);

    return true;
}

void update_game(GameState *gameState, InputState *input)
{
    float vel = 0.1f;

    for (uint32_t i = 0; i < gameState->entityCount; i++)
    {
        Entity *e = &gameState->entities[i];

        if (has_component(e, COMPONENT_LEFT_PADDLE))
        {
            if (key_is_down(input, W_KEY))
            {
                e->transform.yPos -= vel;
            }

            if (key_is_down(input, S_KEY))
            {
                e->transform.yPos += vel;
            }
        }
    }
}