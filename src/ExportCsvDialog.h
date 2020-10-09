#ifndef __EXPORTCSVDIALOG_H__
#define __EXPORTCSVDIALOG_H__

#include <QDialog>

class DBBrowserDB;

namespace Ui {
class ExportCsvDialog;
}

class ExportCsvDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportCsvDialog(DBBrowserDB* db, QWidget* parent = 0, const QString& query = "", const QString& selection = "");
    ~ExportCsvDialog();

private slots:
    virtual void accept();

private:
    Ui::ExportCsvDialog* ui;
    DBBrowserDB* pdb;

    QString m_sQuery;
};

#endif
