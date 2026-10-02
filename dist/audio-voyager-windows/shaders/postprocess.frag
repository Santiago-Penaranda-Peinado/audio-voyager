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

    // Clean optical bloom compositing
    vec3 bloom = texture(u_bloom_blur, sample_uv).rgb;
    vec3 composite = scene_hdr + bloom * u_bloom_intensity;

    // Radial speed lines (warp streaks during high-speed sprints / beat drops)
    if (u_speed_lines > 0.02) {
        float angle = atan(center_offset.y, center_offset.x);
        float r_dist = length(center_offset);
        // High-frequency angular noise lines streaming inwards
        float streak = sin(angle * 48.0 + u_time * 8.0) * sin(angle * 96.0 - u_time * 12.0);
        streak = smoothstep(0.40, 0.95, streak);
        // Radial mask: visible towards periphery, crystal clear at center
        float radial_mask = smoothstep(0.25, 0.75, r_dist);
        float line_alpha = streak * radial_mask * u_speed_lines;
        composite += vec3(line_alpha * 0.45);
    }

    // Eye Adaptation / Dynamic Auto-Exposure (stabilized against whitening)
    float luma = dot(composite, vec3(0.2126, 0.7152, 0.0722));
    float auto_exposure = 1.0 / sqrt(luma + 0.22);
    auto_exposure = clamp(auto_exposure, 0.55, 1.25);
    composite *= auto_exposure;

    // Cinematic Vignette
    float vignette = smoothstep(0.95, 0.35, length(center_offset));
    composite *= vignette;

    // ACES Filmic Tone Mapping & Gamma Correction (Gamma 2.2)
    vec3 ldr = ACESFilm(composite);
    vec3 final_color = pow(ldr, vec3(1.0 / 2.2));

    frag_color = vec4(final_color, 1.0);
}
