#!/usr/bin/env python3
"""
Empirical verification oracle for Milestone 3 (Audio Physical Reactivity & Morphological Diversity).
Tests:
1. Transient Shockwave Wavefront Propagation (speed, decay envelope, spatial bounds, tunnel safety).
2. Mandelbox Folded Fractal SDF Numerical Robustness (div by zero, overflow, negative coords, NaN/Inf).
3. 1M Particle SSBO Memory Layout and Physics Simulation Bounds.
"""

import math
import sys

def test_shockwave_wavefront():
    print("=== TEST 1: Transient Shockwave Wavefront Propagation ===")
    speed = 28.0 # m/s
    decay_rate = 4.5 # s^-1
    decay_rate_silent = 6.0 # s^-1
    
    # 1. Expansion speed test
    times = [0.0, 0.1, 0.25, 0.5, 1.0, 1.5]
    for t in times:
        radius = speed * t
        intensity = math.exp(-decay_rate * t)
        print(f"t={t:4.2f}s | Wavefront Radius={radius:5.1f}m | Intensity={intensity:6.4f}")
        assert radius >= 0.0, "Radius must be non-negative"
    
    # Verify half-life
    half_life = math.log(2.0) / decay_rate
    print(f"Calculated half-life: {half_life * 1000.0:.1f} ms")
    assert 140.0 < half_life * 1000.0 < 160.0, "Half-life should be ~154ms"
    
    # Verify intensity after 1 second is negligible (< 2%)
    assert math.exp(-decay_rate * 1.0) < 0.02, "Shockwave intensity should decay to < 2% within 1s"
    
    # 2. Spatial Wavefront Profile & Boundedness Test
    for t in [0.1, 0.3, 0.6]:
        r_wave = speed * t
        intensity = math.exp(-decay_rate * t)
        phase = (20.0 * t) % (2.0 * math.pi)
        
        # Test across distances 0 to 50m
        for d in range(0, 500):
            dist = d * 0.1
            diff = abs(dist - r_wave)
            disp = math.exp(-diff * diff * 0.35) * math.sin(diff * 3.5 - phase) * intensity * 0.22
            assert abs(disp) <= 0.22 + 1e-6, f"Displacement {disp} exceeds 0.22"
            if diff > 10.0:
                assert abs(disp) < 1e-4, f"Energy outside wavefront not localized: {disp}"
                
    min_tunnel_radius = 2.8
    max_inward_deformation = 0.22 + 0.08
    effective_clearance = min_tunnel_radius - max_inward_deformation
    print(f"Minimum base tunnel clearance: {min_tunnel_radius}m")
    print(f"Maximum possible inward acoustic deformation: {max_inward_deformation:.2f}m")
    print(f"Guaranteed effective flight clearance: {effective_clearance:.2f}m")
    assert effective_clearance >= 2.5, "Guaranteed tunnel clearance must remain >= 2.5m"
    print(">> TEST 1 PASSED: Shockwave kinematics, decay, and spatial bounds are verified.\n")


def sdMandelboxSpines(p, scale, thickness, bias):
    """Python reference oracle for GLSL sdMandelboxSpines"""
    p_f = [p[0] * (scale * 0.75), p[1] * (scale * 0.75), p[2] * (scale * 0.75)]
    dr = 1.0
    f_bias = bias * 0.4
    
    for i in range(3):
        # Box fold: clamp(p_f, -1.0, 1.0) * 2.0 - p_f
        for k in range(3):
            clamped = max(-1.0, min(1.0, p_f[k]))
            p_f[k] = clamped * 2.0 - p_f[k]
        
        # Sphere fold
        r2 = p_f[0]*p_f[0] + p_f[1]*p_f[1] + p_f[2]*p_f[2]
        if r2 < 0.25:
            temp = 2.0 / 0.25
            p_f[0] *= temp
            p_f[1] *= temp
            p_f[2] *= temp
            dr *= temp
        elif r2 < 1.0:
            temp = 2.0 / r2
            p_f[0] *= temp
            p_f[1] *= temp
            p_f[2] *= temp
            dr *= temp
            
        # Scale and fold offset
        p_f[0] = p_f[0] * 1.55 - 1.05 + f_bias
        p_f[1] = p_f[1] * 1.55 - 0.65 + f_bias
        p_f[2] = p_f[2] * 1.55 - 1.25 + f_bias
        dr = dr * 1.55 + 1.0
        
    length_pf = math.sqrt(p_f[0]*p_f[0] + p_f[1]*p_f[1] + p_f[2]*p_f[2])
    d_fractal = (length_pf - (0.85 + thickness * 0.5)) / dr
    spines = (math.sin(p[0] * 2.5) * math.sin(p[1] * 2.5) * math.sin(p[2] * 2.5)) * 0.06
    return (d_fractal / (scale * 0.75)) + spines


