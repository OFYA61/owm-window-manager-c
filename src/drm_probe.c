#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

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

struct ProbeResult {
  uint32_t connector_id;
  uint32_t encoder_id;
  uint32_t crtc_id;
};

struct ProbeResults {
  struct ProbeResult* results;
  size_t count;
};

struct ProbeResults ProbeResults_probeDrm() {
  struct ProbeResults prs = { NULL, 0 };

  struct SystemGraphicsCards sgc = SystemGraphicsCards_discover();
  if (sgc.count == 0) {
    fprintf(stderr, "No cards where found");
    prs.count = 0;
    return prs;
  }

  int prs_size = 0;
  int prs_capacity = 1;
  prs.results = malloc(prs_capacity * sizeof(struct ProbeResult));
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

    // printf("Card %s\n", card_path);
    // printf("\tConnectors: %d\n", res->count_connectors);
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

      // printf("\tConnector %d\n", conn->connector_id);

      for (uint32_t m = 0; m < conn->count_modes; ++m) {
        drmModeModeInfo* mode = &conn->modes[m];
        // printf("\t\t%dx%d @ %dHz\n", mode->hdisplay, mode->vdisplay, mode->vrefresh);
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
      // printf("Using CRTC %u\n", crtc_id);

      if (prs_size == prs_capacity) { // Expand buffer if no space left
        prs_capacity++;
        struct ProbeResult * tmp_results = realloc(prs.results, prs_capacity * sizeof(struct ProbeResult));
        if (tmp_results == NULL) {
          fprintf(stderr, "Failed to expand storage for ProbeResults");
          goto conn_enc_cleanup;
        }
        prs.results = tmp_results;
      }
      struct ProbeResult pr = { conn->connector_id, enc->encoder_id, enc->crtc_id };
      prs.results[prs_size] = pr;
      prs_size++;
      prs.count = prs_size;

    conn_enc_cleanup:
      drmModeFreeEncoder(enc);
      drmModeFreeConnector(conn);

    }

    drmModeFreeResources(res);
    close(fd);
  }

  SystemGraphicsCards_free(&sgc);

  return prs;
}

int main() {
  struct ProbeResults probeResults = ProbeResults_probeDrm();
  if (probeResults.count == 0) {
    return 1;
  }
  for (int i = 0; i < probeResults.count; ++i) {
    printf("Conn:%d Enc:%d Crtc:%d\n", probeResults.results[i].connector_id, probeResults.results[i].encoder_id, probeResults.results[i].crtc_id);
  }
}
