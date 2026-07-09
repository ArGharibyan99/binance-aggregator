#pragma once

#include <agg/output/OutputSink.hpp>

#include <filesystem>
#include <fstream>

namespace agg::output {

/// Appends serialized statistics text to a file on disk.
class FileOutputSink : public OutputSink {
public:
    /// Opens (creating if necessary) the file at `path` for appending.
    /// Throws std::runtime_error if the file cannot be opened.
    explicit FileOutputSink(const std::filesystem::path& path);

    void write(const std::string& text) override;

private:
    std::ofstream stream_;
};

} // namespace agg::output
