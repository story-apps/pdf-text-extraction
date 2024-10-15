#include "TextComposer.h"


#include "../bidi/BidiConversion.h"
#include "../table-composition/Lines.h"

#include <algorithm>
#include <vector>
#include <math.h>
#include <set>

#include <QColor>
#include <QRegularExpression>
#include <QString>
#include <QTextBlock>
#include <QTextCursor>

using namespace std;

typedef std::vector<ParsedTextPlacement> ParsedTextPlacementVector;

static const string scEmpty = "";
static const char scSpace = ' ';
static const string scCRLN = "\r\n";
static const double scPageTopPartCoefficient = 9 / 10.0;
static const double scPageBottomPartCoefficient = 1 / 10.0;
static const double scPageLeftPartCoefficient = 1 / 5.0;
static const double scPageRightPartCoefficient = 4 / 5.0;
static const double scConstantAlphaMin = 0.001;
static const double scConstantAlphaMax = 0.999;
static const double scColorMax = 0.99;

/**
 * @brief Цвет в модели RGB
 */
struct ColorRGB {
    double red = 1;
    double green = 1;
    double blue = 1;
};

/**
 * @brief Является ли цвет белым
 */
static bool IsWhite(const double (&inColor)[3])
{
    return inColor[0] > scColorMax && inColor[1] > scColorMax && inColor[2] > scColorMax;
}

/**
 * @brief Строка текста с форматами и цветом заливки
 */
struct FormatString {
    FormatString(const QString& inText,
                 const std::set<TextFormat>& inFormats = std::set<TextFormat>(),
                 const ColorRGB& inColor = ColorRGB())
        : text(inText)
        , formats(inFormats)
        , backgroundColor(inColor)
    {
    }
    FormatString(const ParsedTextPlacement& inTextPlacements, const Lines& inPageLines);

    QString text;
    std::set<TextFormat> formats;
    ColorRGB backgroundColor;
};

FormatString::FormatString(const ParsedTextPlacement& inTextPlacements, const Lines& inPageLines)
    : text(QString::fromStdString(inTextPlacements.text))
    , formats(inTextPlacements.parameters.formats)
{
    static constexpr int edgeCoef = 10;
    static constexpr int zeroWidthCoef = 10;
    static constexpr int minWidthCoef = 3;
    static constexpr int thinCoef = 4;
    static constexpr int textBottomCoef = 4;
    static constexpr int underlineCoef = 5;

    //
    // Немного корректируем границы текста, чтобы не учитывались соседние линии,
    // и учитывем случай, когда ширина текста равна нулю
    //
    const double textHeight = inTextPlacements.globalBbox[3] - inTextPlacements.globalBbox[1];
    const double textLeftEdge = inTextPlacements.globalBbox[0] + textHeight / edgeCoef;
    const double textRightEdge = (inTextPlacements.globalBbox[2] - inTextPlacements.globalBbox[0])
            > textHeight / zeroWidthCoef
        ? inTextPlacements.globalBbox[2] - textHeight / edgeCoef
        : inTextPlacements.globalBbox[2] + textHeight / minWidthCoef;

    for (const auto& itLines : inPageLines.horizontalLines) {
        const bool lineAtTheTextHeight = itLines.globalPointOne[1] >= inTextPlacements.globalBbox[1]
            && itLines.globalPointOne[1] <= inTextPlacements.globalBbox[3];
        const bool lineAtTheTextDistance = itLines.globalPointOne[0] <= textLeftEdge
            && itLines.globalPointTwo[0] >= textRightEdge;
        const bool lineIsWide = itLines.effectiveLineWidth[0]
                > (inTextPlacements.globalBbox[3] - inTextPlacements.globalBbox[1])
            && itLines.effectiveLineWidth[1]
                > (inTextPlacements.globalBbox[3] - inTextPlacements.globalBbox[1]);
        const bool lineIsThin = itLines.effectiveLineWidth[0] < textHeight / 4
            && itLines.effectiveLineWidth[1] < textHeight / thinCoef;
        const bool lineAtTextBottom
            = itLines.globalPointOne[1] - inTextPlacements.globalBbox[1] < textHeight / textBottomCoef;
        const bool lineUnderText = itLines.globalPointOne[1] < inTextPlacements.globalBbox[1]
            && itLines.globalPointOne[1] > inTextPlacements.globalBbox[1] - textHeight / underlineCoef;

        if (!IsWhite(itLines.colorRGB)) {
            if (lineIsWide && lineAtTheTextHeight && lineAtTheTextDistance) {
                backgroundColor.red = itLines.colorRGB[0];
                backgroundColor.green = itLines.colorRGB[1];
                backgroundColor.blue = itLines.colorRGB[2];
            } else if (lineIsThin && (lineAtTheTextHeight || lineUnderText)
                       && lineAtTheTextDistance) {
                if (lineAtTextBottom || lineUnderText) {
                    formats.insert(TextFormat::Underline);
                } else {
                    formats.insert(TextFormat::Strikeout);
                }
            }
        }
    }
}

/**
 * @brief Параметры страницы
 */
struct PageParameters {
    PDFRectangle mediaBox;
    double minLeftMargin = 0;
    double minRightMargin = 0; // считается от левого края
    double headerLinePosition = 0;
    double footerLinePosition = 0;
    double generalTextSize = 0;
};

