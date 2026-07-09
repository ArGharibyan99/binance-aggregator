#include <agg/net/ConnectionError.hpp>

#include <gtest/gtest.h>

#include <boost/asio/error.hpp>
#include <boost/beast/websocket/error.hpp>

namespace {
using agg::net::classify_error;
using agg::net::ConnectionErrorKind;
using agg::net::ConnectionStage;
}

TEST(ConnectionErrorTest, ClassifiesResolveErrorsAsDnsFailure)
{
    const auto ec = boost::asio::error::make_error_code(boost::asio::error::host_not_found);
    EXPECT_EQ(classify_error(ConnectionStage::Resolve, ec), ConnectionErrorKind::DnsFailure);
}

TEST(ConnectionErrorTest, ClassifiesTcpConnectErrorsAsTcpConnectFailure)
{
    const auto ec = boost::asio::error::make_error_code(boost::asio::error::connection_refused);
    EXPECT_EQ(classify_error(ConnectionStage::TcpConnect, ec), ConnectionErrorKind::TcpConnectFailure);
}

TEST(ConnectionErrorTest, ClassifiesTlsHandshakeErrorsAsTlsHandshakeFailure)
{
    const auto ec = boost::asio::error::make_error_code(boost::asio::error::connection_reset);
    EXPECT_EQ(classify_error(ConnectionStage::TlsHandshake, ec), ConnectionErrorKind::TlsHandshakeFailure);
}

TEST(ConnectionErrorTest, ClassifiesWebSocketHandshakeErrorsAsWebSocketHandshakeFailure)
{
    const auto ec = boost::asio::error::make_error_code(boost::asio::error::connection_reset);
    EXPECT_EQ(
        classify_error(ConnectionStage::WebSocketHandshake, ec), ConnectionErrorKind::WebSocketHandshakeFailure);
}

TEST(ConnectionErrorTest, ClassifiesEofDuringReadAsServerDisconnect)
{
    const auto ec = boost::asio::error::make_error_code(boost::asio::error::eof);
    EXPECT_EQ(classify_error(ConnectionStage::Read, ec), ConnectionErrorKind::ServerDisconnect);
}

TEST(ConnectionErrorTest, ClassifiesWebSocketCloseDuringReadAsServerDisconnect)
{
    const boost::system::error_code ec = boost::beast::websocket::error::closed;
    EXPECT_EQ(classify_error(ConnectionStage::Read, ec), ConnectionErrorKind::ServerDisconnect);
}

TEST(ConnectionErrorTest, ClassifiesOtherReadErrorsAsReadFailure)
{
    const auto ec = boost::asio::error::make_error_code(boost::asio::error::connection_reset);
    EXPECT_EQ(classify_error(ConnectionStage::Read, ec), ConnectionErrorKind::ReadFailure);
}

TEST(ConnectionErrorTest, ClassifiesTimedOutAsTimeoutRegardlessOfStage)
{
    const auto ec = boost::asio::error::make_error_code(boost::asio::error::timed_out);
    EXPECT_EQ(classify_error(ConnectionStage::TcpConnect, ec), ConnectionErrorKind::Timeout);
    EXPECT_EQ(classify_error(ConnectionStage::Read, ec), ConnectionErrorKind::Timeout);
}

TEST(ConnectionErrorTest, ClassifiesNoErrorAsUnknown)
{
    const boost::system::error_code ec;
    EXPECT_EQ(classify_error(ConnectionStage::Read, ec), ConnectionErrorKind::Unknown);
}
