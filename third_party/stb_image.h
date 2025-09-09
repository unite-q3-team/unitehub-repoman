// Minimal drop-in of stb_image to avoid external dependency hurdles
// Full library: https://github.com/nothings/stb (public domain)
#ifndef THIRD_PARTY_STB_IMAGE_H
#define THIRD_PARTY_STB_IMAGE_H

#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ASSERT(x)

extern "C" {
    unsigned char *stbi_load_from_memory(const unsigned char *buffer, int len, int *x, int *y, int *channels_in_file, int desired_channels);
    void stbi_image_free(void *retval_from_stbi_load);
}

#endif // THIRD_PARTY_STB_IMAGE_H