/**
 * @brief Бокс параграфа с боксами строк
 */
struct ParagraphBox {
    double box[4];
    struct Line {
        double box[4];
    };
    QList<Line> lines;

    void clear()
    {
        lines.clear();
        for (int i = 0; i != 4; ++i) {
            box[i] = 0;
        }
    }
};

struct TextItems {
    double height = 0;
    double length = 0;
    unsigned count = 0;

    TextItems(double inHeight, double inLength, double inCount)
        : height(inHeight)
        , length(inLength)
        , count(inCount)
    {
    }
    bool operator<(const TextItems& other) const
    {
        return height < other.height;
    }
};


void UnionLeftBoxToRight(const double (&inLeftBox)[4], double (&refRightBox)[4]) {
    // union left box to right box resulting in a box that contains both

    if(inLeftBox[0] < refRightBox[0])
        refRightBox[0] = inLeftBox[0];
    if(inLeftBox[1] < refRightBox[1])
        refRightBox[1] = inLeftBox[1];
    if(inLeftBox[2] > refRightBox[2])
        refRightBox[2] = inLeftBox[2];
    if(inLeftBox[3] > refRightBox[3])
        refRightBox[3] = inLeftBox[3];
}

double BoxHeight(const double (&inBox)[4]) {
    return inBox[3] - inBox[1];
}

double BoxWidth(const double (&inBox)[4]) {
    return inBox[2] - inBox[0];
}

double BoxTop(const double (&inBox)[4]) {
    return inBox[3];
}

double BoxBottom(const double (&inBox)[4]) {
    return inBox[1];
}


TextComposer::TextComposer(int inBidiFlag, ESpacing inSpacingFlag) {
    bidiFlag = inBidiFlag;
    spacingFlag = inSpacingFlag;
}

TextComposer::~TextComposer() {

}

const double LINE_HEIGHT_THRESHOLD = 5;

int GetOrientationCode(const ParsedTextPlacement& a) {
    // a very symplistic heuristics to try and logically group different text orientations in a way that makes sense

    // 1 0 0 1
    if(a.matrix[0] > 0 && a.matrix[3] > 0)
        return 0;

    // 0 1 -1 0
    if(a.matrix[1] > 0 && a.matrix[2] < 0)
        return 1;

    // -1 0 0 -1
    if(a.matrix[0] < 0 && a.matrix[3] < 0)
        return 2;

    // 0 -1 1 0 or other
    return 3;
}

bool CompareForOrientation(const ParsedTextPlacement& a, const ParsedTextPlacement& b, int code) {
    if(code == 0) {
        if(abs(a.globalBbox[1] - b.globalBbox[1]) > LINE_HEIGHT_THRESHOLD)
            return b.globalBbox[1] < a.globalBbox[1];
        else
            return a.globalBbox[0] < b.globalBbox[0];

    } else if(code == 1) {
        if(abs(a.globalBbox[0] - b.globalBbox[0]) > LINE_HEIGHT_THRESHOLD)
            return a.globalBbox[0] <  b.globalBbox[0];
        else
            return a.globalBbox[1] < b.globalBbox[1];

    } else if(code == 2) {
        if(abs(a.globalBbox[1] - b.globalBbox[1]) > LINE_HEIGHT_THRESHOLD)
            return a.globalBbox[1] < b.globalBbox[1];
        else
            return b.globalBbox[0] < a.globalBbox[0];

    } else {
        // code 3
        if(abs(a.globalBbox[0] - b.globalBbox[0]) > LINE_HEIGHT_THRESHOLD)
            return b.globalBbox[0] < a.globalBbox[0];
        else
            return b.globalBbox[1] < a.globalBbox[1];
    }

}

bool CompareParsedTextPlacement(const ParsedTextPlacement& a, const ParsedTextPlacement& b) {
    int codeA = GetOrientationCode(a);
    int codeB = GetOrientationCode(b);

    if(codeA == codeB) {
        return CompareForOrientation(a,b,codeA);
    }

    return codeA < codeB;
}

bool AreSameLine(const ParsedTextPlacement& a, const ParsedTextPlacement& b) {
    int codeA = GetOrientationCode(a);
    int codeB = GetOrientationCode(b);

    if(codeA != codeB)
        return false;

    if(codeA == 0 || codeA == 2) {
        return abs(a.globalBbox[1] - b.globalBbox[1]) <= LINE_HEIGHT_THRESHOLD
                && abs(a.globalBbox[3] - b.globalBbox[3]) <= LINE_HEIGHT_THRESHOLD;
    } else {
        return abs(a.globalBbox[0] - b.globalBbox[0]) <= LINE_HEIGHT_THRESHOLD;
    }
}

unsigned long GuessHorizontalSpacingBetweenPlacements(const ParsedTextPlacement& left, const ParsedTextPlacement& right) {
    double leftTextRightEdge = left.globalBbox[2];
    double rightTextLeftEdge = right.globalBbox[0];

    if(leftTextRightEdge > rightTextLeftEdge)
        return 0; // left text is overflowing into right text

    double distance = rightTextLeftEdge - leftTextRightEdge;
    double spaceWidth = left.globalSpaceWidth[0];

    if(spaceWidth == 0 && BoxWidth(left.globalBbox) > 0) {
        // if no available space width from font info, try to evaluate per the left string width/char length...not the best...but
        // easy.
        spaceWidth = BoxWidth(left.globalBbox) / left.text.length();
    }

    if(spaceWidth == 0)
        return 0; // protect from 0 errors

    return (unsigned long)floor(distance/spaceWidth);
}

