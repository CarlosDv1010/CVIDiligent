#ifndef CUBE_H
#define CUBE_H

#include "hittable_list.h"
#include "rectangles.h"  // donde están xy_rect, xz_rect, yz_rect

class cube : public hittable {
public:
    cube() {}
    cube(const point3& p0, const point3& p1, shared_ptr<material> ptr);

    virtual bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        return sides.hit(r, ray_t, rec);
    }

public:
    point3 box_min, box_max;
    hittable_list sides;  // almacena los 6 rectángulos
};

cube::cube(const point3& p0, const point3& p1, shared_ptr<material> ptr) {
    box_min = p0;
    box_max = p1;

    // p0.x() < p1.x(), etc.
    // 1) Caras en X = p0.x() y X = p1.x()
    sides.add(make_shared<yz_rect>(
        p0.y(), p1.y(), p0.z(), p1.z(), p0.x(), ptr
    ));
    sides.add(make_shared<yz_rect>(
        p0.y(), p1.y(), p0.z(), p1.z(), p1.x(), ptr
    ));

    // 2) Caras en Y = p0.y() y Y = p1.y()
    sides.add(make_shared<xz_rect>(
        p0.x(), p1.x(), p0.z(), p1.z(), p0.y(), ptr
    ));
    sides.add(make_shared<xz_rect>(
        p0.x(), p1.x(), p0.z(), p1.z(), p1.y(), ptr
    ));

    // 3) Caras en Z = p0.z() y Z = p1.z()
    sides.add(make_shared<xy_rect>(
        p0.x(), p1.x(), p0.y(), p1.y(), p0.z(), ptr
    ));
    sides.add(make_shared<xy_rect>(
        p0.x(), p1.x(), p0.y(), p1.y(), p1.z(), ptr
    ));
}

#endif
