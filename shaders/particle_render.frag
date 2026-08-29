#version 450 core

in vec4 v_color;
out vec4 frag_color;

void main() {
    // Smooth circular Gaussian point sprite profile
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist_sq = dot(coord, coord);
    if (dist_sq > 0.25) {
        discard;
    }

    // Soft Gaussian bloom falloff
    float intensity = exp(-dist_sq * 10.0);
    frag_color = vec4(v_color.rgb * 1.4, v_color.a * intensity);
}
