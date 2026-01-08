#include "display.h"
#include "drm.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

OWM_DRMDisplays OWM_DISPLAYS = { NULL, 0 };

typedef struct {
  char **cards;
  size_t count;
} OWM_DRMGraphicsCards;

static void freeDRMGraphicsCards(OWM_DRMGraphicsCards* gcs) {
  for (size_t i = 0; i < gcs->count; ++i) {
    free(gcs->cards[i]);
  }
  free(gcs->cards);
}

static int discoverDRMGraphicsCards(OWM_DRMGraphicsCards* out) {
  DIR *d;
  struct dirent *dir;
  d = opendir("/dev/dri");
  if (!d) {
    perror("opendir");
    closedir(d);
    return 1;
  }

  size_t i = 0;
  size_t capacity = 2; // Have 2 slots initially
  out->cards = malloc(capacity * sizeof(char *));
  while ((dir = readdir(d)) != NULL) {
    if (strstr(dir->d_name, "card") == NULL) {
      continue;
    }

    if (i == capacity) { // Expand buffer if no space left
      capacity++;
      char **tmp_cards = realloc(out->cards, capacity * sizeof(char *));
      if (tmp_cards == NULL) {
        freeDRMGraphicsCards(out);
        return 1;
      }
      out->cards = tmp_cards;
    }

    out->cards[i] = (char *) malloc(sizeof(char) * strlen(dir->d_name) + 1);
    mempcpy(out->cards[i], dir->d_name, sizeof(char) * strlen(dir->d_name));
    i++;
    out->count = i;
  }
  closedir(d);
  return 0;
}

static uint32_t getPropId(int fd, uint32_t obj_id, uint32_t obj_type, const char *name) {
  drmModeObjectProperties *props = drmModeObjectGetProperties(fd, obj_id, obj_type);
  for (uint32_t i = 0; i < props->count_props; ++i) {
    drmModePropertyRes *prop = drmModeGetProperty(fd, props->props[i]);
    if (strcmp(prop->name, name) == 0) {
      uint32_t prop_id = prop->prop_id;
      drmModeFreeProperty(prop);
      drmModeFreeObjectProperties(props);
      return prop_id;
    }
    drmModeFreeProperty(prop);
  }
  drmModeFreeObjectProperties(props);
  fprintf(stderr, "Failed to find property obj_id=%d obj_type=%d name=%s\n", obj_id, obj_type, name);
  return 0;
}


