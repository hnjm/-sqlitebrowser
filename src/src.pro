TEMPLATE = app

QT += core gui network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

TARGET = sqlitebrowser

CONFIG += debug_and_release
CONFIG += qt
CONFIG += warn_on

# create a unittest option
CONFIG(unittest) {
  greaterThan(QT_MAJOR_VERSION, 4) {
    QT += testlib
  } else {
    CONFIG += qtestlib
  }

  HEADERS += tests/testsqlobjects.h tests/TestImport.h
  SOURCES += tests/testsqlobjects.cpp tests/TestImport.cpp tests/TestMain.cpp
} else {
  SOURCES += main.cpp
}

HEADERS += \
    sqlitedb.h \
    MainWindow.h \
    CreateIndexDialog.h \
    AboutDialog.h \
    EditTableDialog.h \
    PreferencesDialog.h \
    EditDialog.h \
    ExportCsvDialog.h \
    ImportCsvDialog.h \
    sqltextedit.h \
    sqlitetypes.h \
    csvparser.h \
    ExtendedTableWidget.h \
    grammar/Sqlite3Lexer.hpp \
    grammar/Sqlite3Parser.hpp \
    grammar/sqlite3TokenTypes.hpp \
    sqlitetablemodel.h \
    FilterTableHeader.h \
    version.h \
    SqlExecutionArea.h \
    VacuumDialog.h \
    DbStructureModel.h \
    Application.h \
    sqlite.h \
    CipherDialog.h \
    ExportSqlDialog.h \
    SqlUiLexer.h \
    FileDialog.h \
    ColumnDisplayFormatDialog.h \
    FilterLineEdit.h

SOURCES += \
    sqlitedb.cpp \
    MainWindow.cpp \
    CreateIndexDialog.cpp \
    EditTableDialog.cpp \
    PreferencesDialog.cpp \
    AboutDialog.cpp \
    EditDialog.cpp \
    ExportCsvDialog.cpp \
    ImportCsvDialog.cpp \
    sqltextedit.cpp \
    sqlitetypes.cpp \
    csvparser.cpp \
    ExtendedTableWidget.cpp \
    grammar/Sqlite3Lexer.cpp \
    grammar/Sqlite3Parser.cpp \
    sqlitetablemodel.cpp \
    FilterTableHeader.cpp \
    SqlExecutionArea.cpp \
    VacuumDialog.cpp \
    DbStructureModel.cpp \
    Application.cpp \
    CipherDialog.cpp \
    ExportSqlDialog.cpp \
    SqlUiLexer.cpp \
    FileDialog.cpp \
    ColumnDisplayFormatDialog.cpp \
    FilterLineEdit.cpp

RESOURCES += icons/icons.qrc \
             translations/flags/flags.qrc \
             translations/translations.qrc

FORMS += \
    MainWindow.ui \
    CreateIndexDialog.ui \
    AboutDialog.ui \
    EditTableDialog.ui \
    PreferencesDialog.ui \
    EditDialog.ui \
    ExportCsvDialog.ui \
    ImportCsvDialog.ui \
    SqlExecutionArea.ui \
    VacuumDialog.ui \
    CipherDialog.ui \
    ExportSqlDialog.ui \
    ColumnDisplayFormatDialog.ui

TRANSLATIONS += \
    translations/sqlb_ar_SA.ts \
    translations/sqlb_zh.ts \
    translations/sqlb_zh_TW.ts \
    translations/sqlb_de.ts \
    translations/sqlb_es_ES.ts \
    translations/sqlb_fr.ts \
    translations/sqlb_ru.ts \
    translations/sqlb_pt_BR.ts \
    translations/sqlb_en_GB.ts \
    translations/sqlb_ko_KR.ts \
    translations/sqlb_tr.ts

