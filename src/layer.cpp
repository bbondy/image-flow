#include "layer.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace {
std::size_t pixelIndex(int x, int y, int width) {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
}

float clamp01(float v) {
    if (v < 0.0f) {
        return 0.0f;
    }
    if (v > 1.0f) {
        return 1.0f;
    }
    return v;
}

float srgbToLinear(float c) {
    if (c <= 0.04045f) {
        return c / 12.92f;
    }
    return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

float linearToSrgb(float c) {
    const float clamped = clamp01(c);
    if (clamped <= 0.0031308f) {
        return clamped * 12.92f;
    }
    return 1.055f * std::pow(clamped, 1.0f / 2.4f) - 0.055f;
}

std::uint8_t toByte(float unit) {
    const float clamped = clamp01(unit);
    return static_cast<std::uint8_t>(std::lround(clamped * 255.0f));
}

float blendChannel(BlendMode mode, float d, float s) {
    switch (mode) {
        case BlendMode::Normal:
            return s;
        case BlendMode::Multiply:
            return d * s;
        case BlendMode::Screen:
            return 1.0f - (1.0f - d) * (1.0f - s);
        case BlendMode::Overlay:
            return d < 0.5f ? (2.0f * d * s) : (1.0f - 2.0f * (1.0f - d) * (1.0f - s));
        case BlendMode::Darken:
            return std::min(d, s);
        case BlendMode::Lighten:
            return std::max(d, s);
        case BlendMode::Add:
            return std::min(1.0f, d + s);
        case BlendMode::Subtract:
            return std::max(0.0f, d - s);
        case BlendMode::Difference:
            return std::abs(d - s);
        default:
            return s;
    }
}

float maskWeight(const PixelRGBA8& maskPixel) {
    const float alpha = static_cast<float>(maskPixel.a) / 255.0f;
    const float luma = (static_cast<float>(maskPixel.r) + static_cast<float>(maskPixel.g) + static_cast<float>(maskPixel.b)) / (255.0f * 3.0f);
    return clamp01(alpha * luma);
}

void compositePixel(PixelRGBA8& dst, const PixelRGBA8& src, BlendMode mode, float sourceOpacityScale) {
    const float sa = (static_cast<float>(src.a) / 255.0f) * clamp01(sourceOpacityScale);
    if (sa <= 0.0f) {
        return;
    }

    const float da = static_cast<float>(dst.a) / 255.0f;

    const float sr = srgbToLinear(static_cast<float>(src.r) / 255.0f);
    const float sg = srgbToLinear(static_cast<float>(src.g) / 255.0f);
    const float sb = srgbToLinear(static_cast<float>(src.b) / 255.0f);

    const float dr = srgbToLinear(static_cast<float>(dst.r) / 255.0f);
    const float dg = srgbToLinear(static_cast<float>(dst.g) / 255.0f);
    const float db = srgbToLinear(static_cast<float>(dst.b) / 255.0f);

    const float br = blendChannel(mode, dr, sr);
    const float bg = blendChannel(mode, dg, sg);
    const float bb = blendChannel(mode, db, sb);

    const float outA = sa + da * (1.0f - sa);

    float outR = 0.0f;
    float outG = 0.0f;
    float outB = 0.0f;

    if (outA > 0.0f) {
        const float premR = dr * da * (1.0f - sa) + sr * sa * (1.0f - da) + br * sa * da;
        const float premG = dg * da * (1.0f - sa) + sg * sa * (1.0f - da) + bg * sa * da;
        const float premB = db * da * (1.0f - sa) + sb * sa * (1.0f - da) + bb * sa * da;
        outR = premR / outA;
        outG = premG / outA;
        outB = premB / outA;
    }

    dst = PixelRGBA8(toByte(linearToSrgb(outR)), toByte(linearToSrgb(outG)), toByte(linearToSrgb(outB)), toByte(outA));
}

void compositeLayerOnto(ImageBuffer& out, const Layer& layer, int parentOffsetX, int parentOffsetY) {
    if (!layer.visible() || layer.opacity() <= 0.0f) {
        return;
    }

    const int layerOriginX = parentOffsetX + layer.offsetX();
    const int layerOriginY = parentOffsetY + layer.offsetY();

    for (int sy = 0; sy < layer.image().height(); ++sy) {
        const int dy = sy + layerOriginY;
        if (dy < 0 || dy >= out.height()) {
            continue;
        }

        for (int sx = 0; sx < layer.image().width(); ++sx) {
            const int dx = sx + layerOriginX;
            if (dx < 0 || dx >= out.width()) {
                continue;
            }

            const PixelRGBA8& src = layer.image().getPixel(sx, sy);
            PixelRGBA8 dst = out.getPixel(dx, dy);

            float opacityScale = layer.opacity();
            if (layer.hasMask()) {
                opacityScale *= maskWeight(layer.mask().getPixel(sx, sy));
            }

            compositePixel(dst, src, layer.blendMode(), opacityScale);
            out.setPixel(dx, dy, dst);
        }
    }
}

void compositeBufferOnto(ImageBuffer& out, const ImageBuffer& src, BlendMode mode, float opacity) {
    for (int y = 0; y < out.height(); ++y) {
        for (int x = 0; x < out.width(); ++x) {
            const PixelRGBA8& p = src.getPixel(x, y);
            PixelRGBA8 dst = out.getPixel(x, y);
            compositePixel(dst, p, mode, opacity);
            out.setPixel(x, y, dst);
        }
    }
}

void compositeNodeOnto(ImageBuffer& out, const LayerNode& node, int parentOffsetX, int parentOffsetY) {
    if (node.isLayer()) {
        compositeLayerOnto(out, node.asLayer(), parentOffsetX, parentOffsetY);
        return;
    }

    const LayerGroup& group = node.asGroup();
    if (!group.visible() || group.opacity() <= 0.0f) {
        return;
    }

    ImageBuffer groupSurface(out.width(), out.height(), PixelRGBA8(0, 0, 0, 0));
    const int groupOffsetX = parentOffsetX + group.offsetX();
    const int groupOffsetY = parentOffsetY + group.offsetY();

    for (std::size_t i = 0; i < group.nodeCount(); ++i) {
        compositeNodeOnto(groupSurface, group.node(i), groupOffsetX, groupOffsetY);
    }

    compositeBufferOnto(out, groupSurface, group.blendMode(), group.opacity());
}
} // namespace