/**
 * @brief Добавить границы строки к границам параграфа
 */
static void AddLineToParagraphBox(const double (&inNewLineBox)[4], ParagraphBox& outParagraph)
{
    if (outParagraph.lines.empty()) {
        CopyBox(inNewLineBox, outParagraph.box);
    } else {
        // нижняя граница
        outParagraph.box[1] = inNewLineBox[1];
        // левая
        if (inNewLineBox[0] < outParagraph.box[0]) {
            outParagraph.box[0] = inNewLineBox[0];
        }
        // правая
        if (inNewLineBox[2] > outParagraph.box[2]) {
            outParagraph.box[2] = inNewLineBox[2];
        }
    }
    ParagraphBox::Line line;
    CopyBox(inNewLineBox, line.box);
    outParagraph.lines.append(line);
}

/**
 * @brief Принадлежит ли текстовый итем новому параграфу
 */
static bool IsNewParagraph(const double (&inPreviousLineBox)[4], const double (&inNewLineBox)[4],
                           const PageParameters& inPageParameters)
{
    const double textHeight = BoxHeight(inNewLineBox);
    const double maxTextWidth = inPageParameters.minRightMargin - inPageParameters.minLeftMargin;
    const double spaceBtwLines = inPreviousLineBox[1] - inNewLineBox[3];
    const double diffLeftIndent = inPreviousLineBox[0] - inNewLineBox[0];
    const double diffRightIndent = inPreviousLineBox[2] - inNewLineBox[2];
    const double previousLineLeftMargin = inPreviousLineBox[0] - inPageParameters.minLeftMargin;
    const double previousLineRightMargin = inPageParameters.minRightMargin - inPreviousLineBox[2];
    const double newLineLeftMargin = inNewLineBox[0] - inPageParameters.minLeftMargin;

    const bool onRightSide
        = inNewLineBox[2] - inPageParameters.mediaBox.UpperRightX * scPageRightPartCoefficient > 0
        && inPreviousLineBox[2] - inPageParameters.mediaBox.UpperRightX * scPageRightPartCoefficient
            > 0;

    //
    // Считаем, что новый параграф, если:
    // 1. у строк нет отступа слева и при этом отступ справа у предыдущей строки больше трети ширины
    // текста
    //
    const bool newParagraph = newLineLeftMargin < 1 && previousLineLeftMargin < 1
        && previousLineRightMargin > maxTextWidth / 3;

    //
    // Считаем, что тот же параграф, если:
    // 1. одинаковые отступы слева  и нет отступа между строк
    //    (т.е. расстояние между строками * 1.5 < высоты текста)
    // 2. или одинаковые отступы справа, нет отступа между строк и края строк в правой части
    //
    const bool sameParagraph = (abs(diffLeftIndent) < 10 && spaceBtwLines * 1.5 < textHeight)
        || (abs(diffRightIndent) < 1 && spaceBtwLines < textHeight && onRightSide);

    return newParagraph || !sameParagraph;
}

/**
 * @brief Установить формат для предыдущего блока текста
 */
static void SetFormatToPreviousBlock(const ParagraphBox& inParagraph,
                                     const PageParameters& inPageParameters,
                                     double inPreviousParagraphBottom, QTextCursor& inCursor)
{
    QTextBlockFormat format;

    //
    // Отступ слева
    //
    qreal leftMargin = inParagraph.box[0] - inPageParameters.minLeftMargin;
    format.setLeftMargin(leftMargin);

    //
    // Отступ сверху
    // Считаем как количество строк, которые могут поместиться между параграфами
    //
    double textHeight = BoxHeight(inParagraph.lines[0].box);
    int topMargin = (inPreviousParagraphBottom - inParagraph.box[3]) / textHeight;
    format.setTopMargin(topMargin);

    //
    // Выравнивание
    // Проверяем только выравнивание по правому краю - если не оно, то считаем что по левому
    //
    bool alignRight = true;

    //
    //  Сначала смотрим, где находится правая граница первой строки
    //
    bool onRightSide = inParagraph.lines[0].box[2]
            - inPageParameters.mediaBox.UpperRightX * scPageRightPartCoefficient > 0;
    if (onRightSide) {
        //
        // ... если в правой части листа
        //
        if (inParagraph.lines.count() > 1) {
            //
            // ... и если в параграфе больше одной строки, то определяем по положению строк
            // относительно первой строки
            //
            auto itLines = inParagraph.lines.begin();
            const auto firstLine = itLines;
            ++itLines;
            for (; alignRight && itLines != inParagraph.lines.end(); ++itLines) {
                if (abs(itLines->box[2] - firstLine->box[2]) > 1) {
                    alignRight = false;
                }
            }
        } else {
            //
            // ... если одна строка, то определяем по положению относительно правой границы всего
            // текста
            //
            if (inPageParameters.minRightMargin - inParagraph.lines[0].box[2] > 1) {
                alignRight = false;
            }
        }
    } else {
        //
        // ... если не в правой части листа, то считаем, что выравнивание идет по левому краю
        //
        alignRight = false;
    }

    if (alignRight) {
        format.setAlignment(Qt::AlignRight);
    } else {
        format.setAlignment(Qt::AlignLeft);
    }

    inCursor.setBlockFormat(format);
}

