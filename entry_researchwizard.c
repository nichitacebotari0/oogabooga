#define debug

#include "easings.c"
#include "range.c"
#include "entity.c"

#define MAX_ENTITY_COUNT 1024
typedef struct World
{
	Entity entities[MAX_ENTITY_COUNT];
} World;
World *world = 0;

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

void add_knocknack(Entity *entity, float32 strengh, float32 duration, Vector2 direction)
{
	if (fabsf(direction.x) < 0.01 && fabsf(direction.y) < 0.01)
		return;
	entity->knockback_strengh = strengh;
	entity->knockback_durationLeft = duration;
	entity->knockback_direction = v2_normalize(direction);
}

void tick_knockback(Entity *entity, float64 delta_time)
{
	entity->position = v2_add(entity->position, v2_mulf(entity->knockback_direction, entity->knockback_strengh * delta_time));
	entity->knockback_durationLeft -= delta_time;
	entity->knockback_durationLeft = max(0, entity->knockback_durationLeft);
}

// shader stuff
#define SHADER_EFFECT_COLORCHANGE 1
// BEWARE std140 packing:
// https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-packing-rules
typedef struct My_Cbuffer
{
	// Vector2 mouse_pos_screen; // We use this to make a light around the mouse cursor
	Vector2 window_size; // We only use this to revert the Y in the shader because for some reason d3d11 inverts it.
} My_Cbuffer;
Draw_Quad *draw_image_xform_hit(Gfx_Image *image, Matrix4 xform, Vector2 size, Vector4 color);

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
	window.point_width = 1280; // We need to set the scaled size if we want to handle system scaling (DPI)
	window.point_height = 720;
	window.x = 0;
	window.y = 0;
	window.width = 1280;
	window.height = 720;
	window.force_topmost = false;
	window.fullscreen = false;
	window.clear_color = hex_to_rgba(0x14093b);

	// load assets
	sprites[SPRITE_player] = (Sprite){.size = v2(7, 11), .Image = load_image_from_disk(fixed_string("../Assets/Player.png"), get_heap_allocator())};
	sprites[SPRITE_slug] = (Sprite){.size = v2(9, 6), .Image = load_image_from_disk(fixed_string("../Assets/slug.png"), get_heap_allocator())};
	sprites[SPRITE_projectile0] = (Sprite){.size = v2(7, 7), .Image = load_image_from_disk(fixed_string("../Assets/projectile0.png"), get_heap_allocator())};
	sprites[SPRITE_projectile0_sheet] = (Sprite){.size = v2(28, 7), .Image = load_image_from_disk(fixed_string("../Assets/projectile0_sheet.png"), get_heap_allocator())};
	assert(sprites[SPRITE_projectile0_sheet].Image, "load");

	Gfx_Font *font = load_font_from_disk(STR("C:/windows/fonts/arial.ttf"), get_heap_allocator());
	assert(font, "Failed loading arial.ttf");
	const u32 font_height = 48;
// shader
#pragma region shaderinit
	string source;
	bool ok = os_read_entire_file("shader.hlsl", &source, get_heap_allocator());
	assert(ok, "Could not read oogabooga/examples/custom_shader.hlsl");
	// This memory needs to stay alive throughout the frame because we pass the pointer to it in draw_frame.cbuffer.
	// If this memory is invalidated before gfx_update after setting draw_frame.cbuffer, then gfx_update will copy
	// memory from an invalid address.
	My_Cbuffer cbuffer;
	// This is slow and needs to recompile the shader. However, it should probably only happen once (or each hot reload)
	// If it fails, it will return false and return to whatever shader it was before.
	gfx_shader_recompile_with_extension(source, sizeof(My_Cbuffer));
	dealloc_string(get_heap_allocator(), source);
	// pre warm shader- draw every path.
	Matrix4 xform = m4_scalar(0.5);
	draw_image_xform_hit(get_sprite(SPRITE_slug)->Image, xform, get_sprite(SPRITE_slug)->size, COLOR_WHITE);
	draw_image_xform(get_sprite(SPRITE_slug)->Image, xform, get_sprite(SPRITE_slug)->size, COLOR_WHITE);
	// this doesnt help
