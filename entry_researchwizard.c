#define debug

#include "range.c"

typedef enum EntityArchetype
{
	ARCHETYPE_nil = 0,
	ARCHETYPE_slug = 1,
	ARCHETYPE_player = 2,
	ARCHETYPE_brownRock = 3,
} EntityArchetype;

typedef enum SpriteID
{
	SPRITE_nil,
	SPRITE_player,
	SPRITE_slug,
	SPRITE_MAX
} SpriteID;

typedef struct Entity
{
	bool isValid;
	EntityArchetype archetype;
	Vector2 position;

	bool renderSprite;
	SpriteID spriteId;
} Entity;

#define MAX_ENTITY_COUNT 1024
typedef struct World
{
	Entity entities[MAX_ENTITY_COUNT];
} World;

World *world = 0;

typedef struct Sprite
{
	Gfx_Image *Image;
	Vector2 size;
} Sprite;

Sprite sprites[SPRITE_MAX];
Sprite *get_sprite(SpriteID id)
{
	if (id < 0 || id >= SPRITE_MAX)
		return &sprites[0];

	return &sprites[id];
}

Entity *entity_create()
{
	for (int i = 0; i < MAX_ENTITY_COUNT; i++)
	{
		Entity *entity = &world->entities[i];
		if (entity->isValid)
		{
			continue;
		}
		entity->isValid = true;
		return entity;
	}
	assert(true, "no entity found");
	return null;
}

