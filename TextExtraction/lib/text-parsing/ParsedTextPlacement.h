#pragma once

#include "../math/Transformations.h"

#include <string>
#include <list>


enum TextFormat { regular, italic, bold, italicBold };

struct TextParameters {
    TextFormat format = TextFormat::regular;
    double constantAlpha = 1;

    void clear()
    {
        format = TextFormat::regular;
        constantAlpha = 1;
    }
};

struct ParsedTextPlacement {
    ParsedTextPlacement(
        const std::string& inText,
        const double (&inMatrix)[6],
        const double (&inLocalBox)[4],
        const double (&inGlobalBox)[4],
        const double inSpaceWidth,
        const double (&inGlobalSpaceWidth)[2],
        const TextParameters& inParameters = TextParameters()
    ) {
        text = inText;
        CopyMatrix(inMatrix, matrix);
        CopyBox(inLocalBox, localBbox);
        CopyBox(inGlobalBox, globalBbox);
        spaceWidth = inSpaceWidth;
        CopyVector(inGlobalSpaceWidth, globalSpaceWidth);
        parameters = inParameters;
    }

    std::string text;
    double matrix[6];
    double localBbox[4];
    double globalBbox[4];
    double spaceWidth;
    double globalSpaceWidth[2];
    TextParameters parameters;
};

typedef std::list<ParsedTextPlacement> ParsedTextPlacementList;

