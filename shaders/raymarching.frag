#version 450 core

in vec2 v_uv;
out vec4 frag_color;

#define DEBUG_MODE 0

layout(std140, binding = 0) uniform RaymarchingUniforms {
    vec4 u_resolution_time;   // xy: resolution, z: total_time, w: delta_time
    vec4 u_cam_pos;           // xyz: camera position, w: camera roll
    vec4 u_cam_dir;           // xyz: forward direction, w: dynamic FOV
    vec4 u_cam_up;            // xyz: up vector, w: camera speed
    vec4 u_biome_weights;     // x: w_liquid, y: w_metal, z: w_cyber, w: w_dubstep
    vec4 u_physical_params;   // x: elastic_dilation, y: surface_ripple, z: emission_pulse, w: norm_centroid
    vec4 u_laser_pos;         // xyz: laser light position, w: arousal
    vec4 u_extra_physics;     // x: melodic_mids, y: treble_sparkle, z: is_silent, w: bpm
    vec4 u_color_primary;     // rgb: smoothed primary color
    vec4 u_color_accent;      // rgb: smoothed accent color
    vec4 u_color_zenith;      // rgb: smoothed zenith sky color
};

uniform mat4 u_view_proj;
uniform sampler2D u_waterfall_energy;

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

// Polynomial Smooth Maximum (C1 continuous subtraction / intersection)
float smax(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (a - b) / k, 0.0, 1.0);
    return mix(b, a, h) + k * h * (1.0 - h);
}

// 2D Rotation Matrix
mat2 rot(float a) {
    float s = sin(a), c = cos(a);
    return mat2(c, s, -s, c);
}

// Smooth 3D Hash for Procedural Stars
float hash3(vec3 p) {
    p = fract(p * vec3(443.897, 441.423, 437.195));
    p += dot(p, p.yzx + 19.19);
    return fract((p.x + p.y) * p.z);
}

// =============================================================================
// BIOME 1: Liquid Silk Ocean (Jazz, Lofi, Chill, Acoustic, Ambient)
// Oceano suave con oleaje armonico, perlas bioluminiscentes y anillos dorados
// =============================================================================
float sdf_liquid(vec3 p, vec3 cam_p, float dilation, float mids, float is_silent, float time) {
    float wave_scale = 1.0 - is_silent * 0.95;

    // Harmonic multi-octave liquid waves with high amplitude reaction to sub-bass
    float wave1 = sin(p.x * 0.35 + time * 1.4) * cos(p.z * 0.28 + time * 1.0) * (0.55 * wave_scale + dilation * 0.85);
    float wave2 = sin((p.x + p.z) * 0.70 + time * 2.0) * (0.28 * wave_scale + mids * 0.45);
    float wave_bass = sin(length(p.xz - cam_p.xz) * 0.25 - time * 3.0) * (dilation * 0.75 * wave_scale);
    float ocean_y = -2.2 + (wave1 + wave2 + wave_bass);
    float d_ocean = p.y - ocean_y;

    // Floating bioluminescent mercury dew pearls flanking path (using world p.z so they fly past!)
    vec3 q_orb = p - cam_p;
    q_orb.z = mod(p.z + 7.0, 14.0) - 7.0;
    float side = (q_orb.x >= 0.0) ? 1.0 : -1.0;
    q_orb.x = abs(q_orb.x) - (4.8 + dilation * 0.6);
    q_orb.y = (p.y - cam_p.y) - (0.4 + 0.8 * sin(p.z * 0.35 + time * 1.3));
    float d_orb = length(q_orb) - (0.95 + dilation * 0.50 + mids * 0.30);

    // Floating torus rings flanking (using world p.z)
    vec3 q_ring = p - cam_p;
    q_ring.z = mod(p.z + 7.0, 14.0) - 7.0;
    q_ring.x = abs(q_ring.x) - (4.8 + dilation * 0.6);
    q_ring.y = (p.y - cam_p.y) - 0.2;
    vec2 t_ring = vec2(length(q_ring.xz) - 1.8, q_ring.y);
    float d_ring = length(t_ring) - 0.09;

    float d_floating = min(d_orb, d_ring);
    float d_liq_geom = smin(d_ocean, d_floating, 1.10);
    float d_flight_corridor = length(p.xy - cam_p.xy) - (2.2 + dilation * 0.3);
    return smax(d_liq_geom, -d_flight_corridor, 0.40);
}

