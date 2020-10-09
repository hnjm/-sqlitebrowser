#include "ExportCsvDialog.h"
#include "ui_ExportCsvDialog.h"
#include "sqlitedb.h"
#include "PreferencesDialog.h"
#include "sqlitetablemodel.h"
#include "sqlite.h"
#include "FileDialog.h"

#include <QFile>
#include <QTextStream>
#include <QMessageBox>

ExportCsvDialog::ExportCsvDialog(DBBrowserDB* db, QWidget* parent, const QString& query, const QString& selection)
    : QDialog(parent),
      ui(new Ui::ExportCsvDialog),
      pdb(db),
      m_sQuery(query)
{
    // Create UI
    ui->setupUi(this);

    // Retrieve the saved dialog preferences
    bool firstRow = PreferencesDialog::getSettingsValue("exportcsv", "firstrowheader").toBool();
    int separatorChar = PreferencesDialog::getSettingsValue("exportcsv", "separator").toInt();
    int quoteChar = PreferencesDialog::getSettingsValue("exportcsv", "quotecharacter").toInt();
    QString newLineString = PreferencesDialog::getSettingsValue("exportcsv", "newlinecharacters").toString();

    // Set the widget values using the retrieved preferences
    ui->checkHeader->setChecked(firstRow);
    setSeparatorChar(separatorChar);
    setQuoteChar(quoteChar);
    setNewLineString(newLineString);

    // Update the visible/hidden status of the "Other" line edit fields
    showCustomCharEdits();

    // If a SQL query was specified hide the table combo box. If not fill it with tables to export
    if(query.isEmpty())
    {
        // Get list of tables to export
        objectMap objects = pdb->getBrowsableObjects();
        for(objectMap::ConstIterator i=objects.begin();i!=objects.end();++i)
        {
            ui->listTables->addItem(new QListWidgetItem(QIcon(QString(":icons/%1").arg(i.value().gettype())), i.value().getname()));
        }

        // Sort list of tables and select the table specified in the selection parameter or alternatively the first one
        ui->listTables->model()->sort(0);
        if(selection.isEmpty())
        {
            ui->listTables->setCurrentItem(ui->listTables->item(0));
        }
        else
        {
            QList<QListWidgetItem*> items = ui->listTables->findItems(selection, Qt::MatchExactly);
            if (!items.isEmpty())
                ui->listTables->setCurrentItem(items.first());
        }
    } else {
        // Hide table combo box
        ui->labelTable->setVisible(false);
        ui->listTables->setVisible(false);
        resize(minimumSize());
    }
}

ExportCsvDialog::~ExportCsvDialog()
{
    delete ui;
}

bool ExportCsvDialog::exportQuery(const QString& sQuery, const QString& sFilename)
{
    // Prepare the quote and separating characters
    QChar quoteChar = currentQuoteChar();
    QString quotequoteChar = QString(quoteChar) + quoteChar;
    QChar sepChar = currentSeparatorChar();
    QString newlineStr = currentNewLineString();

    // Chars that require escaping
    std::string special_chars = newlineStr.toStdString() + sepChar.toLatin1() + quoteChar.toLatin1();

    // Open file
    QFile file(sFilename);
    if(file.open(QIODevice::WriteOnly))
    {
        // Open text stream to the file
        QTextStream stream(&file);

        QByteArray utf8Query = sQuery.toUtf8();
        sqlite3_stmt *stmt;

        int status = sqlite3_prepare_v2(pdb->_db, utf8Query.data(), utf8Query.size(), &stmt, NULL);
        if(SQLITE_OK == status)
        {
            if(ui->checkHeader->isChecked())
            {
                int columns = sqlite3_column_count(stmt);
                for (int i = 0; i < columns; ++i)
                {
                    QString content = QString::fromUtf8(sqlite3_column_name(stmt, i));
                    if(content.toStdString().find_first_of(special_chars) != std::string::npos)
                        stream << quoteChar << content.replace(quoteChar, quotequoteChar) << quoteChar;
                    else
                        stream << content;
                    if(i != columns - 1)
                        // Only output the separator value if sepChar isn't 0,
                        // as that's used to indicate no separator character
                        // should be used
                        if (sepChar != 0) {
                            stream << sepChar;
                        }
                }
                stream << newlineStr;
            }

            QApplication::setOverrideCursor(Qt::WaitCursor);
            int columns = sqlite3_column_count(stmt);
            size_t counter = 0;
            while(sqlite3_step(stmt) == SQLITE_ROW)
            {
                for (int i = 0; i < columns; ++i)
                {
                    QString content = QString::fromUtf8(
                                (const char*)sqlite3_column_blob(stmt, i),
                                sqlite3_column_bytes(stmt, i));
                    if(content.toStdString().find_first_of(special_chars) != std::string::npos)
                        stream << quoteChar << content.replace(quoteChar, quotequoteChar) << quoteChar;
                    else
                        stream << content;
                    if(i != columns - 1)
                        // Only output the separator value if sepChar isn't 0,
                        // as that's used to indicate no separator character
                        // should be used
                        if (sepChar != 0) {
                            stream << sepChar;
                        }
                }
                stream << newlineStr;
                if(counter % 1000 == 0)
                    qApp->processEvents();
                counter++;
            }
        }
        sqlite3_finalize(stmt);

        QApplication::restoreOverrideCursor();
        qApp->processEvents();

        // Done writing the file
        file.close();
    } else {
        QMessageBox::warning(this, QApplication::applicationName(),
                             tr("Could not open output file: %1").arg(sFilename));
        return false;
    }

    return true;
}

