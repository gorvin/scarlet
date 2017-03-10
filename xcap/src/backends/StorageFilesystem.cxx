#include "scarlet/xcap/Storage.h"
#include "bmu/Logger.h"
#include <iostream>
#include <fstream>
#include <map>

namespace scarlet {
namespace xcap {

//TODO: trebalo bi sinhronizovati pristup fajlsistemu
class StorageFilesystemImpl {
    boost::filesystem::path const base;
    boost::filesystem::path const subxcadb;
    boost::filesystem::path const etags_fullname;
    boost::filesystem::path const users_fullname;
    std::time_t                   users_fullname_time;//last write time

    //etags se ucitaju u kreatoru iz etags_fname i sacuvaju u destruktoru u isti fajl
    std::map<boost::filesystem::path, std::string> etags_map;
    std::map<std::string, std::string> users_map;

    void update_users_map(void);

    boost::filesystem::path make_filepath(
        document_selector_t const& uri
        , std::string const& domain
    ) const;

	int store_etags(void);

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

    int del(
        document_selector_t const& uri
        , std::string const& etag
        , std::string const& domain
    );

    int user(std::string& digest, std::string const& username);

    StorageFilesystemImpl(
        boost::filesystem::path const& base
        , boost::filesystem::path const& subxcadb
        , boost::filesystem::path const& etags_fname
        , boost::filesystem::path const& users_fname
    );
    ~StorageFilesystemImpl();
};


StorageFilesystemImpl::StorageFilesystemImpl(
    boost::filesystem::path const& base
    , boost::filesystem::path const& subxcadb
    , boost::filesystem::path const& etags_fname
    , boost::filesystem::path const& users_fname
)
 : base(base)
 , subxcadb(subxcadb)
 , etags_fullname(base/subxcadb/etags_fname)
 , users_fullname(base/subxcadb/users_fname)
 , users_fullname_time()
 , etags_map()
 , users_map()
{
    std::ifstream fetags(etags_fullname.string().c_str());

    if(!fetags.good()) {
        DBGMSGAT("Storage etags file not good: " << etags_fullname.string());
    }

    std::istreambuf_iterator<char> it(fetags);
    std::istreambuf_iterator<char> itend;

    while(fetags.good()) {
        std::string docname;
        for( ; it!= itend && *it != '\t'; ++it) docname.push_back(*it);

        if(it == itend) break;

        std::string docetag;
        for(++it ; it!= itend && *it != '\n'; ++it) docetag.push_back(*it);

        if(it == itend) break;

        if(docname.empty() || docetag.empty()) continue;

        etags_map.insert(std::pair<boost::filesystem::path, std::string>(docname, docetag));
    }

    update_users_map();
}


StorageFilesystemImpl::~StorageFilesystemImpl()
{
	store_etags();
}


int StorageFilesystemImpl::store_etags(void)
{
	std::ofstream fetags(etags_fullname.string().c_str());
	if (!fetags.good()) {
		DBGMSGAT("Storage etags file not good: " << etags_fullname.string());
		return -2;
	}
	std::map<boost::filesystem::path, std::string>::iterator it(etags_map.begin());
	for (; it != etags_map.end(); ++it) {
		fetags << it->first << "\t" << it->second << "\n";
	}

	return 0;
}


void StorageFilesystemImpl::update_users_map(void)
{
    std::ifstream fusers(users_fullname.string().c_str());

    if(!fusers.good()) {
        DBGMSGAT("Storage users file not good: " << users_fullname.string());
        return;
    }

    std::istreambuf_iterator<char> it(fusers);
    std::istreambuf_iterator<char> itend;

    while(fusers.good()) {
        std::string username;
        for( ; it!= itend && *it != '\t'; ++it) username.push_back(*it);

        if(it == itend) break;

        std::string digest;
        for(++it ; it!= itend && *it != '\n'; ++it) digest.push_back(*it);

        if(it == itend) break;

        if(username.empty() || digest.empty()) continue;

        users_map.insert(std::pair<std::string, std::string>(username, digest));
    }
    users_fullname_time = boost::filesystem::last_write_time(users_fullname);
}


boost::filesystem::path
StorageFilesystemImpl::make_filepath(
    document_selector_t const& uri
    , std::string const& domain
) const
{
	u8vector_t safe_root(uri.root);
	for (auto& c : safe_root) {
		if ((unsigned char)':' == c) c = (unsigned char)'-';
	}
	u8vector_t safe_subtree(uri.subtree);
	for (auto& c : safe_subtree) {
		if ((unsigned char)'/' == c) c = (unsigned char)boost::filesystem::path::preferred_separator;
	}
	return base/subxcadb
        /bmu::utf8_string(safe_root)
        /domain
        /bmu::utf8_string(uri.auid)
        /bmu::utf8_string(safe_subtree)
        /bmu::utf8_string(uri.docname);
}

int StorageFilesystemImpl::get(
    u8vector_t& doc
   , std::string& etag
   , document_selector_t const& uri
   , std::string const& domain
)
{
    boost::filesystem::path filepath(make_filepath(uri, domain));
	{
		std::wclog << "Loading document from file: " << filepath.string();
		std::basic_ifstream<u8unit_t> ifs(filepath.string().c_str(), std::ios_base::binary);
		if (!ifs.good()) {
			std::wclog << " WARNING: file not good" << std::endl;
		}
		else {
			doc.assign(std::istreambuf_iterator<u8unit_t>(ifs), std::istreambuf_iterator<u8unit_t>());
			std::wclog << " loaded" << std::endl;
		}
	}
	etag.clear();
	if (!doc.empty())
	{
		std::map<boost::filesystem::path, std::string>::iterator it(etags_map.find(filepath));
		if (it == etags_map.end()) return -2;
		etag = it->second;
	}

    return 0;
}


int StorageFilesystemImpl::put(
    document_selector_t const& uri
    , rawcontent_t const& doc
    , std::string const& etagnew
    , std::string const& /*etagprev*/
    , std::string const& domain
)
{
    //etagprev bi se mogao iskoristiti za provjeru konzistentnosti rada
    assert(!etagnew.empty());

    boost::filesystem::path filepath(make_filepath(uri, domain));
    {
		auto parent_dir = filepath.parent_path();
		bool bSuccess = boost::filesystem::create_directories(parent_dir);
		//std::string sErr = boost::system::error_code().message();
        std::ofstream fdoc(filepath.string().c_str());
		if(!fdoc.good()) {
			DBGMSGAT("Can't open storage output file: " << filepath.string());
			return -2;
		}
        fdoc.write((char const*)doc.content, doc.length);
		if (!fdoc.good()) {
			DBGMSGAT("Error writing to storage output file: " << filepath.string());
			return -3;
		}
	}

    etags_map[filepath] = etagnew;

    return 0;
}


int StorageFilesystemImpl::del(
    document_selector_t const& uri
    , std::string const& etagprev
    , std::string const& domain
)
{
    assert(!etagprev.empty());

    boost::filesystem::path filepath(make_filepath(uri, domain));
    boost::filesystem::remove(filepath);

    etags_map.erase(filepath);

    return 0;
}


int StorageFilesystemImpl::user(std::string& digest, std::string const& username)
{
    std::time_t const new_time(boost::filesystem::last_write_time(users_fullname));
    if(new_time > users_fullname_time) update_users_map();

    std::map<std::string, std::string>::iterator it(users_map.find(username));
    if(it == users_map.end()) {
        digest.clear();
        return -2;
    }

    digest = it->second;

    return 0;
}


StorageFilesystem::StorageFilesystem(
    boost::filesystem::path const& base
    , boost::filesystem::path const& subxcadb
    , boost::filesystem::path const& etags_fname
    , boost::filesystem::path const& users_fname
)
 : impl(new StorageFilesystemImpl(base, subxcadb, etags_fname, users_fname))
 { }


StorageFilesystem::~StorageFilesystem() { }


int StorageFilesystem::get(
    u8vector_t& doc
    , std::string& etag
    , document_selector_t const& uri
    , std::string const& domain
)
{
    return impl->get(doc, etag, uri, domain);
}


int StorageFilesystem::put(
    document_selector_t const& uri
    , rawcontent_t const& doc
    , std::string const& etagnew
    , std::string const& etagprev
    , std::string const& domain
)
{
    return impl->put(uri, doc, etagnew, etagprev, domain);
}


int StorageFilesystem::del(
    document_selector_t const& uri
    , std::string const& etagprev
    , std::string const& domain
)
{
    return impl->del(uri, etagprev, domain);
}


int StorageFilesystem::user(std::string& digest, std::string const& username)
{
    return impl->user(digest, username);
}

}
}
