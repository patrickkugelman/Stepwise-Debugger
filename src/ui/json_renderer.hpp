#pragma once

#include <iosfwd>

#include <stepwise/ui/renderer.hpp>

namespace stepwise::ui {

class JsonRenderer final : public Renderer {
public:
    JsonRenderer(core::Session& session, std::ostream& out) : session_(session), out_(out) {}

    void on_event(const core::Event& event) override;
    void on_output(const commands::CommandOutput& output) override;
    void on_error(const Error& error) override;

private:
    core::Session& session_;
    std::ostream& out_;
};

}  // namespace stepwise::ui
