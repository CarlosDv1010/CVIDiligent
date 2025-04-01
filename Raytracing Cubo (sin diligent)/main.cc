#include "rtweekend.h"
#include "camera.h"
#include "hittable_list.h"
#include "sphere.h"
#include "cube.h"
#include "material.h"

int main() {
    hittable_list world;

    auto ground_material = make_shared<lambertian>(color(0.5, 0.5, 0.5));
    world.add(make_shared<sphere>(point3(0, -1000, 0), 1000, ground_material));

    auto cube_material = make_shared<lambertian>(color(0.7, 0.3, 0.3));
    world.add(make_shared<cube>(
        point3(-1, 0, -1),
        point3(1, 2, 1),
        cube_material
    ));

    camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 720;
    cam.samples_per_pixel = 100;
    cam.max_depth = 20;

    cam.vfov = 30;                  
    cam.lookfrom = point3(8, 4, 8);     
    cam.lookat = point3(0, 1, 0);    
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0.3;          
    cam.focus_dist = 12.0;         



    cam.render(world);
}
