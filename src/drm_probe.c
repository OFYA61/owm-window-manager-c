#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

struct system_graphics_cards{
  char **cards;
  size_t count;
};

char **get_cards(size_t* n_out) {
  DIR *d;
  struct dirent *dir;
  d = opendir("/dev/dri");
  if (!d) {
    perror("opendir");
    closedir(d);
    return NULL;
  }
  
  uint32_t card_count = 0;
  while ((dir = readdir(d)) != NULL) {
    if (strstr(dir->d_name, "card") == NULL) {
      continue;
    }
    card_count++;
  }
  *n_out = card_count;
  closedir(d);

  d = opendir("/dev/dri");
  if (!d) {
    perror("opendir");
    closedir(d);
    return NULL;
  }

  char** cards = malloc(sizeof(char *) * card_count);
  uint32_t i = 0;
  while ((dir = readdir(d)) != NULL) {
    if (strstr(dir->d_name, "card") == NULL) {
      continue;
    }
    cards[i] = (char *) malloc(sizeof(char) * strlen(dir->d_name) + 1);
    mempcpy(cards[i], dir->d_name, sizeof(char) * strlen(dir->d_name));
    i++;
  }

  closedir(d);
  return cards;
}

void free_cards(char **cards, size_t n) {
  for (size_t i = 0; i < n; ++ i) {
    free(cards[i]);
  }
  free(cards);
}

uint32_t main() {
  size_t card_count;
  char ** cards = get_cards(&card_count);
  if (!cards) {
    perror("get_cards");
    return 1;
  }

  char card_path[64];
  for (size_t i = 0; i < card_count; ++i) {
    card_path[0] = '\0';
    strcat(card_path, "/dev/dri/");
    strcat(card_path, cards[i]);

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

    printf("Card %s\n", card_path);
    printf("\tConnectors: %d\n", res->count_connectors);
    for (uint32_t i = 0; i < res->count_connectors; ++i) {
      drmModeConnector* conn = drmModeGetConnector(fd, res->connectors[i]);
      if (!conn) {
        fprintf(stderr, "drmModeGetConnnector: %s\n", card_path);
        drmModeFreeConnector(conn);
        continue;
      }
      // Physical port has no counnection to a display
      if (conn->connection != DRM_MODE_CONNECTED && conn->count_modes <= 0) {
        drmModeFreeConnector(conn);
        continue;
      }

      printf("\tConnector %d\n", conn->connector_id);

      for (uint32_t m = 0; m < conn->count_modes; ++m) {
        drmModeModeInfo* mode = &conn->modes[m];
        printf("\t\t%dx%d @ %dHz\n", mode->hdisplay, mode->vdisplay, mode->vrefresh);
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
        drmModeFreeEncoder(enc);
        drmModeFreeConnector(conn);
        continue;
      }
      printf("Using CRTC %u\n", crtc_id);

      drmModeFreeEncoder(enc);
      drmModeFreeConnector(conn);
    }

    drmModeFreeResources(res);
    close(fd);
  }

  free_cards(cards, card_count);

  return 0;
}
