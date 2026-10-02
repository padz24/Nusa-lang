/* Shim OpenCL query-only untuk FFI Nusa (paket referensi ekosistem).
 *
 * `extern fn` Nusa hanya mengenal string/i64/bool — maka shim ini
 * hanya mengembalikan ANGKA (jumlah + bitmask tipe); NAMA device
 * (string lintas batas) = follow-up (butuh buffer/out-param =
 * butuh pointer, di luar FFI hari ini).
 *
 * OpenCL di-load via dlopen (TANPA dependensi link-time ke vendor
 * lib): tak ada lib = 0/-1 bersih, bukan gagal link. Link: -ldl.
 * Query murni (tanpa context/queue/buffer/kernel = follow-up).
 */
#include <stdint.h>
#include <dlfcn.h>
#include <stddef.h>

typedef int32_t cl_int;
typedef uint32_t cl_uint;
typedef void *cl_platform_id;
typedef void *cl_device_id;
typedef uint64_t cl_device_type;

static void *ocl_h = 0;
static cl_int (*oclGetPlatforms)(cl_uint, cl_platform_id *, cl_uint *) = 0;
static cl_int (*oclGetDevices)(cl_platform_id, cl_device_type, cl_uint,
                               cl_device_id *, cl_uint *) = 0;
static cl_int (*oclGetDevInfo)(cl_device_id, cl_uint, size_t, void *,
                               size_t *) = 0;

static int ocl_ensure(void) {
  if (ocl_h)
    return 1;
  ocl_h = dlopen("/system/vendor/lib64/libOpenCL.so", RTLD_NOW | RTLD_LOCAL);
  if (!ocl_h)
    ocl_h = dlopen("libOpenCL.so", RTLD_NOW | RTLD_LOCAL);
  if (!ocl_h)
    return 0;
  oclGetPlatforms = (void *)dlsym(ocl_h, "clGetPlatformIDs");
  oclGetDevices = (void *)dlsym(ocl_h, "clGetDeviceIDs");
  oclGetDevInfo = (void *)dlsym(ocl_h, "clGetDeviceInfo");
  return oclGetPlatforms && oclGetDevices && oclGetDevInfo;
}

/* Jumlah platform; 0 = tak tersedia (bukan error). */
int64_t ocl_platforms(void) {
  cl_uint n = 0;
  if (!ocl_ensure())
    return 0;
  if (oclGetPlatforms(0, 0, &n) != 0)
    return 0;
  return (int64_t)n;
}

static cl_device_id ocl_first_device(void) {
  cl_platform_id p = 0;
  cl_uint np = 0;
  static cl_device_id dev = 0;
  static int done = 0;
  if (done)
    return dev;
  done = 1;
  if (!ocl_ensure())
    return 0;
  if (oclGetPlatforms(1, &p, &np) != 0 || np == 0)
    return 0;
  if (oclGetDevices(p, (cl_device_type)0xFFFFFFFF, 1, &dev, 0) != 0)
    dev = 0;
  return dev;
}

/* Jumlah device (semua tipe) platform pertama; -1 = gagal. */
int64_t ocl_devices(void) {
  cl_platform_id p = 0;
  cl_uint np = 0;
  cl_uint nd = 0;
  if (!ocl_ensure())
    return -1;
  if (oclGetPlatforms(1, &p, &np) != 0 || np == 0)
    return -1;
  if (oclGetDevices(p, (cl_device_type)0xFFFFFFFF, 0, 0, &nd) != 0)
    return -1;
  return (int64_t)nd;
}

/* Bitmask CL_DEVICE_TYPE device pertama (GPU=4); 0 = tak ada. */
int64_t ocl_device_class(void) {
  cl_device_id dev = ocl_first_device();
  cl_device_type ty = 0;
  if (!dev)
    return 0;
  if (oclGetDevInfo(dev, 0x1000, sizeof(ty), &ty, 0) != 0)
    return 0;
  return (int64_t)ty;
}
