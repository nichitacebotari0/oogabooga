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

    // sheet stuff
    bool isSheet;
    u32 anim_frame_width;
    u32 anim_frame_height;
    u32 number_of_columns;
    u32 number_of_rows;
    u32 anim_start_index;
    u32 anim_end_index;
    u32 anim_number_of_frames;
    float32 playback_fps;

} Sprite;

typedef struct SpriteUV
{
    Vector2 start;
    Vector2 end;
} SpriteUV;

/// Configure the animation by setting the start & end frames in the grid of frames
/// (Inspect sheet image and count the frame indices you want)
/// In sprite sheet animations, it usually goes down. So Y 0 is actuall the top of the
/// sprite sheet, and +Y is down on the sprite sheet.
void setup_spriteSheet(Sprite *sprite, u32 number_of_rows, u32 number_of_columns,
                       u32 anim_start_frame_row, u32 anim_start_frame_col,
                       u32 anim_end_frame_row, u32 anim_end_frame_col,
                       float32 playback_fps)
{
    sprite->isSheet = true;
    sprite->number_of_columns = number_of_columns;
    sprite->number_of_rows = number_of_rows;
    sprite->anim_frame_width = sprite->Image->width / number_of_columns;
    sprite->anim_frame_height = sprite->Image->height / number_of_rows;

    sprite->anim_start_index = anim_start_frame_row * number_of_columns + anim_start_frame_col;
    sprite->anim_end_index = anim_end_frame_row * number_of_columns + anim_end_frame_col;
    sprite->anim_number_of_frames = sprite->anim_end_index - sprite->anim_start_index + 1;
    sprite->playback_fps = playback_fps;

    // Sanity check configuration
    assert(sprite->anim_end_index > sprite->anim_start_index, "The last frame must come before the first frame");
    assert(anim_start_frame_col < number_of_columns, "anim_start_frame_x is out of bounds");
    assert(anim_start_frame_row < number_of_rows, "anim_start_frame_y is out of bounds");
    assert(anim_end_frame_col < number_of_columns, "anim_end_frame_x is out of bounds");
    assert(anim_end_frame_row < number_of_rows, "anim_end_frame_y is out of bounds");
}

SpriteUV get_sprite_animation_uv(Sprite *sprite, float32 animation_progress)
{
    assert(sprite->isSheet, "this is not an animation sheet sprite");
    Gfx_Image *anim_sheet = sprite->Image;

    float32 anim_time_per_frame = 1.0 / sprite->playback_fps;
    float32 anim_duration = anim_time_per_frame * (float32)sprite->anim_number_of_frames;
    float32 anim_elapsed = min(animation_progress, anim_duration - 0.001);
    // Get current progression in animation from 0.0 to 1.0
    float32 anim_progression_factor = anim_elapsed / anim_duration;

    u32 anim_current_index = sprite->anim_number_of_frames * anim_progression_factor;
    u32 anim_absolute_index_in_sheet = sprite->anim_start_index + anim_current_index;

    u32 anim_index_x = anim_absolute_index_in_sheet % sprite->number_of_columns;
    u32 anim_index_y = anim_absolute_index_in_sheet / sprite->number_of_columns + 1;

    u32 anim_sheet_pos_x = anim_index_x * sprite->anim_frame_width;
    u32 anim_sheet_pos_y = (sprite->number_of_rows - anim_index_y) * sprite->anim_frame_height; // Remember, Y inverted.

    // Draw the sprite sheet, with the uv box for the current frame.
    // Uv box is a Vector4 of x1, y1, x2, y2 where each value is a percentage value 0.0 to 1.0
    // from left to right / bottom to top in the texture.
    SpriteUV result = {
        .start = {.x = (float32)(anim_sheet_pos_x) / (float32)anim_sheet->width, .y = (float32)(anim_sheet_pos_y) / (float32)anim_sheet->height},
        .end = {.x = ((float32)(anim_sheet_pos_x + sprite->anim_frame_width) / (float32)anim_sheet->width), .y = ((float32)(anim_sheet_pos_y + sprite->anim_frame_height) / (float32)anim_sheet->height)}};

    return result;
}

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
