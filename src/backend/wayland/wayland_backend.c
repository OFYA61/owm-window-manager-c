#pragma clang diagnostic ignored "-Wunused-parameter"
#define _GNU_SOURCE

#include <stdbool.h>
#include <wayland-util.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-version.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <wayland-client.h>
#include <linux/input-event-codes.h>

#include "backend/backend.h"
#include "wayland_backend.h"
#include "xdg-shell-client-protocol.h"
#include "event.h"

#define WIDTH 1920;
#define HEIGHT 1080;

typedef struct {
  int width;
  int height;
  int init_width;
  int init_height;
  struct wl_surface *surface;
  struct xdg_surface *xdg_surface;
  struct xdg_toplevel *xdg_toplevel;
  struct buffer *prev_buffer;
  struct wl_callback *callback;
  bool wait_for_configure;
  bool maximized;
  bool fullscreen;
  bool needs_update_buffer;
} WaylandWindow;

typedef struct {
  struct wl_display *display;
  struct wl_registry *registry;
  struct wl_compositor *compositor;
  struct xdg_wm_base *xdg_wm_base;
  struct wl_seat *seat;
  struct wl_keyboard *keyboard;
  struct wl_pointer *pointer;
  struct wl_shm *shm;
} WaylandDisplay;

#define FB_COUNT 3
typedef struct {
  OWM_FrameBuffer frame_buffer;
  struct wl_buffer *wl_buffer;
  OWM_FrameBufferState state;
  size_t size;
} WaylandFrameBuffer;

typedef struct {
  uint64_t last_timestamp;
  uint64_t frame_time;
  WaylandFrameBuffer frame_buffers[FB_COUNT];
  int displayed_buffer_idx;
  int queued_buffer_idx;
  int next_buffer_idx;
} OWM_WaylandRenderContext;

static WaylandDisplay WAYLAND_DISPLAY = { 0 };
static WaylandWindow WAYLAND_WINDOW = { 0 };
static OWM_WaylandRenderContext OWM_WAYLAND_RENDER_CONTEXT = { 0 };

static uint32_t getWidth() {
  return WAYLAND_WINDOW.width;
}

static uint32_t getHeight() {
  return WAYLAND_WINDOW.height;
}

static void dispatch() {
  wl_display_dispatch(WAYLAND_DISPLAY.display);
}

static OWM_FrameBuffer* aquireFreeFrameBuffer() {
  if (OWM_WAYLAND_RENDER_CONTEXT.frame_buffers[OWM_WAYLAND_RENDER_CONTEXT.next_buffer_idx].state != OWM_FRAME_BUFFER_STATE_FREE) {
    return NULL;
  }

  WaylandFrameBuffer *wfb = &OWM_WAYLAND_RENDER_CONTEXT.frame_buffers[OWM_WAYLAND_RENDER_CONTEXT.next_buffer_idx];
  wfb->state = OWM_FRAME_BUFFER_STATE_QUEUED;
  OWM_WAYLAND_RENDER_CONTEXT.next_buffer_idx++;
  if (OWM_WAYLAND_RENDER_CONTEXT.next_buffer_idx >= FB_COUNT) {
    OWM_WAYLAND_RENDER_CONTEXT.next_buffer_idx = 0;
  }
  return &wfb->frame_buffer;
}

static int swapBuffers() {
  WaylandFrameBuffer *wfb = &OWM_WAYLAND_RENDER_CONTEXT.frame_buffers[OWM_WAYLAND_RENDER_CONTEXT.queued_buffer_idx];
  wl_surface_attach(WAYLAND_WINDOW.surface, wfb->wl_buffer, 0, 0);
  wl_surface_damage(WAYLAND_WINDOW.surface, 0, 0, WAYLAND_WINDOW.width, WAYLAND_WINDOW.height);
  wl_surface_commit(WAYLAND_WINDOW.surface);

  OWM_WAYLAND_RENDER_CONTEXT.queued_buffer_idx++;
  if (OWM_WAYLAND_RENDER_CONTEXT.queued_buffer_idx >= FB_COUNT) {
    OWM_WAYLAND_RENDER_CONTEXT.queued_buffer_idx = 0;
  }

  return 0;
}

static int create_shm_file(off_t size) {
  int fd = memfd_create("wayland-shm", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd < 0) return -1;
  if (ftruncate(fd, size) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

static void seat_handle_capabilities(void *data, struct wl_seat *seat, enum wl_seat_capability caps)
{
	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !WAYLAND_DISPLAY.keyboard) {
		WAYLAND_DISPLAY.keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(WAYLAND_DISPLAY.keyboard, OWM_waylandGetKeyboardListener(), NULL);
	} else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && WAYLAND_DISPLAY.keyboard) {
		wl_keyboard_destroy(WAYLAND_DISPLAY.keyboard);
		WAYLAND_DISPLAY.keyboard = NULL;
  }

  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !WAYLAND_DISPLAY.pointer) {
    WAYLAND_DISPLAY.pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(WAYLAND_DISPLAY.pointer, OWM_waylandGetPointerListener(), NULL);
  } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && WAYLAND_DISPLAY.pointer) {
    wl_pointer_destroy(WAYLAND_DISPLAY.pointer);
    WAYLAND_DISPLAY.pointer = NULL;
  }

}

