#include <drm.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xf86drm.h>
#include <sys/mman.h>
#include <xf86drmMode.h>
#include <drm_mode.h>

struct SystemGraphicsCards{
  char **cards;
  size_t count;
};

void SystemGraphicsCards_free(struct SystemGraphicsCards* sgc) {
  for (size_t i = 0; i < sgc->count; ++i) {
    free(sgc->cards[i]);
  }
  free(sgc->cards);
}

// On error, the `count` in the struct will be 0
struct SystemGraphicsCards SystemGraphicsCards_discover() {
  struct SystemGraphicsCards sgc = { NULL, 0 };

  DIR *d;
  struct dirent *dir;
  d = opendir("/dev/dri");
  if (!d) {
    perror("opendir");
    closedir(d);
    return sgc;
  }

  size_t i = 0;
  size_t capacity = 2; // Have 2 slots initially
  sgc.cards = malloc(capacity * sizeof(char *));
  while ((dir = readdir(d)) != NULL) {
    if (strstr(dir->d_name, "card") == NULL) {
      continue;
    }

    if (i == capacity) { // Expand buffer if no space left
      capacity++;
      char **tmp_cards = realloc(sgc.cards, capacity * sizeof(char *));
      if (tmp_cards == NULL) {
        SystemGraphicsCards_free(&sgc);
        sgc.count = 0;
        return sgc;
      }
      sgc.cards = tmp_cards;
    }

    sgc.cards[i] = (char *) malloc(sizeof(char) * strlen(dir->d_name) + 1);
    mempcpy(sgc.cards[i], dir->d_name, sizeof(char) * strlen(dir->d_name));
    i++;
    sgc.count = i;
  }
  closedir(d);
  return sgc;
}

struct DiscoveryDisplayMode {
  uint16_t h;
  uint16_t v;
  uint32_t hz;
};

struct DiscoveryDisplay {
  uint32_t connector_id;
  uint32_t encoder_id;
  uint32_t crtc_id;
  drmModeModeInfo* displayModes;
  uint32_t displayModesCount;
  char cardPath[64];
};

struct DiscoveryDisplays {
  struct DiscoveryDisplay* results;
  size_t count;
};

void DiscoveryDisplays_free(struct DiscoveryDisplays *displays) {
  for (int r = 0; r < displays->count; ++r){
    struct DiscoveryDisplay* d = &displays->results[r];
    // free(d->displayModes);
  }
  free(displays->results);
}

void DiscoveryDisplays_log(struct DiscoveryDisplays *displays) {
  for (int i = 0; i < displays->count; ++i) {
    printf("Display %d\n", i);
    printf("\tConn:%d Enc:%d Crtc:%d\n", displays->results[i].connector_id, displays->results[i].encoder_id, displays->results[i].crtc_id);
    printf("\tModes\n");
    for (int m = 0; m < displays->results[i].displayModesCount; ++m) {
      drmModeModeInfo dm = displays->results[i].displayModes[m];
      printf("\t\t%d: %dx%d @ %dHz\n", m, dm.hdisplay, dm.vdisplay, dm.vrefresh);
    }
  }
}

struct DiscoveryDisplays DiscoveryDisplays_probeDRM() {
  struct DiscoveryDisplays displays = { NULL, 0 };

  struct SystemGraphicsCards sgc = SystemGraphicsCards_discover();
  if (sgc.count == 0) {
    fprintf(stderr, "No cards where found");
    displays.count = 0;
    return displays;
  }

