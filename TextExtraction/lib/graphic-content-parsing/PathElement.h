#pragma once

#include "ContentGraphicState.h"
#include "Path.h"

enum EFillMethod {
    eFillMethodNonZeroWindingNumberRule,
    eFillMethodEventOddRule,
};

struct PathElement {
    PathElement(const Path& inPath = Path(),
                const ContentGraphicState& inGraphicState = ContentGraphicState(),
                const double (&inColorRGB)[3] = {}, bool inShouldStroke = false,
                bool inShouldFill = false, const EFillMethod& inFillMethod = EFillMethod())
        : path(inPath)
        , graphicState(inGraphicState)
        , colorRGB{ inColorRGB[0], inColorRGB[1], inColorRGB[2] }
        , shouldStroke(inShouldStroke)
        , shouldFill(inShouldFill)
        , fillMethod(inFillMethod)
    {
    }

    Path path;
    ContentGraphicState graphicState;
    double colorRGB[3] = {};

    bool shouldStroke;
    bool shouldFill;
    EFillMethod fillMethod;
};
