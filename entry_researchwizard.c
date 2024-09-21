// #define debug

#include "easings.c"
#include "range.c"
#include "entity.c"
#include "state.c"
#include "utils.c"

GameState *gameState = 0;
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
	assert(false, "no entity found");
	return null;
}

void entity_destroy(Entity *entity)
{
	memset(entity, 0, sizeof(Entity));
}

// shader stuff
#define SHADER_EFFECT_COLORCHANGE 1
// BEWARE std140 packing:
// https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-packing-rules
typedef struct Shader_ConstantsBuffer
{
	Vector2 window_size; // We only use this to revert the Y in the shader because for some reason d3d11 inverts it.
} Shader_ConstantsBuffer;
void shader_set_highlight(Draw_Quad *quad);
Draw_Quad *draw_image_xform_sheet_animated(Sprite *sheet_sprite, Matrix4 xform, float32 animation_progress);
void DrawProgressBar(Matrix4 barTransform, Vector2 barSize, float32 padding, float32 fillPercentage, Vector4 outlineColor, Vector4 fillColor);
void DrawEntityHealthBar(Entity *entity, Matrix4 xform);
void RenderWorld(World *world, Vector2 entity_player_position, float64 now, float64 delta_time);
void set_screen_space();
void set_world_space();
Vector2 get_screenPixels(Vector2 input, Vector2 windowSize);

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

#pragma region Load Assets
	sprites[SPRITE_player] = (Sprite){.size = v2(7, 11), .Image = load_image_from_disk(fixed_string("../Assets/Player.png"), get_heap_allocator())};
	sprites[SPRITE_slug] = (Sprite){.size = v2(9, 6), .Image = load_image_from_disk(fixed_string("../Assets/slug.png"), get_heap_allocator())};
	sprites[SPRITE_projectile0] = (Sprite){.size = v2(7, 7), .Image = load_image_from_disk(fixed_string("../Assets/projectile0.png"), get_heap_allocator())};
	{
		Gfx_Image *projectile0_sheet = load_image_from_disk(fixed_string("../Assets/projectile0_sheet.png"), get_heap_allocator());
		assert(projectile0_sheet, "load");
		sprites[SPRITE_projectile0_sheet] = (Sprite){.size = v2(28, 7), .Image = projectile0_sheet};
		setup_spriteSheet(&sprites[SPRITE_projectile0_sheet], 1, 4, 0, 1, 0, 3, 6);
	}

	string kenney_impact_snow03 = STR("../Assets/kenney_impact-sounds/Audio/footstep_snow_003.ogg");
	// hacky way to avoid stutter on audio load
	{
		Audio_Playback_Config audio_player_config = {0};
		audio_player_config.volume = 0.001;
		audio_player_config.playback_speed = 9999.0;
		play_one_audio_clip_with_config(kenney_impact_snow03, audio_player_config);
	}

	Gfx_Font *font = load_font_from_disk(STR("C:/windows/fonts/arial.ttf"), get_heap_allocator());
	assert(font, "Failed loading arial.ttf");
	const u32 font_height = 48;
#pragma endregion Load Assets

#pragma region shaderinit
	string source;
	bool ok = os_read_entire_file("shader.hlsl", &source, get_heap_allocator());
	assert(ok, "Could not read oogabooga/examples/custom_shader.hlsl");
	/// This memory needs to stay alive throughout the frame because we pass the pointer to it in draw_frame.cbuffer.
	/// If this memory is invalidated before gfx_update after setting draw_frame.cbuffer, then gfx_update will copy
	/// memory from an invalid address.
	Shader_ConstantsBuffer cbuffer;
	// This is slow and needs to recompile the shader. However, it should probably only happen once (or each hot reload)
	// If it fails, it will return false and return to whatever shader it was before.
	gfx_shader_recompile_with_extension(source, sizeof(Shader_ConstantsBuffer));
	dealloc_string(get_heap_allocator(), source);
