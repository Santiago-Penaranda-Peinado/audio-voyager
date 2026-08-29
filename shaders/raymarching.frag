#version 450 core

in vec2 v_uv;
out vec4 frag_color;

#define DEBUG_MODE 0

layout(std140, binding = 0) uniform RaymarchingUniforms {
    vec4 u_resolution_time;   // xy: resolution, z: total_time, w: delta_time
    vec4 u_cam_pos;           // xyz: camera position, w: camera roll
    vec4 u_cam_dir;           // xyz: forward direction, w: dynamic FOV
    vec4 u_cam_up;            // xyz: up vector, w: camera speed
    vec4 u_biome_weights;     // x: w_liquid, y: w_crystal, z: w_cyber, w: valence
    vec4 u_physical_params;   // x: elastic_dilation, y: surface_ripple, z: emission_pulse, w: norm_centroid
    vec4 u_laser_pos;         // xyz: laser light position, w: arousal
    vec4 u_extra_physics;     // x: melodic_mids, y: treble_sparkle, z: is_silent, w: bpm
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

// Smooth 3D Hash for Procedural Stars
float hash3(vec3 p) {
    p = fract(p * vec3(443.897, 441.423, 437.195));
    p += dot(p, p.yzx + 19.19);
    return fract((p.x + p.y) * p.z);
}

// =============================================================================
// BIOME 1: Liquid Silk Ocean (Lofi, Chill, Acoustic, Frog Family)
// Océano suave con oleaje armónico potente, perlas amigables y cielo ámbar/esmeralda
// =============================================================================
float sdf_liquid(vec3 p, vec3 cam_p, float dilation, float mids, float is_silent, float time) {
    float wave_scale = 1.0 - is_silent * 0.95;

    // Harmonic multi-octave liquid waves with high amplitude reaction to sub-bass
    float wave1 = sin(p.x * 0.35 + time * 1.4) * cos(p.z * 0.28 + time * 1.0) * (0.55 * wave_scale + dilation * 0.85);
    float wave2 = sin((p.x + p.z) * 0.70 + time * 2.0) * (0.28 * wave_scale + mids * 0.45);
    float wave_bass = sin(length(p.xz - cam_p.xz) * 0.25 - time * 3.0) * (dilation * 0.75 * wave_scale);
    float ocean_y = -2.2 + (wave1 + wave2 + wave_bass);
    float d_ocean = p.y - ocean_y;

    // Floating bioluminescent mercury dew pearls
    vec3 q_orb = p - cam_p;
    q_orb.xz = mod(q_orb.xz + vec2(7.0), 14.0) - vec2(7.0);
    q_orb.y = p.y - (0.4 + 0.8 * sin(p.z * 0.35 + time * 1.3));
    float d_orb = length(q_orb) - (0.95 + dilation * 0.50 + mids * 0.30);

    // Floating torus rings
    vec3 q_ring = p - cam_p;
    q_ring.xz = mod(q_ring.xz + vec2(7.0), 14.0) - vec2(7.0);
    q_ring.y = p.y - 1.4;
    vec2 t_ring = vec2(length(q_ring.xz) - 1.8, q_ring.y);
    float d_ring = length(t_ring) - 0.09;

    float d_floating = min(d_orb, d_ring);
    return smin(d_ocean, d_floating, 1.10);
}

// =============================================================================
// BIOME 2: Razor Crystal & Obsidian Spikes (Metal, Rock, Hand of Blood)
// Espacio roto con espinas afiladas de obsidiana, monolitos y fractales angulares
// =============================================================================
float sdf_crystal(vec3 p, vec3 cam_p, float dilation, float mids, float time) {
    vec3 q_grid = p - cam_p;
    q_grid = mod(q_grid + vec3(6.0), 12.0) - vec3(6.0);

    // Rotating sharp razor quartz monolith
    vec3 q_mono = q_grid;
    q_mono.xy = rot(p.z * 0.35 + time * 0.5) * q_mono.xy;
    q_mono.yz = rot(p.x * 0.30 + time * 0.4) * q_mono.yz;
    
    vec3 d_shard = abs(q_mono) - vec3(0.50 + mids * 0.25, 0.50 + mids * 0.25, 2.4 + dilation * 0.7);
    float d_monolith = max(d_shard.x, max(d_shard.y, d_shard.z));

    // Floor of razor obsidian spikes surging with double-bass kicks
    float spike_ground = p.y - (-2.6 + 0.8 * abs(sin(p.x * 1.2) * cos(p.z * 1.2)) * (1.0 + dilation * 1.5));

    // Floating crystal shrapnel
    vec3 q_shrap = abs(q_grid) - vec3(2.2 + dilation * 0.6);
    float d_shrapnel = length(max(q_shrap, 0.0)) - 0.25;

    return min(d_monolith, min(spike_ground, d_shrapnel));
}

// =============================================================================
// BIOME 3: Cyber Matrix & Neon Equalizer City (Dubstep, Techno, Trap Drops)
// Megaciudad cuántica con ecualizadores gigantes que emergen del suelo
// =============================================================================
float sdf_cyber(vec3 p, vec3 cam_p, float dilation, float mids, float time) {
    vec2 q_xy = abs(p.xy - cam_p.xy);
    float d_corridor = (3.8 + dilation * 0.6) - max(q_xy.x, q_xy.y);

    // Giant Audio-Reactive Equalizer Towers rising from floor & ceiling
    vec3 q_eq = p - cam_p;
    q_eq.z = mod(q_eq.z + 1.8, 3.6) - 1.8;
    float eq_height = 0.6 + 2.2 * (dilation * 1.4 + mids * 0.8) * abs(sin(floor((p.z - cam_p.z) / 3.6) * 1.4 + time * 5.0));
    vec3 d_eq_box = abs(vec3(abs(q_eq.x) - 3.0, abs(q_eq.y) - (3.6 - eq_height * 0.5), q_eq.z)) - vec3(0.40, eq_height * 0.5, 0.40);
    float d_eq = max(d_eq_box.x, max(d_eq_box.y, d_eq_box.z));

    // Periodic Quantum Accelerator Gate Rings every 6 meters
    vec3 q_ring = p - cam_p;
    q_ring.z = mod(p.z + 3.0, 6.0) - 3.0;
    float ring_body = abs(length(q_ring.xy) - (3.2 + dilation * 0.4)) - 0.14;
    float d_ring = max(ring_body, abs(q_ring.z) - 0.22);

    // 4 Neon Energy Conduit Rails
    vec2 q_rails = abs(p.xy - cam_p.xy) - vec2(2.6);
    float d_rails = length(q_rails) - 0.12;

    float d_tech = min(d_ring, min(d_rails, d_eq));
    return min(d_corridor, d_tech);
}

// =============================================================================
// Trilateral Barycentric Distance Map
// =============================================================================
float map(vec3 p) {
    float time = u_resolution_time.z;
    vec3 cam_p = u_cam_pos.xyz;

    float w_liquid  = u_biome_weights.x;
    float w_crystal = u_biome_weights.y;
    float w_cyber   = u_biome_weights.z;

    float dilation  = u_physical_params.x;
    float ripple    = u_physical_params.y;
    float mids      = u_extra_physics.x;
    float is_silent = u_extra_physics.z;

    // Evaluate the 3 biomes
    float d_l = sdf_liquid(p, cam_p, dilation, mids, is_silent, time);
    float d_x = sdf_crystal(p, cam_p, dilation, mids, time);
    float d_c = sdf_cyber(p, cam_p, dilation, mids, time);

    // Trilateral Barycentric Blending
    float d_interpolated = w_liquid * d_l + w_crystal * d_x + w_cyber * d_c;

    // Transient Onset Shockwave Ripple
    if (ripple > 0.01) {
        float r = length(p - cam_p);
        float shock = sin(r * 4.0 - time * 18.0) * exp(-0.25 * r) * ripple * 0.60;
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
        float h = 0.03 + 0.18 * float(i) / 4.0;
        float d = map(p + h * n);
        occ += (h - d) * sca;
        sca *= 0.80;
    }
    return clamp(1.0 - 1.5 * occ, 0.0, 1.0);
}

// Smooth Spherical Starfield & Celestial Sky
vec3 getSkyColor(vec3 rd, float base_hue, float arousal, float is_silent, float w_crystal, float w_cyber) {
    float horizon = smoothstep(-0.2, 0.45, rd.y);
    
    // Ambient night sky colored by genre
    vec3 zenith_color = vec3(0.008, 0.012, 0.024);
    if (w_crystal > 0.5) {
        zenith_color = vec3(0.025, 0.006, 0.018); // Dark crimson/violet space for Metal
    } else if (w_cyber > 0.5) {
        zenith_color = vec3(0.006, 0.018, 0.035); // Electric dark cyan space for Cyber
    }

    vec3 horizon_color = hsv2rgb(vec3(base_hue, 0.65, mix(0.18 + arousal * 0.08, 0.06, is_silent)));
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
        vec3 aurora_color = hsv2rgb(vec3(fract(base_hue + 0.35), 0.75, 0.30 * (1.0 - is_silent * 0.8)));
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
    float w_crystal = u_biome_weights.y;
    float w_cyber   = u_biome_weights.z;
    float valence   = u_biome_weights.w;

    float emission_pulse = u_physical_params.z;
    float spec_centroid  = u_physical_params.w;
    float arousal        = u_laser_pos.w;
    float is_silent      = u_extra_physics.z;

    // Synesthetic color palette by genre
    float base_hue = 0.0;
    if (w_crystal > 0.5) {
        // Metal / Crystal: Crimson / Deep Violet / Obsidian (0.85 -> 0.05)
        base_hue = fract(0.92 + spec_centroid * 0.15 + time * 0.003);
    } else if (w_cyber > 0.5) {
        // Dubstep / Cyber: Electric Cyan / Neon Magenta (0.50 -> 0.85)
        base_hue = fract(0.52 + spec_centroid * 0.30 + time * 0.005);
    } else {
        // Lofi / Liquid: Warm Sunset Amber / Jade Green (0.08 -> 0.40)
        base_hue = fract(0.10 + spec_centroid * 0.30 + time * 0.004);
    }

    float sat = mix(0.85, 0.30, w_crystal);
    float val = (0.70 + emission_pulse * 0.30) * (1.0 - is_silent * 0.6);
    vec3 synesthetic_color = hsv2rgb(vec3(base_hue, sat, val));

    // Deep cosmic background sky
    vec3 sky_color = getSkyColor(rd, base_hue, arousal, is_silent, w_crystal, w_cyber);

    // Raymarching loop
    float t = 0.05;
    float t_max = 75.0;
    float hit_dist = 0.0;
    vec3 p = ro;
    bool hit = false;

    vec3 accum_glow = vec3(0.0);

    for (int i = 0; i < 75; ++i) {
        p = ro + rd * t;
        hit_dist = map(p);

        // Volumetric atmospheric glow
        float density = 0.008 / (1.0 + hit_dist * hit_dist * 4.0);
        accum_glow += synesthetic_color * density * (0.5 + emission_pulse * 0.5) * (1.0 - is_silent * 0.7);

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
        vec3 laser_pos = u_laser_pos.xyz;
        vec3 l_dir = laser_pos - p;
        float l_dist = length(l_dir);
        l_dir = normalize(l_dir);

        float l_atten = 1.0 / (1.0 + l_dist * 0.15 + l_dist * l_dist * 0.04);
        float diff = max(dot(n, l_dir), 0.0);

        // Specular highlight
        vec3 view_dir = -rd;
        vec3 half_v = normalize(l_dir + view_dir);
        float spec_power = mix(24.0, 110.0, w_crystal);
        float spec = pow(max(dot(n, half_v), 0.0), spec_power);

        // Fresnel reflection
        float fresnel = pow(clamp(1.0 - max(dot(n, view_dir), 0.0), 0.0, 1.0), 2.8);
        vec3 iridescence = mix(sky_color, synesthetic_color * 1.3, fresnel);

        vec3 albedo = synesthetic_color * 0.60;
        vec3 laser_light = synesthetic_color * (diff * 2.5 + spec * 3.5) * l_atten * (1.0 - is_silent * 0.8);
        vec3 ambient = sky_color * (ao * 1.2 + 0.3);

        scene_color = (albedo * (ambient + laser_light) + iridescence * fresnel * 2.0) * ao;

        // Depth fog
        float fog = 1.0 - exp(-t * 0.030);
        scene_color = mix(scene_color, sky_color, fog);
    } else {
        scene_color = sky_color;
    }

    // Additive volumetric glow
    scene_color += accum_glow;

    frag_color = vec4(scene_color, 1.0);
}
