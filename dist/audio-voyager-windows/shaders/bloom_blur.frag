#version 450 core

in vec2 v_uv;
out vec4 frag_color;

uniform sampler2D u_image;
uniform bool u_horizontal;
uniform float u_bloom_threshold;

const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
    vec2 tex_offset = 1.0 / textureSize(u_image, 0);
    vec3 result = vec3(0.0);

    if (u_horizontal) {
        for (int i = -4; i <= 4; ++i) {
            vec3 col = texture(u_image, v_uv + vec2(tex_offset.x * float(i) * 1.5, 0.0)).rgb;
            result += col * weights[abs(i)];
        }
    } else {
        for (int i = -4; i <= 4; ++i) {
            vec3 col = texture(u_image, v_uv + vec2(0.0, tex_offset.y * float(i) * 1.5)).rgb;
            result += col * weights[abs(i)];
        }
    }

    frag_color = vec4(result, 1.0);
}
