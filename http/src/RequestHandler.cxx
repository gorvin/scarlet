#include "scarlet/http/RequestHandler.h"
#include "scarlet/http/SvcHandler.h"
#include "scarlet/http/HTTPDefs.h"
#include <scarlet/http/MsgReader.h>
#include <scarlet/http/MsgParser.h>
#include <scarlet/http/MsgWriter.h>
#include <boost/make_shared.hpp>
#include <boost/bind.hpp>
#include <boost/uuid/uuid.hpp> //boost version >= 1.42
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace scarlet
{
void split_at_semicolon(std::vector<std::string>& out, std::string const& fields)
{
	out.clear();
	typedef boost::token_iterator<
		boost::char_separator<char>, std::string::const_iterator, std::string
	> string_token_iterator;
	boost::char_separator<char> const separator(";");
	string_token_iterator it(separator, fields.begin(), fields.end());
	string_token_iterator const itend(separator, fields.end(), fields.end());
	for (; it != itend; ++it)
		out.push_back(*it);
}

namespace http
{

/** creates a new HTTPServer object
* @param scheduler the WorkScheduler that will be used to manage worker threads
* @param endpoint TCP endpoint used to listen for new connections (see ASIO docs)
*/
RequestHandler::RequestHandler(SvcHandlerPtr svc_handler)
	: svc_handler(svc_handler)
	, auth(boost::make_shared<ResourceAuth>())
	, m_max_content_length(HTTPDefs::DEFAULT_MAX_BODY_SIZE)
{
}

RequestHandlerPtr RequestHandler::create(SvcHandlerPtr svc_handler)
{
	return RequestHandlerPtr(new RequestHandler(svc_handler));
}

std::string status_message(scarlet::http::response_code_e code)
{
	switch (code) {
	case HTTP_OK: // = 200,
		return HTTPDefs::RESPONSE_MESSAGE_OK;
	case HTTP_OK_CREATED: // = 201,
		return HTTPDefs::RESPONSE_MESSAGE_CREATED;
	case HTTP_FAIL_BAD_REQUEST: // = 400,//Bad Request
		return HTTPDefs::RESPONSE_MESSAGE_BAD_REQUEST;
	case HTTP_FAIL_AUTHORIZATION: // = 401,//Unauthorized
		return HTTPDefs::RESPONSE_MESSAGE_UNAUTHORIZED;
	case HTTP_FAIL_NOT_FOUND: // = 404,//Not Found
		return HTTPDefs::RESPONSE_MESSAGE_NOT_FOUND;
	case HTTP_FAIL_NOT_ALLOWED: // = 405,//Method Not Allowed
		return HTTPDefs::RESPONSE_MESSAGE_METHOD_NOT_ALLOWED;
	case HTTP_FAIL_CONSTRAINTS: // = 409, //Conflict
		return "Conflict - Failed XCAP constraints";
	case HTTP_FAIL_IF_PERFORM: // = 412,//Precondition Failed
		return "Precondition Failed - Failed If-Match, If-None-Match or their combination";
	case HTTP_FAIL_MIME: // = 415,//Unsupported Media Type
		return "Unsupported Media Type by XCAP";
	case HTTP_ERROR_INTERNAL: // = 500, //Internal Server Error
		;
	}
	return HTTPDefs::RESPONSE_MESSAGE_SERVER_ERROR;
}

//request->get_major(), request->get_minor()
void RequestHandler::sendResponse(unsigned short ver_major, unsigned short ver_minor, net::TCPConnectionPtr tcp_conn, FinishedConnectionFn finishHandler
	, boost::shared_ptr<httpresponse_t> response, std::string const& challenge_domain)
{
	DBGMSGAT("Sending response ... ");
	std::wclog << "Response body = " << bmu::utf8_string(response->body).c_str() << std::endl;
	try {
		MsgSerializerPtr http_response(new MsgSerializer);
		http_response->set_first_line(scarlet::http::StatusLine(
			ver_major
			, ver_minor
			, response->code
			, status_message(response->code)
			));
		if (response->code == scarlet::http::HTTP_FAIL_AUTHORIZATION) {
			std::string const nonce_str(boost::uuids::to_string(boost::uuids::random_generator()()));
			auth->add_sent_nonce(nonce_str);//used for future request authentication
			std::string digest_challenge("Digest qop=\"auth\", realm=\"");
			digest_challenge.append(challenge_domain)
				.append("\", nonce=\"")
				.append(nonce_str)
				.append("\"");
			//algorithm defaults to MD5
			http_response->set_header(scarlet::http::HTTPDefs::HEADER_WWW_AUTHENTICATE, digest_challenge);
		}
		if (!response->mime.empty()) http_response->set_header(scarlet::http::HTTPDefs::HEADER_CONTENT_TYPE, response->mime);
		if (!response->etag.empty()) http_response->set_header(scarlet::http::HTTPDefs::HEADER_ETAG, response->etag);
		for (std::map<std::string, std::string>::const_iterator it(response->extra_hdrs.begin());
		it != response->extra_hdrs.end(); ++it) {
			http_response->set_header(it->first, it->second);
		}
		if (!response->body.empty()) {
			if ((response->code&(-2)) != scarlet::http::HTTP_OK) {
				http_response->set_header(scarlet::http::HTTPDefs::HEADER_CONTENT_TYPE, "application/xcap-error+xml");
				static std::string const xerror_start(
					"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
					"<xcap-error xmlns=\"urn:ietf:params:xml:ns:xcap-error\">\n"
					);
				http_response->append_nocopy(xerror_start);
			}
			http_response->append_nocopy(reinterpret_cast<const char*>(response->body.data()), response->body.size());
			if ((response->code&(-2)) != scarlet::http::HTTP_OK) {
				static std::string const xerror_end("\n</xcap-error>\n");
				http_response->append_nocopy(xerror_end);
			}
		}
		scarlet::http::MsgWriterPtr writer(
			scarlet::http::MsgWriter::create(
				tcp_conn
				, http_response
				, finishHandler//boost::bind(&http::TCPConnection::finish, tcp_conn)
				)
			);
		if (!writer->send_async()) { // non-blocking call
			DBGMSGAT("Failed sending response: lost TCP connection");
		}
		else {
			DBGMSGAT("Response is sent");
		}
	}
	catch (std::bad_alloc& ex) {
		_CRT_UNUSED(ex);
		DBGMSGAT(ex.what());
		assert(0);//std::terminate(); //throw;// propagate memory errors (FATAL)
	}
}

void RequestHandler::exceptions_handler(boost::shared_ptr<scarlet::http::httpresponse_t> response)
{
	try {
		throw;
	}
	catch (...) {
		DBGMSGAT("EXCEPTION: Internal server error");
	}
	response->code = scarlet::http::HTTP_ERROR_INTERNAL;
	response->body.clear();
	response->etag.clear();
	response->mime.clear();
}

bool RequestHandler::formatRequest(MsgParserPtr http_request, boost::shared_ptr<httprequest_t> req, boost::shared_ptr<httpresponse_t> response)
{
	std::string const requested_method(http_request->get_requested_method());
	if (requested_method == scarlet::http::HTTPDefs::REQUEST_METHOD_GET) {
		req->method = scarlet::http::httprequest_t::RQ_METHOD_GET;
	}
	else if (requested_method == scarlet::http::HTTPDefs::REQUEST_METHOD_PUT) {
		req->method = scarlet::http::httprequest_t::RQ_METHOD_PUT;
	}
	else if (requested_method == scarlet::http::HTTPDefs::REQUEST_METHOD_DELETE) {
		req->method = scarlet::http::httprequest_t::RQ_METHOD_DELETE;
	}
	else {
		DBGMSGAT("Requested method not allowed it's not one of GET, PUT or DELETE");
		response->code = scarlet::http::HTTP_FAIL_NOT_ALLOWED;
		return false;
	}
	{
		bmu::u8vector_t domain;
		if (!svc_handler->parseSvcURI(http_request, req, domain)) {
			response->code = scarlet::http::HTTP_FAIL_BAD_REQUEST;
			return false;
		}
		req->domain.assign(domain.begin(), domain.end());
	}
	DBGMSGAT("Authenticating request");
	// try to verify authentication
	scarlet::http::ResourceAuth::auth_status_t astat(auth->handleAuthentication(
		boost::bind(&SvcHandler::resolveUserImpl, svc_handler, _1, _2), http_request, req->domain)
		);
	if (astat == scarlet::http::ResourceAuth::AUTH_FAIL) {
		response->code = scarlet::http::HTTP_FAIL_AUTHORIZATION;
		DBGMSGAT("Failed authentication for resource: " << http_request->get_requested_resource());
		return false;
	}
	else if (astat == scarlet::http::ResourceAuth::AUTH_BAD) {
		response->code = scarlet::http::HTTP_FAIL_BAD_REQUEST;
		DBGMSGAT("Bad authentication for resource: " << http_request->get_requested_resource());
		return false;
	}
	DBGMSGAT("Request authenticated");
	req->username = http_request->get_username();//NOTE: ovo moze samo nakon authentication
	std::vector<char> const& vBody = http_request->get_body();
	req->body.length = vBody.size();
	req->body.content = (req->body.length > 0) ? reinterpret_cast<bmu::u8unit_t const*>(&vBody[0]) : 0;
	req->mime = http_request->get_header(scarlet::http::HTTPDefs::HEADER_CONTENT_TYPE);
	split_at_semicolon(req->ifetags, http_request->get_header(scarlet::http::HTTPDefs::HEADER_IF_MATCH));
	split_at_semicolon(req->ifnoetags, http_request->get_header(scarlet::http::HTTPDefs::HEADER_IF_NONE_MATCH));
	return true;
}

void RequestHandler::handleRequest(net::TCPConnectionPtr tcp_conn, FinishedConnectionFn finishHandler, MsgParserPtr http_request)
{
	boost::shared_ptr<httpresponse_t> response(boost::make_shared<httpresponse_t>());
	boost::shared_ptr<httprequest_t> fmt_request(svc_handler->createFormattedRequestObject());

	DBGMSGAT("Handling received HTTP request");
	if (!http_request->is_finished()) {//if(!request->isValid()) {
									   // the request is invalid or an error occured
		DBGMSGAT("Received incomplete HTTP request");
		response->code = scarlet::http::HTTP_FAIL_BAD_REQUEST;
	}
	else {
		DBGMSGAT("Received a valid HTTP request");
#ifndef NDEBUG
		http_request->dump_header();
#endif
		try {
			if (!formatRequest(http_request, fmt_request, response)) {
				DBGMSGAT("Received an invalid HTTP request");
				//NOTE: u fill je vec postavljen response->code
			}
		}
		catch (...) {
			exceptions_handler(response);
			//if not fatal then it's HTTP server internal error (500)
		}
	}
	// try to handle the request
	svc_handler->handleResource(fmt_request, response); // obrada XCAP zahtjeva i formiranje responsa (sa odgovorom ili sa greskom)
	sendResponse(http_request->get_major(), http_request->get_minor(), tcp_conn, finishHandler, response, fmt_request->domain); // rtask->finish(); // slanje HTTP responsa
}

}
}
