/* Copyright (c) 2018 Mozilla */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "arch.h"
#include "lpcnet.h"
#include "freq.h"
#include "os_support.h"
#include "fargan.h"

#ifdef USE_WEIGHTS_FILE
# if __unix__
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <unistd.h>
#  include <sys/stat.h>
/* When available, mmap() is preferable to reading the file, as it leads to
   better resource utilization, especially if multiple processes are using the same
   file (mapping will be shared in cache). */
unsigned char *load_blob(const char *filename, int *len) {
  int fd;
  unsigned char *data;
  struct stat st;
  stat(filename, &st);
  *len = st.st_size;
  fd = open(filename, O_RDONLY);
  data = mmap(NULL, *len, PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  return data;
}
void free_blob(unsigned char *blob, int len) {
  munmap(blob, len);
}
# else
unsigned char *load_blob(const char *filename, int *len) {
  FILE *file;
  unsigned char *data;
  file = fopen(filename, "r");
  fseek(file, 0L, SEEK_END);
  *len = ftell(file);
  fseek(file, 0L, SEEK_SET);
  if (*len <= 0) return NULL;
  data = malloc(*len);
  *len = fread(data, 1, *len, file);
  return data;
}
void free_blob(unsigned char *blob, int len) {
  free(blob);
  (void)len;
}
# endif
#endif

#define MODE_FEATURES 2
/*#define MODE_SYNTHESIS 3*/
#define MODE_PLC 4
#define MODE_ADDLPC 5
#define MODE_FWGAN_SYNTHESIS 6
#define MODE_FARGAN_SYNTHESIS 7

void usage(void) {
    fprintf(stderr, "usage: lpcnet_demo -features <input.pcm> <features.f32>\n");
    fprintf(stderr, "       lpcnet_demo -fargan-synthesis <features.f32> <output.pcm>\n");
    fprintf(stderr, "       lpcnet_demo -plc <plc_options> <percent> <input.pcm> <output.pcm>\n");
    fprintf(stderr, "       lpcnet_demo -plc-file <plc_options> <percent> <input.pcm> <output.pcm>\n");
    fprintf(stderr, "       lpcnet_demo -addlpc <features_without_lpc.f32> <features_with_lpc.lpc>\n\n");
    fprintf(stderr, "  plc_options:\n");
    fprintf(stderr, "       causal:       normal (causal) PLC\n");
    fprintf(stderr, "       codec:        normal (causal) PLC without cross-fade (will glitch)\n");
    exit(1);
}

int main(int argc, char **argv) {
    int mode=0;
    int plc_percent=0;
    FILE *fin, *fout;
    FILE *plc_file = NULL;
    const char *plc_options;
    int plc_flags=-1;
#ifdef USE_WEIGHTS_FILE
    int len;
    unsigned char *data;
    const char *filename = "weights_blob.bin";
#endif
    if (argc < 4) usage();
    if (strcmp(argv[1], "-features") == 0) mode=MODE_FEATURES;
    else if (strcmp(argv[1], "-fargan-synthesis") == 0) mode=MODE_FARGAN_SYNTHESIS;
    else if (strcmp(argv[1], "-plc") == 0) {
        mode=MODE_PLC;
        plc_options = argv[2];
        plc_percent = atoi(argv[3]);
        argv+=2;
        argc-=2;
    } else if (strcmp(argv[1], "-plc-file") == 0) {
        mode=MODE_PLC;
        plc_options = argv[2];
        plc_file = fopen(argv[3], "r");
        if (!plc_file) {
            fprintf(stderr, "Can't open %s\n", argv[3]);
            exit(1);
        }
        argv+=2;
        argc-=2;
    } else if (strcmp(argv[1], "-addlpc") == 0){
        mode=MODE_ADDLPC;
    } else {
        usage();
    }
    if (mode == MODE_PLC) {
        if (strcmp(plc_options, "causal")==0) plc_flags = LPCNET_PLC_CAUSAL;
        else if (strcmp(plc_options, "codec")==0) plc_flags = LPCNET_PLC_CODEC;
        else usage();
    }
    if (argc != 4) usage();
    fin = fopen(argv[2], "rb");
    if (fin == NULL) {
        fprintf(stderr, "Can't open %s\n", argv[2]);
        exit(1);
    }

    fout = fopen(argv[3], "wb");
    if (fout == NULL) {
        fprintf(stderr, "Can't open %s\n", argv[3]);
        exit(1);
    }
#ifdef USE_WEIGHTS_FILE
    data = load_blob(filename, &len);
#endif
    if (mode == MODE_FEATURES) {
        LPCNetEncState *net;
        net = lpcnet_encoder_create();
        while (1) {
            float features[NB_TOTAL_FEATURES];
            opus_int16 pcm[LPCNET_FRAME_SIZE];
            size_t ret;
            ret = fread(pcm, sizeof(pcm[0]), LPCNET_FRAME_SIZE, fin);
            if (feof(fin) || ret != LPCNET_FRAME_SIZE) break;
            lpcnet_compute_single_frame_features(net, pcm, features);
            fwrite(features, sizeof(float), NB_TOTAL_FEATURES, fout);
        }
        lpcnet_encoder_destroy(net);
    } else if (mode == MODE_FARGAN_SYNTHESIS) {
        FARGANState fargan;
        size_t ret, i;
        float in_features[5*NB_TOTAL_FEATURES];
        float zeros[320] = {0};
        fargan_init(&fargan);
#ifdef USE_WEIGHTS_FILE
        fargan_load_model(fwgan, data, len);
#endif
        /* uncomment the following to align with Python code */
        /*ret = fread(&in_features[0], sizeof(in_features[0]), NB_TOTAL_FEATURES, fin);*/
        for (i=0;i<5;i++) {
          ret = fread(&in_features[i*NB_FEATURES], sizeof(in_features[0]), NB_TOTAL_FEATURES, fin);
        }
        fargan_cont(&fargan, zeros, in_features);
        while (1) {
            float features[NB_FEATURES];
            float fpcm[LPCNET_FRAME_SIZE];
            opus_int16 pcm[LPCNET_FRAME_SIZE];
            ret = fread(in_features, sizeof(features[0]), NB_TOTAL_FEATURES, fin);
            if (feof(fin) || ret != NB_TOTAL_FEATURES) break;
            OPUS_COPY(features, in_features, NB_FEATURES);
            fargan_synthesize(&fargan, fpcm, features);
            for (i=0;i<LPCNET_FRAME_SIZE;i++) pcm[i] = (int)floor(.5 + MIN32(32767, MAX32(-32767, 32768.f*fpcm[i])));
            fwrite(pcm, sizeof(pcm[0]), LPCNET_FRAME_SIZE, fout);
        }
    } else if (mode == MODE_PLC) {
        opus_int16 pcm[FRAME_SIZE];
        int count=0;
        int loss=0;
        int skip=0, extra=0;
        LPCNetPLCState *net;
        net = lpcnet_plc_create(plc_flags);
#ifdef USE_WEIGHTS_FILE
        lpcnet_plc_load_model(net, data, len);
#endif
        while (1) {
            size_t ret;
            ret = fread(pcm, sizeof(pcm[0]), FRAME_SIZE, fin);
            if (feof(fin) || ret != FRAME_SIZE) break;
            if (count % 2 == 0) {
              if (plc_file != NULL) ret = fscanf(plc_file, "%d", &loss);
              else loss = rand() < (float)RAND_MAX*(float)plc_percent/100.f;
            }
            if (loss) lpcnet_plc_conceal(net, pcm);
            else lpcnet_plc_update(net, pcm);
            fwrite(&pcm[skip], sizeof(pcm[0]), FRAME_SIZE-skip, fout);
            skip = 0;
            count++;
        }
        if (extra) {
          lpcnet_plc_conceal(net, pcm);
          fwrite(pcm, sizeof(pcm[0]), extra, fout);
        }
        lpcnet_plc_destroy(net);
    } else if (mode == MODE_ADDLPC) {
        float features[36];
        size_t ret;

        while (1) {
            ret = fread(features, sizeof(features[0]), 36, fin);
            if (ret != 36 || feof(fin)) break;
            lpc_from_cepstrum(&features[20], &features[0]);
            fwrite(features, sizeof(features[0]), 36, fout);
        }

    } else {
        fprintf(stderr, "unknown action\n");
    }
    fclose(fin);
    fclose(fout);
    if (plc_file) fclose(plc_file);
#ifdef USE_WEIGHTS_FILE
    free_blob(data, len);
#endif
    return 0;
}
