#pragma clang diagnostic ignored "-Wunused-parameter"

#include "event.h"

#include <unistd.h>
#include <wayland-client-protocol.h>
#include <wayland-util.h>

#include "core/event.h"

static int last_mouse_x = 0;
static int last_mouse_y = 0;

static void pointer_handle_enter(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y) {
  last_mouse_x = (int) wl_fixed_to_double(surface_x);
  last_mouse_y = (int) wl_fixed_to_double(surface_y);
  OWM_submitMouseSetPositionCallback(last_mouse_x, last_mouse_y);
}

static void pointer_handle_leave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface) {
}

static void pointer_handle_motion(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
  int new_x = (int) wl_fixed_to_double(x);
  int new_y = (int) wl_fixed_to_double(y);
  OWM_submitMouseMoveCallback(new_x - last_mouse_x, new_y - last_mouse_y);
  last_mouse_x = new_x;
  last_mouse_y = new_y;
}

static void pointer_handle_button(void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
  OWM_submitMouseKeyPressCallback((OWM_KeyCode) button, state ? true : false);
}

static void pointer_handle_axis(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, int32_t value) {
}

static struct wl_pointer_listener pointer_listener = {
  pointer_handle_enter,
  pointer_handle_leave,
  pointer_handle_motion,
  pointer_handle_button,
  pointer_handle_axis,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

static void keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format, int fd, uint32_t size)
{
	close(fd);
}

static void keyboard_handle_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
}

static void keyboard_handle_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface)
{
}

static void keyboard_handle_key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
  OWM_submitKeyboardKeyPressCallback((OWM_KeyCode) key, state ? true : false);
}

static void keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
}

static struct wl_keyboard_listener keyboard_listener = {
	keyboard_handle_keymap,
	keyboard_handle_enter,
	keyboard_handle_leave,
  keyboard_handle_key,
	keyboard_handle_modifiers,
  NULL
};

struct wl_pointer_listener* OWM_waylandGetPointerListener() {
  return &pointer_listener;
}

struct wl_keyboard_listener* OWM_waylandGetKeyboardListener() {
  return &keyboard_listener;
}

