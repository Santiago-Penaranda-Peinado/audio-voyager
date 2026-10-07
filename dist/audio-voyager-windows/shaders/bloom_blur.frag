#version 450 core

in vec2 v_uv;
out vec4 frag_color;

uniform sampler2D u_image;
uniform bool u_horizontal;
uniform float u_bloom_threshold;
uniform bool u_first_iteration;

const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

vec3 extract_bright(vec3 col) {
    float luma = max(col.r, max(col.g, col.b));
    float thresh = max(u_bloom_threshold, 0.1);
    float knee = 0.12; // Crisp cutoff so standard terrain surfaces do NOT bleed into bloom
    float soft = clamp(luma - thresh + knee, 0.0, 2.0 * knee);
    soft = (soft * soft) / (4.0 * knee + 1e-4);
    float factor = max(soft, luma - thresh) / max(luma, 1e-4);
    return col * clamp(factor, 0.0, 10.0);
}

void main() {
    vec2 tex_offset = 1.0 / textureSize(u_image, 0);
    vec3 result = vec3(0.0);

    if (u_horizontal) {
        for (int i = -4; i <= 4; ++i) {
            vec3 col = texture(u_image, v_uv + vec2(tex_offset.x * float(i) * 1.5, 0.0)).rgb;
            if (u_first_iteration) {
                col = extract_bright(col);
            }
            result += col * weights[abs(i)];
        }
    } else {
        for (int i = -4; i <= 4; ++i) {
            vec3 col = texture(u_image, v_uv + vec2(0.0, tex_offset.y * float(i) * 1.5)).rgb;
            if (u_first_iteration) {
                col = extract_bright(col);
            }
            result += col * weights[abs(i)];
        }
    }

    frag_color = vec4(result, 1.0);
}
