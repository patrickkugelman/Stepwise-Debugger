#pragma once

#include <iosfwd>
#include <vector>

#include <stepwise/ui/renderer.hpp>

namespace stepwise::ui {

class TextRenderer final : public Renderer {
public:
    TextRenderer(core::Session& session, std::ostream& out, std::ostream& err)
        : session_(session), out_(out), err_(err) {}

    void on_event(const core::Event& event) override;
    void on_output(const commands::CommandOutput& output) override;
    void on_error(const Error& error) override;

private:
    void print_frames(const std::vector<core::Frame>& frames);
    void print_crash_report(const core::CrashReportEvent& crash);

    core::Session& session_;
    std::ostream& out_;
    std::ostream& err_;
};

}  // namespace stepwise::ui
