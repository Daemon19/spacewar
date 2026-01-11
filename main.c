#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

// #define DRAW_HITBOX

#define MAX_PLAYER_BULLETS 3
#define MAX_POOL_BULLETS (MAX_PLAYER_BULLETS * 2)
#define LEFT_SHIP_TEXTURE_FILEPATH "assets/red-spaceship.png"
#define RIGHT_SHIP_TEXTURE_FILEPATH "assets/blue-spaceship.png"
#define SHOOT_SFX_FILEPATH "assets/shoot-sfx.wav"
#define HIT_SFX_FILEPATH "assets/hit-sfx.wav"
#define WIN_SFX_FILEPATH "assets/win-sfx.wav"
#define PAUSE_SFX_FILEPATH "assets/pause-sfx.wav"
#define BACKGROUND_MUSIC_FILEPATH "assets/background-music.ogg"
#define PAUSE_ICON_FILEPATH "assets/pause-icon.png"
#define WINDOW_ICON_FILEPATH "assets/window-icon.png"

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
    bool left_side;
    int bullet_count;
    Texture2D texture;
    int health;
} Ship;

typedef struct {
    Vector2 position;
    Vector2 last_position;
    bool active;
    Ship *owner;
} Bullet;

typedef enum {
    NONE,
    LEFT,
    RIGHT,
    DRAW,
} Winner;

typedef Bullet BulletPool[MAX_POOL_BULLETS];

typedef struct {
    Vector2 center;
    float font_size;
    Vector2 padding;
    Color background_color;
    Color text_color;
    const char *text;
} TextButton;

typedef struct {
    Vector2 center;
    Vector2 padding;
    float scale;
    Texture2D texture;
} TextureButton;

typedef struct {
    enum ContentType { TEXT, TEXTURE } content_type;
    union Content {
        TextButton text;
        TextureButton texture;
    } content;
} Button;

typedef struct {
    Rectangle play_button;
    Button exit_button;
} MainMenuGui;

typedef struct {
    Button pause_button;
} PlayingGui;

typedef struct {
    TextButton resume_button;
    TextButton main_menu_button;
} PauseGui;

typedef struct {
    Rectangle play_again_button;
    Rectangle exit_button;
} WinGui;

typedef struct {
    MainMenuGui main_menu_gui;
    PlayingGui playing_gui;
    PauseGui pause_gui;
    WinGui win_gui;
} Gui;

typedef struct {
    Ship ship1;
    Ship ship2;
    BulletPool bullet_pool;
    Winner winner;

    Sound shoot_sfx;
    Sound hit_sfx;
    Sound win_sfx;
    Sound pause_sfx;
    Music background_music;

    Gui gui;
} Game;

typedef struct GameState {
    void (*Init)(Game *game);
    struct GameState *(*Update)(Game *game, float deltatime);
    void (*Draw)(const Game *game);
} GameState;

const int SCREEN_WIDTH = 480;
const int SCREEN_HEIGHT = 270;
const Vector2 SCREEN_HALF = {SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f};
const int INITIAL_SCREEN_SCALE = 2;
const int INITIAL_WINDOW_WIDTH = SCREEN_WIDTH * INITIAL_SCREEN_SCALE;
const int INITIAL_WINDOW_HEIGHT = SCREEN_HEIGHT * INITIAL_SCREEN_SCALE;

const int SHIP_WIDTH = 24;
const int SHIP_HEIGHT = 26;
const float SHIP_VELOCITY = 180.0f;
const int SHIP_HITBOX_WIDTH = 14;
const int SHIP_HITBOX_HEIGHT = 20;
const int SHIP_INITIAL_HEALTH = 3;
const int SHIP_HEALTH_X_OFF = 10;
const int SHIP_HEALTH_Y_OFF = 10;

const int BULLET_WIDTH = 12;
const int BULLET_HEIGHT = 1;
const float BULLET_VELOCITY = 600.0f;

const float WIN_FONT_SIZE = 64.0f;
const float DEFAULT_LETTER_SPACING = 1.0f;
const Color PAUSE_DIM_COLOR = (Color){0, 0, 0, 170};

GameState main_menu_state;
GameState playing_state;
GameState pause_state;
GameState win_state;

Vector2 GetMousePositionOnScreen(void)
{
    Vector2 mouse = GetMousePosition();
    return (Vector2){Remap(mouse.x, 0, GetScreenWidth(), 0, SCREEN_WIDTH),
                     Remap(mouse.y, 0, GetScreenHeight(), 0, SCREEN_HEIGHT)};
}

