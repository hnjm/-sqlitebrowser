#ifndef SQLEXECUTIONAREA_H
#define SQLEXECUTIONAREA_H

#include "sqltextedit.h"

#include <QWidget>

class SqliteTableModel;
class DBBrowserDB;
class QMenu;

namespace Ui {
class SqlExecutionArea;
}

class SqlExecutionArea : public QWidget
{
    Q_OBJECT

public:
    explicit SqlExecutionArea(QWidget* parent = 0, DBBrowserDB* _db = 0);
    ~SqlExecutionArea();

    QString getSql() const;
    QString getSelectedSql() const;

    QString fileName() const { return sqlFileName; }
    void setFileName(const QString& filename) { sqlFileName = filename; }

    SqliteTableModel* getModel() { return model; }
    QTextEdit* getResultView();
    SqlTextEdit* getEditor();

public slots:
    virtual void setTableNames(const QStringList& tables);
    virtual void finishExecution(const QString& result);
    virtual void enableSaveButton(bool enable);
    virtual void saveAsCsv();
    virtual void saveAsView();

private:
    DBBrowserDB* db;
    Ui::SqlExecutionArea* ui;
    SqliteTableModel* model;
    QMenu* menuPopupSave;
    QString sqlFileName;
};

#endif
