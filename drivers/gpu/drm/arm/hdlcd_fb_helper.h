#ifndef __DRM_FB_CMA_HELPER_H__
#define __DRM_FB_CMA_HELPER_H__

struct hdlcd_drm_fbdev;
struct drm_gem_cma_object;

struct drm_fb_helper_surface_size;
struct drm_framebuffer_funcs;
struct drm_fb_helper_funcs;
struct drm_framebuffer;
struct drm_fb_helper;
struct drm_device;
struct drm_file;
struct drm_mode_fb_cmd2;
struct drm_plane;
struct drm_plane_state;

struct hdlcd_drm_fbdev *hdlcd_drm_fbdev_init_with_funcs(struct drm_device *dev,
	unsigned int preferred_bpp, unsigned int max_conn_count,
	const struct drm_framebuffer_funcs *funcs);
struct hdlcd_drm_fbdev *hdlcd_drm_fbdev_init(struct drm_device *dev,
	unsigned int preferred_bpp, unsigned int max_conn_count);
void hdlcd_drm_fbdev_fini(struct hdlcd_drm_fbdev *hdlcd_fbdev);

void hdlcd_drm_fbdev_restore_mode(struct hdlcd_drm_fbdev *hdlcd_fbdev);
void hdlcd_drm_fbdev_hotplug_event(struct hdlcd_drm_fbdev *hdlcd_fbdev);
void hdlcd_drm_fbdev_set_suspend(struct hdlcd_drm_fbdev *hdlcd_fbdev, int state);
void hdlcd_drm_fbdev_set_suspend_unlocked(struct hdlcd_drm_fbdev *hdlcd_fbdev,
					int state);

void hdlcd_fb_destroy(struct drm_framebuffer *fb);
int hdlcd_fb_create_handle(struct drm_framebuffer *fb,
	struct drm_file *file_priv, unsigned int *handle);

struct drm_framebuffer *hdlcd_fb_create_with_funcs(struct drm_device *dev,
	struct drm_file *file_priv, const struct drm_mode_fb_cmd2 *mode_cmd,
	const struct drm_framebuffer_funcs *funcs);
struct drm_framebuffer *hdlcd_fb_create(struct drm_device *dev,
	struct drm_file *file_priv, const struct drm_mode_fb_cmd2 *mode_cmd);

struct drm_gem_cma_object *hdlcd_fb_get_gem_obj(struct drm_framebuffer *fb,
	unsigned int plane);

int hdlcd_fb_prepare_fb(struct drm_plane *plane,
			  struct drm_plane_state *state);

#ifdef CONFIG_DEBUG_FS
struct seq_file;

int hdlcd_fb_debugfs_show(struct seq_file *m, void *arg);
#endif

#endif

