typedef enum EntityArchetype
{
    ARCHETYPE_nil = 0,
    ARCHETYPE_slug = 1,
    ARCHETYPE_player = 2,
    ARCHETYPE_projectile0 = 3,
    ARCHETYPE_brownRock = 4,
} EntityArchetype;

typedef enum SpriteID
{
    SPRITE_nil,
    SPRITE_player,
    SPRITE_slug,
    SPRITE_projectile0,
    SPRITE_MAX
} SpriteID;

typedef struct Entity
{
    bool isValid;
    EntityArchetype archetype;
    Vector2 position;
    Vector2 velocity; // dPosition
    // projectile stuff
    Vector2 direction;
    float32 lifetime_progress; // how long till isValid = false
    float32 lifetime_total;
    float32 projectile_acceleration;
    float32 projectile_knockback;
    easing_function *get_easing;

    // knockback effect
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
} Sprite;

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

void setup_projectile0(Entity *entity, Vector2 position, float32 lifetime, Vector2 direction, easing_function *easing_func, float32 speed, float32 knockback_strengh)
{
    entity->archetype = ARCHETYPE_projectile0;
    entity->isValid = true;
    entity->renderSprite = true;
    entity->spriteId = SPRITE_projectile0;
    entity->get_easing = easing_func;
    entity->lifetime_total = lifetime;
    entity->lifetime_progress = 0;
    entity->direction = v2_normalize(direction);
    entity->position = position;
    entity->projectile_acceleration = speed;
    entity->projectile_knockback = knockback_strengh;
}

bool is_projectile(Entity *entity)
{
    return entity->archetype == ARCHETYPE_projectile0;
}

float32 get_lifetime_progress(Entity *entity)
{
    float32 lifetime_progress = (entity->lifetime_total - entity->lifetime_progress) / entity->lifetime_total;
    return clamp(lifetime_progress, 0, 1);
}

// typedef struct MotionResult {
//     Vector2 Position;
//     Vector2 dPosition; // AKA velocity
// } A;

// motion equations
// newPosition =  1/2 * acceleration * delta_time^2 + oldVelocity * delta_time + oldPosition = 1/2*a*t^2 + v*t + p
// newVelocity = acceleration * delta_time + oldVelocity
// entity->position = v2_add(
// 	v2_add(
// 		v2_mulf(entity->direction, (1 / 2 * projectile_acceleration * pow(delta_time, 2))),
// 		v2_mulf(entity->velocity, delta_time)),
// 	entity->position);