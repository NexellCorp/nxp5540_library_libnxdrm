#ifndef __LIB_NX_DRM_H__
#define __LIB_NX_DRM_H__

#ifdef __cplusplus
extern "C" {
#endif

#define NX_GEM_MAX_PLANES	3

struct nx_gem_buffer {
	uint32_t num_planes;
	uint32_t sizes[NX_GEM_MAX_PLANES];
	uint32_t strides[NX_GEM_MAX_PLANES];
	int	 gem_fds[NX_GEM_MAX_PLANES];
	int	 dma_fds[NX_GEM_MAX_PLANES];
};

int nx_drm_open_device(void);
int nx_drm_alloc_gem(int drm_fd, unsigned int size, int flags);
int nx_drm_alloc_dumb(int drm_fd, unsigned int bpp, unsigned int width,
		      unsigned int height);
int nx_drm_alloc_dumb_gem(int drm_fd, unsigned int width, unsigned int height,
			  unsigned int format, unsigned int *size);
void nx_drm_free_gem(int drm_fd, int gem);
int nx_drm_gem_to_dmafd(int drm_fd, int gem_fd);
int nx_drm_get_vaddr(int drm_fd, int gem_fd, int size, void **vaddr);
unsigned int nx_drm_get_flink_name(int drm_fd, int gem_fd);
int nx_drm_import_gem_from_flink(int drm_fd, unsigned int flink_name);
int nx_drm_alloc_nx_gem(int drm_fd, unsigned int width, unsigned int height,
			unsigned int format, struct nx_gem_buffer *buf);

#ifdef __cplusplus
}
#endif

#endif
