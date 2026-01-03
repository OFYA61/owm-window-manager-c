#include "evdev.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/input.h>
#include <xf86drm.h>

#include "events.h"
#include "render.h"

#define EV_BITSIZE(bits) ((bits + 7) / 8)
#define test_bit(bit, array) ((array[(bit) / 8] >> ((bit) % 8)) & 1)

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

typedef struct {
  int *fds;
  size_t count;
} OWM_EvDevKeyboards;

typedef struct {
  int *fds;
  size_t count;
} OWM_EvDevMice;

OWM_EventPollData OWM_EVENT_POLL_DATAS = { 0 };
OWM_EvDevKeyboards OWM_KEYBOARDS = { NULL, 0 };
OWM_EvDevMice OWM_MICE = { NULL, 0 };

bool isKeyboard(uint8_t *ev_bits, uint8_t *key_bits) {
  if (!test_bit(EV_KEY, ev_bits)) { // Check for key capability
    return false;
  }
  if (test_bit(BTN_LEFT, key_bits) || test_bit(BTN_RIGHT, key_bits)) { // This is a mouse
    return false;
  }
  if (!test_bit(KEY_ESC, key_bits)) {
    return false;
  }
  return true;
}

bool isMouse(uint8_t *ev_bits, uint8_t *key_bits) {
  if (!test_bit(EV_REL, ev_bits)) { // Check for key capability
    return false;
  }
  if (test_bit(BTN_LEFT, key_bits) && test_bit(BTN_RIGHT, key_bits)) {
    return true;
  }
  return false;
}

int discoverInputDevices() {
  DIR* dir = opendir("/dev/input");
  if (!dir) {
    fprintf(stderr, "Failed to open directory '/dev/input'\n");
    return 1;
  }

  struct dirent *ent;
  char path[512];

  int kbd_fds_capacity = 8;
  int *kbd_fds = malloc(sizeof(int) * kbd_fds_capacity);
  int kbd_fds_size = 0;

  int mice_fds_capacity = 8;
  int *mice_fds = malloc(sizeof(int) * mice_fds_capacity);
  int mice_fds_size = 0;

  while ((ent = readdir(dir)) != NULL) {
    if (strncmp(ent->d_name, "event", 5) != 0) {
      continue;
    }

    snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);

    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0)
      continue;

    uint8_t ev_bits[EV_BITSIZE(EV_MAX)];
    uint8_t key_bits[EV_BITSIZE(KEY_MAX)];
    memset(ev_bits, 0, sizeof(ev_bits));
    memset(key_bits, 0, sizeof(key_bits));

    if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0) {
      close(fd);
      continue;
    }
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0) {
      close(fd);
      continue;
    }
    if (isKeyboard(ev_bits, key_bits)) {
      if(kbd_fds_size == kbd_fds_capacity) {
        kbd_fds_capacity += 4;
        int *tmp_kbd_fds = realloc(kbd_fds, sizeof(int) * kbd_fds_capacity);
        if (tmp_kbd_fds == NULL) {
          fprintf(stderr, "Failed to reallocate `kbd_fds`\n");
          goto OWM_setupEvDev_failure;
        }
        kbd_fds = tmp_kbd_fds;
      }
      kbd_fds[kbd_fds_size] = fd;
      kbd_fds_size++;
      continue;
    } else if (isMouse(ev_bits, key_bits)) {
      if(mice_fds_size == mice_fds_capacity) {
        mice_fds_capacity += 4;
        int *tmp_mice_fds = realloc(mice_fds, sizeof(int) * mice_fds_capacity);
        if (tmp_mice_fds == NULL) {
          fprintf(stderr, "Failed to reallocate `mice_fds`\n");
          goto OWM_setupEvDev_failure;
        }
        mice_fds = tmp_mice_fds;
      }
      mice_fds[mice_fds_size] = fd;
      mice_fds_size++;
      continue;
    }
    close(fd);
  }

  OWM_KEYBOARDS.count = kbd_fds_size;
  OWM_KEYBOARDS.fds = malloc(sizeof(int) * kbd_fds_size);
  memcpy(OWM_KEYBOARDS.fds, kbd_fds, sizeof(int) * kbd_fds_size);
  free(kbd_fds);

  OWM_MICE.count = mice_fds_size;
  OWM_MICE.fds = malloc(sizeof(int) * mice_fds_size);
  memcpy(OWM_MICE.fds, mice_fds, sizeof(int) * mice_fds_size);
  free(mice_fds);

  closedir(dir);
  return 0;

OWM_setupEvDev_failure:
  for (int i = 0; i < kbd_fds_capacity; ++i) {
    close(kbd_fds[i]);
  }
  for (int i = 0; i < mice_fds_capacity; ++i) {
    close(mice_fds[i]);
  }
  free(kbd_fds);
  free(mice_fds);
  closedir(dir);
  return 1;
}

void cleanupInputDevices() {
  for (size_t kbd_idx = 0; kbd_idx < OWM_KEYBOARDS.count; ++kbd_idx) {
    int kbd_fd = OWM_KEYBOARDS.fds[kbd_idx];
    ioctl(kbd_fd, EVIOCGRAB, 0); // Release control of keyboard, even if we didn't grab it just in case
    close(kbd_fd);
  }
  free(OWM_KEYBOARDS.fds);

  for (size_t mice_idx = 0; mice_idx < OWM_MICE.count; ++mice_idx) {
    int mice_fd = OWM_MICE.fds[mice_idx];
    ioctl(mice_fd, EVIOCGRAB, 0);
    close(mice_fd);
  }
  free(OWM_MICE.fds);
}

int setupEPollEvents() {
  size_t size = 1 +                 // 1 for render display
                OWM_KEYBOARDS.count +  // Keyboard input devices
                OWM_MICE.count;        // Mouse input devices
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
  size_t kbds_to_process = OWM_KEYBOARDS.count;
  while (kbds_to_process > 0) {
    int kbd_fd = OWM_KEYBOARDS.fds[kbds_to_process - 1];

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
  size_t mice_to_process = OWM_MICE.count;
  while(mice_to_process > 0) {
    int mouse_fd = OWM_MICE.fds[mice_to_process - 1];

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

int OWM_setupEvDev() {
  if (discoverInputDevices()) {
    return 1;
  }

  if (setupEPollEvents()) {
    cleanupInputDevices();
    return 1;
  }

  return 0;
}

void OWM_closeEvDev() {
  cleanupInputDevices();
  free(OWM_EVENT_POLL_DATAS.device_infos);
}

#define MAX_EVENTS_TO_PROCESS 16
struct epoll_event events_to_process[MAX_EVENTS_TO_PROCESS];
void OWM_pollEvDevEvents() {
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
          OWM_submitKeyboardKeyPressCallback(ev.code, ev.value ? true : false);
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
          OWM_submitMouseKeyPressCallback(ev.code, ev.value ? true : false);
        } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) { // The mouse "packet" is complete. Dispatch total movement
          OWM_submitMouseMoveCallback(rel_x, rel_y);
          rel_x = 0;
          rel_y = 0;
        }
      }
    }
  }
}
