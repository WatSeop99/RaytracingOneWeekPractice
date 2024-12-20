#ifndef CAMERA_H
#define CAMERA_H

//#include "hittable.h"
//#include "hittable_list.h"
//#include "material.h"
#include "thread.h"

class BitmapBuffer;
class hittable;
class hittable_list;

class camera
{
public:
    static const UINT MAX_THREAD_COUNT = 20;

    volatile long Completed = 0;


    double aspect_ratio = 1.0;  // Ratio of image width over height
    int    image_width = 100;  // Rendered image width in pixel count
    int    samples_per_pixel = 10;   // Count of random samples for each pixel
    int    max_depth = 10;   // Maximum number of ray bounces into scene

    double vfov = 90;  // Vertical view angle (field of view)
    point3 lookfrom = point3(0, 0, 0);   // Point camera is looking from
    point3 lookat = point3(0, 0, -1);  // Point camera is looking at
    vec3   vup = vec3(0, 1, 0);     // Camera-relative "up" direction

    double defocus_angle = 0;  // Variation angle of rays through each pixel
    double focus_dist = 10;    // Distance from camera lookfrom point to plane of perfect focus

    void init(hittable_list* pWorld);

    void clear();

    void render(const hittable* pWorld);

    void set_bitmapDC(BitmapBuffer* pBitmapBuffer);

private:
    BitmapBuffer* m_pBitmapBuffer = nullptr;
    HANDLE m_hThreads[MAX_THREAD_COUNT] = { nullptr, };
    HANDLE m_hCompleted = nullptr;
    ThreadParam* m_pThreadParams = nullptr;
    DWORD m_TotalThreadCount = MAX_THREAD_COUNT;

    int    image_height;   // Rendered image height
    double pixel_samples_scale;  // Color scale factor for a sum of pixel samples
    point3 center;         // Camera center
    point3 pixel00_loc;    // Location of pixel 0, 0
    vec3   pixel_delta_u;  // Offset to pixel to the right
    vec3   pixel_delta_v;  // Offset to pixel below
    vec3   u, v, w;              // Camera frame basis vectors
    vec3   defocus_disk_u;       // Defocus disk horizontal radius
    vec3   defocus_disk_v;       // Defocus disk vertical radius

    void initialize();

    ray get_ray(int i, int j) const;

    vec3 sample_square() const;

    point3 defocus_disk_sample() const;

    color ray_color(const ray& r, int depth, const hittable& world) const;
};

#endif