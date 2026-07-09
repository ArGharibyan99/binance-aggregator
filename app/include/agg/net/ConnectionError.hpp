#pragma once

#include <boost/system/error_code.hpp>

namespace agg::net {

/// Which stage of establishing/using the connection an error occurred in.
enum class ConnectionStage {
    Resolve,
    TcpConnect,
    TlsHandshake,
    WebSocketHandshake,
    Read,
};

/// Category of connection failure reported to callers, independent of
/// the underlying Boost.Asio/Beast error_code.
enum class ConnectionErrorKind {
    DnsFailure,
    TcpConnectFailure,
    TlsHandshakeFailure,
    WebSocketHandshakeFailure,
    ReadFailure,
    Timeout,
    ServerDisconnect,
    Unknown,
};

/// Classifies an error_code encountered during `stage` into the category
/// reported to callers, so failure handling does not need to inspect raw
/// Boost.Asio/Beast error codes directly.
ConnectionErrorKind classify_error(ConnectionStage stage, const boost::system::error_code& ec);

} // namespace agg::net