Rectangle CreateRectangleFromCenter(float centerx, float centery, float width,
                                    float height)
{
    return (Rectangle){centerx - width / 2.0f, centery - height / 2.0f, width,
                       height};
}

void DrawTextCenter(const char *string, Vector2 center, float font_size,
                    float letter_spacing, Color color)
{
    Vector2 text_size =
        MeasureTextEx(GetFontDefault(), string, font_size, letter_spacing);
    Vector2 topleft = Vector2Subtract(center, Vector2Scale(text_size, 0.5f));
    DrawTextEx(GetFontDefault(), string, topleft, font_size, letter_spacing,
               color);
}

Rectangle GetTextButtonRectangle(const TextButton *button)
{
    Vector2 size = MeasureTextEx(GetFontDefault(), button->text,
                                 button->font_size, DEFAULT_LETTER_SPACING);
    // Times 2 for 2 direction padding
    Vector2 extra = Vector2Scale(button->padding, 2.0f);
    size = Vector2Add(size, extra);
    Rectangle rectangle = CreateRectangleFromCenter(
        button->center.x, button->center.y, size.x, size.y);
    return rectangle;
}

Rectangle GetTextureButtonRectangle(const TextureButton *button)
{
    Vector2 size = {button->texture.width, button->texture.height};
    // Times 2 for 2 direction padding
    Vector2 extra = Vector2Scale(button->padding, 2.0f);
    size = Vector2Add(size, extra);
    Rectangle rectangle = CreateRectangleFromCenter(
        button->center.x, button->center.y, size.x, size.y);
    return rectangle;
}

Rectangle GetButtonRectangle(const Button *button)
{
    Rectangle rectangle = {0};
    switch (button->content_type) {
    case TEXT:
        rectangle = GetTextButtonRectangle(&button->content.text);
        break;
    case TEXTURE:
        rectangle = GetTextureButtonRectangle(&button->content.texture);
        break;
    default:
        assert(!"Invalid button ContentType");
        break;
    }
    return rectangle;
}

void DrawTextButton(const TextButton *button)
{
    Rectangle rectangle = GetTextButtonRectangle(button);
    DrawRectangleRec(rectangle, button->background_color);
    DrawTextCenter(button->text, button->center, button->font_size,
                   DEFAULT_LETTER_SPACING, button->text_color);
}

void DrawTextureButton(const TextureButton *button)
{
    Vector2 offset = {button->texture.width, button->texture.height};
    offset = Vector2Scale(offset, 0.5f * button->scale);
    Vector2 topleft = button->center;
    topleft = Vector2Subtract(topleft, offset);
    DrawTextureEx(button->texture, topleft, 0.0f, button->scale, WHITE);
}

void DrawButton(const Button *button)
{
    switch (button->content_type) {
    case TEXT:
        DrawTextButton(&button->content.text);
        break;
    case TEXTURE:
        DrawTextureButton(&button->content.texture);
        break;
    default:
        assert(!"Invalid button ContentType");
        break;
    }

#ifdef DRAW_HITBOX
    DrawRectangleLinesEx(GetButtonRectangle(button), 1.0f, RED);
#endif /* ifdef DRAW_HITBOX */
}

bool RectangleCheckPressed(Rectangle rectangle)
{
    return IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
           CheckCollisionPointRec(GetMousePositionOnScreen(), rectangle);
}

Vector2 RectangleGetCenter(Rectangle rectangle)
{
    return (Vector2){rectangle.x + rectangle.width / 2.0f,
                     rectangle.y + rectangle.height / 2.0f};
}

bool ButtonCheckPressed(const Button *button)
{
    Rectangle rectangle = GetButtonRectangle(button);
    bool pressed = RectangleCheckPressed(rectangle);
    return pressed;
}

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
    bullet->last_position = bullet->position;
    bullet->owner = owner;
}

void BulletDeactivate(Bullet *bullet)
{
    bullet->active = false;
    bullet->owner->bullet_count--;
}

void BulletPoolUpdateMovement(BulletPool bullet_pool, float deltatime)
{
    for (int i = 0; i < MAX_POOL_BULLETS; i++) {
        Bullet *bullet = &bullet_pool[i];
        if (!bullet->active) {
            continue;
        }

        bullet->last_position = bullet->position;
        bullet->position.x +=
            BULLET_VELOCITY * deltatime * (bullet->owner->left_side ? 1 : -1);

        if (bullet->owner->left_side && bullet->position.x > SCREEN_WIDTH) {
            BulletDeactivate(bullet);
        } else if (!bullet->owner->left_side &&
                   bullet->position.x < -BULLET_WIDTH) {
            BulletDeactivate(bullet);
        }
    }
}

