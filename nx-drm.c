#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <drm/drm_fourcc.h>

#include <drm/nexell_drm.h>

#include "nx-drm.h"

#define DRM_IOCTL_NR(n)         _IOC_NR(n)
#define DRM_IOC_VOID            _IOC_NONE
#define DRM_IOC_READ            _IOC_READ
#define DRM_IOC_WRITE           _IOC_WRITE
#define DRM_IOC_READWRITE       _IOC_READ|_IOC_WRITE
#define DRM_IOC(dir, group, nr, size) _IOC(dir, group, nr, size)

static int drm_ioctl(int drm_fd, unsigned long request, void *arg)
{
	int ret;

	do {
		ret = ioctl(drm_fd, request, arg);
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN));
	return ret;
}

static int drm_command_write_read(int fd, unsigned long command_index,
				  void *data, unsigned long size)
{
	unsigned long request;

	request = DRM_IOC(DRM_IOC_READ|DRM_IOC_WRITE, DRM_IOCTL_BASE,
			  DRM_COMMAND_BASE + command_index, size);
	if (drm_ioctl(fd, request, data))
		return -errno;
	return 0;
}

/**
 * return drm_fd
 */
int nx_drm_open_device(void)
{
	const char *dev = "/dev/dri/card0";
	int fd = open(dev, O_RDWR);
	if (fd < 0)
		perror("open\n");

	return fd;
}

/**
 * return gem_fd
 */
int nx_drm_alloc_gem(int drm_fd, unsigned int size, int flags)
{
	struct nx_drm_gem_create arg = { 0, };
	int ret;

	arg.size = size;
	arg.flags = flags;

	ret = drm_command_write_read(drm_fd, DRM_NX_GEM_CREATE, &arg,
				     sizeof(arg));
	if (ret) {
		perror("drm_command_write_read\n");
		return ret;
	}

	/* printf("[DRM ALLOC] gem %d, size %d, flags 0x%x\n", */
	/*        arg.handle, size, flags); */

	return arg.handle;
}

/**
 * raw alloc: alloc by width, height, bpp
 */
int nx_drm_alloc_dumb(int drm_fd, unsigned int bpp, unsigned int width,
		      unsigned int height)
{
	int ret;
	struct drm_mode_create_dumb arg;

	memset(&arg, 0, sizeof(arg));
	arg.bpp = bpp;
	arg.width = width;
	arg.height = height;

	ret = drm_ioctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &arg);
	if (ret) {
		fprintf(stderr, "failed to create dumb buffer: %s\n",
			strerror(errno));
		return -ENOMEM;
	}

	return arg.handle;
}

/**
 * format: drm/drm_fourcc.h
 * alloc by width, height, format
 */
int nx_drm_alloc_dumb_gem(int drm_fd, unsigned int width, unsigned int height,
			  unsigned int format, unsigned int *size)
{
	unsigned int virtual_height;
	unsigned int bpp;
	int ret;
	struct drm_mode_create_dumb arg;

	switch (format) {
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		bpp = 8;
		break;

	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_XRGB4444:
	case DRM_FORMAT_ABGR4444:
	case DRM_FORMAT_XBGR4444:
	case DRM_FORMAT_RGBA4444:
	case DRM_FORMAT_RGBX4444:
	case DRM_FORMAT_BGRA4444:
	case DRM_FORMAT_BGRX4444:
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_XRGB1555:
	case DRM_FORMAT_ABGR1555:
	case DRM_FORMAT_XBGR1555:
	case DRM_FORMAT_RGBA5551:
	case DRM_FORMAT_RGBX5551:
	case DRM_FORMAT_BGRA5551:
	case DRM_FORMAT_BGRX5551:
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
		bpp = 16;
		break;

	case DRM_FORMAT_BGR888:
	case DRM_FORMAT_RGB888:
		bpp = 24;
		break;

	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_ARGB2101010:
	case DRM_FORMAT_XRGB2101010:
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_XBGR2101010:
	case DRM_FORMAT_RGBA1010102:
	case DRM_FORMAT_RGBX1010102:
	case DRM_FORMAT_BGRA1010102:
	case DRM_FORMAT_BGRX1010102:
		bpp = 32;
		break;

	default:
		fprintf(stderr, "unsupported format 0x%08x\n",  format);
		return -EINVAL;
	}

	switch (format) {
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		virtual_height = height * 3 / 2;
		break;

	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
		virtual_height = height * 2;
		break;

	default:
		virtual_height = height;
		break;
	}

	memset(&arg, 0, sizeof(arg));
	arg.bpp = bpp;
	arg.width = width;
	arg.height = virtual_height;

	ret = drm_ioctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &arg);
	if (ret) {
		fprintf(stderr, "failed to create dumb buffer: %s\n",
			strerror(errno));
		return -ENOMEM;
	}

	if (size)
		*size = bpp/8 * width * virtual_height;

	return arg.handle;
}

void nx_drm_free_gem(int drm_fd, int gem)
{
	struct drm_gem_close arg = {0, };

	arg.handle = gem;
	drm_ioctl(drm_fd, DRM_IOCTL_GEM_CLOSE, &arg);
}