#pragma endregion shaderinit

	world = alloc(get_heap_allocator(), sizeof(World));
	memset(world, 0, sizeof(World));
	setup_world(world);

	gameState = alloc(get_heap_allocator(), sizeof(GameState));
	memset(gameState, 0, sizeof(GameState));
	gameState->World = world;
	gameState->GameMode = GameMode_MainMenu;

	Entity *entity_player = entity_create();
	setup_player(entity_player);

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
#ifdef debug // prevent things from flying off screen when breaking in debug
		delta_time = min(delta_time, 1);
#endif
		reset_temporary_storage();
		// Normalize the mouse coordinates
		float window_w = window.width;
		float window_h = window.height;
		float mouse_ndc_x = (input_frame.mouse_x / (window_w * 0.5f)) - 1.0f;
		float mouse_ndc_y = (input_frame.mouse_y / (window_h * 0.5f)) - 1.0f;
		mouse_ndc_x = clamp(mouse_ndc_x, -1.0f, 1.0f);
		mouse_ndc_y = clamp(mouse_ndc_y, -1.0f, 1.0f);
		// setup projections
		draw_frame.projection = m4_make_orthographic_projection(window.width * -0.5, window.width * 0.5, window.height * -0.5, window.height * 0.5, -1, 10);
		// WARNING: camera offset effect achieved without full understanding of why it works
		// todo: make toggleable, figure out if this ruins aiming
		// Vector3 cameraOffsetTargetPos = v3(mouse_ndc_x * -50, mouse_ndc_y * -50, 0);
		// draw_frame.projection = m4_translate(draw_frame.projection, cameraOffsetTargetPos);

		animate_v2_to_target(&world->cameraPosition, entity_player->position, delta_time, 15);
		draw_frame.camera_xform = m4_translate(draw_frame.camera_xform, v3(world->cameraPosition.x, world->cameraPosition.y, 0));
		draw_frame.camera_xform = m4_scale(draw_frame.camera_xform, v3(1 / world->cameraZoom, 1 / world->cameraZoom, 1));
		// calculate after all projections applied
		Matrix4 clipToWorld = m4_scalar(1.0);
		clipToWorld = m4_mul(clipToWorld, draw_frame.camera_xform); // its inversed when we draw
		clipToWorld = m4_mul(clipToWorld, m4_inverse(draw_frame.projection));
		Vector4 mouse_world_pos = v4(mouse_ndc_x, mouse_ndc_y, 0, 1);
		mouse_world_pos = m4_transform(clipToWorld, mouse_world_pos);
		Vector2 playerToMouse = v2_normalize(v2_sub(v2(mouse_world_pos.x, mouse_world_pos.y), entity_player->position));
		gameState->worldProjection = draw_frame.projection;
		gameState->world_camera_xform = draw_frame.camera_xform;

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
		if (is_key_down(MOUSE_FIRST) && world->lmbCooldownLeft <= 0 && entity_player->state == ENTITYSTATE_PLAYER_FreeMove)
		{
			world->lmbCooldownLeft = world->lmbCooldown;
			// todo: refactor bounds so its easier to get it and the center
			Vector2 player_center = v2_add(entity_player->position, v2(0, get_sprite(entity_player->spriteId)->size.y / 2));
			Vector2 projectile_spawn_pos = v2_add(player_center, v2_mulf(playerToMouse, 2));
			Entity *entity_projectile0 = entity_create();
			setup_projectile0(entity_projectile0, SPRITE_projectile0, projectile_spawn_pos, playerToMouse, 150, 50, easeInQuartReverse, 1.1, 0.2, TEAM_1);
		}
		world->lmbCooldownLeft = max(0, world->lmbCooldownLeft - delta_time);

		// dash ability
		if (is_key_down(KEY_SHIFT) && world->dashCooldownLeft <= 0)
		{
			player_stateTransition(entity_player, ENTITYSTATE_PLAYER_Dash);
			world->dashCooldownLeft = world->dashCooldown;
			world->dashStartedAt = now;
			world->dashInitialPosition = entity_player->position;
			world->dashTarget = v2_length(input_axis) > 0.01 ? v2_mulf(input_axis, world->dashDistance) : v2_mulf(entity_player->facingDirection, world->dashDistance);
			world->dashTarget = v2_add(world->dashTarget, world->dashInitialPosition);
		}
		if (world->dashCooldownLeft > 0 && entity_player->state == ENTITYSTATE_PLAYER_FreeMove)
		{
			world->dashCooldownLeft = max(0, world->dashCooldownLeft - delta_time);
		}

		if (entity_player->state == ENTITYSTATE_PLAYER_FreeMove)
		{
			entity_player->facingDirection = input_axis;
			Vector2 player_acceleration = v2_mulf(input_axis, world->playerSpeed);
			// simulate drag
			Vector2 drag = v2_mulf(v2(-entity_player->velocity.x, -entity_player->velocity.y), world->dragForce);
			player_acceleration = v2_add(player_acceleration, drag);

			// newPosition = 1/2*a*t^2 + v*t + p = 1/2 * acceleration * delta_time^2 + oldVelocity * delta_time + oldPosition
			entity_player->position = v2_add(
				v2_add(
					v2_mulf(player_acceleration, (1 / 2 * pow(delta_time, 2))),
					v2_mulf(entity_player->velocity, delta_time)),
				entity_player->position);
			// newVelocity = acceleration * delta_time + oldVelocity
			entity_player->velocity = v2_add(
				v2_mulf(player_acceleration, delta_time),
				entity_player->velocity);
		}
		else if (entity_player->state == ENTITYSTATE_PLAYER_Dash)
		{
			float32 dashTimeProgress = (now - world->dashStartedAt);
			if (dashTimeProgress >= world->dashDuration)
			{
				player_stateTransition(entity_player, ENTITYSTATE_PLAYER_FreeMove);
			}
			else
			{
				if (dashTimeProgress < world->dashFlightDuration)
				{
					Vector2 dashTrajectory = v2_sub(world->dashTarget, world->dashInitialPosition);
					entity_player->position = v2_add(world->dashInitialPosition, v2_mulf(dashTrajectory, dashTimeProgress / world->dashFlightDuration));
				}
			}
		}

		// spawn slugs
		world->spawnCooldownLeft -= delta_time;
		if (world->spawnCooldownLeft <= 0 && world->enemiesSpawned < world->enemiesMax)
		{
			Entity *entity_slug = entity_create();
			setup_slug(entity_slug);
			entity_slug->position.x = get_random_float32_in_range(entity_player->position.x - 50, entity_player->position.x + 50);
			entity_slug->position.y = get_random_float32_in_range(entity_player->position.y - 50, entity_player->position.y + 50);
			Vector2 playerToSlugSpawn = v2_sub(entity_slug->position, entity_player->position);
			if (v2_length(playerToSlugSpawn) < 10)
			{
				entity_slug->position = v2_add(entity_slug->position, v2_mulf(playerToSlugSpawn, 10));
			}
			world->enemiesSpawned++;
			world->spawnCooldownLeft = world->spawnCooldown;
		}

