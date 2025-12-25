#include "input.h"

#include <dirent.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/kd.h>

#define EV_BITSIZE(bits) ((bits + 7) / 8)
#define test_bit(bit, array) ((array[(bit) / 8] >> ((bit) % 8)) & 1)

int handle_keybaord(int kbd_fd) {
  struct input_event ev;

  while (read(kbd_fd, &ev, sizeof(ev)) == sizeof(ev)) {
    if (ev.type == EV_KEY && ev.value == 1) {
      printf("key %d %s\n",
             ev.code,
             ev.value ? "pressed" : "released");
      if (ev.code == KEY_ESC) {
        printf("ESC Key pressed\n");
        return 1;
      }
    }
  }
  return 0;
}

int is_keyboard(int fd) {
  unsigned long evbit[(EV_MAX + 7) / 8] = { 0 }; // Space to fit in EV_MAX bits
  unsigned long keybit[(KEY_MAX + 7) / 8] = { 0 }; // Space to fit in KEY_MAX bits

  if (ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit) < 0) {
    return 0;
  }

  if (!(evbit[EV_KEY / 8] & (1 << (EV_KEY % 8)))) { // EV_KEY / 8 = which byte the bit is located in
                                                    // EV_KEY % 8 = which bit in that byte the EV_KEY bit is in
    return 0;
  }

  if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0) {
    return 0;
  }

  return (keybit[KEY_ESC / 8] & (1 << (KEY_ESC % 8))) != 0;
}

int open_keyboard_device() {
  char *folder_path = "/dev/input";
  DIR *dir = opendir(folder_path);
  if (!dir) {
    fprintf(stderr, "Failed to open directory '%s'\n", folder_path);
    return -1;
  }

  struct dirent *ent;
  char path[256];

  while ((ent = readdir(dir)) != NULL) {
    if (strncmp(ent->d_name, "event", 5) != 0)
      continue;

    snprintf(path, sizeof(path), "/dev/input/event6", ent->d_name);

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

    printf("Using keyboard device: %s\n", path);
    closedir(dir);
    return fd;
  }

  closedir(dir);
  return -1;
}

void release_keyboard(int kbd_fd) {
  ioctl(kbd_fd, EVIOCGRAB, 0);
}
