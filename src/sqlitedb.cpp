#include "sqlitedb.h"
#include "sqlitetablemodel.h"

#include <QFile>
#include <QMessageBox>
#include <QProgressDialog>
#include <QApplication>
#include <QTextStream>
#include <QSettings>
#include <QDebug>
#include <sqlite3.h>

bool DBBrowserDB::isOpen ( ) const
{
    return _db!=0;
}

bool DBBrowserDB::getDirty() const
{
    return !savepointList.empty();
}

bool DBBrowserDB::open ( const QString & db)
{
    bool ok=false;
    int  err;

    if (isOpen()) close();

    //try to verify the SQLite version 3 file header
    QFile dbfile(db);
    if ( dbfile.open( QIODevice::ReadOnly ) ) {
        char buffer[16+1];
        dbfile.readLine(buffer, 16);
        QString contents = QString(buffer);
        dbfile.close();
        if (!contents.startsWith("SQLite format 3")) {
            lastErrorMessage = QObject::tr("File is not a SQLite 3 database");
            return false;
        }
    } else {
        lastErrorMessage = QObject::tr("File could not be read");
        return false;
    }

    lastErrorMessage = QObject::tr("no error");

    err = sqlite3_open_v2(db.toUtf8(), &_db, SQLITE_OPEN_READWRITE, NULL);
    if ( err ) {
        lastErrorMessage = QString::fromUtf8((const char*)sqlite3_errmsg(_db));
        sqlite3_close(_db);
        _db = 0;
        return false;
    }

    if (_db){
        // set preference defaults
        QSettings settings(QApplication::organizationName(), QApplication::organizationName());
        settings.sync();
        bool foreignkeys = settings.value( "/db/foreignkeys", false ).toBool();
        setPragma("foreign_keys", foreignkeys ? "1" : "0");

        if (SQLITE_OK==sqlite3_exec(_db,"PRAGMA empty_result_callbacks = ON;",
                                    NULL,NULL,NULL)){
            if (SQLITE_OK==sqlite3_exec(_db,"PRAGMA show_datatypes = ON;",
                                        NULL,NULL,NULL)){
                ok=true;
            }
            curDBFilename = db;
        }

        // Enable extension loading
        sqlite3_enable_load_extension(_db, 1);
    }

    return ok;
}

bool DBBrowserDB::setRestorePoint(const QString& pointname)
{
    if(!isOpen())
        return false;
    if(savepointList.contains(pointname))
        return true;

    if(_db)
    {
        QString query = QString("SAVEPOINT %1;").arg(pointname);
        sqlite3_exec(_db, query.toUtf8(), NULL, NULL, NULL);
        savepointList.append(pointname);
        emit dbChanged(getDirty());
    }
    return true;
}

bool DBBrowserDB::save(const QString& pointname)
{
    if(!isOpen() || savepointList.contains(pointname) == false)
        return false;

    if(_db)
    {
        QString query = QString("RELEASE %1;").arg(pointname);
        sqlite3_exec(_db, query.toUtf8(), NULL,NULL,NULL);
        savepointList.removeAll(pointname);
        emit dbChanged(getDirty());
    }
    return true;
}

bool DBBrowserDB::revert(const QString& pointname)
{
    if(!isOpen() || savepointList.contains(pointname) == false)
        return false;

    if(_db)
    {
        QString query = QString("ROLLBACK TO SAVEPOINT %1;").arg(pointname);
        sqlite3_exec(_db, query.toUtf8(), NULL, NULL, NULL);
        query = QString("RELEASE %1;").arg(pointname);
        sqlite3_exec(_db, query.toUtf8(), NULL, NULL, NULL);
        savepointList.removeAll(pointname);
        emit dbChanged(getDirty());
    }
    return true;
}

bool DBBrowserDB::saveAll()
{
    foreach(const QString& point, savepointList)
    {
        if(!save(point))
            return false;
    }
    return true;
}

bool DBBrowserDB::revertAll()
{
    foreach(const QString& point, savepointList)
    {
        if(!revert(point))
            return false;
    }
    return true;
}

