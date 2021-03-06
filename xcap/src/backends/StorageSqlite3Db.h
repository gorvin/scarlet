#include "bmu/tydefs.h"
#include "scarlet/xcap/xcadefs.h"
#if defined(WITH_BACKEND_SQLITE3)

#include <sqlite3xx/transaction.hpp>
#include <sqlite3xx/connection.hpp>

namespace scarlet {
namespace xcap {

void sqlitexx_exception_handler(void);

/** Funktori izvedeni iz statement_base i commited_base se koriste kao argumenti prepared_transactor
  i definisu njegovu funkcionalnost. Iz statement_base se izvode funktori za postavljanje varijabli
  pripremljenog iskaza u toku transakcije - funktor iskaza. Pri njihovom kreiranju obicno se trebaju
  definisati ti pridruzeni iskazi. Iz commited_base se izvode funktori za obradu rezultata uspjesno
  izvrsenog pripremljenog iskaza. Bitni rezultati se moraju kopirati, npr. u interne varijable, jer
  vaze samo u toku izvrsenja funktora - funktor rezultata.
 */
struct statement_base {
    virtual sqlite3xx::prepare::invocation operator()(sqlite3xx::transaction& T) const = 0;
};

void EnsureSqliteDb(std::string const& dbpath, std::string const& db_xtable, std::string const& db_utable);

/** @inheritdoc statement_base */
struct commited_base {
    virtual void operator()(sqlite3xx::result const& r) = 0;
};


/** Funktor za ignorisanje rezultata. Koristi se kad nije bitan rezultat izvrsenja iskaza */
struct commited_ignore : public commited_base {
    void operator()(sqlite3xx::result const&) { }
    static commited_ignore& noapply(void);
};


/** Transaktor koga definisu funktori statement_base i commited_base. Izvrsava se pozivom
   C.perform gdje je C objekat tipa sqlite3xx::connection.
 */
class prepared_transactor /*: public sqlite3xx::transactor< >*/ {
    statement_base const& s;
    commited_base&  a;

public:
    void operator()(sqlite3xx::transaction& T) { 
		sqlite3xx::result r(s(T).exec()); 
		a(r);
	}

    prepared_transactor(statement_base& s, commited_base& a = commited_ignore::noapply())
     : s(s)
     , a(a)
     { }
};


/** Glavni tip za predstavljanje xcap tabele pri komunikaciji sa bazom. */
struct table_xcap_row_t {
    std::string etag;
    std::string auid;
    std::string xid;
    std::string filename;
    rawcontent_t document;//NOTE: sve osim ovog se dobija iz URI
};


/** Funktor iskaza za dobijanje etag i sadrzaja fajla odredjenog sa auid, xid i filename.
    Rezultat se prihvata u objektu tipa GetDocApply.
 */
class GetDocStatement : public statement_base {
    static std::string const retrieve;
    table_xcap_row_t const& row;

public:
    sqlite3xx::prepare::invocation operator()(sqlite3xx::transaction& T) const;
    GetDocStatement(std::string const& db_xtable, sqlite3xx::connection& C, table_xcap_row_t const& row);
};


/** Funktor za prihvatanje rezultata nakon izvrsenja iskaza GetDocStatement. */
class GetDocApply : public commited_base {
    std::string& to_etag;
    u8vector_t& to_document;

public:
    void operator()(sqlite3xx::result const& r);
    GetDocApply(u8vector_t& to_document, std::string& to_etag)
     : to_etag(to_etag)
     , to_document(to_document)
     { }
};


class InsertDocStatement : public statement_base {
    static std::string const inserter;

    table_xcap_row_t const& row;

public:
    sqlite3xx::prepare::invocation operator()(sqlite3xx::transaction& T) const;

    InsertDocStatement(std::string const& db_xtable, sqlite3xx::connection& C, table_xcap_row_t const& row);
};


class UpdateDocStatement : public statement_base {
    static std::string const updater;

    table_xcap_row_t const& row;
    std::string const& etagprev;

public:
    sqlite3xx::prepare::invocation operator()(sqlite3xx::transaction& T) const;

    UpdateDocStatement(
		std::string const& db_xtable
		, sqlite3xx::connection& C
        , table_xcap_row_t const& row
        , std::string const& etagprev
    );
};


class DelDocStatement : public statement_base {
    static std::string const deleter;

    table_xcap_row_t const& row;

public:
    sqlite3xx::prepare::invocation operator()(sqlite3xx::transaction& T) const;

    DelDocStatement(std::string const& db_xtable, sqlite3xx::connection& C, table_xcap_row_t const& row);
};


/** Funktor iskaza za dobijanje etag i sadrzaja fajla odredjenog sa auid, xid i filename.
    Rezultat se prihvata u objektu tipa GetDocApply.
 */
class GetUserStatement : public statement_base {
    static std::string const retrieve_user;
    std::string const& username;

public:
    sqlite3xx::prepare::invocation operator()(sqlite3xx::transaction& T) const;
    GetUserStatement(std::string const& db_utable, sqlite3xx::connection& C, std::string const& username);
};


/** Funktor za prihvatanje rezultata nakon izvrsenja iskaza GetDocStatement. */
class GetUserApply : public commited_base {
    std::string& to_digest;

public:
    void operator()(sqlite3xx::result const& r);
    GetUserApply(std::string& to_digest)
     : to_digest(to_digest)
     { }
};

}
}
#endif // WITH_BACKEND_POSTGRESQL