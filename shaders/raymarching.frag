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
    // 4 incommensurate spatial harmonics based on golden ratio phi ~ 1.618034
    float x = (2.4 * sin(z * 0.0350) + 1.2 * sin(z * 0.07204) + 0.3 * sin(z * 0.05663) + 0.3 * sin(z * 0.02163)) * curvature;
    float y = 2.7 + (0.45 * sin(z * 0.0250) + 0.22 * sin(z * 0.05145) + 0.12 * sin(z * 0.04045) + 0.08 * sin(z * 0.01545)) * curvature;
    return vec2(x, y);
}

// =============================================================================
// BIOME 1: Liquid Silk Ocean (Jazz, Lofi, Chill, Acoustic, Ambient)
// Vast monumental ocean under track with colossal fluid spheres and arches
// =============================================================================
float sdf_liquid(vec3 p, vec3 cam_p, float dilation, float mids, float is_silent, float time) {
    vec2 track_c = get_track_spline(p.z);
    vec2 p_rel = p.xy - track_c;
    float wave_scale = 1.0 - is_silent * 0.95;

    // Harmonic liquid waves with wide sub-bass reaction
    float wave1 = sin(p.x * 0.35 + time * 1.4) * cos(p.z * 0.28 + time * 1.0) * (0.45 * wave_scale + dilation * 0.55);
    float wave2 = sin((p.x + p.z) * 0.55 + time * 2.0) * (0.22 * wave_scale + mids * 0.35);
    float wave_bass = sin(length(p_rel) * 0.25 - time * 3.0) * (dilation * 0.50 * wave_scale);
    float ocean_y = (track_c.y - 2.4) + (wave1 + wave2 + wave_bass);
    float d_ocean = p.y - ocean_y;

    // Colossal liquid orbs and monumental arches at lateral distance 5.5m - 9.0m
    float d_liq_geom = d_ocean;
    if (is_silent < 0.8) {
        float orb_id = floor((p.z + 8.0) / 16.0);
        vec3 q_orb = vec3(abs(p_rel.x) - (6.5 + dilation * 1.2), p_rel.y - (0.8 + 1.2 * sin(orb_id * 1.8 + time * 1.2)), mod(p.z + 8.0, 16.0) - 8.0);
        float d_orb = length(q_orb) - (1.4 + dilation * 0.6 + mids * 0.4);

        float qz_arch = mod(p.z + 14.0, 28.0) - 14.0;
        float r_arch = length(vec2(p_rel.x, max(0.0, p_rel.y + 0.5))) - (6.8 + dilation * 1.0);
        float d_arch = length(vec2(r_arch, qz_arch)) - (0.75 + mids * 0.35);

        d_liq_geom = smin(d_ocean, min(d_orb, d_arch), 1.25);
    }

    // Dual Flight Corridor Protection (track corridor + local near-camera safety bubble)
    float d_track_corridor = length(p_rel) - (2.4 + dilation * 0.3);
    float d_cam_bubble     = length(p - cam_p) - (2.0 + dilation * 0.2);
    float d_flight_corridor = min(d_track_corridor, d_cam_bubble);
    return smax(d_liq_geom, -d_flight_corridor, 0.40);
}

// =============================================================================
// BIOME 2: Volcanic Obsidian Chasm & Titanic Crystals (Metal, Rock)
// Open sky chasm with suspended rocky causeway, Tier 1 megaliths & Tier 2 colossal needles
// =============================================================================
float sdf_metal(vec3 p, vec3 cam_p, float dilation, float mids, float time) {
    vec2 track_c = get_track_spline(p.z);
    vec2 p_rel = p.xy - track_c;

    // 1. Suspended Rocky Roadway Slab over deep magma chasm (finite thickness, open sky above)
    vec2 d_slab = vec2(abs(p_rel.x) - (3.2 + dilation * 0.4), abs(p_rel.y - (-2.9)) - 0.5);
    float d_road_slab = length(max(d_slab, 0.0)) + min(max(d_slab.x, d_slab.y), 0.0);

    // 2. Tier 1 Megaliths flanking track at 4.8m (height 8m-10m)
    vec3 q_mega = vec3(abs(p_rel.x) - (4.8 + dilation * 0.5), p_rel.y - 2.0, mod(p.z + 6.0, 12.0) - 6.0);
    vec3 b_mega = vec3(0.65, 4.5 + mids * 0.8, 1.2);
    vec3 d_b_m = abs(q_mega) - b_mega;
    float d_megalith = length(max(d_b_m, 0.0)) + min(max(d_b_m.x, max(d_b_m.y, d_b_m.z)), 0.0) - 0.15;

    // 3. Tier 2 Titanic Obsidian Needles at 9.5m (height 14m-16m)
    float spire_id = floor((p.z + 8.0) / 16.0);
    vec3 q_spire = vec3(abs(p_rel.x) - (9.5 + dilation * 0.6), p_rel.y - 4.5, mod(p.z + 8.0, 16.0) - 8.0);
    q_spire.xy = rot(spire_id * 0.35 + time * 0.3) * q_spire.xy;
    q_spire.yz = rot(mids * 0.35) * q_spire.yz;
    float octa_titan = (abs(q_spire.x) + abs(q_spire.y * 0.14) + abs(q_spire.z) - (1.6 + mids * 0.5)) * 0.57735027;
    float d_titan = max(octa_titan, abs(q_spire.y) - 7.5);

    // 4. Floating razor dagger shards
    float dagger_id = floor((p.z + 3.0) / 6.0);
    vec3 q_dagger = vec3(abs(p_rel.x) - (3.6 + dilation * 0.4), abs(p_rel.y) - 0.8, mod(p.z + 3.0, 6.0) - 3.0);
    q_dagger.xy = rot(time * 0.8 + dagger_id * 0.6) * q_dagger.xy;
    float octa_dagger = (abs(q_dagger.x) + abs(q_dagger.y) + abs(q_dagger.z) - (0.50 + mids * 0.30)) * 0.57735027;
    float d_dagger = max(octa_dagger, abs(q_dagger.z) - 1.8);

    float d_metal_geom = min(d_road_slab, min(d_megalith, min(d_titan, d_dagger)));

    // Dual Flight Corridor Protection
    float d_track_corridor = length(p_rel) - (2.4 + dilation * 0.3);
    float d_cam_bubble     = length(p - cam_p) - (2.0 + dilation * 0.2);
    float d_flight_corridor = min(d_track_corridor, d_cam_bubble);
    return smax(d_metal_geom, -d_flight_corridor, 0.40);
}

