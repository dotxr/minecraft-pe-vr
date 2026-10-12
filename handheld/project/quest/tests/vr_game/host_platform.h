#ifndef TESTS_VR_GAME_HOST_PLATFORM_H__
#define TESTS_VR_GAME_HOST_PLATFORM_H__

#include "AppPlatform.h"

#include <string>

// Desktop stand-in for AppPlatform_android: textures and assets come from the game's
// Android asset folder on disk, instead of through the Java activity.
class AppPlatform_host : public AppPlatform {
public:
    explicit AppPlatform_host(const std::string& assetsDir);

    TextureData loadTexture(const std::string& filename, bool textureFolder);
    BinaryBlob readAssetFile(const std::string& filename);

    bool isSuperFast() { return false; }
    bool isPowerVR() { return false; }
    bool supportsTouchscreen() { return false; }

private:
    std::string _assetsDir;
};

#endif /*TESTS_VR_GAME_HOST_PLATFORM_H__*/
