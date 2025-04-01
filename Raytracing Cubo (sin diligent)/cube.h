#ifndef CUBE_H
#define CUBE_H

#include "hittable.h"
#include "interval.h"
#include <cmath>

class cube : public hittable {
public:
    cube(point3 min, point3 max, shared_ptr<material> m)
        : box_min(min), box_max(max), mat(m) {
    }

    virtual bool hit(const ray& r, interval ray_t, hit_record& rec) const override;

public:
    point3 box_min, box_max;
    shared_ptr<material> mat;
};

bool cube::hit(const ray& r, interval ray_t, hit_record& rec) const {
    double tmin = ray_t.min, tmax = ray_t.max;
    // Método de slabs para intersección de AABB
    for (int i = 0; i < 3; i++) {
        auto invD = 1.0 / r.direction()[i];
        auto t0 = (box_min[i] - r.origin()[i]) * invD;
        auto t1 = (box_max[i] - r.origin()[i]) * invD;
        if (invD < 0) std::swap(t0, t1);
        tmin = t0 > tmin ? t0 : tmin;
        tmax = t1 < tmax ? t1 : tmax;
        if (tmax <= tmin)
            return false;
    }

    rec.t = tmin;
    rec.p = r.at(rec.t);

    // Calcular la normal de la cara golpeada
    vec3 outward_normal(0, 0, 0);
    const double epsilon = 1e-4;
    for (int i = 0; i < 3; i++) {
        if (std::fabs(rec.p[i] - box_min[i]) < epsilon) {
            outward_normal[i] = -1;
            break;
        }
        if (std::fabs(rec.p[i] - box_max[i]) < epsilon) {
            outward_normal[i] = 1;
            break;
        }
    }
    rec.set_face_normal(r, outward_normal);
    rec.mat = mat;
    return true;
}

#endif
