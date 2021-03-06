#include "physics.h"

void physics__lattice_populate(vec r_[], vec v_[], const int n, const float box_radius, const float energy) {
    vec * restrict r = __builtin_assume_aligned(r_, 64);
    vec * restrict v = __builtin_assume_aligned(v_, 64);


    assert(box_radius > 0);
    assert(n >= 0);

    const int m = (int) roundf(sqrtf(n/2));
    assert(2 * sq(m) == n  && "'n / 2' needs to be a perfect square");

    const float lattice_step = 2 * box_radius / (m - 0);

    for (int i = 0; i < m; ++i) {
        #pragma omp simd aligned(r: 64)
        for (int j = 0; j < m; ++j) {
            r[(i * m) + j].x = -box_radius + j * lattice_step;
            r[(i * m) + j].y = -box_radius + i * lattice_step;
        }
        #pragma omp simd aligned(r: 64)
        for (int j = 0; j < m; ++j) {
            r[sq(m) + (i * m) + j].x = -box_radius + (j + 0.5f) * lattice_step;
            r[sq(m) + (i * m) + j].y = -box_radius + (i + 0.5f) * lattice_step;
        }
    }

    for (int i = 0; i < n; ++i) {
        v[i] = mul(normal_vec(), energy);
    }

    // Zero the average moment
    vec v_avg = to_vec(0, 0);
    #pragma omp simd aligned(v: 64)
    for (int i = 0; i < n; ++i)
    {
        v_avg = add(v_avg, v[i]);
    }
    v_avg = mul(v_avg, 1.f/n);
    
    #pragma omp simd aligned(v: 64)
    for (int i = 0; i < n; ++i)
    {
        v[i] = sub(v[i], v_avg);
    }
}

// Insertion sort (the fastest for almost sorted arrays)
void physics__sort_by_Y(vec r_[], vec v_[], const int n) {
    vec * restrict r = __builtin_assume_aligned(r_, 64);
    vec * restrict v = __builtin_assume_aligned(v_, 64);


    for (int i = 1; i < n; ++i) {
        if unlikely(r[i].y > r[i-1].y) {
            int j = i-2;
            while ((j >= 0) && (r[j].y <= r[i].y)) {
                --j;
            }

            const vec temp_r = r[i];
            memmove(&r[j+2], &r[j+1], (i - (j+1)) * sizeof(*r));
            r[j+1] = temp_r;
            
            const vec temp_v = v[i];
            memmove(&v[j+2], &v[j+1], (i - (j+1)) * sizeof(*v));
            v[j+1] = temp_v;
        }
    }

    TEST(
        for (int i = 1; i < n; ++i) {
            assert(r[i-1].y >= r[i].y);
            assert(memcmp(&r[i-1], &r[i], sizeof(r[i])) && "This is very likely an error");
        }
    )
}

// Periodic Boundary Condition
inline vec physics__periodic_boundary_shift(vec v, const float box_radius) {
    if unlikely(norm_max(v) > box_radius) {
        if unlikely(v.x > box_radius) {
            v.x -= 2 * box_radius;
        }
        else if (v.x < -box_radius) {
            v.x += 2 * box_radius;
        }

        if unlikely(v.y > box_radius) {
            v.y -= 2 * box_radius;
        }
        else if (v.y < -box_radius) {
            v.y += 2 * box_radius;
        }
    }

    return v;
}

static inline void wall_bounce(vec * restrict r, vec * restrict v, const float box_radius) {
    if unlikely(norm_max(*r) > box_radius) {
        // Horizontal collisions
        if unlikely(r->x > box_radius) {
            v->x *= -1;
            r->x = -r->x + 2 * box_radius;
        }
        else if likely(r->x < -box_radius) {
            v->x *= -1;
            r->x = -r->x - 2 * box_radius;
        }

        // Vertical collisions
        if unlikely(r->y > box_radius) {
            v->y *= -1;
            r->y = -r->y + 2 * box_radius;
        }
        else if likely(r->y < -box_radius) {
            v->y *= -1;
            r->y = -r->y + 2 * -box_radius;
        }
    }
}

// Velocity verlet
inline void physics__update(vec r_[], vec v_[], vec a_[], const int n, const float dt, const float box_radius) {
    vec * restrict r = __builtin_assume_aligned(r_, 64);
    vec * restrict v = __builtin_assume_aligned(v_, 64);
    vec * restrict a = __builtin_assume_aligned(a_, 64);

    vec accs[THREAD_COUNT][n] __attribute__ ((aligned (64)));


    #pragma omp simd aligned(v, a: 64)
    for (int i = 0; i < n; ++i) {
        v[i].x += 0.5f * a[i].x * dt;
        v[i].y += 0.5f * a[i].y * dt;
    }
    
    #pragma omp simd aligned(r, v: 64)
    for (int i = 0; i < n; ++i) {
        r[i].x += v[i].x * dt;
        r[i].y += v[i].y * dt;
    }

    #pragma omp simd aligned(a: 64)
    for (int i = 0; i < n; ++i) {
        a[i].x = 0;
        a[i].y = 0;
    }

    #pragma omp simd aligned(accs: 64)
    for (int i = 0; i < n; ++i) {
        for (int t = 0; t < THREAD_COUNT; ++t) {
            accs[t][i] = to_vec(0, 0);
        }
    }


    #pragma omp parallel
    {   
        #pragma omp for schedule(auto)
        for (int i = 0; i < n-1; ++i) {
            for (int j = i+1; j < n; ++j) {
                vec dr = sub(r[j], r[i]);
#ifdef PBC
                dr = physics__periodic_boundary_shift(dr, box_radius);
#endif

                const float recip_drdr = 1/dot(dr, dr);
                const float recip_drdr_cube = recip_drdr * recip_drdr * recip_drdr;

                // Gradient of the Lennard-Jones potential
                const vec acc = mul(dr, -24 * recip_drdr * ( 2 * recip_drdr_cube * recip_drdr_cube - recip_drdr_cube));

                const int t = omp_get_thread_num();
                accs[t][i] = add(accs[t][i], acc);
                accs[t][j] = sub(accs[t][j], acc);
            }
        }

        #pragma omp for schedule(static)
        for (int i = 0; i < n; ++i) {
            for (int t = 0; t < THREAD_COUNT; ++t) {
                a[i].x += accs[t][i].x;
                a[i].y += accs[t][i].y;
            }

            v[i].x += 0.5f * a[i].x * dt;
            v[i].y += 0.5f * a[i].y * dt;

#ifdef PBC
            r[i] = physics__periodic_boundary_shift(r[i], box_radius);
#else
            wall_bounce(&r[i], &v[i], box_radius);
#endif
        }
    }
}

float physics__thermometer(const vec v[], const int n) {
    float momentum = 0;

    #pragma omp simd aligned(v: 64) reduction(+: momentum)
    for (int i = 0; i < n; ++i) {
        momentum += norm_sq(v[i]);
    }

    return (momentum * 0.5f) / n;
}

float physics__barometer(const vec v[], const int n, const float box_radius) {
    float momentum = 0;

    #pragma omp simd aligned(v: 64) reduction(+: momentum)
    for (int i = 0; i < n; ++i) {
        momentum += norm_sq(v[i]);
    }

    return (0.5f * momentum) / sq(2 * box_radius);
}
