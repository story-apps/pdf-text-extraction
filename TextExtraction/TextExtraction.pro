TARGET   = TextExtraction
TEMPLATE = lib

CONFIG += staticlib

LIBSDIR = ../../../_build/libs

#
# Конфигурируем расположение файлов сборки
#
DESTDIR = ../../../_build/libs
#

#
# Подключаем библиотеку PDFWriter
#
LIBS += -L$$LIBSDIR/ -lPDFWriter
INCLUDEPATH += $$PWD/../../pdfhummus/PDFWriter
DEPENDPATH += $$PWD/../../pdfhummus/PDFWriter
#

INCLUDEPATH += $$PWD

HEADERS += \
    lib/bidi/BidiConversion.h \
    lib/font-translation/Encoding.h \
    lib/font-translation/FontDecoder.h \
    lib/font-translation/StandardFontsDimensions.h \
    lib/font-translation/Translation.h \
    lib/graphic-content-parsing/ContentGraphicState.h \
    lib/graphic-content-parsing/GraphicContentInterpreter.h \
    lib/graphic-content-parsing/IGraphicContentInterpreterHandler.h \
    lib/graphic-content-parsing/Path.h \
    lib/graphic-content-parsing/PathElement.h \
    lib/graphic-content-parsing/Resources.h \
    lib/graphic-content-parsing/TextElement.h \
    lib/graphic-content-parsing/TextGraphicState.h \
    lib/graphs/Graph.h \
    lib/graphs/Queue.h \
    lib/graphs/Result.h \
    lib/interpreter/IPDFInterpreterHandler.h \
    lib/interpreter/IPDFRecursiveInterpreterHandler.h \
    lib/interpreter/PDFInterpreter.h \
    lib/interpreter/PDFRecursiveInterpreter.h \
    lib/math/Transformations.h \
    lib/pdf-writer-enhancers/Bytes.h \
    lib/table-csv-export/TableCSVExport.h \
    lib/table-line-parsing/ITableLineInterpreterHandler.h \
    lib/table-line-parsing/ParsedLinePlacement.h \
    lib/table-line-parsing/TableLineInterpreter.h \
    lib/table-composition/Lines.h \
    lib/table-composition/Table.h \
    lib/table-composition/TableComposer.h \
    lib/text-composition/TextComposer.h \
    lib/text-parsing/ITextInterpreterHandler.h \
    lib/text-parsing/ParsedTextPlacement.h \
    lib/text-parsing/TextInterpreter.h \
    ErrorsAndWarnings.h \
    TableExtraction.h \
    TextExtraction.h

SOURCES += \
    lib/bidi/BidiConversion.cpp \
    lib/font-translation/Encoding.cpp \
    lib/font-translation/FontDecoder.cpp \
    lib/font-translation/StandardFontsDimensions.cpp \
    lib/graphic-content-parsing/GraphicContentInterpreter.cpp \
    lib/interpreter/PDFInterpreter.cpp \
    lib/interpreter/PDFRecursiveInterpreter.cpp \
    lib/math/Transformations.cpp \
    lib/pdf-writer-enhancers/Bytes.cpp \
    lib/table-csv-export/TableCSVExport.cpp \
    lib/table-line-parsing/TableLineInterpreter.cpp \
    lib/table-composition/Table.cpp \
    lib/table-composition/TableComposer.cpp \
    lib/text-composition/TextComposer.cpp \
    lib/text-parsing/TextInterpreter.cpp \
    TableExtraction.cpp \
    TextExtraction.cpp 