// =============================================================================
// BIOME 2: Volcanic Obsidian Chasm & Jagged Basalt Spires (Metal, Rock, Hand of Blood)
// Canon imponente con espinas de obsidiana, monolitos de basalto y lava ardiente
// =============================================================================
float sdf_metal(vec3 p, vec3 cam_p, float dilation, float mids, float time) {
    // 1. Canyon Chasm Walls on left & right
    float chasm_width = 3.6 + dilation * 0.6;
    float d_walls = chasm_width - abs(p.x - cam_p.x);

    // 2. Chasm Floor with jagged razor crags and glowing magma seams surging with double-bass kicks
    float ground_crags = abs(sin(p.x * 1.8) * cos(p.z * 1.4)) * (0.8 + dilation * 1.4 + mids * 0.5);
    float d_ground = p.y - (-2.6 + ground_crags);

    // 3. Towering Obsidian Monoliths & Angular Spires (world p.z so they fly past!)
    vec3 q_spire = p - cam_p;
    float side = (q_spire.x >= 0.0) ? 1.0 : -1.0;
    q_spire.x = abs(q_spire.x) - (3.4 + dilation * 0.5); // strictly outside central corridor
    q_spire.z = mod(p.z + 4.0, 8.0) - 4.0;

    // Twist with guitar distortion
    q_spire.xy = rot(p.z * 0.25 + time * 0.4) * q_spire.xy;
    q_spire.yz = rot(mids * 0.8) * q_spire.yz;

    vec3 d_shard = abs(q_spire) - vec3(0.55 + mids * 0.35, 3.8 + dilation * 0.8, 0.55 + mids * 0.35);
    float d_monolith = max(d_shard.x, max(d_shard.y, d_shard.z));

    // 4. Floating angular dagger shards flanking (world p.z)
    vec3 q_dagger = abs(p - cam_p) - vec3(2.8 + dilation * 0.4, 0.8, 0.0);
    q_dagger.z = mod(p.z + 3.0, 6.0) - 3.0;
    q_dagger.xy = rot(time * 0.8 + p.z * 0.4) * q_dagger.xy;
    float d_dagger = max(abs(q_dagger.x) + abs(q_dagger.y) - (0.4 + mids * 0.2), abs(q_dagger.z) - 1.2);

    float d_canyon_geom = min(d_walls, min(d_ground, min(d_monolith, d_dagger)));

    // 5. GUARANTEED 2.4m Safe Flight Corridor around Camera:
    // Ensures distance is strictly positive at camera position (NEVER black screen!)
    float d_flight_corridor = length(p.xy - cam_p.xy) - (2.4 + dilation * 0.3);
    return smax(d_canyon_geom, -d_flight_corridor, 0.40);
}

// =============================================================================
// BIOME 3: Cyber Matrix & Neon Highway (Techno, Synthwave, House, Electro Grid)
// Megaciudad cuantizada con carriles de energia neon y torres ecualizadoras
// =============================================================================
float sdf_cyber(vec3 p, vec3 cam_p, float dilation, float mids, float time) {
    vec2 q_xy = abs(p.xy - cam_p.xy);
    float d_corridor = (3.8 + dilation * 0.6) - max(q_xy.x, q_xy.y);

    // Giant Audio-Reactive Equalizer Towers rising from floor & ceiling (world p.z)
    vec3 q_eq = p - cam_p;
    q_eq.z = mod(p.z + 1.8, 3.6) - 1.8;
    float eq_height = 0.6 + 2.2 * (dilation * 1.4 + mids * 0.8) * abs(sin(floor(p.z / 3.6) * 1.4 + time * 5.0));
    vec3 d_eq_box = abs(vec3(abs(q_eq.x) - 3.0, abs(q_eq.y) - (3.6 - eq_height * 0.5), q_eq.z)) - vec3(0.40, eq_height * 0.5, 0.40);
    float d_eq = max(d_eq_box.x, max(d_eq_box.y, d_eq_box.z));

    // Periodic Quantum Accelerator Gate Rings every 6 meters (world p.z)
    vec3 q_ring = p - cam_p;
    q_ring.z = mod(p.z + 3.0, 6.0) - 3.0;
    float ring_body = abs(length(q_ring.xy) - (3.2 + dilation * 0.4)) - 0.14;
    float d_ring = max(ring_body, abs(q_ring.z) - 0.22);

    // 4 Neon Energy Conduit Rails
    vec2 q_rails = abs(p.xy - cam_p.xy) - vec2(2.6);
    float d_rails = length(q_rails) - 0.12;

    float d_tech = min(d_ring, min(d_rails, d_eq));
    float d_cyber_geom = min(d_corridor, d_tech);
    float d_flight_corridor = length(p.xy - cam_p.xy) - (2.4 + dilation * 0.3);
    return smax(d_cyber_geom, -d_flight_corridor, 0.40);
}

