#version 460

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 tex;

layout(location = 2) in vec2 offset;
layout(location = 3) in vec2 size;
layout(location = 4) in vec2 uv_min;
layout(location = 5) in vec2 uv_max;

layout(push_constant) uniform pc {
  vec2 viewport_size;
};

layout(location = 0) out vec2 out_texcoord;
layout(location = 1) out vec4 out_color;

void main() {
  vec2 pos_px = ((pos + 1.0) * size / 2.0) + offset;

  gl_Position = vec4(((pos_px * 2 / viewport_size) - 1), 0.0, 1.0);
  out_texcoord = tex * (uv_max - uv_min) + uv_min;
  out_color = vec4(0.0, 0.0, 0.0, 1.0);
}
