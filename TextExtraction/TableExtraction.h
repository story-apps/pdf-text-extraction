#pragma once

#include "EStatusCode.h"
#include "PDFRectangle.h"

#include "./lib/text-parsing/ParsedTextPlacement.h"
#include "./lib/text-parsing/ITextInterpreterHandler.h"
#include "./lib/text-composition/TextComposer.h"
#include "./lib/graphic-content-parsing/IGraphicContentInterpreterHandler.h"
#include "./lib/text-parsing/TextInterpreter.h"
#include "./lib/table-line-parsing/TableLineInterpreter.h"
#include "./lib/table-line-parsing/ITableLineInterpreterHandler.h"
#include "./lib/table-composition/Lines.h"
#include "./lib/table-composition/Table.h"

#include "ErrorsAndWarnings.h"

class PDFParser;
class QTextDocument;

#include <sstream>
#include <string>
#include <list>

typedef std::list<ParsedTextPlacementList> ParsedTextPlacementListList;
typedef std::list<ParsedTextPlacementWithFormatList> ParsedTextPlacementWithFormatListList;
typedef std::list<TableList> TableListList;
typedef std::list<ExtractionWarning> ExtractionWarningList;
typedef std::list<PDFRectangle> PDFRectangleList;

class TableExtraction : public ITextInterpreterHandler, IGraphicContentInterpreterHandler, ITableLineInterpreterHandler {

    public:
        TableExtraction();
        virtual ~TableExtraction();

        PDFHummus::EStatusCode ExtractTables(const std::string& inFilePath, long inStartPage=0, long inEndPage=-1, bool inForQTextDocumentt = false);

        ExtractionError LatestError;
        ExtractionWarningList LatestWarnings;  

        TableListList tablesForPages;

        // IGraphicContentInterpreterHandler implementation
        virtual bool OnTextElementComplete(const TextElement& inTextElement);
        virtual bool OnTextElementCompleteWithFormats(const TextElement& inTextElement, TextFormat inFormat);
        virtual bool OnPathPainted(const PathElement& inPathElement);
        virtual bool OnResourcesRead(const Resources& inResources, IInterpreterContext* inContext);

        // ITextInterpreterHandler implementation OnParsedTextPlacementCompleteWithFormat
        virtual bool OnParsedTextPlacementComplete(const ParsedTextPlacement& inParsedTextPlacement); 
        virtual bool OnParsedTextPlacementCompleteWithFormat(const ParsedTextPlacement& inParsedTextPlacement, TextFormat inFormat);

        // ITableLineInterpreterHandler implementation
        virtual bool OnParsedHorizontalLinePlacementComplete(const ParsedLinePlacement& inParsedLine); 
        virtual bool OnParsedVerticalLinePlacementComplete(const ParsedLinePlacement& inParsedLine); 

        std::string GetTableAsCSVText(const Table& inTable, int bidiFlag, TextComposer::ESpacing spacingFlag);
        std::string GetAllAsCSVText(int bidiFlag, TextComposer::ESpacing spacingFlag);
        void GetResultsAsDocument(QTextDocument& inDocument);

    private:
        TextInterpeter textInterpeter;
        TableLineInterpreter tableLineInterpreter;

        ParsedTextPlacementListList textsForPages;
        ParsedTextPlacementWithFormatListList textsForPagesWithFormats;
        LinesList tableLinesForPages;
        PDFRectangleList mediaBoxesForPages;


        PDFHummus::EStatusCode ExtractTablePlacements(PDFParser* inParser, long inStartPage, long inEndPage, bool inForQTextDocumentt = false);
        void ComposeTables();
        
};
