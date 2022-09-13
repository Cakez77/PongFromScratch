
#include "assets/assets.h"
#include "defines.h"
#include "logger.h"
#include "input.h"
#include "my_math.h"
#include "ui/ui.h"

uint32_t constexpr MAX_ENTITIES = 100;
float constexpr SPEEDUP = 15.0f;
float constexpr INITIAL_X_VEL = 400.0f;
float constexpr INITIAL_Y_VEL_PADDLE = 500.0f;

enum Components
{
    COMPONENT_BALL = BIT(1),
    COMPONENT_LEFT_PADDLE = BIT(2),
    COMPONENT_RIGHT_PADDLE = BIT(3),
};

struct Entity
{
    uint32_t compMask;
    AssetTypeID assetTypeID;
    Vec4 color;
    Vec2 origin;
    Vec2 vel;
    Vec2 spriteOffset;
    Rect boundingBox;
};

enum GameStateID
{
    GAME_STATE_MAIN_MENU,
    GAME_STATE_RUNNING_LEVEL,
    GAME_STATE_SCORE_SCREEN
};

struct GameState
{
    GameStateID gameState;

    uint32_t entityCount;
    float paddleSpeed;
    int score;
    float gameTime;
    Entity entities[MAX_ENTITIES];
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

internal Entity *create_entity(
    GameState *gameState,
    Vec2 origin,
    Vec2 spriteOffset,
    Rect boundingBox,
    AssetTypeID assetTypeID = ASSET_SPRITE_WHITE,
    Vec4 color = {1.0f, 1.0f, 1.0f, 1.0f})
{
    Entity *e = 0;

    if (gameState->entityCount < MAX_ENTITIES)
    {
        e = &gameState->entities[gameState->entityCount++];
        e->origin = origin;
        e->spriteOffset = spriteOffset;
        e->boundingBox = boundingBox;
        e->assetTypeID = assetTypeID;
        e->color = color;
    }
    else
    {
        CAKEZ_ASSERT(0, "Reached Maximum amount of Entities!");
    }

    return e;
}

//########################################################################################
//                      Functions related to the Game
//########################################################################################
internal Rect get_bounding_box(Entity *e)
{
    CAKEZ_ASSERT(e, "No Entity supplied");
    return Rect{e->origin + e->boundingBox.offet, e->boundingBox.size};
}

bool init_game(GameState *gameState, InputState *input)
{
    Vec2 paddleSize = get_texture(ASSET_SPRITE_PADDLE).subSize;
    Vec2 spriteOffsetPaddle = paddleSize / 2.0f * -1.0f;
    Vec2 ballSize = get_texture(ASSET_SPRITE_BALL).subSize;
    Vec2 spriteOffsetBall = ballSize / 2.0f * -1.0f;

    // Left Paddle
    Entity *e = create_entity(gameState,
                              {10.0f + paddleSize.x / 2.0f, input->screenSize.y / 2.0f},
                              spriteOffsetPaddle,
                              {spriteOffsetPaddle, paddleSize},
                              ASSET_SPRITE_PADDLE);
    add_component(e, COMPONENT_LEFT_PADDLE);

    // Right Paddle
    e = create_entity(gameState,
                      {input->screenSize.x - 10.0f - paddleSize.x / 2.0f, input->screenSize.y / 2.0f},
                      spriteOffsetPaddle,
                      {spriteOffsetPaddle, paddleSize},
                      ASSET_SPRITE_PADDLE);
    add_component(e, COMPONENT_RIGHT_PADDLE);

    // Ball
    e = create_entity(gameState, input->screenSize / 2.0f,
                      spriteOffsetBall,
                      {spriteOffsetBall, ballSize},
                      ASSET_SPRITE_BALL);
    add_component(e, COMPONENT_BALL);
    e->vel = {INITIAL_X_VEL, INITIAL_X_VEL/2.0f};
    gameState->paddleSpeed = INITIAL_Y_VEL_PADDLE;

    return true;
}

internal void update_level(GameState *gameState, InputState *input, UIState *ui, float dt)
{
    gameState->gameTime += dt;
    gameState->paddleSpeed += SPEEDUP * dt;
    float speed = gameState->paddleSpeed;

    // Input
    {
        if(key_pressed_this_frame(input, KEY_ESC))
        {
            gameState->gameState = GAME_STATE_MAIN_MENU;
        }
    }

    // Time
    {
        do_text(ui, {10.0f, 10.0f}, "Time: ");
        do_number(ui, {85.0f, 10.0f}, (int)gameState->gameTime);
    }

    // Score
    {
        do_text(ui, {200.0f, 10.0f}, "Score: ");
        do_number(ui, {295.0f, 10.0f}, gameState->score);
    }

    for (uint32_t i = 0; i < gameState->entityCount; i++)
    {
        Entity *e = &gameState->entities[i];

        if (has_component(e, COMPONENT_LEFT_PADDLE))
        {
            e->vel = {};
            if (key_is_down(input, KEY_W))
            {
                e->vel.y = -speed;
            }

            if (key_is_down(input, KEY_S))
            {
                e->vel.y = speed;
            }

            // Confine the Paddle to the screen
            {
                e->origin.y = clamp(e->origin.y,
                                    e->boundingBox.size.y / 2.0f,
                                    input->screenSize.y - e->boundingBox.size.y / 2.0f);
            }

            e->origin = e->origin + e->vel * dt;
        }

        if (has_component(e, COMPONENT_RIGHT_PADDLE))
        {
            e->vel = {};

            Entity *ball = &gameState->entities[2];

            // We are below the Ball, need to move UP
            if (e->origin.y > ball->origin.y)
            {
                e->vel.y = -speed * 0.9f;

                float yHeading = e->origin.y + e->vel.y * dt;
                float distanceYToBall = ball->origin.y - e->origin.y;

                if (yHeading < ball->origin.y)
                {
                    e->origin.y = ball->origin.y;
                }
                else
                {
                    e->origin.y = yHeading;
                }
            }

            // We are above the Ball, need to move DOWN
            if (e->origin.y < ball->origin.y)
            {
                e->vel.y = speed * 0.9f;

                float yHeading = e->origin.y + e->vel.y * dt;
                float distanceYToBall = ball->origin.y - e->origin.y;

                if (yHeading > ball->origin.y)
                {
                    e->origin.y = ball->origin.y;
                }
                else
                {
                    e->origin.y = yHeading;
                }
            }
        }

        if (has_component(e, COMPONENT_BALL))
        {
            // Right Side
            if (e->origin.x + e->boundingBox.offet.x + e->boundingBox.size.x > input->screenSize.x)
            {
                e->vel.x = -e->vel.x;

                e->origin.x -= 2 * (e->origin.x + e->boundingBox.offet.x + e->boundingBox.size.x - input->screenSize.x);

                gameState->score += (int)speed;
            }

            // Left Side
            if (e->origin.x + e->boundingBox.offet.x < 0.0f)
            {
                e->vel.x = -e->vel.x;

                e->origin.x -= 2 * (e->origin.x + e->boundingBox.offet.x);

                gameState->gameState = GAME_STATE_SCORE_SCREEN;
            }

            if (e->origin.y + e->boundingBox.offet.y + e->boundingBox.size.y > input->screenSize.y)
            {
                e->vel.y = -e->vel.y;

                e->origin.y -= 2 * (e->origin.y + e->boundingBox.offet.y + e->boundingBox.size.y - input->screenSize.y);
            }

            if (e->origin.y + e->boundingBox.offet.y < 0.0f)
            {
                e->vel.y = -e->vel.y;

                e->origin.y -= 2 * (e->origin.y + e->boundingBox.offet.y);
            }

            // Check for collision with the Paddles
            {
                Vec2 nextBallPos = e->origin + e->vel * dt;

                Entity *leftPaddle = &gameState->entities[0];
                Entity *rightPaddle = &gameState->entities[1];

                Rect leftPaddleBoundingBox = get_bounding_box(leftPaddle);
                Rect rightPaddleBoundingBox = get_bounding_box(rightPaddle);

                float leftBarrier = leftPaddleBoundingBox.pos.x + leftPaddleBoundingBox.size.x + e->boundingBox.size.x / 2.0f;
                float rightBarrier = rightPaddleBoundingBox.pos.x - e->boundingBox.size.x / 2.0f;

                if (e->origin.x > leftBarrier && e->origin.x < rightBarrier)
                {
                    // Left Barrier check
                    {
                        Vec2 collisionPoint = {};

                        if (line_intersection(e->origin, nextBallPos,
                                              Vec2{leftBarrier, leftPaddleBoundingBox.pos.y},
                                              Vec2{leftBarrier, leftPaddleBoundingBox.pos.y + leftPaddleBoundingBox.size.y},
                                              &collisionPoint))
                        {
                            e->vel.x = -e->vel.x;

                            // Remove the Penetration
                            nextBallPos.x += 2 * (leftBarrier - nextBallPos.x);
                        }
                    }

                    // Right Barrier check
                    {
                        Vec2 collisionPoint = {};

                        if (line_intersection(e->origin, nextBallPos,
                                              Vec2{rightBarrier, rightPaddleBoundingBox.pos.y},
                                              Vec2{rightBarrier, rightPaddleBoundingBox.pos.y + rightPaddleBoundingBox.size.y},
                                              &collisionPoint))
                        {
                            e->vel.x = -e->vel.x;

                            // Remove the Penetration
                            nextBallPos.x -= 2 * (nextBallPos.x - rightBarrier);
                        }
                    }
                }

                // Add Velocity
                e->origin = nextBallPos;
            }

            // Increase the Speed of the Ball
            {
                float speedUp = 25.0f;

                if (e->vel.x < 0.0f)
                {
                    e->vel.x -= dt * speedUp;
                }
                else
                {
                    e->vel.x += dt * speedUp;
                }

                if (e->vel.y < 0.0f)
                {
                    e->vel.y -= dt * speedUp;
                }
                else
                {
                    e->vel.y += dt * speedUp;
                }
            }
        }
    }
}

void update_game(GameState *gameState, InputState *input, UIState *ui, float dt)
{
    switch (gameState->gameState)
    {
    case GAME_STATE_MAIN_MENU:
    {
        do_button(ui, input, ASSET_SPRITE_PONG_FROM_SCRATCH_LOGO, line_id(1),
                  {0.0f, 0.0f, input->screenSize});

        ui->globalLayer++;
        if (do_button(ui, input, ASSET_SPRITE_BUTTON_64_16, line_id(1),
                      {400.0f, 400.0f, 196.0f, 48.0f}, "Play"))
        {
            gameState->gameState = GAME_STATE_RUNNING_LEVEL;
        }

        if (do_button(ui, input, ASSET_SPRITE_BUTTON_64_16, line_id(1),
                      {400.0f, 470.0f, 196.0f, 48.0f}, "Quit"))
        {
            platform_exit_application();
        }
        ui->globalLayer--;

        break;
    }

    case GAME_STATE_RUNNING_LEVEL:
        update_level(gameState, input, ui, dt);
        break;

    case GAME_STATE_SCORE_SCREEN:
    {
        do_button(ui, input, ASSET_SPRITE_PONG_FROM_SCRATCH_LOGO, line_id(1),
                  {0.0f, 0.0f, input->screenSize});

        // Score
        {
            do_text(ui, {400.0f, 350.0f}, "Score: ");
            do_number(ui, {495.0f, 350.0f}, gameState->score);
        }
        
        ui->globalLayer++;
        if (do_button(ui, input, ASSET_SPRITE_BUTTON_64_16, line_id(1),
                      {400.0f, 400.0f, 196.0f, 48.0f}, "Restart"))
        {
            gameState->gameTime = 0.0f;
            gameState->score = 0;
            gameState->entityCount = 0;

            init_game(gameState, input);

            gameState->gameState = GAME_STATE_RUNNING_LEVEL;
        }

        if (do_button(ui, input, ASSET_SPRITE_BUTTON_64_16, line_id(1),
                      {400.0f, 470.0f, 196.0f, 48.0f}, "Main Menu"))
        {
            gameState->gameTime = 0.0f;
            gameState->score = 0;
            gameState->entityCount = 0;

            init_game(gameState, input);

            gameState->gameState = GAME_STATE_MAIN_MENU;
        }
        ui->globalLayer--;
        
        break;
    }

    }
}