// =============================================================================
// BIOME 3: Cyber Matrix & Megacity Horizon (Techno, Synthwave, House)
// Open-sky neon freeway flanked by equalizer towers at 4.5m and skyscrapers at 10.0m
// =============================================================================
float sdf_cyber(vec3 p, vec3 cam_p, float dilation, float mids, float time) {
    vec2 track_c = get_track_spline(p.z);
    vec2 p_rel = p.xy - track_c;

    // 1. Suspended Freeway Slab under track
    vec2 d_hw = vec2(abs(p_rel.x) - (3.4 + dilation * 0.4), abs(p_rel.y - (-2.7)) - 0.4);
    float d_highway = length(max(d_hw, 0.0)) + min(max(d_hw.x, d_hw.y), 0.0);

    // 2. Row 1: Audio-Reactive Equalizer Towers at 4.5m
    float eq_id = floor((p.z + 2.0) / 4.0);
    float eq_h = 2.0 + 3.0 * (dilation * 1.2 + mids * 0.8) * abs(sin(eq_id * 1.4 + time * 5.0));
    vec3 q_eq = vec3(abs(p_rel.x) - (4.5 + dilation * 0.4), p_rel.y - (-2.3 + eq_h * 0.5), mod(p.z + 2.0, 4.0) - 2.0);
    vec3 b_eq = vec3(0.55, eq_h * 0.5, 0.70);
    vec3 d_b_eq = abs(q_eq) - b_eq;
    float d_eq = length(max(d_b_eq, 0.0)) + min(max(d_b_eq.x, max(d_b_eq.y, d_b_eq.z)), 0.0) - 0.05;

    // 3. Row 2: Colossal Background Skyscrapers at 10.0m (height up to 14m)
    float bg_id = floor((p.z + 5.0) / 10.0);
    float bg_h = 8.0 + 5.5 * abs(sin(bg_id * 1.7 + 0.5));
    vec3 q_bg = vec3(abs(p_rel.x) - (10.0 + dilation * 0.5), p_rel.y - (-2.0 + bg_h * 0.5), mod(p.z + 5.0, 10.0) - 5.0);
    vec3 b_bg = vec3(1.8, bg_h * 0.5, 2.2);
    vec3 d_b_bg = abs(q_bg) - b_bg;
    float d_mega_tower = length(max(d_b_bg, 0.0)) + min(max(d_b_bg.x, max(d_b_bg.y, d_b_bg.z)), 0.0) - 0.1;

    // 4. Clean Accelerator Rings every 12 meters
    vec3 q_ring = vec3(p_rel.x, p_rel.y, mod(p.z + 6.0, 12.0) - 6.0);
    float ring_body = abs(length(q_ring.xy) - (4.6 + dilation * 0.4)) - 0.18;
    float d_ring = max(ring_body, abs(q_ring.z) - 0.25);

    // 5. Energy Conduit Rails
    vec2 q_rails = abs(p_rel) - vec2(2.6);
    float d_rails = length(q_rails) - 0.12;

    float d_cyber_geom = min(d_highway, min(d_eq, min(d_mega_tower, min(d_ring, d_rails))));

    // Dual Flight Corridor Protection
    float d_track_corridor = length(p_rel) - (2.4 + dilation * 0.3);
    float d_cam_bubble     = length(p - cam_p) - (2.0 + dilation * 0.2);
    float d_flight_corridor = min(d_track_corridor, d_cam_bubble);
    return smax(d_cyber_geom, -d_flight_corridor, 0.40);
}

