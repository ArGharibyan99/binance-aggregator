#include <agg/net/BinanceWebSocketClient.hpp>

#include <boost/asio/connect.hpp>
#include <boost/beast/core.hpp>

#include <openssl/ssl.h>

#include <algorithm>
#include <cctype>
#include <cstddef>

namespace agg::net {
namespace {

namespace beast = boost::beast;
namespace websocket = boost::beast::websocket;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

} // namespace

std::string build_combined_stream_target(const std::vector<std::string>& symbols)
{
    std::string target = "/stream?streams=";

    for (std::size_t i = 0; i < symbols.size(); ++i) {
        if (i != 0) {
            target += "/";
        }

        std::string lowercase_symbol = symbols[i];
        std::transform(lowercase_symbol.begin(), lowercase_symbol.end(), lowercase_symbol.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

        target += lowercase_symbol + "@trade";
    }

    return target;
}

BinanceWebSocketClient::BinanceWebSocketClient(
    std::string host,
    std::string port,
    std::string target,
    agg::runtime::BoundedQueue<std::string>& raw_message_queue)
    : host_(std::move(host))
    , port_(std::move(port))
    , target_(std::move(target))
    , raw_message_queue_(raw_message_queue)
    , ssl_ctx_(ssl::context::tlsv12_client)
{
    ssl_ctx_.set_default_verify_paths();
    ssl_ctx_.set_verify_mode(ssl::verify_peer);
}

BinanceWebSocketClient::~BinanceWebSocketClient()
{
    stop();
}

void BinanceWebSocketClient::run(ConnectionErrorHandler error_handler)
{
    stop_requested_.store(false);

    boost::system::error_code ec;

    tcp::resolver resolver(ioc_);
    const auto results = resolver.resolve(host_, port_, ec);
    if (ec) {
        if (error_handler) {
            error_handler(ConnectionStage::Resolve, classify_error(ConnectionStage::Resolve, ec), ec);
        }
        return;
    }

    if (stop_requested_.load()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(stream_mutex_);
        ws_.emplace(ioc_, ssl_ctx_);
    }
    auto& ws = *ws_;

    // Required for SNI: Binance, like most TLS hosts, selects which
    // certificate to present based on the requested hostname.
    if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host_.c_str())) {
        ec.assign(static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category());
        if (error_handler) {
            error_handler(ConnectionStage::TlsHandshake, classify_error(ConnectionStage::TlsHandshake, ec), ec);
        }
        return;
    }

    boost::asio::connect(beast::get_lowest_layer(ws), results, ec);
    if (ec) {
        if (error_handler) {
            error_handler(ConnectionStage::TcpConnect, classify_error(ConnectionStage::TcpConnect, ec), ec);
        }
        return;
    }

    ws.next_layer().handshake(ssl::stream_base::client, ec);
    if (ec) {
        if (error_handler) {
            error_handler(ConnectionStage::TlsHandshake, classify_error(ConnectionStage::TlsHandshake, ec), ec);
        }
        return;
    }

    ws.handshake(host_, target_, ec);
    if (ec) {
        if (error_handler) {
            error_handler(
                ConnectionStage::WebSocketHandshake, classify_error(ConnectionStage::WebSocketHandshake, ec), ec);
        }
        return;
    }

    beast::flat_buffer buffer;

    for (;;) {
        buffer.clear();
        ws.read(buffer, ec);

        if (ec) {
            if (error_handler) {
                error_handler(ConnectionStage::Read, classify_error(ConnectionStage::Read, ec), ec);
            }
            return;
        }

        raw_message_queue_.push(beast::buffers_to_string(buffer.data()));
    }
}

void BinanceWebSocketClient::stop()
{
    stop_requested_.store(true);

    std::lock_guard<std::mutex> lock(stream_mutex_);
    if (!ws_) {
        return;
    }

    boost::system::error_code ec;
    beast::get_lowest_layer(*ws_).cancel(ec);
    beast::get_lowest_layer(*ws_).close(ec);
}

} // namespace agg::net
