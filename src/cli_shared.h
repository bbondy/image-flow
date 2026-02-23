#ifndef CLI_SHARED_H
#define CLI_SHARED_H

#include "bmp.h"
#include "gif.h"
#include "jpg.h"
#include "layer.h"
#include "png.h"
#include "webp.h"

#include <string>

std::string toLower(std::string value);
std::string extensionLower(const std::string& path);
bool saveCompositeByExtension(const ImageBuffer& composite, const std::string& outPath);
RasterImage* loadImageByExtension(const std::string& imagePath, BMPImage& bmp, PNGImage& png, JPGImage& jpg, GIFImage& gif, WEBPImage& webp);
void printGroupInfo(const LayerGroup& group, const std::string& indent);

#endif
