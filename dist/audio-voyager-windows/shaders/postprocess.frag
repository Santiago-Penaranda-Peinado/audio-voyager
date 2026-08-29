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

// ACES Film Tone Mapping curve
vec3 ACESFilm(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Pseudo-random hash
float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

// 2D Kinetic Speed Lines (Anime Hyper-Warp Streaks)
float compute_speed_lines(vec2 uv, float intensity, float time) {
    if (intensity <= 0.01) return 0.0;
    vec2 c = uv - 0.5;
    float r = length(c);
    float angle = atan(c.y, c.x);

    // Discrete angular radial sectors
    float sector = floor((angle + 3.14159) / 6.28318 * 64.0);
    float h = hash(vec2(sector, floor(time * 18.0)));

    if (h > 0.65) {
        float line_val = smoothstep(0.65, 0.98, h);
        float radial_mask = smoothstep(0.18, 0.55, r);
        return line_val * radial_mask * intensity * 1.5;
    }
    return 0.0;
}

void main() {
    vec2 uv = v_uv;

    // 1. High-Treble / Metal Scream Horizontal CRT Glitch Tearing
    float slice_trig = sin(uv.y * 140.0 + u_time * 75.0);
    vec2 glitch_uv = uv;
    if (u_glitch_amount > 0.25 && abs(slice_trig) > 0.82) {
        float dir = (slice_trig > 0.0 ? 1.0 : -1.0);
        glitch_uv.x += dir * u_glitch_amount * 0.028;
    }

    vec2 center_offset = glitch_uv - 0.5;
    float dist_sq = dot(center_offset, center_offset);

    // 2. Radial Chromatic Aberration (intensifies during glitch tearing)
    float ca_strength = u_chromatic_aberration + u_glitch_amount * 0.045;
    vec2 ca_offset = center_offset * dist_sq * ca_strength;

    float r = texture(u_scene_hdr, glitch_uv + ca_offset).r;
    float g = texture(u_scene_hdr, glitch_uv).g;
    float b = texture(u_scene_hdr, glitch_uv - ca_offset).b;
    vec3 scene_hdr = vec3(r, g, b);

    // 3. Additive HDR Bloom Compositing
    vec3 bloom = texture(u_bloom_blur, glitch_uv).rgb;
    vec3 composite = scene_hdr + bloom * u_bloom_intensity;

    // 4. 2D Kinetic Speed Lines Overlay
    float speed_lines = compute_speed_lines(uv, u_speed_lines, u_time);
    composite += vec3(speed_lines) * 1.8;

    // 5. High-Treble Static Scanlines (The Scream effect)
    if (u_glitch_amount > 0.3) {
        float scanline = sin(glitch_uv.y * u_resolution.y * 0.5) * 0.5 + 0.5;
        composite *= (1.0 - scanline * u_glitch_amount * 0.25);
    }

    // 6. Vignette (Cinematic edge darkening)
    float vignette = smoothstep(0.95, 0.35, length(center_offset));
    composite *= vignette;

    // 7. ACES Filmic Tone Mapping & Gamma Correction
    vec3 ldr = ACESFilm(composite);
    vec3 final_color = pow(ldr, vec3(1.0 / 2.2));

    frag_color = vec4(final_color, 1.0);
}
