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
uniform float u_melodic_mids;
uniform vec2 u_sun_pos;

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

    // High-Performance Screen-Space Radial Volumetric God Rays (32 samples from Horizon Sun)
    if (u_melodic_mids > 0.015) {
        vec2 sun_pos = (u_sun_pos.x == 0.0 && u_sun_pos.y == 0.0) ? vec2(0.5, 0.52) : u_sun_pos;
        vec2 ray_delta = (sample_uv - sun_pos) * (1.0 / 32.0) * 0.85;
        vec2 ray_uv = sample_uv;
        vec3 god_rays = vec3(0.0);
        float illumination = 1.0;
        const float decay = 0.96;
        const float weight = 0.045;

        for (int i = 0; i < 32; ++i) {
            ray_uv -= ray_delta;
            vec3 ray_sample = texture(u_scene_hdr, clamp(ray_uv, 0.0, 1.0)).rgb;
            float sample_lum = dot(ray_sample, vec3(0.2126, 0.7152, 0.0722));
            if (sample_lum > 0.35) {
                god_rays += ray_sample * illumination * weight;
            }
            illumination *= decay;
        }

        float ray_gain = clamp(u_melodic_mids * 1.6, 0.0, 2.2);
        composite += god_rays * ray_gain;
    }

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