bool DBBrowserDB::create ( const QString & db)
{
    bool ok=false;

    if (isOpen()) close();

    lastErrorMessage = QObject::tr("no error");

    // read encoding from settings and open with sqlite3_open for utf8
    // and sqlite3_open16 for utf16
    QSettings settings(QApplication::organizationName(), QApplication::organizationName());
    QString sEncoding = settings.value("/db/defaultencoding", "UTF-8").toString();

    int openresult = SQLITE_OK;

    if(sEncoding == "UTF-8" || sEncoding == "UTF8" || sEncoding == "Latin1")
        openresult = sqlite3_open(db.toUtf8(), &_db);
    else
        openresult = sqlite3_open16(db.utf16(), &_db);

    if( openresult != SQLITE_OK ){
        lastErrorMessage = QString::fromUtf8((const char*)sqlite3_errmsg(_db));
        sqlite3_close(_db);
        _db = 0;
        return false;
    }

    if (_db){
        if (SQLITE_OK==sqlite3_exec(_db,"PRAGMA empty_result_callbacks = ON;",
                                    NULL,NULL,NULL)){
            if (SQLITE_OK==sqlite3_exec(_db,"PRAGMA show_datatypes = ON;",
                                        NULL,NULL,NULL)){
                ok=true;
            }
            curDBFilename = db;
        }

        // Enable extension loading
        sqlite3_enable_load_extension(_db, 1);
    }

    return ok;
}


