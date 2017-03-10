#include <scarlet/xcap/xcadefs.h>
#include <boost/filesystem/operations.hpp>
#include <boost/scoped_ptr.hpp>

namespace scarlet {
namespace xcap {

struct Storage {
    virtual int get(
        u8vector_t& doc
        , std::string& etag
        , document_selector_t const& uri
        , std::string const& domain
    ) = 0;

    //etagprev je empty za insert
    virtual int put(
        document_selector_t const& uri
        , rawcontent_t const& doc
        , std::string const& etagnew
        , std::string const& etagprev
        , std::string const& domain
    ) = 0;

    virtual int del(
        document_selector_t const& uri
        , std::string const& etagprev
        , std::string const& domain
    ) = 0;

    virtual int user(std::string& digest, std::string const& username) = 0;

    virtual ~Storage() { }
};

#if defined(WITH_BACKEND_POSTGRESQL)
class StoragePostgreSqlImpl;

class StoragePostgreSql : public Storage {
    boost::scoped_ptr<StoragePostgreSqlImpl> const impl;

public:
    int get(
        u8vector_t& doc
        , std::string& etag
        , document_selector_t const& uri
        , std::string const& domain
    );

    int put(
        document_selector_t const& uri
        , rawcontent_t const& doc
        , std::string const& etagnew
        , std::string const& etagprev
        , std::string const& domain
    );

    int del(
        document_selector_t const& uri
        , std::string const& etagprev
        , std::string const& domain
    );

    int user(std::string& digest, std::string const& username);

    StoragePostgreSql(std::string const& options, std::string const& db_xtable, std::string const& db_utable);

    ~StoragePostgreSql();
};
#endif // WITH_BACKEND_POSTGRESQL

#if defined(WITH_BACKEND_SQLITE3)
class StorageSqlite3Impl;

class StorageSqlite3 : public Storage {
	boost::scoped_ptr<StorageSqlite3Impl> const impl;

public:
	int get(
		u8vector_t& doc
		, std::string& etag
		, document_selector_t const& uri
		, std::string const& domain
		);

	int put(
		document_selector_t const& uri
		, rawcontent_t const& doc
		, std::string const& etagnew
		, std::string const& etagprev
		, std::string const& domain
		);

	int del(
		document_selector_t const& uri
		, std::string const& etagprev
		, std::string const& domain
		);

	int user(std::string& digest, std::string const& username);

	StorageSqlite3(boost::filesystem::path const& dbpath, std::string const& db_xtable /*Options::instance().db_xtable()*/, std::string const& db_utable /*Options::instance().db_utable()*/);

	~StorageSqlite3();
};
#endif // WITH_BACKEND_SQLITE3

class StorageFilesystemImpl;


class StorageFilesystem : public Storage {
    boost::scoped_ptr<StorageFilesystemImpl> const impl;

public:
    int get(
        u8vector_t& doc
        , std::string& etag
        , document_selector_t const& uri
        , std::string const& domain
    );

    int put(
        document_selector_t const& uri
        , rawcontent_t const& doc
        , std::string const& etagnew
        , std::string const& etagprev
        , std::string const& domain
    );

    int del(
        document_selector_t const& uri
        , std::string const& etagprev
        , std::string const& domain
    );

    int user(std::string& digest, std::string const& username);

    StorageFilesystem(
		boost::filesystem::path const& base
        , boost::filesystem::path const& subxcadb = "xcadb"
        , boost::filesystem::path const& etags_fname = "etagsdb.txt"
        , boost::filesystem::path const& users_fname = "usersdb.txt"
    );

    ~StorageFilesystem();
};


}
}