  int prs_size = 0;
  int prs_capacity = 1;
  displays.results = malloc(prs_capacity * sizeof(struct DiscoveryDisplay));
  char card_path[64];
  for (size_t i = 0; i < sgc.count; ++i) {
    card_path[0] = '\0';
    strcat(card_path, "/dev/dri/");
    strcat(card_path, sgc.cards[i]);

    uint32_t fd = open(card_path, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
      fprintf(stderr, "open: %s", card_path);
      continue;
    }
    drmModeRes* res = drmModeGetResources(fd);
    if (!res) {
      fprintf(stderr, "drmModeGetResources: %s\n", card_path);
      close(fd);
      continue;
    }

    for (uint32_t i = 0; i < res->count_connectors; ++i) {
      drmModeConnector* conn = drmModeGetConnector(fd, res->connectors[i]);
      if (!conn) {
        fprintf(stderr, "drmModeGetConnnector: %s\n", card_path);
        drmModeFreeConnector(conn);
        continue;
      }

      if (conn->connection != DRM_MODE_CONNECTED && conn->count_modes <= 0) { // Physical port has no connection to a display
        drmModeFreeConnector(conn);
        continue;
      }

      drmModeEncoder *enc = NULL;
      if (conn -> encoder_id) {
        enc = drmModeGetEncoder(fd, conn->encoder_id);
      } else {
        for (int j = 0; j < conn->count_encoders; ++j) {
          enc = drmModeGetEncoder(fd, conn->encoders[j]);
          if (enc) break;
        }
      }
      if (!enc) {
        fprintf(stderr, "No encoder found \n");
        drmModeFreeConnector(conn);
        continue;
      }

      printf("Using encoder %u\n", enc->encoder_id);

      uint32_t crtc_id = 0;
      for (uint32_t j = 0; j < res->count_crtcs; ++j) {
        if (enc->possible_crtcs & (1 << j)) {
          crtc_id = res->crtcs[j];
          break;
        }
      }

      if (!crtc_id) {
        fprintf(stderr, "No compatible CRTC found\n");
        goto conn_enc_cleanup;
      }

      if (prs_size == prs_capacity) { // Expand buffer if no space left
        prs_capacity++;
        struct DiscoveryDisplay * tmp_results = realloc(displays.results, prs_capacity * sizeof(struct DiscoveryDisplay));
        if (tmp_results == NULL) {
          fprintf(stderr, "Failed to expand storage for ProbeResults");
          goto conn_enc_cleanup;
        }
        displays.results = tmp_results;
      }

      struct DiscoveryDisplay display = {
        conn->connector_id,
        enc->encoder_id,
        crtc_id,
        NULL,
        conn->count_modes,
        ""
      };
      display.displayModes = malloc(conn->count_modes * sizeof(drmModeModeInfo));
      memcpy(display.displayModes, conn->modes, conn->count_modes * sizeof(drmModeModeInfo));
      strncpy(display.cardPath, card_path, strlen(card_path));
      displays.results[prs_size] = display;
      prs_size++;
      displays.count = prs_size;

    conn_enc_cleanup:
      drmModeFreeEncoder(enc);
      drmModeFreeConnector(conn);

    }

    drmModeFreeResources(res);
    close(fd);
  }

  SystemGraphicsCards_free(&sgc);

  return displays;
}

struct Display {
  drmModeModeInfo displayMode;
  uint32_t fd_card;
  uint32_t connector_id;
  uint32_t encoder_id;
  uint32_t crtc_id;
};

struct Display Display_pick() {
  struct DiscoveryDisplays discoveryDisplays = DiscoveryDisplays_probeDRM();
  if (discoveryDisplays.count == 0) {
    exit(1);
  }
  DiscoveryDisplays_log(&discoveryDisplays);

  int n_card, n_mode;

  printf("Pick the card you want to use, the number must be from 0 to %ld: ", discoveryDisplays.count - 1);
  fflush(stdin);
  scanf("%d", &n_card);
  if (n_card < 0 || n_card >= discoveryDisplays.count) {
    fprintf(stderr, "The card you chose '%d' is not valid\n", n_card);
    DiscoveryDisplays_free(&discoveryDisplays);
    exit(1);
  }

