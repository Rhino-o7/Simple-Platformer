#version 300 es
precision highp float;
precision highp int;

#define LIGHT_COUNT 64

in vec2 frag_uv;
out vec4 frag_color;

uniform sampler2D albedo_tex;
uniform sampler2D position_tex;
uniform sampler2D normal_tex;
uniform sampler2D ssao_tex;

uniform mat4 proj;
uniform mat4 view;
uniform float z_far;

struct Light {
    vec4 position;
    vec4 direction;
    vec4 ambient;
    vec4 diffuse;
    float constant;
    float linear;
    float quadratic;
    float _padding;
};

layout (std140) uniform Lights {
   Light lights[LIGHT_COUNT];
};

void main() {
    vec4 albedo = texture(albedo_tex, frag_uv);
    vec3 position = texture(position_tex, frag_uv).xyz;
    vec3 normal = texture(normal_tex, frag_uv).xyz;
    float ao = texture(ssao_tex, frag_uv).r;

    vec3 sky_color = mix(vec3(0.55, 0.60, 0.78), vec3(0.10, 0.25, 0.55), frag_uv.y);

    vec3 lit = albedo.rgb * (0.35 + 0.65 * ao);
    float ambient_probe = lights[0].ambient.x + lights[0].ambient.y + lights[0].ambient.z;
    lit *= (1.0 + ambient_probe * 0.25);

    float has_geometry = step(0.0001, dot(abs(normal), vec3(1.0)) + dot(abs(position), vec3(1.0)));
    vec3 final_color = mix(sky_color, lit, has_geometry);

    float depth = clamp(length(position) / max(z_far, 0.001), 0.0, 1.0);
    final_color = mix(final_color, sky_color, depth * depth * 0.35);

    float keep_uniforms_live = proj[0][0] * 0.000001 + view[0][0] * 0.000001;
    frag_color = vec4(clamp(final_color + vec3(keep_uniforms_live), 0.0, 1.0), 1.0);
}
