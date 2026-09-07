#version 450 core

in vec2 v_uv;
out vec4 frag_color;

#define DEBUG_MODE 0

layout(std140, binding = 0) uniform RaymarchingUniforms {
    vec4 u_resolution_time;   // xy: resolution, z: total_time, w: delta_time
    vec4 u_cam_pos;           // xyz: camera position, w: camera roll
    vec4 u_cam_dir;           // xyz: forward direction, w: dynamic FOV
    vec4 u_cam_up;            // xyz: up vector, w: camera speed
    vec4 u_biome_weights_1;   // x: w_ocean, y: w_metal, z: w_cyber, w: w_ethereal
    vec4 u_biome_weights_2;   // x: w_funk, y: valence, z: arousal, w: norm_centroid
    vec4 u_physical_params;   // x: elastic_dilation, y: surface_ripple, z: emission_pulse, w: melodic_mids
    vec4 u_laser_extra;       // xyz: laser light position, w: treble_sparkle
    vec4 u_extra_params;      // x: is_silent, y: bpm, z: bpm_conf, w: speed_forward
};

// Continuous HSV to RGB Converter
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

// Polynomial Smooth Minimum
float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

// 2D Rotation Matrix
mat2 rot(float a) {
    float s = sin(a), c = cos(a);
    return mat2(c, s, -s, c);
}

// Smooth 3D Hash for Stars & Volumetric Sparkles
float hash3(vec3 p) {
    p = fract(p * vec3(443.897, 441.423, 437.195));
    p += dot(p, p.yzx + 19.19);
    return fract((p.x + p.y) * p.z);
}

// =============================================================================
// BIOME 1: Silk Ocean & Bioluminescent Dew (Lofi, Jazz, Chill, Ambient)
// Organic silky sea undulating with sub-bass dilation & melodic mids
// =============================================================================
float sdf_ocean(vec3 p, vec3 cam_p, float dilation, float mids, float is_silent, float beat_phase) {
    float wave_scale = 1.0 - is_silent * 0.95;

    // Harmonic undulating sea sculpted directly by acoustic forces
    float wave1 = sin(p.x * 0.35 + beat_phase * 0.4) * cos(p.z * 0.28 + beat_phase * 0.3) * (0.30 * wave_scale + dilation * 0.90);
    float wave2 = sin((p.x + p.z) * 0.60 + beat_phase * 0.5) * (0.15 * wave_scale + mids * 0.45);
    float wave_bass = sin(length(p.xz - cam_p.xz) * 0.25 - beat_phase * 0.6) * (dilation * 0.65 * wave_scale);
    float ocean_y = -2.2 + (wave1 + wave2 + wave_bass);
    float d_ocean = p.y - ocean_y;

    // Floating bioluminescent dew pearls on the flanks
    vec3 q_orb = p - cam_p;
    q_orb.x = abs(q_orb.x) - 5.2;
    q_orb.z = mod(q_orb.z + 7.0, 14.0) - 7.0;
    q_orb.y = p.y - (1.2 + 0.3 * sin(p.z * 0.35 + beat_phase * 0.4));
    float d_orb = length(q_orb) - (1.0 + dilation * 0.50 + mids * 0.30);

    return smin(d_ocean, d_orb, 1.10);
}