static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_handle_capabilities,
};

static void registry_handle_global(
  void *data,
  struct wl_registry *registry,
  uint32_t name,
  const char *interface,
  uint32_t version
) {
  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    WAYLAND_DISPLAY.compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
  } else if (strcmp(interface, wl_shm_interface.name) == 0) {
    WAYLAND_DISPLAY.shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
  } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
    WAYLAND_DISPLAY.xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
  } else if (strcmp(interface, wl_seat_interface.name) == 0) {
    WAYLAND_DISPLAY.seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
    wl_seat_add_listener(WAYLAND_DISPLAY.seat, &seat_listener, NULL);
  }
}

static void xdg_surface_handle_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
  xdg_surface_ack_configure(xdg_surface, serial);
  if (WAYLAND_WINDOW.wait_for_configure) {
    WAYLAND_WINDOW.wait_for_configure = false;
  }
}

static void handle_xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states)
{
	uint32_t *p;

	WAYLAND_WINDOW.fullscreen = false;
	WAYLAND_WINDOW.maximized = false;

	wl_array_for_each(p, states) {
		uint32_t state = *p;
		switch (state) {
		case XDG_TOPLEVEL_STATE_FULLSCREEN:
			WAYLAND_WINDOW.fullscreen = true;
			break;
		case XDG_TOPLEVEL_STATE_MAXIMIZED:
			WAYLAND_WINDOW.maximized = true;
			break;
		}
	}

	if (width > 0 && height > 0) {
		if (!WAYLAND_WINDOW.fullscreen && !WAYLAND_WINDOW.maximized) {
			WAYLAND_WINDOW.init_width = width;
			WAYLAND_WINDOW.init_height = height;
		}
		WAYLAND_WINDOW.width = width;
		WAYLAND_WINDOW.height = height;
	} else if (!WAYLAND_WINDOW.fullscreen && !WAYLAND_WINDOW.maximized) {
		WAYLAND_WINDOW.width = WAYLAND_WINDOW.init_width;
		WAYLAND_WINDOW.height = WAYLAND_WINDOW.init_height;
	}

	WAYLAND_WINDOW.needs_update_buffer = true;
}

static void buffer_release(void *data, struct wl_buffer *buffer)
{
  WaylandFrameBuffer *wfb = data;
	wfb->state = OWM_FRAME_BUFFER_STATE_FREE;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_handle_configure,
};

static const struct wl_registry_listener registry_listener = {
  .global = registry_handle_global
};

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
  .configure = handle_xdg_toplevel_configure,
};

static const struct wl_buffer_listener buffer_listener = {
	buffer_release
};

