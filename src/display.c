#include "display.h"
#include "drm.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

struct SystemGraphicsCards{
  char **cards;
  size_t count;
};

struct DiscoveryDisplay {
  uint32_t connector_id;
  uint32_t encoder_id;
  uint32_t crtc_id;
  uint32_t crtc_index;
  drmModeModeInfo* displayModes;
  uint32_t displayModesCount;
  char cardPath[64];
};

struct DiscoveryDisplays {
  struct DiscoveryDisplay* results;
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

void DiscoveryDisplays_free(struct DiscoveryDisplays *displays) {
  for (size_t r = 0; r < displays->count; ++r){
    struct DiscoveryDisplay* d = &displays->results[r];
    free(d->displayModes);
  }
  free(displays->results);
}

void DiscoveryDisplays_log(struct DiscoveryDisplays *displays) {
  for (size_t i = 0; i < displays->count; ++i) {
    printf("Display %ld\n", i);
    printf("\tConn:%d Enc:%d Crtc:%d\n", displays->results[i].connector_id, displays->results[i].encoder_id, displays->results[i].crtc_id);
    printf("\tModes\n");
    for (size_t m = 0; m < displays->results[i].displayModesCount; ++m) {
      drmModeModeInfo dm = displays->results[i].displayModes[m];
      printf("\t\t%ld: %dx%d @ %dHz\n", m, dm.hdisplay, dm.vdisplay, dm.vrefresh);
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
    if (fd <= 0) {
      fprintf(stderr, "open %s: %s", card_path, strerror(errno));
      continue;
    }
    drmModeRes* res = drmModeGetResources(fd);
    if (!res) {
      fprintf(stderr, "drmModeGetResources %s: %s\n", card_path, strerror(errno));
      close(fd);
      continue;
    }

    for (int i = 0; i < res->count_connectors; ++i) {
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
      uint32_t crtc_index = 0;
      for (int j = 0; j < res->count_crtcs; ++j) {
        if (enc->possible_crtcs & (1 << j)) {
          crtc_id = res->crtcs[j];
          crtc_index = j;
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
        crtc_index,
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

struct Display Display_pick() {
  struct DiscoveryDisplays discoveryDisplays = DiscoveryDisplays_probeDRM();
  if (discoveryDisplays.count == 0) {
    exit(1);
  }
  DiscoveryDisplays_log(&discoveryDisplays);

  size_t n_card, n_mode;

  printf("Pick the card you want to use, the number must be from 0 to %ld: ", discoveryDisplays.count - 1);
  fflush(stdin);
  scanf("%ld", &n_card);
  if (n_card >= discoveryDisplays.count) {
    fprintf(stderr, "The card you chose '%ld' is not valid\n", n_card);
    DiscoveryDisplays_free(&discoveryDisplays);
    exit(1);
  }

  printf("Pick the mode you want to use, the number must be from 0 to %d: ", discoveryDisplays.results[n_card].displayModesCount - 1);
  fflush(stdin);
  scanf("%ld", &n_mode);
  if (n_mode >= discoveryDisplays.results[n_card].displayModesCount) {
    fprintf(stderr, "The mode you chose '%ld' is not valid\n", n_mode);
    DiscoveryDisplays_free(&discoveryDisplays);
    exit(1);
  }

  struct DiscoveryDisplay discoveryDisplay = discoveryDisplays.results[n_card];
  drmModeModeInfo displayMode = discoveryDisplay.displayModes[n_mode];
  char *card_path = discoveryDisplay.cardPath;
  uint32_t fd_card = open(card_path, O_RDWR | O_CLOEXEC);
  if (fd_card <= 0) {
    fprintf(stderr, "open %s: %s", card_path, strerror(errno));
    DiscoveryDisplays_free(&discoveryDisplays);
    exit(1);
  }

  struct Display display = { 0 };
  display.displayMode = displayMode;
  display.fd_card = fd_card;
  display.connector_id = discoveryDisplay.connector_id;
  display.encoder_id = discoveryDisplay.encoder_id;
  display.crtc_id = discoveryDisplay.crtc_id;
  display.crtc_index = discoveryDisplay.crtc_index;

  DiscoveryDisplays_free(&discoveryDisplays);

  if (drmSetClientCap(fd_card, DRM_CLIENT_CAP_ATOMIC, 1) != 0) {
    fprintf(stderr, "Atomic modesetting not supported\n");
    Display_close(&display);
    exit(1);
  }
  if (drmSetClientCap(fd_card, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) != 0) {
    fprintf(stderr, "Universal planes not supported\n");
    Display_close(&display);
    exit(1);
  }

  int plane_primary = -1;
  int plane_cursor = -1;
  int plane_overlay = -1;
  drmModePlaneRes *planes = drmModeGetPlaneResources(fd_card);
  for (size_t i = 0; i < planes->count_planes; ++i) {
    uint32_t plane_id = planes->planes[i];
    drmModePlane *planePtr = drmModeGetPlane(fd_card, plane_id);
    drmModeObjectProperties *props = drmModeObjectGetProperties(fd_card, planes->planes[i], DRM_MODE_OBJECT_PLANE);
    if (!props) continue;

    uint64_t type_val = 9999;
    for (size_t i = 0; i < props->count_props; ++i) {
      drmModePropertyRes* prop = drmModeGetProperty(fd_card, props->props[i]);
      if (strcmp(prop->name, "type") == 0) {
        type_val = props->prop_values[i];
      }
      drmModeFreeProperty(prop);
    }

    if (planePtr->possible_crtcs & (1 << display.crtc_index)) {
      printf("DRM_FORMAT_XRGB8888: %c%c%c%c (0x%x)\n",
             DRM_FORMAT_XRGB8888& 0xFF,
             (DRM_FORMAT_XRGB8888>> 8) & 0xFF,
             (DRM_FORMAT_XRGB8888>> 16) & 0xFF,
             (DRM_FORMAT_XRGB8888>> 24) & 0xFF,
             DRM_FORMAT_XRGB8888
             );
      for (size_t i = 0; i < planePtr->count_formats; ++i) {
        uint32_t format = planePtr->formats[i];
        printf("  format: %c%c%c%c (0x%x)\n",
               format & 0xFF,
               (format >> 8) & 0xFF,
               (format >> 16) & 0xFF,
               (format >> 24) & 0xFF,
               format
               );
      }

      switch (type_val) {
        case DRM_PLANE_TYPE_OVERLAY:
          plane_overlay = plane_id;
          break;
        case DRM_PLANE_TYPE_PRIMARY:
          plane_primary = plane_id;
          break;
        case DRM_PLANE_TYPE_CURSOR:
          plane_cursor = plane_id;
          break;
        case 3:
          fprintf(stderr, "Unknown plane type %ld\n", type_val);
          break;
      }
    }

    drmModeFreePlane(planePtr);
  }

  if (plane_primary == -1 || plane_cursor == -1 || plane_overlay == -1) {
    fprintf(
      stderr,
      "Failed to find a plane which is compatible with the crtc with ID %d, PRIMARY(%s) CURSOR(%s) OVERLAY(%s)",
      display.crtc_id,
      plane_primary == -1 ? "NOT FOUND" : "FOUND",
      plane_cursor == -1 ? "NOT FOUND" : "FOUND",
      plane_overlay == -1 ? "NOT FOUND" : "FOUND"
    );
    Display_close(&display);
    exit(1);
  }

  display.plane_primary = plane_primary;
  display.plane_cursor = plane_cursor;
  display.plane_overlay = plane_overlay;

  if (drmModeCreatePropertyBlob(display.fd_card, &display.displayMode, sizeof(display.displayMode), &display.mode_blob_id) != 0) {
    perror("drmModeCreatePropertyBlob");
    Display_close(&display);
    exit(1);
  }

  return display;
}

void Display_close(struct Display *display) {
  close(display->fd_card);
}