// =============================================================================
// BIOME 2: Obsidian Fracture & Magma Spires (Metal, Hard Rock, Hand of Blood)
// Crystalline obsidian colonnades and aggressive magma ground spikes
// =============================================================================
float sdf_metal(vec3 p, vec3 cam_p, float dilation, float mids, float time, float beat_phase) {
    // Grand Lateral Obsidian Colonnade (|x| >= 6.2)
    vec3 q_mono = p - cam_p;
    q_mono.z = mod(q_mono.z + 6.0, 12.0) - 6.0;
    q_mono.x = abs(q_mono.x) - 6.2;
    q_mono.xy = rot(p.z * 0.20 + mids * 0.5 + time * 0.04) * q_mono.xy;
    
    vec3 d_shard = abs(q_mono) - vec3(0.70 + mids * 0.45, 0.70 + mids * 0.45, 3.8 + dilation * 1.2);
    float d_monolith = max(d_shard.x, max(d_shard.y, d_shard.z));

    // Lateral razor ground spikes on the flanks sculpted by bass and mids
    float flank_dist = max(abs(p.x - cam_p.x) - 3.5, 0.0);
    float spike_height = abs(sin(p.x * 1.4) * cos(p.z * 1.4)) * (0.6 + dilation * 1.8 + mids * 1.0);
    float spike_ground = p.y - (-2.6 + spike_height * smoothstep(0.0, 2.0, flank_dist));

    return min(d_monolith, spike_ground);
}

// =============================================================================
// BIOME 3: Quantum Matrix & Equalizer Skyscraper City (Dubstep, Techno, EDM)
// Towering equalizer skyscrapers and quantum gate arches overhead
// =============================================================================
float sdf_cyber(vec3 p, vec3 cam_p, float dilation, float mids) {
    // Open grid highway floor
    float d_floor = p.y - (-2.4 - dilation * 0.3);

    // Audio-Reactive Equalizer Towers rising on the lateral flanks
    vec3 q_eq = p - cam_p;
    float tower_idx = floor((p.z + 2.5) / 5.0);
    q_eq.z = mod(q_eq.z + 2.5, 5.0) - 2.5;
    
    float tower_seed = hash3(vec3(tower_idx, sign(p.x), 1.0));
    float eq_height = 0.6 + (dilation * 3.5 + mids * 2.0) * (0.35 + 0.65 * tower_seed);
    
    vec3 d_eq_box = abs(vec3(abs(q_eq.x) - 5.5, q_eq.y - (-2.4 + eq_height * 0.5), q_eq.z)) - vec3(0.70, eq_height * 0.5, 0.70);
    float d_eq = max(d_eq_box.x, max(d_eq_box.y, d_eq_box.z));

    // Quantum Gate Arches overhead every 10 meters
    vec3 q_arch = p - cam_p;
    q_arch.z = mod(p.z + 5.0, 10.0) - 5.0;
    float arch_body = abs(length(vec2(q_arch.x, max(q_arch.y + 1.0, 0.0))) - (4.2 + dilation * 0.4)) - 0.18;
    float d_arch = max(arch_body, abs(q_arch.z) - 0.25);

    return min(d_floor, min(d_eq, d_arch));
}

// =============================================================================
// BIOME 4: Ethereal Nebula & Floating Celestial Rings (Symphonic, Dreamy)
// Floating concentric golden rings of light and astral obelisks
// =============================================================================
float sdf_ethereal(vec3 p, vec3 cam_p, float dilation, float mids, float time, float beat_phase) {
    // Floating Concentric Golden Rings of Light
    vec3 q_ring = p - cam_p;
    q_ring.z = mod(p.z + 6.0, 12.0) - 6.0;
    q_ring.xy = rot(beat_phase * 0.20 + time * 0.03) * q_ring.xy;
    vec2 t1 = vec2(length(q_ring.xy) - (3.4 + mids * 0.7), abs(q_ring.z) - 0.15);
    float d_ring1 = length(t1) - 0.10;

    // Floating Golden Obelisks on the sides
    vec3 q_obelisk = p - cam_p;
    q_obelisk.x = abs(q_obelisk.x) - 5.8;
    q_obelisk.z = mod(q_obelisk.z + 5.0, 10.0) - 5.0;
    q_obelisk.y = p.y - 1.2;
    vec3 d_ob_box = abs(q_obelisk) - vec3(0.40, 3.2 + dilation * 0.8 + mids * 0.4, 0.40);
    float d_obelisk = max(d_ob_box.x, max(d_ob_box.y, d_ob_box.z));

    return min(d_ring1, d_obelisk);
}

