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

// Smooth 3D Hash for Procedural Stars & Micro-Sparkles
float hash3(vec3 p) {
    p = fract(p * vec3(443.897, 441.423, 437.195));
    p += dot(p, p.yzx + 19.19);
    return fract((p.x + p.y) * p.z);
}

// =============================================================================
// Continuous World-Space Spline Track C(z) = (curve_x(z), curve_y(z))
// Decouples corridor and features from being glued to camera X and Y!
// Shared symmetrically between AutonomousArtDirector CPU kinematics and Shader
// Curvature is 100% audio reactive (0.0 in silence = dead straight)
// =============================================================================
vec2 get_track_spline(float z) {
    float curvature = u_color_primary.w;
    float x = (2.8 * sin(z * 0.035) + 1.2 * sin(z * 0.075)) * curvature;
    float y = 2.7 + (0.55 * sin(z * 0.025) + 0.25 * sin(z * 0.060)) * curvature;
    return vec2(x, y);
}

// =============================================================================
// BIOME 1: Liquid Silk Ocean (Jazz, Lofi, Chill, Acoustic, Ambient)
// Oceano suave con oleaje armonico, perlas bioluminiscentes y serenidad zen
// Sculpted around world spline C(p.z)
// =============================================================================
float sdf_liquid(vec3 p, vec3 cam_p, float dilation, float mids, float is_silent, float time) {
    vec2 track_c = get_track_spline(p.z);
    vec2 p_rel = p.xy - track_c;
    float wave_scale = 1.0 - is_silent * 0.95;

    // Harmonic multi-octave liquid waves with high amplitude reaction to sub-bass
    float wave1 = sin(p.x * 0.35 + time * 1.4) * cos(p.z * 0.28 + time * 1.0) * (0.55 * wave_scale + dilation * 0.85);
    float wave2 = sin((p.x + p.z) * 0.70 + time * 2.0) * (0.28 * wave_scale + mids * 0.45);
    float wave_bass = sin(length(p_rel) * 0.25 - time * 3.0) * (dilation * 0.75 * wave_scale);
    float ocean_y = (track_c.y - 2.4) + (wave1 + wave2 + wave_bass);
    float d_ocean = p.y - ocean_y;

    // Floating bioluminescent mercury dew pearls flanking spline path (only during active music)
    float d_liq_geom = d_ocean;
    if (is_silent < 0.8) {
        vec3 q_orb = vec3(p_rel.x, p_rel.y, mod(p.z + 7.0, 14.0) - 7.0);
        q_orb.x = abs(q_orb.x) - (4.8 + dilation * 0.6);
        q_orb.y = p_rel.y - (0.4 + 0.8 * sin(p.z * 0.35 + time * 1.3));
        float d_orb = length(q_orb) - (0.95 + dilation * 0.50 + mids * 0.30);
        d_liq_geom = smin(d_ocean, d_orb, 1.10);
    }

    // Dual Flight Corridor Protection (track spline corridor + local near-camera safety bubble)
    float d_track_corridor = length(p_rel) - (2.4 + dilation * 0.3);
    float d_cam_bubble     = length(p - cam_p) - (2.0 + dilation * 0.2);
    float d_flight_corridor = min(d_track_corridor, d_cam_bubble);
    return smax(d_liq_geom, -d_flight_corridor, 0.40);
}

// =============================================================================
// BIOME 2: Volcanic Obsidian Chasm & Jagged Basalt Spires (Metal, Rock, Hand of Blood)
// Canon imponente con espinas de obsidiana, monolitos de basalto y lava ardiente
// Sculpted around world spline C(p.z)
// =============================================================================
float sdf_metal(vec3 p, vec3 cam_p, float dilation, float mids, float time) {
    vec2 track_c = get_track_spline(p.z);
    vec2 p_rel = p.xy - track_c;

    // 1. Canyon Chasm Walls on left & right of track spline
    float chasm_width = 3.8 + dilation * 0.6;
    float d_walls = chasm_width - abs(p_rel.x);

    // 2. Chasm Floor with jagged razor crags and glowing magma seams below track
    float ground_crags = abs(sin(p.x * 1.8) * cos(p.z * 1.4)) * (0.8 + dilation * 1.4 + mids * 0.5);
    float d_ground = p.y - ((track_c.y - 2.6) + ground_crags);

    // 3. Towering Obsidian Monoliths & Angular Spires flanking track
    vec3 q_spire;
    q_spire.x = abs(p_rel.x) - (3.4 + dilation * 0.5);
    q_spire.y = p_rel.y;
    q_spire.z = mod(p.z + 4.0, 8.0) - 4.0;

    // Twist with guitar distortion
    q_spire.xy = rot(p.z * 0.25 + time * 0.4) * q_spire.xy;
    q_spire.yz = rot(mids * 0.8) * q_spire.yz;

    vec3 d_shard = abs(q_spire) - vec3(0.55 + mids * 0.35, 3.8 + dilation * 0.8, 0.55 + mids * 0.35);
    float d_monolith = max(d_shard.x, max(d_shard.y, d_shard.z));

    // 4. Floating angular dagger shards flanking track
    vec3 q_dagger = vec3(abs(p_rel.x) - (2.8 + dilation * 0.4), abs(p_rel.y) - 0.8, mod(p.z + 3.0, 6.0) - 3.0);
    q_dagger.xy = rot(time * 0.8 + p.z * 0.4) * q_dagger.xy;
    float d_dagger = max(abs(q_dagger.x) + abs(q_dagger.y) - (0.4 + mids * 0.2), abs(q_dagger.z) - 1.2);

    float d_canyon_geom = min(d_walls, min(d_ground, min(d_monolith, d_dagger)));

    // Dual Flight Corridor Protection (track spline corridor + local near-camera safety bubble)
    float d_track_corridor = length(p_rel) - (2.4 + dilation * 0.3);
    float d_cam_bubble     = length(p - cam_p) - (2.0 + dilation * 0.2);
    float d_flight_corridor = min(d_track_corridor, d_cam_bubble);
    return smax(d_canyon_geom, -d_flight_corridor, 0.40);
}

