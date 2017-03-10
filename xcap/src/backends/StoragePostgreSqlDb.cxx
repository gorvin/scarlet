#include "StoragePostgreSqlDb.h"
#if defined(WITH_BACKEND_POSTGRESQL)
#include "bmu/tydefs.h"
#include "bmu/exdefs.h"
#include <scarlet/sqlite3xx/except.hpp>
#include <iostream>


namespace pqxx {


template<> struct string_traits<scarlet::u8vector_t>
{
    static const char *name() { return "u8vector"; }

    static bool has_null() { return false; }

    static bool is_null(const scarlet::u8vector_t &) { return false; }

    static scarlet::u8vector_t null()
    {
        internal::throw_null_conversion(name()); return scarlet::u8vector_t();
    }

    static void from_string(const char Str[], scarlet::u8vector_t &Obj)
    {
        Obj.assign(Str, Str+std::char_traits<char>::length(Str));
    }

    static PGSTD::string to_string(const scarlet::u8vector_t &Obj)
    {
        return PGSTD::string(Obj.begin(), Obj.end());
    }
};


template<> struct string_traits<scarlet::rawcontent_t>
{
    static const char *name() { return "rawcontent_t"; }

    static bool has_null() { return true; }

    static bool is_null(const scarlet::rawcontent_t& val) { return val.content == 0; }

    static scarlet::rawcontent_t null()
    {
        internal::throw_null_conversion(name());
		scarlet::rawcontent_t nulldata = {0, 0};
        return nulldata;
    }

    static PGSTD::string to_string(const scarlet::rawcontent_t &Obj)
    {
        return PGSTD::string((char const*)Obj.content, Obj.length);
    }
};


}


