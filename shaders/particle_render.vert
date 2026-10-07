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

    // Elegant, delicate pin-sharp stardust sizing (1.2 to 2.8 pixels, never giant blobs!)
    float dist_w = max(gl_Position.w, 1.0);
    float base_size = (18.0 / pow(dist_w, 0.70)) * size_mult;
    float audio_size_boost = 1.0 + u_audio_energy.x * 0.35 + treble_sparkle * 0.45;
    gl_PointSize = clamp(base_size * audio_size_boost, 1.2, 2.8);

    // Procedural Color Blending (Harmonized Biome Base to Peak)
    float spec_centroid = u_audio_physics.x;
    float dissonance = u_audio_physics.y;
    float onset = u_audio_physics.z;

    float energy_factor = clamp(
        speed * 0.12 + 
        onset * 0.40 + 
        dissonance * 0.25 + 
        treble_sparkle * 0.45 +
        max(0.0, (spec_centroid - 0.35) * 0.5), 
        0.0, 1.0
    );

    vec3 final_color = mix(u_color_base.rgb, u_color_peak.rgb, energy_factor);

    // High frequency stardust twinkle
    final_color += vec3(treble_sparkle * 0.50);

    // Near-plane fadeout: smooth fade as particle approaches camera near plane to prevent clipping/blinding
    float near_fade = smoothstep(1.5, 4.2, dist_w);

    // Delicate translucent alpha (never blinding whiteout)
    float alpha = clamp(life * 0.50 * (0.30 + u_audio_energy.x * 0.50) * opacity_mult * near_fade, 0.0, 0.65);
    v_color = vec4(final_color, alpha);
}
