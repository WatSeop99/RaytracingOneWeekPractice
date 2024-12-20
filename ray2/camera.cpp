#include "framework.h"
#include "BitmapBuffer.h"
#include "rtweekend.h"
#include "hittable.h"
#include "hittable_list.h"
#include "material.h"
#include "camera.h"

void camera::init(hittable_list* pWorld)
{
    _ASSERT(pWorld);

    DWORD physicalCoreCount = 0;
    DWORD logicalThreadCount = 0;
    if (!GetPhysicalCoreCount(&physicalCoreCount, &logicalThreadCount))
    {
        __debugbreak();
    }

    m_TotalThreadCount = physicalCoreCount;

    m_pThreadParams = new ThreadParam[m_TotalThreadCount];
    if (!m_pThreadParams)
    {
        __debugbreak();
    }
    ZeroMemory(m_pThreadParams, sizeof(ThreadParam) * m_TotalThreadCount);

    m_hCompleted = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_hCompleted || m_hCompleted == INVALID_HANDLE_VALUE)
    {
        __debugbreak();
    }

    for (DWORD threadNo = 0; threadNo < physicalCoreCount; ++threadNo)
    {
        for (int i = 0; i < THREAD_EVENT_TOTAL_COUNT; ++i)
        {
            m_pThreadParams[threadNo].hEventList[i] = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if (!m_pThreadParams[threadNo].hEventList[i] || m_pThreadParams[threadNo].hEventList[i] == INVALID_HANDLE_VALUE)
            {
                __debugbreak();
            }
        }

        m_pThreadParams[threadNo].ThreadNo = (int)threadNo;
        m_pThreadParams[threadNo].TotalThreadCount = (int)m_TotalThreadCount;
        m_pThreadParams[threadNo].pBitmapBuffer = m_pBitmapBuffer;
        m_pThreadParams[threadNo].pCamera = this;
        m_pThreadParams[threadNo].pWorld = pWorld;
        m_pThreadParams[threadNo].phCompleted = &m_hCompleted;

        UINT threadID = 0;
        m_hThreads[threadNo] = (HANDLE)_beginthreadex(nullptr, 0, ThreadFunc, m_pThreadParams + threadNo, 0, &threadID);
        if (!m_hThreads[threadNo] || m_hThreads[threadNo] == INVALID_HANDLE_VALUE)
        {
            __debugbreak();
        }
    }
}

void camera::clear()
{
    if (m_hCompleted)
    {
        CloseHandle(m_hCompleted);
        m_hCompleted = nullptr;
    }

    for (DWORD threadNo = 0; threadNo < m_TotalThreadCount; ++threadNo)
    {
        SetEvent(m_pThreadParams[threadNo].hEventList[THREAD_EVENT_DESTROY]);

        for (int i = 0; i < THREAD_EVENT_TOTAL_COUNT; ++i)
        {
            if (m_pThreadParams[threadNo].hEventList[i])
            {
                CloseHandle(m_pThreadParams[threadNo].hEventList[i]);
                m_pThreadParams[threadNo].hEventList[i] = nullptr;
            }
        }

        if (m_hThreads[threadNo])
        {
            CloseHandle(m_hThreads[threadNo]);
            m_hThreads[threadNo] = nullptr;
        }
    }

    if (m_pThreadParams)
    {
        delete m_pThreadParams;
        m_pThreadParams = nullptr;
    }
}

void camera::render(const hittable& world)
{
    _ASSERT(m_pBitmapBuffer);

    initialize();

    //std::cout << "P3\n" << image_width << ' ' << image_height << "\n255\n";
    WCHAR szDebugString[MAX_PATH];
    swprintf_s(szDebugString, MAX_PATH, L"width : %d, height : %d\n", image_width, image_height);
    OutputDebugString(szDebugString);

    //for (int j = 0; j < image_height; ++j)
    //{
    //    //std::clog << "\rScanlines remaining: " << (image_height - j) << ' ' << std::flush;
    //    swprintf_s(szDebugString, MAX_PATH, L"Scanlines remaining: %d \n", image_height - j);
    //    OutputDebugString(szDebugString);

    //    for (int i = 0; i < image_width; ++i)
    //    {
    //        color pixel_color(0, 0, 0);
    //        for (int sample = 0; sample < samples_per_pixel; ++sample)
    //        {
    //            ray r = get_ray(i, j);
    //            pixel_color += ray_color(r, max_depth, world);
    //        }
    //        //write_color(std::cout, pixel_samples_scale * pixel_color);
    //        {
    //            pixel_color = pixel_samples_scale * pixel_color;

    //            double r = pixel_color.x();
    //            double g = pixel_color.y();
    //            double b = pixel_color.z();

    //            // Apply a linear to gamma transform for gamma 2
    //            r = linear_to_gamma(r);
    //            g = linear_to_gamma(g);
    //            b = linear_to_gamma(b);

    //            // Translate the [0,1] component values to the byte range [0,255].
    //            static const interval intensity(0.000, 0.999);
    //            int rbyte = int(256 * intensity.clamp(r));
    //            int gbyte = int(256 * intensity.clamp(g));
    //            int bbyte = int(256 * intensity.clamp(b));

    //            // Write out the pixel color components.
    //            m_pBitmapBuffer->SetColor(i, j, RGB(rbyte, gbyte, bbyte));
    //        }
    //    }
    //}

    Completed = m_TotalThreadCount;
    for (DWORD threadNo = 0; threadNo < m_TotalThreadCount; ++threadNo)
    {
        if (!m_pThreadParams[threadNo].hEventList[THREAD_EVENT_COMPUTE])
        {
            __debugbreak();
        }
        SetEvent(m_pThreadParams[threadNo].hEventList[THREAD_EVENT_COMPUTE]);
    }

    WaitForSingleObject(m_hCompleted, INFINITE);

    //std::clog << "\rDone.                 \n";
    swprintf_s(szDebugString, MAX_PATH, L"Done.                 \n");
    OutputDebugString(szDebugString);

    m_pBitmapBuffer->Draw();
}

