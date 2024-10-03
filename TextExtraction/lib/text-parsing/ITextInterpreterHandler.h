#pragma once

#include "ParsedTextPlacement.h"

class ITextInterpreterHandler {

public:
    virtual bool OnParsedTextPlacementComplete(const ParsedTextPlacement& inParsedTextPlacement) = 0;
    virtual bool OnParsedTextPlacementCompleteWithParameters(
        const ParsedTextPlacement& inParsedTextPlacement, const TextParameters& inParameters) = 0;
};