// =============================================================================
// BIOME 4: Quantum Bass Abyss & Glitch Void (Dubstep, Speedcore, Skrillex, Camellia)
// Vacio gravitacional con anillos hexagonales de sub-bajo y wobble LFO
// =============================================================================
float sdf_dubstep(vec3 p, vec3 cam_p, float dilation, float mids, float time) {
    // 1. Sub-Bass LFO Wobble Chamber
    float wobble = sin(p.z * 1.2 - time * 12.0) * (dilation * 0.75);
    float tunnel_radius = 3.6 + dilation * 0.8 + wobble;
    float d_void_tunnel = tunnel_radius - length(p.xy - cam_p.xy);

    // 2. Heavy Hexagonal Quantum Gravity Rings every 4 meters (world p.z)
    vec3 q_ring = p - cam_p;
    q_ring.z = mod(p.z + 2.0, 4.0) - 2.0;
    // Hexagonal ring profile
    vec2 p_hex = abs(q_ring.xy);
    float hex_dist = max(p_hex.x * 0.866025 + p_hex.y * 0.5, p_hex.y);
    float d_hex_ring = abs(hex_dist - (3.3 + dilation * 0.6)) - 0.18;
    float d_ring_segment = max(d_hex_ring, abs(q_ring.z) - 0.26);

    // 3. Pulsating Sub-Bass Monolith Resonators along the perimeter (world p.z)
    vec3 q_res = abs(p - cam_p);
    q_res.x -= (3.4 + dilation * 0.5);
    q_res.z = mod(p.z + 3.0, 6.0) - 3.0;
    vec3 box_dim = vec3(0.45, 1.2 + dilation * 1.5 + mids * 0.6, 0.45);
    vec3 d_b = abs(q_res) - box_dim;
    float d_resonator = max(d_b.x, max(d_b.y, d_b.z));

    // 4. Glitch Fractures: Step quantizer along tunnel surface
    float glitch_step = 0.08 * sin(floor(p.z * 4.0) * 1.7 + time * 15.0) * step(0.4, dilation);
    d_void_tunnel += glitch_step;

    float d_dub_geom = min(d_void_tunnel, min(d_ring_segment, d_resonator));

    // Flight clearance corridor
    float d_flight_corridor = length(p.xy - cam_p.xy) - (2.6 + dilation * 0.3);
    return smax(d_dub_geom, -d_flight_corridor, 0.40);
}

// =============================================================================
// 4-Way Barycentric Distance Map
// =============================================================================
float map(vec3 p) {
    float time = u_resolution_time.z;
    vec3 cam_p = u_cam_pos.xyz;

    float w_liquid  = u_biome_weights.x;
    float w_metal   = u_biome_weights.y;
    float w_cyber   = u_biome_weights.z;
    float w_dubstep = u_biome_weights.w;

    float dilation  = u_physical_params.x;
    float ripple    = u_physical_params.y;
    float mids      = u_extra_physics.x;
    float is_silent = u_extra_physics.z;

    // Evaluate the 4 distinct biomes
    float d_l = sdf_liquid(p, cam_p, dilation, mids, is_silent, time);
    float d_m = sdf_metal(p, cam_p, dilation, mids, time);
    float d_c = sdf_cyber(p, cam_p, dilation, mids, time);
    float d_d = sdf_dubstep(p, cam_p, dilation, mids, time);

    // 4-Way Barycentric Interpolation
    float d_interpolated = w_liquid * d_l + w_metal * d_m + w_cyber * d_c + w_dubstep * d_d;

    // Sculpt 3D tunnel/terrain walls using 2D Waterfall energy matrix (safe forward coords)
    float forward_t = clamp((p.z - cam_p.z) / 45.0, 0.0, 1.0);
    vec2 p_rel = p.xy - cam_p.xy;
    float angle = (dot(p_rel, p_rel) > 1e-7) ? atan(p_rel.y, p_rel.x) : 0.0;
    float freq_u = fract(angle / 6.2831853 + 0.5);
    float wf_energy = textureLod(u_waterfall_energy, vec2(freq_u, forward_t), 0.0).r;
    d_interpolated -= wf_energy * 0.45;

    // Transient Onset Shockwave Ripple
    if (ripple > 0.01) {
        float r = length(p - cam_p);
        float shock = sin(r * 4.5 - time * 20.0) * exp(-0.25 * r) * ripple * 0.60;
        d_interpolated += shock;
    }

    // GUARANTEED Safe Flight Corridor across all biomes & blend states:
    // Guarantees camera center and near-frustum can NEVER clip or black out under any combination
    float d_global_corridor = length(p.xy - cam_p.xy) - (2.2 + dilation * 0.2);
    d_interpolated = smax(d_interpolated, -d_global_corridor, 0.45);

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
        float h = 0.03 + 0.18 * float(i) / 4.0;
        float d = map(p + h * n);
        occ += (h - d) * sca;
        sca *= 0.80;
    }
    return clamp(1.0 - 1.5 * occ, 0.0, 1.0);
}