/**
 * @brief Извлечь текст
 * @return Строка извлеченного текста
 */
static std::string ExtractText(const QList<FormatString>& inLineText)
{
    std::string text;
    for (auto itText = inLineText.constBegin(); itText != inLineText.constEnd(); ++itText) {
        text.append(itText->text.toStdString());
    }
    return text;
}

/**
 * @brief Является ли строка номером
 */
static bool IsNumber(const std::string& inText)
{
    QRegularExpression re("^\\d{1,}\\s{0,}\\Z");
    QRegularExpressionMatch match = re.match(QString::fromStdString(inText));
    return match.hasMatch();
}

/**
 * @brief Является ли строка точкой или двоеточием
 */
static bool IsDotOrColon(const std::string& inText)
{
    return inText == "." || inText == ":";
}

/**
 * @brief Является ли строка номером с точкой
 */
static bool IsNumberAndDot(const std::string& inText)
{
    QRegularExpression re("^\\d{1,}\\.{1,1}\\s{0,}\\Z");
    QRegularExpressionMatch match = re.match(QString::fromStdString(inText));
    return match.hasMatch();
}

/**
 * @brief Является ли строка пустой
 * @note Без учета пробельных символов
 */
static bool isEmptyString(const std::string& inString)
{
    return QString::fromStdString(inString).simplified().isEmpty();
}

/**
 * @brief Минимальный отступ слева
 */
static double MinLeftMargin(const ParsedTextPlacementVector& inTextPlacements,
                            const PDFRectangle& inMediaBox)
{
    //
    // Изначально берем максимально допустимый левый отступ
    // на случай, если на странице только текст посередине
    //
    double minLeftMargin = inMediaBox.UpperRightX * scPageLeftPartCoefficient;

    auto it = inTextPlacements.begin();
    double lineStart = it->globalBbox[0];

    bool startsWithNumber = IsNumber(it->text) ? true : false;
    bool shouldSubtractNumberPosition = IsNumberAndDot(it->text) ? true : false;

    ParsedTextPlacement latestItem = *it;
    ++it;
    for (; it != inTextPlacements.end(); ++it) {
        //
        // Итемы без текста пропусаем
        //
        if (isEmptyString(it->text)) {
            continue;
        }

        if (AreSameLine(latestItem, *it)) {
            //
            // Если параграф начинается с номера и точки, то перезаписываем значение lineStart без
            // учета этого номера
            //
            if (shouldSubtractNumberPosition) {
                lineStart = it->globalBbox[0];
                shouldSubtractNumberPosition = false;
                startsWithNumber = false;
            }

            //
            // Проверяем что параграф начинается с номера и точки (или двоеточия)
            //
            if (startsWithNumber) {
                if (IsDotOrColon(it->text)) {
                    shouldSubtractNumberPosition = true;
                } else {
                    if (!IsNumber(it->text)) {
                        startsWithNumber = false;
                    }
                }
            }
        } else {
            if (lineStart < minLeftMargin || minLeftMargin < 0) {
                minLeftMargin = lineStart;
            }

            lineStart = it->globalBbox[0];
            startsWithNumber = IsNumber(it->text) ? true : false;
            shouldSubtractNumberPosition = IsNumberAndDot(it->text) ? true : false;
        }
        latestItem = *it;
    }

    if (lineStart < minLeftMargin || minLeftMargin < 0) {
        minLeftMargin = lineStart;
    }

    return minLeftMargin;
}

/**
 * @brief Минимальный отступ справа
 * @note Считается от левого края
 */
static double MinRightMargin(const ParsedTextPlacementVector& inTextPlacements)
{
    double minRightMargin = -1; // считается от левого края
    auto it = inTextPlacements.rbegin();
    double lineEnd = it->globalBbox[2];

    bool endsWithDot = IsDotOrColon(it->text) ? true : false;
    bool endsWithNumberAndDot = IsNumberAndDot(it->text) ? true : false;
    ParsedTextPlacement latestItem = *it;
    ++it;
    for (; it != inTextPlacements.rend(); ++it) {
        //
        // Итемы без текста пропусаем
        //
        if (isEmptyString(it->text)) {
            continue;
        }

        if (AreSameLine(latestItem, *it)) {
            //
            // Если параграф заканчивается номером и точкой, то перезаписываем значение lineEnd без
            // учета этого номера
            //
            if (endsWithNumberAndDot && !IsNumber(it->text)) {
                lineEnd = it->globalBbox[2];
                endsWithDot = false;
                endsWithNumberAndDot = false;
            }

            //
            // Проверяем что параграф заканчивается номером и точкой
            //
            if (endsWithDot) {
                if (IsNumber(it->text)) {
                    endsWithNumberAndDot = true;
                } else {
                    endsWithDot = false;
                }
            }
        } else {
            if (lineEnd > minRightMargin || minRightMargin < 0) {
                minRightMargin = lineEnd;
            }

            lineEnd = it->globalBbox[2];
            endsWithDot = IsDotOrColon(it->text) ? true : false;
            endsWithNumberAndDot = IsNumberAndDot(it->text) ? true : false;
        }
        latestItem = *it;
    }

    if (lineEnd > minRightMargin || minRightMargin < 0) {
        minRightMargin = lineEnd;
    }

    return minRightMargin;
}

