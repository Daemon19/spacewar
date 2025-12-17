#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "raylib.h"
#include "raymath.h"

// #define DRAW_HITBOX

#define MAX_PLAYER_BULLETS 3
#define MAX_POOL_BULLETS (MAX_PLAYER_BULLETS * 2)
#define LEFT_SHIP_TEXTURE_FILEPATH "assets/red-spaceship.png"
#define RIGHT_SHIP_TEXTURE_FILEPATH "assets/blue-spaceship.png"

typedef struct {
    int move_up;
    int move_down;
    int move_left;
    int move_right;
    int shoot;
} ShipKeyMap;

typedef struct {
    Vector2 position;
    Vector2 velocity;
    ShipKeyMap key_map;
    // Determines x clamp on the middle screen
    bool left_side;
    int bullet_count;
    Texture2D texture;
    int health;
} Ship;

typedef struct {
    Vector2 position;
    bool active;
    Ship *owner;
} Bullet;

typedef Bullet BulletPool[MAX_POOL_BULLETS];

const int WINDOW_WIDTH = 960;
const int WINDOW_HEIGHT = 540;
const int SCREEN_WIDTH = WINDOW_WIDTH / 2;
const int SCREEN_HEIGHT = WINDOW_HEIGHT / 2;

const int SHIP_WIDTH = 24;
const int SHIP_HEIGHT = 26;
const float SHIP_VELOCITY = 3.0f;
const int SHIP_HITBOX_WIDTH = 14;
const int SHIP_HITBOX_HEIGHT = 20;
const int SHIP_INITIAL_HEALTH = 3;
const int SHIP_HEALTH_X_OFF = 10;
const int SHIP_HEALTH_Y_OFF = 10;

const int BULLET_WIDTH = 12;
const int BULLET_HEIGHT = 1;
const float BULLET_VELOCITY = 10.0f;

RenderTexture2D screen;

Rectangle ShipGetHitbox(const Ship *ship)
{
    return (Rectangle){
        ship->position.x + SHIP_WIDTH / 2.0f - SHIP_HITBOX_WIDTH / 2.0f,
        ship->position.y + SHIP_HEIGHT / 2.0f - SHIP_HITBOX_HEIGHT / 2.0f,
        SHIP_HITBOX_WIDTH, SHIP_HITBOX_HEIGHT};
}

void BulletPoolAddBullet(BulletPool bullet_pool, Ship *owner)
{
    Bullet *bullet = 0;
    for (int i = 0; i < MAX_POOL_BULLETS; i++) {
        if (!bullet_pool[i].active) {
            bullet = &bullet_pool[i];
            break;
        }
    }

    assert(0 != bullet);

    bullet->active = true;
    bullet->position.x = (owner->left_side) ? owner->position.x + SHIP_WIDTH
                                            : owner->position.x - BULLET_WIDTH;
    bullet->position.y =
        owner->position.y + SHIP_HEIGHT / 2.0f - BULLET_HEIGHT / 2.0f;
    bullet->owner = owner;
}

void BulletDeactivate(Bullet *bullet)
{
    bullet->active = false;
    bullet->owner->bullet_count--;
}

void BulletPoolUpdateMovement(BulletPool bullet_pool)
{
    for (int i = 0; i < MAX_POOL_BULLETS; i++) {
        Bullet *bullet = &bullet_pool[i];
        if (!bullet->active) {
            continue;
        }

        bullet->position.x +=
            BULLET_VELOCITY * (bullet->owner->left_side ? 1 : -1);

        if (bullet->owner->left_side && bullet->position.x > SCREEN_WIDTH) {
            BulletDeactivate(bullet);
        } else if (!bullet->owner->left_side &&
                   bullet->position.x < -BULLET_WIDTH) {
            BulletDeactivate(bullet);
        }
    }
}

void BulletPoolDraw(BulletPool bullet_pool)
{
    for (int i = 0; i < MAX_POOL_BULLETS; i++) {
        Bullet *bullet = &bullet_pool[i];
        if (!bullet->active) {
            continue;
        }

        DrawRectangleV(bullet->position, (Vector2){BULLET_WIDTH, BULLET_HEIGHT},
                       RAYWHITE);
    }
}

int BulletPoolHandleCollisions(BulletPool bullet_pool, const Ship *shooter,
                               const Ship *target)
{
    int collision_count = 0;
    for (int i = 0; i < MAX_POOL_BULLETS; i++) {
        Bullet *bullet = &bullet_pool[i];
        if (!bullet->active || bullet->owner != shooter) {
            continue;
        }

        Rectangle bullet_rectangle = {bullet->position.x, bullet->position.y,
                                      BULLET_WIDTH, BULLET_HEIGHT};
        if (!CheckCollisionRecs(bullet_rectangle, ShipGetHitbox(target))) {
            continue;
        }

        BulletDeactivate(bullet);
        collision_count++;
    }
    return collision_count;
}

