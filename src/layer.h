#ifndef LAYER_H
#define LAYER_H

#include "image.h"
#include "transform.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

enum class BlendMode {
    Normal,
    Multiply,
    Screen,
    Overlay,
    Darken,
    Lighten,
    Add,
    Subtract,
    Difference,
    ColorDodge
};

struct PixelRGBA8 {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t a;

    PixelRGBA8() : r(0), g(0), b(0), a(0) {}
    PixelRGBA8(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha)
        : r(red), g(green), b(blue), a(alpha) {}
};

class ImageBuffer {
public:
    ImageBuffer();
    ImageBuffer(int width, int height, const PixelRGBA8& fill = PixelRGBA8(0, 0, 0, 0));

    int width() const;
    int height() const;

    bool inBounds(int x, int y) const;
    const PixelRGBA8& getPixel(int x, int y) const;
    bool trySetPixel(int x, int y, const PixelRGBA8& pixel);
    void setPixel(int x, int y, const PixelRGBA8& pixel);
    void fill(const PixelRGBA8& pixel);

private:
    int m_width;
    int m_height;
    std::vector<PixelRGBA8> m_pixels;
};

class Layer : public Transformable {
public:
    Layer();
    Layer(const std::string& name, int width, int height, const PixelRGBA8& fill = PixelRGBA8(0, 0, 0, 0));

    const std::string& name() const;
    void setName(const std::string& name);

    bool visible() const;
    void setVisible(bool visible);

    float opacity() const;
    void setOpacity(float opacity);

    BlendMode blendMode() const;
    void setBlendMode(BlendMode mode);

    int offsetX() const;
    int offsetY() const;
    void setOffset(int x, int y);

    bool hasMask() const;
    void enableMask(const PixelRGBA8& fill = PixelRGBA8(255, 255, 255, 255));
    void clearMask();
    ImageBuffer& mask();
    const ImageBuffer& mask() const;

    ImageBuffer& image();
    const ImageBuffer& image() const;
    void setImageFromRaster(const RasterImage& source, std::uint8_t alpha = 255);

private:
    std::string m_name;
    bool m_visible;
    float m_opacity;
    BlendMode m_blendMode;
    int m_offsetX;
    int m_offsetY;
    ImageBuffer m_image;
    bool m_hasMask;
    ImageBuffer m_mask;
};

class LayerGroup;

class LayerNode {
public:
    enum class Kind {
        Layer,
        Group
    };

    explicit LayerNode(const Layer& layer);
    explicit LayerNode(const LayerGroup& group);
    LayerNode(const LayerNode& other);
    LayerNode& operator=(const LayerNode& other);
    LayerNode(LayerNode&& other) noexcept;
    LayerNode& operator=(LayerNode&& other) noexcept;
    ~LayerNode();

    Kind kind() const;
    bool isLayer() const;
    bool isGroup() const;

    Layer& asLayer();
    const Layer& asLayer() const;

    LayerGroup& asGroup();
    const LayerGroup& asGroup() const;

private:
    Kind m_kind;
    Layer m_layer;
    std::unique_ptr<LayerGroup> m_group;
};

class LayerGroup : public Transformable {
public:
    LayerGroup();
    explicit LayerGroup(const std::string& name);

    const std::string& name() const;
    void setName(const std::string& name);

    bool visible() const;
    void setVisible(bool visible);

    float opacity() const;
    void setOpacity(float opacity);

    BlendMode blendMode() const;
    void setBlendMode(BlendMode mode);

    int offsetX() const;
    int offsetY() const;
    void setOffset(int x, int y);

    Layer& addLayer(const Layer& layer);
    LayerGroup& addGroup(const LayerGroup& group);

    std::size_t nodeCount() const;
    LayerNode& node(std::size_t index);
    const LayerNode& node(std::size_t index) const;

    std::size_t layerCount() const;
    Layer& layer(std::size_t index);
    const Layer& layer(std::size_t index) const;

private:
    std::string m_name;
    bool m_visible;
    float m_opacity;
    BlendMode m_blendMode;
    int m_offsetX;
    int m_offsetY;
    std::vector<LayerNode> m_nodes;
};

class Document {
public:
    Document(int width, int height);

    int width() const;
    int height() const;

    Layer& addLayer(const Layer& layer);
    LayerGroup& addGroup(const LayerGroup& group);

    std::size_t nodeCount() const;
    LayerNode& node(std::size_t index);
    const LayerNode& node(std::size_t index) const;

    std::size_t layerCount() const;
    Layer& layer(std::size_t index);
    const Layer& layer(std::size_t index) const;

    LayerGroup& rootGroup();
    const LayerGroup& rootGroup() const;

    ImageBuffer composite() const;

private:
    int m_width;
    int m_height;
    LayerGroup m_root;
};

ImageBuffer fromRasterImage(const RasterImage& source, std::uint8_t alpha = 255);
void copyToRasterImage(const ImageBuffer& source, RasterImage& destination);
bool saveDocumentIFLOW(const Document& document, const std::string& path);
Document loadDocumentIFLOW(const std::string& path);

#endif
