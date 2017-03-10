#include "StorageSqlite3Db.h"
#if defined(WITH_BACKEND_SQLITE3)
#include "bmu/tydefs.h"
#include "scarlet/xcap/xcadefs.h"
#include <sqlite3xx/except.hpp>
#include <iostream>
#include <boost/filesystem.hpp>

namespace scarlet {
namespace xcap {

void EnsureSqliteDb(std::string const& dbpath, std::string const& db_xtable, std::string const& db_utable)
{
	boost::filesystem::path fdbpath(dbpath);
	if (boost::filesystem::exists(fdbpath))
		return;
#ifndef NDEBUG
	try {
		sqlite3xx::connection conn(dbpath);
		sqlite3xx::work dbwork(conn);
		std::string sqlCreateUTable = "CREATE TABLE " + db_utable + " (username TEXT primary key, digest TEXT not null)";
		sqlite3xx::result r1 = dbwork.exec(sqlCreateUTable);
		std::string sqlCreateXTable =
			"CREATE TABLE " + db_xtable + " (xid TEXT not null, auid TEXT not null,"
			" filename TEXT not null, document TEXT not null, etag TEXT not null, created INTEGER not null,"
			" modified INTEGER not null, primary key(xid, auid, filename))";
		sqlite3xx::result r2 = dbwork.exec(sqlCreateXTable);
		std::string sqlAddDefaultUser = "INSERT INTO " + db_utable + " (username, digest) VALUES('myname@localdomain', 'mypass')";
		sqlite3xx::result r3 = dbwork.exec(sqlAddDefaultUser);
		dbwork.commit();
		//int result1 = 0;
		//if (r1.size() > 0) {
		//	std::cout << r1[0][0] << std::endl;
		//	r1[0][0].to(result1);
		//}
	}
	catch (const std::exception &e) {
		std::wcerr << e.what() << std::endl;
	}
#else
	std::wcerr << "ERROR: Sqlite backend not initialized, contact administrator or run sqlite3-create-tables.sql." << std::endl;
	std::wclog << "ERROR: Sqlite backend not initialized, contact administrator or run sqlite3-create-tables.sql." << std::endl;
#endif
}

void sqlitexx_exception_handler(void)
{
    std::string msg;
    try {
        throw;
    }
	catch (storage_error&) {
        throw;
    }
	catch (sqlite3xx::sql_error const& e) {
        msg = std::string("sql exception for query: ") + e.query() + "\n reason: " + e.what();
        //std::wclog << msg << std::endl;
        throw boost::enable_current_exception(storage_error()) << bmu::errinfo_message(msg);
    }
	catch (sqlite3xx::failure const& e) {
        msg = std::string("unknown pqxx failure: ") + e.what();
        //std::wclog << msg << std::endl;
        throw boost::enable_current_exception(storage_error()) << bmu::errinfo_message(msg);
    }
	catch (sqlite3xx::sqlitexx_exception const& e) {
        msg = std::string("unknown pqxx exception: ") + e.base().what();
        //std::wclog << msg << std::endl;
        throw boost::enable_current_exception(storage_error()) << bmu::errinfo_message(msg);
    }
	catch (std::exception const& e) {
        msg = std::string("unknown std exception in pqxx: ") + e.what();
        //std::wclog << msg << std::endl;
        throw boost::enable_current_exception(storage_error()) << bmu::errinfo_message(msg);
    }
	catch (char const* const& e) {
        msg = std::string("local-informational exception: ") + e;
        //std::wclog << msg << std::endl;
        throw boost::enable_current_exception(storage_error()) << bmu::errinfo_message(msg);
    }
	catch (...) {
        msg = "unhandled unknown pqxx exception";
        //std::wclog << msg << std::endl;
        throw boost::enable_current_exception(storage_error()) << bmu::errinfo_message(msg);
    }
}

commited_ignore& commited_ignore::noapply(void)
{
    static commited_ignore a;
    return a;
}

std::string const GetDocStatement::retrieve("get_document");

GetDocStatement::GetDocStatement(std::string const& db_xtable, sqlite3xx::connection& C, table_xcap_row_t const& row)
 : row(row)
{
    std::string ststr("SELECT etag, document FROM ");
    ststr.append(db_xtable)
         .append(" WHERE auid=$1 AND xid=$2 AND filename=$3");

   C.prepare(retrieve, ststr);
}

sqlite3xx::prepare::invocation GetDocStatement::operator()(sqlite3xx::transaction& T) const
{
	return T.prepared(retrieve)(row.auid)(row.xid)(row.filename);
}

void GetDocApply::operator()(sqlite3xx::result const& r)
{
    std::wclog << "Query successfully commited." << std::endl;
    if(r.size() > 1) {//Rezultat je vise redova iz tabele
        throw boost::enable_current_exception(storage_error()) << bmu::errinfo_message(
            "Inconsistent table: multiple (etag, document) found for given (auid, xid, filename)"
        );
    }
    if(r.size() > 0) {//Rezultat ima jedan red iz tabele
        if(r.begin().size() < 2)//Iz reda tabele nije dobijeno jedno od trazenih polja (etag, doc).
            throw boost::enable_current_exception(storage_error()) << bmu::errinfo_message(
                "Incorrect statement: not found etag or doc for given (auid, xid, filename)"
            );
        r.begin()[0].to(to_etag);//Prvo trazeno polje iz reda tabele
        r.begin()[1].to(to_document);//Drugo trazeno polje iz reda tabele
        std::wclog << "Found doc with etag: " << to_etag.c_str() << std::endl;
    } else {
        to_etag.clear();
        to_document.clear();
        std::wclog << "Not found doc" << std::endl;
    }
}

std::string const InsertDocStatement::inserter("put_document_insert");

InsertDocStatement::InsertDocStatement(std::string const& db_xtable, sqlite3xx::connection& C, table_xcap_row_t const& row)
 : row(row)
{
    std::string ststr("INSERT INTO ");
    ststr.append(db_xtable)
         .append(" (etag, auid, xid, filename, document, created, modified) "
                 "VALUES ($1, $2, $3, $4, $5, strftime('%s','now'), strftime('%s','now'))");

    C.prepare(inserter, ststr);
}

//NOTE: za row.xid.empty() ce baza odbaciti insert/update jer mora biti not null
sqlite3xx::prepare::invocation InsertDocStatement::operator()(sqlite3xx::transaction& T) const
{ 
	u8vector_t u8doc(row.document.content, row.document.length);
	return T.prepared(inserter)(row.etag)(row.auid)(row.xid, !row.xid.empty())(row.filename)(u8doc); 
}

std::string const UpdateDocStatement::updater("put_document_update");

UpdateDocStatement::UpdateDocStatement(
	std::string const& db_xtable
	, sqlite3xx::connection& C
    , table_xcap_row_t const& row
    , std::string const& etagprev)
	: row(row)
	, etagprev(etagprev)
{
    std::string ststr("UPDATE ");
    ststr.append(db_xtable)
         .append(" SET etag=$1, document=$2, modified=strftime('%s','now')"
                 " WHERE etag=$3 AND auid=$4 AND xid=$5 AND filename=$6");

    C.prepare(updater, ststr);
}

//NOTE: za row.xid.empty() ce baza odbaciti insert/update jer mora biti not null
sqlite3xx::prepare::invocation UpdateDocStatement::operator()(sqlite3xx::transaction& T) const
{ 
	u8vector_t u8doc(row.document.content, row.document.length);
	return T.prepared(updater)(row.etag)(u8doc)(etagprev)(row.auid)(row.xid, !row.xid.empty())(row.filename);
}

std::string const DelDocStatement::deleter("delete_document");

DelDocStatement::DelDocStatement(std::string const& db_xtable, sqlite3xx::connection& C, table_xcap_row_t const& row)
 : row(row)
{
    std::string ststr("DELETE FROM ");
    ststr.append(db_xtable)
         .append(" WHERE etag=$1 AND auid=$2 AND xid=$3 AND filename=$4");
    C.prepare(deleter, ststr);
}

sqlite3xx::prepare::invocation DelDocStatement::operator()(sqlite3xx::transaction& T) const
{
    assert(!row.etag.empty());//pozivajuci treba prije provjeriti etag i za empty dati odziv 404
    return T.prepared(deleter)(row.etag)(row.auid)(row.xid, !row.xid.empty())(row.filename);
}

std::string const GetUserStatement::retrieve_user("get_user");

GetUserStatement::GetUserStatement(std::string const& db_utable, sqlite3xx::connection& C, std::string const& username)
 : username(username)
{
    std::string ststr("SELECT digest FROM ");
    ststr.append(db_utable)
         .append(" WHERE username=$1");
    C.prepare(retrieve_user, ststr);
}

sqlite3xx::prepare::invocation GetUserStatement::operator()(sqlite3xx::transaction& T) const
{
    std::wclog << "Querying database for username: " << username.c_str() << std::endl;
    return T.prepared(retrieve_user)(username);
}

void GetUserApply::operator()(sqlite3xx::result const& r)
{
    std::wclog << "Query successfully commited." << std::endl;
    if(r.size() > 1) {//Rezultat je vise redova iz tabele
        throw boost::enable_current_exception(storage_error()) << bmu::errinfo_message(
            "Inconsistent table: multiple (digest) found for given (username)"
        );
    }
    if(r.size() > 0) {//Rezultat ima jedan red iz tabele
        if(r.begin().size() < 1)//Iz reda tabele nije dobijeno jedno od trazenih polja (etag, doc).
            throw boost::enable_current_exception(storage_error()) << bmu::errinfo_message(
                "Incorrect statement: not found (digest) for given (username)"
            );
        r.begin()[0].to(to_digest);//Prvo trazeno polje iz reda tabele
        std::wclog << "Found username with digest: " << to_digest.c_str() << std::endl;
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
