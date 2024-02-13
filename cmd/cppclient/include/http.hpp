#pragma once
// Adapted from the Boost.Beast SSL client example:
// https://www.boost.org/doc/libs/1_74_0/libs/beast/example/http/client/async-ssl/http_client_async_ssl.cpp

#include <memory>
#include <string>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/ssl.hpp>

namespace http {
namespace detail {

class SessionBase : public std::enable_shared_from_this<SessionBase> {
protected:
    explicit SessionBase(boost::asio::io_context& io_context);
    virtual ~SessionBase();

    void initiate_request(boost::beast::http::verb method,
                          std::string const& host,
                          std::string const& target);

    virtual void on_error() = 0;
    virtual void on_result(std::string const& result) = 0;

private:
    void on_resolve(boost::beast::error_code ec,
                    boost::asio::ip::tcp::resolver::results_type results);
    void on_connect(boost::beast::error_code ec,
                    boost::asio::ip::tcp::resolver::results_type::endpoint_type);
    void on_handshake(boost::beast::error_code ec);
    void on_write(boost::beast::error_code ec, std::size_t bytes_transferred);
    void on_read(boost::beast::error_code ec, std::size_t bytes_transferred);
    void on_shutdown(boost::beast::error_code ec);

    boost::asio::ip::tcp::resolver m_resolver;
    boost::beast::ssl_stream<boost::beast::tcp_stream> m_stream;
    boost::beast::flat_buffer m_buffer;
    boost::beast::http::request<boost::beast::http::empty_body> m_request;
    boost::beast::http::response<boost::beast::http::string_body> m_response;
};

}  // namespace detail

template <typename Callback>
class Session : public detail::SessionBase {
public:
    Session(boost::asio::io_context& io_context, Callback&& callback)
        : detail::SessionBase{io_context}, m_callback{std::forward<Callback>(callback)} {}
    ~Session() override = default;

    void get(std::string const& host, std::string const& target) {
        initiate_request(boost::beast::http::verb::get, host, target);
    }

protected:
    void on_error() override {
        // TODO
    }

    void on_result(std::string const& result) override { m_callback(result); }

private:
    Callback m_callback;
};

template <typename F>
void get(boost::asio::io_context& io_context,
         std::string const& host,
         std::string const& target,
         F&& response_callback) {
    auto client = std::make_shared<Session<F>>(io_context, std::forward<F>(response_callback));
    client->get(host, target);
}

}  // namespace http
