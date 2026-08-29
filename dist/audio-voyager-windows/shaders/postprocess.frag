#version 450 core

in vec2 v_uv;
out vec4 frag_color;

uniform sampler2D u_scene_hdr;
uniform sampler2D u_bloom_blur;

uniform float u_bloom_intensity;
uniform float u_chromatic_aberration;
uniform float u_glitch_amount;
uniform float u_speed_lines;
uniform float u_time;
uniform vec2 u_resolution;

// ACES Filmic Tone Mapping Curve
vec3 ACESFilm(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec2 uv = v_uv;

    // Subtle horizontal micro-slice on explosive onsets
    vec2 sample_uv = uv;
    if (u_glitch_amount > 0.45) {
        float slice = sin(uv.y * 140.0 + u_time * 60.0);
        if (abs(slice) > 0.90) {
            sample_uv.x += (slice > 0.0 ? 1.0 : -1.0) * (u_glitch_amount - 0.45) * 0.015;
        }
    }

    vec2 center_offset = sample_uv - 0.5;
    float dist_sq = dot(center_offset, center_offset);

    // Highly responsive chromatic aberration
    float ca_strength = u_chromatic_aberration * 3.5 * (1.0 + u_glitch_amount * 0.6);
    vec2 ca_offset = center_offset * dist_sq * ca_strength;

    float r = texture(u_scene_hdr, sample_uv + ca_offset).r;
    float g = texture(u_scene_hdr, sample_uv).g;
    float b = texture(u_scene_hdr, sample_uv - ca_offset).b;
    vec3 scene_hdr = vec3(r, g, b);

    // Highly responsive Bloom from slider
    vec3 bloom = texture(u_bloom_blur, sample_uv).rgb;
    vec3 composite = scene_hdr + bloom * u_bloom_intensity * 1.5;

    // Eye Adaptation / Dynamic Auto-Exposure
    float luma = dot(composite, vec3(0.2126, 0.7152, 0.0722));
    float auto_exposure = 1.0 / sqrt(luma + 0.18);
    auto_exposure = clamp(auto_exposure, 0.45, 1.35);
    composite *= auto_exposure;

    // Cinematic Vignette
    float vignette = smoothstep(0.95, 0.35, length(center_offset));
    composite *= vignette;

    // ACES Filmic Tone Mapping & Gamma Correction (Gamma 2.2)
    vec3 ldr = ACESFilm(composite);
    vec3 final_color = pow(ldr, vec3(1.0 / 2.2));

    frag_color = vec4(final_color, 1.0);
}