/**
 * return dmabuf fd
 */
int nx_drm_gem_to_dmafd(int drm_fd, int gem_fd)
{
	int ret;
	struct drm_prime_handle arg = {0, };

	arg.handle = gem_fd;
	ret = drm_ioctl(drm_fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &arg);
	if (ret) {
		perror("DRM_IOCTL_PRIM_HANDLE_TO_FD\n");
		return -1;
	}

	return arg.fd;
}

int nx_drm_get_vaddr(int drm_fd, int gem_fd, int size, void **vaddr)
{
	int ret;
	struct drm_mode_map_dumb arg = {0, };
	void *map = NULL;

	arg.handle = gem_fd;
	ret = drm_ioctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &arg);
	if (ret) {
		perror("DRM_IOCTL_MODE_MAP_DUMB\n");
		return -1;
	}

	map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, drm_fd,
		   arg.offset);
	if (map == MAP_FAILED) {
		perror("mmap\n");
		return -1;
	}

	*vaddr = map;
	return 0;
}

unsigned int nx_drm_get_flink_name(int fd, int gem)
{
	struct drm_gem_flink arg = { 0, };

	arg.handle = gem;
	if (drm_ioctl(fd, DRM_IOCTL_GEM_FLINK, &arg)) {
		fprintf(stderr,
			"fail : get flink from gem:%d (DRM_IOCTL_GEM_FLINK)\n",
			gem);
		return 0;
	}
	return (unsigned int)arg.name;
}

int nx_drm_import_gem_from_flink(int fd, unsigned int flink_name)
{
	struct drm_gem_open arg = { 0, };
	/* struct nx_drm_gem_info info = { 0, }; */

	arg.name = flink_name;
	if (drm_ioctl(fd, DRM_IOCTL_GEM_OPEN, &arg)) {
		fprintf(stderr, "fail : cannot open gem name=%d\n", flink_name);
		return -EINVAL;
	}

	return arg.handle;
}

int nx_drm_alloc_nx_gem(int drm_fd, unsigned int width, unsigned int height,
			unsigned int format, struct nx_gem_buffer *buf)
{
	int i;
	int ret;
	uint32_t bpp;
	uint32_t cbcr_stride;
	uint32_t cbcr_height;
	uint32_t cbcr_size;
	uint32_t y_stride;
	uint32_t y_size;

	switch (format) {
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
		buf->num_planes = 2;
		break;
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		buf->num_planes = 3;
		break;
	default:
		buf->num_planes = 1;
		break;
	}

	switch (format) {
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		cbcr_stride = width / 2;
		cbcr_height = height / 2;
		break;
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
		cbcr_stride = width;
		cbcr_height = height / 2;
		break;
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
		cbcr_stride = width;
		cbcr_height = height;
		break;
	}
	cbcr_size = cbcr_stride * cbcr_height;

	switch (format) {
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		bpp = 8;
		break;
	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_XRGB4444:
	case DRM_FORMAT_ABGR4444:
	case DRM_FORMAT_XBGR4444:
	case DRM_FORMAT_RGBA4444:
	case DRM_FORMAT_RGBX4444:
	case DRM_FORMAT_BGRA4444:
	case DRM_FORMAT_BGRX4444:
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_XRGB1555:
	case DRM_FORMAT_ABGR1555:
	case DRM_FORMAT_XBGR1555:
	case DRM_FORMAT_RGBA5551:
	case DRM_FORMAT_RGBX5551:
	case DRM_FORMAT_BGRA5551:
	case DRM_FORMAT_BGRX5551:
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
		bpp = 16;
		break;
	case DRM_FORMAT_BGR888:
	case DRM_FORMAT_RGB888:
		bpp = 24;
		break;
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_ARGB2101010:
	case DRM_FORMAT_XRGB2101010:
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_XBGR2101010:
	case DRM_FORMAT_RGBA1010102:
	case DRM_FORMAT_RGBX1010102:
	case DRM_FORMAT_BGRA1010102:
	case DRM_FORMAT_BGRX1010102:
		bpp = 32;
		break;
	default:
		fprintf(stderr, "Unknown format: 0x%x\n", format);
		return -EINVAL;
	}

	y_stride = bpp/8 * width;
	y_size = y_stride * height;

	for (i = 0; i < buf->num_planes; i++) {
		if (i == 0) {
			ret = nx_drm_alloc_dumb(drm_fd, bpp, width, height);
			if (ret < 0)
				return ret;

			buf->strides[i] = y_stride;
			buf->sizes[i] = y_size;
		} else {
			ret = nx_drm_alloc_dumb(drm_fd, bpp, cbcr_stride,
						cbcr_height);
			if (ret < 0)
				return ret;

			buf->strides[i] = cbcr_stride;
			buf->sizes[i] = cbcr_size;
		}

		buf->gem_fds[i] = ret;
		buf->dma_fds[i] = nx_drm_gem_to_dmafd(drm_fd, ret);
	}

	return 0;
}
