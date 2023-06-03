// WebSocketClient.h

// TODO: Remove redundant code and use algorithm/client.cpp
// TODO: Improve thread safety

#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H

#include <iostream>
#include <cstdlib>
#include <memory>
#include <string>
#include <functional>
#include <QObject>
#include <QString>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/strand.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>


// Report a failure
inline void fail(beast::error_code ec, char const* what) {
    std::cerr << what << ": " << ec.message() << "\n";
}


class WebSocketClient : public QObject, public std::enable_shared_from_this<WebSocketClient>
{
    Q_OBJECT

signals:
    void newDataReceived(const QString& data);

private:
    tcp::resolver resolver_;
    websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws_;
    beast::flat_buffer buffer_;
    std::string host_;
    std::string text_;

public:
volatile bool stopped = false;
    ~WebSocketClient() {
        std::cout << "WebSocketClient::~WebSocketClient()" << std::endl;
        std::cout << "TODO: close websocket connection" << std::endl;
    }

    // Resolver and socket require an io_context
    WebSocketClient(net::io_context& ioc, ssl::context& ctx)
        : resolver_(net::make_strand(ioc))
        , ws_(net::make_strand(ioc), ctx)
    {
        std::cout << "WebSocketClient::WebSocketClient()" << std::endl;
    }

    void close() {
        std::cout << "WebSocketClient::close()" << std::endl;
        // Close the WebSocket connection
        ws_.async_close(websocket::close_code::normal,
        beast::bind_front_handler(&WebSocketClient::on_close,shared_from_this()));
    }

    // Start the asynchronous operation
    void run(char const* host, char const* port, char const* text) {
        // Save these for later
        host_ = host;
        text_ = text;
        std::cout << "WebSocketClient::run()" << std::endl;

        // Look up the domain name
        resolver_.async_resolve(host,port,
            beast::bind_front_handler(&WebSocketClient::on_resolve,shared_from_this()));
    }

    void on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
        if(ec) return fail(ec, "resolve");

        // Set a timeout on the operation
        beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(ws_).async_connect( results,
            beast::bind_front_handler(&WebSocketClient::on_connect,shared_from_this()));
    }

    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep) {
        if(ec) return fail(ec, "connect");

        // Set a timeout on the operation
        beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

        // Set SNI Hostname (many hosts need this to handshake successfully)
        if(! SSL_set_tlsext_host_name(ws_.next_layer().native_handle(),
                host_.c_str()))
        {
            ec = beast::error_code(static_cast<int>(::ERR_get_error()),
                net::error::get_ssl_category());
            return fail(ec, "connect");
        }

        // Update the host_ string. This will provide the value of the
        // Host HTTP header during the WebSocket handshake.
        // See https://tools.ietf.org/html/rfc7230#section-5.4
        host_ += ':' + std::to_string(ep.port());
        
        // Perform the SSL handshake
        ws_.next_layer().async_handshake(ssl::stream_base::client,
            beast::bind_front_handler(&WebSocketClient::on_ssl_handshake,shared_from_this()));
    }

    void on_ssl_handshake(beast::error_code ec) {
        if(ec) return fail(ec, "ssl_handshake");

        // Turn off the timeout on the tcp_stream, because
        // the websocket stream has its own timeout system.
        beast::get_lowest_layer(ws_).expires_never();

        // Set suggested timeout settings for the websocket
        ws_.set_option(
            websocket::stream_base::timeout::suggested(beast::role_type::client));

        // Set a decorator to change the User-Agent of the handshake
        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req)
            {
                req.set(http::field::user_agent,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " websocket-client-async-ssl");
            }));

        // Perform the websocket handshake
        ws_.async_handshake(host_, "/",
            beast::bind_front_handler(&WebSocketClient::on_handshake,shared_from_this()));
    }

    void on_handshake(beast::error_code ec) {
        if(ec) return fail(ec, "handshake");

        // Send the message
        ws_.async_write( net::buffer(text_), 
                beast::bind_front_handler(&WebSocketClient::on_write,shared_from_this()));
    }

    void on_write(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        if(ec) return fail(ec, "write");

        // Read a message into our buffer
        ws_.async_read(buffer_,
            beast::bind_front_handler(&WebSocketClient::on_read,shared_from_this()));
    }

    void on_read(beast::error_code ec,std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        if(ec) return fail(ec, "read");

        std::cout << beast::make_printable(buffer_.data()) << std::endl;
        emit newDataReceived(QString::fromStdString(beast::buffers_to_string(buffer_.data())));
        buffer_.consume(buffer_.size());

        // Read a single message into our buffer
        ws_.async_read(buffer_,
            beast::bind_front_handler(&WebSocketClient::on_read,shared_from_this()));
    }

    void on_close(beast::error_code ec) {
        if(ec) return fail(ec, "close");

        // If we get here then the connection is closed gracefully
        std::cout << "WebSocketClient::on_close()" << std::endl;

        
        stopped = true;
        // The make_printable() function helps print a ConstBufferSequence
        // std::cout << beast::make_printable(buffer_.data()) << std::endl;
    }
};


#endif // WEBSOCKETCLIENT_H