/**
 * @brief Вставить текст с форматом
 * @return Записанный текст
 */
static std::string InsertText(const QList<FormatString>& inLineText, QTextCursor& inCursor)
{
    std::string text;
    auto itText = inLineText.constBegin();
    for (; itText != inLineText.constEnd(); ++itText) {
        QTextCharFormat format;
        if (itText->formats.find(TextFormat::Bold) != itText->formats.end()) {
            format.setFontWeight(QFont::Bold);
        }
        if (itText->formats.find(TextFormat::Italic) != itText->formats.end()) {
            format.setFontItalic(true);
        }
        if (itText->formats.find(TextFormat::Underline) != itText->formats.end()) {
            format.setFontUnderline(true);
        }
        if (itText->formats.find(TextFormat::Strikeout) != itText->formats.end()) {
            format.setFontStrikeOut(true);
        }

        QColor color(itText->backgroundColor.red * 255, itText->backgroundColor.green * 255,
                     itText->backgroundColor.blue * 255);
        if (color.isValid() && color != Qt::white) {
            format.setForeground(Qt::black);
            format.setBackground(color);
        }

        inCursor.insertText(itText->text, format);
        text.append(itText->text.toStdString());
    }
    return text;
}

/**
 * @brief Границы строки текста
 */
static ParagraphBox::Line LineBox(
    const ParsedTextPlacementVector::iterator inIterator,
    const ParsedTextPlacementVector::iterator inEnd)
{
    ParsedTextPlacementVector::iterator iterator = inIterator;
    ParsedTextPlacement& firstItem = *iterator;
    ParagraphBox::Line line;
    CopyBox(inIterator->globalBbox, line.box);
    ++iterator;
    for (; iterator != inEnd && AreSameLine(firstItem, *iterator); ++iterator) {
        if (!isEmptyString(iterator->text)) {
            UnionLeftBoxToRight(iterator->globalBbox, line.box);
        }
    }
    return line;
}

/**
 * @brief Является ли линия линией колонтитула
 */
static bool IsHeaderOrFooterLine(const PDFRectangle& inMediaBox, const ParsedLinePlacement& inLine)
{
    double lineLength = abs(inLine.globalPointTwo[0] - inLine.globalPointOne[0]);
    double mediaBoxWidth = inMediaBox.UpperRightX - inMediaBox.LowerLeftX;
    double mediaBoxHeight = inMediaBox.UpperRightY - inMediaBox.LowerLeftY;
    return !inLine.isVertical && lineLength > mediaBoxWidth * 3 / 4
        && inLine.effectiveLineWidth[0] == inLine.effectiveLineWidth[1]
        && inLine.effectiveLineWidth[0] < mediaBoxHeight / 100;
}

/**
 * @brief Является ли линия линией верхнего колонтитула
 */
static bool IsHeaderLine(const PDFRectangle& inMediaBox, const ParsedLinePlacement& inLine)
{
    return IsHeaderOrFooterLine(inMediaBox, inLine)
        && inLine.globalPointOne[1] > inMediaBox.UpperRightY * scPageTopPartCoefficient;
}

/**
 * @brief Является ли линия линией нижнего колонтитула
 */
static bool IsFooterLine(const PDFRectangle& inMediaBox, const ParsedLinePlacement& inLine)
{
    return IsHeaderOrFooterLine(inMediaBox, inLine)
        && inLine.globalPointOne[1] < inMediaBox.UpperRightY * scPageBottomPartCoefficient;
}

/**
 * @brief Позиция верхнего колонтитула
 */
static double PageHeaderLinePosition(const PDFRectangle& inMediaBox, const Lines& inPageLines)
{
    double pageHeader = inMediaBox.UpperRightY;
    //
    // Берем самую нижнюю из возможных линию
    //
    for (auto it = inPageLines.horizontalLines.cbegin(); it != inPageLines.horizontalLines.cend();
         ++it) {
        if (IsHeaderLine(inMediaBox, *it) && it->globalPointOne[1] < pageHeader) {
            pageHeader = it->globalPointOne[1];
        }
    }
    return pageHeader;
}

/**
 * @brief Позиция нижнего колонтитула
 */
static double PageFooterLinePosition(const PDFRectangle& inMediaBox, const Lines& inPageLines)
{
    double pageFooter = 0;
    //
    // Берем самую верхнуюю из возможных линию
    //
    for (auto it = inPageLines.horizontalLines.cbegin(); it != inPageLines.horizontalLines.cend();
         ++it) {
        if (IsFooterLine(inMediaBox, *it) && it->globalPointOne[1] > pageFooter) {
            pageFooter = it->globalPointOne[1];
        }
    }
    return pageFooter;
}

/**
 * @brief Выходит ли итем за правую границу текста
 */
static bool IsBeyondRightTextBorder(const ParsedTextPlacement& inItem,
                                    const PageParameters& inPageParameters)
{
    return inItem.globalBbox[2] > inPageParameters.minRightMargin;
}

/**
 * @brief Является ли итем верхним или нижним колонтитулом
 */
