#include "events.h"

#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <xf86drm.h>

#include "input.h"
#include "render.h"

#define BITSIZE(bits) ((bits + 7) / 8)

// Stores if the key is pressed or not, we need to keep track of this in order to differentiate between
// an initial press and a repeat press
uint8_t KEY_STATUS[BITSIZE(KEY_MAX)] = { 0 };

#define IS_KEY_PRESSED(idx) (KEY_STATUS[(idx)/8] & (1 << ((idx) % 8)))
#define CLEAR_KEY_PRESS(idx) (KEY_STATUS[(idx)/8] &= ~(1 << ((idx) % 8)))
#define MARK_KEY_PRESSED(idx) (KEY_STATUS[(idx)/8] |= (1 << ((idx) % 8)))

typedef struct {
  int fd;
  enum {
    OWM_INPUT_DEVICE_TYPE_KBD,
    OWM_INPUT_DEVICE_TYPE_MOUSE,
    OWM_INPUT_DEVICE_TYPE_DRM
  } type;
} OWM_InputDeviceInfo;

typedef struct {
  int epoll_instance_fd;
  size_t count;
  OWM_InputDeviceInfo *device_infos;
} OWM_EventPollData;

OWM_EventPollData OWM_EVENT_POLL_DATAS = { 0 };

int OWM_setupEvents() {
  const OWM_EvDevKeyboards *keyboards = OWM_getEvDevKeyboards();
  const OWM_EvDevMice *mice = OWM_getEvDevMice();
  size_t size = 1 +                 // 1 for render display
                keyboards->count +  // Keyboard input devices
                mice->count;        // Mouse input devices
  int epoll_instance_fd = epoll_create1(0);
  struct epoll_event ev;
  
  OWM_EVENT_POLL_DATAS.epoll_instance_fd = epoll_instance_fd;
  OWM_EVENT_POLL_DATAS.device_infos = malloc(sizeof(OWM_InputDeviceInfo) * size);
  OWM_EVENT_POLL_DATAS.count = size;

  // Register render display
  size_t index = 0;
  int drm_fd = OWM_drmGetCardFileDescritor();
  OWM_EVENT_POLL_DATAS.device_infos[index].fd = drm_fd;
  OWM_EVENT_POLL_DATAS.device_infos[index].type = OWM_INPUT_DEVICE_TYPE_DRM;
  ev.data.fd = drm_fd;
  ev.data.ptr = &OWM_EVENT_POLL_DATAS.device_infos[index];
  ev.events = EPOLLIN;
  if (epoll_ctl(epoll_instance_fd, EPOLL_CTL_ADD, drm_fd, &ev)) {
    perror("epoll_ctl: register DMR");
    goto OWM_events_setup_failure;
  }
  index++;

  // Register keyboards
  size_t kbds_to_process = keyboards->count;
  while (kbds_to_process > 0) {
    int kbd_fd = keyboards->fds[kbds_to_process - 1];

    OWM_EVENT_POLL_DATAS.device_infos[index].fd = kbd_fd;
    OWM_EVENT_POLL_DATAS.device_infos[index].type = OWM_INPUT_DEVICE_TYPE_KBD;
    ev.data.fd = kbd_fd;
    ev.data.ptr = &OWM_EVENT_POLL_DATAS.device_infos[index];
    ev.events = EPOLLIN;
    if (epoll_ctl(epoll_instance_fd, EPOLL_CTL_ADD, kbd_fd, &ev)) {
      perror("epoll_ctl: register DMR");
      goto OWM_events_setup_failure;
    }

    index++;
    --kbds_to_process;
  }

  // Register mice
  size_t mice_to_process = mice->count;
  while(mice_to_process > 0) {
    int mouse_fd = mice->fds[mice_to_process - 1];

    OWM_EVENT_POLL_DATAS.device_infos[index].fd = mouse_fd;
    OWM_EVENT_POLL_DATAS.device_infos[index].type = OWM_INPUT_DEVICE_TYPE_MOUSE;
    ev.data.fd = mouse_fd;
    ev.data.ptr = &OWM_EVENT_POLL_DATAS.device_infos[index];
    ev.events = EPOLLIN;
    if (epoll_ctl(epoll_instance_fd, EPOLL_CTL_ADD, mouse_fd, &ev)) {
      perror("epoll_ctl: register DMR");
      goto OWM_events_setup_failure;
    }

    index++;
    --mice_to_process;
  }
  return 0;

OWM_events_setup_failure:
  free(OWM_EVENT_POLL_DATAS.device_infos);
  return 1;
}