// Smooth Spherical Starfield & Celestial Sky
vec3 getSkyColor(vec3 rd, float arousal, float is_silent) {
    float horizon = smoothstep(-0.2, 0.45, rd.y);
    
    vec3 zenith_color = u_color_zenith.rgb;
    vec3 horizon_color = mix(u_color_accent.rgb * 0.35, u_color_primary.rgb * 0.55, 0.5) * mix(0.18 + arousal * 0.08, 0.06, is_silent);
    vec3 sky = mix(horizon_color, zenith_color, horizon);

    // Spherical 3D Stars
    vec3 star_dir = normalize(rd);
    float star_val = hash3(floor(star_dir * 120.0));
    if (star_val > 0.988) {
        float blink = sin(star_val * 80.0 + u_resolution_time.z * 2.5) * 0.5 + 0.5;
        float star_brightness = smoothstep(0.988, 1.0, star_val) * blink;
        sky += vec3(star_brightness * 0.95);
    }

    // Upper Aurora Ribbon
    if (rd.y > 0.1) {
        float aurora_wave = sin(rd.x * 6.0 + u_resolution_time.z * 0.4) * cos(rd.z * 4.0);
        float aurora_band = smoothstep(0.3, 0.6, rd.y + aurora_wave * 0.15) * smoothstep(0.8, 0.5, rd.y);
        vec3 aurora_color = u_color_accent.rgb * 0.45 * (1.0 - is_silent * 0.8);
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
    float w_liquid  = u_biome_weights.x;
    float w_metal   = u_biome_weights.y;
    float w_cyber   = u_biome_weights.z;
    float w_dubstep = u_biome_weights.w;

    float emission_pulse = u_physical_params.z;
    float spec_centroid  = u_physical_params.w;
    float arousal        = u_laser_pos.w;
    float is_silent      = u_extra_physics.z;

    // Cohesive, temporally low-pass filtered color palette (flicker-free)
    vec3 synesthetic_color = u_color_primary.rgb * (0.80 + emission_pulse * 0.35) * (1.0 - is_silent * 0.6);
    vec3 accent_color = u_color_accent.rgb;

    // Deep cosmic background sky
    vec3 sky_color = getSkyColor(rd, arousal, is_silent);

    // Raymarching loop (Expanded render distance to 160m for grand tunnel vista)
    float t = 0.05;
    float t_max = 160.0;
    float hit_dist = 0.0;
    vec3 p = ro;
    bool hit = false;

    vec3 accum_glow = vec3(0.0);

    for (int i = 0; i < 112; ++i) {
        p = ro + rd * t;
        hit_dist = map(p);

        // Volumetric atmospheric glow (stabilized, tight density to avoid scene washout)
        float density = 0.005 / (1.0 + hit_dist * hit_dist * 6.0);
        accum_glow += synesthetic_color * density * (0.45 + emission_pulse * 0.45) * (1.0 - is_silent * 0.7);

        if (hit_dist < 0.003) {
            hit = true;
            break;
        }

        t += hit_dist * 0.88;
        if (t >= t_max) break;
    }

    // Clamp volumetric glow to prevent scene whitening during aggressive drops
    accum_glow = min(accum_glow, vec3(0.85));

#if DEBUG_MODE
    if (hit) {
        vec3 n = calcNormal(p);
        frag_color = vec4(n * 0.5 + 0.5, 1.0);
        vec4 clip = u_view_proj * vec4(p, 1.0);
        gl_FragDepth = (clip.w > 0.001) ? clamp((clip.z / clip.w) * 0.5 + 0.5, 0.0, 1.0) : 1.0;
    } else {
        frag_color = vec4(sky_color, 1.0);
        gl_FragDepth = 1.0;
    }
    return;
#endif

    vec3 scene_color = vec3(0.0);

    if (hit) {
        vec3 n = calcNormal(p);
        float ao = calcAO(p, n);

        // 3D Point Light from Laser Ribbon Entity
        vec3 laser_pos = u_laser_pos.xyz;
        vec3 l_dir = laser_pos - p;
        float l_dist = length(l_dir);
        l_dir = normalize(l_dir);

        float l_atten = 1.0 / (1.0 + l_dist * 0.15 + l_dist * l_dist * 0.04);
        float diff = max(dot(n, l_dir), 0.0);

        // Specular highlight
        vec3 view_dir = -rd;
        vec3 half_v = normalize(l_dir + view_dir);
        float spec_power = mix(24.0, 110.0, max(w_metal, w_dubstep));
        float spec = pow(max(dot(n, half_v), 0.0), spec_power);

        vec3 albedo = synesthetic_color * 0.65;

        // Biome-specific accent highlights & emissive veins (Multi-tonal harmonized materials)
        float biome_accent_pattern = 0.0;
        if (w_metal > 0.10) {
            float magma_vein = smoothstep(0.70, 0.95, abs(sin(p.x * 2.2 + p.z * 1.5)));
            biome_accent_pattern += magma_vein * w_metal * (0.55 + emission_pulse * 0.45);
        }
        if (w_cyber > 0.10) {
            float grid_line = max(smoothstep(0.92, 0.98, sin(p.z * 3.14159 * 0.5)), 
                                  smoothstep(0.92, 0.98, sin(angle * 4.0)));
            biome_accent_pattern += grid_line * w_cyber * 0.75;
        }
        if (w_dubstep > 0.10) {
            float ring_glow = smoothstep(0.65, 0.98, sin(p.z * 1.57079 - time * 6.0));
            biome_accent_pattern += ring_glow * w_dubstep * 0.65;
        }
        if (w_liquid > 0.10) {
            float caustics = smoothstep(0.55, 0.95, sin(p.x * 1.2 + time) * cos(p.z * 1.2 - time));
            biome_accent_pattern += caustics * w_liquid * 0.40;
        }

        albedo = mix(albedo, accent_color * 0.95, clamp(biome_accent_pattern, 0.0, 0.85));

        // Fresnel reflection (harmonized with secondary accent color)
        float fresnel = pow(clamp(1.0 - max(dot(n, view_dir), 0.0), 0.0, 1.0), 3.5);
        vec3 fresnel_rim = mix(albedo * 0.35, accent_color * 0.90, fresnel) * (fresnel * 0.55);

        vec3 laser_light = (synesthetic_color * (diff * 2.2) + accent_color * (spec * 2.5)) * l_atten * (1.0 - is_silent * 0.8);
        vec3 ambient = sky_color * (ao * 1.2 + 0.3);

        scene_color = (albedo * (ambient + laser_light) + fresnel_rim) * ao;

        // Depth fog (Delayed onset past 35m: crystal clear forward view of tunnel)
        float fog_dist = max(0.0, t - 35.0);
        float fog = 1.0 - exp(-fog_dist * 0.022);
        scene_color = mix(scene_color, sky_color, fog);
    } else {
        scene_color = sky_color;
    }

    // Additive volumetric glow
    scene_color += accum_glow;

    frag_color = vec4(scene_color, 1.0);

    // Write calculated NDC depth to gl_FragDepth for accurate depth buffer occlusion
    if (hit) {
        vec4 clip = u_view_proj * vec4(p, 1.0);
        gl_FragDepth = (clip.w > 0.001) ? clamp((clip.z / clip.w) * 0.5 + 0.5, 0.0, 1.0) : 1.0;
    } else {
        gl_FragDepth = 1.0;
    }
}