void ExportCsvDialog::accept()
{
    if(!m_sQuery.isEmpty())
    {
        // called from sqlexecute query tab
        QString sFilename = FileDialog::getSaveFileName(
                this,
                tr("Choose a filename to export data"),
                tr("Text files(*.csv *.txt)"));
        if(sFilename.isEmpty())
        {
            close();
            return;
        }

        exportQuery(m_sQuery, sFilename);
    }
    else
    {
        // called from the File export menu
        QList<QListWidgetItem*> selectedItems = ui->listTables->selectedItems();

        if(selectedItems.isEmpty())
        {
            QMessageBox::warning(this, QApplication::applicationName(),
                                 tr("Please select at least 1 table."));
            return;
        }

        // Get filename
        QStringList filenames;
        if(selectedItems.size() == 1)
        {
            QString fileName = FileDialog::getSaveFileName(
                    this,
                    tr("Choose a filename to export data"),
                    tr("Text files(*.csv *.txt)"),
                    selectedItems.at(0)->text() + ".csv");
            if(fileName.isEmpty())
            {
                close();
                return;
            }

            filenames << fileName;
        }
        else
        {
            // ask for folder
            QString csvfolder = FileDialog::getExistingDirectory(
                        this,
                        tr("Choose a directory"),
                        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

            if(csvfolder.isEmpty())
            {
                close();
                return;
            }

            for(QList<QListWidgetItem*>::iterator it = selectedItems.begin(); it != selectedItems.end(); ++it)
            {
                filenames << QDir(csvfolder).filePath((*it)->text() + ".csv");
            }
        }

        // Only if the user hasn't clicked the cancel button
        for(int i = 0; i < selectedItems.size(); ++i)
        {
            // if we are called from execute sql tab, query is already set
            // and we only export 1 select
            QString sQuery = QString("SELECT * FROM %1;").arg(sqlb::escapeIdentifier(selectedItems.at(i)->text()));

            exportQuery(sQuery, filenames.at(i));
        }
    }

    // Save the dialog preferences for future use
    PreferencesDialog::setSettingsValue("exportcsv", "firstrowheader", ui->checkHeader->isChecked(), false);
    PreferencesDialog::setSettingsValue("exportcsv", "separator", currentSeparatorChar(), false);
    PreferencesDialog::setSettingsValue("exportcsv", "quotecharacter", currentQuoteChar(), false);
    PreferencesDialog::setSettingsValue("exportcsv", "newlinecharacters", currentNewLineString(), false);

    // Notify the user the export has completed
    QMessageBox::information(this, QApplication::applicationName(), tr("Export completed."));
    QDialog::accept();
}

void ExportCsvDialog::showCustomCharEdits()
{
    // Retrieve selection info for the quote, separator, and newline widgets
    int quoteIndex = ui->comboQuoteCharacter->currentIndex();
    int quoteCount = ui->comboQuoteCharacter->count();
    int sepIndex = ui->comboFieldSeparator->currentIndex();
    int sepCount = ui->comboFieldSeparator->count();
    int newLineIndex = ui->comboNewLineString->currentIndex();
    int newLineCount = ui->comboNewLineString->count();

    // Determine which will have their 'Other' line edit widget visible
    bool quoteVisible = quoteIndex == (quoteCount - 1);
    bool sepVisible = sepIndex == (sepCount - 1);
    bool newLineVisible = newLineIndex == (newLineCount - 1);

    // Update the visibility of the 'Other' line edit widgets
    ui->editCustomQuote->setVisible(quoteVisible);
    ui->editCustomSeparator->setVisible(sepVisible);
    ui->editCustomNewLine->setVisible(newLineVisible);
}

void ExportCsvDialog::setQuoteChar(const QChar& c)
{
    QComboBox* combo = ui->comboQuoteCharacter;

    // Set the combo and/or Other box to the correct selection
    switch (c.toLatin1()) {
    case '"':
        combo->setCurrentIndex(0);  // First option is a quote character
        break;

    case '\'':
        combo->setCurrentIndex(1);  // Second option is a single quote character
        break;

    case 0:
        combo->setCurrentIndex(2);  // Third option is blank (no character)
        break;

    default:
        // For everything else, set the combo box to option 3 ('Other') and
        // place the desired string into the matching edit line box
        combo->setCurrentIndex(3);
        if (c != 0) {
            // Don't set it if/when it's the 0 flag value
            ui->editCustomQuote->setText(c);
        }
        break;
    }
}

char ExportCsvDialog::currentQuoteChar() const
{
    QComboBox* combo = ui->comboQuoteCharacter;

    switch (combo->currentIndex()) {
    case 0:
        return '"';  // First option is a quote character

    case 1:
        return '\'';  // Second option is a single quote character

    case 2:
        return 0;  // Third option is a blank (no character)

    default:
        // The 'Other' option was selected, so check if the matching edit
        // line widget contains something
        int customQuoteLength = ui->editCustomQuote->text().length();
        if (customQuoteLength > 0) {
            // Yes it does.  Return its first character
            char customQuoteChar = ui->editCustomQuote->text().at(0).toLatin1();
            return customQuoteChar;
        } else {
            // No it doesn't, so return 0 to indicate it was empty
            return 0;
        }
    }
}

void ExportCsvDialog::setSeparatorChar(const QChar& c)
{
    QComboBox* combo = ui->comboFieldSeparator;

    // Set the combo and/or Other box to the correct selection
    switch (c.toLatin1()) {
    case ',':
        combo->setCurrentIndex(0);  // First option is a comma character
        break;

    case ';':
        combo->setCurrentIndex(1);  // Second option is a semi-colon character
        break;

    case '\t':
        combo->setCurrentIndex(2);  // Third option is a tab character
        break;

    case '|':
        combo->setCurrentIndex(3);  // Fourth option is a pipe symbol
        break;

    default:
        // For everything else, set the combo box to option 3 ('Other') and
        // place the desired string into the matching edit line box
        combo->setCurrentIndex(4);

        // Only put the separator character in the matching line edit box if
        // it's not the flag value of 0, which is for indicating its empty
        if (c != 0) {
            ui->editCustomSeparator->setText(c);
        }
        break;
    }
}

char ExportCsvDialog::currentSeparatorChar() const
{
    QComboBox* combo = ui->comboFieldSeparator;

    switch (combo->currentIndex()) {
    case 0:
        return ',';  // First option is a comma character

    case 1:
        return ';';  // Second option is a semi-colon character

    case 2:
        return '\t';  // Third option is a tab character

    case 3:
        return '|';  // Fourth option is a pipe character

    default:
        // The 'Other' option was selected, so check if the matching edit
        // line widget contains something
        int customSeparatorLength = ui->editCustomSeparator->text().length();
        if (customSeparatorLength > 0) {
            // Yes it does.  Return its first character
            char customSeparatorChar = ui->editCustomSeparator->text().at(0).toLatin1();
            return customSeparatorChar;
        } else {
            // No it doesn't, so return 0 to indicate it was empty
            return 0;
        }
    }
}

void ExportCsvDialog::setNewLineString(const QString& s)
{
    QComboBox* combo = ui->comboNewLineString;

    // Set the combo and/or Other box to the correct selection
    if (s == "\r\n") {
        // For Windows style newlines, set the combo box to option 0
        combo->setCurrentIndex(0);
    } else if (s == "\n") {
        // For Unix style newlines, set the combo box to option 1
        combo->setCurrentIndex(1);
    } else {
        // For everything else, set the combo box to option 2 ('Other') and
        // place the desired string into the matching edit line box
        combo->setCurrentIndex(2);
        ui->editCustomNewLine->setText(s);
    }
}

QString ExportCsvDialog::currentNewLineString() const
{
    QComboBox* combo = ui->comboNewLineString;

    switch (combo->currentIndex()) {
    case 0:
        // Windows style newlines
        return QString("\r\n");

    case 1:
        // Unix style newlines
        return QString("\n");

    default:
        // Return the text from the 'Other' box
        return QString(ui->editCustomNewLine->text().toLatin1());
    }
}
