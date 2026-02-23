#ifndef EFFECTS_H
#define EFFECTS_H

#include "image.h"
#include "layer.h"

void applyGrayscale(RasterImage& image);
void applyGrayscale(ImageBuffer& buffer);
void applyGrayscale(Layer& layer);

void applySepia(RasterImage& image, float strength = 1.0f);
void applySepia(ImageBuffer& buffer, float strength = 1.0f);
void applySepia(Layer& layer, float strength = 1.0f);

#endif
