#include <agg/runtime/CliOptions.hpp>

#include <stdexcept>
#include <string_view>

namespace agg::runtime {
namespace {

constexpr std::string_view kConfigFlag = "--config";
constexpr std::string_view kConfigFlagWithEquals = "--config=";

} // namespace

CliOptions parse_cli_options(int argc, char** argv)
{
    CliOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];

        if (arg == kConfigFlag) {
            if (i + 1 >= argc) {
                throw std::invalid_argument("--config requires a path argument");
            }
            options.config_path = argv[++i];
        } else if (arg.rfind(kConfigFlagWithEquals, 0) == 0) {
            options.config_path = std::string(arg.substr(kConfigFlagWithEquals.size()));
        }
    }

    return options;
}

} // namespace agg::runtime
