#ifndef CLI_OPS_DRAW_H
#define CLI_OPS_DRAW_H

#include "layer.h"

#include <functional>
#include <string>
#include <unordered_map>

bool tryApplyDrawOperation(
    const std::string& action,
    Document& document,
    const std::unordered_map<std::string, std::string>& kv,
    const std::function<Layer&(Document&, const std::string&)>& resolveLayerPath,
    const std::function<ImageBuffer&(Layer&, const std::unordered_map<std::string, std::string>&)>& resolveDrawTargetBuffer);

#endif
