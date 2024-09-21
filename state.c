#define MAX_ENTITY_COUNT 1024
typedef struct World World;

typedef enum GameMode
{
    GameMode_MainMenu,
    GameMode_InWorld
} GameMode;

typedef struct GameState
{
    GameMode GameMode;
    World *World;
    Matrix4 worldProjection;
    Matrix4 world_camera_xform;
} GameState;

struct World
{
    Entity entities[MAX_ENTITY_COUNT];

    // im super lazy so player state/config goes here
    float64 cameraZoom;
    Vector2 cameraPosition;
    // lmb
    float64 lmbCooldown;
    float64 lmbCooldownLeft;
    // dash
    float32 dashCooldown;
    float32 dashCooldownLeft;
    Vector2 dashInitialPosition;
    Vector2 dashTarget;
    float32 dashStartedAt;
    float32 dashDistance;
    float32 dashFlightDuration;
    float32 dashHighlight1Start;
    float32 dashHighlight2Start;
    float32 dashHighlightDuration;
    float32 dashDuration;
    float64 playerSpeed;
    float32 dragForce;
};

void setup_world(World *world)
{
    world->cameraZoom = 6.0f;
    world->cameraPosition = v2(0, 0);
    // lmb
    world->lmbCooldown = 0.3;
    world->lmbCooldownLeft = 0;
    // dash
    world->dashCooldown = 0.35;
    world->dashCooldownLeft = 0;
    world->dashInitialPosition = v2(1, 0);
    world->dashTarget = v2(1, 0);
    world->dashStartedAt = 0;
    world->dashDistance = 80;
    world->dashFlightDuration = 0.2;
    world->dashHighlight1Start = 0;
    world->dashHighlight2Start = 0.17;
    world->dashHighlightDuration = 0.05;
    world->dashDuration = world->dashHighlight2Start + world->dashHighlightDuration;
    world->playerSpeed = 900;
    world->dragForce = 8.3;
}