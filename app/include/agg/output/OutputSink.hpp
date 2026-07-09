#pragma once

#include <string>

namespace agg::output {

/// Abstract destination for serialized statistics text blocks.
class OutputSink {
public:
    virtual ~OutputSink() = default;

    /// Writes one serialized text block (as produced by
    /// StatsSerializer::serialize) to the sink, verbatim.
    virtual void write(const std::string& text) = 0;
};

} // namespace agg::output
