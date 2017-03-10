#include "Options.h"
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/program_options.hpp>
//namespace po = boost::program_options;
using boost::program_options::value;
using boost::program_options::bool_switch;
using boost::program_options::options_description;
using boost::program_options::positional_options_description;
using boost::program_options::variables_map;
using boost::program_options::command_line_parser;
#include <boost/thread/thread.hpp>
#include <iostream>
#include <fstream>

//configurable defaults
#define DEFAULT_OPTION_LOCALE "en_US"
#define DEFAULT_OPTION_STORAGE_DIR boost::filesystem::system_complete(__argv[0]).parent_path()
#define DEFAULT_OPTION_BASE_DIR boost::filesystem::current_path()
#define DEFAULT_OPTION_CONFIG_NAME "scarlet.config"
#define DEFAULT_OPTION_XSD_SUBDIR "xsd"
#define DEFAULT_OPTION_DOMAIN_NAME "localdomain"
#define DEFAULT_OPTION_ROOT_URI "localhost"
#define DEFAULT_OPTION_ROOT_URI_2 "127.0.0.1"
#define DEFAULT_OPTION_DB_CONNECT_STRING "host=/tmp dbname=postgres"
#define DEFAULT_OPTION_DB_XML_TABLE "xcaptree"
#define DEFAULT_OPTION_DB_USER_TABLE "xcapusers"
//#define DEFAULT_OPTION_STORAGE "filesystem"
//#define DEFAULT_OPTION_STORAGE "postgresql"
#define DEFAULT_OPTION_STORAGE "sqlite3"

