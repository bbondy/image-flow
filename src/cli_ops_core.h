#ifndef CLI_OPS_CORE_H
#define CLI_OPS_CORE_H

#include "layer.h"

#include <functional>
#include <string>

void applyDocumentOperation(Document& document,
                            const std::string& opSpec,
                            const std::function<void(const std::string&)>& emitOutput);

#endif