// =============================================================================
// BIOME 5: Psychedelic Acid Funk & Jelly Hyper-Tubes (Snarez / Frog Family)
// Bouncy undulating floor & jumping metaballs locked to tempo & bassline
// =============================================================================
float sdf_funk(vec3 p, vec3 cam_p, float dilation, float mids, float beat_phase) {
    // Bouncy jelly ground sculpted by funky bassline
    float funk_wave = sin(p.x * 0.8 + beat_phase * 0.5) * sin(p.z * 0.6 + beat_phase * 0.3) * (dilation * 1.1 + mids * 0.7);
    float funk_y = -2.2 + funk_wave;
    float d_funk_ground = p.y - funk_y;

    // Morphing funky bouncing metaballs locked to tempo
    vec3 q_jelly = p - cam_p;
    q_jelly.x = abs(q_jelly.x) - 4.5;
    q_jelly.z = mod(q_jelly.z + 5.0, 10.0) - 5.0;
    float bounce = abs(sin(beat_phase * 0.5 + p.z * 0.2)) * (0.4 + dilation * 0.9);
    q_jelly.y = p.y - (0.8 + bounce);
    float d_jelly = length(q_jelly) - (0.85 + dilation * 0.60 + mids * 0.35);

    // Twisted Chromatic Corkscrew Arches overhead
    vec3 q_arch = p - cam_p;
    q_arch.z = mod(p.z + 4.0, 8.0) - 4.0;
    q_arch.xy = rot(p.z * 0.25 + beat_phase * 0.25) * q_arch.xy;
    float d_arch = abs(length(q_arch.xy) - (3.6 + dilation * 0.4)) - 0.20;

    return smin(d_funk_ground, min(d_jelly, d_arch), 0.80);
}

// =============================================================================
// Master 5-Biome Barycentric Distance Map
// =============================================================================
float map(vec3 p) {
    float time = u_resolution_time.z;
    vec3 cam_p = u_cam_pos.xyz;

    float w_ocean    = u_biome_weights_1.x;
    float w_metal    = u_biome_weights_1.y;
    float w_cyber    = u_biome_weights_1.z;
    float w_ethereal = u_biome_weights_1.w;
    float w_funk     = u_biome_weights_2.x;

    float dilation   = u_physical_params.x;
    float ripple     = u_physical_params.y;
    float mids       = u_physical_params.w;
    float is_silent  = u_extra_params.x;
    float beat_phase = u_extra_params.z;

    // Evaluate the 5 biomes (all sculpted by audio forces)
    float d_o = sdf_ocean(p, cam_p, dilation, mids, is_silent, beat_phase);
    float d_m = sdf_metal(p, cam_p, dilation, mids, time, beat_phase);
    float d_c = sdf_cyber(p, cam_p, dilation, mids);
    float d_e = sdf_ethereal(p, cam_p, dilation, mids, time, beat_phase);
    float d_f = sdf_funk(p, cam_p, dilation, mids, beat_phase);

    // 5-Way Barycentric Interpolation
    float d_interpolated = w_ocean * d_o + w_metal * d_m + w_cyber * d_c + w_ethereal * d_e + w_funk * d_f;

    // Transient Onset Shockwave
    if (ripple > 0.01) {
        float r = length(p - cam_p);
        float shock = sin(r * 2.5 - beat_phase * 2.0) * exp(-0.20 * r) * ripple * 0.65;
        d_interpolated += shock;
    }

    return d_interpolated;
}

// Tetrahedron normal computation
vec3 calcNormal(vec3 p) {
    const vec2 e = vec2(0.002, -0.002);
    return normalize(
        e.xyy * map(p + e.xyy) +
        e.yyx * map(p + e.yyx) +
        e.yxy * map(p + e.yxy) +
        e.xxx * map(p + e.xxx)
    );
}

