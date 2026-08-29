#version 450 core

in vec2 v_uv;
out vec4 frag_color;

#define DEBUG_MODE 0

layout(std140, binding = 0) uniform RaymarchingUniforms {
    vec4 u_resolution_time;   // xy: resolution, z: total_time, w: delta_time
    vec4 u_cam_pos;           // xyz: camera position, w: camera roll
    vec4 u_cam_dir;           // xyz: forward direction, w: dynamic FOV
    vec4 u_cam_up;            // xyz: up vector, w: camera speed
    vec4 u_audio_params_1;    // x: norm_dissonance, y: norm_centroid, z: norm_energy, w: norm_sub_bass
    vec4 u_audio_params_2;    // x: norm_treble, y: cavity_scale, z: glitch_intensity, w: is_onset
    vec4 u_laser_pos;         // xyz: laser light position, w: speed_lines
    vec4 u_tuners;            // x: bloom_intensity, y: chromatic_aberration, z: speed_multiplier, w: reserved
};

// Continuous HSV to RGB Converter
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

// Smooth Minimum (Polynomial smin)
float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

// 2D Rotation Matrix
mat2 rot(float a) {
    float s = sin(a), c = cos(a);
    return mat2(c, s, -s, c);
}

// 3D Infinite Gyroid Distance
float gyroid(vec3 p, float scale, float thickness, float bias) {
    vec3 q = p * scale;
    float g = abs(dot(sin(q), cos(q.zxy)) + bias) - thickness;
    return g / scale;
}

// 3D Path Function for Camera Tracking
vec2 getPath(float z) {
    float x = 2.4 * sin(z * 0.075) + 1.2 * cos(z * 0.16);
    float y = 1.6 * cos(z * 0.055) + 0.8 * sin(z * 0.12);
    return vec2(x, y);
}

// 100% Procedural Infinite Domain SDF (Gyroid + KIFS Space Folding)
float map(vec3 p) {
    float time = u_resolution_time.z;
    float diss = u_audio_params_1.x;      // AGC Dissonance [0.0 to 1.0]
    float spec_centroid = u_audio_params_1.y;  // AGC Spectral Centroid [0.0 to 1.0]
    float energy = u_audio_params_1.z;    // AGC Energy [0.0 to 1.0]
    float sub_bass = u_audio_params_1.w;  // AGC Sub-Bass [0.0 to 1.0]
    float treble = u_audio_params_2.x;    // AGC Treble [0.0 to 1.0]
    float scale_mod = u_audio_params_2.y; // Dynamic Scale
    float glitch = u_audio_params_2.z;

    vec2 path_xy = getPath(p.z);
    vec3 p_loc = vec3(p.xy - path_xy, p.z);

    // 1. Kinetic Space Torsion & Z-Twist
    float twist = p.z * (0.08 + diss * 0.35);
    vec3 q = p_loc;
    q.xy = rot(twist) * q.xy;

    // 2. Shockwave Ripple / Space Warp Glitch
    float dist_xy = length(p_loc.xy);
    float shock_wave = sin(dist_xy * 3.5 - time * 18.0) * exp(-0.35 * dist_xy) * glitch * 0.6;

    // 3. Infinite Procedural Gyroid Topology (Liquid Minimal Surface)
    float g_scale1 = 0.55 * scale_mod;
    float g_thick1 = 0.22 + sub_bass * 0.25;
    float g_bias1 = 0.35 * sin(p.z * 0.25 + time * 0.8);
    float d_gyroid1 = gyroid(q, g_scale1, g_thick1, g_bias1);

    // High-frequency harmonic ripples on the gyroid surface
    float g_scale2 = 1.65;
    float g_thick2 = 0.08 + energy * 0.12;
    float d_gyroid2 = gyroid(q + vec3(sin(time), cos(time), 0.0), g_scale2, g_thick2, 0.0);

    float d_fluid = smin(d_gyroid1, d_gyroid2, 0.45);

    // 4. KIFS Space Folding (Sharpens into razor fractals with Dissonance)
    vec3 kp = q * 0.35;
    float k_scale = 1.0;
    float fold_angle = 0.45 + diss * 3.0 + treble * 0.8;
    mat2 rot_xy = rot(fold_angle);
    mat2 rot_yz = rot(fold_angle * 0.72);

    for (int i = 0; i < 4; ++i) {
        kp = abs(kp) - vec3(0.60, 0.50, 0.70) * mix(1.0, 0.65, diss);
        kp.xy = rot_xy * kp.xy;
        kp.yz = rot_yz * kp.yz;
        kp *= 1.40;
        k_scale *= 1.40;
    }
    float d_kifs = (length(kp) - 0.85) / k_scale;

    // 5. Open central navigation corridor along the camera spline
    float corridor_radius = 2.8 + sub_bass * 1.4;
    float d_corridor = corridor_radius - dist_xy; // Cavity > 0

    // Combine Gyroid/KIFS with continuous blending and open corridor
    float d_procedural = smin(d_fluid, d_kifs, mix(0.75, 0.08, diss));
    float final_dist = smin(d_corridor, d_procedural, 0.65) + shock_wave * 0.35;

    return final_dist;
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
        float h = 0.01 + 0.12 * float(i) / 4.0;
        float d = map(p + h * n);
        occ += (h - d) * sca;
        sca *= 0.85;
    }
    return clamp(1.0 - 2.0 * occ, 0.0, 1.0);
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

    // Audio reactive variables
    float time = u_resolution_time.z;
    float diss = u_audio_params_1.x;
    float spec_centroid = u_audio_params_1.y;
    float energy = u_audio_params_1.z;
    float is_onset = u_audio_params_2.w;

    // Continuous Synesthetic Color (HSV Mapping)
    // Low Centroid (Bass/Warmth) -> Red/Amber (0.02 - 0.12)
    // Mid Centroid -> Emerald/Cyan (0.35 - 0.55)
    // High Centroid (Air/Treble) -> Electric Violet/Magenta (0.75 - 0.95)
    float base_hue = fract(spec_centroid * 0.85 + ro.z * 0.006 + time * 0.012);
    float sat = mix(0.92, 0.15, pow(diss, 1.8)); // Dissonance desaturates to stark monochrome chrome
    float val = 0.6 + energy * 1.5 + is_onset * 0.8;
    vec3 synesthetic_color = hsv2rgb(vec3(base_hue, sat, val));

    // Background mood color
    vec3 ambient_bg = hsv2rgb(vec3(fract(base_hue + 0.5), sat * 0.8, 0.03 + energy * 0.05));

    // Raymarching loop
    float t = 0.05;
    float t_max = 85.0;
    float hit_dist = 0.0;
    vec3 p = ro;
    bool hit = false;

    vec3 accum_glow = vec3(0.0);

    for (int i = 0; i < 96; ++i) {
        p = ro + rd * t;
        hit_dist = map(p);

        // Volumetric glowing fog accumulation with synesthetic color
        float density = 0.020 / (1.0 + hit_dist * hit_dist * 3.0);
        accum_glow += synesthetic_color * density * (0.8 + energy * 2.0);

        if (hit_dist < 0.002) {
            hit = true;
            break;
        }

        t += hit_dist * 0.82;
        if (t >= t_max) break;
    }

