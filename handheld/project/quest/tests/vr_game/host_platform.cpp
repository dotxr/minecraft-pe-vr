#include "host_platform.h"

#include <cstdio>
#include <cstdlib>

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

AppPlatform_host::AppPlatform_host(const std::string& assetsDir)
:   _assetsDir(assetsDir)
{
}

TextureData AppPlatform_host::loadTexture(const std::string& filename, bool textureFolder) {
    TextureData out;
    std::string path = _assetsDir + "/" + filename;
    int w = 0, h = 0, channels = 0;
    // RGBA8, rows top to bottom, as the GL upload in Textures expects.
    unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!pixels) return out;
    out.w = w;
    out.h = h;
    out.data = pixels;
    out.numBytes = w * h * 4;
    out.memoryHandledExternally = false;
    return out;
}

BinaryBlob AppPlatform_host::readAssetFile(const std::string& filename) {
    std::string path = _assetsDir + "/" + filename;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return BinaryBlob();
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* data = (unsigned char*)malloc(size > 0 ? size : 1);
    size_t read = fread(data, 1, size, f);
    fclose(f);
    return BinaryBlob(data, (unsigned int)read);
}