// Ambient Occlusion
float calcAO(vec3 p, vec3 n) {
    float occ = 0.0;
    float sca = 1.0;
    for (int i = 0; i < 5; ++i) {
        float h = 0.04 + 0.20 * float(i) / 4.0;
        float d = map(p + h * n);
        occ += (h - d) * sca;
        sca *= 0.80;
    }
    return clamp(1.0 - 1.4 * occ, 0.0, 1.0);
}

// Smooth Spherical Starfield & Celestial Sky
vec3 getSkyColor(vec3 rd, float base_hue, float arousal, float is_silent, float w_metal, float w_cyber, float w_funk) {
    float horizon = smoothstep(-0.2, 0.45, rd.y);
    
    // Ambient night sky colored by genre
    vec3 zenith_color = vec3(0.008, 0.012, 0.024);
    if (w_metal > 0.35) {
        zenith_color = vec3(0.035, 0.008, 0.025); // Dark crimson/violet space for Metal
    } else if (w_cyber > 0.35) {
        zenith_color = vec3(0.008, 0.022, 0.045); // Electric dark cyan space for Cyber
    } else if (w_funk > 0.35) {
        zenith_color = vec3(0.025, 0.030, 0.008); // Acid lime/yellow cosmos for Funk
    }

    vec3 horizon_color = hsv2rgb(vec3(base_hue, 0.65, mix(0.24 + arousal * 0.10, 0.08, is_silent)));
    vec3 sky = mix(horizon_color, zenith_color, horizon);

    // Spherical 3D Stars
    vec3 star_dir = normalize(rd);
    float star_val = hash3(floor(star_dir * 120.0));
    if (star_val > 0.988) {
        float blink = sin(star_val * 80.0 + u_resolution_time.z * 1.5) * 0.5 + 0.5;
        float star_brightness = smoothstep(0.988, 1.0, star_val) * blink;
        sky += vec3(star_brightness * 0.95);
    }

    // Upper Aurora Ribbon
    if (rd.y > 0.1) {
        float aurora_wave = sin(rd.x * 6.0 + u_resolution_time.z * 0.2) * cos(rd.z * 4.0);
        float aurora_band = smoothstep(0.3, 0.6, rd.y + aurora_wave * 0.15) * smoothstep(0.8, 0.5, rd.y);
        vec3 aurora_color = hsv2rgb(vec3(fract(base_hue + 0.35), 0.75, 0.35 * (1.0 - is_silent * 0.8)));
        sky += aurora_color * aurora_band * 0.6;
    }

    return sky;
}