#if DEBUG_MODE
    if (hit) {
        vec3 n = calcNormal(p);
        frag_color = vec4(n * 0.5 + 0.5, 1.0);
    } else {
        frag_color = vec4(1.0, 0.0, 1.0, 1.0);
    }
    return;
#endif

    vec3 scene_color = vec3(0.0);

    if (hit) {
        vec3 n = calcNormal(p);
        float ao = calcAO(p, n);

        // Laser Ribbon Light
        vec3 laser_pos = u_laser_pos.xyz;
        vec3 l_dir = laser_pos - p;
        float l_dist = length(l_dir);
        l_dir = normalize(l_dir);

        float l_atten = 1.0 / (1.0 + l_dist * 0.12 + l_dist * l_dist * 0.03);
        float diff = max(dot(n, l_dir), 0.0);

        // Specular highlight (Sharpened by dissonance)
        vec3 view_dir = -rd;
        vec3 half_v = normalize(l_dir + view_dir);
        float spec_power = mix(24.0, 140.0, diss);
        float spec = pow(max(dot(n, half_v), 0.0), spec_power);

        // Fresnel reflection & Quartz Glass Iridescence
        float fresnel = pow(clamp(1.0 - max(dot(n, view_dir), 0.0), 0.0, 1.0), mix(3.0, 1.5, diss));
        
        vec3 iridescence = mix(
            ambient_bg * 2.0,
            synesthetic_color * (1.2 + energy * 1.5),
            fresnel
        );

        vec3 albedo = mix(
            synesthetic_color * 0.4,
            vec3(0.92, 0.94, 0.98), // Chrome in high dissonance
            diss * 0.75
        );

        vec3 laser_light = synesthetic_color * (diff * 2.5 + spec * mix(4.0, 14.0, diss)) * l_atten;
        vec3 ambient = ambient_bg * ao;

        scene_color = (albedo * (ambient + laser_light) + iridescence * fresnel * 2.5) * ao;
    } else {
        scene_color = ambient_bg;
    }

    // Blend volumetric emission
    scene_color += accum_glow;

    // Atmospheric distance fog falloff
    float fog = 1.0 - exp(-t * 0.025);
    scene_color = mix(scene_color, ambient_bg, fog);

    frag_color = vec4(scene_color, 1.0);
}