bool DBBrowserDB::close()
{
    if(_db)
    {
        if (getDirty())
        {
            QMessageBox::StandardButton reply = QMessageBox::question(0,
                                                                      QApplication::applicationName(),
                                                                      QObject::tr("Do you want to save the changes "
                                                                                  "made to the database file %1?").arg(curDBFilename),
                                                                      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

            // If the user clicked the cancel button stop here and return false
            if(reply == QMessageBox::Cancel)
                return false;

            // If he didn't it was either yes or no
            if(reply == QMessageBox::Yes)
                saveAll();
            else
                revertAll(); //not really necessary, I think... but will not hurt.
        }
        sqlite3_close(_db);
    }
    _db = 0;
    objMap.clear();
    savepointList.clear();
    emit dbChanged(getDirty());

    // Return true to tell the calling function that the closing wasn't cancelled by the user
    return true;
}

bool DBBrowserDB::dump(const QString& filename)
{
    // Open file
    QFile file(filename);
    if(file.open(QIODevice::WriteOnly))
    {
        // Create progress dialog. For this count the number of all table rows to be exported first;
        // this does neither take the table creation itself nor
        // indices, views or triggers into account but compared to the number of rows those should be neglectable
        unsigned int numRecordsTotal = 0, numRecordsCurrent = 0;
        QList<DBBrowserObject> tables = objMap.values("table");
        QMutableListIterator<DBBrowserObject> it(tables);
        while(it.hasNext())
        {
            it.next();

            // Remove the sqlite_stat1 table if there is one
            if(it.value().getname() == "sqlite_stat1" || it.value().getname() == "sqlite_sequence")
            {
                it.remove();
            } else {
                // Otherwise get the number of records in this table
                SqliteTableModel tableModel(0, this);
                tableModel.setTable(it.value().getname());
                numRecordsTotal += tableModel.totalRowCount();
            }
        }
        QProgressDialog progress(QObject::tr("Exporting database to SQL file..."),
                                 QObject::tr("Cancel"), 0, numRecordsTotal);
        progress.setWindowModality(Qt::ApplicationModal);

        // Regular expression to check for numeric strings
        QRegExp regexpIsNumeric("\\d*");

        // Open text stream to the file
        QTextStream stream(&file);

        // Put the SQL commands in a transaction block
        stream << "BEGIN TRANSACTION;\n";

        // Loop through all tables first as they are required to generate views, indices etc. later
        for(QList<DBBrowserObject>::ConstIterator it=tables.begin();it!=tables.end();++it)
        {
            // Write the SQL string used to create this table to the output file
            stream << (*it).getsql() << ";\n";

            // Get data of this table
            SqliteTableModel tableModel(0, this);
            tableModel.setTable((*it).getname());
            while(tableModel.canFetchMore())
                tableModel.fetchMore();

            // Dump all the content of the table
            for(int row=0;row<tableModel.totalRowCount();row++)
            {
                stream << "INSERT INTO `" << (*it).getname() << "` VALUES(";
                for(int col=1;col<tableModel.columnCount();col++)
                {
                    QString content;
                    if(tableModel.isBinary(tableModel.index(row, col)))
                    {
                        content = QString("X'%1'").arg(QString(tableModel.data(tableModel.index(row, col), Qt::EditRole).toByteArray().toHex()));
                        if(content.isNull())
                            content = "NULL";
                    } else {
                        content = tableModel.data(tableModel.index(row, col)).toString().replace("'", "''");
                        if(content.isNull())
                            content = "NULL";
                        else if(content.length() && !regexpIsNumeric.exactMatch(content))
                            content = "'" + content + "'";
                        else if(content.length() == 0)
                            content = "''";
                    }

                    stream << content;
                    if(col < tableModel.columnCount() - 1)
                        stream << ",";
                    else
                        stream << ");\n";
                }

                // Update progress dialog
                progress.setValue(++numRecordsCurrent);
                qApp->processEvents();
                if(progress.wasCanceled())
                {
                    file.close();
                    file.remove();
                    return false;
                }
            }
        }

        // Now dump all the other objects
        for(objectMap::ConstIterator it=objMap.begin();it!=objMap.end();++it)
        {
            // Make sure it's not a table again
            if(it.value().gettype() == "table")
                continue;

            // Write the SQL string used to create this object to the output file
            stream << (*it).getsql() << ";\n";
        }

        // Done
        stream << "COMMIT;\n";
        file.close();
        return true;
    } else {
        return false;
    }
}

bool DBBrowserDB::executeSQL ( const QString & statement, bool dirtyDB, bool logsql)
{
    char *errmsg;
    bool ok = false;

    if (!isOpen()) return false;

    if (_db){
        if (logsql) logSQL(statement, kLogMsg_App);
        if (dirtyDB) setRestorePoint();
        if (SQLITE_OK==sqlite3_exec(_db,statement.toUtf8(),
                                    NULL,NULL,&errmsg)){
            ok=true;
        }
    }

    if (!ok){
        lastErrorMessage = QString::fromUtf8(errmsg);
        qWarning() << "executeSQL: " << statement << "->" << lastErrorMessage;
        return false;
    }
    return true;
}

bool DBBrowserDB::executeMultiSQL(const QString& statement, bool dirty, bool log)
{
    // First check if a DB is opened
    if(!isOpen())
        return false;

    // Log the statement if needed
    if(log)
        logSQL(statement, kLogMsg_App);

    // Set DB to dirty/create restore point if necessary
    if(dirty)
        setRestorePoint();

    // Show progress dialog
    int statement_size = statement.size();
    QProgressDialog progress(QObject::tr("Executing SQL..."),
                             QObject::tr("Cancel"), 0, statement_size);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.show();

    // Execute the statement by looping until SQLite stops giving back a tail string
    sqlite3_stmt* vm;
    QByteArray utf8Query = statement.toUtf8();
    const char *tail = utf8Query.data();
    int res = 0;
    unsigned int line = 0;
    do
    {
        line++;
        size_t tail_length = strlen(tail);

        // Update progress dialog, keep UI responsive
        progress.setValue(statement_size - tail_length);
        qApp->processEvents();
        if(progress.wasCanceled())
        {
            lastErrorMessage = QObject::tr("Action cancelled.");
            return false;
        }

        // Execute next statement
        res = sqlite3_prepare_v2(_db, tail, tail_length, &vm, &tail);
        if(res == SQLITE_OK)
        {
            if(sqlite3_step(vm) == SQLITE_ERROR)
            {
                sqlite3_finalize(vm);
                lastErrorMessage = QObject::tr("Error in statement #%1: %2.\n"
                    "Aborting execution.").arg(line).arg(sqlite3_errmsg(_db));
                qWarning() << lastErrorMessage;
                return false;
            } else {
                sqlite3_finalize(vm);
            }
        } else {
            lastErrorMessage = QObject::tr("Error in statement #%1: %2.\n"
                "Aborting execution.").arg(line).arg(sqlite3_errmsg(_db));
            qWarning() << lastErrorMessage;
            return false;
        }
    } while(tail && *tail != 0 && (res == SQLITE_OK || res == SQLITE_DONE));

    // Exit
    return true;
}

bool DBBrowserDB::getRow(const QString& sTableName, int rowid, QList<QByteArray>& rowdata)
{
    QString sQuery = QString("SELECT * from %1 WHERE rowid=%2;").arg(sTableName).arg(rowid);
    QByteArray utf8Query = sQuery.toUtf8();
    sqlite3_stmt *stmt;

    int status = sqlite3_prepare_v2(_db, utf8Query, utf8Query.size(), &stmt, NULL);
    if(SQLITE_OK == status)
    {
        // even this is a while loop, the statement should always only return 1 row
        while(sqlite3_step(stmt) == SQLITE_ROW)
        {
            for (int i = 0; i < sqlite3_column_count(stmt); ++i)
                rowdata.append(QByteArray(static_cast<const char*>(sqlite3_column_blob(stmt, i)), sqlite3_column_bytes(stmt, i)));
        }
    }
    sqlite3_finalize(stmt);

    return status == SQLITE_OK;
}

int DBBrowserDB::addRecord(const QString& sTableName)
{
    char *errmsg;
    if (!isOpen()) return false;

    // add record is seldom called, for now this is ok
    // but if we ever going to add a lot of records
    // we should cache the parsed tables somewhere
    sqlb::Table table = sqlb::Table::parseSQL(getObjectByName(sTableName).getsql());
    QString sInsertstmt = table.emptyInsertStmt();
    lastErrorMessage = "";
    logSQL(sInsertstmt, kLogMsg_App);
    setRestorePoint();

    if (SQLITE_OK != sqlite3_exec(_db, sInsertstmt.toUtf8(), NULL, NULL, &errmsg))
    {
        lastErrorMessage = QString::fromUtf8(errmsg);
        qWarning() << "addRecord: " << lastErrorMessage;
        return -1;
    } else {
        return sqlite3_last_insert_rowid(_db);
    }
}

bool DBBrowserDB::deleteRecord(const QString& table, int rowid)
{
    char * errmsg;
    if (!isOpen()) return false;
    bool ok = false;
    lastErrorMessage = QString("no error");

    QString statement = QString("DELETE FROM `%1` WHERE rowid=%2;").arg(table).arg(rowid);

    if (_db){
        logSQL(statement, kLogMsg_App);
        setRestorePoint();
        if (SQLITE_OK==sqlite3_exec(_db,statement.toUtf8(),
                                    NULL,NULL,&errmsg)){
            ok=true;
        } else {
            lastErrorMessage = QString::fromUtf8(errmsg);
            qWarning() << "deleteRecord: " << lastErrorMessage;
        }
    }
    return ok;
}

bool DBBrowserDB::updateRecord(const QString& table, const QString& column, int row, const QByteArray& value)
{
    if (!isOpen()) return false;

    lastErrorMessage = QString("no error");

    QString sql = QString("UPDATE `%1` SET `%2`=? WHERE rowid=%3;").arg(table).arg(column).arg(row);

    logSQL(sql, kLogMsg_App);
    setRestorePoint();

    sqlite3_stmt* stmt;
    int success = 1;
    if(sqlite3_prepare_v2(_db, sql.toUtf8(), -1, &stmt, 0) != SQLITE_OK)
        success = 0;
    if(success == 1 && sqlite3_bind_text(stmt, 1, value.constData(), value.length(), SQLITE_STATIC) != SQLITE_OK)
        success = -1;
    if(success == 1 && sqlite3_step(stmt) != SQLITE_DONE)
        success = -1;
    if(success != 0 && sqlite3_finalize(stmt) != SQLITE_OK)
        success = -1;

    if(success == 1)
    {
        return true;
    } else {
        lastErrorMessage = sqlite3_errmsg(_db);
        qWarning() << "updateRecord: " << lastErrorMessage;
        return false;
    }
}

bool DBBrowserDB::createTable(const QString& name, const sqlb::FieldVector& structure)
{
    // Build SQL statement
    sqlb::Table table(name);
    for(int i=0;i<structure.size();i++)
        table.addField(structure.at(i));

    // Execute it and update the schema
    bool result = executeSQL(table.sql());
    updateSchema();
    return result;
}

bool DBBrowserDB::addColumn(const QString& tablename, const sqlb::FieldPtr& field)
{
    QString sql = QString("ALTER TABLE `%1` ADD COLUMN %2").arg(tablename).arg(field->toString());

    // Execute it and update the schema
    bool result = executeSQL(sql);
    updateSchema();
    return result;
}

bool DBBrowserDB::renameColumn(const QString& tablename, const QString& name, sqlb::FieldPtr to, int move)
{
    // NOTE: This function is working around the incomplete ALTER TABLE command in SQLite.
    // If SQLite should fully support this command one day, this entire
    // function can be changed to executing something like this:
    //QString sql;
    //if(to.isNull())
    //    sql = QString("ALTER TABLE `%1` DROP COLUMN `%2`;").arg(table).arg(column);
    //else
    //    sql = QString("ALTER TABLE `%1` MODIFY `%2` %3").arg(tablename).arg(to).arg(type);    // This is wrong...
    //return executeSQL(sql);

    // Collect information on the current DB layout
    QString tableSql = getObjectByName(tablename).getsql();
    if(tableSql.isEmpty())
    {
        lastErrorMessage = QObject::tr("renameColumn: cannot find table %1.").arg(tablename);
        qWarning() << lastErrorMessage;
        return false;
    }

    // Create table schema
    sqlb::Table oldSchema = sqlb::Table::parseSQL(tableSql);

    // Check if field actually exists
    if(oldSchema.findField(name) == -1)
    {
        lastErrorMessage = QObject::tr("renameColumn: cannot find column %1.").arg(name);
        qWarning() << lastErrorMessage;
        return false;
    }

    // Create savepoint to be able to go back to it in case of any error
    if(!executeSQL("SAVEPOINT sqlitebrowser_rename_column"))
    {
        lastErrorMessage = QObject::tr("renameColumn: creating savepoint failed. DB says: %1").arg(lastErrorMessage);
        qWarning() << lastErrorMessage;
        return false;
    }

    // Create a new table with a name that hopefully doesn't exist yet.
    // Its layout is exactly the same as the one of the table to change - except for the column to change
    // of course
    sqlb::Table newSchema = oldSchema;
    newSchema.setName("sqlitebrowser_rename_column_new_table");
    QString select_cols;
    if(to.isNull())
    {
        // We want drop the column - so just remove the field
        newSchema.removeField(name);

        for(int i=0;i<newSchema.fields().count();++i)
            select_cols.append(QString("`%1`,").arg(newSchema.fields().at(i)->name()));
        select_cols.chop(1);    // remove last comma
    } else {
        // We want to modify it

        // Move field
        int index = newSchema.findField(name);
        sqlb::FieldPtr temp = newSchema.fields().at(index);
        newSchema.setField(index, newSchema.fields().at(index + move));
        newSchema.setField(index + move, temp);

        // Get names of fields to select from old table now - after the field has been moved and before it might be renamed
        for(int i=0;i<newSchema.fields().count();++i)
            select_cols.append(QString("`%1`,").arg(newSchema.fields().at(i)->name()));
        select_cols.chop(1);    // remove last comma

        // Modify field
        newSchema.setField(index + move, to);
    }

    // Create the new table
    if(!executeSQL(newSchema.sql()))
    {
        lastErrorMessage = QObject::tr("renameColumn: creating new table failed. DB says: %1").arg(lastErrorMessage);
        qWarning() << lastErrorMessage;
        executeSQL("ROLLBACK TO SAVEPOINT sqlitebrowser_rename_column;");
        return false;
    }

    // Copy the data from the old table to the new one
    if(!executeSQL(QString("INSERT INTO sqlitebrowser_rename_column_new_table SELECT %1 FROM `%2`;").arg(select_cols).arg(tablename)))
    {
        lastErrorMessage = QObject::tr("renameColumn: copying data to new table failed. DB says:\n"
                                       "%1").arg(lastErrorMessage);
        qWarning() << lastErrorMessage;
        executeSQL("ROLLBACK TO SAVEPOINT sqlitebrowser_rename_column;");
        return false;
    }

    // Save all indices, triggers and views associated with this table because SQLite deletes them when we drop the table in the next step
    QString otherObjectsSql;
    for(objectMap::ConstIterator it=objMap.begin();it!=objMap.end();++it)
    {
        // If this object references the table and it's not the table itself save it's SQL string
        if((*it).getTableName() == tablename && (*it).gettype() != "table")
            otherObjectsSql += (*it).getsql() + "\n";
    }

    // Delete the old table
    if(!executeSQL(QString("DROP TABLE `%1`;").arg(tablename)))
    {
        lastErrorMessage = QObject::tr("renameColumn: deleting old table failed. DB says: %1").arg(lastErrorMessage);
        qWarning() << lastErrorMessage;
        executeSQL("ROLLBACK TO SAVEPOINT sqlitebrowser_rename_column;");
        return false;
    }

    // Rename the temporary table
    if(!renameTable("sqlitebrowser_rename_column_new_table", tablename))
    {
        executeSQL("ROLLBACK TO SAVEPOINT sqlitebrowser_rename_column;");
        return false;
    }

    // Restore the saved triggers, views and indices
    if(!executeMultiSQL(otherObjectsSql, true, true))
    {
        QMessageBox::information(0, qApp->applicationName(), QObject::tr("Restoring some of the objects associated with this table failed. "
                                                                         "This is most likely because some column names changed. "
                                                                         "Here's the SQL statement which you might want to fix and execute manually:\n\n")
                                 + otherObjectsSql);
    }

    // Release the savepoint - everything went fine
    if(!executeSQL("RELEASE SAVEPOINT sqlitebrowser_rename_column;"))
    {
        lastErrorMessage = QObject::tr("renameColumn: releasing savepoint failed. DB says: %1").arg(lastErrorMessage);
        qWarning() << lastErrorMessage;
        return false;
    }

    // Success, update the DB schema before returning
    updateSchema();
    return true;
}

bool DBBrowserDB::renameTable(const QString& from_table, const QString& to_table)
{
    QString sql = QString("ALTER TABLE `%1` RENAME TO `%2`").arg(from_table, to_table);
    if(!executeSQL(sql))
    {
        QString error = QObject::tr("Error renaming table '%1' to '%2'."
            "Message from database engine:\n%3").arg(from_table).arg(to_table).arg(lastErrorMessage);
        lastErrorMessage = error;
        qWarning() << lastErrorMessage;
        return false;
    } else {
        updateSchema();
        return true;
    }
}

QStringList DBBrowserDB::getBrowsableObjectNames() const
{
    objectMap::ConstIterator it;
    QStringList res;

    for(it=objMap.begin();it!=objMap.end();++it)
    {
        if(it.key() == "table" || it.key() == "view")
            res.append(it.value().getname());
    }

    return res;
}

objectMap DBBrowserDB::getBrowsableObjects() const
{
    objectMap::ConstIterator it;
    objectMap res;

    for(it=objMap.begin();it!=objMap.end();++it)
    {
        if(it.key() == "table" || it.key() == "view")
            res.insert(it.key(), it.value());
    }

    return res;
}

QStringList DBBrowserDB::getTableFields(const QString & tablename) const
{
    objectMap::ConstIterator it;
    QStringList res;

    for ( it = objMap.begin(); it != objMap.end(); ++it )
    {
        if((*it).getname() == tablename)
        {
            for(int i=0;i<(*it).fldmap.size();i++)
                res.append((*it).fldmap.at(i)->name());
        }
    }
    return res;
}

DBBrowserObject DBBrowserDB::getObjectByName(const QString& name) const
{
    objectMap::ConstIterator it;
    QStringList res;

    for ( it = objMap.begin(); it != objMap.end(); ++it )
    {
        if((*it).getname() == name)
            return *it;
    }
    return DBBrowserObject();
}

void DBBrowserDB::logSQL(QString statement, int msgtype)
{
    // Replace binary log messages by a placeholder text instead of printing gibberish
    for(int i=0;i<statement.size();i++)
    {
        if(statement.at(i) < 32 && statement.at(i) != '\n')
        {
            statement.truncate(32);
            statement.append(QObject::tr("... <string can not be logged, contains binary data> ..."));

            // early exit if we detect a binary character,
            // to prevent checking all characters in a potential big string
            break;
        }
    }

    emit sqlExecuted(statement, msgtype);
}

void DBBrowserDB::updateSchema( )
{
    sqlite3_stmt *vm;
    const char *tail;
    int err=0;
    lastErrorMessage = QObject::tr("no error");

    objMap.clear();

    // Exit here is no DB is opened
    if(!isOpen())
        return;

    QString statement = "SELECT type,name,sql,tbl_name FROM sqlite_master UNION SELECT type,name,sql,tbl_name FROM sqlite_temp_master;";

    QByteArray utf8Statement = statement.toUtf8();
    err=sqlite3_prepare_v2(_db, utf8Statement, utf8Statement.length(),
                        &vm, &tail);
    if (err == SQLITE_OK){
        logSQL(statement, kLogMsg_App);
        while ( sqlite3_step(vm) == SQLITE_ROW ){
            QString val1 = QString::fromUtf8((const char*)sqlite3_column_text(vm, 0));
            QString val2 = QString::fromUtf8((const char*)sqlite3_column_text(vm, 1));
            QString val3 = QString::fromUtf8((const char*)sqlite3_column_text(vm, 2));
            QString val4 = QString::fromUtf8((const char*)sqlite3_column_text(vm, 3));
            val3.replace("\r", "");

            if(val1 == "table" || val1 == "index" || val1 == "view" || val1 == "trigger")
                objMap.insert(val1, DBBrowserObject(val2, val3, val1, val4));
            else
                qWarning() << QObject::tr("unknown object type %1").arg(val1);
        }
        sqlite3_finalize(vm);
    }else{
        qWarning() << QObject::tr("could not get list of db objects: %1, %2").arg(err).arg(sqlite3_errmsg(_db));
    }

    //now get the field list for each table
    objectMap::Iterator it;
    for ( it = objMap.begin(); it != objMap.end(); ++it )
    {
        // Use our SQL parser to generate the field list for tables. For views we currently have to fall back to the
        // pragma SQLite offers.
        if((*it).gettype() == "table")
        {
            sqlb::Table t((*it).getname());
            (*it).fldmap = t.parseSQL((*it).getsql()).fields();
        } else if((*it).gettype() == "view") {
            statement = QString("PRAGMA TABLE_INFO(`%1`);").arg((*it).getname());
            logSQL(statement, kLogMsg_App);
            err=sqlite3_prepare_v2(_db,statement.toUtf8(),statement.length(),
                                &vm, &tail);
            if (err == SQLITE_OK){
                while ( sqlite3_step(vm) == SQLITE_ROW ){
                    if (sqlite3_column_count(vm)==6)
                    {
                        QString val_name = QString::fromUtf8((const char *)sqlite3_column_text(vm, 1));
                        QString val_type = QString::fromUtf8((const char *)sqlite3_column_text(vm, 2));

                        sqlb::FieldPtr f(new sqlb::Field(val_name, val_type));
                        (*it).addField(f);
                    }
                }
                sqlite3_finalize(vm);
            } else{
                lastErrorMessage = QObject::tr("could not get types");
            }
        }
    }
}

QStringList DBBrowserDB::decodeCSV(const QString & csvfilename, char sep, char quote, int maxrecords, int * numfields)
{
    QFile file(csvfilename);
    QStringList result;
    *numfields = 0;
    int recs = 0;

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return result;
    }

    //Other than QFile, the QTextStream-class properly detects 2-Byte QChars and converts them accordingly (UTF-8)
    QTextStream inStream(&file);

    QProgressDialog progress(QObject::tr("Decoding CSV file..."), QObject::tr("Cancel"), 0, file.size());
    progress.setWindowModality(Qt::ApplicationModal);



    while (!inStream.atEnd()) {

        bool inquotemode = false;
        bool inescapemode = false;
        QString line = "";
        QString current = "";

        line = inStream.readLine();

        //For every Line, we iterate over the single QChars
        QString::ConstIterator i = line.begin();

        while (i != line.end()) {

            QChar c = *i;

            if (c==quote){
                if (inquotemode){
                    if (inescapemode){
                        inescapemode = false;
                        //add the escaped char here
                        current.append(c);
                    } else {
                        //are we escaping, or just finishing the quote?
                        i++; //Performing lookahead using the iterator
                        QChar d = *i;

                        if (d==quote) {
                            inescapemode = true;
                        } else {
                            inquotemode = false;
                        }
                        i--;
                    }
                } else {
                    inquotemode = true;
                }
            } else if (c==sep) {
                if (inquotemode){
                    //add the sep here
                    current.append(c);
                } else {
                    //not quoting, start new record
                    result << current;
                    current = "";
                }
            } else if (c==10 || c==13) {
                if (inquotemode){
                    //add the newline/carrier return
                    current.append(c);
                }
            } else if (c==32) {
              
              // Only append blanks if we are inside of quotes
              if (inquotemode || quote == 0) {
                current.append(c);
              }

            } else {//another character type
                current.append(c);
            }

            i++;
        }

        //Moved this block from (c==10), as line-separation is now handeled by the outer-loop
        result << current;

        if (*numfields == 0){
            *numfields = result.count();
        }
        recs++;
        progress.setValue(file.pos());
        qApp->processEvents();

        if ( (progress.wasCanceled() || recs>maxrecords) && maxrecords!=-1) {
            break;
        }
    }

    file.close();

    return result;

}

