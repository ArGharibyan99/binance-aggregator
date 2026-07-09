#include <agg/net/ConnectionError.hpp>

#include <boost/asio/error.hpp>
#include <boost/beast/websocket/error.hpp>

namespace agg::net {

ConnectionErrorKind classify_error(ConnectionStage stage, const boost::system::error_code& ec)
{
    if (!ec) {
        return ConnectionErrorKind::Unknown;
    }

    if (ec == boost::asio::error::timed_out) {
        return ConnectionErrorKind::Timeout;
    }

    switch (stage) {
    case ConnectionStage::Resolve:
        return ConnectionErrorKind::DnsFailure;

    case ConnectionStage::TcpConnect:
        return ConnectionErrorKind::TcpConnectFailure;

    case ConnectionStage::TlsHandshake:
        return ConnectionErrorKind::TlsHandshakeFailure;

    case ConnectionStage::WebSocketHandshake:
        return ConnectionErrorKind::WebSocketHandshakeFailure;

    case ConnectionStage::Read:
        if (ec == boost::asio::error::eof || ec == boost::beast::websocket::error::closed) {
            return ConnectionErrorKind::ServerDisconnect;
        }
        return ConnectionErrorKind::ReadFailure;
    }

    return ConnectionErrorKind::Unknown;
}

} // namespace agg::net