ImageBuffer::ImageBuffer() : m_width(0), m_height(0) {}

ImageBuffer::ImageBuffer(int width, int height, const PixelRGBA8& fill) : m_width(width), m_height(height) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("ImageBuffer dimensions must be positive");
    }
    m_pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), fill);
}

int ImageBuffer::width() const {
    return m_width;
}

int ImageBuffer::height() const {
    return m_height;
}

bool ImageBuffer::inBounds(int x, int y) const {
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

const PixelRGBA8& ImageBuffer::getPixel(int x, int y) const {
    if (!inBounds(x, y)) {
        throw std::out_of_range("ImageBuffer pixel out of bounds");
    }
    return m_pixels[pixelIndex(x, y, m_width)];
}

void ImageBuffer::setPixel(int x, int y, const PixelRGBA8& pixel) {
    if (!inBounds(x, y)) {
        return;
    }
    m_pixels[pixelIndex(x, y, m_width)] = pixel;
}

void ImageBuffer::fill(const PixelRGBA8& pixel) {
    std::fill(m_pixels.begin(), m_pixels.end(), pixel);
}

Layer::Layer()
    : m_name("Layer"),
      m_visible(true),
      m_opacity(1.0f),
      m_blendMode(BlendMode::Normal),
      m_offsetX(0),
      m_offsetY(0),
      m_hasMask(false) {}

Layer::Layer(const std::string& name, int width, int height, const PixelRGBA8& fill)
    : m_name(name),
      m_visible(true),
      m_opacity(1.0f),
      m_blendMode(BlendMode::Normal),
      m_offsetX(0),
      m_offsetY(0),
      m_image(width, height, fill),
      m_hasMask(false) {}

const std::string& Layer::name() const {
    return m_name;
}

void Layer::setName(const std::string& name) {
    m_name = name;
}

bool Layer::visible() const {
    return m_visible;
}

void Layer::setVisible(bool visible) {
    m_visible = visible;
}

float Layer::opacity() const {
    return m_opacity;
}

void Layer::setOpacity(float opacity) {
    m_opacity = clamp01(opacity);
}

BlendMode Layer::blendMode() const {
    return m_blendMode;
}

void Layer::setBlendMode(BlendMode mode) {
    m_blendMode = mode;
}

int Layer::offsetX() const {
    return m_offsetX;
}

int Layer::offsetY() const {
    return m_offsetY;
}

void Layer::setOffset(int x, int y) {
    m_offsetX = x;
    m_offsetY = y;
}

bool Layer::hasMask() const {
    return m_hasMask;
}

void Layer::enableMask(const PixelRGBA8& fill) {
    m_mask = ImageBuffer(m_image.width(), m_image.height(), fill);
    m_hasMask = true;
}

void Layer::clearMask() {
    m_mask = ImageBuffer();
    m_hasMask = false;
}

ImageBuffer& Layer::mask() {
    if (!m_hasMask) {
        enableMask();
    }
    return m_mask;
}

const ImageBuffer& Layer::mask() const {
    if (!m_hasMask) {
        throw std::logic_error("Layer mask is not enabled");
    }
    return m_mask;
}

ImageBuffer& Layer::image() {
    return m_image;
}

const ImageBuffer& Layer::image() const {
    return m_image;
}

LayerNode::LayerNode(const Layer& layer) : m_kind(Kind::Layer), m_layer(layer) {}

LayerNode::LayerNode(const LayerGroup& group)
    : m_kind(Kind::Group),
      m_group(std::make_unique<LayerGroup>(group)) {}

LayerNode::LayerNode(const LayerNode& other) : m_kind(other.m_kind), m_layer(other.m_layer) {
    if (other.m_group) {
        m_group = std::make_unique<LayerGroup>(*other.m_group);
    }
}

LayerNode& LayerNode::operator=(const LayerNode& other) {
    if (this == &other) {
        return *this;
    }

    m_kind = other.m_kind;
    m_layer = other.m_layer;
    if (other.m_group) {
        m_group = std::make_unique<LayerGroup>(*other.m_group);
    } else {
        m_group.reset();
    }

    return *this;
}

LayerNode::LayerNode(LayerNode&& other) noexcept = default;

LayerNode& LayerNode::operator=(LayerNode&& other) noexcept = default;

LayerNode::~LayerNode() = default;

LayerNode::Kind LayerNode::kind() const {
    return m_kind;
}

bool LayerNode::isLayer() const {
    return m_kind == Kind::Layer;
}

bool LayerNode::isGroup() const {
    return m_kind == Kind::Group;
}

Layer& LayerNode::asLayer() {
    if (!isLayer()) {
        throw std::logic_error("LayerNode is not a layer");
    }
    return m_layer;
}

const Layer& LayerNode::asLayer() const {
    if (!isLayer()) {
        throw std::logic_error("LayerNode is not a layer");
    }
    return m_layer;
}

LayerGroup& LayerNode::asGroup() {
    if (!isGroup() || !m_group) {
        throw std::logic_error("LayerNode is not a group");
    }
    return *m_group;
}

const LayerGroup& LayerNode::asGroup() const {
    if (!isGroup() || !m_group) {
        throw std::logic_error("LayerNode is not a group");
    }
    return *m_group;
}

LayerGroup::LayerGroup()
    : m_name("Group"),
      m_visible(true),
      m_opacity(1.0f),
      m_blendMode(BlendMode::Normal),
      m_offsetX(0),
      m_offsetY(0) {}

LayerGroup::LayerGroup(const std::string& name)
    : m_name(name),
      m_visible(true),
      m_opacity(1.0f),
      m_blendMode(BlendMode::Normal),
      m_offsetX(0),
      m_offsetY(0) {}

const std::string& LayerGroup::name() const {
    return m_name;
}

void LayerGroup::setName(const std::string& name) {
    m_name = name;
}

bool LayerGroup::visible() const {
    return m_visible;
}

void LayerGroup::setVisible(bool visible) {
    m_visible = visible;
}

float LayerGroup::opacity() const {
    return m_opacity;
}

void LayerGroup::setOpacity(float opacity) {
    m_opacity = clamp01(opacity);
}

BlendMode LayerGroup::blendMode() const {
    return m_blendMode;
}

void LayerGroup::setBlendMode(BlendMode mode) {
    m_blendMode = mode;
}

int LayerGroup::offsetX() const {
    return m_offsetX;
}

int LayerGroup::offsetY() const {
    return m_offsetY;
}

void LayerGroup::setOffset(int x, int y) {
    m_offsetX = x;
    m_offsetY = y;
}

Layer& LayerGroup::addLayer(const Layer& layer) {
    m_nodes.emplace_back(layer);
    return m_nodes.back().asLayer();
}

LayerGroup& LayerGroup::addGroup(const LayerGroup& group) {
    m_nodes.emplace_back(group);
    return m_nodes.back().asGroup();
}

std::size_t LayerGroup::nodeCount() const {
    return m_nodes.size();
}

LayerNode& LayerGroup::node(std::size_t index) {
    return m_nodes.at(index);
}

const LayerNode& LayerGroup::node(std::size_t index) const {
    return m_nodes.at(index);
}

std::size_t LayerGroup::layerCount() const {
    std::size_t count = 0;
    for (const LayerNode& nodeRef : m_nodes) {
        if (nodeRef.isLayer()) {
            ++count;
        }
    }
    return count;
}

Layer& LayerGroup::layer(std::size_t index) {
    std::size_t current = 0;
    for (LayerNode& nodeRef : m_nodes) {
        if (!nodeRef.isLayer()) {
            continue;
        }
        if (current == index) {
            return nodeRef.asLayer();
        }
        ++current;
    }
    throw std::out_of_range("LayerGroup layer index out of range");
}

const Layer& LayerGroup::layer(std::size_t index) const {
    std::size_t current = 0;
    for (const LayerNode& nodeRef : m_nodes) {
        if (!nodeRef.isLayer()) {
            continue;
        }
        if (current == index) {
            return nodeRef.asLayer();
        }
        ++current;
    }
    throw std::out_of_range("LayerGroup layer index out of range");
}

Document::Document(int width, int height) : m_width(width), m_height(height), m_root("Root") {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Document dimensions must be positive");
    }
}

