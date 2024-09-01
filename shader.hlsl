
// PS_INPUT is defined in the default shader in gfx_impl_d3d11.c at the bottom of the file

// BEWARE std140 packing:
// https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-packing-rules
cbuffer some_cbuffer : register(b0)
{
    // float2 mouse_pos_screen; // In pixels
    float2 window_size;
}

#define SHADER_EFFECT_COLORCHANGE 1

// This procedure is the "entry" of our extension to the shader
// It basically just takes in the resulting color and input from vertex shader, for us to transform it
// however we want.
float4 pixel_shader_extension(PS_INPUT input, float4 color)
{

    float detail_type = input.userdata[0].x;
    if (detail_type == SHADER_EFFECT_COLORCHANGE)
    {
        color = float4(1, 1, 1, color.w);
    }

    return color;
}