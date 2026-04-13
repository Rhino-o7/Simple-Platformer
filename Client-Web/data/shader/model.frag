#version 300 es
precision highp float;
precision highp int;

flat in uint frag_material;

out vec4 frag_color;

vec3 material_color(uint m) {
    float f = float(m);
    return vec3(
        0.25 + 0.75 * fract(f * 0.37),
        0.25 + 0.75 * fract(f * 0.61),
        0.25 + 0.75 * fract(f * 0.83)
    );
}

void main() {
    frag_color = vec4(material_color(frag_material), 1.0);
}
