#include "testsqlobjects.h"
#include "../sqlitetypes.h"

#include <QtTest/QtTest>

using namespace sqlb;

void TestTable::sqlOutput()
{
    Table tt("testtable");
    FieldPtr f = FieldPtr(new Field("id", "integer"));
    f->setPrimaryKey(true);
    FieldPtr fkm = FieldPtr(new Field("km", "integer", false, "", "km > 1000"));
    fkm->setPrimaryKey(true);
    tt.addField(f);
    tt.addField(FieldPtr(new Field("car", "text")));
    tt.addField(fkm);

    QCOMPARE(tt.sql(), QString("CREATE TABLE `testtable` (\n"
                               "\t`id`\tinteger,\n"
                               "\t`car`\ttext,\n"
                               "\t`km`\tinteger CHECK(km > 1000),\n"
                               "\tPRIMARY KEY(id,km)\n"
                               ");"));
}

void TestTable::autoincrement()
{
    Table tt("testtable");
    FieldPtr f = FieldPtr(new Field("id", "integer"));
    f->setPrimaryKey(true);
    f->setAutoIncrement(true);
    FieldPtr fkm = FieldPtr(new Field("km", "integer"));
    tt.addField(f);
    tt.addField(FieldPtr(new Field("car", "text")));
    tt.addField(fkm);

    QCOMPARE(tt.sql(), QString("CREATE TABLE `testtable` (\n"
                               "\t`id`\tinteger PRIMARY KEY AUTOINCREMENT,\n"
                               "\t`car`\ttext,\n"
                               "\t`km`\tinteger\n"
                               ");"));
}

void TestTable::notnull()
{
    Table tt("testtable");
    FieldPtr f = FieldPtr(new Field("id", "integer"));
    f->setPrimaryKey(true);
    f->setAutoIncrement(true);
    FieldPtr fkm = FieldPtr(new Field("km", "integer"));
    tt.addField(f);
    tt.addField(FieldPtr(new Field("car", "text", true)));
    tt.addField(fkm);

    QCOMPARE(tt.sql(), QString("CREATE TABLE `testtable` (\n"
                               "\t`id`\tinteger PRIMARY KEY AUTOINCREMENT,\n"
                               "\t`car`\ttext NOT NULL,\n"
                               "\t`km`\tinteger\n"
                               ");"));
}

void TestTable::withoutRowid()
{
    Table tt("testtable");
    FieldPtr f = FieldPtr(new Field("a", "integer"));
    f->setPrimaryKey(true);
    f->setAutoIncrement(true);
    tt.addField(f);
    tt.addField(FieldPtr(new Field("b", "integer")));
    tt.setRowidColumn("a");

    QCOMPARE(tt.sql(), QString("CREATE TABLE `testtable` (\n"
                               "\t`a`\tinteger PRIMARY KEY AUTOINCREMENT,\n"
                               "\t`b`\tinteger\n"
                               ") WITHOUT ROWID;"));
}

void TestTable::parseSQL()
{
    QString sSQL = "create TABLE hero (\n"
            "\tid integer PRIMARY KEY AUTOINCREMENT,\n"
            "\tname text NOT NULL DEFAULT 'xxxx',\n"
            "\tinfo VARCHAR(255) CHECK (info == 'x')\n"
            ");";

    Table tab = Table::parseSQL(sSQL);

    QVERIFY(tab.name() == "hero");
    QVERIFY(tab.rowidColumn() == "rowid");
    QVERIFY(tab.fields().at(0)->name() == "id");
    QVERIFY(tab.fields().at(1)->name() == "name");
    QVERIFY(tab.fields().at(2)->name() == "info");

    QVERIFY(tab.fields().at(0)->type() == "integer");
    QVERIFY(tab.fields().at(1)->type() == "text");
    QVERIFY(tab.fields().at(2)->type() == "VARCHAR(255)");

    QVERIFY(tab.fields().at(0)->autoIncrement());
    QVERIFY(tab.fields().at(0)->primaryKey());
    QVERIFY(tab.fields().at(1)->notnull());
    QCOMPARE(tab.fields().at(1)->defaultValue(), QString("'xxxx'"));
    QCOMPARE(tab.fields().at(1)->check(), QString(""));
    QCOMPARE(tab.fields().at(2)->check(), QString("info=='x'"));
}

