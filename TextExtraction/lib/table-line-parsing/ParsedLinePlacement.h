#pragma once

#include "../math/Transformations.h"
#include "../graphic-content-parsing/Path.h"

struct ParsedLinePlacement {
    ParsedLinePlacement() {
        isVertical = false;
        ZeroVector(globalPointOne);
        ZeroVector(globalPointTwo);
        ZeroVector(effectiveLineWidth);
        ZeroVector(colorRGB);
    }

    ParsedLinePlacement(
        bool inIsVertical,
        const double (&inGlobalPointOne)[2],
        const double (&inGlobalPointTwo)[2],
        const double (&inEffectiveLineWidth)[2],
        const double (&inColorRGB)[3] = {}
    ) {
        isVertical = inIsVertical;
        CopyVector(inGlobalPointOne, globalPointOne);
        CopyVector(inGlobalPointTwo, globalPointTwo);
        CopyVector(inEffectiveLineWidth, effectiveLineWidth);
        CopyVector(inColorRGB, colorRGB);
    }

    bool isVertical;

	// in vertical lines, globalPointOne is top, and globalPointTwo is bottom
	// in horizontal lines, globalPointOne is left, and globalPointTwo is right
    double globalPointOne[2];
    double globalPointTwo[2];
    // width having the matrix applied.
    double effectiveLineWidth[2];

    double colorRGB[3];
};