void main() {
    vec2 resolution = max(u_resolution_time.xy, vec2(1.0));
    vec2 uv = (gl_FragCoord.xy - 0.5 * resolution) / resolution.y;

    // Camera ray setup
    vec3 ro = u_cam_pos.xyz;
    vec3 fwd = u_cam_dir.xyz;
    vec3 up = u_cam_up.xyz;
    vec3 right = normalize(cross(fwd, up));

    float fov = max(u_cam_dir.w, 0.1);
    vec3 rd = normalize(fwd + (uv.x * right + uv.y * up) * tan(fov * 0.5));

    float time = u_resolution_time.z;
    float w_ocean    = u_biome_weights_1.x;
    float w_metal    = u_biome_weights_1.y;
    float w_cyber    = u_biome_weights_1.z;
    float w_ethereal = u_biome_weights_1.w;
    float w_funk     = u_biome_weights_2.x;

    float emission_pulse = u_physical_params.z;
    float melodic_mids   = u_physical_params.w;
    float spec_centroid  = u_biome_weights_2.w;
    float arousal        = u_biome_weights_2.z;
    float is_silent      = u_extra_params.x;

    // Synesthetic color palette by 5 specialized genres
    float base_hue = 0.0;
    if (w_metal > 0.35) {
        base_hue = fract(0.92 + spec_centroid * 0.15 + time * 0.002);
    } else if (w_cyber > 0.35) {
        base_hue = fract(0.52 + spec_centroid * 0.30 + time * 0.003);
    } else if (w_funk > 0.35) {
        base_hue = fract(0.24 + spec_centroid * 0.35 + time * 0.004);
    } else if (w_ethereal > 0.35) {
        base_hue = fract(0.12 + spec_centroid * 0.20 + time * 0.002);
    } else {
        base_hue = fract(0.10 + spec_centroid * 0.30 + time * 0.002);
    }

    float sat = mix(0.85, 0.35, w_metal);
    float val = (0.75 + emission_pulse * 0.35) * (1.0 - is_silent * 0.6);
    vec3 synesthetic_color = hsv2rgb(vec3(base_hue, sat, val));

    // Deep cosmic background sky
    vec3 sky_color = getSkyColor(rd, base_hue, arousal, is_silent, w_metal, w_cyber, w_funk);

    // Raymarching loop
    float t = 0.05;
    float t_max = 75.0;
    float hit_dist = 0.0;
    vec3 p = ro;
    bool hit = false;

    vec3 accum_glow = vec3(0.0);
    vec3 laser_pos = u_laser_extra.xyz;

    for (int i = 0; i < 75; ++i) {
        p = ro + rd * t;
        hit_dist = map(p);

        // Volumetric God Rays & Atmospheric scattering toward laser entity
        float d_laser = length(p - laser_pos);
        float god_ray = 1.0 / (1.0 + d_laser * d_laser * 0.10);
        float density = 0.005 / (1.0 + max(hit_dist, 0.0) * 5.0);
        accum_glow += synesthetic_color * (density + god_ray * 0.015 * (0.7 + melodic_mids * 1.1)) * (0.6 + emission_pulse * 0.4) * (1.0 - is_silent * 0.7);

        if (hit_dist < 0.003) {
            hit = true;
            break;
        }

        t += hit_dist * 0.88;
        if (t >= t_max) break;
    }

#if DEBUG_MODE
    if (hit) {
        vec3 n = calcNormal(p);
        frag_color = vec4(n * 0.5 + 0.5, 1.0);
    } else {
        frag_color = vec4(sky_color, 1.0);
    }
    return;
#endif

    vec3 scene_color = vec3(0.0);

    if (hit) {
        vec3 n = calcNormal(p);
        float ao = calcAO(p, n);

        // 3D Point Light from Laser Ribbon Entity
        vec3 l_dir = laser_pos - p;
        float l_dist = length(l_dir);
        l_dir = normalize(l_dir);

        float l_atten = 1.0 / (1.0 + l_dist * 0.15 + l_dist * l_dist * 0.04);
        float diff = max(dot(n, l_dir), 0.0);

        // Specular highlight
        vec3 view_dir = -rd;
        vec3 half_v = normalize(l_dir + view_dir);
        float spec_power = mix(24.0, 110.0, w_metal);
        float spec = pow(max(dot(n, half_v), 0.0), spec_power);

        // Fresnel reflection
        float fresnel = pow(clamp(1.0 - max(dot(n, view_dir), 0.0), 0.0, 1.0), 2.8);
        vec3 iridescence = mix(sky_color, synesthetic_color * 1.3, fresnel);

        vec3 albedo = synesthetic_color * 0.60;
        vec3 laser_light = synesthetic_color * (diff * 2.2 + spec * 3.2) * l_atten * (1.0 - is_silent * 0.8);
        vec3 ambient = sky_color * (ao * 1.2 + 0.35);

        scene_color = (albedo * (ambient + laser_light) + iridescence * fresnel * 1.8) * (ao * ao);

        // Depth fog
        float fog = 1.0 - exp(-t * 0.028);
        scene_color = mix(scene_color, sky_color, fog);
    } else {
        scene_color = sky_color;
    }

    // Additive volumetric glow & God Rays
    scene_color += accum_glow;

    frag_color = vec4(scene_color, 1.0);
}
