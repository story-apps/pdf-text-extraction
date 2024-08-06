#pragma once

#include "EStatusCode.h"

#include "./lib/text-parsing/ParsedTextPlacement.h"
#include "./lib/text-parsing/ITextInterpreterHandler.h"
#include "./lib/text-composition/TextComposer.h"
#include "./lib/graphic-content-parsing/IGraphicContentInterpreterHandler.h"
#include "./lib/text-parsing/TextInterpreter.h"

#include "ErrorsAndWarnings.h"

class PDFParser;

#include <sstream>
#include <string>
#include <list>

typedef std::list<ParsedTextPlacementList> ParsedTextPlacementListList;
typedef std::list<ParsedTextPlacementWithFormatList> ParsedTextPlacementWithFormatListList;
typedef std::list<ExtractionWarning> ExtractionWarningList;


class TextExtraction : public ITextInterpreterHandler, IGraphicContentInterpreterHandler {

    public:
        TextExtraction();
        virtual ~TextExtraction();

        PDFHummus::EStatusCode ExtractText(const std::string& inFilePath, long inStartPage=0, long inEndPage=-1);
        PDFHummus::EStatusCode ExtractTextWithFormats(const std::string& inFilePath, long inStartPage=0, long inEndPage=-1);

        ExtractionError LatestError;
        ExtractionWarningList LatestWarnings;

        // end result construct
        ParsedTextPlacementListList textsForPages;
        ParsedTextPlacementWithFormatListList textsForPagesWithFormats;

        // just descrypt input file to its easier to read its contnets
        PDFHummus::EStatusCode DecryptPDFForDebugging(
            const std::string& inTemplateFilePath,
            const std::string& inTargetOutputFilePath
        );

        std::string GetResultsAsText(int bidiFlag, TextComposer::ESpacing spacingFlag);
        std::string GetResultsAsTextWithFormats(int bidiFlag, TextComposer::ESpacing spacingFlag);

        // IGraphicContentInterpreterHandler implementation
        virtual bool OnTextElementComplete(const TextElement& inTextElement);
        virtual bool OnTextElementCompleteWithFormats(const TextElement& inTextElement, TextFormat inFormat);
        virtual bool OnPathPainted(const PathElement& inPathElement);
        virtual bool OnResourcesRead(const Resources& inResources, IInterpreterContext* inContext);

        // ITextInterpreterHandler implementation
        virtual bool OnParsedTextPlacementComplete(const ParsedTextPlacement& inParsedTextPlacement); 
        virtual bool OnParsedTextPlacementCompleteWithFormat(const ParsedTextPlacement& inParsedTextPlacement, TextFormat format);

    private:
        TextInterpeter textInterpeter;
        double currentPageScopeBox[4];

        PDFHummus::EStatusCode ExtractTextPlacements(PDFParser* inParser, long inStartPage, long inEndPage);
        PDFHummus::EStatusCode ExtractTextPlacementsWithFormats(PDFParser* inParser, long inStartPage, long inEndPage);
};
