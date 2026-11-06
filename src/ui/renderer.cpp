#include <stepwise/ui/renderer.hpp>

#include "json_renderer.hpp"
#include "text_renderer.hpp"

namespace stepwise::ui {

std::optional<OutputFormat> parse_output_format(std::string_view text) noexcept {
    if (text == "text") {
        return OutputFormat::Text;
    }
    if (text == "json") {
        return OutputFormat::Json;
    }
    return std::nullopt;
}

std::unique_ptr<Renderer> make_renderer(OutputFormat format, core::Session& session, std::ostream& out,
                                        std::ostream& err) {
    switch (format) {
        case OutputFormat::Json:
            return std::make_unique<JsonRenderer>(session, out);
        case OutputFormat::Text:
            break;
    }
    return std::make_unique<TextRenderer>(session, out, err);
}

}  // namespace stepwise::ui
