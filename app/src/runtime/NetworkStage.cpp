#include <agg/runtime/NetworkStage.hpp>

namespace agg::runtime {

NetworkStage::NetworkStage(
    std::string host,
    std::string port,
    std::string target,
    agg::config::ReconnectConfig reconnect_config,
    agg::runtime::BoundedQueue<std::string>& raw_message_queue,
    agg::net::ConnectionErrorHandler error_handler)
    : host_(std::move(host))
    , port_(std::move(port))
    , target_(std::move(target))
    , raw_message_queue_(raw_message_queue)
    , error_handler_(std::move(error_handler))
    , reconnect_policy_(std::move(reconnect_config))
{
}

NetworkStage::~NetworkStage()
{
    stop();
}

void NetworkStage::start()
{
    thread_ = std::thread(&NetworkStage::run, this);
}

void NetworkStage::stop()
{
    stop_requested_.store(true);
    reconnect_policy_.stop();

    {
        std::lock_guard<std::mutex> lock(client_mutex_);
        if (client_) {
            client_->stop();
        }
    }

    if (thread_.joinable()) {
        thread_.join();
    }
}

void NetworkStage::run()
{
    while (!stop_requested_.load()) {
        {
            std::lock_guard<std::mutex> lock(client_mutex_);
            client_ = std::make_unique<agg::net::BinanceWebSocketClient>(host_, port_, target_, raw_message_queue_);
        }

        bool had_successful_read = false;

        client_->run([this, &had_successful_read](
                         agg::net::ConnectionStage stage,
                         agg::net::ConnectionErrorKind kind,
                         boost::system::error_code ec) {
            if (stage == agg::net::ConnectionStage::Read) {
                had_successful_read = true;
            }
            if (error_handler_) {
                error_handler_(stage, kind, ec);
            }
        });

        {
            std::lock_guard<std::mutex> lock(client_mutex_);
            client_.reset();
        }

        if (stop_requested_.load()) {
            break;
        }

        if (had_successful_read) {
            reconnect_policy_.reset();
        }

        if (!reconnect_policy_.wait_for_next_attempt()) {
            break;
        }
    }
}

} // namespace agg::runtime