#pragma region physicsTick
		Sprite *player_sprite = get_sprite(entity_player->spriteId);
		Range2f player_bounds = range2f_make_bottom_center(player_sprite->size);
		player_bounds = range2f_shift(player_bounds, entity_player->position);
		for (int i = 0; i < MAX_ENTITY_COUNT; i++)
		{
			Entity *entity = &world->entities[i];
			if (!entity->isValid)
				continue;
			entity->hitHighlightDurationLeft = max(0, entity->hitHighlightDurationLeft - delta_time);

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
					// check for impact
					for (int i = 0; i < MAX_ENTITY_COUNT && entity->canCollide; i++)
					{
						Entity *targetEntity = &world->entities[i];
						if (!targetEntity->isValid ||
							!targetEntity->canCollide ||
							targetEntity->team == entity->team ||
							targetEntity->entityLayer && entity->collisionLayer == 0)
							continue;

						Sprite *projectileSprite = get_sprite(entity->spriteId);
						Range2f projectileBounds = range2f_make_center(projectileSprite->size);
						projectileBounds = range2f_shift(projectileBounds, entity->position);
						Sprite *targetEntitySprite = get_sprite(targetEntity->spriteId);
						Range2f targetEntitySpriteBounds = range2f_make_bottom_center(targetEntitySprite->size);
						targetEntitySpriteBounds = range2f_shift(targetEntitySpriteBounds, targetEntity->position);
						if (range2f_AABB(targetEntitySpriteBounds, projectileBounds))
						{
							Vector2 projectile0ToEntity = v2_normalize(v2_sub(targetEntity->position, entity->position));
							apply_damage(targetEntity, 10, now);
							if (targetEntity->Health <= 0)
								entity_destroy(targetEntity);
							add_knocknack(targetEntity, entity->projectile_knockback, 0.1, projectile0ToEntity);
							// TODO: audio player, config position in ndc, volume, mixing?.
							play_one_audio_clip(kenney_impact_snow03);
							entity->state = ENTITYSTATE_PROJECTILE_Impact;
							entity->hitHighlightDurationLeft = 0.1;
						}
					}

					// tick
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
				// tick slugs
				float64 detectionDistance = 500;
				Vector2 slugToPlayerV2 = v2_sub(entity_player->position, entity->position);
				if (v2_length(slugToPlayerV2) <= detectionDistance) // could compare squared, but lazy
				{
					entity->position = v2_add(entity->position, v2_mulf(v2_normalize(slugToPlayerV2), 15 * delta_time));
				}

				Sprite *slug_sprite = get_sprite(entity->spriteId);
				Range2f slug_bounds = range2f_make_bottom_center(slug_sprite->size);
				slug_bounds = range2f_shift(slug_bounds, entity->position);
				// player hit by slug
				if (entity_player->canCollide && !entity_player->isInvincible && range2f_AABB(slug_bounds, player_bounds))
				{
					// todo: player take damage
					Vector2 player_knockbackv2 = slugToPlayerV2;
					apply_damage(entity_player, 5, now);
					add_knocknack(entity_player, 100, 0.1, slugToPlayerV2);
					entity_player->hitHighlightDurationLeft = 0.1;
				}
			}

			// tick knockbacks
			if (entity->knockback_durationLeft > 0)
			{
				tick_knockback(entity, delta_time);
			}
		}