#pragma endregion shaderinit

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
	// cooldowns
	float64 lmbCooldown = 0;
	while (!window.should_close)
	{
		float64 now = os_get_elapsed_seconds();
		delta_time = now - last_time;
		last_time = now;
#ifdef debug
		delta_time = min(delta_time, 1);
#endif
		reset_temporary_storage();

		draw_frame.projection = m4_make_orthographic_projection(window.width * -0.5, window.width * 0.5, window.height * -0.5, window.height * 0.5, -1, 10);
		draw_frame.camera_xform = m4_make_scale(v3(1 / cameraZoom, 1 / cameraZoom, 1));
		Matrix4 clipToWorld = m4_scalar(1.0);
		clipToWorld = m4_mul(clipToWorld, draw_frame.camera_xform); // its inversed when we draw
		clipToWorld = m4_mul(clipToWorld, m4_inverse(draw_frame.projection));
		// Normalize the mouse coordinates
		float window_w = window.width;
		float window_h = window.height;
		float ndc_x = (input_frame.mouse_x / (window_w * 0.5f)) - 1.0f;
		float ndc_y = (input_frame.mouse_y / (window_h * 0.5f)) - 1.0f;
		ndc_x = clamp(ndc_x, -1.0f, 1.0f);
		ndc_y = clamp(ndc_y, -1.0f, 1.0f);
		Vector4 mouse_world_pos = v4(ndc_x, ndc_y, 0, 1);
		mouse_world_pos = m4_transform(clipToWorld, mouse_world_pos);
		Vector2 playerToMouse = v2_normalize(v2_sub(v2(mouse_world_pos.x, mouse_world_pos.y), entity_player->position));

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

		// shoot ability
		if (is_key_down(MOUSE_FIRST) && lmbCooldown <= 0)
		{
			lmbCooldown = 0.3;
			// todo: refactor bounds so its easier to get it and the center
			Vector2 player_center = v2_add(entity_player->position, v2(0, get_sprite(entity_player->spriteId)->size.y / 2));
			Vector2 projectile_spawn_pos = v2_add(player_center, v2_mulf(playerToMouse, 2));
			Entity *entity_projectile0 = entity_create();
			setup_projectile0(entity_projectile0, SPRITE_projectile0, projectile_spawn_pos, playerToMouse, 150, 50, easeInQuartReverse, 1.1, 0.2);
		}
		if (lmbCooldown > 0)
		{
			lmbCooldown = max(0, lmbCooldown - delta_time);
		}

		// tick player
		entity_player->position = v2_add(entity_player->position, v2_mulf(input_axis, 50 * delta_time));

		// tick slug
		float64 detectionDistance = 50;
		Vector2 slugToPlayerV2 = v2_sub(entity_player->position, entity_slug->position);
		if (v2_length(slugToPlayerV2) <= detectionDistance) // could compare squared, but lazy
		{
			entity_slug->position = v2_add(entity_slug->position, v2_mulf(v2_normalize(slugToPlayerV2), 15 * delta_time));
		}

		// collision. player hit by slugs and slugs hit by projectiles
		Sprite *player_sprite = get_sprite(entity_player->spriteId);
		Range2f player_bounds = range2f_make_bottom_center(player_sprite->size);
		player_bounds = range2f_shift(player_bounds, entity_player->position);
		for (int i = 0; i < MAX_ENTITY_COUNT; i++)
		{
			Entity *entity = &world->entities[i];
			if (!entity->isValid)
				continue;
			entity->hitHighlightDurationLeft -= delta_time;

			// tick projectile
			if (is_projectile(entity))
			{
				if (entity->state == ENTITYSTATE_PROJECTILE_Impact)
				{
					entity->projectile_impactLifetime_progress += delta_time;
					if (entity->projectile_impactLifetime_progress >= entity->projectile_impactLifetime_total)
					{
						entity_destroy(entity);
					}
				}
				if (entity->state == ENTITYSTATE_PROJECTILE_InFlight)
				{
					entity->projectile_flightLifetime_progress += delta_time;
					float32 projectile_easing = entity->get_projectile_flight_easing(entity->projectile_flightLifetime_progress / entity->projectile_flightLifetime_total);
					entity->velocity = v2_mulf(entity->projectile_direction, projectile_easing * entity->projectile_speed * delta_time);
					entity->position = v2_add(entity->velocity, entity->position);
					if (entity->projectile_flightLifetime_progress >= entity->projectile_flightLifetime_total)
					{
						entity_destroy(entity);
					}
				}
			}

			if (entity->archetype == ARCHETYPE_slug)
			{
				Sprite *slug_sprite = get_sprite(entity->spriteId);
				Range2f slug_bounds = range2f_make_bottom_center(slug_sprite->size);
				slug_bounds = range2f_shift(slug_bounds, entity->position);
				// player hit by slug
				if (range2f_AABB(slug_bounds, player_bounds))
				{
					// todo: player take damage
					Vector2 player_knockbackv2 = slugToPlayerV2;
					add_knocknack(entity_player, 100, 0.1, slugToPlayerV2);
				}
				// slug hit by projectile
				for (int i = 0; i < MAX_ENTITY_COUNT; i++)
				{
					Entity *entity_projectile = &world->entities[i];
					if (!entity_projectile->isValid || !is_projectile(entity_projectile) || entity_projectile->state != ENTITYSTATE_PROJECTILE_InFlight)
						continue;

					Sprite *projectile0_sprite = get_sprite(entity_projectile->spriteId);
					Range2f projectile0_bounds = range2f_make_center(projectile0_sprite->size);
					projectile0_bounds = range2f_shift(projectile0_bounds, entity_projectile->position);
					if (range2f_AABB(slug_bounds, projectile0_bounds))
					{
						Vector2 projectile0ToSlug = v2_normalize(v2_sub(entity_slug->position, entity_projectile->position));
						add_knocknack(entity_slug, entity_projectile->projectile_knockback, 0.1, projectile0ToSlug);
						// TODO: audio player, config position in ndc, volume, mixing?.
						play_one_audio_clip(STR("../Assets/kenney_impact-sounds/Audio/footstep_snow_003.ogg"));
						entity_projectile->state = ENTITYSTATE_PROJECTILE_Impact;
						entity->hitHighlightDurationLeft = 0.1;
					}
				}
			}

			// tick knockbacks
			if (entity->knockback_durationLeft > 0)
			{
				tick_knockback(entity, delta_time);
			}
		}

// rendering
#pragma region rendering
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
				xform = m4_translate(xform, v3(sprite->size.x * -0.5, 0, 0)); // bottom center
				xform = m4_translate(xform, v3(entity->position.x, entity->position.y, 0));
				draw_image_xform(sprite->Image, xform, sprite->size, COLOR_WHITE);
				// trynna understand
				// debug_projection(&xform, entity, font, font_height);
				break;
			}
			case ARCHETYPE_projectile:
			{
				if (!entity->renderSprite)
					break;

				Sprite *sprite = get_sprite(entity->spriteId);
				Matrix4 xform = m4_scalar(1.0);
				xform = m4_translate(xform, v3(sprite->size.x * -0.5, sprite->size.y * -0.5, 0)); // center
				xform = m4_translate(xform, v3(entity->position.x, entity->position.y, 0));
				if (entity->state == ENTITYSTATE_PROJECTILE_Impact)
				{
					// TODO: refactor this into sprite struct and a method
					Sprite *sheet_sprite = get_sprite(SPRITE_projectile0_sheet);
					Gfx_Image *anim_sheet = sheet_sprite->Image;
					// animate impact
					// Configure information about the whole image as a sprite sheet
					u32 number_of_columns = 4;
					u32 number_of_rows = 1;
					u32 total_number_of_frames = number_of_rows * number_of_columns;

					u32 anim_frame_width = anim_sheet->width / number_of_columns;
					u32 anim_frame_height = anim_sheet->height / number_of_rows;

					// Configure the animation by setting the start & end frames in the grid of frames
					// (Inspect sheet image and count the frame indices you want)
					// In sprite sheet animations, it usually goes down. So Y 0 is actuall the top of the
					// sprite sheet, and +Y is down on the sprite sheet.
					u32 anim_start_frame_col = 1;
					u32 anim_start_frame_row = 0;
					u32 anim_end_frame_col = 3;
					u32 anim_end_frame_row = 0;
					u32 anim_start_index = anim_start_frame_row * number_of_columns + anim_start_frame_col;
					u32 anim_end_index = anim_end_frame_row * number_of_columns + anim_end_frame_col;
					u32 anim_number_of_frames = anim_end_index - anim_start_index + 1;

					// Sanity check configuration
					assert(anim_end_index > anim_start_index, "The last frame must come before the first frame");
					assert(anim_start_frame_col < number_of_columns, "anim_start_frame_x is out of bounds");
					assert(anim_start_frame_row < number_of_rows, "anim_start_frame_y is out of bounds");
					assert(anim_end_frame_col < number_of_columns, "anim_end_frame_x is out of bounds");
					assert(anim_end_frame_row < number_of_rows, "anim_end_frame_y is out of bounds");

					// Calculate duration per frame in seconds
					float32 playback_fps = 6;
					float32 anim_time_per_frame = 1.0 / playback_fps;
					float32 anim_duration = anim_time_per_frame * (float32)anim_number_of_frames;

					// Float modulus to "loop" around the timer over the anim duration
					// float32 anim_elapsed = fmodf(entity->projectile_impactLifetime_progress, anim_duration);
					float32 anim_elapsed = min(entity->projectile_impactLifetime_progress / anim_duration, anim_duration - 0.001);
					// Get current progression in animation from 0.0 to 1.0
					float32 anim_progression_factor = anim_elapsed / anim_duration;

					u32 anim_current_index = anim_number_of_frames * anim_progression_factor;
					u32 anim_absolute_index_in_sheet = anim_start_index + anim_current_index;

					u32 anim_index_x = anim_absolute_index_in_sheet % number_of_columns;
					u32 anim_index_y = anim_absolute_index_in_sheet / number_of_columns + 1;

					u32 anim_sheet_pos_x = anim_index_x * anim_frame_width;
					u32 anim_sheet_pos_y = (number_of_rows - anim_index_y) * anim_frame_height; // Remember, Y inverted.

					// Draw the sprite sheet, with the uv box for the current frame.
					// Uv box is a Vector4 of x1, y1, x2, y2 where each value is a percentage value 0.0 to 1.0
					// from left to right / bottom to top in the texture.

					Draw_Quad *quad = draw_image_xform(anim_sheet, xform, v2(7, 7), COLOR_WHITE);
					quad->uv.x1 = (float32)(anim_sheet_pos_x) / (float32)anim_sheet->width;
					quad->uv.y1 = (float32)(anim_sheet_pos_y) / (float32)anim_sheet->height;
					quad->uv.x2 = (float32)(anim_sheet_pos_x + anim_frame_width) / (float32)anim_sheet->width;
					quad->uv.y2 = (float32)(anim_sheet_pos_y + anim_frame_height) / (float32)anim_sheet->height;
					// draw_image_xform_hit(sprite->Image, xform, sprite->size, COLOR_RED);
					break;
				}
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
				if (entity->hitHighlightDurationLeft > 0)
					draw_image_xform_hit(sprite->Image, xform, sprite->size, COLOR_WHITE);
				else
					draw_image_xform(sprite->Image, xform, sprite->size, COLOR_WHITE);
			}
			}
		}
#pragma endregion rendering
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

Draw_Quad *draw_image_xform_hit(Gfx_Image *image, Matrix4 xform, Vector2 size, Vector4 color)
{
	Draw_Quad *q = draw_image_xform(image, xform, size, COLOR_WHITE);
	// detail_type
	q->userdata[0].x = SHADER_EFFECT_COLORCHANGE;
	return q;
}
