#ifndef API_H
#define API_H

#include "bmp.h"
#include "gif.h"
#include "jpg.h"
#include "png.h"

namespace api {
BMPImage createSmiley256BMP();
PNGImage createSmiley256PNG();
JPGImage createSmiley256JPG();
GIFImage createSmiley256GIF();
PNGImage createSmiley256LayeredPNG();
PNGImage createLayerBlendDemoPNG();
}

#endif
