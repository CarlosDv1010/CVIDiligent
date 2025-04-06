#ifndef RECTANGLES_H
#define RECTANGLES_H

#include "hittable.h"
#include "material.h"

// ---------------------
// 1) xy_rect
// Plano Z = k, abarca [x0,x1] y [y0,y1]
// ---------------------
class xy_rect : public hittable {
public:
    xy_rect() {}

    // k: la coordenada Z donde está el rectángulo
    // x0, x1: rango en X
    // y0, y1: rango en Y
    // mat: material
    xy_rect(double x0, double x1, double y0, double y1, double k,
        shared_ptr<material> mat)
        : x0(x0), x1(x1), y0(y0), y1(y1), k(k), mp(mat)
    {
    }

    virtual bool hit(const ray& r, interval ray_t, hit_record& rec) const override
    {
        // Resolver intersección con plano Z = k
        double t = (k - r.origin().z()) / r.direction().z();
        if (t < ray_t.min || t > ray_t.max) return false;

        double x = r.origin().x() + t * r.direction().x();
        double y = r.origin().y() + t * r.direction().y();
        if (x < x0 || x > x1 || y < y0 || y > y1) return false;

        rec.t = t;
        rec.p = r.at(t);

        // Normal sale en +z o -z según el sentido del rayo
        vec3 outward_normal(0, 0, 1);
        rec.set_face_normal(r, outward_normal);
        rec.mat = mp;

        return true;
    }

public:
    shared_ptr<material> mp;
    double x0, x1, y0, y1, k;
};

// ---------------------
// 2) xz_rect
// Plano Y = k, abarca [x0,x1] y [z0,z1]
// ---------------------
class xz_rect : public hittable {
public:
    xz_rect() {}

    xz_rect(double x0, double x1, double z0, double z1, double k,
        shared_ptr<material> mat)
        : x0(x0), x1(x1), z0(z0), z1(z1), k(k), mp(mat)
    {
    }

    virtual bool hit(const ray& r, interval ray_t, hit_record& rec) const override
    {
        // Intersección con plano Y = k
        double t = (k - r.origin().y()) / r.direction().y();
        if (t < ray_t.min || t > ray_t.max) return false;

        double x = r.origin().x() + t * r.direction().x();
        double z = r.origin().z() + t * r.direction().z();
        if (x < x0 || x > x1 || z < z0 || z > z1) return false;

        rec.t = t;
        rec.p = r.at(t);

        // Normal sale en +y o -y
        vec3 outward_normal(0, 1, 0);
        rec.set_face_normal(r, outward_normal);
        rec.mat = mp;

        return true;
    }

public:
    shared_ptr<material> mp;
    double x0, x1, z0, z1, k;
};

// ---------------------
// 3) yz_rect
// Plano X = k, abarca [y0,y1] y [z0,z1]
// ---------------------
class yz_rect : public hittable {
public:
    yz_rect() {}

    yz_rect(double y0, double y1, double z0, double z1, double k,
        shared_ptr<material> mat)
        : y0(y0), y1(y1), z0(z0), z1(z1), k(k), mp(mat)
    {
    }

    virtual bool hit(const ray& r, interval ray_t, hit_record& rec) const override
    {
        // Intersección con plano X = k
        double t = (k - r.origin().x()) / r.direction().x();
        if (t < ray_t.min || t > ray_t.max) return false;

        double y = r.origin().y() + t * r.direction().y();
        double z = r.origin().z() + t * r.direction().z();
        if (y < y0 || y > y1 || z < z0 || z > z1) return false;

        rec.t = t;
        rec.p = r.at(t);

        // Normal sale en +x o -x
        vec3 outward_normal(1, 0, 0);
        rec.set_face_normal(r, outward_normal);
        rec.mat = mp;

        return true;
    }

public:
    shared_ptr<material> mp;
    double y0, y1, z0, z1, k;
};

#endif
