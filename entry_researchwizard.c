
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

	float64 last_time = os_get_current_time_in_seconds();
	float64 delta_time = 0;
	while (!window.should_close)
	{
		float64 now = os_get_current_time_in_seconds();
		if ((int)now != (int)last_time)
			log("%.2f FPS\n%.2fms", 1.0 / (now - last_time), (now - last_time) * 1000);
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
		player_pos = v2_add(player_pos, v2_mulf(input_axis, 1 * delta_time));

		Matrix4 xform = m4_scalar(1.0);
		xform = m4_translate(xform, v3(player_pos.x, player_pos.y, 0));
		draw_image_xform(player_sprite, xform, v2(.5f, .5f), COLOR_GREEN);

		// draw_rect(v2(sin(now), -.8), v2(.5, .25), COLOR_RED);

		// draws line to mouse pos
		// float aspect = (f32)window.width/(f32)window.height;
		// float mx = (input_frame.mouse_x/(f32)window.width  * 2.0 - 1.0)*aspect;
		// float my = input_frame.mouse_y/(f32)window.height * 2.0 - 1.0;
		// draw_line(v2(-.75, -.75), v2(mx, my), 0.005, COLOR_WHITE);

		os_update();
		gfx_update();
	}

	return 0;
}