void (*owm_keyboard_key_press_callback)(uint16_t key_code, OWM_KeyEventType event_type) = NULL;
void (*owm_mouse_key_press_callback)(uint16_t key_code, OWM_KeyEventType event_type) = NULL;
void (*owm_mouse_move_callback)(int rel_x, int rel_y) = NULL;

void OWM_setKeyboardKeyPressCallback(void (*callback)(uint16_t key_code, OWM_KeyEventType event_type)) {
  owm_keyboard_key_press_callback = callback;
}

void OWM_setMouseKeyPressCallback(void (*callback)(uint16_t key_code, OWM_KeyEventType event_type)) {
  owm_mouse_key_press_callback = callback;
}

void OWM_setMouseMoveCallback(void (*callback)(int rel_x, int rel_y)) {
  owm_mouse_move_callback = callback;
}

OWM_KeyEventType OWM_getKeyEventType(uint16_t key_code, bool pressed) {
  if (!pressed) {
    CLEAR_KEY_PRESS(key_code);
    return OWM_EVENT_KEY_EVENT_RELEASE;
  }
  if (IS_KEY_PRESSED(key_code)) {
    return OWM_EVENT_KEY_EVENT_PRESS_REPEATE;
  } else {
    MARK_KEY_PRESSED(key_code);
    return OWM_EVENT_KEY_EVENT_PRESS;
  }
}

#define MAX_EVENTS_TO_PROCESS 16
struct epoll_event events_to_process[MAX_EVENTS_TO_PROCESS];
void OWM_pollEvents() {
  int timeout = OWM_drmIsNextBufferFree() ? 10 : -1;

  int num_ready = epoll_wait(
    OWM_EVENT_POLL_DATAS.epoll_instance_fd,
    events_to_process,
    MAX_EVENTS_TO_PROCESS,
    timeout
  );

  for (int i = 0; i < num_ready; ++i) {
    OWM_InputDeviceInfo *device_info = events_to_process[i].data.ptr;
    int fd = device_info->fd;
    int type = device_info->type;

    if (type == OWM_INPUT_DEVICE_TYPE_DRM) {
      drmEventContext ev = {
        .version = DRM_EVENT_CONTEXT_VERSION,
        .page_flip_handler = OWM_drmFlipRenderContextHandler
      };
      drmHandleEvent(fd, &ev);
    }

    if (type == OWM_INPUT_DEVICE_TYPE_KBD) {
      struct input_event ev;
      while (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
        if (ev.type == EV_KEY) {
          OWM_KeyEventType event_type = OWM_getKeyEventType(ev.code, ev.value ? true : false);
          if (owm_keyboard_key_press_callback != NULL) {
            owm_keyboard_key_press_callback(ev.code, event_type);
          }
        }
      }
    }

    if (type == OWM_INPUT_DEVICE_TYPE_MOUSE) {
      struct input_event ev;
      static int rel_x = 0;
      static int rel_y = 0;
      while (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
        if (ev.type == EV_REL) {
          if (ev.code == REL_X) {
            rel_x += ev.value;
          } else if (ev.code == REL_Y) {
            rel_y += ev.value;
          } else if (ev.code == REL_WHEEL) {
            // TODO: handle scroll wheel
          }
        } else if (ev.type == EV_KEY) {
          OWM_KeyEventType event_type = OWM_getKeyEventType(ev.code, ev.value ? true : false);
          if (owm_mouse_key_press_callback != NULL) {
            owm_mouse_key_press_callback(ev.code, event_type);
          }
        } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) { // The mouse "packet" is complete. Dispatch total movement
          if (owm_mouse_move_callback != NULL) {
            owm_mouse_move_callback(rel_x, rel_y);
          }
          rel_x = 0;
          rel_y = 0;
        }
      }
    }
  }
}

void OWM_cleanupEvents() {
  free(OWM_EVENT_POLL_DATAS.device_infos);
}