int Document::width() const {
    return m_width;
}

int Document::height() const {
    return m_height;
}

Layer& Document::addLayer(const Layer& layer) {
    return m_root.addLayer(layer);
}

LayerGroup& Document::addGroup(const LayerGroup& group) {
    return m_root.addGroup(group);
}

std::size_t Document::nodeCount() const {
    return m_root.nodeCount();
}

LayerNode& Document::node(std::size_t index) {
    return m_root.node(index);
}

const LayerNode& Document::node(std::size_t index) const {
    return m_root.node(index);
}

std::size_t Document::layerCount() const {
    return m_root.layerCount();
}

Layer& Document::layer(std::size_t index) {
    return m_root.layer(index);
}

const Layer& Document::layer(std::size_t index) const {
    return m_root.layer(index);
}

LayerGroup& Document::rootGroup() {
    return m_root;
}

const LayerGroup& Document::rootGroup() const {
    return m_root;
}

ImageBuffer Document::composite() const {
    ImageBuffer out(m_width, m_height, PixelRGBA8(0, 0, 0, 0));

    for (std::size_t i = 0; i < m_root.nodeCount(); ++i) {
        compositeNodeOnto(out, m_root.node(i), 0, 0);
    }

    return out;
}

ImageBuffer fromRasterImage(const RasterImage& source, std::uint8_t alpha) {
    ImageBuffer out(source.width(), source.height(), PixelRGBA8(0, 0, 0, alpha));
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            const Color& c = source.getPixel(x, y);
            out.setPixel(x, y, PixelRGBA8(c.r, c.g, c.b, alpha));
        }
    }
    return out;
}