// =============================================================================
// BIOME 4: Quantum Bass Abyss & Resonator Void (Dubstep, Speedcore)
// Open cosmic void, heavy energy track, mega-hex rings at 5.2m & massive resonators
// =============================================================================
float sdf_dubstep(vec3 p, vec3 cam_p, float dilation, float mids, float time) {
    vec2 track_c = get_track_spline(p.z);
    vec2 p_rel = p.xy - track_c;

    // 1. Heavy Energy Track Slab in cosmic void
    vec2 d_dub_track = vec2(abs(p_rel.x) - (3.0 + dilation * 0.4), abs(p_rel.y - (-2.6)) - 0.4);
    float d_heavy_track = length(max(d_dub_track, 0.0)) + min(max(d_dub_track.x, d_dub_track.y), 0.0);

    // 2. Mega-Hexagonal Quantum Gravity Rings (radius 5.2m, recurring every 8m)
    vec3 q_hex = vec3(p_rel.x, p_rel.y, mod(p.z + 4.0, 8.0) - 4.0);
    vec2 p_hex = abs(q_hex.xy);
    float hex_dist = max(p_hex.x * 0.866025 + p_hex.y * 0.5, p_hex.y);
    float r_hex = 5.2 + dilation * 0.7;
    float d_hex_ring = abs(hex_dist - r_hex) - (0.24 + dilation * 0.08);
    float d_ring_segment = max(d_hex_ring, abs(q_hex.z) - (0.35 + dilation * 0.10));

    // 3. Massive Sub-Bass Resonator Pillars floating in space
    float wobble_scale = sin(p.z * 1.2 - time * 12.0) * (dilation * 0.5);
    vec3 q_res = vec3(abs(p_rel.x) - (5.0 + dilation * 0.8), p_rel.y, mod(p.z + 3.0, 6.0) - 3.0);
    vec3 box_dim = vec3(0.65 + dilation * 0.3, 2.2 + dilation * 2.5 + mids * 0.8 + wobble_scale, 0.65 + dilation * 0.3);
    vec3 d_b = abs(q_res) - box_dim;
    float d_resonator = length(max(d_b, 0.0)) + min(max(d_b.x, max(d_b.y, d_b.z)), 0.0) - 0.12;

    float d_dub_geom = min(d_heavy_track, min(d_ring_segment, d_resonator));

    // Dual Flight Corridor Protection
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

    // 2. Floating Crystalline Asteroids flanking track (Tumbling Octahedra, cell-synchronized)
    float ast_id = floor((p.z + 9.0) / 18.0);
    float ast_side = (mod(ast_id, 2.0) < 1.0) ? 1.0 : -1.0;
    vec3 q_ast = vec3(p_rel.x, p_rel.y, mod(p.z + 9.0, 18.0) - 9.0);
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

    // Physical kinematic emergence: sinks smoothly under ground when calm
    float sink_offset = (1.0 - emergence) * (1.0 - emergence) * 4.5;
    vec2 p_arch = vec2(p_rel.x, p_rel.y + sink_offset);

    // 1. Neon Rhythmic Gate Portals / Monumental Arches recurring every 16m
    float gate_spacing = 16.0;
    float gate_z = mod(p.z + 8.0, gate_spacing) - 8.0;
    float r_gate = 2.65 + dilation * 0.50 + ripple * 0.30;
    float arch_depth = 0.32 + ripple * 0.12;
    float r_xy = length(p_arch);

    // Polymorphic Gate Architecture modulated by Biome Weights & Beat Phase
    float bpm = max(u_extra_physics.w, 60.0);
    float beat_phase = time * (bpm / 60.0) * 3.14159265;
    float gate_id = floor((p.z + 8.0) / 16.0);
    float gate_angle = (dot(p_arch, p_arch) > 1e-6) ? atan(p_arch.y, p_arch.x) : 0.0;

    // Liquid: smooth harmonic fluid undulation
    float mod_liquid = sin(gate_angle * 3.0 + beat_phase) * 0.09;
    // Metal: sharp angular faceted blade-like ridges
    float mod_metal = (abs(sin(gate_angle * 4.0 + gate_id)) - 0.5) * 0.14;
    // Cyber: quantized octagonal profile rotating in rhythmic steps
    float cyber_step = floor(beat_phase / 3.14159265 * 2.0) * (3.14159265 / 8.0);
    float oct_ang = mod(gate_angle + cyber_step + 0.392699, 0.785398) - 0.392699;
    float mod_cyber = (0.92388 / max(cos(oct_ang), 0.7) - 0.96) * 1.5;
    // Dubstep: resonant hexagonal profile with sub-bass modulation and counter-rotating twin grooves
    float hex_ang = mod(gate_angle + beat_phase * 0.25 + 0.523599, 1.047198) - 0.523599;
    float grv_cw  = sin(gate_angle * 5.0 + beat_phase * 0.8);
    float grv_ccw = sin(gate_angle * 5.0 - beat_phase * 0.8);
    float mod_dubstep = (0.8660 / max(cos(hex_ang), 0.7) - 0.933) * 1.2 + (grv_cw * grv_ccw) * (0.03 + dilation * 0.04);

    float gate_poly_mod = u_biome_weights.x * mod_liquid
                        + u_biome_weights.y * mod_metal
                        + u_biome_weights.z * mod_cyber
                        + u_biome_weights.w * mod_dubstep;
    float r_gate_mod = r_gate + gate_poly_mod;

    float arch_torus = max(abs(r_xy - r_gate_mod) - 0.20, abs(gate_z) - arch_depth);
    vec2 p_pillar = vec2(abs(p_arch.x) - r_gate, p_arch.y + 1.0);
    float arch_pillars = max(max(abs(p_pillar.x) - 0.24, abs(p_pillar.y) - 1.4), abs(gate_z) - arch_depth);
    float d_gate_arch = min(arch_torus, arch_pillars);

    // 2. Audio-Reactive Floating Equalizer Monoliths / Totems recurring every 6m
    float totem_spacing = 6.0;
    float cell_z = floor((p.z + 3.0) / totem_spacing);
    float totem_z = mod(p.z + 3.0, totem_spacing) - 3.0;
    float wave_phase = cell_z * 0.75 + time * 3.5;
    float totem_h = 1.1 + 2.2 * (dilation * 0.65 + mids * 0.75) * (0.65 + 0.35 * sin(wave_phase));
    float bevel = 0.04 + 0.04 * dilation;
    vec3 q_totem = vec3(abs(p_rel.x) - (3.45 + dilation * 0.35), p_arch.y - (totem_h * 0.5 - 1.1), totem_z);
    vec3 b_totem = vec3(0.30, totem_h * 0.5, 0.30) - bevel;
    vec3 d_box = abs(q_totem) - b_totem;
    float d_monolith = length(max(d_box, vec3(0.0))) + min(max(d_box.x, max(d_box.y, d_box.z)), 0.0) - bevel;
    float eq_stripes = abs(sin(q_totem.y * 6.0)) * 0.03;
    d_monolith += eq_stripes;

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
    float d_rails = min(min(d_rails_lower, d_rails_upper), d_strut);

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
    // Modulated primarily to lateral walls, attenuated in Liquid biome, with safe gain (0.28)
    float forward_t = clamp((p.z - cam_p.z) / 45.0, 0.0, 1.0);
    float angle = (dot(p_rel, p_rel) > 1e-7) ? atan(p_rel.y, p_rel.x) : 0.0;
    float freq_u = fract(angle / 6.2831853 + 0.5);
    float wf_energy = textureLod(u_waterfall_energy, vec2(freq_u, forward_t), 0.0).r;
    float wall_sculpt_mask = smoothstep(1.8, 3.2, abs(p_rel.x)) * (1.0 - w_liquid * 0.75);
    d_interpolated -= wf_energy * 0.28 * wall_sculpt_mask * (1.0 - is_silent);

    // Transient Onset Shockwave Ripple (clean pressure wave, Lipschitz compliant)
    if (ripple > 0.01) {
        float r = length(p - cam_p);
        float shock = sin(r * 1.8 - time * 20.0) * exp(-0.25 * r) * ripple * 0.18;
        d_interpolated += shock;
    }

    // Dynamic Procedural 3D Architecture (Portals, Totems, Rails)
    // Read directly from physical bass-driven UBO channel u_color_zenith.w
    float arch_emergence = clamp(u_color_zenith.w, 0.0, 1.0) * (1.0 - is_silent);
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
    vec3 zenith_color = max(u_color_zenith.rgb * 1.2, vec3(0.006));
    vec3 horizon_glow = mix(u_color_primary.rgb * 0.45, u_color_accent.rgb * 0.35, 0.4) * (0.30 + arousal * 0.25) * (1.0 - is_silent * 0.7);
    vec3 sky = mix(horizon_glow, zenith_color, horizon_factor);

    // Deep cosmic stars (crisp pin-point circular stardust)
    vec3 star_dir = normalize(rd);
    vec3 star_grid = star_dir * 160.0;
    vec3 star_id = floor(star_grid);
    vec3 star_uv = fract(star_grid) - 0.5;
    float star_val = hash3(star_id);
    if (star_val > 0.980) {
        float d_star = length(star_uv - (vec3(fract(star_val * 13.0), fract(star_val * 29.0), fract(star_val * 47.0)) - 0.5) * 0.5);
        float star_spot = smoothstep(0.14, 0.01, d_star);
        float blink = sin(star_val * 80.0 + u_resolution_time.z * 2.0) * 0.5 + 0.5;
        float star_brightness = smoothstep(0.980, 1.0, star_val) * blink * star_spot;
        vec3 star_color = mix(u_color_primary.rgb, u_color_accent.rgb, fract(star_val * 19.0));
        sky += star_color * (star_brightness * (1.1 + w_celestial * 0.9));
    }

    // Celestial Nebula Clouds & Aurora Stream
    float nebula_noise = sin(rd.x * 3.5 + u_resolution_time.z * 0.15) * cos(rd.z * 3.0) + sin(rd.y * 5.0) * 0.5;
    if (rd.y > -0.05) {
        float nebula_band = smoothstep(0.1, 0.55, rd.y + nebula_noise * 0.18) * smoothstep(0.85, 0.40, rd.y);
        vec3 nebula_tint = mix(u_color_primary.rgb, u_color_accent.rgb, 0.65);
        vec3 nebula_color = nebula_tint * (0.4 + w_celestial * 0.8);
        sky += nebula_color * nebula_band * (0.35 + arousal * 0.25) * (1.0 - is_silent * 0.6);
    }

    // 2D/3D Hybrid Holographic Beat-Compass at Horizon
    vec2 far_spline = get_track_spline(u_cam_pos.z + 120.0);
    vec3 far_target = normalize(vec3(far_spline.x - u_cam_pos.x, far_spline.y - u_cam_pos.y, 120.0));
    vec3 h_right = normalize(cross(vec3(0.0, 1.0, 0.0), far_target));
    vec3 h_up = cross(far_target, h_right);

    float rd_target_dot = dot(rd, far_target);
    if (rd_target_dot > 0.72) {
        vec2 h_uv = vec2(dot(rd, h_right), dot(rd, h_up)) / max(rd_target_dot, 0.1);
        float r_h = length(h_uv);
        float phi_h = atan(h_uv.y, h_uv.x);

        float bpm = max(u_extra_physics.w, 60.0);
        float beat_phase = u_resolution_time.z * (bpm / 60.0) * 3.14159265;
        float dilation = u_physical_params.x;
        float mids = u_extra_physics.x;

        // 2 Thin Concentric Rings rotating in opposite directions
        float r1 = 0.12 + dilation * 0.025;
        float r2 = 0.24 + dilation * 0.035;
        float ring1 = smoothstep(0.005, 0.001, abs(r_h - r1));
        float ring2 = smoothstep(0.005, 0.001, abs(r_h - r2));

        float phi_cw  = phi_h + beat_phase * 0.5;
        float phi_ccw = phi_h - beat_phase * 0.5;

        // Radial Marks reactive to dilation & melodic_mids
        float ticks1 = smoothstep(0.96, 0.995, cos(phi_cw * 12.0)) * smoothstep(0.015, 0.0, abs(r_h - r1));
        float tick_len = 0.03 + mids * 0.03;
        float ticks2 = smoothstep(0.97, 0.998, cos(phi_ccw * 24.0)) * step(r2 - tick_len, r_h) * step(r_h, r2 + tick_len);
        float spokes = (smoothstep(0.003, 0.0, abs(h_uv.x)) + smoothstep(0.003, 0.0, abs(h_uv.y))) * smoothstep(0.32, 0.08, r_h);

        // Biome Dominant Polymorphic Geometry
        // Liquid: Fluid undulating circle
        float r_liq_circle = 0.18 + 0.015 * sin(phi_h * 5.0 + beat_phase * 1.2);
        float geom_liquid = smoothstep(0.005, 0.001, abs(r_h - r_liq_circle));

        // Metal: 8-point sharp razor star
        float star_val = abs(sin(phi_cw * 4.0));
        float r_metal_star = 0.08 + 0.14 * (star_val * star_val * star_val);
        float geom_metal = smoothstep(0.005, 0.001, abs(r_h - r_metal_star));

        // Cyber: Quantized technical octagonal grid
        float oct_a = mod(phi_ccw + 0.392699, 0.785398) - 0.392699;
        float r_cyber_oct = (0.19 + dilation * 0.02) / max(cos(oct_a), 0.6);
        float geom_cyber = smoothstep(0.005, 0.001, abs(r_h - r_cyber_oct));

        // Dubstep: Resonant pulsating hexagon
        float hex_a = mod(phi_cw + 0.523599, 1.047198) - 0.523599;
        float r_dub_hex = (0.21 + dilation * 0.035 * (1.0 + sin(beat_phase * 4.0) * 0.25)) / max(cos(hex_a), 0.6);
        float geom_dubstep = smoothstep(0.005, 0.001, abs(r_h - r_dub_hex));

        float compass_poly = u_biome_weights.x * geom_liquid
                           + u_biome_weights.y * geom_metal
                           + u_biome_weights.z * geom_cyber
                           + u_biome_weights.w * geom_dubstep;

        float compass_geom = (ring1 + ring2 + ticks1 * 1.4 + ticks2 * 1.4 + spokes * 0.5) * 0.55 + compass_poly * 0.75;
        float compass_mask = smoothstep(0.35, 0.22, r_h) * smoothstep(0.72, 0.95, rd_target_dot) * (1.0 - is_silent * 0.85);

        vec3 compass_col = mix(u_color_primary.rgb, u_color_accent.rgb, 0.5 + 0.5 * sin(phi_h * 2.0 + beat_phase));
        // Strict HDR cap <= 0.45 to prevent glare/burn
        float compass_intensity = clamp(compass_geom * compass_mask * 0.40, 0.0, 0.45);
        sky += compass_col * compass_intensity;
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
    float surface_ripple = ripple;
    float emission_pulse = u_physical_params.z;
    float spec_centroid  = u_physical_params.w;
    float arousal        = u_laser_pos.w;
    float is_silent      = u_extra_physics.z;
    float bpm            = max(u_extra_physics.w, 60.0);
    float beat_phase     = time * (bpm / 60.0) * 3.14159265;

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
        // Metal Biome: Faceted obsidian crystal normal
        if (w_metal > 0.05) {
            vec3 facet_n = normalize(floor(n * 5.0 + 0.5) / 5.0);
            n = normalize(mix(n, facet_n, 0.45 * w_metal));
        }
        float ao = calcAO(p, n);

        vec2 track_c = get_track_spline(p.z);
        vec2 track_offset = p.xy - track_c;
        float dist_center = abs(track_offset.x);

        // Ground/Floor detection: surfaces pointing upwards near track floor
        float is_ground = smoothstep(0.15, 0.65, n.y) * step(p.y, track_c.y + 0.5);
        float is_floor = is_ground;
        // Wall detection: vertical surfaces on corridor edges
        float is_wall = smoothstep(0.30, 0.80, abs(n.x));

        vec3 view_dir = -rd;

        // Distance-dependent anti-Moiré high-frequency attenuation
        float hf_fade = exp(-0.035 * t);

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
        float pert_strength = 0.06 * (treble_sparkle * 1.8 + melodic_mids * 0.9) * (0.4 + 0.8 * micro_boost) * (1.0 - is_silent) * hf_fade;
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
        vec3 neutral_palette = mix(u_color_primary.rgb, u_color_accent.rgb, 0.50);
        vec3 ivory_base = mix(vec3(0.96, 0.94, 0.90), neutral_palette, 0.40);
        vec3 headlight_color = mix(ivory_base, synesthetic_color, 0.55);
        vec3 cam_headlight = headlight_color * (cam_diff * spot_beam * cam_atten * 0.45);

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
        vec3 micro_specular = mix(accent_color, vec3(1.0), 0.20) * (micro_spec * (1.4 + treble_sparkle * 2.6 + micro_boost * 1.8)) * l_atten;

        // 3. Ambient Light (Deep rich obsidian darks with cubic AO falloff in crevices)
        vec3 ambient = (sky_color * 0.30 + synesthetic_color * 0.12 + u_color_zenith.rgb * 0.8) * (ao * ao * ao * 0.85 + 0.15);

        // Distance along ray for waterfall texture sampling
        float forward_t = clamp((p.z - ro.z) / 55.0, 0.0, 1.0);

        // 4. Distinct Biome Roadways, Fissures, Guide Rails & Patterns along Spline Track
        float biome_accent_pattern = 0.0;
        float track_glow = 0.0;

        // Vertical normalized height for monolith/tower spectral energy sampling
        float v_norm = clamp((p.y - (track_c.y - 1.5)) / 7.0, 0.05, 0.95);
        float wall_spec_energy = textureLod(u_waterfall_energy, vec2(v_norm, forward_t), 0.0).r;

        if (w_cyber > 0.12 && is_silent < 0.5) {
            // High-speed Cyber Matrix: Fine, elegant spaced lane lines on dark obsidian
            float center_dash = smoothstep(0.025, 0.005, dist_center) * step(0.0, sin(p.z * 0.785398));
            float side_lanes  = smoothstep(0.020, 0.005, abs(dist_center - 1.4)) * step(-0.2, sin(p.z * 1.57079));
            float curb_line   = smoothstep(0.025, 0.005, abs(dist_center - 2.4));
            float cyber_floor = is_ground * (center_dash * 1.2 + side_lanes * 0.9 + curb_line * 0.7);

            // Selective spectral equalizer panels on inner faces of towers (bass low, mids mid, treble high)
            float tower_inner = is_wall * smoothstep(7.0, 3.8, dist_center);
            float cyber_spec_panel = tower_inner * smoothstep(0.25, 0.85, wall_spec_energy) * (1.0 + melodic_mids * 1.4 + treble_sparkle * 1.8) * hf_fade;

            track_glow += (cyber_floor + cyber_spec_panel * 0.9) * w_cyber;
            biome_accent_pattern += (cyber_floor * 0.4 + cyber_spec_panel * 0.45) * w_cyber;
        }
        if (w_metal > 0.12 && is_silent < 0.5) {
            // Obsidian Chasm: Fine narrow central magma fissure and basalt shoulder veins
            float center_fissure = smoothstep(0.045, 0.008, dist_center) * smoothstep(0.35, 0.95, abs(sin(p.z * 0.8 + sin(p.x * 2.0))));
            float shoulder_mask  = smoothstep(1.6, 2.4, dist_center);
            float magma_vein     = smoothstep(0.75, 0.98, abs(sin(p.x * 2.2 + sin(p.z * 0.8) * 1.5) * cos(p.z * 1.2))) * hf_fade * shoulder_mask;

            // Deep obsidian megaliths with selective incandescent magma energy pulses
            float mega_inner = is_wall * smoothstep(11.0, 4.0, dist_center);
            float metal_spire_glow = mega_inner * smoothstep(0.35, 0.95, wall_spec_energy) * (0.8 + emission_pulse * 1.2) * hf_fade;

            float metal_floor = is_ground * (center_fissure * 1.3 + magma_vein * 0.7);
            track_glow += (metal_floor + metal_spire_glow * 0.85) * w_metal * (0.8 + emission_pulse * 0.5);
            biome_accent_pattern += (metal_floor * 0.4 + metal_spire_glow * 0.4) * w_metal;
        }
        if (w_dubstep > 0.12 && is_silent < 0.5) {
            // Quantum Bass Void: Fine spaced bass lane tracks and outer pulse rings
            float bass_lane  = is_ground * smoothstep(0.030, 0.006, abs(dist_center - 1.3)) * (0.7 + emission_pulse * 0.6);
            float ring_glow  = smoothstep(0.88, 0.98, sin(p.z * 1.57079 - time * 6.0)) * smoothstep(1.5, 4.5, length(track_offset));

            // Sub-bass resonant bar on inner resonator faces
            float res_inner = is_wall * smoothstep(6.5, 4.0, dist_center);
            float dub_wobble_bar = res_inner * smoothstep(0.30, 0.90, wall_spec_energy) * (1.2 + dilation * 1.5 + surface_ripple * 1.2) * hf_fade;

            float dub_features = bass_lane + ring_glow * 0.6 + dub_wobble_bar * 0.7;
            track_glow += dub_features * w_dubstep * (0.7 + emission_pulse * 0.5);
            biome_accent_pattern += dub_features * 0.3 * w_dubstep;
        }
        if (w_liquid > 0.12 && is_silent < 0.5) {
            // Liquid Silk Ocean: Fine bioluminescent central thread and wake ripples at shoulders
            float ocean_thread  = is_ground * smoothstep(0.025, 0.005, dist_center) * 1.1;
            float shoulder_wake = smoothstep(1.4, 2.5, dist_center);
            float wave_crests   = is_ground * smoothstep(0.72, 0.98, sin(p.x * 0.6 + time * 1.4) * cos(p.z * 0.5 - time * 0.8)) * shoulder_wake * 0.6;

            float liq_features = ocean_thread + wave_crests;
            track_glow += liq_features * w_liquid * (0.6 + emission_pulse * 0.4);
            biome_accent_pattern += liq_features * 0.25 * w_liquid;
        }

        // Celestial Nebula & Open-Air Stargate Runway Lighting
        if (w_celestial > 0.12 && is_silent < 0.5) {
            float causeway_lane = is_ground * smoothstep(0.025, 0.006, abs(dist_center - 1.5)) * (0.6 + treble_sparkle * 0.8);
            float sg_phase = mod(p.z + 16.0, 32.0) - 16.0;
            float is_stargate = smoothstep(0.50, 0.05, abs(sg_phase)) * smoothstep(0.35, 0.05, abs(length(track_offset) - 4.0));
            vec3 stargate_color = mix(u_color_primary.rgb, u_color_accent.rgb, 0.70);
            vec3 stargate_glow = stargate_color * (is_stargate * 1.5 * (1.0 + melodic_mids * 0.7));
            track_glow += (causeway_lane * 0.8) * w_celestial;
            biome_accent_pattern += causeway_lane * 0.25 * w_celestial;
        }

        biome_accent_pattern *= (0.2 + 0.8 * arousal) * (1.0 - is_silent);

        // Metal Biome: Incandescent magma veins in deep crevices (ao < 0.45)
        float magma_crevice = smoothstep(0.45, 0.12, ao) * w_metal;
        vec3 magma_glow = vec3(1.0, 0.30, 0.04) * (magma_crevice * (2.4 + emission_pulse * 1.6) * (1.0 - is_silent));

        // Liquid Silk Ocean: Specular Fresnel sheen reflecting celestial sky
        float liq_fresnel = pow(clamp(1.0 - max(dot(n, view_dir), 0.0), 0.0, 1.0), 4.0);
        vec3 liq_refl = mix(sky_color * 1.3, vec3(0.4, 0.85, 1.0) * (0.8 + emission_pulse * 0.5), 0.35);
        vec3 liq_specular = liq_refl * (liq_fresnel * 2.2 * is_floor * w_liquid * (1.0 - is_silent * 0.6));

        // Phase 2 & 3: 3D Procedural Architecture Neon Illumination & Shading
        // Dynamic Emergence: Read directly from physical bass-driven UBO channel
        float arch_emergence = clamp(u_color_zenith.w, 0.0, 1.0) * (1.0 - is_silent);
        float arch_fade = smoothstep(0.05, 0.65, arch_emergence) * smoothstep(130.0, 70.0, t);
        float arch_sink = (1.0 - arch_emergence) * (1.0 - arch_emergence) * 4.5;

        // Sonic Shockwave Rings & Pulse Portals along Spline Track (40% gain)
        float sonic_portal = 0.0;
        if (ripple > 0.05 && arch_emergence > 0.10 && is_silent < 0.5) {
            float ring_spacing = 16.0;
            float ring_phase = mod(p.z - time * 18.0, ring_spacing) - (ring_spacing * 0.5);
            float pulse_ring = smoothstep(0.24, 0.0, abs(ring_phase));
            float r_dist = length(track_offset);
            float portal_edge = smoothstep(0.20, 0.0, abs(r_dist - (2.7 + dilation * 0.5)));
            sonic_portal = pulse_ring * portal_edge * ripple * 1.4 * arch_fade;
        }

        // 1. Neon Gate Portals / Monumental Arches Shading with Polymorphic Biome Textures
        float gate_id = floor((p.z + 8.0) / 16.0);
        float gate_z_dist = abs(mod(p.z + 8.0, 16.0) - 8.0);
        float is_gate = smoothstep(0.40, 0.04, gate_z_dist);
        vec2 p_rel_gate = vec2(track_offset.x, track_offset.y + arch_sink);
        float gate_angle_sh = (dot(p_rel_gate, p_rel_gate) > 1e-6) ? atan(p_rel_gate.y, p_rel_gate.x) : 0.0;

        // Polymorphic Gate Contour Profile Matching
        float mod_liq_sh = sin(gate_angle_sh * 3.0 + beat_phase) * 0.09;
        float mod_met_sh = (abs(sin(gate_angle_sh * 4.0 + gate_id)) - 0.5) * 0.14;
        float cyber_step_sh = floor(beat_phase / 3.14159265 * 2.0) * (3.14159265 / 8.0);
        float oct_ang_sh = mod(gate_angle_sh + cyber_step_sh + 0.392699, 0.785398) - 0.392699;
        float mod_cyb_sh = (0.92388 / max(cos(oct_ang_sh), 0.7) - 0.96) * 1.5;
        float hex_ang_sh = mod(gate_angle_sh + beat_phase * 0.25 + 0.523599, 1.047198) - 0.523599;
        float grv_cw_sh  = sin(gate_angle_sh * 5.0 + beat_phase * 0.8);
        float grv_ccw_sh = sin(gate_angle_sh * 5.0 - beat_phase * 0.8);
        float mod_dub_sh = (0.8660 / max(cos(hex_ang_sh), 0.7) - 0.933) * 1.2 + (grv_cw_sh * grv_ccw_sh) * (0.03 + dilation * 0.04);
        float gate_poly_sh = w_liquid * mod_liq_sh + w_metal * mod_met_sh + w_cyber * mod_cyb_sh + w_dubstep * mod_dub_sh;

        float r_gate_sh = 2.65 + dilation * 0.50 + ripple * 0.30 + gate_poly_sh;
        float gate_edge = smoothstep(0.24, 0.02, abs(length(p_rel_gate) - r_gate_sh));
        float pillar_edge = smoothstep(0.24, 0.02, abs(abs(p_rel_gate.x) - r_gate_sh)) * step(-2.4, p_rel_gate.y) * step(p_rel_gate.y, 0.5);
        float arch_structure = max(gate_edge, pillar_edge);
        float gate_pattern = 1.0;
        vec3 gate_tint = accent_color;
        if (w_cyber > 0.05) {
            float cyber_seg = smoothstep(0.30, 0.75, sin(gate_angle_sh * 8.0 + beat_phase * 2.0));
            gate_pattern = mix(gate_pattern, 0.45 + 0.95 * cyber_seg, w_cyber);
            gate_tint = mix(gate_tint, mix(accent_color, u_color_primary.rgb, 0.35), w_cyber);
        }
        if (w_dubstep > 0.05) {
            float dub_seg = smoothstep(0.30, 0.75, sin(gate_angle_sh * 6.0 - beat_phase * 3.0) * cos(gate_z_dist * 6.0));
            gate_pattern = mix(gate_pattern, 0.40 + 1.10 * dub_seg, w_dubstep);
            gate_tint = mix(gate_tint, mix(accent_color, vec3(0.9, 0.2, 1.0), 0.5), w_dubstep);
        }
        if (w_metal > 0.05) {
            float magma_veins = smoothstep(0.35, 0.90, abs(sin(gate_angle_sh * 7.0 + p.z * 1.5 + time * 4.0)));
            gate_pattern = mix(gate_pattern, 0.40 + 1.20 * magma_veins * (1.0 + emission_pulse * 0.5), w_metal);
            gate_tint = mix(gate_tint, vec3(1.0, 0.32, 0.05), w_metal * 0.75);
        }
        if (w_liquid > 0.05) {
            float silk_flow = 0.70 + 0.30 * sin(gate_angle_sh * 2.0 + beat_phase * 0.8 + time * 1.5);
            gate_pattern = mix(gate_pattern, silk_flow, w_liquid);
            gate_tint = mix(gate_tint, mix(synesthetic_color, accent_color, 0.45), w_liquid);
        }

        vec3 gate_neon = gate_tint * (is_gate * arch_structure * gate_pattern * 1.45 * (1.0 + ripple * 0.70 + emission_pulse * 0.50));

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

        // Live spectral equalizer on rails: lower roadway rail samples bass/low-mids (u ~ 0.18), upper rail samples air/treble (u ~ 0.82)
        float rail_spec_low = textureLod(u_waterfall_energy, vec2(0.18, forward_t), 0.0).r;
        float rail_spec_hi  = textureLod(u_waterfall_energy, vec2(0.82, forward_t), 0.0).r;
        float is_upper_rail = step(0.0, track_offset.y);
        float rail_spec_energy = mix(rail_spec_low, rail_spec_hi, is_upper_rail);

        // High-velocity light pulse traveling on rails in sync with kicks/snares/hi-hats
        float rail_speed = 22.0 + surface_ripple * 18.0;
        float rail_pulse = smoothstep(0.72, 0.98, sin(p.z * 3.2 - time * rail_speed));
        float rail_sparkle_flash = treble_sparkle * smoothstep(0.60, 0.96, sin(p.z * 10.0 - time * 28.0));
        vec3 rail_neon = mix(synesthetic_color, accent_color, 0.70) * (is_rail * (1.20 + rail_spec_energy * 2.20 + rail_pulse * (1.40 + surface_ripple * 1.60) + rail_sparkle_flash * 1.20 + melodic_mids * 0.80));

        // 3. Floating Equalizer Monoliths Shading (Strictly Y-Bounded VU Pillars)
        float totem_z_dist = abs(mod(p.z + 3.0, 6.0) - 3.0);
        float totem_x_dist = abs(abs(track_offset.x) - (3.45 + dilation * 0.35));
        float cell_z_sh = floor((p.z + 3.0) / 6.0);
        float totem_h_sh = 1.1 + 2.2 * (dilation * 0.65 + melodic_mids * 0.75) * (0.65 + 0.35 * sin(cell_z_sh * 0.75 + time * 3.5));
        float totem_y_top = totem_h_sh - 1.1;
        float totem_y_eval = track_offset.y + arch_sink;
        float is_totem_y = step(-1.15, totem_y_eval) * step(totem_y_eval, totem_y_top + 0.05);
        float is_totem = smoothstep(0.40, 0.05, totem_x_dist) * step(totem_z_dist, 0.45) * is_totem_y;
        float bar_frac = clamp((totem_y_eval + 1.1) / max(totem_h_sh, 0.1), 0.0, 1.0);
        vec3 totem_bar_col = mix(synesthetic_color, accent_color, bar_frac);
        float bar_grid = smoothstep(0.15, 0.85, sin(totem_y_eval * 12.0)) * mix(1.0, hf_fade, 0.6);

        // Totem VU meter sampling live waterfall energy matrix at exact height frequency (bass low, mids mid, treble high)
        float totem_freq = clamp(bar_frac * 0.85 + 0.08, 0.05, 0.95);
        float totem_spec_energy = textureLod(u_waterfall_energy, vec2(totem_freq, forward_t), 0.0).r;
        vec3 totem_neon = totem_bar_col * (is_totem * bar_grid * (1.10 + totem_spec_energy * 2.40 + dilation * 0.80 + melodic_mids * 0.75));

        vec3 arch_emissive = (gate_neon + rail_neon + totem_neon) * arch_fade;

        // ---------------------------------------------------------------------
        // 3D Beat-Synced Kinetic Trails with Spectral Memory (Waterfall History)
        // ---------------------------------------------------------------------
        float trail_angle = (dot(track_offset, track_offset) > 1e-6) ? atan(track_offset.y, track_offset.x) : 0.0;
        float trail_freq = fract(trail_angle / 6.2831853 + 0.5);
        float trail_history = textureLod(u_waterfall_energy, vec2(trail_freq, forward_t), 0.0).r;

        // Helical biome patterns
        // Liquid: Smooth flowing double helix
        float liq_helix = sin(trail_angle * 2.0 - (p.z * 0.15 + beat_phase * 0.7));
        float liq_trail = smoothstep(0.80, 0.98, abs(liq_helix));

        // Metal: Sharp broken angular strokes
        float metal_helix = sin(trail_angle * 3.0 + p.z * 0.22 + beat_phase * 1.1 + sin(p.z * 0.6) * 1.4);
        float metal_trail = smoothstep(0.85, 0.99, abs(metal_helix));

        // Cyber: Parallel quantized beat tracks
        float cyber_quant_step = floor(beat_phase * 2.0) * 0.5;
        float cyber_helix = fract((trail_angle * 4.0 / 6.2831853) - p.z * 0.10 - cyber_quant_step * 0.25);
        float cyber_trail = smoothstep(0.035, 0.008, abs(cyber_helix - 0.5));

        // Dubstep: LFO wobble modulated waves
        float lfo_dub = sin(p.z * 0.7 - beat_phase * 1.8) * (0.4 + dilation * 0.4);
        float dub_helix = sin(trail_angle * 2.0 - p.z * 0.20 + lfo_dub * 2.2);
        float dub_trail = smoothstep(0.82, 0.98, abs(dub_helix));

        float kinetic_trail_geom = w_liquid * liq_trail
                                 + w_metal * metal_trail
                                 + w_cyber * cyber_trail
                                 + w_dubstep * dub_trail;

        float wall_proximity = smoothstep(1.7, 3.2, length(track_offset)) * exp(-0.022 * t);
        float kinetic_trail_intensity = kinetic_trail_geom * (0.30 + 0.70 * trail_history) * (0.75 + 0.55 * emission_pulse) * (1.0 - is_silent);
        vec3 trail_tint = mix(u_color_primary.rgb, u_color_accent.rgb, fract(forward_t * 1.8 + trail_freq));
        vec3 kinetic_trail_emissive = trail_tint * (kinetic_trail_intensity * wall_proximity * 0.85);

        // Dark Mirror Road: Saturated Roadway Emissive at 40% gain (prevents washing to white)
        vec3 road_emissive_tint = mix(u_color_primary.rgb, u_color_accent.rgb, clamp(dist_center * 0.40 + 0.15, 0.0, 1.0));
        vec3 road_emissive = road_emissive_tint * (track_glow * 0.72 + sonic_portal * 0.80) * (1.0 - is_silent);

        // Emissive light from track features, veins, sonic pulse portals, kinetic trails and 3D architecture
        vec3 emissive = road_emissive + arch_emissive + magma_glow + kinetic_trail_emissive;

        // 2. High-Frequency Harmonic Vein Pulses (Razor-Sharp Veins, audio-reactive)
        float vein_phase = p.z * 8.0 + p.x * 5.0 + sin(p.y * 10.0);
        float vein_pulse = smoothstep(0.965, 0.998, sin(vein_phase + time * (10.0 + treble_sparkle * 14.0))) * hf_fade;
        float micro_vein = vein_pulse * (treble_sparkle * 2.4 + melodic_mids * 1.2) * (0.4 + 1.2 * micro_boost) * (1.0 - is_silent);
        vec3 micro_vein_emissive = mix(accent_color, u_color_primary.rgb, 0.40) * (micro_vein * 0.50);

        // 3. Acoustic Micro-Sparkles (High-Hat & Cymbal Surface Stardust, audio-reactive)
        float sparkle_cell = hash3(floor(p * 52.0));
        float sparkle_mask = step(0.91, sparkle_cell);
        float sparkle_flash = abs(sin(sparkle_cell * 80.0 + time * 24.0)) * hf_fade;
        float micro_sparkle = sparkle_mask * sparkle_flash * (treble_sparkle * 3.2) * (0.4 + 1.4 * micro_boost) * (1.0 - is_silent);
        vec3 micro_sparkle_emissive = mix(u_color_primary.rgb, accent_color, 0.70) * (micro_sparkle * 0.50);

        // 4. Percussive Rim Flashes & Silhouette Hardening
        float rim_exp = mix(6.0, 16.0, micro_boost);
        float micro_rim = pow(clamp(1.0 - max(dot(n, view_dir), 0.0), 0.0, 1.0), rim_exp);
        float percussive_flash = micro_rim * (treble_sparkle * 1.9 + max(0.0, emission_pulse - 0.9) * 1.4) * (0.3 + 0.8 * micro_boost) * (1.0 - is_silent);
        vec3 micro_rim_emissive = mix(accent_color, u_color_primary.rgb, 0.50) * (percussive_flash * 0.50);

        // 4b. Destellos de Micro-Transitorios en Aristas (Edge Specular Flash)
        float edge_fresnel = pow(clamp(1.0 - max(dot(n, view_dir), 0.0), 0.0, 1.0), mix(4.0, 9.0, micro_boost));
        float edge_flash = edge_fresnel * (treble_sparkle * 2.8 + surface_ripple * 2.2) * (1.0 - is_silent);
        vec3 edge_specular_flash = mix(accent_color, vec3(1.0), 0.40) * (edge_flash * 0.65);

        // 5. Biome-Specific 100% Saturation Micro-Accents
        vec3 biome_micro_accents = vec3(0.0);
        if (w_liquid > 0.4) {
            float liq_micro = smoothstep(0.85, 0.98, sin(p.x * 14.0 + time * 4.0) * cos(p.z * 14.0 - time * 3.0)) * hf_fade;
            biome_micro_accents += vec3(0.3, 0.7, 1.0) * (liq_micro * treble_sparkle * 1.8 * w_liquid * micro_boost);
        }
        if (w_metal > 0.4) {
            float metal_micro = smoothstep(0.92, 0.99, abs(sin(p.x * 20.0 + sin(p.z * 12.0)))) * hf_fade;
            biome_micro_accents += vec3(1.0, 0.4, 0.1) * (metal_micro * (0.8 + emission_pulse * 1.5) * w_metal * micro_boost);
        }
        if (w_cyber > 0.4) {
            float cyber_micro = step(0.94, fract(p.z * 6.0)) * step(0.90, fract(p.x * 6.0)) * hf_fade;
            biome_micro_accents += vec3(0.1, 1.0, 0.8) * (cyber_micro * (1.0 + treble_sparkle * 2.0) * w_cyber * micro_boost);
        }
        if (w_dubstep > 0.4) {
            float dub_micro = smoothstep(0.88, 0.99, sin(p.y * 16.0 + p.z * 16.0 + time * 18.0)) * hf_fade;
            biome_micro_accents += vec3(0.9, 0.2, 1.0) * (dub_micro * (1.0 + emission_pulse * 1.8) * w_dubstep * micro_boost);
        }

        // Base Albedo: solid obsidian darks tinted by zenith and primary + synesthetic tint
        vec3 dark_obsidian = u_color_zenith.rgb * 1.4 + u_color_primary.rgb * 0.02;
        vec3 base_mat = mix(dark_obsidian, synesthetic_color * 0.70, 0.35);
        vec3 albedo = mix(base_mat, accent_color * 0.85, clamp(biome_accent_pattern, 0.0, 0.65));

        // Dark Mirror Road: Center roadway has dampened diffuse albedo and reflective obsidian finish
        if (is_floor > 0.01) {
            float road_corridor = smoothstep(2.8, 0.5, dist_center);
            albedo = mix(albedo, dark_obsidian * 0.06, is_floor * road_corridor * 0.90);
        }

        // Standard Fresnel reflection
        float fresnel = pow(clamp(1.0 - max(dot(n, view_dir), 0.0), 0.0, 1.0), 3.0);
        vec3 fresnel_rim = mix(albedo * 0.15, accent_color * 0.80, fresnel) * (fresnel * 0.40);

        // Mirror reflection on dark obsidian floor (saturated, non-washed)
        float floor_fresnel = pow(clamp(1.0 - max(dot(n, view_dir), 0.0), 0.0, 1.0), 2.8);
        vec3 floor_mirror = mix(u_color_primary.rgb * 0.45, sky_color * 0.65, 0.5) * (floor_fresnel * 0.65 * is_floor);

        // Combined Surface Color with Multi-Scale Ambient Occlusion and Micro-Details
        vec3 total_micro = micro_specular + cam_headlight_glint + micro_vein_emissive + micro_sparkle_emissive + micro_rim_emissive + edge_specular_flash + biome_micro_accents;
        vec3 ambient_light = albedo * ambient * ao;
        vec3 direct_light = albedo * (cam_headlight + laser_light);
        vec3 reflective_light = (fresnel_rim + floor_mirror + total_micro + liq_specular) * (0.35 + 0.65 * ao);
        scene_color = ambient_light + direct_light + reflective_light + emissive;

        // Depth fog: Rich atmospheric aerial perspective past 35m into horizon sky
        // Hermite blend to 100% sky color at t_max ensures seamless transition (zero pop/cutoff)
        float fog_norm = clamp((t - 35.0) / (t_max - 40.0), 0.0, 1.0);
        float fog = fog_norm * fog_norm * (3.0 - 2.0 * fog_norm);
        float horizon_blend = smoothstep(30.0, 130.0, t);
        vec3 horizon_glow = mix(u_color_primary.rgb * 0.40, u_color_accent.rgb * 0.35, 0.5) * (0.30 + arousal * 0.25) * (1.0 - is_silent * 0.7);
        vec3 fog_color = mix(horizon_glow, sky_color, horizon_blend);
        scene_color = mix(scene_color, fog_color, fog);

        // Diegetic Holographic Horizon Beat-Compass Bleed-Through past 65m
        if (t > 65.0) {
            float compass_bleed = smoothstep(65.0, 135.0, t) * (1.0 - is_silent * 0.85);
            scene_color = mix(scene_color, sky_color, compass_bleed * 0.65);
        }
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