int OWM_scanDRMDisplays() {
  OWM_DRMGraphicsCards gcs;
  if (discoverDRMGraphicsCards(&gcs)) {
    fprintf(stderr, "Failed to located graphics cards\n");
    return 1;
  }

  size_t displaysCapacity = 1;
  OWM_DISPLAYS.displays = malloc(displaysCapacity * sizeof(OWM_DRMDisplay));
  OWM_DISPLAYS.count = 0;
  char card_path[64];
  for (size_t gc_idx = 0; gc_idx < gcs.count; ++gc_idx) {
    card_path[0] = '\0';
    strcat(card_path, "/dev/dri/");
    strcat(card_path, gcs.cards[gc_idx]);
    printf("Processing card '%s'\n", card_path);

    uint32_t fd = open(card_path, O_RDWR | O_CLOEXEC);
    if (fd <= 0) {
      fprintf(stderr, "open %s: %s", card_path, strerror(errno));
      continue;
    }

    if (drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1) != 0) {
      fprintf(stderr, "Atomic modesetting not supported for card '%s'\n", card_path);
      close(fd);
      continue;
    }
    if (drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) != 0) {
      fprintf(stderr, "Universal planes not supported for card '%s'\n", card_path);
      close(fd);
      continue;
    }

    drmModeRes* res = drmModeGetResources(fd);
    if (!res) {
      fprintf(stderr, "drmModeGetResources %s: %s\n", card_path, strerror(errno));
      close(fd);
      continue;
    }

    for (int conn_idx = 0; conn_idx < res->count_connectors; ++conn_idx) {
      drmModeConnector* conn = drmModeGetConnector(fd, res->connectors[conn_idx]);
      if (!conn) {
        fprintf(stderr, "drmModeGetConnnector: %s\n", card_path);
        drmModeFreeConnector(conn);
        continue;
      }

      if (conn->connection != DRM_MODE_CONNECTED && conn->count_modes <= 0) { // Physical port has no connection to display
        drmModeFreeConnector(conn);
        continue;
      }

      drmModeEncoder *enc = NULL;
      if (conn->encoder_id) {
        enc = drmModeGetEncoder(fd, conn->encoder_id);
      } else {
        for (int encIdx = 0; encIdx < conn->count_encoders; ++encIdx) {
          enc = drmModeGetEncoder(fd, conn->encoders[encIdx]);
          if (enc) break;
        }
      }
      if (!enc) {
        fprintf(stderr, "No encoder found \n");
        drmModeFreeConnector(conn);
        continue;
      }

      uint32_t crtc_id = 0;
      uint32_t crtc_index = 0;
      for (int crtc_idx = 0; crtc_idx < res->count_crtcs; ++crtc_idx) {
        if (enc->possible_crtcs & (1 << crtc_idx)) {
          crtc_id = res->crtcs[crtc_idx];
          crtc_index = crtc_idx;
          break;
        }
      }

      if (!crtc_id) {
        fprintf(stderr, "No compatible CRTC found\n");
        goto conn_enc_cleanup;
      }

      if (OWM_DISPLAYS.count == displaysCapacity) { // Expand buffer if no space left
        displaysCapacity++;
        OWM_DRMDisplay* tmp_displays = realloc(OWM_DISPLAYS.displays, displaysCapacity * sizeof(OWM_DRMDisplay));
        if (tmp_displays == NULL) {
          fprintf(stderr, "Failed to expand storage for ProbeResults");
          goto conn_enc_cleanup;
        }
        OWM_DISPLAYS.displays = tmp_displays;
      }

      OWM_DRMDisplay display = { 0 };
      display.display_modes = malloc(conn->count_modes * sizeof(drmModeModeInfo));
      memcpy(display.display_modes, conn->modes, conn->count_modes * sizeof(drmModeModeInfo));

      display.count_display_modes = conn->count_modes;
      display.fd_card = fd;
      display.connector_id = conn->connector_id;
      display.encoder_id = enc->encoder_id;
      display.crtc_id = crtc_id;
      display.crtc_index = crtc_index;

      // TODO set plane_primary, plane_cursor, plane_overlay, mode_blob_id
      int plane_primary = -1;
      int plane_cursor = -1;
      int plane_overlay = -1;
      drmModePlaneRes *planes = drmModeGetPlaneResources(fd);
      for (size_t i = 0; i < planes->count_planes; ++i) {
        uint32_t plane_id = planes->planes[i];
        drmModePlane *plane = drmModeGetPlane(fd, plane_id);
        drmModeObjectProperties *props = drmModeObjectGetProperties(fd, planes->planes[i], DRM_MODE_OBJECT_PLANE);
        if (!props) {
          drmModeFreePlane(plane);
          continue;
        }

        uint64_t type_val = 99999999;
        for (size_t i = 0; i < props->count_props; ++i) {
          drmModePropertyRes* prop = drmModeGetProperty(fd, props->props[i]);
          if (strcmp(prop->name, "type") == 0) {
            type_val = props->prop_values[i];
          }
          drmModeFreeProperty(prop);
        }

        if (plane->possible_crtcs & (1 << crtc_index)) {
          // printf("DRM_FORMAT_XRGB8888: %c%c%c%c (0x%x)\n",
          //        DRM_FORMAT_XRGB8888& 0xFF,
          //        (DRM_FORMAT_XRGB8888>> 8) & 0xFF,
          //        (DRM_FORMAT_XRGB8888>> 16) & 0xFF,
          //        (DRM_FORMAT_XRGB8888>> 24) & 0xFF,
          //        DRM_FORMAT_XRGB8888
          //        );
          // for (size_t i = 0; i < plane->count_formats; ++i) {
          //   uint32_t format = plane->formats[i];
          //   printf("  format: %c%c%c%c (0x%x)\n",
          //          format & 0xFF,
          //          (format >> 8) & 0xFF,
          //          (format >> 16) & 0xFF,
          //          (format >> 24) & 0xFF,
          //          format
          //          );
          // }

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

        drmModeFreePlane(plane);
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
        goto conn_enc_cleanup;
      }

      display.plane_primary = plane_primary;
      display.plane_cursor = plane_cursor;
      display.plane_overlay = plane_overlay;

      display.plane_primary_properties.connector_crtc_id = getPropId(fd, conn->connector_id, DRM_MODE_OBJECT_CONNECTOR, "CRTC_ID");
      display.plane_primary_properties.crtc_activate = getPropId(fd, crtc_id, DRM_MODE_OBJECT_CRTC, "ACTIVE");
      display.plane_primary_properties.crtc_mode_id = getPropId(fd, crtc_id, DRM_MODE_OBJECT_CRTC, "MODE_ID");
      display.plane_primary_properties.plane_fb_id = getPropId(fd, plane_primary, DRM_MODE_OBJECT_PLANE, "FB_ID");
      display.plane_primary_properties.plane_crtc_id = getPropId(fd, plane_primary, DRM_MODE_OBJECT_PLANE, "CRTC_ID");
      display.plane_primary_properties.plane_crtc_x = getPropId(fd, plane_primary, DRM_MODE_OBJECT_PLANE, "CRTC_X");
      display.plane_primary_properties.plane_crtc_y = getPropId(fd, plane_primary, DRM_MODE_OBJECT_PLANE, "CRTC_Y");
      display.plane_primary_properties.plane_crtc_w = getPropId(fd, plane_primary, DRM_MODE_OBJECT_PLANE, "CRTC_W");
      display.plane_primary_properties.plane_crtc_h = getPropId(fd, plane_primary, DRM_MODE_OBJECT_PLANE, "CRTC_H");
      display.plane_primary_properties.plane_src_x = getPropId(fd, plane_primary, DRM_MODE_OBJECT_PLANE, "SRC_X");
      display.plane_primary_properties.plane_src_y = getPropId(fd, plane_primary, DRM_MODE_OBJECT_PLANE, "SRC_Y");
      display.plane_primary_properties.plane_src_w = getPropId(fd, plane_primary, DRM_MODE_OBJECT_PLANE, "SRC_W");
      display.plane_primary_properties.plane_src_h = getPropId(fd, plane_primary, DRM_MODE_OBJECT_PLANE, "SRC_H");

      OWM_DISPLAYS.displays[OWM_DISPLAYS.count] = display;
      OWM_DISPLAYS.count = OWM_DISPLAYS.count + 1;

    conn_enc_cleanup:
      drmModeFreeEncoder(enc);
      drmModeFreeConnector(conn);
    }

    drmModeFreeResources(res);
  }

  freeDRMGraphicsCards(&gcs);
  
  if (OWM_DISPLAYS.count == 0) {
    return 1;
  }

  return 0;
}

void OWM_closeDRMDisplays() {
  for (size_t display_idx = 0; display_idx < OWM_DISPLAYS.count; ++display_idx) {
    close(OWM_DISPLAYS.displays[display_idx].fd_card);
  }
}

inline const OWM_DRMDisplays* OWM_getDRMDisplays() {
  return &OWM_DISPLAYS;
}