// =============================================================================
// BIOME 3: Cyber Matrix & Neon Highway (Techno, Synthwave, House, Electro Grid)
// Megaciudad cuantizada con carriles de energia neon y torres ecualizadoras
// Sculpted around world spline C(p.z)
// =============================================================================
float sdf_cyber(vec3 p, vec3 cam_p, float dilation, float mids, float time) {
    vec2 track_c = get_track_spline(p.z);
    vec2 p_rel = p.xy - track_c;
    vec2 q_xy = abs(p_rel);
    float d_corridor = (3.8 + dilation * 0.6) - max(q_xy.x, q_xy.y);

    // Giant Audio-Reactive Equalizer Towers rising from floor & ceiling
    vec3 q_eq = vec3(p_rel.x, p_rel.y, mod(p.z + 1.8, 3.6) - 1.8);
    float eq_height = 0.6 + 2.2 * (dilation * 1.4 + mids * 0.8) * abs(sin(floor(p.z / 3.6) * 1.4 + time * 5.0));
    vec3 d_eq_box = abs(vec3(abs(q_eq.x) - 3.0, abs(q_eq.y) - (3.6 - eq_height * 0.5), q_eq.z)) - vec3(0.40, eq_height * 0.5, 0.40);
    float d_eq = max(d_eq_box.x, max(d_eq_box.y, d_eq_box.z));

    // Periodic Quantum Accelerator Gate Rings every 6 meters around track spline
    vec3 q_ring = vec3(p_rel.x, p_rel.y, mod(p.z + 3.0, 6.0) - 3.0);
    float ring_body = abs(length(q_ring.xy) - (3.2 + dilation * 0.4)) - 0.14;
    float d_ring = max(ring_body, abs(q_ring.z) - 0.22);

    // 4 Neon Energy Conduit Rails running parallel to track spline
    vec2 q_rails = abs(p_rel) - vec2(2.6);
    float d_rails = length(q_rails) - 0.12;

    float d_tech = min(d_ring, min(d_rails, d_eq));
    float d_cyber_geom = min(d_corridor, d_tech);

    // Dual Flight Corridor Protection (track spline corridor + local near-camera safety bubble)
    float d_track_corridor = length(p_rel) - (2.4 + dilation * 0.3);
    float d_cam_bubble     = length(p - cam_p) - (2.0 + dilation * 0.2);
    float d_flight_corridor = min(d_track_corridor, d_cam_bubble);
    return smax(d_cyber_geom, -d_flight_corridor, 0.40);
}

// =============================================================================
// BIOME 4: Quantum Bass Abyss & Glitch Void (Dubstep, Speedcore, Skrillex, Camellia)
// Vacio gravitacional con anillos hexagonales de sub-bajo y wobble LFO
// Sculpted around world spline C(p.z)
// =============================================================================
float sdf_dubstep(vec3 p, vec3 cam_p, float dilation, float mids, float time) {
    vec2 track_c = get_track_spline(p.z);
    vec2 p_rel = p.xy - track_c;

    // 1. Sub-Bass LFO Wobble Chamber sculpted around track spline
    float wobble = sin(p.z * 1.2 - time * 12.0) * (dilation * 0.75);
    float tunnel_radius = 3.6 + dilation * 0.8 + wobble;
    float d_void_tunnel = tunnel_radius - length(p_rel);

    // 2. Heavy Hexagonal Quantum Gravity Rings every 4 meters around track spline
    vec3 q_ring = vec3(p_rel.x, p_rel.y, mod(p.z + 2.0, 4.0) - 2.0);
    vec2 p_hex = abs(q_ring.xy);
    float hex_dist = max(p_hex.x * 0.866025 + p_hex.y * 0.5, p_hex.y);
    float d_hex_ring = abs(hex_dist - (3.3 + dilation * 0.6)) - 0.18;
    float d_ring_segment = max(d_hex_ring, abs(q_ring.z) - 0.26);

    // 3. Pulsating Sub-Bass Monolith Resonators along the perimeter
    vec3 q_res = vec3(abs(p_rel.x) - (3.4 + dilation * 0.5), p_rel.y, mod(p.z + 3.0, 6.0) - 3.0);
    vec3 box_dim = vec3(0.45, 1.2 + dilation * 1.5 + mids * 0.6, 0.45);
    vec3 d_b = abs(q_res) - box_dim;
    float d_resonator = max(d_b.x, max(d_b.y, d_b.z));

    // 4. Glitch Fractures: Step quantizer along tunnel surface
    float glitch_step = 0.08 * sin(floor(p.z * 4.0) * 1.7 + time * 15.0) * step(0.4, dilation);
    d_void_tunnel += glitch_step;

    float d_dub_geom = min(d_void_tunnel, min(d_ring_segment, d_resonator));

    // Dual Flight Corridor Protection (track spline corridor + local near-camera safety bubble)
    float d_track_corridor = length(p_rel) - (2.6 + dilation * 0.3);
    float d_cam_bubble     = length(p - cam_p) - (2.0 + dilation * 0.2);
    float d_flight_corridor = min(d_track_corridor, d_cam_bubble);
    return smax(d_dub_geom, -d_flight_corridor, 0.40);
}

