#include "wav.h"

int main() {
    FILE* fp = 0;
    fp = fopen("test.wav", "r");
    if(fp == 0) {
        printf("Failed to open file\n");
        return 1;
    }

    wav_metadata* md;
    error_code res = create_wav_metadata(fp, &md);
    res = read_wav_metadata(md);
    if(res != E_OK) {
        printf("read_wav failed with error code %d", res);
        return 1;
    }

    FILE* fp2 = 0;
    fp2 = fopen("test2.wav", "w");
    if(fp == 0) {
        printf("Failed to open file\n");
        return 1;
    }

    wav_metadata* md2;
    res = create_wav_metadata(fp2, &md2);
    if(res != E_OK) {
        printf("read_wav failed with error code %d", res);
        return 1;
    }

    md2->bits_per_sample = 32;
    md2->num_channels = 2;
    md2->num_samples = 0;
    md2->audio_format = 3;
    md2->sample_rate = 48000;

    res = write_wav_metadata(md2);

    print_metadata(md);

    fclose(fp2);
    fclose(fp);
    destroy_wav_metadata(md2);
    destroy_wav_metadata(md);
}