#pragma endregion collision

		RenderWorld(world, entity_player->position, now, delta_time);
		// render HUD
		set_screen_space();
		for (int i = 0; i < 1; (i = 1, set_world_space()))
		{
			Vector2 windowSize = v2(window.width, window.height);
			// all sizes in ndc, then multiplied by screen width
			Vector2 screenPadding = get_screenPixels(v2(1 / 100.f, 1 / 100.f), windowSize);
			Vector2 dashBarSize = get_screenPixels(v2(1 / 24.f * 3, 1 / 24.f), windowSize);

			// dash cooldown bar
			float64 heightDrawn = windowSize.y - screenPadding.y;
			heightDrawn -= dashBarSize.y;
			Matrix4 xform = m4_scalar(1);
			xform = m4_translate(xform, v3(screenPadding.x, heightDrawn, 0));
			float64 dashReadyProgress = 1 - world->dashCooldownLeft / world->dashCooldown;
			DrawProgressBar(xform, dashBarSize, 0.1f, dashReadyProgress, COLOR_WHITE, (dashReadyProgress >= 1) ? COLOR_GREEN : COLOR_BLUE);
		}

		os_update();
		gfx_update();

		// fps counter
		if (fpsTime > second)
		{
#ifdef debug // log fps
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

void RenderWorld(World *world, Vector2 entity_player_position, float64 now, float64 delta_time)
{
	// draw checkerboard
	Vector2 playerTile = v2(roundf(entity_player_position.x / 10), roundf(entity_player_position.y / 10));
	for (int i = playerTile.x - 30; i < playerTile.x + 30; i++)
	{
		for (int j = playerTile.y - 25; j < playerTile.y + 25; j++)
		{
			float32 alpha = 0.1 * (float32)abs(((j % 2) + i) % 2);
			draw_rect(v2(i * 10, j * 10), v2(10, 10), v4(1, 0, 0, alpha));
		}
	}

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
			if (entity->state == ENTITYSTATE_PLAYER_FreeMove)
			{
				Draw_Quad *quad = draw_image_xform(sprite->Image, xform, sprite->size, COLOR_WHITE);
				if (entity->hitHighlightDurationLeft > 0)
					shader_set_highlight(quad);
			}
			else if (entity->state == ENTITYSTATE_PLAYER_Dash)
			{
				float32 dashTimeProgress = (now - world->dashStartedAt);
				Vector2 spriteSize = sprite->size;
				if (dashTimeProgress > 0 && dashTimeProgress < world->dashFlightDuration)
				{
					spriteSize = v2(spriteSize.x * 1.5, spriteSize.y * 0.5);
					animate_v2_to_target(&spriteSize, sprite->size, delta_time, 15);
				}
				Draw_Quad *quad = draw_image_xform(sprite->Image, xform, spriteSize, COLOR_WHITE);
				if (dashTimeProgress > world->dashHighlight1Start && dashTimeProgress < world->dashHighlight1Start + world->dashHighlightDuration)
				{
					shader_set_highlight(quad);
				}
				if (dashTimeProgress > world->dashHighlight2Start && dashTimeProgress < world->dashHighlight2Start + world->dashHighlightDuration)
				{
					shader_set_highlight(quad);
				}
			}
			DrawEntityHealthBar(entity, xform);
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
				Sprite *sheet_sprite = get_sprite(SPRITE_projectile0_sheet);
				float32 animation_progress = entity->projectile_impactLifetime_progress / entity->projectile_impactLifetime_total;
				draw_image_xform_sheet_animated(sheet_sprite, xform, animation_progress);
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

			Draw_Quad *quad = draw_image_xform(sprite->Image, xform, sprite->size, COLOR_WHITE);
			if (entity->hitHighlightDurationLeft > 0)
				shader_set_highlight(quad);
			DrawEntityHealthBar(entity, xform);
		}
		}
	}
}

