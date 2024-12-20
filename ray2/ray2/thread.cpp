#include "framework.h"
#include "BitmapBuffer.h"
#include "rtweekend.h"
#include "camera.h"
#include "hittable.h"
#include "hittable_list.h"
#include "material.h"
#include "sphere.h"
#include "thread.h"

ray GetRay(int i, int j, camera* pCamera);
color RayColor(const ray& r, int depth, const hittable& world);

inline vec3 SampleSquare()
{
	return vec3(random_double() - 0.5, random_double() - 0.5, 0);
}

DWORD CountSetBits(ULONG_PTR bitMask)
{
	DWORD LSHIFT = sizeof(ULONG_PTR) * 8 - 1;
	DWORD bitSetCount = 0;
	ULONG_PTR bitTest = (ULONG_PTR)1 << LSHIFT;
	DWORD i;

	for (i = 0; i <= LSHIFT; ++i)
	{
		bitSetCount += ((bitMask & bitTest) ? 1 : 0);
		bitTest /= 2;
	}

	return bitSetCount;
}

BOOL GetPhysicalCoreCount(DWORD* pdwOutPhysicalCoreCount, DWORD* pdwOutLogicalCoreCount)
{
	BOOL bResult = FALSE;

	{
		LPFN_GLPI glpi;

		PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pBuffer = nullptr;
		PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = nullptr;
		DWORD returnLength = 0;
		DWORD logicalProcessorCount = 0;
		DWORD numaNodeCount = 0;
		DWORD processorCoreCount = 0;
		DWORD processorL1CacheCount = 0;
		DWORD processorL2CacheCount = 0;
		DWORD processorL3CacheCount = 0;
		DWORD processorPackageCount = 0;
		DWORD byteOffset = 0;
		PCACHE_DESCRIPTOR Cache;



		glpi = (LPFN_GLPI)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "GetLogicalProcessorInformation");
		if (!glpi)
		{
			SYSTEM_INFO systemInfo;
			GetSystemInfo(&systemInfo);
			*pdwOutPhysicalCoreCount = systemInfo.dwNumberOfProcessors;
			*pdwOutLogicalCoreCount = systemInfo.dwNumberOfProcessors;

			OutputDebugStringW(L"\nGetLogicalProcessorInformation is not supported.\n");
			goto lb_return;
		}

		BOOL done = FALSE;
		while (!done)
		{
			DWORD rc = glpi(pBuffer, &returnLength);

			if (FALSE == rc)
			{
				if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
				{
					if (pBuffer)
						free(pBuffer);

					pBuffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(returnLength);
				}
				else
				{
					break;
				}
			}
			else
			{
				done = TRUE;
			}
		}

		ptr = pBuffer;

		while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength)
		{
			switch (ptr->Relationship)
			{
				case RelationNumaNode:
					// Non-NUMA systems report a single record of this type.
					numaNodeCount++;
					break;

				case RelationProcessorCore:
					processorCoreCount++;

					// A hyperthreaded core supplies more than one logical processor.
					logicalProcessorCount += CountSetBits(ptr->ProcessorMask);
					break;

				case RelationCache:
					// Cache data is in ptr->Cache, one CACHE_DESCRIPTOR structure for each cache. 
					Cache = &ptr->Cache;
					if (Cache->Level == 1)
					{
						processorL1CacheCount++;
					}
					else if (Cache->Level == 2)
					{
						processorL2CacheCount++;
					}
					else if (Cache->Level == 3)
					{
						processorL3CacheCount++;
					}
					break;

				case RelationProcessorPackage:
					// Logical processors share a physical package.
					processorPackageCount++;
					break;

				default:
					OutputDebugStringW(L"\nError: Unsupported LOGICAL_PROCESSOR_RELATIONSHIP value.\n");
					break;
			}
			byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
			ptr++;
		}
		*pdwOutPhysicalCoreCount = processorCoreCount;
		*pdwOutLogicalCoreCount = logicalProcessorCount;
		//numaNodeCount;
		//processorPackageCount;
		//processorCoreCount;
		//logicalProcessorCount;
		//processorL1CacheCount;
		//processorL2CacheCount;
		//processorL3CacheCount

		free(pBuffer);

		bResult = TRUE;
	}
lb_return:
	return bResult;
}