void copyToRasterImage(const ImageBuffer& source, RasterImage& destination) {
    if (source.width() != destination.width() || source.height() != destination.height()) {
        throw std::invalid_argument("copyToRasterImage dimensions must match");
    }

    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            const PixelRGBA8& p = source.getPixel(x, y);
            destination.setPixel(x, y, Color(p.r, p.g, p.b));
        }
    }
}

namespace {
constexpr char kIFLOWMagic[8] = {'I', 'F', 'L', 'O', 'W', '0', '1', '\0'};
constexpr std::uint32_t kIFLOWVersion = 1;

template <typename T>
void writeBinary(std::ostream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
    if (!out.good()) {
        throw std::runtime_error("Failed writing IFLOW binary payload");
    }
}

template <typename T>
T readBinary(std::istream& in) {
    T value{};
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    if (!in.good()) {
        throw std::runtime_error("Failed reading IFLOW binary payload");
    }
    return value;
}

std::int32_t blendModeToInt(BlendMode mode) {
    return static_cast<std::int32_t>(mode);
}

BlendMode intToBlendMode(std::int32_t value) {
    if (value < static_cast<std::int32_t>(BlendMode::Normal) ||
        value > static_cast<std::int32_t>(BlendMode::Difference)) {
        throw std::runtime_error("Invalid IFLOW blend mode value");
    }
    return static_cast<BlendMode>(value);
}

void writeString(std::ostream& out, const std::string& value) {
    const std::uint32_t length = static_cast<std::uint32_t>(value.size());
    writeBinary(out, length);
    out.write(value.data(), static_cast<std::streamsize>(length));
    if (!out.good()) {
        throw std::runtime_error("Failed writing IFLOW string");
    }
}

std::string readString(std::istream& in) {
    const std::uint32_t length = readBinary<std::uint32_t>(in);
    std::string value(length, '\0');
    if (length > 0) {
        in.read(value.data(), static_cast<std::streamsize>(length));
        if (!in.good()) {
            throw std::runtime_error("Failed reading IFLOW string");
        }
    }
    return value;
}

void writeImageBuffer(std::ostream& out, const ImageBuffer& image) {
    writeBinary(out, static_cast<std::int32_t>(image.width()));
    writeBinary(out, static_cast<std::int32_t>(image.height()));

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const PixelRGBA8& p = image.getPixel(x, y);
            out.put(static_cast<char>(p.r));
            out.put(static_cast<char>(p.g));
            out.put(static_cast<char>(p.b));
            out.put(static_cast<char>(p.a));
        }
    }

    if (!out.good()) {
        throw std::runtime_error("Failed writing IFLOW image buffer");
    }
}