namespace scarlet {
namespace xcap {

void pqxx_exception_handler(void)
{
    std::string msg;

    try {

        throw;

    } catch (storage_error&) {

        throw;

    } catch (pqxx::sql_error const& e) {

        msg = std::string("sql exception for query: ") + e.query() + "\n reason: " + e.what();
        //std::wclog << msg << std::endl;
        throw boost::enable_current_exception(storage_error()) << errinfo_message(msg);

    } catch (pqxx::failure const& e) {

        msg = std::string("unknown pqxx failure: ") + e.what();
        //std::wclog << msg << std::endl;
        throw boost::enable_current_exception(storage_error()) << errinfo_message(msg);

    } catch (pqxx::pqxx_exception const& e) {

        msg = std::string("unknown pqxx exception: ") + e.base().what();
        //std::wclog << msg << std::endl;
        throw boost::enable_current_exception(storage_error()) << errinfo_message(msg);

    } catch (std::exception const& e) {

        msg = std::string("unknown std exception in pqxx: ") + e.what();
        //std::wclog << msg << std::endl;
        throw boost::enable_current_exception(storage_error()) << errinfo_message(msg);

    } catch (char const* const& e) {

        msg = std::string("local-informational exception: ") + e;
        //std::wclog << msg << std::endl;
        throw boost::enable_current_exception(storage_error()) << errinfo_message(msg);

    } catch (...) {

        msg = "unhandled unknown pqxx exception";
        //std::wclog << msg << std::endl;
        throw boost::enable_current_exception(storage_error()) << errinfo_message(msg);

    }
}


commited_ignore& commited_ignore::noapply(void)
{
    static commited_ignore a;
    return a;
}


std::string const GetDocStatement::retrieve("get_document");


GetDocStatement::GetDocStatement(std::string const& db_xtable, pqxx::connection& C, table_xcap_row_t const& row)
 : row(row)
{
    std::string ststr("SELECT etag, document FROM ");
    ststr.append(db_xtable)
         .append(" WHERE auid=$1 AND xid=$2 AND filename=$3");

    C.prepare(retrieve, ststr)("text")("text")("text");
}


pqxx::prepare::invocation GetDocStatement::operator()(pqxx::transaction_base& T) const
{
    return T.prepared(retrieve)(row.auid)(row.xid)(row.filename);
}


void GetDocApply::operator()(pqxx::result const& r)
{
    std::wclog << "Query successfully commited." << std::endl;
    if(r.size() > 1) {//Rezultat je vise redova iz tabele
        throw boost::enable_current_exception(storage_error()) << errinfo_message(
            "Inconsistent table: multiple (etag, document) found for given (auid, xid, filename)"
        );
    }
    if(!r.empty()) {//Rezultat ima jedan red iz tabele
        if(r.front().size() < 2)//Iz reda tabele nije dobijeno jedno od trazenih polja (etag, doc).
            throw boost::enable_current_exception(storage_error()) << errinfo_message(
                "Incorrect statement: not found etag or doc for given (auid, xid, filename)"
            );
        r.front()[0].as<std::string>().swap(to_etag);//Prvo trazeno polje iz reda tabele
        r.front()[1].as<u8vector_t>().swap(to_document);//Drugo trazeno polje iz reda tabele
        std::wclog << "Found doc with etag: " << to_etag << std::endl;
    } else {
        to_etag.clear();
        to_document.clear();
        std::wclog << "Not found doc" << std::endl;
    }
}


std::string const InsertDocStatement::inserter("put_document_insert");


InsertDocStatement::InsertDocStatement(std::string const& db_xtable, pqxx::connection& C, table_xcap_row_t const& row)
 : row(row)
{
    std::string ststr("INSERT INTO ");
    ststr.append(db_xtable)
         .append(" (etag, auid, xid, filename, document, tstamp.created, tstamp.modified) "
                 "VALUES ($1, $2, $3, $4, $5, 'now', 'now')");

    C.prepare(inserter, ststr)("uuid")("text")("text")("text")("text");
}


//NOTE: za row.xid.empty() ce baza odbaciti insert/update jer mora biti not null
pqxx::prepare::invocation InsertDocStatement::operator()(pqxx::transaction_base& T) const
 { return T.prepared(inserter)(row.etag)(row.auid)(row.xid, !row.xid.empty())(row.filename)(row.document); }


std::string const UpdateDocStatement::updater("put_document_update");


UpdateDocStatement::UpdateDocStatement(
	std::string const& db_xtable
	, pqxx::connection& C
    , table_xcap_row_t const& row
    , std::string const& etagprev
)
 : row(row),
   etagprev(etagprev)
{
    std::string ststr("UPDATE ");
    ststr.append(db_xtable)
         .append(" SET etag=$1, document=$2, tstamp.modified='now'"
                 " WHERE etag=$3 AND auid=$4 AND xid=$5 AND filename=$6");

    C.prepare(updater, ststr)("uuid")("text")("uuid")("text")("text")("text");
}


//NOTE: za row.xid.empty() ce baza odbaciti insert/update jer mora biti not null
pqxx::prepare::invocation UpdateDocStatement::operator()(pqxx::transaction_base& T) const
 { return T.prepared(updater)(row.etag)(row.document)(etagprev)(row.auid)(row.xid, !row.xid.empty())(row.filename); }


std::string const DelDocStatement::deleter("delete_document");


DelDocStatement::DelDocStatement(std::string const& db_xtable, pqxx::connection& C, table_xcap_row_t const& row)
 : row(row)
{
    std::string ststr("DELETE FROM ");
    ststr.append(db_xtable)
         .append(" WHERE etag=$1 AND auid=$2 AND xid=$3 AND filename=$4");

    C.prepare(deleter, ststr)("uuid")("text")("text")("text");
}


pqxx::prepare::invocation DelDocStatement::operator()(pqxx::transaction_base& T) const
{
    assert(!row.etag.empty());//pozivajuci treba prije provjeriti etag i za empty dati odziv 404

    return T.prepared(deleter)(row.etag)(row.auid)(row.xid, !row.xid.empty())(row.filename);
}


std::string const GetUserStatement::retrieve_user("get_user");


GetUserStatement::GetUserStatement(std::string const& db_utable, pqxx::connection& C, std::string const& username)
 : username(username)
{
    std::string ststr("SELECT digest FROM ");
    ststr.append(db_utable)
         .append(" WHERE username=$1");

    C.prepare(retrieve_user, ststr)("text");
}


pqxx::prepare::invocation GetUserStatement::operator()(pqxx::transaction_base& T) const
{
    std::wclog << "Querying database for username: " << username << std::endl;
    return T.prepared(retrieve_user)(username);
}


void GetUserApply::operator()(pqxx::result const& r)
{
    std::wclog << "Query successfully commited." << std::endl;
    if(r.size() > 1) {//Rezultat je vise redova iz tabele
        throw boost::enable_current_exception(storage_error()) << errinfo_message(
            "Inconsistent table: multiple (digest) found for given (username)"
        );
    }
    if(!r.empty()) {//Rezultat ima jedan red iz tabele
        if(r.front().size() < 1)//Iz reda tabele nije dobijeno jedno od trazenih polja (etag, doc).
            throw boost::enable_current_exception(storage_error()) << errinfo_message(
                "Incorrect statement: not found (digest) for given (username)"
            );
        r.front()[0].as<std::string>().swap(to_digest);//Prvo trazeno polje iz reda tabele
        std::wclog << "Found username with digest: " << to_digest << std::endl;
    } else {
        to_digest.clear();
        std::wclog << "Not found username" << std::endl;
    }
}

}
}
#else // WITH_BACKEND_POSTGRESQL
int this_definition_prevents_linker_warning_storage_postgresql_db_cxx = 0;
#endif // WITH_BACKEND_POSTGRESQL