void BulletPoolDraw(const BulletPool bullet_pool)
{
    for (int i = 0; i < MAX_POOL_BULLETS; i++) {
        const Bullet *bullet = &bullet_pool[i];
        if (!bullet->active) {
            continue;
        }

        DrawRectangleV(bullet->position, (Vector2){BULLET_WIDTH, BULLET_HEIGHT},
                       RAYWHITE);
    }
}

// Returns bullet's collision rectangle based on its movement
Rectangle BulletGetCollisionRectangle(const Bullet *bullet)
{
    float x = fmin(bullet->position.x, bullet->last_position.x);
    float dx = fabs(bullet->position.x - bullet->last_position.x);
    // Bullet only move horizontally
    float y = bullet->position.y;
    float dy = BULLET_HEIGHT;
    Rectangle collision_rectangle = {x, y, dx, dy};
    return collision_rectangle;
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

        if (!CheckCollisionRecs(BulletGetCollisionRectangle(bullet),
                                ShipGetHitbox(target))) {
            continue;
        }

        BulletDeactivate(bullet);
        collision_count++;
    }
    return collision_count;
}

void ShipHandleMovement(Ship *ship, float deltatime)
{
    if (IsKeyDown(ship->key_map.move_up)) {
        ship->velocity.y = -1;
    } else if (IsKeyDown(ship->key_map.move_down)) {
        ship->velocity.y = 1;
    } else {
        ship->velocity.y = 0;
    }

    if (IsKeyDown(ship->key_map.move_left)) {
        ship->velocity.x = -1;
    } else if (IsKeyDown(ship->key_map.move_right)) {
        ship->velocity.x = 1;
    } else {
        ship->velocity.x = 0;
    }

    ship->velocity = Vector2Normalize(ship->velocity);
    ship->velocity = Vector2Scale(ship->velocity, SHIP_VELOCITY * deltatime);

    ship->position.x += ship->velocity.x;
    ship->position.y += ship->velocity.y;

    float left_bound = ship->left_side ? 0 : SCREEN_WIDTH / 2.0f;
    float right_bound =
        (ship->left_side ? SCREEN_WIDTH / 2.0f : SCREEN_WIDTH) - SHIP_WIDTH;
    ship->position.x = Clamp(ship->position.x, left_bound, right_bound);
    ship->position.y = Clamp(ship->position.y, 0, SCREEN_HEIGHT - SHIP_HEIGHT);
}

bool ShipHandleShoot(Ship *ship, BulletPool bullet_pool)
{
    bool shooting = IsKeyPressed(ship->key_map.shoot) &&
                    ship->bullet_count < MAX_PLAYER_BULLETS;
    if (shooting) {
        BulletPoolAddBullet(bullet_pool, ship);
        ship->bullet_count++;
    }
    return shooting;
}

void ShipDraw(const Ship *ship)
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

void DrawWinDialog(Winner winner)
{
    assert(winner != NONE);

    const char *win_str;
    Color text_color;
    if (LEFT == winner) {
        win_str = "Red Wins!";
        text_color = RED;
    } else if (RIGHT == winner) {
        win_str = "Blue Wins!";
        text_color = BLUE;
    } else {
        win_str = "Draw.";
        text_color = WHITE;
    }
    const float y_offset = -50.0f;
    DrawTextCenter(
        win_str,
        (Vector2){SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f + y_offset},
        WIN_FONT_SIZE, DEFAULT_LETTER_SPACING, text_color);
}

void DrawWinButtons(const Gui *gui)
{
    DrawRectangleRec(gui->win_gui.play_again_button, WHITE);
    DrawTextCenter("PLAY AGAIN",
                   RectangleGetCenter(gui->win_gui.play_again_button), 24.0f,
                   DEFAULT_LETTER_SPACING, BLACK);
    DrawRectangleRec(gui->win_gui.exit_button, WHITE);
    DrawTextCenter("EXIT", RectangleGetCenter(gui->win_gui.exit_button), 24.0f,
                   DEFAULT_LETTER_SPACING, BLACK);
}