int OWM_initWaylandBackend(OWM_Backend *out_backend) {
  WAYLAND_DISPLAY.display = wl_display_connect(NULL);
  if (!WAYLAND_DISPLAY.display) {
    fprintf(stderr, "Failed to connect to Wayland composotor\n");
    goto init_failure_display;
  }

  WAYLAND_DISPLAY.registry = wl_display_get_registry(WAYLAND_DISPLAY.display);
  if (!WAYLAND_DISPLAY.registry) {
    fprintf(stderr, "Failed to get Wayland registry\n");
    goto init_failure_registry;
  }

  wl_registry_add_listener(WAYLAND_DISPLAY.registry, &registry_listener, NULL);
  wl_display_roundtrip(WAYLAND_DISPLAY.display); // Sync with the server
	if (WAYLAND_DISPLAY.shm == NULL) {
		fprintf(stderr, "No wl_shm global\n");
    goto init_failure_shm;
	}
	wl_display_roundtrip(WAYLAND_DISPLAY.display);

  WAYLAND_WINDOW.width = WIDTH;
  WAYLAND_WINDOW.height = HEIGHT;
  WAYLAND_WINDOW.init_width = WIDTH;
  WAYLAND_WINDOW.init_height = HEIGHT;
  WAYLAND_WINDOW.surface = wl_compositor_create_surface(WAYLAND_DISPLAY.compositor);
  if (!WAYLAND_WINDOW.surface) {
    fprintf(stderr, "Failed to create wayland window surface\n");
    goto init_failure_surface;
  }
  WAYLAND_WINDOW.needs_update_buffer = false;
  if (!WAYLAND_DISPLAY.xdg_wm_base) {
    fprintf(stderr, "Failed to create xdg_wm_base\n");
    goto init_failure_xdg_wm_base;
  }

  WAYLAND_WINDOW.xdg_surface = xdg_wm_base_get_xdg_surface(WAYLAND_DISPLAY.xdg_wm_base, WAYLAND_WINDOW.surface);
  if (!WAYLAND_WINDOW.xdg_surface) {
    fprintf(stderr, "Failed to get xdg_surface\n");
    goto init_failure_xdg_surface;
  }
  xdg_surface_add_listener(WAYLAND_WINDOW.xdg_surface, &xdg_surface_listener, NULL);

  WAYLAND_WINDOW.xdg_toplevel = xdg_surface_get_toplevel(WAYLAND_WINDOW.xdg_surface);
  if (!WAYLAND_WINDOW.xdg_toplevel) {
    fprintf(stderr, "Failed to get xdg_toplevel\n");
    goto init_failure_xdg_toplevel;
  }
  xdg_toplevel_add_listener(WAYLAND_WINDOW.xdg_toplevel, &xdg_toplevel_listener, NULL);
  xdg_toplevel_set_title(WAYLAND_WINDOW.xdg_toplevel, "OWM");
  xdg_toplevel_set_app_id(WAYLAND_WINDOW.xdg_toplevel, "com.ofya.owm");
  wl_surface_commit(WAYLAND_WINDOW.surface);
  wl_display_roundtrip(WAYLAND_DISPLAY.display); // Sync with the server

  WAYLAND_WINDOW.wait_for_configure = true;

	for (int i = 0; i < FB_COUNT; i++) {
    OWM_FrameBuffer frameBuffer;
    frameBuffer.width = WAYLAND_WINDOW.width;
    frameBuffer.height = WAYLAND_WINDOW.height;
    frameBuffer.stride = WAYLAND_WINDOW.width;
    size_t stride = WAYLAND_WINDOW.width * 4;
    size_t size = stride * WAYLAND_WINDOW.height;

    int fd = create_shm_file(size);
    if (fd <= 0) {
      fprintf(stderr, "We're screwed\n");
      continue;
    }

    frameBuffer.pixels = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (!frameBuffer.pixels) {
      fprintf(stderr, "We're screwed 2\n");
      continue;
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(WAYLAND_DISPLAY.shm, fd, size);
    struct wl_buffer *wl_buffer = wl_shm_pool_create_buffer(
      pool,
      0,
      frameBuffer.width,
      frameBuffer.height,
      stride,
      WL_SHM_FORMAT_XRGB8888
    );

    WaylandFrameBuffer waylandFrameBuffer;
    waylandFrameBuffer.frame_buffer = frameBuffer;
    waylandFrameBuffer.wl_buffer = wl_buffer;
    waylandFrameBuffer.state = OWM_FRAME_BUFFER_STATE_FREE;
    waylandFrameBuffer.size = size;

    OWM_WAYLAND_RENDER_CONTEXT.frame_buffers[i] = waylandFrameBuffer;

    wl_buffer_add_listener(wl_buffer, &buffer_listener, &OWM_WAYLAND_RENDER_CONTEXT.frame_buffers[i]);

    wl_shm_pool_destroy(pool);
  }

  OWM_WAYLAND_RENDER_CONTEXT.last_timestamp = 0;
  OWM_WAYLAND_RENDER_CONTEXT.displayed_buffer_idx = 0;
  OWM_WAYLAND_RENDER_CONTEXT.queued_buffer_idx = 1;
  OWM_WAYLAND_RENDER_CONTEXT.next_buffer_idx = 2;

  out_backend->type = OWM_BACKEND_TYPE_WAYLAND;
  out_backend->getDisplayWidth = getWidth;
  out_backend->getDisplayHeight = getHeight;
  out_backend->dispatch = dispatch;
  out_backend->aquireFreeFrameBuffer = aquireFreeFrameBuffer;
  out_backend->swapBuffers = swapBuffers;
  return 0;
  xdg_toplevel_destroy(WAYLAND_WINDOW.xdg_toplevel);
init_failure_xdg_toplevel:
  xdg_surface_destroy(WAYLAND_WINDOW.xdg_surface);
init_failure_xdg_surface:
init_failure_xdg_wm_base:
  wl_surface_destroy(WAYLAND_WINDOW.surface);
init_failure_surface:
init_failure_shm:
  wl_registry_destroy(WAYLAND_DISPLAY.registry);
init_failure_registry:
  wl_display_disconnect(WAYLAND_DISPLAY.display);
init_failure_display:
  return 1;
}

void OWM_shutdownWaylandBackend() {
  for (int i = 0; i < FB_COUNT; ++i) {
    WaylandFrameBuffer *wfb = &OWM_WAYLAND_RENDER_CONTEXT.frame_buffers[i];
    munmap(wfb->frame_buffer.pixels, wfb->size);
    wl_buffer_destroy(wfb->wl_buffer);
  }

  xdg_toplevel_destroy(WAYLAND_WINDOW.xdg_toplevel);
  xdg_surface_destroy(WAYLAND_WINDOW.xdg_surface);
  wl_surface_destroy(WAYLAND_WINDOW.surface);
  wl_registry_destroy(WAYLAND_DISPLAY.registry);
  wl_display_disconnect(WAYLAND_DISPLAY.display);
}
