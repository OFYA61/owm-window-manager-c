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

OfyaKeyboards KEYBOARDS = { NULL, 0 };
void (*key_press_callback)(uint16_t key_code, bool pressed) = NULL;

int OfyaKeyboards_setup() {
  DIR* dir = opendir("/dev/input");
  if (!dir) {
    fprintf(stderr, "Failed to open directory '/dev/input'\n");
    return -1;
  }

  struct dirent *ent;
  char path[512];
  int kbd_fds_capacity = 8;
  int *kbd_fds = malloc(sizeof(int) * 8);
  int kbd_fds_size = 0;
  while ((ent = readdir(dir)) != NULL) {
    if (strncmp(ent->d_name, "event", 5) != 0) {
      continue;
    }

    snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);

    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0)
      continue;

    // Check EV_KEY capability
    unsigned long ev_bits[EV_BITSIZE(EV_MAX)];
    memset(ev_bits, 0, sizeof(ev_bits));
    if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0) {
      close(fd);
      continue;
    }
    if (!test_bit(EV_KEY, ev_bits)) {
      close(fd);
      continue;
    }

    // Check if device has KEY_ESC
    unsigned long key_bits[EV_BITSIZE(KEY_MAX)];
    memset(key_bits, 0, sizeof(key_bits));
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0) {
      close(fd);
      continue;
    }
    if (!test_bit(KEY_ESC, key_bits)) {
      close(fd);
      continue;
    }

    // Optional: grab device to prevent echo to TTY
    // if (ioctl(fd, EVIOCGRAB, 1) < 0) {
    //   perror("EVIOCGRAB");
    //   // Not fatal, we can still use it
    // }

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
  }

  KEYBOARDS.count = kbd_fds_size;
  KEYBOARDS.kbd_fds = malloc(sizeof(int) * kbd_fds_size);
  memcpy(KEYBOARDS.kbd_fds, kbd_fds, sizeof(int) * kbd_fds_size);
  free(kbd_fds);

  closedir(dir);
  return 0;
}

void OfyaKeyboards_close() {
  for (size_t kbd_idx = 0; kbd_idx < KEYBOARDS.count; ++kbd_idx) {
    int kbd_fd = KEYBOARDS.kbd_fds[kbd_idx];
    ioctl(kbd_fd, EVIOCGRAB, 0); // Release control of keyboard, even if we didn't grab it just in case
    close(kbd_fd);
  }
  free(KEYBOARDS.kbd_fds);
}

void OfyaKeyboards_set_key_press_callback(void (*callback)(uint16_t key_code, bool pressed)) {
  key_press_callback = callback;
}

void OfyaKeyboard_handle_poll_event(int kbd_fd) {
  struct input_event ev;

  while (read(kbd_fd, &ev, sizeof(ev)) == sizeof(ev)) {
    if (ev.type == EV_KEY && ev.value == 1) {
      if (key_press_callback != NULL) {
         key_press_callback(ev.code, ev.value ? true : false);
      }
    }
  }
}
