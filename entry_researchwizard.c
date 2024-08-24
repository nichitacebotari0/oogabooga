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
int entry(int argc, char **argv)
{
	window.title = STR("Minimal Game Example");
	window.scaled_width = 1280; // We need to set the scaled size if we want to handle system scaling (DPI)
	window.scaled_height = 720;
	window.x = 200;
	window.y = 90;
	window.clear_color = hex_to_rgba(0xffffffff);

	// load assets
	sprites[SPRITE_player] = (Sprite){.size = v2(7, 11), .Image = load_image_from_disk(fixed_string("../Assets/Player.png"), get_heap_allocator())};
	sprites[SPRITE_slug] = (Sprite){.size = v2(9, 6), .Image = load_image_from_disk(fixed_string("../Assets/slug.png"), get_heap_allocator())};

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
	while (!window.should_close)
	{
		float64 now = os_get_elapsed_seconds();
		delta_time = now - last_time;
		last_time = now;

		reset_temporary_storage();

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
		entity_player->position = v2_add(entity_player->position, v2_mulf(input_axis, 50 * delta_time));

		draw_frame.projection = m4_make_orthographic_projection(window.width * -0.5, window.width * 0.5, window.height * -0.5, window.height * 0.5, -1, 10);
		float64 zoom = 6.0f;
		draw_frame.camera_xform = m4_make_scale(v3(1 / zoom, 1 / zoom, 1));

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

		// fps countern
		if (fpsTime > second)
		{
			log("%.2f FPS", fps);
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