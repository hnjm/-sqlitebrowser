#ifndef FILTERTABLEHEADER_H
#define FILTERTABLEHEADER_H

#include <QHeaderView>
#include <QList>

class QLineEdit;
class QTableView;
class FilterLineEdit;

class FilterTableHeader : public QHeaderView
{
    Q_OBJECT

public:
    explicit FilterTableHeader(QTableView* parent = nullptr);
    virtual QSize sizeHint() const;
    bool hasFilters() const {return (filterWidgets.count() > 0);};
    
public slots:
    void generateFilters(int number, bool showFirst = false);
    void adjustPositions();
    void clearFilters();
    void setFilter(int column, const QString& value);

signals:
    void filterChanged(int column, QString value);

protected:
    virtual void updateGeometries();

private slots:
    void inputChanged(const QString& new_value);

private:
    QList<FilterLineEdit*> filterWidgets;
};

#endif
