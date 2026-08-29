#version 450 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in float a_progress;

uniform mat4 u_view_proj;
uniform float u_time;
uniform float u_rms;
uniform float u_peak;

out float v_progress;
out float v_amplitude;

void main() {
    v_progress = a_progress;
    v_amplitude = abs(a_position.y);

    vec3 pos = a_position;
    // Organic 3D helical wave depth
    pos.z += sin(a_progress * 6.28318 + u_time * 1.5) * (0.8 + u_rms * 2.0);

    gl_Position = u_view_proj * vec4(pos, 1.0);
}