void entity_destroy(Entity *entity)
{
	memset(entity, 0, sizeof(Entity));
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

#ifdef debug
void debug_position(Vector2 entityPosition, Gfx_Font *font, const u32 font_height, Vector2 *printPosition, string prefix)
{
	string printStr = string_concat(prefix, STR(": %f %f"), get_temporary_allocator());
	printStr = sprint(get_temporary_allocator(), printStr, entityPosition.x, entityPosition.y);
	draw_text(font, printStr, font_height, v2(printPosition->x, printPosition->y), v2(0.1, 0.1), COLOR_WHITE);
}

void debug_projection(Matrix4 *position, Entity *entity, Gfx_Font *font, const u32 font_height)
{
	Matrix4 xform = *position;
	Matrix4 world_to_view = m4_mul(m4_scalar(1.0), m4_inverse(draw_frame.camera_xform));
	Matrix4 view_pos = m4_mul(world_to_view, xform);

	Matrix4 view_to_clip = m4_mul(m4_scalar(1.0), draw_frame.projection);
	Matrix4 clip_pos = m4_mul(view_to_clip, view_pos);

	Vector2 printPosition = v2(entity->position.x, entity->position.y - 0.1);
	debug_position(entity->position, font, font_height, &printPosition, STR("world pos "));
	printPosition.y -= 4.8;
	debug_position(v2(xform.m[0][3], xform.m[1][3]), font, font_height, &printPosition, STR("xform pos "));
	printPosition.y -= 4.8;
	debug_position(v2(view_pos.m[0][3], view_pos.m[1][3]), font, font_height, &printPosition, STR("view pos"));
	printPosition.y -= 4.8;
	debug_position(v2(clip_pos.m[0][3], clip_pos.m[1][3]), font, font_height, &printPosition, STR("view pos"));
}
#endif

int entry(int argc, char **argv)
{
	window.title = STR("wizrd");
	window.scaled_width = 1280; // We need to set the scaled size if we want to handle system scaling (DPI)
	window.scaled_height = 720;
	window.x = 200;
	window.y = 90;
	window.clear_color = hex_to_rgba(0x14093b);

	// load assets
	sprites[SPRITE_player] = (Sprite){.size = v2(7, 11), .Image = load_image_from_disk(fixed_string("../Assets/Player.png"), get_heap_allocator())};
	sprites[SPRITE_slug] = (Sprite){.size = v2(9, 6), .Image = load_image_from_disk(fixed_string("../Assets/slug.png"), get_heap_allocator())};
	Gfx_Font *font = load_font_from_disk(STR("C:/windows/fonts/arial.ttf"), get_heap_allocator());
	assert(font, "Failed loading arial.ttf");
	const u32 font_height = 48;

	world = alloc(get_heap_allocator(), sizeof(World));
	memset(world, 0, sizeof(World));

	Entity *entity_player = entity_create();
	setup_player(entity_player);
	Entity *entity_slug = entity_create();
	setup_slug(entity_slug);

	// game loop
	const float32 second = 1;
	float64 fpsTime = 0;
	float64 fps = 0;
	float64 last_time = os_get_elapsed_seconds();
	float64 delta_time = 0;
	float64 cameraZoom = 6.0f;
	while (!window.should_close)
	{
		float64 now = os_get_elapsed_seconds();
		delta_time = now - last_time;
		last_time = now;
		reset_temporary_storage();

		draw_frame.projection = m4_make_orthographic_projection(window.width * -0.5, window.width * 0.5, window.height * -0.5, window.height * 0.5, -1, 10);
		draw_frame.camera_xform = m4_make_scale(v3(1 / cameraZoom, 1 / cameraZoom, 1));
		Matrix4 clipToWorld = m4_scalar(1.0);
		clipToWorld = m4_mul(clipToWorld, draw_frame.camera_xform); // its inversed when we draw
		clipToWorld = m4_mul(clipToWorld, m4_inverse(draw_frame.projection));

		// input reading
		if (is_key_just_pressed(KEY_ESCAPE))
		{
			window.should_close = true;
		}

		Vector2 input_axis = v2(0, 0);
		if (is_key_down('A'))
		{
			input_axis.x -= 1.0;
		}
		if (is_key_down('D'))
		{
			input_axis.x += 1.0;
		}
		if (is_key_down('S'))
		{
			input_axis.y -= 1.0;
		}
		if (is_key_down('W'))
		{
			input_axis.y += 1.0;
		}
		input_axis = v2_normalize(input_axis);

		// tick player
		entity_player->position = v2_add(entity_player->position, v2_mulf(input_axis, 50 * delta_time));

		// tick slug
		float64 detectionDistance = 50;
		Vector2 vectorToPlayer = v2_sub(entity_player->position, entity_slug->position);
		if (v2_length(vectorToPlayer) <= detectionDistance) // could compare squared, but lazy
		{
			entity_slug->position = v2_add(entity_slug->position, v2_mulf(v2_normalize(vectorToPlayer), 15 * delta_time));
		}

		// collision
		Sprite *player_sprite = get_sprite(entity_player->spriteId);
		Range2f player_bounds = range2f_make_bottom_center(player_sprite->size);
		player_bounds = range2f_shift(player_bounds, entity_player->position);

		Sprite *slug_sprite = get_sprite(entity_slug->spriteId);
		Range2f slug_bounds = range2f_make_bottom_center(slug_sprite->size);
		slug_bounds = range2f_shift(slug_bounds, entity_slug->position);
		// todo: loop of multiple entities
		if (range2f_AABB(slug_bounds, player_bounds))
		{
			// todo: player take damage
		}

		// rendering
		for (int i = 0; i < MAX_ENTITY_COUNT; i++)
		{
			Entity *entity = &world->entities[i];
			if (!entity->isValid)
				continue;

			switch (entity->archetype)
			{
			case ARCHETYPE_player:
			{
				if (!entity->renderSprite)
					break;

				Sprite *sprite = get_sprite(entity->spriteId);
				Matrix4 xform = m4_scalar(1.0);
				xform = m4_translate(xform, v3(sprite->size.x * -0.5, 0, 0)); // centering
				xform = m4_translate(xform, v3(entity->position.x, entity->position.y, 0));
				draw_image_xform(sprite->Image, xform, sprite->size, COLOR_WHITE);
				// trynna understand
				// debug_projection(&xform, entity, font, font_height);
				break;
			}

			default:
			{
				if (!entity->renderSprite)
					break;

				Sprite *sprite = get_sprite(entity->spriteId);
				Matrix4 xform = m4_scalar(1.0);
				xform = m4_translate(xform, v3(sprite->size.x * -0.5, 0, 0));
				xform = m4_translate(xform, v3(entity->position.x, entity->position.y, 0));
				draw_image_xform(sprite->Image, xform, sprite->size, COLOR_WHITE);
			}
			}
		}

		os_update();
		gfx_update();

		// fps counter
		if (fpsTime > second)
		{
#ifdef debug
			log("%.2f FPS", fps);
#endif
			fps = 0;
			fpsTime -= second;
		}
		else
		{
			fpsTime += delta_time;
			fps++;
		}
	}
	return 0;
}
