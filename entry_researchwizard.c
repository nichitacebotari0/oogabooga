
int entry(int argc, char **argv)
{

	window.title = STR("Minimal Game Example");
	window.scaled_width = 1280; // We need to set the scaled size if we want to handle system scaling (DPI)
	window.scaled_height = 720;
	window.x = 200;
	window.y = 90;
	window.clear_color = hex_to_rgba(0xffffffff);

	Gfx_Image *player_sprite = load_image_from_disk(fixed_string("../Assets/Player.png"), get_heap_allocator());
	assert(player_sprite, "Failed to load player sprite");


	Vector2 player_pos = v2(0, 0);
	const float32 second = 1;
	float64 fpsTime = 0;
	float64 fps = 0;
	float64 last_time = os_get_current_time_in_seconds();
	float64 delta_time = 0;
	while (!window.should_close)
	{
		float64 now = os_get_current_time_in_seconds();
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
		player_pos = v2_add(player_pos, v2_mulf(input_axis, 50 * delta_time));

		draw_frame.projection = m4_make_orthographic_projection(window.width * -0.5, window.width * 0.5, window.height * -0.5, window.height * 0.5, -1, 10);
		const float64 zoom = 6.0f;
		draw_frame.view = m4_make_scale(v3(1 / zoom, 1 / zoom, 1));

		Vector2 size = v2(7,11);
		Matrix4 xform = m4_scalar(1.0);
		xform = m4_translate(xform, v3(size.x * -0.5, 0, 0));
		xform = m4_translate(xform, v3(player_pos.x, player_pos.y, 0));
		draw_image_xform(player_sprite, xform, size, COLOR_RED);


		os_update();
		gfx_update();
		
		// fps counter
		if (fpsTime > second)
		{
			log("%.2f FPS\n%.2fms", fps, fpsTime);
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