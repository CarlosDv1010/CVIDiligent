#include "rtweekend.h"
#include "camera.h"
#include "hittable_list.h"
#include "cube.h"
#include "material.h"
#include <vector>

int main() {
    hittable_list world;

    // Piso: un cubo gigante que simula un plano
    auto ground_material = make_shared<lambertian>(color(0.5, 0.5, 0.5));
    world.add(make_shared<cube>(
        point3(-1000, -1, -1000),
        point3(1000, 0, 1000),
        ground_material
    ));

    // Centros de cubos especiales para evitar colisiones:
    // Cubo dieléctrico: de (-1,0,-1) a (1,2,1) => centro (0,1,0), medio lado = 1.
    // Cubo difuso: de (-4,0,-1) a (-2,2,1) => centro (-3,1,0), medio lado = 1.
    // Cubo metálico: de (4,0,-1) a (6,2,1) => centro (5,1,0), medio lado = 1.
    std::vector<point3> specialCenters;
    specialCenters.push_back(point3(0, 1, 0));
    specialCenters.push_back(point3(-3, 1, 0));
    specialCenters.push_back(point3(5, 1, 0));

    // Vector para almacenar los centros de los cubos pequeños ya colocados
    std::vector<point3> placedCenters;

    // Bucle similar al original, con muchos pequeños cubos
    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            auto choose_mat = random_double();
            point3 center(a + 0.9 * random_double(), 0.2, b + 0.9 * random_double());

            // Evitamos colisiones con cubos especiales
            bool collides = false;
            for (const auto& sp_center : specialCenters) {
                if (std::fabs(center.x() - sp_center.x()) < 1.2 &&
                    std::fabs(center.z() - sp_center.z()) < 1.2) {
                    collides = true;
                    break;
                }
            }
            // Evitar colisiones con cubos pequeños ya colocados (lado 0.4 => separación mínima en x y z: 0.4)
            for (const auto& placed : placedCenters) {
                if (std::fabs(center.x() - placed.x()) < 0.4 &&
                    std::fabs(center.z() - placed.z()) < 0.4) {
                    collides = true;
                    break;
                }
            }
            if (collides) continue;

            // Asignar materiales con probabilidad igual (1/3 cada uno)
            shared_ptr<material> cube_material;
            if (choose_mat < 1.0 / 3) {
                // Difuso
                auto albedo = color::random() * color::random();
                cube_material = make_shared<lambertian>(albedo);
            }
            else if (choose_mat < 2.0 / 3) {
                // Metálico
                auto albedo = color::random(0.5, 1);
                auto fuzz = random_double(0, 0.5);
                cube_material = make_shared<metal>(albedo, fuzz);
            }
            else {
                // Dieléctrico (vidrio)
                cube_material = make_shared<dielectric>(1.5);
            }
            // Crear el cubo pequeño (lado = 0.4, extendido 0.2 en cada dirección)
            world.add(make_shared<cube>(
                center - vec3(0.2, 0.2, 0.2),
                center + vec3(0.2, 0.2, 0.2),
                cube_material
            ));
            placedCenters.push_back(center);
        }
    }

    // Cubos especiales para ver claramente los materiales:

    // Cubo dieléctrico (vidrio)
    auto material1 = make_shared<dielectric>(1.5);
    world.add(make_shared<cube>(
        point3(-1, 0, -1),
        point3(1, 2, 1),
        material1
    ));

    // Cubo difuso
    auto material2 = make_shared<lambertian>(color(0.4, 0.2, 0.1));
    world.add(make_shared<cube>(
        point3(-4, 0, -1),
        point3(-2, 2, 1),
        material2
    ));

    // Cubo metálico
    auto material3 = make_shared<metal>(color(0.7, 0.6, 0.5), 0.0);
    world.add(make_shared<cube>(
        point3(4, 0, -1),
        point3(6, 2, 1),
        material3
    ));

    // Configuración de la cámara
    camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 500;
    cam.samples_per_pixel = 50;
    cam.max_depth = 25;

    cam.vfov = 20;
    cam.lookfrom = point3(13, 2, 3);
    cam.lookat = point3(0, 0, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0.6;
    cam.focus_dist = 10.0;

    cam.render(world);
}