static bool IsPageHeaderOrFooter(const ParsedTextPlacement& inItem,
                                 const PageParameters& inPageParameters)
{
    const bool beyondBorders = inItem.globalBbox[1] > inPageParameters.headerLinePosition
        || inItem.globalBbox[3] < inPageParameters.footerLinePosition;

    const bool smallTextOnEdge
        = BoxHeight(inItem.globalBbox) < inPageParameters.generalTextSize
        && (inItem.globalBbox[1]
                > inPageParameters.mediaBox.UpperRightY * scPageTopPartCoefficient
            || inItem.globalBbox[3]
                < inPageParameters.mediaBox.UpperRightY * scPageBottomPartCoefficient);

    return beyondBorders || smallTextOnEdge;
}

/**
 * @brief Имеет ли текстовый итем наклон
 */
static bool HasRotation(const ParsedTextPlacement& inItem)
{
    //
    // Угол наклона текста в PDF определяется как atan2(matrix[1], matrix[0])
    // или как atan2(-matrix[2], matrix[0]), если текст имеет искажение (skewing);
    // но чтобы сказать что он ненулевой, достаточно оценить первый параметр
    //
    return abs(inItem.matrix[1]) > 0 || abs(inItem.matrix[2]) > 0;
}

/**
 * @brief Является ли текстовый итем прозрачным
 */
static bool IsTransparent(const ParsedTextPlacement& inItem)
{
    return !isEmptyString(inItem.text) && inItem.parameters.constantAlpha > scConstantAlphaMin
        && inItem.parameters.constantAlpha < scConstantAlphaMax;
}

/**
 * @brief Является ли текстовый итем частью сценария
 */
static bool IsScript(const ParsedTextPlacement& inItem,
                     const PageParameters& inPageParameters)
{
    //
    // Не считаем текстовый итем частью сценария, если он:
    // 1. выходит за границы текста справа (обычно это номера сцен или всякий мусор в виде
    //    пробельных символов);
    // 2. является колонтитулом;
    // 3. является водяным знаком (текст с наклоном или прозрачный);
    //
    return !IsBeyondRightTextBorder(inItem, inPageParameters)
        && !IsPageHeaderOrFooter(inItem, inPageParameters) && !HasRotation(inItem)
        && !IsTransparent(inItem);
}

/**
 * @brief Основной размер текста
 */
static double GeneralTextSize(const ParsedTextPlacementVector::iterator& inIterator,
                              const ParsedTextPlacementVector::iterator& inEnd)
{
    const auto compare = [](const TextItems& _lhs, const TextItems& _rhs) { return _lhs < _rhs; };
    std::set<TextItems, decltype(compare)> items(compare);

    for (auto iterator = inIterator; iterator != inEnd; ++iterator) {
        //
        // Пропускаем пробельные символы и вотермарки (текст с наклоном или прозрачный)
        //
        if (isEmptyString(iterator->text) || HasRotation(*iterator)
            || IsTransparent(*iterator)) {
            continue;
        }
        double height = int(BoxHeight(iterator->globalBbox) * 100) / 100.0;
        double width = BoxWidth(iterator->globalBbox);
        auto itItems = items.find(TextItems(height, 0, 0));
        if (itItems != items.end()) {
            TextItems newItems(height, itItems->length + width, itItems->count + 1);
            items.erase(itItems);
            items.insert(newItems);
        } else {
            TextItems newItems(height, width, 1);
            items.insert(newItems);
        }
    }

    auto itItems = items.begin();
    auto generalItems = itItems;
    ++itItems;
    if (itItems == items.end()) {
        return generalItems->height;
    }

    //
    // В некоторых PDF-файлах каждый итем - это отдельный символ с нулевой (почему-то) шириной,
    // в этом случае смотрим на их количество
    //
    if (itItems->length == 0) {
        for (; itItems != items.end(); ++itItems) {
            if (itItems->count > generalItems->count) {
                generalItems = itItems;
            }
        }
    } else {
        for (; itItems != items.end(); ++itItems) {
            if (itItems->length > generalItems->length) {
                generalItems = itItems;
            }
        }
    }

    return generalItems->height;
}

void TextComposer::MergeLineStreamToResultString(const stringstream& inStream, int bidiFlag,
                                                 bool shouldAddSpacesPerLines,
                                                 const double (&inLineBox)[4],
                                                 const double (&inPrevLineBox)[4])
{
    BidiConversion bidi;

    // add spaces before line, per distance from last line
    if (shouldAddSpacesPerLines && BoxTop(inLineBox) < BoxBottom(inPrevLineBox)) {
        unsigned long verticalLines
            = floor((BoxBottom(inPrevLineBox) - BoxTop(inLineBox)) / BoxHeight(inPrevLineBox));
        for (unsigned long i = 0; i < verticalLines; ++i)
            buffer << scCRLN;
    }


    if (bidiFlag == -1) {
        buffer << inStream.str();
    } else {
        string bidiResult;
        bidi.ConvertVisualToLogical(
            inStream.str(), bidiFlag,
            bidiResult); // returning status may be used to convey that's succeeded
        buffer << bidiResult;
    }
}

