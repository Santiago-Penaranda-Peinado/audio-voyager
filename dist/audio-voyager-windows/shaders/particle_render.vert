#version 450 core

layout(location = 0) in vec4 a_pos_life;
layout(location = 1) in vec4 a_vel_mass;

layout(std140, binding = 1) uniform AudioPhysicsUniforms {
    vec4 u_audio_physics;  // x: spec_centroid_norm, y: dissonance, z: onset_strength, w: is_onset
    vec4 u_audio_energy;   // x: energy_rms, y: peak_amplitude, z: sub_bass, w: high_treble
    vec4 u_sim_params;     // x: delta_time, y: total_time, z: particle_count, w: damping
    vec4 u_physics_scales; // x: gravity_scale, y: vorticity_scale, z: shockwave_scale, w: attraction_scale
    vec4 u_color_base;     // rgb: base resting color, w: point_size_scale
    vec4 u_color_peak;     // rgb: peak high-energy color, w: spare
};

uniform mat4 u_view_proj;

out vec4 v_color;

void main() {
    vec3 pos = a_pos_life.xyz;
    float life = a_pos_life.w;
    vec3 vel = a_vel_mass.xyz;
    float speed = length(vel);

    gl_Position = u_view_proj * vec4(pos, 1.0);

    // Dynamic point sprite sizing based on camera distance, RMS and point size scale tuner
    float size_mult = u_color_base.w;
    float point_size = (350.0 / max(gl_Position.w, 0.5)) * (0.7 + u_audio_energy.x * 0.8 + life * 0.5) * size_mult;
    gl_PointSize = clamp(point_size, 1.5, 24.0);

    // Procedural Color Blending (Base Resting Color <-> Peak High-Energy Color)
    float spec_centroid = u_audio_physics.x;
    float dissonance = u_audio_physics.y;
    float onset = u_audio_physics.z;

    // Continuous physics energy factor [0.0 -> 1.0]
    float energy_factor = clamp(
        speed * 0.12 + 
        onset * 0.45 + 
        dissonance * 0.35 + 
        max(0.0, (spec_centroid - 0.35) * 0.6), 
        0.0, 1.0
    );

    vec3 final_color = mix(u_color_base.rgb, u_color_peak.rgb, energy_factor);

    // Alpha modulated by lifespan and onset burst
    float alpha = clamp(life * 0.85 * (0.6 + u_audio_energy.x * 0.6), 0.05, 0.95);
    v_color = vec4(final_color, alpha);
}
