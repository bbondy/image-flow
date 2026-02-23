#ifndef CLI_OPS_RESOLVE_H
#define CLI_OPS_RESOLVE_H

#include "layer.h"

#include <string>
#include <unordered_map>

LayerGroup& resolveGroupPath(Document& document, const std::string& path);
LayerNode& resolveNodePath(Document& document, const std::string& path);
Layer& resolveLayerPath(Document& document, const std::string& path);
ImageBuffer& resolveDrawTargetBuffer(Layer& layer, const std::unordered_map<std::string, std::string>& kv);

#endif