void GameReset(Game *game)
{
    game->ship1 =
        (Ship){.position = {SCREEN_HALF.x * 0.5f - SHIP_WIDTH / 2.0f,
                            SCREEN_HALF.y * 0.5f - SHIP_HEIGHT / 2.0f},
               .velocity = {0, 0},
               .key_map = {KEY_W, KEY_S, KEY_A, KEY_D, KEY_SPACE},
               .left_side = true,
               .bullet_count = 0,
               .texture = ShipLoadTexture(LEFT_SHIP_TEXTURE_FILEPATH, 90),
               .health = SHIP_INITIAL_HEALTH};

    game->ship2 =
        (Ship){.position = {SCREEN_HALF.x * 1.5f - SHIP_WIDTH / 2.0f,
                            SCREEN_HALF.y * 1.5f - SHIP_HEIGHT / 2.0f},
               .velocity = {0, 0},
               .key_map = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_COMMA},
               .left_side = false,
               .bullet_count = 0,
               .texture = ShipLoadTexture(RIGHT_SHIP_TEXTURE_FILEPATH, -90),
               .health = SHIP_INITIAL_HEALTH};

    size_t bullet_pool_bytes = MAX_POOL_BULLETS * sizeof(game->bullet_pool[0]);
    memset(game->bullet_pool, 0, bullet_pool_bytes);

    SeekMusicStream(game->background_music, 0.0f);
    game->winner = NONE;
}

void GameLoadSounds(Game *game)
{
    game->shoot_sfx = LoadSound(SHOOT_SFX_FILEPATH);
    game->hit_sfx = LoadSound(HIT_SFX_FILEPATH);
    game->win_sfx = LoadSound(WIN_SFX_FILEPATH);
    game->pause_sfx = LoadSound(PAUSE_SFX_FILEPATH);
    game->background_music = LoadMusicStream(BACKGROUND_MUSIC_FILEPATH);

    SetSoundVolume(game->shoot_sfx, 0.5f);
    SetSoundVolume(game->hit_sfx, 0.5f);
    SetSoundVolume(game->win_sfx, 0.3f);
    SetSoundVolume(game->pause_sfx, 0.3f);
    SetMusicVolume(game->background_music, 0.3f);

    game->background_music.looping = true;
}

void GameInitGui(Game *game)
{
    Texture2D pause_icon = LoadTexture(PAUSE_ICON_FILEPATH);
    game->gui = (Gui){
        .main_menu_gui =
            {.play_button = CreateRectangleFromCenter(
                 SCREEN_HALF.x, SCREEN_HALF.y + 20.0f, 150.0f, 30.0f),
             .exit_button = {TEXT,
                             {.text = {{SCREEN_HALF.x, SCREEN_HALF.y + 70.0f},
                                       24.0f,
                                       {30.0f, 2.0f},
                                       WHITE,
                                       BLACK,
                                       "EXIT"}}}},
        .playing_gui = {.pause_button = {.content_type = TEXTURE,
                                         .content = {.texture = {{SCREEN_HALF.x,
                                                                  19.0f},
                                                                 {0.0f, 0.0f},
                                                                 0.5f,
                                                                 pause_icon}}}},
        .pause_gui = {.resume_button = {{SCREEN_HALF.x, SCREEN_HALF.y + 20.0f},
                                        24.0f,
                                        {10.0f, 4.0f},
                                        WHITE,
                                        BLACK,
                                        "RESUME"},
                      .main_menu_button = {{SCREEN_HALF.x,
                                            SCREEN_HALF.y + 70.0f},
                                           24.0f,
                                           {10.0f, 4.0f},
                                           WHITE,
                                           BLACK,
                                           "MAIN MENU"}},
        .win_gui = {.play_again_button = CreateRectangleFromCenter(
                        SCREEN_HALF.x, SCREEN_HALF.y + 20.0f, 150.0f, 30.0f),
                    .exit_button = CreateRectangleFromCenter(
                        SCREEN_HALF.x, SCREEN_HALF.y + 70.0f, 100.0f, 30.0f)}};
}

void GameInit(Game *game)
{
    GameLoadSounds(game);
    GameReset(game);
    GameInitGui(game);
}

void GameDeinit(Game *game)
{
    UnloadTexture(game->ship1.texture);
    UnloadTexture(game->ship2.texture);
    UnloadTexture(game->gui.playing_gui.pause_button.content.texture.texture);
    UnloadSound(game->shoot_sfx);
    UnloadSound(game->hit_sfx);
    UnloadSound(game->win_sfx);
    UnloadMusicStream(game->background_music);
}