void camera::set_bitmapDC(BitmapBuffer* pBitmapBuffer)
{
    m_pBitmapBuffer = pBitmapBuffer;
}

void camera::initialize()
{
    image_height = int(image_width / aspect_ratio);
    image_height = (image_height < 1) ? 1 : image_height;

    pixel_samples_scale = 1.0 / samples_per_pixel;

    center = lookfrom;

    // Determine viewport dimensions.
    double theta = degrees_to_radians(vfov);
    double h = std::tan(theta / 2);
    double viewport_height = 2 * h * focus_dist;
    double viewport_width = viewport_height * (double(image_width) / image_height);

    // Calculate the u,v,w unit basis vectors for the camera coordinate frame.
    w = unit_vector(lookfrom - lookat);
    u = unit_vector(cross(vup, w));
    v = cross(w, u);

    // Calculate the vectors across the horizontal and down the vertical viewport edges.
    vec3 viewport_u = viewport_width * u;    // Vector across viewport horizontal edge
    vec3 viewport_v = viewport_height * -v;  // Vector down viewport vertical edge

    // Calculate the horizontal and vertical delta vectors from pixel to pixel.
    pixel_delta_u = viewport_u / image_width;
    pixel_delta_v = viewport_v / image_height;

    // Calculate the location of the upper left pixel.
    vec3 viewport_upper_left = center - (focus_dist * w) - viewport_u / 2 - viewport_v / 2;
    pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

    // Calculate the camera defocus disk basis vectors.
    double defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2));
    defocus_disk_u = u * defocus_radius;
    defocus_disk_v = v * defocus_radius;
}

ray camera::get_ray(int i, int j) const
{
    // Construct a camera ray originating from the origin and directed at randomly sampled
    // point around the pixel location i, j.

    vec3 offset = sample_square();
    vec3 pixel_sample = pixel00_loc + ((i + offset.x()) * pixel_delta_u) + ((j + offset.y()) * pixel_delta_v);

    vec3 ray_origin = ((defocus_angle <= 0) ? center : defocus_disk_sample());
    vec3 ray_direction = pixel_sample - ray_origin;

    return ray(ray_origin, ray_direction);
}

vec3 camera::sample_square() const
{
    // Returns the vector to a random point in the [-.5,-.5]-[+.5,+.5] unit square.
    return vec3(random_double() - 0.5, random_double() - 0.5, 0);
}

point3 camera::defocus_disk_sample() const
{
    // Returns a random point in the camera defocus disk.
    vec3 p = random_in_unit_disk();
    return center + (p[0] * defocus_disk_u) + (p[1] * defocus_disk_v);
}

color camera::ray_color(const ray& r, int depth, const hittable& world) const
{
    // If we've exceeded the ray bounce limit, no more light is gathered.
    if (depth <= 0)
    {
        return color(0, 0, 0);
    }

    hit_record rec;

    if (world.hit(r, interval(0.001, infinity), rec))
    {
        ray scattered;
        color attenuation;
        if (rec.mat->scatter(r, rec, attenuation, scattered))
        {
            return attenuation * ray_color(scattered, depth - 1, world);
        }
        return color(0, 0, 0);
    }

    vec3 unit_direction = unit_vector(r.direction());
    double a = 0.5 * (unit_direction.y() + 1.0); // using alpha value using ray direction's y param. this will have [0.0, 1.0].
    return (1.0 - a) * color(1.0, 1.0, 1.0) + a * color(0.5, 0.7, 1.0);
}
