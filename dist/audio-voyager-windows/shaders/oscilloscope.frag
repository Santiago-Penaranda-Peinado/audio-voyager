#version 450 core

in float v_progress;
in float v_amplitude;

uniform float u_rms;
uniform float u_peak;
uniform vec3 u_color_primary = vec3(0.08, 0.42, 0.88);
uniform vec3 u_color_accent  = vec3(1.00, 0.72, 0.22);

out vec4 frag_color;

void main() {
    // Dynamic holographic laser ribbon color harmonized with biome palette
    vec3 col_edge = u_color_primary;
    vec3 col_core = vec3(1.0, 1.0, 1.0);
    vec3 col_hot  = u_color_accent;

    vec3 col = mix(col_edge, col_core, clamp(v_amplitude * 1.5, 0.0, 1.0));
    col = mix(col, col_hot, clamp(u_peak * 0.8, 0.0, 1.0));

    float alpha = clamp(0.7 + u_rms * 0.3 + v_amplitude * 0.5, 0.3, 1.0);
    frag_color = vec4(col * 2.0, alpha); // Emissive HDR glow for additive blend
}
