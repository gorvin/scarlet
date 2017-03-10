#include "scarlet/xcap/Storage.h"
#if defined(WITH_BACKEND_SQLITE3)
#include "StorageSqlite3Db.h"
#include <iostream>

namespace scarlet {
namespace xcap {

class StorageSqlite3Impl {
    sqlite3xx::connection dbconn;
    table_xcap_row_t   row;
    std::string        userrow;
    u8vector_t         to_doc;
    std::string        to_etag;
    std::string        to_digest;
    GetDocStatement    docget_statement;
    GetDocApply        docget_apply;
    InsertDocStatement docinsert_statement;
    UpdateDocStatement docupdate_statement;
    DelDocStatement    docdel_statement;
    GetUserStatement   userget_statement;
    GetUserApply       userget_apply;
    explicit StorageSqlite3Impl(void); //NE
    void set_row(std::string const& etag
                 , document_selector_t const& uri
                 , rawcontent_t const& doc
                 , std::string const& domain);
public:
    int get(
        u8vector_t& doc
        , std::string& etag
        , document_selector_t const& uri
        , std::string const& domain
    );
    //za create treba biti etagprev == 0 a za update sadrzi dosadasnji etag dokumenta koji se mijenja
    int put(
        document_selector_t const& uri
        , rawcontent_t const& doc
        , std::string const& etagnew
        , std::string const& etagprev
        , std::string const& domain
    );
    int del(document_selector_t const& uri, std::string const& etag, std::string const& domain);
    int user(std::string& digest, std::string const& username);
    StorageSqlite3Impl(boost::filesystem::path const& dbpath, std::string const& db_xpath, std::string const& db_upath);
};

StorageSqlite3Impl::StorageSqlite3Impl(boost::filesystem::path const& dbpath, std::string const& db_xtable, std::string const& db_utable)
 : dbconn(dbpath.string())
 , row()
 , userrow()
 , to_doc()
 , to_etag()
 , to_digest()
 , docget_statement(db_xtable, dbconn, row)
 , docget_apply(to_doc, to_etag)
 , docinsert_statement(db_xtable, dbconn, row)
 , docupdate_statement(db_xtable, dbconn, row, to_etag)
 , docdel_statement(db_xtable, dbconn, row)
 , userget_statement(db_utable, dbconn, userrow)
 , userget_apply(to_digest)
{
    row.document.content = 0;
    row.document.length = 0;
}

void StorageSqlite3Impl::set_row(
    std::string const& etag
    , document_selector_t const& uri
    , rawcontent_t const& doc
    , std::string const& domain)
{
    row.etag.assign(etag.begin(), etag.end());
    row.auid.assign(uri.auid.begin(), uri.auid.end());

    row.xid.assign(uri.xui.begin(), uri.xui.end());//NOTE: za global je samo '@domain'
    row.xid.push_back('@');
    row.xid.append(domain);

    row.filename.assign(uri.docname.begin(), uri.docname.end());
    row.document = doc;
}

int StorageSqlite3Impl::get(
    u8vector_t& doc
    , std::string& etag
    , document_selector_t const& uri
    , std::string const& domain)
{
    rawcontent_t nulldoc = { 0, 0 };
    set_row(std::string(), uri, nulldoc, domain);
    try {
		prepared_transactor(docget_statement, docget_apply)(sqlite3xx::transaction(dbconn));
    } catch (...) {
        sqlitexx_exception_handler();
        return -2;
    }
    to_doc.swap(doc);
    to_etag.swap(etag);
    return 0;
}

int StorageSqlite3Impl::put(
    document_selector_t const& uri
    , rawcontent_t const& doc
    , std::string const& etagnew
    , std::string const& etagprev
    , std::string const& domain)
{
    assert(!etagnew.empty());
    set_row(etagnew, uri, doc, domain);

    try {
        if(etagprev.empty()) {
			prepared_transactor T(docinsert_statement);
			T(sqlite3xx::transaction(dbconn));
        } else {
            to_etag = etagprev;
			prepared_transactor T(docupdate_statement);
			T(sqlite3xx::transaction(dbconn));
        }
    } catch (...) {
        sqlitexx_exception_handler();
        return -2;
    }
    return 0;
}

int StorageSqlite3Impl::del(document_selector_t const& uri, std::string const& etagprev, std::string const& domain)
{
    rawcontent_t nulldoc = { 0, 0 };
    assert(!etagprev.empty());
    set_row(etagprev, uri, nulldoc, domain);

    try {
		prepared_transactor T(docdel_statement);
		T(sqlite3xx::transaction(dbconn));
    } catch (...) {
        sqlitexx_exception_handler();
        return -2;
    }
    return 0;
}

int StorageSqlite3Impl::user(std::string& digest, std::string const& username)
{
    to_digest.clear();
    userrow = username;

    try {
		prepared_transactor(userget_statement, userget_apply)(sqlite3xx::transaction(dbconn));
    } catch (...) {
        sqlitexx_exception_handler();
        return -2;
    }

    to_digest.swap(digest);

    return 0;
}

StorageSqlite3Impl* create_StorageSqlite3Impl(boost::filesystem::path const& dbpath, std::string const& db_xtable, std::string const& db_utable)
{
    std::wclog << "Connecting to database file: " << dbpath << std::endl;
    StorageSqlite3Impl* p = 0;
    try {
		EnsureSqliteDb(dbpath.string(), db_xtable, db_utable);
        p = new StorageSqlite3Impl(dbpath, db_xtable, db_utable);
    } catch(...) {
        p = 0;
        sqlitexx_exception_handler();
        //std::wclog << "Failed connection to database" << std::endl;
        //std::terminate(); //TODO: kad budes znao gdje ces ga uhvatiti, baci failed_db_connection
    }
    std::wclog << "Connection to database suceeded" << std::endl;
    return p;
}

StorageSqlite3::StorageSqlite3(boost::filesystem::path const& dbpath, std::string const& db_xtable, std::string const& db_utable)
 : impl(create_StorageSqlite3Impl(dbpath, db_xtable, db_utable))
 { }

StorageSqlite3::~StorageSqlite3() { }

int StorageSqlite3::get(
    u8vector_t& doc
    , std::string& etag
    , document_selector_t const& uri
    , std::string const& domain)
{
    return impl->get(doc, etag, uri, domain);
}

int StorageSqlite3::put(
    document_selector_t const& uri
    , rawcontent_t const& doc
    , std::string const& etagnew
    , std::string const& etagprev
    , std::string const& domain)
{
    return impl->put(uri, doc, etagnew, etagprev, domain);
}

int StorageSqlite3::del(document_selector_t const& uri, std::string const& etagprev, std::string const& domain)
{
    return impl->del(uri, etagprev, domain);
}

int StorageSqlite3::user(std::string& digest, std::string const& username)
{
    return impl->user(digest, username);
}

}
}
#else // WITH_BACKEND_POSTGRESQL
int this_definition_prevents_linker_warning_storage_postgresql_cxx = 0;
#endif // WITH_BACKEND_POSTGRESQL