UINT WINAPI ThreadFunc(void* pArg)
{
	ThreadParam* pRealArg = (ThreadParam*)pArg;
	int threadNo = pRealArg->ThreadNo;
	int totalThreadCount = pRealArg->TotalThreadCount;
	BitmapBuffer* pBitmapBuffer = pRealArg->pBitmapBuffer;
	camera* pCamera = pRealArg->pCamera;
	hittable_list* pWorld = pRealArg->pWorld;
	HANDLE* pEventList = pRealArg->hEventList;
	HANDLE* phCompleted = pRealArg->phCompleted;

	while (true)
	{
		DWORD eventIndex = WaitForMultipleObjects(THREAD_EVENT_TOTAL_COUNT, pEventList, FALSE, INFINITE);

		switch (eventIndex)
		{
			case THREAD_EVENT_COMPUTE:
			{ 
				int imageWidth = pCamera->image_width;
				int imageHeight = (int)(imageWidth / pCamera->aspect_ratio);
				imageHeight = (imageHeight < 1 ? 1 : imageHeight);

				double pixelSamplesScale = 1.0 / pCamera->samples_per_pixel;

				for (int j = threadNo; j < imageHeight; j += totalThreadCount)
				{
					for (int i = 0; i < imageWidth; ++i)
					{
						color pixelColor(0, 0, 0);
						for (int sample = 0; sample < pCamera->samples_per_pixel; ++sample)
						{
							ray r = GetRay(i, j, pCamera);
							pixelColor += RayColor(r, pCamera->max_depth, *pWorld);
						}

						{
							pixelColor = pixelSamplesScale * pixelColor;

							double r = pixelColor.x();
							double g = pixelColor.y();
							double b = pixelColor.z();

							// Apply a linear to gamma transform for gamma 2
							r = linear_to_gamma(r);
							g = linear_to_gamma(g);
							b = linear_to_gamma(b);

							// Translate the [0,1] component values to the byte range [0,255].
							static const interval intensity(0.000, 0.999);
							int rbyte = int(256 * intensity.clamp(r));
							int gbyte = int(256 * intensity.clamp(g));
							int bbyte = int(256 * intensity.clamp(b));

							// Write out the pixel color components.
							pBitmapBuffer->SetColor(i, j, RGB(rbyte, gbyte, bbyte));
						}
					}
				}

				InterlockedDecrement(&pCamera->Completed);
				{
					WCHAR szDebugString[MAX_PATH];
					swprintf_s(szDebugString, MAX_PATH, L"Thread %d work done.\n", threadNo);
					OutputDebugString(szDebugString);
				}
				if (pCamera->Completed == 0)
				{
					SetEvent(*phCompleted);
				}

				break;
			}

			case THREAD_EVENT_DESTROY:
				goto LB_RET;

			default:
				__debugbreak();
				break;
		}
	}

LB_RET:
	return 0;
}

ray GetRay(int i, int j, camera* pCamera)
{
	_ASSERT(pCamera);

	int image_height = (int)(pCamera->image_width / pCamera->aspect_ratio);
	image_height = (image_height < 1 ? 1 : image_height);

	point3 center = pCamera->lookfrom;

	double theta = degrees_to_radians(pCamera->vfov);
	double h = std::tan(theta / 2);
	double viewport_height = 2 * h * pCamera->focus_dist;
	double viewport_width = viewport_height * (double(pCamera->image_width) / image_height);

	vec3 w = unit_vector(pCamera->lookfrom - pCamera->lookat);
	vec3 u = unit_vector(cross(pCamera->vup, w));
	vec3 v = cross(w, u);

	vec3 viewport_u = viewport_width * u;
	vec3 viewport_v = viewport_height * -v;

	vec3 pixel_delta_u = viewport_u / pCamera->image_width;
	vec3 pixel_delta_v = viewport_v / image_height;

	vec3 viewportUpperLeft = center - (pCamera->focus_dist * w) - viewport_u / 2 - viewport_v / 2;
	point3 pixel00Loc = viewportUpperLeft + 0.5 * (pixel_delta_u + pixel_delta_v);

	double defocus_radius = pCamera->focus_dist * std::tan(degrees_to_radians(pCamera->defocus_angle / 2));
	vec3 defocus_disk_u = u * defocus_radius;
	vec3 defocus_disk_v = v * defocus_radius;


	// Construct a camera ray originating from the origin and directed at randomly sampled
	// point around the pixel location i, j.

	vec3 offset = SampleSquare();
	vec3 pixel_sample = pixel00Loc + ((i + offset.x()) * pixel_delta_u) + ((j + offset.y()) * pixel_delta_v);

	vec3 p = random_in_unit_disk();
	vec3 defocusDiskSample = center + (p[0] * defocus_disk_u) + (p[1] * defocus_disk_v);

	vec3 ray_origin = ((pCamera->defocus_angle <= 0) ? center : defocusDiskSample);
	vec3 ray_direction = pixel_sample - ray_origin;

	return ray(ray_origin, ray_direction);
}

color RayColor(const ray& r, int depth, const hittable& world)
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
			return attenuation * RayColor(scattered, depth - 1, world);
		}
		return color(0, 0, 0);
	}

	vec3 unit_direction = unit_vector(r.direction());
	double a = 0.5 * (unit_direction.y() + 1.0); // using alpha value using ray direction's y param. this will have [0.0, 1.0].
	return (1.0 - a) * color(1.0, 1.0, 1.0) + a * color(0.5, 0.7, 1.0);
}