GameState *MainMenuStateUpdate(Game *game, float deltatime)
{
    (void)deltatime;
    if (WindowShouldClose() ||
        ButtonCheckPressed(&game->gui.main_menu_gui.exit_button)) {
        return NULL;
    }
    if (RectangleCheckPressed(game->gui.main_menu_gui.play_button)) {
        return &playing_state;
    }
    return &main_menu_state;
}

void MainMenuStateDraw(const Game *game)
{
    ClearBackground(BLACK);
    DrawTextCenter("SPACEWAR", (Vector2){SCREEN_HALF.x, SCREEN_HALF.y - 50.0f},
                   48.0f, 5.0f, WHITE);
    DrawRectangleRec(game->gui.main_menu_gui.play_button, WHITE);
    DrawTextCenter("PLAY",
                   RectangleGetCenter(game->gui.main_menu_gui.play_button),
                   24.0f, DEFAULT_LETTER_SPACING, RED);
    DrawButton(&game->gui.main_menu_gui.exit_button);
}

void PlayingStateInit(Game *game) { PlayMusicStream(game->background_music); }

void PlayingStateDraw(const Game *game)
{
    ClearBackground(BLACK);
    DrawText("Hello Bup :3", 100, 100, 24, (Color){255, 255, 255, 4});
    BulletPoolDraw(game->bullet_pool);
    ShipDraw(&game->ship1);
    ShipDraw(&game->ship2);
    ShipDrawHealth(&game->ship1);
    ShipDrawHealth(&game->ship2);
    DrawButton(&game->gui.playing_gui.pause_button);
}

GameState *PlayingStateUpdate(Game *game, float deltatime)
{
    // Unpacking Game struct
    Ship *ship1 = &game->ship1;
    Ship *ship2 = &game->ship2;
    BulletPool *bullet_pool = &game->bullet_pool;
    Winner *winner = &game->winner;
    Sound *shoot_sfx = &game->shoot_sfx;
    Sound *hit_sfx = &game->hit_sfx;
    Music *background_music = &game->background_music;

    if (WindowShouldClose()) {
        return NULL;
    }
    if (IsKeyPressed(KEY_ESCAPE) ||
        ButtonCheckPressed(&game->gui.playing_gui.pause_button)) {
        return &pause_state;
    }

    BulletPoolUpdateMovement(*bullet_pool, deltatime);

    ShipHandleMovement(ship1, deltatime);
    if (ShipHandleShoot(ship1, *bullet_pool)) {
        PlaySound(*shoot_sfx);
    }
    ShipHandleMovement(ship2, deltatime);
    if (ShipHandleShoot(ship2, *bullet_pool)) {
        PlaySound(*shoot_sfx);
    }

    int collision_count =
        BulletPoolHandleCollisions(*bullet_pool, ship1, ship2);
    if (collision_count) {
        PlaySound(*hit_sfx);
    }
    ship2->health =
        (ship2->health < collision_count) ? 0 : ship2->health - collision_count;

    collision_count = BulletPoolHandleCollisions(*bullet_pool, ship2, ship1);
    if (collision_count) {
        PlaySound(*hit_sfx);
    }
    ship1->health =
        (ship1->health < collision_count) ? 0 : ship1->health - collision_count;

    if (0 == ship1->health && 0 == ship2->health) {
        *winner = DRAW;
    } else if (0 == ship1->health) {
        *winner = RIGHT;
    } else if (0 == ship2->health) {
        *winner = LEFT;
    }

    if (NONE != *winner) {
        return &win_state;
    }

    UpdateMusicStream(*background_music);

    return &playing_state;
}

void PauseStateInit(Game *game) { PlaySound(game->pause_sfx); }

GameState *PauseStateUpdate(Game *game, float deltatime)
{
    (void)deltatime;
    if (WindowShouldClose()) {
        return NULL;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        return &playing_state;
    }
    if (RectangleCheckPressed(
            GetTextButtonRectangle(&game->gui.pause_gui.resume_button))) {
        return &playing_state;
    }
    if (RectangleCheckPressed(
            GetTextButtonRectangle(&game->gui.pause_gui.main_menu_button))) {
        GameReset(game);
        return &main_menu_state;
    }
    return &pause_state;
}

void DimScreen(Color color)
{
    rlSetBlendFactorsSeparate(0x0302, 0x0303, 1, 0x0303, 0x8006, 0x8006);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    DrawRectangle(0, 0, INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, color);
    EndBlendMode();
}

