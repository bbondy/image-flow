#ifndef EXAMPLE_API_H
#define EXAMPLE_API_H

#include "bmp.h"
#include "gif.h"
#include "jpg.h"
#include "png.h"

namespace example_api {
BMPImage createSmiley256BMP();
PNGImage createSmiley256PNG();
JPGImage createSmiley256JPG();
GIFImage createSmiley256GIF();
PNGImage createSmiley256LayeredPNG();
PNGImage createLayerBlendDemoPNG();
}

#endif