ImageBuffer readImageBuffer(std::istream& in) {
    const std::int32_t width = readBinary<std::int32_t>(in);
    const std::int32_t height = readBinary<std::int32_t>(in);
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Invalid IFLOW image dimensions");
    }

    ImageBuffer image(width, height, PixelRGBA8(0, 0, 0, 0));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto r = static_cast<std::uint8_t>(in.get());
            const auto g = static_cast<std::uint8_t>(in.get());
            const auto b = static_cast<std::uint8_t>(in.get());
            const auto a = static_cast<std::uint8_t>(in.get());
            if (!in.good()) {
                throw std::runtime_error("Failed reading IFLOW image pixels");
            }
            image.setPixel(x, y, PixelRGBA8(r, g, b, a));
        }
    }
    return image;
}

void writeLayer(std::ostream& out, const Layer& layer) {
    writeString(out, layer.name());
    writeBinary(out, static_cast<std::uint8_t>(layer.visible() ? 1 : 0));
    writeBinary(out, layer.opacity());
    writeBinary(out, blendModeToInt(layer.blendMode()));
    writeBinary(out, static_cast<std::int32_t>(layer.offsetX()));
    writeBinary(out, static_cast<std::int32_t>(layer.offsetY()));
    writeImageBuffer(out, layer.image());
    writeBinary(out, static_cast<std::uint8_t>(layer.hasMask() ? 1 : 0));
    if (layer.hasMask()) {
        writeImageBuffer(out, layer.mask());
    }
}