void PauseStateDraw(const Game *game)
{
    PlayingStateDraw(game);

    DimScreen(PAUSE_DIM_COLOR);
    DrawTextCenter("PAUSED", (Vector2){SCREEN_HALF.x, SCREEN_HALF.y - 50.0f},
                   64.0f, DEFAULT_LETTER_SPACING, WHITE);
    DrawTextButton(&game->gui.pause_gui.resume_button);
    DrawTextButton(&game->gui.pause_gui.main_menu_button);
}

void WinStateInit(Game *game) { PlaySound(game->win_sfx); }

GameState *WinStateUpdate(Game *game, float deltatime)
{
    (void)deltatime;
    if (WindowShouldClose()) {
        return NULL;
    }
    if (RectangleCheckPressed(game->gui.win_gui.play_again_button)) {
        GameReset(game);
        return &playing_state;
    }
    if (RectangleCheckPressed(game->gui.win_gui.exit_button)) {
        return NULL;
    }
    return &win_state;
}

void WinStateDraw(const Game *game)
{
    PlayingStateDraw(game);
    DrawWinDialog(game->winner);
    DrawWinButtons(&game->gui);
}

void EmptyStateInit(Game *game) { (void)game; }

void GameStatesInit(void)
{
    main_menu_state = (GameState){.Init = &EmptyStateInit,
                                  .Update = &MainMenuStateUpdate,
                                  .Draw = &MainMenuStateDraw};

    playing_state = (GameState){.Init = &PlayingStateInit,
                                .Update = &PlayingStateUpdate,
                                .Draw = &PlayingStateDraw};

    pause_state = (GameState){.Init = &PauseStateInit,
                              .Update = &PauseStateUpdate,
                              .Draw = &PauseStateDraw};

    win_state = (GameState){.Init = &WinStateInit,
                            .Update = &WinStateUpdate,
                            .Draw = &WinStateDraw};
}

// Screen's draw destination is the biggest centered rectangle following screen
// size ratio
Rectangle CreateScreenDrawDestination(void)
{
    float width = GetScreenWidth();
    float height = GetScreenHeight();
    if (height > width * SCREEN_HEIGHT / SCREEN_WIDTH) {
        height = width * SCREEN_HEIGHT / SCREEN_WIDTH;
    } else {
        width = height * SCREEN_WIDTH / SCREEN_HEIGHT;
    }
    Rectangle result = CreateRectangleFromCenter(
        GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f, width, height);
    return result;
}

void DrawScreenToWindow(RenderTexture2D screen)
{
    BeginDrawing();
    ClearBackground(BLACK);
    Rectangle source = {0, 0, (float)screen.texture.width,
                        (float)-screen.texture.height};
    Rectangle destination = CreateScreenDrawDestination();
    DrawTexturePro(screen.texture, source, destination, (Vector2){0, 0}, 0.0f,
                   WHITE);
#ifdef DRAW_FPS
    DrawFPS(0, 0);
#endif /* ifdef DRAW_FPS */

    EndDrawing();
}

void SetFullscreen(bool fullscreen)
{
    int new_width = INITIAL_WINDOW_WIDTH;
    int new_height = INITIAL_WINDOW_HEIGHT;
    if (fullscreen) {
        new_width = GetMonitorWidth(GetCurrentMonitor());
        new_height = GetMonitorHeight(GetCurrentMonitor());
    }
    SetWindowSize(new_width, new_height);
    ToggleFullscreen();
}

int main(void)
{
    SetTraceLogLevel(LOG_WARNING);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, "Space War");
    SetTargetFPS(GetMonitorRefreshRate(GetCurrentMonitor()));
    InitAudioDevice();
    SetExitKey(KEY_NULL);

    Image window_icon = LoadImage(WINDOW_ICON_FILEPATH);
    SetWindowIcon(window_icon);
    UnloadImage(window_icon);

    RenderTexture2D screen = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
    Game game = {0};
    GameInit(&game);

    GameStatesInit();
    GameState *current_state = &main_menu_state;
    GameState *previous_state = NULL;

    while (NULL != current_state) {
        if (IsKeyPressed(KEY_F11)) {
            SetFullscreen(!IsWindowFullscreen());
        }

        // Run game state initialization function on state change
        if (previous_state != current_state) {
            current_state->Init(&game);
            previous_state = current_state;
        }

        BeginTextureMode(screen);
        current_state->Draw(&game);
        EndTextureMode();

        DrawScreenToWindow(screen);

        float deltatime = GetFrameTime();
        current_state = current_state->Update(&game, deltatime);
    }

    GameDeinit(&game);
    UnloadRenderTexture(screen);
    return 0;
}
