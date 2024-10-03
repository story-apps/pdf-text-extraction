#pragma once

#include "../math/Transformations.h"

#include <string>
#include <list>

struct ParsedTextPlacement {
    ParsedTextPlacement(
        const std::string& inText,
        const double (&inMatrix)[6],
        const double (&inLocalBox)[4],
        const double (&inGlobalBox)[4],
        const double inSpaceWidth,
        const double (&inGlobalSpaceWidth)[2]
    ) {
        text = inText;
        CopyMatrix(inMatrix, matrix);
        CopyBox(inLocalBox, localBbox);
        CopyBox(inGlobalBox, globalBbox);
        spaceWidth = inSpaceWidth;
        CopyVector(inGlobalSpaceWidth, globalSpaceWidth);
    }

    std::string text;
    double matrix[6];
    double localBbox[4];
    double globalBbox[4];
    double spaceWidth;
    double globalSpaceWidth[2];
};

typedef std::list<ParsedTextPlacement> ParsedTextPlacementList;

enum TextFormat { regular, italic, bold, italicBold };

struct TextParameters {
    bool shouldProcess = false;
    TextFormat currentFormat = TextFormat::regular;
    double constantAlpha = 1;
    struct {
        double red = 0;
        double green = 0;
        double blue = 0;
    } colorRGB;

    void clear()
    {
        currentFormat = TextFormat::regular;
        constantAlpha = 1;
        colorRGB = {};
    }
};

typedef std::list<std::pair<ParsedTextPlacement, TextParameters>> ParsedTextPlacementWithParametersList;

