#version 450 core

in vec4 v_color;
out vec4 frag_color;

void main() {
    // Diamond needle spark profile
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist_sq = dot(coord, coord);
    if (dist_sq > 0.25) {
        discard;
    }

    // Incandescent spark profile: needle-sharp diamond core + hot plasma corona
    float diamond = abs(coord.x) + abs(coord.y);
    float core = exp(-dist_sq * 45.0) + exp(-diamond * 12.0) * 0.5;
    float corona = exp(-dist_sq * 12.0) * 0.35;
    float intensity = core + corona;

    // Searing hot core shifts to incandescent white-gold
    vec3 spark_col = mix(v_color.rgb, vec3(1.0, 0.98, 0.94), clamp(core * 0.80, 0.0, 1.0));

    frag_color = vec4(spark_col * (1.2 + core * 0.6), v_color.a * intensity);
}
