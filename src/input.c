#include "input.h"

#include <dirent.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/kd.h>

#define EV_BITSIZE(bits) ((bits + 7) / 8)
#define test_bit(bit, array) ((array[(bit) / 8] >> ((bit) % 8)) & 1)

owmKeyboards OWM_KEYBOARDS = { NULL, 0 };
owmMice OWM_MICE = { NULL, 0 };
void (*key_press_callback)(uint16_t key_code, bool pressed) = NULL;

bool is_keyboard(uint8_t *ev_bits, uint8_t *key_bits) {
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

bool is_mouse(uint8_t *ev_bits, uint8_t *key_bits) {
  if (!test_bit(EV_REL, ev_bits)) { // Check for key capability
    return false;
  }
  if (test_bit(BTN_LEFT, key_bits) && test_bit(BTN_RIGHT, key_bits)) {
    return true;
  }
  return false;
}

int owmInput_setup() {
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
    if (is_keyboard(ev_bits, key_bits)) {
      if(kbd_fds_size == kbd_fds_capacity) {
        kbd_fds_capacity += 4;
        int *tmp_kbd_fds = realloc(kbd_fds, sizeof(int) * kbd_fds_capacity);
        if (tmp_kbd_fds == NULL) {
          fprintf(stderr, "Failed to reallocate `kbd_fds`\n");
          return 1;
        }
        kbd_fds = tmp_kbd_fds;
      }
      kbd_fds[kbd_fds_size] = fd;
      kbd_fds_size++;
      continue;
    } else if (is_mouse(ev_bits, key_bits)) {
      if(mice_fds_size == mice_fds_capacity) {
        mice_fds_capacity += 4;
        int *tmp_mice_fds = realloc(mice_fds, sizeof(int) * mice_fds_capacity);
        if (tmp_mice_fds == NULL) {
          fprintf(stderr, "Failed to reallocate `mice_fds`\n");
          return 1;
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
}

void owmInput_close() {
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

void owmKeyboards_set_key_press_callback(void (*callback)(uint16_t key_code, bool pressed)) {
  key_press_callback = callback;
}

void owmKeyboard_handle_poll_event(int kbd_fd) {
  struct input_event ev;

  while (read(kbd_fd, &ev, sizeof(ev)) == sizeof(ev)) {
    if (ev.type == EV_KEY && ev.value == 1) {
      if (key_press_callback != NULL) {
         key_press_callback(ev.code, ev.value ? true : false);
      }
    }
  }
}

void owmMice_handle_poll_event(int mouse_fd) {
  struct input_event ev;

  static int rel_x = 0;
  static int rel_y = 0;
  while (read(mouse_fd, &ev, sizeof(ev)) == sizeof(ev)) {
    if (ev.type == EV_REL) {
      if (ev.code == REL_X) {
        rel_x += ev.value;
      } else if (ev.code == REL_Y) {
        rel_y += ev.value;
      } else if (ev.code == REL_WHEEL) {
        // TODO: handle scroll wheel
      }
    } else if (ev.type == EV_KEY) {
      // TODO: handle mouse button callback
      printf("Mouse button %s %d\n", ev.value ? "PRESSED" : "RELEASED", ev.code);
    } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
      // The mouse "packet" is complete. Dispatch total movement
      // TODO: mouse movement callback
      printf("%d %d\n", rel_x, rel_y);
      rel_x = 0;
      rel_y = 0;
    }
  }
}

inline const owmKeyboards* owmKeyboards_get() {
  return &OWM_KEYBOARDS;
}

inline const owmMice* owmMice_get() {
  return &OWM_MICE;
}
