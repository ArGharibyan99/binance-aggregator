#include <spdlog/spdlog.h>

int main()
{
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

    spdlog::info("Binance Aggregator service starting");

    return 0;
}