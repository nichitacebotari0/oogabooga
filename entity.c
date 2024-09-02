typedef enum EntityArchetype
{
    ARCHETYPE_nil = 0,
    ARCHETYPE_slug = 1,
    ARCHETYPE_player = 2,
    ARCHETYPE_projectile = 3,
    ARCHETYPE_brownRock = 4,
} EntityArchetype;

typedef enum SpriteID
{
    SPRITE_nil,
    SPRITE_player,
    SPRITE_slug,
    SPRITE_projectile0,
    SPRITE_projectile0_sheet,
    SPRITE_MAX
} SpriteID;

typedef enum EntityState
{
    ENTITYSTATE_nil,

    // projectile states
    ENTITYSTATE_PROJECTILE_InFlight,
    ENTITYSTATE_PROJECTILE_Impact
} EntityState;

typedef struct Entity
{
    bool isValid;
    EntityArchetype archetype;
    Vector2 position;
    Vector2 velocity; // dPosition
    EntityState state;

    // projectile stuff
    Vector2 projectile_direction;
    float32 projectile_speed;
    float32 projectile_knockback;
    float32 projectile_flightLifetime_total;
    float32 projectile_flightLifetime_progress; // how long till isValid = false
    easing_function *get_projectile_flight_easing;
    float32 projectile_impactLifetime_total; // needed for animation of impact
    float32 projectile_impactLifetime_progress;

    // being hit
    float32 hitHighlightDurationLeft;

    // knockback effect to this entity
    float32 knockback_strengh;
    float32 knockback_durationLeft;
    Vector2 knockback_direction;

    bool renderSprite;
    SpriteID spriteId;

} Entity;

typedef struct Sprite
{
    Gfx_Image *Image;
    Vector2 size;
    bool isSheet;

} Sprite;

bool is_projectile(Entity *entity)
{
    return entity->archetype == ARCHETYPE_projectile;
}

void setup_player(Entity *entity)
{
    entity->archetype = ARCHETYPE_player;
    entity->renderSprite = true;
    entity->spriteId = SPRITE_player;
}

void setup_slug(Entity *entity)
{
    entity->archetype = ARCHETYPE_slug;
    entity->renderSprite = true;
    entity->spriteId = SPRITE_slug;
}

void setup_projectile0(Entity *projectile, SpriteID spriteId, Vector2 position, Vector2 projectile_direction,
                       float32 speed, float32 knockback_strengh, easing_function *flight_easing_func,
                       float32 flightLifetime, float32 impactLifetime)
{
    projectile->archetype = ARCHETYPE_projectile;
    projectile->isValid = true;
    projectile->renderSprite = true;
    projectile->spriteId = spriteId;
    projectile->state = ENTITYSTATE_PROJECTILE_InFlight;

    projectile->position = position;
    projectile->projectile_direction = v2_normalize(projectile_direction);
    projectile->projectile_flightLifetime_total = flightLifetime;
    projectile->projectile_flightLifetime_progress = 0;
    projectile->projectile_impactLifetime_total = impactLifetime;
    projectile->projectile_impactLifetime_progress = 0;
    projectile->get_projectile_flight_easing = flight_easing_func;

    projectile->projectile_speed = speed;
    projectile->projectile_knockback = knockback_strengh;
}

float32 get_lifetime_progress(Entity *entity)
{
    float32 lifetime_progress = (entity->projectile_flightLifetime_total - entity->projectile_flightLifetime_progress) / entity->projectile_flightLifetime_total;
    return clamp(lifetime_progress, 0, 1);
}

// typedef struct MotionResult {
//     Vector2 Position;
//     Vector2 dPosition; // AKA velocity
// } A;
