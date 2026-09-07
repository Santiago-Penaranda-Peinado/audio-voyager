#version 450 core

in vec2 v_uv;
out vec4 frag_color;

uniform sampler2D u_image;
uniform bool u_horizontal;
uniform bool u_first_pass;
uniform float u_bloom_threshold;

const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

// Energy-conserving Karis soft-knee bright-pass filter
vec3 extract_bright_pass(vec3 color, float threshold, float knee) {
    float luma = max(color.r, max(color.g, color.b));
    float soft = clamp(luma - threshold + knee, 0.0, 2.0 * knee);
    soft = (soft * soft) / (4.0 * knee + 0.00001);
    float weight = max(luma - threshold, soft) / max(luma, 0.0001);
    return color * clamp(weight, 0.0, 1.0);
}

void main() {
    vec2 tex_offset = 1.0 / textureSize(u_image, 0);
    vec3 result = vec3(0.0);
    float knee = 0.50;
    float threshold = (u_bloom_threshold > 0.01) ? u_bloom_threshold : 1.20;

    if (u_horizontal) {
        for (int i = -4; i <= 4; ++i) {
            vec3 col = texture(u_image, v_uv + vec2(tex_offset.x * float(i) * 1.5, 0.0)).rgb;
            if (u_first_pass) {
                col = extract_bright_pass(col, threshold, knee);
            }
            result += col * weights[abs(i)];
        }
    } else {
        for (int i = -4; i <= 4; ++i) {
            vec3 col = texture(u_image, v_uv + vec2(0.0, tex_offset.y * float(i) * 1.5)).rgb;
            if (u_first_pass) {
                col = extract_bright_pass(col, threshold, knee);
            }
            result += col * weights[abs(i)];
        }
    }

    frag_color = vec4(result, 1.0);
}