# SQLite / SQLCipher switch pieces
CONFIG(sqlcipher) {
    QMAKE_CXXFLAGS += -DENABLE_SQLCIPHER
    LIBS += -lsqlcipher

    # Add the paths for Homebrew installed SQLCipher
    mac {
        INCLUDEPATH += /usr/local/opt/sqlcipher/include
        LIBS += -L/usr/local/opt/sqlcipher/lib
    }

    win32 {
        # Added SQLCipher installation path variables, matching our setup guide
        LIBS += -L"C:\git_repos\sqlcipher" -L"C:\dev\OpenSSL-Win64"
        INCLUDEPATH += "C:\git_repos\sqlcipher" -L"C:\dev\OpenSSL-Win64"
        DEPENDPATH += "C:\git_repos\sqlcipher" -L"C:\dev\OpenSSL-Win64"
    }
} else {
    LIBS += -lsqlite3

    # Add the paths for Homebrew installed SQLite
    mac {
        INCLUDEPATH += /usr/local/opt/sqlite/include
        LIBS += -L/usr/local/opt/sqlite/lib
    }

    win32 {
        # Added SQLite installation path variables, matching our setup guide
        LIBS += -L"C:\dev\SQLite-Win64"
        INCLUDEPATH += "C:\dev\SQLite-Win64"
        DEPENDPATH += "C:\dev\SQLite-Win64"
    }
}

LIBPATH_QHEXEDIT=$$OUT_PWD/../libs/qhexedit
LIBPATH_ANTLR=$$OUT_PWD/../libs/antlr-2.7.7
LIBPATH_QCUSTOMPLOT=$$OUT_PWD/../libs/qcustomplot-source
LIBPATH_QSCINTILLA=$$OUT_PWD/../libs/qscintilla/Qt4Qt5
unix {
    LIBS += -ldl
}
os2 {
    RC_FILE = os2app.rc
}
win32 {
    TARGET = "DB Browser for SQLite"
    RC_FILE = winapp.rc
    INCLUDEPATH += $$PWD
    CONFIG(debug,debug|release) {
        LIBPATH_QHEXEDIT = $$LIBPATH_QHEXEDIT/debug
        LIBPATH_ANTLR = $$LIBPATH_ANTLR/debug
        LIBPATH_QCUSTOMPLOT = $$LIBPATH_QCUSTOMPLOT/debug
    LIBPATH_QSCINTILLA = $$LIBPATH_QSCINTILLA/debug
    }
    CONFIG(release,debug|release) {
        LIBPATH_QHEXEDIT = $$LIBPATH_QHEXEDIT/release
        LIBPATH_ANTLR = $$LIBPATH_ANTLR/release
        LIBPATH_QCUSTOMPLOT = $$LIBPATH_QCUSTOMPLOT/release
    LIBPATH_QSCINTILLA = $$LIBPATH_QSCINTILLA/release
    }
    QMAKE_CXXFLAGS += -DCHECKNEWVERSION
}
mac {
    TARGET = "DB Browser for SQLite"
    RC_FILE = macapp.icns
    QT+= macextras
    INCLUDEPATH += /usr/local/include
    LIBS += -L/usr/local/lib -framework Carbon
    QMAKE_INFO_PLIST = app.plist
    QMAKE_CXXFLAGS += -DCHECKNEWVERSION
}

UI_DIR = .ui
INCLUDEPATH += $$PWD/../libs/antlr-2.7.7 $$PWD/../libs/qhexedit $$PWD/../libs/qcustomplot-source $$PWD/../libs/qscintilla/Qt4Qt5 $$PWD/..
LIBS += -L$$LIBPATH_QHEXEDIT -L$$LIBPATH_ANTLR -L$$LIBPATH_QCUSTOMPLOT -L$$LIBPATH_QSCINTILLA -lantlr -lqhexedit -lqcustomplot -lqscintilla2
DEPENDPATH += $$PWD/../libs/antlr-2.7.7 $$PWD/../libs/qhexedit $$PWD/../libs/qcustomplot-source $$PWD/../libs/qscintilla/Qt4Qt5

# Rules for creating/updating {ts|qm}-files
include(i18n.pri)