Layer readLayer(std::istream& in) {
    const std::string name = readString(in);
    const bool visible = readBinary<std::uint8_t>(in) != 0;
    const float opacity = readBinary<float>(in);
    const BlendMode blendMode = intToBlendMode(readBinary<std::int32_t>(in));
    const int offsetX = readBinary<std::int32_t>(in);
    const int offsetY = readBinary<std::int32_t>(in);
    ImageBuffer image = readImageBuffer(in);
    const bool hasMask = readBinary<std::uint8_t>(in) != 0;

    Layer layer(name, image.width(), image.height(), PixelRGBA8(0, 0, 0, 0));
    layer.setVisible(visible);
    layer.setOpacity(opacity);
    layer.setBlendMode(blendMode);
    layer.setOffset(offsetX, offsetY);
    layer.image() = image;

    if (hasMask) {
        ImageBuffer mask = readImageBuffer(in);
        if (mask.width() != image.width() || mask.height() != image.height()) {
            throw std::runtime_error("IFLOW layer mask dimensions do not match layer image");
        }
        layer.enableMask();
        layer.mask() = mask;
    }

    return layer;
}

void writeGroup(std::ostream& out, const LayerGroup& group) {
    writeString(out, group.name());
    writeBinary(out, static_cast<std::uint8_t>(group.visible() ? 1 : 0));
    writeBinary(out, group.opacity());
    writeBinary(out, blendModeToInt(group.blendMode()));
    writeBinary(out, static_cast<std::int32_t>(group.offsetX()));
    writeBinary(out, static_cast<std::int32_t>(group.offsetY()));
    writeBinary(out, static_cast<std::uint32_t>(group.nodeCount()));

    for (std::size_t i = 0; i < group.nodeCount(); ++i) {
        const LayerNode& node = group.node(i);
        if (node.isLayer()) {
            writeBinary(out, static_cast<std::uint8_t>(0));
            writeLayer(out, node.asLayer());
        } else {
            writeBinary(out, static_cast<std::uint8_t>(1));
            writeGroup(out, node.asGroup());
        }
    }
}

LayerGroup readGroup(std::istream& in) {
    LayerGroup group(readString(in));
    group.setVisible(readBinary<std::uint8_t>(in) != 0);
    group.setOpacity(readBinary<float>(in));
    group.setBlendMode(intToBlendMode(readBinary<std::int32_t>(in)));
    group.setOffset(readBinary<std::int32_t>(in), readBinary<std::int32_t>(in));

    const std::uint32_t nodeCount = readBinary<std::uint32_t>(in);
    for (std::uint32_t i = 0; i < nodeCount; ++i) {
        const std::uint8_t nodeType = readBinary<std::uint8_t>(in);
        if (nodeType == 0) {
            group.addLayer(readLayer(in));
        } else if (nodeType == 1) {
            group.addGroup(readGroup(in));
        } else {
            throw std::runtime_error("Invalid IFLOW node type");
        }
    }

    return group;
}
} // namespace

bool saveDocumentIFLOW(const Document& document, const std::string& path) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    try {
        out.write(kIFLOWMagic, sizeof(kIFLOWMagic));
        writeBinary(out, kIFLOWVersion);
        writeBinary(out, static_cast<std::int32_t>(document.width()));
        writeBinary(out, static_cast<std::int32_t>(document.height()));
        writeGroup(out, document.rootGroup());
    } catch (const std::exception&) {
        return false;
    }

    return out.good();
}

Document loadDocumentIFLOW(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        throw std::runtime_error("Failed to open IFLOW file");
    }

    char magic[sizeof(kIFLOWMagic)] = {};
    in.read(magic, sizeof(magic));
    if (!in.good()) {
        throw std::runtime_error("Failed to read IFLOW header");
    }
    for (std::size_t i = 0; i < sizeof(kIFLOWMagic); ++i) {
        if (magic[i] != kIFLOWMagic[i]) {
            throw std::runtime_error("Invalid IFLOW magic");
        }
    }

    const std::uint32_t version = readBinary<std::uint32_t>(in);
    if (version != kIFLOWVersion) {
        throw std::runtime_error("Unsupported IFLOW version");
    }

    const std::int32_t width = readBinary<std::int32_t>(in);
    const std::int32_t height = readBinary<std::int32_t>(in);
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Invalid IFLOW document dimensions");
    }

    Document document(width, height);
    document.rootGroup() = readGroup(in);
    return document;
}