namespace scarlet {

Options& Options::instance(void)
{
    static Options one;
    return one;
}

Options::Options(void)
 : _nthreads(std::max(1u, boost::thread::hardware_concurrency()))
 , _locale(DEFAULT_OPTION_LOCALE)
 , _base_path(DEFAULT_OPTION_STORAGE_DIR.string())
 , _start_path(DEFAULT_OPTION_BASE_DIR.string())
 , _config_file((DEFAULT_OPTION_BASE_DIR / DEFAULT_OPTION_CONFIG_NAME).string())
 , _xsd_subdir(DEFAULT_OPTION_XSD_SUBDIR)
 , _disabled_services()
 , _nsxsd()
 , _domain(DEFAULT_OPTION_DOMAIN_NAME)
 , _xcap_roots()
 , _tcp_port(0)
 , _tcp_ssl_pem()
#if defined(WITH_BACKEND_POSTGRESQL)
 , _connect_options(DEFAULT_OPTION_DB_CONNECT_STRING)
#endif
 , _xtable(DEFAULT_OPTION_DB_XML_TABLE)
 , _utable(DEFAULT_OPTION_DB_USER_TABLE)
 , _storage(DEFAULT_OPTION_STORAGE)
{ 
}

bool Options::reset(int argc, char** argv)
{
    //std::map<std::string, std::string> const _nsxsd;
    _nsxsd["http://www.w3.org/XML/1998/namespace"] = "xml.xsd";
    _nsxsd["urn:ietf:params:xml:ns:xcap-error"] = "xcap-error.xsd";
    _nsxsd["urn:ietf:params:xml:ns:xcap-caps"] = "xcap-caps.xsd";
    _nsxsd["urn:oma:xml:xdm:xcap-directory"] = "xcap-directory.xsd";
    _nsxsd["urn:ietf:params:xml:ns:resource-lists"] = "resource-lists.xsd";
    _nsxsd["urn:ietf:params:xml:ns:rls-services"] = "rls-services.xsd";
    _nsxsd["urn:ietf:params:xml:ns:common-policy"] = "common-policy.xsd";
    _nsxsd["urn:ietf:params:xml:ns:pres-rules"] = "presence-rules.xsd";
    _nsxsd["urn:ietf:params:xml:ns:pidf"] = "pidf.xsd";
    _nsxsd["urn:ietf:params:xml:ns:pidf:data-model"] = "pidf-data-model.xsd";
    _nsxsd["http://openxcap.org/ns/watchers"] = "openxcap-watchers.xsd ";
    _nsxsd["http://example.com"] = "test_schema.xsd";

    _xcap_roots.push_back(DEFAULT_OPTION_ROOT_URI);
	_xcap_roots.push_back(DEFAULT_OPTION_ROOT_URI_2);

    options_description generic("Arguments");
    options_description config("Configuration");
    options_description config_only("ConfigurationOnly");

    generic.add_options()
        ("version,v", "print version string")
        ("help", "produce help message")
        ("config,f", value(&_config_file), "name of a file of a configuration.")
        ;

	//IMPORTANT: after comma there must be only one letter otherwise fails assertion in newer boost versions.
    config.add_options()
        ("threads-count,c", value(&_nthreads), "default threads count")
        ("locale,l", value(&_locale), "Localization of XML documents for XML backend")
        ("base-dir,b", value(&_base_path), "server's root path (top directory)")
        ("xmlparser.xsd-subdir,x", value(&_xsd_subdir), "subdirectory under root where XML Schemas are stored (*.xsd)")
        ("domain,d", value(&_domain), "name of default domain which Scarlet serves")
        ("port,p", value(&_tcp_port), "TCP port for Scarlet server")
        ("ssl-pem-file", value(&_tcp_ssl_pem), "use secure TCP connections with this certificate in Scarlet server")
#if defined(WITH_BACKEND_POSTGRESQL)
		("database.connect-options,o", value(&_connect_options), "database connection string, database name, username, password etc.")
#endif
        ("storage,s", value(&_storage), "storage backend for Scarlet services")
        ("disable-service,n", value(&_disabled_services)->composing(), "don't start this services")
        ("xcap-root,r", value(&_xcap_roots)->composing(), "XCAP root URIs (server resources)")
        ;

    config_only.add_options()
        ("database.table-xml", value(&_xtable), "name of table in database which Scarlet serves")
        ("database.table-users", value(&_utable), "name of table in database whith users")
        ("xmlparser.schema.http://www.w3.org/XML/1998/namespace"
            , value(&_nsxsd["http://www.w3.org/XML/1998/namespace"]))
        ("xmlparser.schema.urn:ietf:params:xml:ns:xcap-error"
            , value(&_nsxsd["urn:ietf:params:xml:ns:xcap-error"]))
        ("xmlparser.schema.urn:ietf:params:xml:ns:xcap-caps"
            , value(&_nsxsd["urn:ietf:params:xml:ns:xcap-caps"]))
        ("xmlparser.schema.urn:oma:xml:xdm:xcap-directory"
            , value(&_nsxsd["urn:oma:xml:xdm:xcap-directory"]))
        ("xmlparser.schema.urn:ietf:params:xml:ns:resource-lists"
            , value(&_nsxsd["urn:ietf:params:xml:ns:resource-lists"]))
        ("xmlparser.schema.urn:ietf:params:xml:ns:rls-services"
            , value(&_nsxsd["urn:ietf:params:xml:ns:rls-services"]))
        ("xmlparser.schema.urn:ietf:params:xml:ns:common-policy"
            , value(&_nsxsd["urn:ietf:params:xml:ns:common-policy"]))
        ("xmlparser.schema.urn:ietf:params:xml:ns:pres-rules"
            , value(&_nsxsd["urn:ietf:params:xml:ns:pres-rules"]))
        ("xmlparser.schema.urn:ietf:params:xml:ns:pidf"
            , value(&_nsxsd["urn:ietf:params:xml:ns:pidf"]))
        ("xmlparser.schema.urn:ietf:params:xml:ns:pidf:data-model"
            , value(&_nsxsd["urn:ietf:params:xml:ns:pidf:data-model"]))
        ("xmlparser.schema.http://openxcap.org/ns/watchers"
            , value(&_nsxsd["http://openxcap.org/ns/watchers"]))
        ;

    options_description cmdline_options;
    options_description config_file_options;

    cmdline_options.add(generic).add(config);
    config_file_options.add(config).add(config_only);

    variables_map vm;

    try {
        std::wclog << "reading command line options ... ";
        store(command_line_parser(argc, argv).options(cmdline_options).run(), vm);
        notify(vm);
        std::wclog << "OK\n";

    } catch(std::exception& e) {
        std::wcerr << "FAIL\n" << e.what() << "\n";
    }

    std::ifstream ifs(_config_file.c_str());
    if (!ifs) {
        std::wcerr << "can not open config file: " << _config_file.c_str() << "\n";
    } else {
        std::wclog << "reading config file: " << _config_file.c_str() << "\n";
        try {
            store(parse_config_file(ifs, config_file_options), vm);
            notify(vm);
        } catch(std::exception& e) {
            std::cout << e.what() << "\n";
        }
    }
    if (vm.count("help")) {
        std::cout << cmdline_options << "\n";
        return false;
    }

    if (vm.count("version")) {
        std::cout << "Multiple sources example, version 1.0\n";
        return false;
    }

    return true;
}


}
