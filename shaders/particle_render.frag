#version 450 core

in vec4 v_color;
out vec4 frag_color;

void main() {
    // Circular point sprite profile
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist_sq = dot(coord, coord);
    if (dist_sq > 0.25) {
        discard;
    }

    // Celestial Stardust Profile: sharp luminous core + gentle ethereal halo
    float core = exp(-dist_sq * 36.0);
    float halo = exp(-dist_sq * 10.0) * 0.35;
    float intensity = core + halo;

    frag_color = vec4(v_color.rgb * (1.0 + core * 0.4), v_color.a * intensity);
}
