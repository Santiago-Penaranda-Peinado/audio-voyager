#version 450 core

layout(location = 0) in vec4 a_pos_life;
layout(location = 1) in vec4 a_vel_mass;

layout(std140, binding = 1) uniform AudioPhysicsUniforms {
    vec4 u_audio_physics;  // x: spec_centroid_norm, y: dissonance, z: onset_strength, w: is_onset
    vec4 u_audio_energy;   // x: energy_rms, y: peak_amplitude, z: sub_bass, w: high_treble
    vec4 u_sim_params;     // x: delta_time, y: total_time, z: particle_count, w: damping
    vec4 u_physics_scales; // x: gravity_scale, y: vorticity_scale, z: shockwave_scale, w: attraction_scale
    vec4 u_color_base;     // rgb: base color, w: point_size_scale
    vec4 u_color_peak;     // rgb: peak color, w: opacity_scale
    vec4 u_cam_pos;        // xyz: camera pos, w: melodic_mids
    vec4 u_laser_pos;      // xyz: laser entity pos, w: treble_sparkle
    vec4 u_cam_dir;        // xyz: camera forward dir, w: spare
};

uniform mat4 u_view_proj;

out vec4 v_color;

void main() {
    vec3 pos = a_pos_life.xyz;
    float life = a_pos_life.w;
    vec3 vel = a_vel_mass.xyz;
    float speed = length(vel);

    gl_Position = u_view_proj * vec4(pos, 1.0);

    float treble_sparkle = u_laser_pos.w;
    float size_mult = u_color_base.w;
    float opacity_mult = u_color_peak.w;

    // Incandescent needle-sharp spark & filament sizing
    float dist_w = max(gl_Position.w, 0.5);
    float base_size = (36.0 / pow(dist_w, 0.58)) * size_mult;
    float audio_size_boost = 1.0 + u_audio_energy.x * 0.45 + treble_sparkle * 0.55;
    gl_PointSize = clamp(base_size * audio_size_boost, 2.0, 24.0);

    // Procedural Color Blending (Harmonized Biome Base to Peak)
    float spec_centroid = u_audio_physics.x;
    float dissonance = u_audio_physics.y;
    float onset = u_audio_physics.z;

    float energy_factor = clamp(
        speed * 0.08 + 
        onset * 0.45 + 
        dissonance * 0.25 + 
        treble_sparkle * 0.45 +
        max(0.0, (spec_centroid - 0.35) * 0.5), 
        0.0, 1.0
    );

    vec3 final_color = mix(u_color_base.rgb, u_color_peak.rgb, energy_factor);

    // Incandescent thermal flash on fast friction sparks
    final_color += vec3(0.20, 0.16, 0.10) * clamp(speed * 0.04 + onset * 0.6, 0.0, 1.0);
    final_color += vec3(treble_sparkle * 0.40);

    // Near-plane fadeout: smooth fade as particle approaches camera near plane to prevent clipping/blinding
    float near_fade = smoothstep(0.4, 1.8, dist_w);

    // Translucent alpha calibrated to avoid whiteout
    float alpha = clamp(life * 0.70 * (0.35 + u_audio_energy.x * 0.45) * opacity_mult * near_fade, 0.0, 0.85);
    v_color = vec4(final_color, alpha);
}
