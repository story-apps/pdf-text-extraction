#include "TableExtraction.h"

#include "InputFile.h"
#include "PDFParser.h"
#include "PDFWriter.h"
#include "PDFPageInput.h"


#include "./lib/interpreter/PDFRecursiveInterpreter.h"
#include "./lib/graphic-content-parsing/GraphicContentInterpreter.h"
#include "./lib/table-csv-export/TableCSVExport.h"
#include "./lib/table-composition/TableComposer.h"

#include <QTextCursor>


using namespace std;
using namespace PDFHummus;


TableExtraction::TableExtraction():
    textInterpeter(this), 
    tableLineInterpreter(this)
{

}
    
TableExtraction::~TableExtraction() {
    textsForPages.clear();
    tableLinesForPages.clear();
    tablesForPages.clear();
    mediaBoxesForPages.clear();
}

bool TableExtraction::OnParsedHorizontalLinePlacementComplete(const ParsedLinePlacement& inParsedLine) {
    tableLinesForPages.back().horizontalLines.push_back(inParsedLine);
    return true;
}

bool TableExtraction::OnParsedVerticalLinePlacementComplete(const ParsedLinePlacement& inParsedLine) {
    tableLinesForPages.back().verticalLines.push_back(inParsedLine);
    return true;
}


bool TableExtraction::OnParsedTextPlacementComplete(const ParsedTextPlacement& inParsedTextPlacement) {
    textsForPages.back().push_back(inParsedTextPlacement);
    return true;
}

bool TableExtraction::OnParsedTextPlacementCompleteWithParameters(
    const ParsedTextPlacement& inParsedTextPlacement, const TextParameters& inParameters)
{
    textsForPagesWithParameters.back().push_back(
        std::pair<ParsedTextPlacement, TextParameters>(inParsedTextPlacement, inParameters));
    return true;
}

bool TableExtraction::OnTextElementComplete(const TextElement& inTextElement) {
    return textInterpeter.OnTextElementComplete(inTextElement);
}

bool TableExtraction::OnTextElementCompleteWithParameters(const TextElement& inTextElement, const TextParameters& inParameters) {
    return textInterpeter.OnTextElementCompleteWithParameters(inTextElement, inParameters);
}

bool TableExtraction::OnPathPainted(const PathElement& inPathElement) {
    return tableLineInterpreter.OnPathPainted(inPathElement);
}

bool TableExtraction::OnResourcesRead(const Resources& inResources, IInterpreterContext* inContext) {
    return textInterpeter.OnResourcesRead(inResources, inContext);
}

EStatusCode TableExtraction::ExtractTablePlacements(PDFParser* inParser, long inStartPage, long inEndPage, bool inForQTextDocument) {
    EStatusCode status = eSuccess;
    unsigned long start = (unsigned long)(inStartPage >= 0 ? inStartPage : (inParser->GetPagesCount() + inStartPage));
    unsigned long end = (unsigned long)(inEndPage >= 0 ? inEndPage :  (inParser->GetPagesCount() + inEndPage));
    GraphicContentInterpreter interpreter;


    if(end > inParser->GetPagesCount()-1)
        end = inParser->GetPagesCount()-1;
    if(start > end)
        start = end;

    for(unsigned long i=start;i<=end && status == eSuccess;++i) {
        RefCountPtr<PDFDictionary> pageObject(inParser->ParsePage(i));
        if(!pageObject) {
            status = eFailure;
            break;
        }

        PDFPageInput pageInput(inParser,pageObject);

        mediaBoxesForPages.push_back(pageInput.GetMediaBox());
        textsForPages.push_back(ParsedTextPlacementList());
        textsForPagesWithParameters.push_back(ParsedTextPlacementWithParametersList());
        tableLinesForPages.push_back(Lines());
        // the interpreter will trigger the textInterpreter which in turn will trigger this object to collect text elements
        interpreter.InterpretPageContents(inParser, pageObject.GetPtr(), this, inForQTextDocument);
    }    

    textInterpeter.ResetInterpretationState();

    return status;
}

static const string scEmpty = "";

