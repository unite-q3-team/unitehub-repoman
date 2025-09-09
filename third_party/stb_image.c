#include "stb_image.h"

#ifdef _WIN32
#include <windows.h>
#include <gdiplus.h>
#include <cstdlib>
#include <cstring>

using namespace Gdiplus;

extern "C" unsigned char* stbi_load_from_memory(const unsigned char* buffer,
                                                 int len,
                                                 int* x,
                                                 int* y,
                                                 int* channels_in_file,
                                                 int desired_channels)
{
    (void)desired_channels; // Always return RGBA (4)
    if (!buffer || len <= 0) return nullptr;

    ULONG_PTR gdipToken = 0;
    GdiplusStartupInput gdipStartupInput;
    if (GdiplusStartup(&gdipToken, &gdipStartupInput, nullptr) != Ok) {
        return nullptr;
    }

    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)len);
    if (!hGlobal) {
        GdiplusShutdown(gdipToken);
        return nullptr;
    }

    void* dst = GlobalLock(hGlobal);
    if (!dst) {
        GlobalFree(hGlobal);
        GdiplusShutdown(gdipToken);
        return nullptr;
    }
    std::memcpy(dst, buffer, (size_t)len);
    GlobalUnlock(hGlobal);

    IStream* stream = nullptr;
    if (CreateStreamOnHGlobal(hGlobal, TRUE, &stream) != S_OK || !stream) {
        GlobalFree(hGlobal);
        GdiplusShutdown(gdipToken);
        return nullptr;
    }

    Bitmap* source = Bitmap::FromStream(stream);
    stream->Release();
    if (!source || source->GetLastStatus() != Ok) {
        delete source;
        GdiplusShutdown(gdipToken);
        return nullptr;
    }

    const UINT width = source->GetWidth();
    const UINT height = source->GetHeight();
    if (x) *x = (int)width;
    if (y) *y = (int)height;
    if (channels_in_file) *channels_in_file = 4;

    Bitmap* converted = new Bitmap(width, height, PixelFormat32bppARGB);
    if (!converted || converted->GetLastStatus() != Ok) {
        delete converted;
        delete source;
        GdiplusShutdown(gdipToken);
        return nullptr;
    }

    {
        Graphics g(converted);
        g.DrawImage(source, 0, 0, width, height);
    }
    delete source;

    BitmapData data{};
    Rect rect(0, 0, (INT)width, (INT)height);
    if (converted->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &data) != Ok) {
        delete converted;
        GdiplusShutdown(gdipToken);
        return nullptr;
    }

    const size_t outSize = (size_t)width * (size_t)height * 4u;
    unsigned char* out = (unsigned char*)std::malloc(outSize);
    if (!out) {
        converted->UnlockBits(&data);
        delete converted;
        GdiplusShutdown(gdipToken);
        return nullptr;
    }

    // Convert GDI+ BGRA to RGBA without un-premultiplying alpha
    for (UINT row = 0; row < height; ++row) {
        const unsigned char* src = (const unsigned char*)data.Scan0 + row * (size_t)data.Stride;
        unsigned char* dstRow = out + row * (size_t)width * 4u;
        for (UINT col = 0; col < width; ++col) {
            const unsigned char b = src[col * 4 + 0];
            const unsigned char g = src[col * 4 + 1];
            const unsigned char r = src[col * 4 + 2];
            const unsigned char a = src[col * 4 + 3];
            dstRow[col * 4 + 0] = r;
            dstRow[col * 4 + 1] = g;
            dstRow[col * 4 + 2] = b;
            dstRow[col * 4 + 3] = a;
        }
    }

    converted->UnlockBits(&data);
    delete converted;
    GdiplusShutdown(gdipToken);
    return out;
}

extern "C" void stbi_image_free(void* retval_from_stbi_load)
{
    if (retval_from_stbi_load) std::free(retval_from_stbi_load);
}

#else

#include <png.h>
#include <stdlib.h>
#include <string.h>

typedef struct MemoryPngReader {
    const unsigned char* data;
    size_t size;
    size_t offset;
} MemoryPngReader;

static void png_read_from_memory(png_structp png_ptr, png_bytep out_bytes, png_size_t byte_count_to_read)
{
    MemoryPngReader* reader = (MemoryPngReader*)png_get_io_ptr(png_ptr);
    if (!reader || reader->offset + byte_count_to_read > reader->size) {
        png_error(png_ptr, "png_read_from_memory: read past end");
        return;
    }
    memcpy(out_bytes, reader->data + reader->offset, byte_count_to_read);
    reader->offset += byte_count_to_read;
}

extern "C" unsigned char* stbi_load_from_memory(const unsigned char* buffer,
                                                 int len,
                                                 int* x,
                                                 int* y,
                                                 int* channels_in_file,
                                                 int desired_channels)
{
    (void)desired_channels; // We will output RGBA
    if (!buffer || len <= 0) return NULL;

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) return NULL;
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) { png_destroy_read_struct(&png_ptr, NULL, NULL); return NULL; }
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    MemoryPngReader reader; reader.data = buffer; reader.size = (size_t)len; reader.offset = 0;
    png_set_read_fn(png_ptr, &reader, png_read_from_memory);

    png_read_info(png_ptr, info_ptr);
    png_uint_32 width, height;
    int bit_depth, color_type;
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

    if (bit_depth == 16) png_set_strip_16(png_ptr);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png_ptr);
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    png_size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    int out_channels = 4; // RGBA
    size_t out_size = (size_t)width * (size_t)height * (size_t)out_channels;
    unsigned char* image_data = (unsigned char*)malloc(out_size);
    if (!image_data) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    if (!row_pointers) {
        free(image_data);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }
    for (png_uint_32 i = 0; i < height; ++i)
        row_pointers[i] = image_data + i * (size_t)width * (size_t)out_channels;

    // If rowbytes already matches width*4, we can read directly; else use temp row then copy
    if (rowbytes == (png_size_t)width * (png_size_t)out_channels) {
        png_read_image(png_ptr, row_pointers);
    } else {
        png_bytep temp_row = (png_bytep)malloc(rowbytes);
        if (!temp_row) {
            free(row_pointers);
            free(image_data);
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            return NULL;
        }
        for (png_uint_32 i = 0; i < height; ++i) {
            png_read_row(png_ptr, temp_row, NULL);
            memcpy(row_pointers[i], temp_row, (size_t)width * (size_t)out_channels);
        }
        free(temp_row);
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    free(row_pointers);

    if (x) *x = (int)width;
    if (y) *y = (int)height;
    if (channels_in_file) *channels_in_file = out_channels;
    return image_data;
}

extern "C" void stbi_image_free(void* retval_from_stbi_load)
{
    if (retval_from_stbi_load) free(retval_from_stbi_load);
}

#endif

