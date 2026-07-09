#include <agg/output/FileOutputSink.hpp>

#include <stdexcept>

namespace agg::output {

FileOutputSink::FileOutputSink(const std::filesystem::path& path)
    : stream_(path, std::ios::out | std::ios::app)
{
    if (!stream_.is_open()) {
        throw std::runtime_error("Failed to open output file: " + path.string());
    }
}

void FileOutputSink::write(const std::string& text)
{
    stream_ << text;
    stream_.flush();

    if (!stream_) {
        throw std::runtime_error("Failed to write to output file");
    }
}

} // namespace agg::output
