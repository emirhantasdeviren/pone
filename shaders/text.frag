#version 460

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec4 in_color;

layout(binding = 0) uniform texture2D _texture;
layout(binding = 1) uniform sampler _sampler;

layout(location = 0) out vec4 out_frag_color;

void main() {
  vec4 dist = texture(sampler2D(_texture, _sampler), in_uv);
  float alpha = smoothstep(0.4, 0.6, dist.r);

  out_frag_color = vec4(in_color.xyz, in_color.a * alpha);
}