void TextComposer::ComposeText(const ParsedTextPlacementList& inTextPlacements) {
    double lineBox[4];
    double prevLineBox[4];
    bool isFirstLine;
    bool addVerticalSpaces = spacingFlag & TextComposer::eSpacingVertical;
    bool addHorizontalSpaces = spacingFlag & TextComposer::eSpacingHorizontal;

    ParsedTextPlacementVector sortedTextCommands(inTextPlacements.begin(), inTextPlacements.end());
    sort(sortedTextCommands.begin(), sortedTextCommands.end(), CompareParsedTextPlacement);

    ParsedTextPlacementVector::iterator itCommands = sortedTextCommands.begin();
    if(itCommands == sortedTextCommands.end())
        return;

    // k. got some text, let's build it
    stringstream lineResult;
    ParsedTextPlacement& latestItem = *itCommands;
    bool hasPreviousLineInPage = false;
    CopyBox(itCommands->globalBbox, lineBox);
    lineResult<<latestItem.text;
    ++itCommands;
    for(; itCommands != sortedTextCommands.end();++itCommands) {
        if(AreSameLine(latestItem, *itCommands)) {
            if(addHorizontalSpaces) {
                unsigned long spaces = GuessHorizontalSpacingBetweenPlacements(latestItem, *itCommands);
                if(spaces != 0)
                    lineResult<<string(spaces, scSpace);
            }
            UnionLeftBoxToRight(itCommands->globalBbox, lineBox);
        } else {
            // merge complete line to accumulated text, and start a fresh line with fresh accumulators
            MergeLineStreamToResultString(lineResult, bidiFlag ,addVerticalSpaces && hasPreviousLineInPage, lineBox, prevLineBox);
            buffer<<scCRLN;
            lineResult.str(scEmpty);
            CopyBox(lineBox, prevLineBox);
            CopyBox(itCommands->globalBbox, lineBox);
            hasPreviousLineInPage = true;
        }
        lineResult<<itCommands->text;
        latestItem = *itCommands;
    }
    MergeLineStreamToResultString(lineResult, bidiFlag ,addVerticalSpaces && hasPreviousLineInPage, lineBox, prevLineBox);

}

void TextComposer::AppendText(const std::string inText) {
    buffer<<inText;
}

std::string TextComposer::GetText() {
    return buffer.str();
}

void TextComposer::Reset() {
    buffer.str(scEmpty);
}