void ShipHandleMovement(Ship *ship)
{
    if (IsKeyDown(ship->key_map.move_up)) {
        ship->velocity.y = -SHIP_VELOCITY;
    } else if (IsKeyDown(ship->key_map.move_down)) {
        ship->velocity.y = SHIP_VELOCITY;
    } else {
        ship->velocity.y = 0;
    }

    if (IsKeyDown(ship->key_map.move_left)) {
        ship->velocity.x = -SHIP_VELOCITY;
    } else if (IsKeyDown(ship->key_map.move_right)) {
        ship->velocity.x = SHIP_VELOCITY;
    } else {
        ship->velocity.x = 0;
    }

    ship->position.x += ship->velocity.x;
    ship->position.y += ship->velocity.y;

    float left_bound = ship->left_side ? 0 : SCREEN_WIDTH / 2.0f;
    float right_bound =
        (ship->left_side ? SCREEN_WIDTH / 2.0f : SCREEN_WIDTH) - SHIP_WIDTH;
    ship->position.x = Clamp(ship->position.x, left_bound, right_bound);
    ship->position.y = Clamp(ship->position.y, 0, SCREEN_HEIGHT - SHIP_HEIGHT);
}

void ShipHandleShoot(Ship *ship, BulletPool bullet_pool)
{
    if (IsKeyPressed(ship->key_map.shoot) &&
        ship->bullet_count < MAX_PLAYER_BULLETS) {
        BulletPoolAddBullet(bullet_pool, ship);
        ship->bullet_count++;
    }
}

void ShipDraw(Ship *ship)
{
    DrawTextureV(ship->texture, ship->position, WHITE);

#ifdef DRAW_HITBOX
    DrawRectangleLinesEx(ShipGetHitbox(ship), 1.0f, RED);
#endif // NHITBOX
}

void ShipDrawHealth(const Ship *ship)
{
    char health_str[10];
    sprintf(health_str, "%d", ship->health);
    int health_width = MeasureText(health_str, 24);
    int health_x = ship->left_side
                       ? SHIP_HEALTH_X_OFF
                       : SCREEN_WIDTH - health_width - SHIP_HEALTH_X_OFF;
    DrawText(health_str, health_x, SHIP_HEALTH_Y_OFF, 24, RAYWHITE);
}

Texture2D ShipLoadTexture(const char *filename, int rotation_degree)
{
    Image image = LoadImage(filename);
    if (image.data == 0) {
        TraceLog(LOG_ERROR, "Failed to load texture: %s", filename);
        exit(1);
    }

    ImageRotate(&image, rotation_degree);
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    return texture;
}

int main(void)
{
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Space War");
    SetTargetFPS(60);

    Ship ship1 = {{0, 0},
                  {0, 0},
                  {KEY_W, KEY_S, KEY_A, KEY_D, KEY_SPACE},
                  true,
                  0,
                  ShipLoadTexture(LEFT_SHIP_TEXTURE_FILEPATH, 90),
                  SHIP_INITIAL_HEALTH};
    Ship ship2 = {{SCREEN_WIDTH * 0.75f, 0},
                  {0, 0},
                  {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_COMMA},
                  false,
                  0,
                  ShipLoadTexture(RIGHT_SHIP_TEXTURE_FILEPATH, -90),
                  SHIP_INITIAL_HEALTH};
    BulletPool bullet_pool = {0};
    screen = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);

    while (!WindowShouldClose()) {
        BulletPoolUpdateMovement(bullet_pool);

        ShipHandleMovement(&ship1);
        ShipHandleShoot(&ship1, bullet_pool);
        ShipHandleMovement(&ship2);
        ShipHandleShoot(&ship2, bullet_pool);

        int collision_count =
            BulletPoolHandleCollisions(bullet_pool, &ship1, &ship2);
        ship2.health -= collision_count;

        collision_count =
            BulletPoolHandleCollisions(bullet_pool, &ship2, &ship1);
        ship1.health -= collision_count;

        BeginTextureMode(screen);
        ClearBackground(BLACK);
        BulletPoolDraw(bullet_pool);
        ShipDraw(&ship1);
        ShipDraw(&ship2);
        ShipDrawHealth(&ship1);
        ShipDrawHealth(&ship2);
        DrawText("Hello Ray", 100, 100, 24, WHITE);
        EndTextureMode();

        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawTexturePro(
            screen.texture,
            (Rectangle){0, 0, (float)screen.texture.width,
                        (float)-screen.texture.height},
            (Rectangle){0, 0, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT},
            (Vector2){0, 0}, 0.0f, WHITE);
        EndDrawing();
    }

    UnloadTexture(ship1.texture);
    UnloadTexture(ship2.texture);
    UnloadRenderTexture(screen);
    return 0;
}
