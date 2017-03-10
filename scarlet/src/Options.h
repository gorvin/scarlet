#ifndef OPTIONS_H
#define OPTIONS_H
#include <vector>
#include <map>

namespace scarlet {

class Options {
    size_t                              _nthreads;
    std::string                         _locale;//en_US
    std::string                         _base_path;
	std::string                         _start_path;
	std::string                         _config_file;
    std::string                         _xsd_subdir;
    std::vector<std::string>            _disabled_services;
    std::map<std::string, std::string>  _nsxsd;
    std::string                         _domain;
    std::vector<std::string>            _xcap_roots;
	unsigned short                      _tcp_port;
    std::string                         _tcp_ssl_pem;
#if defined(WITH_BACKEND_POSTGRESQL)
	std::string                         _connect_options;
#endif
    std::string                         _xtable;
    std::string                         _utable;
    std::string                         _storage;

    Options(void);

public:
    size_t concurrency(void) const { return _nthreads; }
    std::string const& locale(void) const { return _locale; }
	std::string const& topdir(void) const { return _base_path; }
	std::string const& startdir(void) const { return _start_path; } // executable location
	std::string const& subdir_xsd(void) const { return _xsd_subdir; }
    std::vector<std::string> const& disabled_services(void) const { return _disabled_services; }
    std::map<std::string, std::string> const& namespace_schema(void) const { return _nsxsd; }
    std::string const& domain(void) const { return _domain; }
    std::vector<std::string> const& xcap_roots(void) const { return _xcap_roots; }
    unsigned short port(void) const { return _tcp_port; }
    std::string const& ssl_pem(void) const { return _tcp_ssl_pem; }
#if defined(WITH_BACKEND_POSTGRESQL)
	std::string const& connect_options(void) const { return _connect_options; }
#endif
    std::string const& db_xtable(void) const { return _xtable; }
    std::string const& db_utable(void) const { return _utable; }
    std::string const& storage_backend(void) const { return _storage; }
    ///\return false ako se trazio help (ne treba nastavljati izvrsavanje programa)
    bool reset(int argc, char** argv);
    static Options& instance(void);
};

}

#endif // OPTIONS_H
