#include "text_renderer.hpp"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <ostream>
#include <string>
#include <variant>

#include <stepwise/backend/signal_name.hpp>
#include <stepwise/base/overloaded.hpp>

namespace stepwise::ui {
namespace {

std::string short_file(const std::string& path) {
    return std::filesystem::path{path}.filename().string();
}

std::string describe(const core::CodeLocation& where) {
    std::string text;
    if (where.function) {
        text += std::format(" in {}", where.function->name);
    }
    if (where.source) {
        text += std::format(" ({}:{})", short_file(where.source->file), where.source->line);
    }
    return text;
}

std::string condition_suffix(const std::optional<core::Condition>& condition) {
    return condition ? std::format(" if {}", condition->text) : std::string{};
}

std::string symbolic(const core::CodeLocation& where) {
    if (!where.function) {
        return {};
    }
    if (where.function->offset == 0) {
        return std::format(" <{}>", where.function->name);
    }
    return std::format(" <{}+{}>", where.function->name, where.function->offset);
}

}  // namespace

void TextRenderer::on_error(const Error& error) {
    err_ << "error: " << error.message() << std::endl;
}

void TextRenderer::on_event(const core::Event& event) {
    std::visit(
        Overloaded{
            [&](const core::StoppedEvent& stop) {
                out_ << std::format("process {} stopped at {:#018x}{} ({})\n", stop.pid.value(),
                                    stop.pc.value(), describe(stop.where), core::to_string(stop.reason));
                if (stop.where.source) {
                    if (auto text = session_.source_line(stop.where.source->file, stop.where.source->line)) {
                        out_ << std::format("{}\t{}\n", stop.where.source->line, *text);
                    }
                }
            },
            [&](const core::BreakpointHitEvent& hit) {
                out_ << std::format("breakpoint {} hit at {:#018x} (hit count {})\n", hit.id.value(),
                                    hit.pc.value(), hit.hit_count);
            },
            [&](const core::CrashReportEvent& crash) { print_crash_report(crash); },
            [&](const core::SignalReceivedEvent& received) {
                out_ << std::format("process {} received {}\n", received.pid.value(),
                                    backend::signal_name(received.signal));
            },
            [&](const core::ExitedEvent& exited) {
                std::visit(Overloaded{
                               [&](int code) {
                                   out_ << std::format("process {} exited with code {}\n",
                                                       exited.pid.value(), code);
                               },
                               [&](Signal signal) {
                                   out_ << std::format("process {} terminated by {}\n",
                                                       exited.pid.value(),
                                                       backend::signal_name(signal));
                               },
                           },
                           exited.status);
            },
        },
        event);
    out_ << std::flush;
}

void TextRenderer::print_frames(const std::vector<core::Frame>& frames) {
    for (std::size_t i = 0; i < frames.size(); ++i) {
        out_ << std::format("#{:<2} {:#018x}{}\n", i, frames[i].pc.value(), describe(frames[i].where));
    }
}

void TextRenderer::print_crash_report(const core::CrashReportEvent& crash) {
    out_ << std::format("---- crash report: {} ({}) ----\n", backend::signal_name(crash.signal),
                        backend::signal_description(crash.signal));
    if (crash.fault_address) {
        out_ << std::format("fault address: {:#x}\n", crash.fault_address->value());
    } else {
        out_ << "fault address: unknown\n";
    }
    out_ << std::format("location:      {:#018x}{}\n", crash.where.address.value(), describe(crash.where));
    if (crash.where.source) {
        if (auto text = session_.source_line(crash.where.source->file, crash.where.source->line)) {
            out_ << std::format("source:        {}\t{}\n", crash.where.source->line, *text);
        }
    }

    out_ << "registers:\n";
    constexpr std::size_t columns = 3;
    std::size_t column = 0;
    for (const auto& field : backend::register_fields) {
        out_ << std::format("  {:<9}{:#018x}", field.name, crash.registers.*field.member);
        if (++column % columns == 0) {
            out_ << '\n';
        }
    }
    if (column % columns != 0) {
        out_ << '\n';
    }

    out_ << "backtrace:\n";
    print_frames(crash.backtrace);
    out_ << "----\n";
}

void TextRenderer::on_output(const commands::CommandOutput& output) {
    std::visit(
        Overloaded{
            [](const commands::NoOutput&) {},
            [](const commands::QuitOutput&) {},
            [&](const commands::RegistersOutput& registers) {
                for (const auto& field : backend::register_fields) {
                    out_ << std::format("{:<9}{:#018x}\n", field.name,
                                        registers.registers.*field.member);
                }
            },
            [&](const commands::MemoryOutput& memory) {
                constexpr std::size_t row = 16;
                for (std::size_t offset = 0; offset < memory.bytes.size(); offset += row) {
                    std::string hex;
                    std::string text;
                    for (std::size_t i = 0; i < row; ++i) {
                        if (offset + i >= memory.bytes.size()) {
                            hex += "   ";
                            continue;
                        }
                        const auto byte = std::to_integer<unsigned>(memory.bytes[offset + i]);
                        hex += std::format("{:02x} ", byte);
                        text += std::isprint(static_cast<int>(byte)) != 0 ? static_cast<char>(byte)
                                                                           : '.';
                    }
                    out_ << std::format("{:#018x}  {} |{}|\n", memory.address.value() + offset, hex,
                                        text);
                }
            },
            [&](const commands::BreakpointSetOutput& set) {
                out_ << std::format("breakpoint {} at {:#018x}{}{}\n", set.breakpoint.id.value(),
                                    set.breakpoint.address.value(), describe(set.where),
                                    condition_suffix(set.breakpoint.condition));
            },
            [&](const commands::BacktraceOutput& trace) { print_frames(trace.frames); },
            [&](const commands::BreakpointListOutput& list) {
                if (list.rows.empty()) {
                    out_ << "no breakpoints\n";
                    return;
                }
                out_ << "num  enabled  address             hits  where\n";
                for (const auto& row : list.rows) {
                    out_ << std::format("{:<5}{:<9}{:#018x}  {:<6}{}{}\n", row.breakpoint.id.value(),
                                        row.breakpoint.enabled ? "yes" : "no",
                                        row.breakpoint.address.value(), row.breakpoint.hit_count,
                                        describe(row.where), condition_suffix(row.breakpoint.condition));
                }
            },
            [&](const commands::ListOutput& list) {
                std::uint32_t number = list.first_line;
                for (const std::string& line : list.lines) {
                    out_ << std::format("{}\t{}\n", number++, line);
                }
            },
            [&](const commands::InfoLineOutput& info) {
                if (!info.line) {
                    out_ << std::format("No line number information available for address {:#x}{}\n",
                                        info.where.address.value(), symbolic(info.where));
                    return;
                }
                if (info.requested_line) {
                    out_ << std::format("Line {} of \"{}\" is at address {:#x}{} but contains no code.\n",
                                        *info.requested_line, info.line->location.file,
                                        info.line->start.value(), symbolic(*info.start));
                    return;
                }
                out_ << std::format("Line {} of \"{}\" starts at address {:#x}{} and ends at {:#x}{}.\n",
                                    info.line->location.line, info.line->location.file,
                                    info.line->start.value(), symbolic(*info.start),
                                    info.line->end.value(), symbolic(*info.end));
            },
            [&](const commands::HelpOutput& help) {
                for (const auto& info : help.commands) {
                    if (!help.detailed) {
                        out_ << std::format("{:<10}{}\n", info.name, info.help);
                        continue;
                    }
                    out_ << std::format("usage: {}\n{}\n", info.usage, info.help);
                    if (!info.aliases.empty()) {
                        out_ << "aliases:";
                        for (const auto& alias : info.aliases) {
                            out_ << ' ' << alias;
                        }
                        out_ << '\n';
                    }
                }
            },
        },
        output);
    out_ << std::flush;
}

}  // namespace stepwise::ui