void TestTable::parseSQLdefaultexpr()
{
    QString sSQL = "CREATE TABLE chtest(\n"
            "id integer primary key,\n"
            "dumpytext text default('axa') CHECK(dumpytext == \"aa\"),\n"
            "zoi integer)";

    Table tab = Table::parseSQL(sSQL);

    QVERIFY(tab.name() == "chtest");
    QVERIFY(tab.fields().at(0)->name() == "id");
    QVERIFY(tab.fields().at(1)->name() == "dumpytext");
    QVERIFY(tab.fields().at(2)->name() == "zoi");

    QVERIFY(tab.fields().at(0)->type() == "integer");
    QVERIFY(tab.fields().at(1)->type() == "text");
    QVERIFY(tab.fields().at(2)->type() == "integer");

    QCOMPARE(tab.fields().at(1)->defaultValue(), QString("('axa')"));
    QCOMPARE(tab.fields().at(1)->check(), QString("dumpytext==\"aa\""));
    QCOMPARE(tab.fields().at(2)->defaultValue(), QString(""));
    QCOMPARE(tab.fields().at(2)->check(), QString(""));

    QVERIFY(tab.fields().at(0)->primaryKey());
}

void TestTable::parseSQLMultiPk()
{
    QString sSQL = "CREATE TABLE hero (\n"
            "\tid1 integer,\n"
            "\tid2 integer,\n"
            "\tnonpkfield blob,\n"
            "PRIMARY KEY(id1,id2)\n"
            ");";

    Table tab = Table::parseSQL(sSQL);

    QVERIFY(tab.name() == "hero");
    QVERIFY(tab.fields().at(0)->name() == "id1");
    QVERIFY(tab.fields().at(1)->name() == "id2");

    QVERIFY(tab.fields().at(0)->type() == "integer");
    QVERIFY(tab.fields().at(1)->type() == "integer");

    QVERIFY(tab.fields().at(0)->primaryKey());
    QVERIFY(tab.fields().at(1)->primaryKey());
}

void TestTable::parseSQLForeignKey()
{
    QString sSQL = "CREATE TABLE grammar_test(id, test, FOREIGN KEY(test) REFERENCES other_table);";

    Table tab = Table::parseSQL(sSQL);

    QVERIFY(tab.name() == "grammar_test");
    QVERIFY(tab.fields().at(0)->name() == "id");
    QVERIFY(tab.fields().at(1)->name() == "test");
}

void TestTable::parseSQLSingleQuotes()
{
    QString sSQL = "CREATE TABLE 'test'('id','test');";

    Table tab = Table::parseSQL(sSQL);

    QVERIFY(tab.name() == "test");
    QVERIFY(tab.fields().at(0)->name() == "id");
    QVERIFY(tab.fields().at(1)->name() == "test");
}

void TestTable::parseSQLKeywordInIdentifier()
{
    QString sSQL = "CREATE TABLE deffered(key integer primary key, if text);";

    Table tab = Table::parseSQL(sSQL);

    QVERIFY(tab.name() == "deffered");
    QVERIFY(tab.fields().at(0)->name() == "key");
    QVERIFY(tab.fields().at(1)->name() == "if");
}

void TestTable::parseSQLWithoutRowid()
{
    QString sSQL = "CREATE TABLE test(a integer primary key, b integer) WITHOUT ROWID;";

    Table tab = Table::parseSQL(sSQL);

    QVERIFY(tab.fields().at(tab.findPk())->name() == "a");
    QVERIFY(tab.rowidColumn() == "a");
}

void TestTable::parseNonASCIIChars()
{
    QString sSQL = "CREATE TABLE `lösung` ("
            "`Fieldöäüß`	INTEGER,"
            "PRIMARY KEY(Fieldöäüß)"
            ");";

    Table tab = Table::parseSQL(sSQL);

    QVERIFY(tab.name() == "lösung");
    QVERIFY(tab.fields().at(0)->name() == "Fieldöäüß");
}

QTEST_MAIN(TestTable)
//#include "testsqlobjects.moc"