// =============================================================================
// BIOME 5: Celestial Nebula / Open-Air Stargate (Ambient, Ethereal, Post-Rock)
// Open-air cosmic biome: no ceiling, floating crystalline asteroids, stargates
// =============================================================================
float sdf_celestial(vec3 p, vec3 cam_p, float dilation, float mids, float is_silent, float time) {
    vec2 track_c = get_track_spline(p.z);
    vec2 p_rel = p.xy - track_c;

    // 1. Floating Stargate Causeway / Runway below track (Open-air, defined slab thickness, space underneath!)
    float causeway_y = track_c.y - 2.5 + sin(p.z * 0.12 + time * 0.8) * 0.25;
    float d_causeway = max(p.y - causeway_y, (causeway_y - 0.7) - p.y);
    float causeway_width = 4.2 + dilation * 0.5;
    float d_floor = max(d_causeway, abs(p_rel.x) - causeway_width);

    // 2. Floating Crystalline Asteroids flanking track (Tumbling Octahedra)
    vec3 q_ast = vec3(p_rel.x, p_rel.y, mod(p.z + 9.0, 18.0) - 9.0);
    float ast_side = sign(sin(p.z * 0.18));
    q_ast.x -= ast_side * (4.6 + dilation * 0.6);
    q_ast.y -= (0.8 + 1.2 * sin(p.z * 0.25 + time * 0.9));
    q_ast.xy = rot(p.z * 0.2 + time * 0.4) * q_ast.xy;
    q_ast.yz = rot(time * 0.3) * q_ast.yz;
    float d_asteroid = (abs(q_ast.x) + abs(q_ast.y) + abs(q_ast.z) - (1.4 + mids * 0.6)) * 0.57735027;

    // 3. Floating Monumental Stargate Rings recurring every 32m
    vec3 q_stargate = vec3(p_rel.x, p_rel.y - 0.2, mod(p.z + 16.0, 32.0) - 16.0);
    vec2 sg_ring = vec2(length(q_stargate.xy) - (4.0 + dilation * 0.5), q_stargate.z);
    float d_stargate = length(sg_ring) - 0.20;

    // 4. Floating Crystalline Shards
    vec3 q_shard = vec3(abs(p_rel.x) - (3.8 + dilation * 0.4), p_rel.y + 0.4, mod(p.z + 4.5, 9.0) - 4.5);
    q_shard.xz = rot(time * 0.7 + p.z * 0.3) * q_shard.xz;
    vec3 b_shard = abs(q_shard) - vec3(0.30, 0.90 + mids * 0.4, 0.30);
    float d_shards = max(b_shard.x, max(b_shard.y, b_shard.z));

    float d_celestial_geom = min(d_floor, min(d_asteroid, min(d_stargate, d_shards)));

    // Dual flight corridor protection (track spline corridor + local near-camera safety bubble)
    float d_track_corridor = length(p_rel) - (2.4 + dilation * 0.3);
    float d_cam_bubble     = length(p - cam_p) - (2.0 + dilation * 0.2);
    float d_flight = min(d_track_corridor, d_cam_bubble);
    return smax(d_celestial_geom, -d_flight, 0.40);
}

// =============================================================================
// PHASE 2 & 3: Procedural 3D Architectural Elements inside Raymarching SDF
// Dynamic Emergence: Portals and totems emerge dynamically during tempo/speed
// sprints and rhythmic drops, remaining clean and uncluttered in calm passages
// =============================================================================
float sdf_architecture(vec3 p, vec3 cam_p, float dilation, float mids, float ripple, float time, float emergence) {
    if (emergence < 0.01) return 1e5;
    vec2 track_c = get_track_spline(p.z);
    vec2 p_rel = p.xy - track_c;

    // 1. Neon Rhythmic Gate Portals / Monumental Arches recurring every 16m
    float gate_spacing = 16.0;
    float gate_z = mod(p.z + 8.0, gate_spacing) - 8.0;
    float r_gate = 2.65 + dilation * 0.50 + ripple * 0.30;
    float arch_depth = 0.32 + ripple * 0.12;
    float r_xy = length(p_rel);
    float arch_torus = max(abs(r_xy - r_gate) - 0.20, abs(gate_z) - arch_depth);
    vec2 p_pillar = vec2(abs(p_rel.x) - r_gate, p_rel.y + 1.0);
    float arch_pillars = max(max(abs(p_pillar.x) - 0.24, abs(p_pillar.y) - 1.4), abs(gate_z) - arch_depth);
    float d_gate_arch = min(arch_torus, arch_pillars) + (1.0 - emergence) * 6.0;

    // 2. Audio-Reactive Floating Equalizer Monoliths / Totems recurring every 6m
    float totem_spacing = 6.0;
    float cell_z = floor((p.z + 3.0) / totem_spacing);
    float totem_z = mod(p.z + 3.0, totem_spacing) - 3.0;
    float wave_phase = cell_z * 0.75 + time * 3.5;
    float totem_h = 1.1 + 2.2 * (dilation * 0.65 + mids * 0.75) * (0.65 + 0.35 * sin(wave_phase));
    float bevel = 0.04 + 0.04 * dilation;
    vec3 q_totem = vec3(abs(p_rel.x) - (3.45 + dilation * 0.35), p_rel.y - (totem_h * 0.5 - 1.1), totem_z);
    vec3 b_totem = vec3(0.30, totem_h * 0.5, 0.30) - bevel;
    vec3 d_box = abs(q_totem) - b_totem;
    float d_monolith = length(max(d_box, vec3(0.0))) + min(max(d_box.x, max(d_box.y, d_box.z)), 0.0) - bevel;
    float eq_stripes = abs(sin(q_totem.y * 6.0)) * 0.03;
    d_monolith += eq_stripes + (1.0 - emergence) * 6.0;

    // 3. 3D Extruded Holographic Energy Rails / Guardrails along spline C(z)
    float rail_x_offset = 2.22 + dilation * 0.15;
    float rail_wave = sin(p.z * 0.45 + time * 3.2) * (0.12 + mids * 0.16) + sin(p.z * 1.4 - time * 9.0) * (ripple * 0.12);
    float rail_y = -1.15 + rail_wave;
    float r_rail = 0.10 + mids * 0.03;
    float d_rail_left  = length(vec2(p_rel.x + rail_x_offset, p_rel.y - rail_y)) - r_rail;
    float d_rail_right = length(vec2(p_rel.x - rail_x_offset, p_rel.y - rail_y)) - r_rail;
    float d_rails_lower = min(d_rail_left, d_rail_right);

    float rail_high_y = 1.35 + 0.08 * cos(p.z * 0.40 + time * 2.2);
    float d_rail_high_left  = length(vec2(p_rel.x + (rail_x_offset + 0.30), p_rel.y - rail_high_y)) - 0.065;
    float d_rail_high_right = length(vec2(p_rel.x - (rail_x_offset + 0.30), p_rel.y - rail_high_y)) - 0.065;
    float d_rails_upper = min(d_rail_high_left, d_rail_high_right);

    float pylon_z = mod(p.z + 2.0, 4.0) - 2.0;
    vec2 p_strut = vec2(abs(p_rel.x) - (rail_x_offset + 0.45), p_rel.y - (-1.15));
    float d_strut = max(max(abs(p_strut.x) - 0.50, abs(p_strut.y) - 0.05), abs(pylon_z) - 0.07);
    float d_rails = min(min(d_rails_lower, d_rails_upper), d_strut) + (1.0 - emergence) * 4.0;

    return min(d_gate_arch, min(d_monolith, d_rails));
}

