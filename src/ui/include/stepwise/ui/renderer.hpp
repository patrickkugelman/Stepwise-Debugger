#pragma once

#include <iosfwd>
#include <memory>
#include <optional>
#include <string_view>

#include <stepwise/base/error.hpp>
#include <stepwise/commands/command.hpp>
#include <stepwise/core/events.hpp>
#include <stepwise/core/session.hpp>

namespace stepwise::ui {

enum class OutputFormat { Text, Json };

[[nodiscard]] std::optional<OutputFormat> parse_output_format(std::string_view text) noexcept;

class Renderer {
public:
    virtual ~Renderer() = default;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    virtual void on_event(const core::Event& event) = 0;
    virtual void on_output(const commands::CommandOutput& output) = 0;
    virtual void on_error(const Error& error) = 0;

protected:
    Renderer() = default;
};

std::unique_ptr<Renderer> make_renderer(OutputFormat format, core::Session& session,
                                        std::ostream& out, std::ostream& err);

}  // namespace stepwise::ui