void TextComposer::ComposeDocument(const ParsedTextPlacementList& inTextPlacements,
                                   const PDFRectangle& inMediaBox, const Lines& inPageLines,
                                   QTextCursor& inCursor)
{
    ParsedTextPlacementVector sortedTextCommands(inTextPlacements.begin(), inTextPlacements.end());
    sort(sortedTextCommands.begin(), sortedTextCommands.end(), CompareParsedTextPlacement);

    ParsedTextPlacementVector::iterator itCommands = sortedTextCommands.begin();
    if (itCommands == sortedTextCommands.end()) {
        return;
    }

    PageParameters pageParameters;
    pageParameters.mediaBox = inMediaBox;
    pageParameters.minLeftMargin = MinLeftMargin(sortedTextCommands, inMediaBox);
    pageParameters.minRightMargin = MinRightMargin(sortedTextCommands);
    pageParameters.headerLinePosition = PageHeaderLinePosition(inMediaBox, inPageLines);
    pageParameters.footerLinePosition = PageFooterLinePosition(inMediaBox, inPageLines);
    pageParameters.generalTextSize = GeneralTextSize(itCommands, sortedTextCommands.end());

    bool firstLineOnPage = true;
    double lineBox[4];
    bool addHorizontalSpaces = spacingFlag & TextComposer::eSpacingHorizontal;

    //
    // Новый параграф
    //
    double previousParagraphBottom = inMediaBox.UpperRightY;
    ParagraphBox paragraph = { { 0, 0, 0, 0 }, {} };
    inCursor.insertBlock();
    //
    // ... номера в начале параграфа учитывать не будем
    //
    bool startsWithNumber = IsNumber(itCommands->text) ? true : false;
    bool shouldSubtractNumberPosition = IsNumberAndDot(itCommands->text) ? true : false;

    //
    // Пропускаем итемы, которые не относятся к тексту сценария
    //
    while (itCommands != sortedTextCommands.end() && !IsScript(*itCommands, pageParameters)) {
        ++itCommands;
    }

    //
    // Проверяем, что после пропуска ненужных итемов мы не дошли до конца
    //
    if (itCommands == sortedTextCommands.end()) {
        return;
    }

    ParsedTextPlacement& latestItem = *itCommands;
    CopyBox(itCommands->globalBbox, lineBox);
    ColorRGB latestColor;
    std::set<TextFormat> latestFormats;
    QList<FormatString> lineTextWithFormats;
    {
        FormatString formatString(*itCommands, inPageLines);
        lineTextWithFormats.append(formatString);
        latestColor = formatString.backgroundColor;
        latestFormats = formatString.formats;
    }

    ++itCommands;
    for (; itCommands != sortedTextCommands.end(); ++itCommands) {
        //
        // Пропускаем итемы, которые не относятся к тексту сценария
        //
        if (!IsScript(*itCommands, pageParameters)) {
            continue;
        }

        //
        // Иногда встречаются мусорные (выходящие за границы строки) пробелы,
        // поэтому при переходе на новую строку будем их пропускать
        //
        if (!AreSameLine(latestItem, *itCommands)) {
            while (itCommands != sortedTextCommands.end() && isEmptyString(itCommands->text)) {
                //
                // ... нормальные пробелы пишем
                //
                if (AreSameLine(latestItem, *itCommands)) {
                    lineTextWithFormats.append(FormatString(" "));
                }
                ++itCommands;
            }

            //
            // Проверяем, что после пропуска пробельных символов мы не дошли до конца итемов
            //
            if (itCommands == sortedTextCommands.end()) {
                break;
            }

            //
            // Проверяем, что после пропуска пробельных символов текущий относится к тексту сценария
            //
            if (!IsScript(*itCommands, pageParameters)) {
                continue;
            }
        }

        if (AreSameLine(latestItem, *itCommands)) {
            if (addHorizontalSpaces) {
                //
                // NOTE: это работает не всегда!
                //
                unsigned long spaces
                    = GuessHorizontalSpacingBetweenPlacements(latestItem, *itCommands);
                if (spaces != 0) {
                    lineTextWithFormats.append(FormatString(" "));
                }
            }

            //
            // Если параграф начинается с номера и точки, то перезаписываем значение lineBox без
            // учета этого номера
            //
            if (shouldSubtractNumberPosition) {
                CopyBox(itCommands->globalBbox, lineBox);
                //
                // Если позиция предыдущего итема выходит за левую границу текста, то считаем, что
                // это номер сцены и добавим пробел, т.к. иногда он не считывается
                //
                if (latestItem.globalBbox[0] < pageParameters.minLeftMargin) {
                    lineTextWithFormats.append(FormatString(" "));
                } else {
                    //
                    // ... если не выходит, считаем, что это номер реплики - его не пишем
                    //
                    lineTextWithFormats.clear();
                }
                shouldSubtractNumberPosition = false;
                startsWithNumber = false;
            } else if (!isEmptyString(itCommands->text)) {
                UnionLeftBoxToRight(itCommands->globalBbox, lineBox);
            }

            //
            // Проверяем что параграф начинается с номера и (точки или двоеточия)
            //
            if (startsWithNumber) {
                if (IsDotOrColon(itCommands->text)) {
                    shouldSubtractNumberPosition = true;
                } else {
                    if (!IsNumber(itCommands->text)) {
                        startsWithNumber = false;
                    }
                }
            }
        } else {
            //
            // Если предыдущая строка состоит только из номера, то её пропускаем
            //
            const std::string previousLineText = ExtractText(lineTextWithFormats);
            if (IsNumberAndDot(previousLineText) || IsNumber(previousLineText)) {
                lineTextWithFormats.clear();
            } else {
                //
                // Добавляем к предыдущей строке пробел с теми же форматами и выделением цветом, что
                // и у последнего записанного символа, чтобы многострочное форматирование не
                // разбивалось на несколько
                //
                lineTextWithFormats.append(FormatString(" ", latestFormats, latestColor));

                //
                // Костыль для импорта из КИТа - убираем дублирующиеся строки при переходе на новую
                // страницу
                //
                if (firstLineOnPage) {
                    firstLineOnPage = false;
                    const int countToRemove = QString::fromStdString(lastWtrittenText).size();
                    if (countToRemove > 0
                        && ExtractText(lineTextWithFormats).find(lastWtrittenText)
                            != std::string::npos) {
                        inCursor.movePosition(QTextCursor::PreviousBlock);
                        inCursor.movePosition(QTextCursor::EndOfBlock);
                        inCursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor,
                                              countToRemove);
                        inCursor.removeSelectedText();
                        inCursor.movePosition(QTextCursor::End);
                        inCursor.insertBlock();
                    }
                }

                lastWtrittenText = InsertText(lineTextWithFormats, inCursor);
                AddLineToParagraphBox(lineBox, paragraph);
                lineTextWithFormats.clear();

                //
                // Определяем границы новой строки
                //
                ParagraphBox::Line newLine = LineBox(itCommands, sortedTextCommands.end());

                if (IsNewParagraph(lineBox, newLine.box, pageParameters)) {
                    SetFormatToPreviousBlock(paragraph, pageParameters, previousParagraphBottom,
                                             inCursor);
                    previousParagraphBottom = paragraph.box[1];
                    paragraph.clear();
                    inCursor.insertBlock();
                }
            }
            startsWithNumber = IsNumber(itCommands->text) ? true : false;
            shouldSubtractNumberPosition = IsNumberAndDot(itCommands->text) ? true : false;
            CopyBox(itCommands->globalBbox, lineBox);
        }

        {
            FormatString formatString(*itCommands, inPageLines);
            lineTextWithFormats.append(formatString);
            latestColor = formatString.backgroundColor;
            latestFormats = formatString.formats;
            if (!isEmptyString(itCommands->text)) {
                latestItem = *itCommands;
            }
        }
    }
    const std::string previousLineText = ExtractText(lineTextWithFormats);
    if (!IsNumberAndDot(previousLineText) && !IsNumber(previousLineText)) {
        lineTextWithFormats.append(FormatString(" "));
        lastWtrittenText = InsertText(lineTextWithFormats, inCursor);
    }
    AddLineToParagraphBox(lineBox, paragraph);
    SetFormatToPreviousBlock(paragraph, pageParameters, previousParagraphBottom, inCursor);
}