EStatusCode TableExtraction::ExtractTables(const std::string& inFilePath, long inStartPage, long inEndPage, bool inForQTextDocument) {
    EStatusCode status = eSuccess;
    InputFile sourceFile;

    LatestWarnings.clear();
    LatestError.code = eErrorNone;
    LatestError.description = scEmpty;

    textsForPages.clear();
    tableLinesForPages.clear();
    tablesForPages.clear();
    mediaBoxesForPages.clear();

    do {
        status = sourceFile.OpenFile(inFilePath);
        if (status != eSuccess) {
            LatestError.code = eErrorFileNotReadable;
            LatestError.description = string("Cannot read file ") + inFilePath;
            break;
        }


        PDFParser parser;
        status = parser.StartPDFParsing(sourceFile.GetInputStream());
        if(status != eSuccess)
        {
            LatestError.code = eErrorInternalPDFWriter;
            LatestError.description = string("Failed to parse file");
            break;
        }

        status = ExtractTablePlacements(&parser, inStartPage, inEndPage, inForQTextDocument);
        if(status != eSuccess)
            break;

        if (!inForQTextDocument) {
            ComposeTables();
        }
    } while(false);

    return status;
}


void TableExtraction::ComposeTables() {
    TableComposer tableComposer;
    ParsedTextPlacementListList::iterator itTextsforPages = textsForPages.begin();
    LinesList::iterator itTablesLinesForPages = tableLinesForPages.begin();
    PDFRectangleList::iterator itMediaBoxForPages = mediaBoxesForPages.begin();

    // iterate the pages (lists are supposed to be synced)
    for(; itTextsforPages != textsForPages.end() &&  
            itTablesLinesForPages != tableLinesForPages.end() && 
            itMediaBoxForPages != mediaBoxesForPages.end(); 
            ++itTextsforPages, ++itTablesLinesForPages, ++itMediaBoxForPages) {
        double pageScopeBox[4] ={itMediaBoxForPages->LowerLeftX, itMediaBoxForPages->LowerLeftY, itMediaBoxForPages->UpperRightX, itMediaBoxForPages->UpperRightY};
        tablesForPages.push_back(tableComposer.ComposeTables(*itTablesLinesForPages, *itTextsforPages, pageScopeBox));
    }
}

static const string scCRLN = "\r\n";

string TableExtraction::GetTableAsCSVText(const Table& inTable, int bidiFlag, TextComposer::ESpacing spacingFlag) {
    TableCSVExport exporter(bidiFlag, spacingFlag);
    exporter.ComposeTableText(inTable);
    return exporter.GetText();  
}

string TableExtraction::GetAllAsCSVText(int bidiFlag, TextComposer::ESpacing spacingFlag) {
    TableCSVExport exporter(bidiFlag, spacingFlag);

    TableListList::iterator itPages = tablesForPages.begin();

    for(; itPages != tablesForPages.end(); ++itPages) {
        TableList::iterator itTables = itPages->begin();
        for(; itTables != itPages->end(); ++itTables) {
            exporter.ComposeTableText(*itTables);
            exporter.AppendText(scCRLN); // two newlines to separate tables on the same page
            exporter.AppendText(scCRLN);
        }
        exporter.AppendText(scCRLN); // 4 newlines to separate pages
        exporter.AppendText(scCRLN);
        exporter.AppendText(scCRLN);
        exporter.AppendText(scCRLN);
    }

    return exporter.GetText();
}

void TableExtraction::GetResultsAsDocument(QTextDocument& inDocument)
{
    QTextCursor cursor(&inDocument);
    cursor.beginEditBlock();

    TextComposer composer(0, TextComposer::eSpacingHorizontal);
    ParsedTextPlacementWithParametersListList::iterator itTextsforPages
        = textsForPagesWithParameters.begin();
    LinesList::iterator itTablesLinesForPages = tableLinesForPages.begin();
    PDFRectangleList::iterator itMediaBoxForPages = mediaBoxesForPages.begin();

    for (; itTextsforPages != textsForPagesWithParameters.end()
         && itTablesLinesForPages != tableLinesForPages.end()
         && itMediaBoxForPages != mediaBoxesForPages.end();
         ++itTextsforPages, ++itTablesLinesForPages, ++itMediaBoxForPages) {
        composer.ComposeDocument(*itTextsforPages, *itMediaBoxForPages, *itTablesLinesForPages,
                                 cursor);
    }
    cursor.endEditBlock();
}