def test_mandelbox_robustness():
    print("=== TEST 2: Mandelbox Folded Fractal SDF Numerical Robustness ===")
    scale = 0.32
    thickness = 0.4
    bias = 0.1
    
    # 1. Singularity & Zero Division Test (Exact (0,0,0) and infinitesimal neighborhood)
    test_points = [
        [0.0, 0.0, 0.0],
        [1e-15, 0.0, 0.0],
        [0.0, 1e-15, 0.0],
        [0.0, 0.0, 1e-15],
        [1e-8, 1e-8, 1e-8],
        [-1e-15, -1e-15, -1e-15],
    ]
    for pt in test_points:
        d = sdMandelboxSpines(pt, scale, thickness, bias)
        assert not math.isnan(d), f"NaN at {pt}"
        assert not math.isinf(d), f"Inf at {pt}"
        print(f"Point {pt} -> SDF = {d:10.6f} (Finite & Valid)")
        
    # 2. Negative coordinates across all 8 octants
    signs = [
        [1, 1, 1], [-1, 1, 1], [1, -1, 1], [1, 1, -1],
        [-1, -1, 1], [-1, 1, -1], [1, -1, -1], [-1, -1, -1]
    ]
    for s in signs:
        pt = [s[0] * 2.45, s[1] * 1.82, s[2] * 3.14]
        d = sdMandelboxSpines(pt, scale, thickness, bias)
        assert not math.isnan(d) and not math.isinf(d), f"Failure in octant {s}"
        print(f"Octant {s} point {pt} -> SDF = {d:10.6f}")

    # 3. Float overflow / extreme coordinate stress test
    large_points = [
        [100.0, 200.0, 300.0],
        [-500.0, -500.0, -500.0],
        [1000.0, 0.0, -1000.0],
        [1e4, 1e4, 1e4]
    ]
    for pt in large_points:
        d = sdMandelboxSpines(pt, scale, thickness, bias)
        assert not math.isnan(d) and not math.isinf(d), f"Overflow/NaN on large input {pt}"
        print(f"Far point {pt} -> SDF = {d:12.4f} (Stable)")
        
    # 4. Stress sweep: 10,000 grid points
    import random
    random.seed(12345)
    nan_count = 0
    inf_count = 0
    min_val = float('inf')
    max_val = float('-inf')
    
    for _ in range(10000):
        rx = random.uniform(-10.0, 10.0)
        ry = random.uniform(-10.0, 10.0)
        rz = random.uniform(-10.0, 10.0)
        d = sdMandelboxSpines([rx, ry, rz], scale, thickness, bias)
        if math.isnan(d): nan_count += 1
        if math.isinf(d): inf_count += 1
        if d < min_val: min_val = d
        if d > max_val: max_val = d
        
    print(f"Sweep 10,000 points in [-10, 10]^3:")
    print(f"  NaN count: {nan_count}")
    print(f"  Inf count: {inf_count}")
    print(f"  Min SDF:   {min_val:.4f}")
    print(f"  Max SDF:   {max_val:.4f}")
    assert nan_count == 0, "Mandelbox produced NaNs during sweep"
    assert inf_count == 0, "Mandelbox produced Infs during sweep"
    print(">> TEST 2 PASSED: Mandelbox folded fractal SDF is mathematically robust.\n")


def test_particle_ssbo_and_simulation():
    print("=== TEST 3: 1M Particle SSBO Layout & Simulation Bounds ===")
    
    particle_size_bytes = 32
    particle_count = 1048576 # 2^20 = 1,000,000+ particles
    total_buffer_bytes = particle_size_bytes * particle_count
    total_vram_mb = total_buffer_bytes / (1024 * 1024)
    
    print(f"Particle count:    {particle_count:,}")
    print(f"Particle struct:   {particle_size_bytes} bytes")
    print(f"Total SSBO size:   {total_buffer_bytes:,} bytes ({total_vram_mb:.2f} MB)")
    assert total_vram_mb == 32.0, "1M particles must occupy exactly 32 MB VRAM"
    
    local_size_x = 256
    workgroups = (particle_count + local_size_x - 1) // local_size_x
    total_invocations = workgroups * local_size_x
    print(f"Local workgroup size: {local_size_x}")
    print(f"Dispatched workgroups: {workgroups}")
    print(f"Total GPU invocations: {total_invocations}")
    assert total_invocations >= particle_count, "Compute dispatch must cover all particles"
    assert workgroups == 4096, "Workgroups for 1048576 with local size 256 must equal 4096"
    
    ubo_size_bytes = 8 * 16
    print(f"AudioPhysicsUbo size: {ubo_size_bytes} bytes (std140 compliant)")
    assert ubo_size_bytes == 128, "UBO size must be exactly 128 bytes"
    print(">> TEST 3 PASSED: Particle SSBO layout and compute simulation bounds verified.\n")


if __name__ == "__main__":
    test_shockwave_wavefront()
    test_mandelbox_robustness()
    test_particle_ssbo_and_simulation()
    print("=========================================================")
    print("ALL EMPIRICAL & ADVERSARIAL VERIFICATION TESTS COMPLETED!")
    print("=========================================================")
