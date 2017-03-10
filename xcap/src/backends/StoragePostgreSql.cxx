#include "scarlet/xcap/Storage.h"
#if defined(WITH_BACKEND_POSTGRESQL)
#include "StoragePostgreSqlDb.h"
#include <iostream>


namespace scarlet {
namespace xcap {

class StoragePostgreSqlImpl {
    pqxx::connection dbconn;

    table_xcap_row_t row;
    std::string      userrow;

    u8vector_t       to_doc;
    std::string      to_etag;
    std::string      to_digest;


    GetDocStatement    docget_statement;
    GetDocApply        docget_apply;
    InsertDocStatement docinsert_statement;
    UpdateDocStatement docupdate_statement;
    DelDocStatement    docdel_statement;
    GetUserStatement   userget_statement;
    GetUserApply       userget_apply;

    explicit StoragePostgreSqlImpl(void); //NE

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

    StoragePostgreSqlImpl(std::string const& options, std::string const& db_xtable, std::string const& db_utable);
};


StoragePostgreSqlImpl::StoragePostgreSqlImpl(std::string const& options, std::string const& db_xtable, std::string const& db_utable)
 : dbconn(options)
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


void StoragePostgreSqlImpl::set_row(
    std::string const& etag
    , document_selector_t const& uri
    , rawcontent_t const& doc
    , std::string const& domain
)
{
    row.etag.assign(etag.begin(), etag.end());
    row.auid.assign(uri.auid.begin(), uri.auid.end());

    row.xid.assign(uri.xui.begin(), uri.xui.end());//NOTE: za global je samo '@domain'
    row.xid.push_back('@');
    row.xid.append(domain);

    row.filename.assign(uri.docname.begin(), uri.docname.end());
    row.document = doc;
}


int StoragePostgreSqlImpl::get(
    u8vector_t& doc
    , std::string& etag
    , document_selector_t const& uri
    , std::string const& domain
)
{
    rawcontent_t nulldoc = { 0, 0 };
    set_row(std::string(), uri, nulldoc, domain);

    try {
        dbconn.perform(prepared_transactor(docget_statement, docget_apply));
    } catch (...) {
        pqxx_exception_handler();
        return -2;
    }

    to_doc.swap(doc);
    to_etag.swap(etag);

    return 0;
}


int StoragePostgreSqlImpl::put(
    document_selector_t const& uri
    , rawcontent_t const& doc
    , std::string const& etagnew
    , std::string const& etagprev
    , std::string const& domain
)
{
    assert(!etagnew.empty());
    set_row(etagnew, uri, doc, domain);

    try {
        if(etagprev.empty()) {
            dbconn.perform(prepared_transactor(docinsert_statement));
        } else {
            to_etag = etagprev;
            dbconn.perform(prepared_transactor(docupdate_statement));
        }
    } catch (...) {
        pqxx_exception_handler();
        return -2;
    }
    return 0;
}


int StoragePostgreSqlImpl::del(document_selector_t const& uri, std::string const& etagprev, std::string const& domain)
{
    rawcontent_t nulldoc = { 0, 0 };
    assert(!etagprev.empty());
    set_row(etagprev, uri, nulldoc, domain);

    try {
        dbconn.perform(prepared_transactor(docdel_statement));
    } catch (...) {
        pqxx_exception_handler();
        return -2;
    }
    return 0;
}


int StoragePostgreSqlImpl::user(std::string& digest, std::string const& username)
{
    to_digest.clear();
    userrow = username;

    try {
        dbconn.perform(prepared_transactor(userget_statement, userget_apply));
    } catch (...) {
        pqxx_exception_handler();
        return -2;
    }

    to_digest.swap(digest);

    return 0;
}


StoragePostgreSqlImpl* create_StoragePostgreSqlImpl(std::string const& options, std::string const& db_xtable, std::string const& db_utable)
{
    std::wclog << "Connecting to database with options: " << options << std::endl;

    StoragePostgreSqlImpl* p = 0;

    try {
        p = new StoragePostgreSqlImpl(options, db_xtable, db_utable);
    } catch(...) {
        p = 0;
        pqxx_exception_handler();
        //std::wclog << "Failed connection to database" << std::endl;
        //std::terminate(); //TODO: kad budes znao gdje ces ga uhvatiti, baci failed_db_connection
    }
    std::wclog << "Connection to database suceeded" << std::endl;
    return p;
}


StoragePostgreSql::StoragePostgreSql(std::string const& options, std::string const& db_xtable, std::string const& db_utable)
 : impl(create_StoragePostgreSqlImpl(options, db_xtable, db_utable))
 { }


StoragePostgreSql::~StoragePostgreSql() { }


int StoragePostgreSql::get(
    u8vector_t& doc
    , std::string& etag
    , document_selector_t const& uri
    , std::string const& domain
)
{
    return impl->get(doc, etag, uri, domain);
}


int StoragePostgreSql::put(
    document_selector_t const& uri
    , rawcontent_t const& doc
    , std::string const& etagnew
    , std::string const& etagprev
    , std::string const& domain
)
{
    return impl->put(uri, doc, etagnew, etagprev, domain);
}


int StoragePostgreSql::del(document_selector_t const& uri, std::string const& etagprev, std::string const& domain)
{
    return impl->del(uri, etagprev, domain);
}


int StoragePostgreSql::user(std::string& digest, std::string const& username)
{
    return impl->user(digest, username);
}

}
}
#else // WITH_BACKEND_POSTGRESQL
int this_definition_prevents_linker_warning_storage_postgresql_cxx = 0;
#endif // WITH_BACKEND_POSTGRESQL