QString DBBrowserDB::getPragma(const QString& pragma)
{
    if(!isOpen())
        return "";

    QString sql = QString("PRAGMA %1").arg(pragma);
    sqlite3_stmt* vm;
    const char* tail;
    QString retval = "";

    // Get value from DB
    int err = sqlite3_prepare_v2(_db, sql.toUtf8(), sql.toUtf8().length(), &vm, &tail);
    if(err == SQLITE_OK){
        logSQL(sql, kLogMsg_App);
        if(sqlite3_step(vm) == SQLITE_ROW)
            retval = QString::fromUtf8((const char *) sqlite3_column_text(vm, 0));
        else
            qWarning() << QObject::tr("didn't receive any output from pragma %1").arg(pragma);

        sqlite3_finalize(vm);
    } else {
        qWarning() << QObject::tr("could not execute pragma command: %1, %2").arg(err).arg(sqlite3_errmsg(_db));
    }

    // Return it
    return retval;
}

bool DBBrowserDB::setPragma(const QString& pragma, const QString& value)
{
    // Set the pragma value
    QString sql = QString("PRAGMA %1 = \"%2\";").arg(pragma).arg(value);

    save();
    bool res = executeSQL(sql, false, true); // PRAGMA statements are usually not transaction bound, so we can't revert
    if( !res )
        qWarning() << QObject::tr("Error setting pragma %1 to %2: %3").arg(pragma).arg(value).arg(lastErrorMessage);
    return res;
}

bool DBBrowserDB::setPragma(const QString& pragma, const QString& value, QString& originalvalue)
{
    if( originalvalue != value )
    {
        if( setPragma(pragma, value))
        {
            originalvalue = value;
            return true;
        }
    }
    return false;
}

bool DBBrowserDB::setPragma(const QString& pragma, int value, int& originalvalue)
{
    if( originalvalue != value )
    {
        QString val = QString::number(value);
        QString origval = QString::number(originalvalue);
        if( setPragma(pragma, val, origval))
        {
            originalvalue = value;
        }
    }
    return false;
}

bool DBBrowserDB::loadExtension(const QString& filename)
{
    if(!isOpen())
        return false;

    // Check if file exists
    if(!QFile::exists(filename))
    {
        lastErrorMessage = QObject::tr("File not found.");
        return false;
    }

    // Try to load extension
    char* error;
    if(sqlite3_load_extension(_db, filename.toUtf8(), 0, &error) == SQLITE_OK)
    {
        return true;
    } else {
        lastErrorMessage = QString::fromUtf8(error);
        sqlite3_free(error);
        return false;
    }
}
