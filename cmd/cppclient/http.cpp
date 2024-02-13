#include "http.hpp"

#include <chrono>
#include <iostream>
#include <sstream>

#include <boost/asio/io_context.hpp>
#include <boost/beast/http.hpp>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace ssl = asio::ssl;

namespace http {
namespace detail {
namespace {

std::once_flag ssl_init;

void load_certificates(ssl::context& ssl_context) {
    ssl_context.set_verify_mode(ssl::verify_none);
}

ssl::context& get_ssl_context() {
    static ssl::context ssl_context{ssl::context::tlsv12_client};
    std::call_once(ssl_init, load_certificates, ssl_context);
    return ssl_context;
}

}  // namespace

SessionBase::SessionBase(asio::io_context& io_context)
    : m_resolver{io_context}, m_stream{io_context, get_ssl_context()} {}

SessionBase::~SessionBase() = default;

void SessionBase::initiate_request(beast::http::verb method,
                                   std::string const& host,
                                   std::string const& target) {
    m_request.version(11);  // HTTP 1.1
    m_request.method(method);
    m_request.target(target);
    m_request.set(beast::http::field::host, host);
    m_request.set(beast::http::field::user_agent, "async rust ffi demo");
    m_resolver.async_resolve(
        host, "443", beast::bind_front_handler(&SessionBase::on_resolve, shared_from_this()));
}

void SessionBase::on_resolve(beast::error_code ec, asio::ip::tcp::resolver::results_type results) {
    if (ec) {
        std::cerr << "failed to resolve: " << ec.message() << std::endl;
        return;
    }
    beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds{30});
    beast::get_lowest_layer(m_stream).async_connect(
        results, beast::bind_front_handler(&SessionBase::on_connect, shared_from_this()));
}

void SessionBase::on_connect(boost::beast::error_code ec,
                             boost::asio::ip::tcp::resolver::results_type::endpoint_type) {
    if (ec) {
        std::cerr << "failed to connect: " << ec.message() << std::endl;
        return;
    }
    m_stream.async_handshake(
        ssl::stream_base::client,
        beast::bind_front_handler(&SessionBase::on_handshake, shared_from_this()));
}

void SessionBase::on_handshake(boost::beast::error_code ec) {
    if (ec) {
        std::cerr << "handshake failed: " << ec.message() << std::endl;
        return;
    }
    beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds{30});
    beast::http::async_write(m_stream, m_request,
                             beast::bind_front_handler(&SessionBase::on_write, shared_from_this()));
}

void SessionBase::on_write(boost::beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        std::cerr << "write failed: " << ec.message() << std::endl;
        return;
    }
    beast::http::async_read(m_stream, m_buffer, m_response,
                            beast::bind_front_handler(&SessionBase::on_read, shared_from_this()));
}

void SessionBase::on_read(boost::beast::error_code ec, std::size_t) {
    if (ec) {
        std::cerr << "read failed: " << ec.message() << std::endl;
        return;
    }
    std::stringstream string_stream{};
    string_stream << m_response.body();
    on_result(string_stream.str());
    beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds{30});
    m_stream.async_shutdown(
        beast::bind_front_handler(&SessionBase::on_shutdown, shared_from_this()));
}

void SessionBase::on_shutdown(boost::beast::error_code ec) {
    if (ec == boost::asio::error::eof) {
        ec = {};
    }
    if (ec) {
        std::cerr << "shutdown failed: " << ec.message() << std::endl;
        return;
    }
}

}  // namespace detail
}  // namespace http
