#define _GNU_SOURCE

#include "backend/backend.h"
#include "wayland_backend.h"
#include "xdg-shell-client-protocol.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <wayland-client.h>

const int WIDTH = 1280;
const int HEIGHT = 720;

struct wl_display *display = NULL;
struct wl_registry *registry = NULL;
struct wl_surface *surface = NULL;
struct xdg_surface *xdg_surface = NULL;
struct xdg_toplevel *xdg_toplevel = NULL;
int stride = 0;
int size = 0;
int fd = 0;
uint32_t *data;
struct wl_shm_pool *pool;
struct wl_buffer *buffer;

struct wl_compositor *compositor = NULL;
struct wl_shm *shm = NULL;
struct xdg_wm_base *xdg_wm_base = NULL;

uint32_t getWidth() {
  return WIDTH;
}

uint32_t getHeight() {
  return HEIGHT;
}

void dispatch() {
  wl_display_dispatch(display);
}

OWM_FrameBuffer frame_buffer = { 0 };

OWM_FrameBuffer* aquireFreeFrameBuffer() {
  return &frame_buffer;
}

int swapBuffers() {
  wl_surface_attach(surface, buffer, 0, 0);
  wl_surface_damage(surface, 0, 0, WIDTH, HEIGHT);
  wl_surface_commit(surface);
  return 0;
}

static int create_shm_file(off_t size) {
  int fd = memfd_create("wayland-shm", MFD_CLOEXEC);
  if (fd < 0) return -1;
  if (ftruncate(fd, size) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

// Registry listener to find globals
static void registry_handle_global(
  void *data,
  struct wl_registry *registry,
  uint32_t name,
  const char *interface,
  uint32_t version
) {
  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
  } else if (strcmp(interface, wl_shm_interface.name) == 0) {
    shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
  } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
    xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
  }
}

static void xdg_surface_handle_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    // 1. Acknowledge the configuration
    xdg_surface_ack_configure(xdg_surface, serial);

    // 2. Now it is safe to draw! 
    // Usually, you'd set a flag here or call your redraw function.
    printf("Surface configured. Ready to draw.\n");
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_handle_configure,
};

static const struct wl_registry_listener registry_listener = { .global = registry_handle_global };

int OWM_initWaylandBackend(OWM_Backend *out_backend) {
  display = wl_display_connect(NULL);
  if (!display) {
    fprintf(stderr, "Failed to connect to Wayland composotor\n");
    goto init_failure_display;
  }

  registry = wl_display_get_registry(display);
  if (!registry) {
    fprintf(stderr, "Failed to get Wayland registry\n");
    goto init_failure_registry;
  }
  wl_registry_add_listener(registry, &registry_listener, NULL);
  wl_display_roundtrip(display); // Sync with server

  // Setup surface
  surface = wl_compositor_create_surface(compositor);
  if (!surface) {
    fprintf(stderr, "Failed to create Wayland surface\n");
    goto init_failure_surface;
  }

  // Setup XDG Shell
  xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
  if (!xdg_surface) {
    fprintf(stderr, "Failed to create XDG surface\n");
    goto init_failure_xdg_surface;
  }
  xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
  xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);
  if (!xdg_toplevel) {
    fprintf(stderr, "Failed to get XDG top level\n");
    goto init_failure_xdg_toplevel;
  }
  wl_surface_commit(surface);
  wl_display_roundtrip(display);

  // Create Framebuffer (Shared Memory)
  stride = WIDTH * 4;
  size = stride * HEIGHT;
  fd = create_shm_file(size);
  if (fd <= 0) {
    fprintf(stderr, "Failed to create frame buffer file\n");
    goto init_failure_frame_buffer_file;
  }
  data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (!data) {
    fprintf(stderr, "Failed to allocate memory for frame buffer\n");
    goto init_failure_frame_buffer_memory;
  }

  for (int i = 0; i < WIDTH * HEIGHT; i++) data[i] = 0x0000FF00;

  pool = wl_shm_create_pool(shm, fd, size);
  if (!pool) {
    fprintf(stderr, "Failed to create pool\n");
    goto init_failure_pool_create;
  }
  buffer = wl_shm_pool_create_buffer(pool, 0, WIDTH, HEIGHT, stride, WL_SHM_FORMAT_XRGB8888);
  if (!buffer) {
    fprintf(stderr, "Failed to create Wayland buffer\n");
    goto init_failure_wayland_buffer_create;
  }

  // 4. Attach and Map
  frame_buffer.pixels = data;
  frame_buffer.width = WIDTH;
  frame_buffer.height = HEIGHT;
  frame_buffer.stride = WIDTH;
  wl_surface_attach(surface, buffer, 0, 0);
  wl_surface_damage(surface, 0, 0, WIDTH, HEIGHT);
  wl_surface_commit(surface);

  // while (wl_display_dispatch(display) != -1) {
  //   // Main Loop
  // }

  out_backend->getDisplayWidth = getWidth;
  out_backend->getDisplayHeight = getHeight;
  out_backend->dispatch = dispatch;
  out_backend->aquireFreeFrameBuffer = aquireFreeFrameBuffer;
  out_backend->swapBuffers = swapBuffers;

  return 0;
  wl_buffer_destroy(buffer);
init_failure_wayland_buffer_create:
  wl_shm_pool_destroy(pool);
init_failure_pool_create:
  munmap(data, size);
init_failure_frame_buffer_memory:
  close(fd);
init_failure_frame_buffer_file:
  xdg_toplevel_destroy(xdg_toplevel);
init_failure_xdg_toplevel:
  xdg_surface_destroy(xdg_surface);
init_failure_xdg_surface:
  wl_surface_destroy(surface);
init_failure_surface:
  wl_registry_destroy(registry);
init_failure_registry:
  wl_display_disconnect(display);
init_failure_display:
  return 1;
}

void OWM_shutdownWaylandBackend() {
  wl_buffer_destroy(buffer);
  wl_shm_pool_destroy(pool);
  munmap(data, size);
  close(fd);
  xdg_toplevel_destroy(xdg_toplevel);
  xdg_surface_destroy(xdg_surface);
  wl_surface_destroy(surface);
  wl_registry_destroy(registry);
  wl_display_disconnect(display);
}