// =============================================================================
// Optimized Barycentric Distance Map with Branching Biomes & 3D Procedural Architecture
// Phase 3.a: Only evaluates active/dominant biomes (w > 0.02) to maximize framerate
// =============================================================================
float map(vec3 p) {
    float time = u_resolution_time.z;
    vec3 cam_p = u_cam_pos.xyz;

    float w_liquid    = u_biome_weights.x;
    float w_metal     = u_biome_weights.y;
    float w_cyber     = u_biome_weights.z;
    float w_dubstep   = u_biome_weights.w;
    float w_celestial = u_color_accent.w;

    float dilation  = u_physical_params.x;
    float ripple    = u_physical_params.y;
    float mids      = u_extra_physics.x;
    float is_silent = u_extra_physics.z;

    vec2 track_c = get_track_spline(p.z);
    vec2 p_rel = p.xy - track_c;

    // Phase 3.a: Optimized Branching Biomes - only evaluate dominant biomes (w > 0.02)
    float d_interpolated = 0.0;
    float sum_w = 0.0;

    if (w_liquid > 0.02) {
        float d = sdf_liquid(p, cam_p, dilation, mids, is_silent, time);
        d_interpolated += w_liquid * d;
        sum_w += w_liquid;
    }
    if (w_metal > 0.02) {
        float d = sdf_metal(p, cam_p, dilation, mids, time);
        d_interpolated += w_metal * d;
        sum_w += w_metal;
    }
    if (w_cyber > 0.02) {
        float d = sdf_cyber(p, cam_p, dilation, mids, time);
        d_interpolated += w_cyber * d;
        sum_w += w_cyber;
    }
    if (w_dubstep > 0.02) {
        float d = sdf_dubstep(p, cam_p, dilation, mids, time);
        d_interpolated += w_dubstep * d;
        sum_w += w_dubstep;
    }
    if (w_celestial > 0.02) {
        float d = sdf_celestial(p, cam_p, dilation, mids, is_silent, time);
        d_interpolated += w_celestial * d;
        sum_w += w_celestial;
    }

    if (sum_w > 0.001) {
        d_interpolated /= sum_w;
    } else {
        d_interpolated = length(p_rel) - 2.8;
    }

    // Sculpt 3D tunnel/terrain walls using 2D Waterfall energy matrix along track spline
    float forward_t = clamp((p.z - cam_p.z) / 45.0, 0.0, 1.0);
    float angle = (dot(p_rel, p_rel) > 1e-7) ? atan(p_rel.y, p_rel.x) : 0.0;
    float freq_u = fract(angle / 6.2831853 + 0.5);
    float wf_energy = textureLod(u_waterfall_energy, vec2(freq_u, forward_t), 0.0).r;
    d_interpolated -= wf_energy * 0.45 * (1.0 - is_silent);

    // Transient Onset Shockwave Ripple
    if (ripple > 0.01) {
        float r = length(p - cam_p);
        float shock = sin(r * 4.5 - time * 20.0) * exp(-0.25 * r) * ripple * 0.60;
        d_interpolated += shock;
    }

    // Dynamic Procedural 3D Architecture (Portals, Totems, Rails)
    // Emerges dynamically to punctuate high speed sprints, beat drops, and rhythmic peaks
    float arch_emergence = 0.0;
    if (is_silent < 0.5) {
        float speed_factor = smoothstep(8.5, 20.0, u_cam_up.w);
        float rhythm_drive = max(dilation, ripple);
        float genre_bias   = max(w_cyber * 0.35, max(w_metal * 0.25, w_dubstep * 0.30));
        arch_emergence = clamp(speed_factor * 0.60 + rhythm_drive * 0.70 + u_laser_pos.w * 0.40 + genre_bias, 0.0, 1.0);
    }
    float d_world = d_interpolated;
    if (arch_emergence > 0.01) {
        float d_architecture = sdf_architecture(p, cam_p, dilation, mids, ripple, time, arch_emergence);
        d_world = min(d_interpolated, d_architecture);
    }

    // GUARANTEED Safe Flight Corridor:
    // Guarantees camera center and near-frustum can NEVER clip or black out under any combination
    // Local spherical near-camera safety bubble + track corridor (ZERO infinite cylinder hole!)
    float d_track_corridor = length(p_rel) - (2.2 + dilation * 0.2);
    float d_cam_bubble     = length(p - cam_p) - (2.0 + dilation * 0.2);
    float d_global_corridor = min(d_track_corridor, d_cam_bubble);
    d_world = smax(d_world, -d_global_corridor, 0.45);

    return d_world;
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

// Enhanced Multi-Scale Ambient Occlusion (contact crevices + wider cavity shadowing)
float calcAO(vec3 p, vec3 n) {
    float occ = 0.0;
    float sca = 1.0;
    for (int i = 0; i < 6; ++i) {
        float h = 0.04 + 0.65 * float(i * i) / 25.0;
        float d = map(p + h * n);
        occ += (h - d) * sca;
        sca *= 0.75;
    }
    return clamp(1.0 - 1.8 * max(occ, 0.0), 0.0, 1.0);
}

// Deep Cosmic Background Sky & Celestial Nebula
vec3 getSkyColor(vec3 rd, float arousal, float is_silent, float w_celestial) {
    float horizon_factor = smoothstep(-0.20, 0.40, rd.y);
    vec3 zenith_color = max(u_color_zenith.rgb * 1.2, vec3(0.008, 0.012, 0.024));
    vec3 horizon_glow = mix(u_color_primary.rgb * 0.45, u_color_accent.rgb * 0.35, 0.4) * (0.30 + arousal * 0.25) * (1.0 - is_silent * 0.7);
    vec3 sky = mix(horizon_glow, zenith_color, horizon_factor);

    // Deep cosmic stars
    vec3 star_dir = normalize(rd);
    float star_val = hash3(floor(star_dir * 140.0));
    if (star_val > 0.982) {
        float blink = sin(star_val * 80.0 + u_resolution_time.z * 2.0) * 0.5 + 0.5;
        float star_brightness = smoothstep(0.982, 1.0, star_val) * blink;
        sky += vec3(star_brightness * (0.9 + w_celestial * 0.8));
    }

    // Celestial Nebula Clouds & Aurora Stream
    float nebula_noise = sin(rd.x * 3.5 + u_resolution_time.z * 0.15) * cos(rd.z * 3.0) + sin(rd.y * 5.0) * 0.5;
    if (rd.y > -0.05) {
        float nebula_band = smoothstep(0.1, 0.55, rd.y + nebula_noise * 0.18) * smoothstep(0.85, 0.40, rd.y);
        vec3 nebula_color = mix(u_color_accent.rgb, vec3(0.3, 0.7, 1.0), 0.5) * (0.4 + w_celestial * 0.8);
        sky += nebula_color * nebula_band * (0.35 + arousal * 0.25) * (1.0 - is_silent * 0.6);
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
    float w_liquid    = u_biome_weights.x;
    float w_metal     = u_biome_weights.y;
    float w_cyber     = u_biome_weights.z;
    float w_dubstep   = u_biome_weights.w;
    float w_celestial = u_color_accent.w;

    float dilation       = u_physical_params.x;
    float ripple         = u_physical_params.y;
    float emission_pulse = u_physical_params.z;
    float spec_centroid  = u_physical_params.w;
    float arousal        = u_laser_pos.w;
    float is_silent      = u_extra_physics.z;

    // Cohesive, temporally low-pass filtered color palette (flicker-free)
    vec3 synesthetic_color = u_color_primary.rgb * (0.80 + emission_pulse * 0.35) * (1.0 - is_silent * 0.6);
    vec3 accent_color = u_color_accent.rgb;

    // Deep cosmic background sky with celestial nebula
    vec3 sky_color = getSkyColor(rd, arousal, is_silent, w_celestial);

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
        float density = 0.0006 / (1.0 + hit_dist * hit_dist * 16.0);
        accum_glow += synesthetic_color * density * (0.25 + emission_pulse * 0.25) * (1.0 - is_silent * 0.7);

        if (hit_dist < 0.003) {
            hit = true;
            break;
        }

        t += hit_dist * 0.88;
        if (t >= t_max) break;
    }

    // Clamp volumetric glow to prevent scene whitening during aggressive drops
    accum_glow = min(accum_glow, vec3(0.06));

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

        vec2 track_c = get_track_spline(p.z);
        vec2 track_offset = p.xy - track_c;
        float dist_center = abs(track_offset.x);

        // Ground/Floor detection: surfaces pointing upwards near track floor
        float is_ground = smoothstep(0.15, 0.65, n.y) * step(p.y, track_c.y + 0.5);
        // Wall detection: vertical surfaces on corridor edges
        float is_wall = smoothstep(0.30, 0.80, abs(n.x));

        vec3 view_dir = -rd;

        // =========================================================================
        // USER REQUIREMENT: High-Frequency Reactive Micro-Details at 100% Saturation / High Speed
        // Accents acoustic micro-details (treble, air, mids, cymbals, guitar picks, synth harmonics)
        // that remain razor-sharp and distinct even when speed and biome energy are at 100%!
        // Covers BOTH individual biome saturation AND overall ensemble drop intensity (el conjunto al 100%).
        // =========================================================================
        float treble_sparkle = u_extra_physics.y;
        float melodic_mids   = u_extra_physics.x;
        float max_biome_w = max(max(w_liquid, w_metal), max(w_cyber, w_dubstep));
        float biome_saturation = smoothstep(0.65, 1.0, max_biome_w);
        float speed_intensity = smoothstep(12.0, 32.0, u_cam_up.w);
        float ensemble_saturation = smoothstep(0.60, 0.95, arousal);
        float ensemble_pulse = smoothstep(0.70, 1.30, emission_pulse);
        float micro_boost = clamp(max(max(biome_saturation, speed_intensity), max(ensemble_saturation, ensemble_pulse)) * (1.0 - is_silent * 0.85), 0.0, 1.0);

        // 1. High-Frequency Micro-Relief Normal Perturbation (Micro-Facets, audio reactive)
        vec3 p_micro = p * 44.0;
        vec3 micro_pert = vec3(
            sin(p_micro.x + time * 3.5) * cos(p_micro.z * 1.2),
            cos(p_micro.y * 1.5 + time * 2.8),
            sin(p_micro.z + time * 3.0) * cos(p_micro.x * 1.4)
        );
        float pert_strength = 0.06 * (treble_sparkle * 1.8 + melodic_mids * 0.9) * (0.4 + 0.8 * micro_boost) * (1.0 - is_silent);
        vec3 micro_n = (pert_strength > 0.001) ? normalize(n + micro_pert * pert_strength) : n;

        // 1. Camera Forward Searchlight / Cockpit Headlight
        // Casts a forward illumination cone penetrating the tunnel with sculpted contrast
        vec3 cam_to_p = p - ro;
        float cam_dist = length(cam_to_p);
        vec3 cam_dir_p = cam_to_p / max(cam_dist, 0.001);
        float spot_cos = dot(cam_dir_p, fwd);
        float spot_beam = smoothstep(0.28, 0.94, spot_cos);
        float cam_atten = 1.0 / (1.0 + cam_dist * 0.05 + cam_dist * cam_dist * 0.002);
        
        // Crisp Lambertian cosine falloff modulated by micro-relief normals
        float cam_diff_macro = max(dot(n, -cam_dir_p), 0.0);
        float cam_diff_micro = max(dot(micro_n, -cam_dir_p), 0.0);
        float cam_diff = mix(cam_diff_macro, cam_diff_micro, 0.35 * (0.4 + micro_boost * 0.6));
        vec3 headlight_color = mix(vec3(0.72, 0.76, 0.86), synesthetic_color, 0.55);
        vec3 cam_headlight = headlight_color * (cam_diff * spot_beam * cam_atten * 0.85);

        // Forward searchlight specular micro-glints on oncoming surfaces
        vec3 cam_half = normalize(-cam_dir_p + view_dir);
        float cam_spec_pow = mix(40.0, 140.0, micro_boost);
        float cam_micro_spec = pow(max(dot(micro_n, cam_half), 0.0), cam_spec_pow);
        vec3 cam_headlight_glint = headlight_color * (cam_micro_spec * (1.0 + treble_sparkle * 2.2) * spot_beam * cam_atten * (0.35 + 0.65 * micro_boost));

        // 2. 3D Laser Ribbon Entity Point Light (omnidirectional luminous pool)
        vec3 laser_pos = u_laser_pos.xyz;
        vec3 l_dir = laser_pos - p;
        float l_dist = length(l_dir);
        l_dir = normalize(l_dir);

        float l_atten = 1.0 / (1.0 + l_dist * 0.06 + l_dist * l_dist * 0.006);
        float diff = max(dot(n, l_dir), 0.0);

        // Specular highlight
        vec3 half_v = normalize(l_dir + view_dir);
        float spec_power = mix(24.0, 90.0, max(w_metal, w_dubstep));
        float spec = pow(max(dot(n, half_v), 0.0), spec_power);

        vec3 laser_light = (synesthetic_color * (diff * 2.2) + accent_color * (spec * 2.6)) * l_atten * (1.0 - is_silent * 0.6);

        // High-frequency specular micro-glints from point light
        float micro_spec_pow = mix(45.0, 160.0, micro_boost);
        float micro_spec = pow(max(dot(micro_n, half_v), 0.0), micro_spec_pow);
        vec3 micro_specular = mix(accent_color, vec3(1.0), 0.70) * (micro_spec * (1.4 + treble_sparkle * 2.6 + micro_boost * 1.8)) * l_atten;

        // 3. Ambient Light (Deep rich obsidian darks with cubic AO falloff in crevices)
        vec3 ambient = (sky_color * 0.30 + synesthetic_color * 0.12 + vec3(0.008, 0.010, 0.014)) * (ao * ao * ao * 0.85 + 0.15);

        // 4. Distinct Biome Roadways, Fissures, Guide Rails & Patterns along Spline Track
        float biome_accent_pattern = 0.0;
        float track_glow = 0.0;

        if (w_cyber > 0.12 && is_silent < 0.5) {
            // High-speed Cyber Matrix: Runway markings, center lane, side rails, pulse rings
            float center_lane = smoothstep(0.14, 0.02, dist_center) * smoothstep(0.25, 0.75, sin(p.z * 1.5 - time * 9.0));
            float side_rails  = smoothstep(0.10, 0.02, abs(dist_center - 2.0));
            float outer_curb  = smoothstep(0.12, 0.02, abs(dist_center - 3.2));
            float cyber_angle = (dot(track_offset, track_offset) > 1e-7) ? atan(track_offset.y, track_offset.x) : 0.0;
            float grid_cross  = smoothstep(0.88, 0.98, sin(p.z * 1.57079));
            float wall_ribs   = smoothstep(0.85, 0.98, sin(cyber_angle * 6.0));

            float cyber_floor = is_ground * (center_lane * 2.8 + side_rails * 1.8 + outer_curb * 1.2 + grid_cross * 0.8);
            float cyber_walls = is_wall * (wall_ribs * 1.4 + grid_cross * 0.6);

            track_glow += (cyber_floor + cyber_walls) * w_cyber;
            biome_accent_pattern += (cyber_floor * 0.7 + cyber_walls * 0.5) * w_cyber;
        }
        if (w_metal > 0.12 && is_silent < 0.5) {
            // Obsidian Chasm: glowing magma fissure along spline center, basalt spire veins
            float center_fissure = smoothstep(0.60, 0.05, dist_center) * smoothstep(0.30, 0.95, abs(sin(p.z * 0.8 + sin(p.x * 2.0))));
            float magma_vein = smoothstep(0.55, 0.95, abs(sin(p.x * 2.2 + sin(p.z * 0.8) * 1.5) * cos(p.z * 1.2)));
            float spire_vein = smoothstep(0.65, 0.96, abs(sin(p.y * 3.0 + p.z * 0.5)));

            float metal_floor = is_ground * (center_fissure * 3.0 + magma_vein * 1.6);
            float metal_walls = is_wall * (spire_vein * 1.5 + magma_vein * 0.8);

            track_glow += (metal_floor + metal_walls) * w_metal * (0.9 + emission_pulse * 0.6);
            biome_accent_pattern += (metal_floor * 0.8 + metal_walls * 0.5) * w_metal;
        }
        if (w_dubstep > 0.12 && is_silent < 0.5) {
            // Quantum Bass Void: undulating resonant rings, bass ripple tracks, hexagonal strobe
            float ring_glow = smoothstep(0.55, 0.96, sin(p.z * 1.57079 - time * 6.0));
            float hex_angle = (dot(track_offset, track_offset) > 1e-7) ? atan(track_offset.y, track_offset.x) : 0.0;
            float hex_pulse = smoothstep(0.65, 0.96, sin(hex_angle * 6.0 + p.z * 0.5));
            float bass_track = is_ground * smoothstep(0.22, 0.02, abs(dist_center - 1.5)) * (0.8 + emission_pulse * 0.8);

            float dub_features = ring_glow * 1.8 + hex_pulse * 1.0 + bass_track * 1.6;
            track_glow += dub_features * w_dubstep * (0.8 + emission_pulse * 0.6);
            biome_accent_pattern += dub_features * 0.6 * w_dubstep;
        }
        if (w_liquid > 0.12 && is_silent < 0.5) {
            // Liquid Silk Ocean: bioluminescent wave crests, caustic webs, luminous flight wake
            float wave_crests = smoothstep(0.40, 0.90, sin(p.x * 0.6 + time * 1.4) * cos(p.z * 0.5 - time * 0.8));
            float caustics = smoothstep(0.55, 0.95, sin(p.x * 1.8 + time * 2.0) * sin(p.z * 1.8 - time * 2.0));
            float ocean_lane = is_ground * smoothstep(0.30, 0.02, dist_center) * (0.8 + wave_crests * 0.8);

            float liq_features = is_ground * (wave_crests * 1.0 + caustics * 0.8 + ocean_lane * 1.2);
            track_glow += liq_features * w_liquid * (0.7 + emission_pulse * 0.5);
            biome_accent_pattern += liq_features * 0.4 * w_liquid;
        }

        // Celestial Nebula & Open-Air Stargate Runway Lighting
        if (w_celestial > 0.12 && is_silent < 0.5) {
            float causeway_lane = is_ground * smoothstep(0.40, 0.02, dist_center) * (0.6 + treble_sparkle * 1.2);
            float sg_phase = mod(p.z + 16.0, 32.0) - 16.0;
            float is_stargate = smoothstep(0.50, 0.05, abs(sg_phase)) * smoothstep(0.35, 0.05, abs(length(track_offset) - 4.0));
            vec3 stargate_glow = vec3(0.35, 0.90, 1.0) * (is_stargate * 3.5 * (1.0 + melodic_mids * 1.5));
            track_glow += (causeway_lane * 1.5) * w_celestial;
            biome_accent_pattern += causeway_lane * 0.5 * w_celestial;
        }

        biome_accent_pattern *= (0.2 + 0.8 * arousal) * (1.0 - is_silent);

        // Phase 2 & 3: 3D Procedural Architecture Neon Illumination & Shading
        // Dynamic Emergence: Portals and totems only illuminate when emerged
        float arch_emergence = 0.0;
        if (is_silent < 0.5) {
            float speed_factor = smoothstep(8.5, 20.0, u_cam_up.w);
            float rhythm_drive = max(dilation, ripple);
            float genre_bias   = max(w_cyber * 0.35, max(w_metal * 0.25, w_dubstep * 0.30));
            arch_emergence = clamp(speed_factor * 0.60 + rhythm_drive * 0.70 + arousal * 0.40 + genre_bias, 0.0, 1.0);
        }

        // Sonic Shockwave Rings & Pulse Portals along Spline Track
        // Emerges dynamically ONLY during explosive drum kicks, drops and high rhythm surges
        float sonic_portal = 0.0;
        if (ripple > 0.05 && arch_emergence > 0.10 && is_silent < 0.5) {
            float ring_spacing = 16.0;
            float ring_phase = mod(p.z - time * 18.0, ring_spacing) - (ring_spacing * 0.5);
            float pulse_ring = smoothstep(0.30, 0.0, abs(ring_phase));
            float r_dist = length(track_offset);
            float portal_edge = smoothstep(0.25, 0.0, abs(r_dist - (2.7 + dilation * 0.5)));
            sonic_portal = pulse_ring * portal_edge * ripple * 3.5 * arch_emergence;
        }

        // 1. Neon Gate Portals / Monumental Arches Shading
        float gate_z_dist = abs(mod(p.z + 8.0, 16.0) - 8.0);
        float is_gate = smoothstep(0.40, 0.04, gate_z_dist);
        float gate_edge = smoothstep(0.24, 0.02, abs(length(track_offset) - (2.65 + dilation * 0.50)));
        float pillar_edge = smoothstep(0.24, 0.02, abs(abs(track_offset.x) - (2.65 + dilation * 0.50))) * step(-2.4, track_offset.y) * step(track_offset.y, 0.5);
        float arch_structure = max(gate_edge, pillar_edge);
        vec3 gate_neon = accent_color * (is_gate * arch_structure * 4.2 * (1.0 + ripple * 2.0 + emission_pulse * 1.4));

        // 2. 3D Extruded Energy Rails Shading (Both Lower Roadway Rails & Upper Holographic Conduits)
        float rail_wave_sh = sin(p.z * 0.45 + time * 3.2) * (0.12 + melodic_mids * 0.16) + sin(p.z * 1.4 - time * 9.0) * (ripple * 0.12);
        float rail_y_sh = -1.15 + rail_wave_sh;
        float rail_x_cur = 2.22 + dilation * 0.15;
        float rail_dist_l = length(vec2(track_offset.x + rail_x_cur, track_offset.y - rail_y_sh));
        float rail_dist_r = length(vec2(track_offset.x - rail_x_cur, track_offset.y - rail_y_sh));
        float rail_high_y_sh = 1.35 + 0.08 * cos(p.z * 0.40 + time * 2.2);
        float rail_high_dist_l = length(vec2(track_offset.x + (rail_x_cur + 0.30), track_offset.y - rail_high_y_sh));
        float rail_high_dist_r = length(vec2(track_offset.x - (rail_x_cur + 0.30), track_offset.y - rail_high_y_sh));
        float min_rail_dist = min(min(rail_dist_l, rail_dist_r), min(rail_high_dist_l, rail_high_dist_r));
        float is_rail = smoothstep(0.16, 0.03, min_rail_dist);
        float rail_pulse = smoothstep(0.35, 0.95, sin(p.z * 2.5 - time * 14.0));
        vec3 rail_neon = mix(synesthetic_color, accent_color, 0.70) * (is_rail * (2.6 + rail_pulse * 2.2 + melodic_mids * 1.4));

        // 3. Floating Equalizer Monoliths Shading (Strictly Y-Bounded VU Pillars)
        float totem_z_dist = abs(mod(p.z + 3.0, 6.0) - 3.0);
        float totem_x_dist = abs(abs(track_offset.x) - (3.45 + dilation * 0.35));
        float cell_z_sh = floor((p.z + 3.0) / 6.0);
        float totem_h_sh = 1.1 + 2.2 * (dilation * 0.65 + melodic_mids * 0.75) * (0.65 + 0.35 * sin(cell_z_sh * 0.75 + time * 3.5));
        float totem_y_top = totem_h_sh - 1.1;
        float is_totem_y = step(-1.15, track_offset.y) * step(track_offset.y, totem_y_top + 0.05);
        float is_totem = smoothstep(0.40, 0.05, totem_x_dist) * step(totem_z_dist, 0.45) * is_totem_y;
        float bar_frac = clamp((track_offset.y + 1.1) / max(totem_h_sh, 0.1), 0.0, 1.0);
        vec3 totem_bar_col = mix(synesthetic_color, accent_color, bar_frac);
        float bar_grid = smoothstep(0.15, 0.85, sin(track_offset.y * 12.0));
        vec3 totem_neon = totem_bar_col * (is_totem * bar_grid * (2.4 + dilation * 1.8 + melodic_mids * 1.2));

        vec3 arch_emissive = (gate_neon + rail_neon + totem_neon) * (arch_emergence * (1.0 - is_silent));

        // Emissive light from track features, veins, sonic pulse portals, and 3D architecture
        vec3 emissive = accent_color * ((track_glow * 1.8 + sonic_portal * 2.0)) * (1.0 - is_silent) + arch_emissive;

        // 2. High-Frequency Harmonic Vein Pulses (Razor-Sharp Veins, audio-reactive)
        float vein_phase = p.z * 8.0 + p.x * 5.0 + sin(p.y * 10.0);
        float vein_pulse = smoothstep(0.965, 0.998, sin(vein_phase + time * (10.0 + treble_sparkle * 14.0)));
        float micro_vein = vein_pulse * (treble_sparkle * 2.4 + melodic_mids * 1.2) * (0.4 + 1.2 * micro_boost) * (1.0 - is_silent);
        vec3 micro_vein_emissive = mix(accent_color, vec3(1.0, 0.95, 0.88), 0.75) * micro_vein;

        // 3. Acoustic Micro-Sparkles (High-Hat & Cymbal Surface Stardust, audio-reactive)
        float sparkle_cell = hash3(floor(p * 52.0));
        float sparkle_mask = step(0.91, sparkle_cell);
        float sparkle_flash = abs(sin(sparkle_cell * 80.0 + time * 24.0));
        float micro_sparkle = sparkle_mask * sparkle_flash * (treble_sparkle * 3.2) * (0.4 + 1.4 * micro_boost) * (1.0 - is_silent);
        vec3 micro_sparkle_emissive = mix(vec3(1.0), accent_color, 0.35) * micro_sparkle;

        // 4. Percussive Rim Flashes & Silhouette Hardening
        float rim_exp = mix(6.0, 16.0, micro_boost);
        float micro_rim = pow(clamp(1.0 - max(dot(n, view_dir), 0.0), 0.0, 1.0), rim_exp);
        float percussive_flash = micro_rim * (treble_sparkle * 1.9 + max(0.0, emission_pulse - 0.9) * 1.4) * (0.3 + 0.8 * micro_boost) * (1.0 - is_silent);
        vec3 micro_rim_emissive = mix(accent_color, vec3(0.92, 0.96, 1.0), 0.6) * percussive_flash;

        // 5. Biome-Specific 100% Saturation Micro-Accents
        vec3 biome_micro_accents = vec3(0.0);
        if (w_liquid > 0.4) {
            float liq_micro = smoothstep(0.85, 0.98, sin(p.x * 14.0 + time * 4.0) * cos(p.z * 14.0 - time * 3.0));
            biome_micro_accents += vec3(0.3, 0.7, 1.0) * (liq_micro * treble_sparkle * 1.8 * w_liquid * micro_boost);
        }
        if (w_metal > 0.4) {
            float metal_micro = smoothstep(0.92, 0.99, abs(sin(p.x * 20.0 + sin(p.z * 12.0))));
            biome_micro_accents += vec3(1.0, 0.4, 0.1) * (metal_micro * (0.8 + emission_pulse * 1.5) * w_metal * micro_boost);
        }
        if (w_cyber > 0.4) {
            float cyber_micro = step(0.94, fract(p.z * 6.0)) * step(0.90, fract(p.x * 6.0));
            biome_micro_accents += vec3(0.1, 1.0, 0.8) * (cyber_micro * (1.0 + treble_sparkle * 2.0) * w_cyber * micro_boost);
        }
        if (w_dubstep > 0.4) {
            float dub_micro = smoothstep(0.88, 0.99, sin(p.y * 16.0 + p.z * 16.0 + time * 18.0));
            biome_micro_accents += vec3(0.9, 0.2, 1.0) * (dub_micro * (1.0 + emission_pulse * 1.8) * w_dubstep * micro_boost);
        }

        // Base Albedo: solid neutral reflectance floor + synesthetic tint
        vec3 base_mat = mix(vec3(0.06, 0.07, 0.09), synesthetic_color * 0.80, 0.50);
        vec3 albedo = mix(base_mat, accent_color * 1.0, clamp(biome_accent_pattern, 0.0, 0.80));

        // Standard Fresnel reflection
        float fresnel = pow(clamp(1.0 - max(dot(n, view_dir), 0.0), 0.0, 1.0), 3.0);
        vec3 fresnel_rim = mix(albedo * 0.25, accent_color * 1.0, fresnel) * (fresnel * 0.55);

        // Combined Surface Color with Multi-Scale Ambient Occlusion and Micro-Details
        vec3 total_micro = micro_specular + cam_headlight_glint + micro_vein_emissive + micro_sparkle_emissive + micro_rim_emissive + biome_micro_accents;
        vec3 ambient_light = albedo * ambient * ao;
        vec3 direct_light = albedo * (cam_headlight + laser_light);
        vec3 reflective_light = (fresnel_rim + total_micro) * (0.35 + 0.65 * ao);
        scene_color = ambient_light + direct_light + reflective_light + emissive;

        // Depth fog: Rich atmospheric aerial perspective past 35m into horizon sky
        // Hermite blend to 100% sky color at t_max ensures seamless transition (zero pop/cutoff)
        float fog_norm = clamp((t - 35.0) / (t_max - 40.0), 0.0, 1.0);
        float fog = fog_norm * fog_norm * (3.0 - 2.0 * fog_norm);
        float horizon_blend = smoothstep(30.0, 130.0, t);
        vec3 horizon_glow = mix(u_color_primary.rgb * 0.40, u_color_accent.rgb * 0.35, 0.5) * (0.30 + arousal * 0.25) * (1.0 - is_silent * 0.7);
        vec3 fog_color = mix(horizon_glow, sky_color, horizon_blend);
        scene_color = mix(scene_color, fog_color, fog);
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
