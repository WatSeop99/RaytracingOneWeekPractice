#ifndef HITTABLE_H
#define HITTABLE_H

class material;

class hit_record
{
public:
    point3 p; // hit point.
    vec3 normal; // normal vector at hit point.
    shared_ptr<material> mat;
    double t; // time? t at (v = a + tb)
    bool front_face;

    void set_face_normal(const ray& r, const vec3& outward_normal)
    {
        // Sets the hit record normal vector.
        // NOTE: the parameter `outward_normal` is assumed to have unit length.

        front_face = (dot(r.direction(), outward_normal) < 0);
        normal = (front_face ? outward_normal : -outward_normal);
    }
};

class hittable
{
public:
    virtual ~hittable() = default;

    virtual bool hit(const ray& r, interval ray_t, hit_record& rec) const = 0;
};

#endif