void shader_set_highlight(Draw_Quad *quad)
{
	quad->userdata[0].x = SHADER_EFFECT_COLORCHANGE;
}

Draw_Quad *draw_image_xform_sheet_animated(Sprite *sheet_sprite, Matrix4 xform, float32 animation_progress)
{
	Draw_Quad *quad = draw_image_xform(sheet_sprite->Image, xform, v2(sheet_sprite->anim_frame_width, sheet_sprite->anim_frame_height), COLOR_WHITE);
	SpriteUV uvs = get_sprite_animation_uv(sheet_sprite, animation_progress);
	quad->uv.x1 = uvs.start.x;
	quad->uv.y1 = uvs.start.y;
	quad->uv.x2 = uvs.end.x;
	quad->uv.y2 = uvs.end.y;
	return quad;
}

void DrawProgressBar(Matrix4 barTransform, Vector2 barSize, float32 padding, float32 fillPercentage, Vector4 outlineColor, Vector4 fillColor)
{
	draw_rect_xform(barTransform, v2(barSize.x, barSize.y), outlineColor);
	barTransform = m4_translate(barTransform, v3(padding, padding, 0));
	draw_rect_xform(barTransform, v2((barSize.x - padding * 2) * fillPercentage, barSize.y - padding * 2), fillColor);
}

void DrawEntityHealthBar(Entity *entity, Matrix4 xform)
{
	Sprite *sprite = get_sprite(entity->spriteId);
	float32 barHeight = max(sprite->size.y / 4, 2);
	Vector2 barSize = v2(sprite->size.x - 0.1, barHeight);
	float32 border = max(sprite->size.y / 25, 0.1);
	xform = m4_translate(xform, v3(0, -barSize.y - 0.5, 0));
	const float healthPercentage = entity->Health / entity->MaxHealth;
	DrawProgressBar(xform, barSize, border, healthPercentage, COLOR_WHITE, COLOR_RED);
}

void set_screen_space()
{
	draw_frame.camera_xform = m4_scalar(1.0);
	draw_frame.projection = m4_make_orthographic_projection(0.0, window.width, 0.0, window.height, -1, 10);
}
void set_world_space()
{
	draw_frame.projection = gameState->worldProjection;
	draw_frame.camera_xform = gameState->world_camera_xform;
}

Vector2 get_screenPixels(Vector2 ndc, Vector2 windowSize)
{
	// todo: figure out widths vs pixelwidth vs point width
	return v2(ndc.x * windowSize.x, ndc.y * windowSize.y);
}