  printf("Pick the mode you want to use, the number must be from 0 to %ld: ", discoveryDisplays.results[n_card].displayModesCount - 1);
  fflush(stdin);
  scanf("%d", &n_mode);
  if (n_mode < 0 || n_mode >= discoveryDisplays.results[n_card].displayModesCount) {
    fprintf(stderr, "The card you chose '%d' is not valid\n", n_card);
    DiscoveryDisplays_free(&discoveryDisplays);
    exit(1);
  }

  printf("1\n");

  struct DiscoveryDisplay discoveryDisplay = discoveryDisplays.results[n_card];
  drmModeModeInfo displayMode = discoveryDisplay.displayModes[n_mode];
  char *card_path = discoveryDisplay.cardPath;
  uint32_t fd_card = open(card_path, O_RDWR | O_CLOEXEC);
  printf("2\n");
  if (fd_card < 0) {
    fprintf(stderr, "open: %s", card_path);
    DiscoveryDisplays_free(&discoveryDisplays);
    exit(1);
  }
  struct Display display = {
    displayMode,
    fd_card,
    discoveryDisplay.connector_id,
    discoveryDisplay.encoder_id,
    discoveryDisplay.crtc_id
  };
  printf("3\n");
  DiscoveryDisplays_free(&discoveryDisplays);
  printf("4\n");

  return display;
}

void Display_close(struct Display *display) {
  close(display->fd_card);
}

int main() {
  struct Display display = Display_pick();

  printf("Chosen display stats: %dx%d @ %dHz\n", display.displayMode.hdisplay, display.displayMode.vdisplay, display.displayMode.vrefresh);

  struct drm_mode_create_dumb create = { 0 };
  create.width = display.displayMode.hdisplay;
  create.height = display.displayMode.vdisplay;
  create.bpp = 32;

  if (drmIoctl(display.fd_card, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
    perror("drmIoctl");
    Display_close(&display);
    return 1;
  }

  uint32_t fb_id;
  if (drmModeAddFB(
    display.fd_card,
    display.displayMode.hdisplay,
    display.displayMode.vdisplay,
    24,
    32,
    create.pitch,
    create.handle,
    &fb_id)
  ) {
    perror("drmModeAddFB");
    Display_close(&display);
    return 1;
  }

  struct drm_mode_map_dumb map = { 0 };
  map.handle = create.handle;

  if (drmIoctl(display.fd_card, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
    perror("drmIoctl");
    Display_close(&display);
    return 1;
  }

  void *fb_ptr = mmap(NULL, create.size, PROT_READ | PROT_WRITE, MAP_SHARED, display.fd_card, map.offset);
  uint32_t *pixel = fb_ptr;
  for (uint32_t y = 0; y < display.displayMode.vdisplay; ++y) {
    for (uint32_t x = 0; x < display.displayMode.hdisplay; ++x) {
      pixel[x] = 0x000000FF;
    }
    pixel += create.pitch / 4;
  }

  drmModeCrtc *old_crtc = drmModeGetCrtc(display.fd_card, display.crtc_id);
  drmModeSetCrtc(display.fd_card, display.crtc_id, 0, 0, 0, NULL, 0, NULL);
  if (drmModeSetCrtc(display.fd_card, display.crtc_id, fb_id, 0, 0, &display.connector_id, 1, &display.displayMode)) {
    perror("drmModeSetCrtc");
    Display_close(&display);
    return 1;
  }

  sleep(5);

  drmModeSetCrtc(
    display.fd_card,
    old_crtc->crtc_id,
    old_crtc->buffer_id,
    old_crtc->x,
    old_crtc->y,
    &display.connector_id,
    1,
    &old_crtc->mode
  );
  munmap(fb_ptr, create.size);
  drmModeRmFB(display.fd_card, fb_id);

  struct drm_mode_destroy_dumb destory = { 0 };
  destory.handle = create.handle;
  drmIoctl(display.fd_card, DRM_IOCTL_MODE_DESTROY_DUMB, &destory);
  drmModeFreeCrtc(old_crtc);

  Display_close(&display);

  return 0;
}
