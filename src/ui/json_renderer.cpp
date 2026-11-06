#include "json_renderer.hpp"

#include <cstddef>
#include <format>
#include <ostream>
#include <string>
#include <variant>

#include <stepwise/backend/signal_name.hpp>
#include <stepwise/base/overloaded.hpp>

#include "json_writer.hpp"

namespace stepwise::ui {
namespace {

std::string hex(std::uint64_t value) {
    return std::format("{:#x}", value);
}

const char* error_code_name(ErrorCode code) {
    switch (code) {
        case ErrorCode::InvalidArgument:
            return "invalid_argument";
        case ErrorCode::InvalidState:
            return "invalid_state";
        case ErrorCode::NotFound:
            return "not_found";
        case ErrorCode::AlreadyExists:
            return "already_exists";
        case ErrorCode::Os:
            return "os";
        case ErrorCode::Protocol:
            return "protocol";
    }
    return "unknown";
}

void location_fields(JsonWriter& json, const core::CodeLocation& where) {
    if (where.function) {
        json.key("function").string(where.function->name);
        json.key("offset").number(where.function->offset);
    }
    if (where.source) {
        json.key("file").string(where.source->file);
        json.key("line").number(static_cast<std::uint64_t>(where.source->line));
    }
}

void frames_array(JsonWriter& json, const std::vector<core::Frame>& frames) {
    json.begin_array();
    for (std::size_t i = 0; i < frames.size(); ++i) {
        json.begin_object();
        json.key("index").number(static_cast<std::uint64_t>(i));
        json.key("pc").string(hex(frames[i].pc.value()));
        location_fields(json, frames[i].where);
        json.end_object();
    }
    json.end_array();
}

void registers_object(JsonWriter& json, const backend::Registers& registers) {
    json.begin_object();
    for (const auto& field : backend::register_fields) {
        json.key(field.name).string(hex(registers.*field.member));
    }
    json.end_object();
}

}  // namespace

void JsonRenderer::on_error(const Error& error) {
    JsonWriter json{out_};
    json.begin_object();
    json.key("type").string("error");
    json.key("code").string(error_code_name(error.code()));
    json.key("message").string(error.message());
    json.end_object();
    json.finish();
}

void JsonRenderer::on_event(const core::Event& event) {
    JsonWriter json{out_};
    json.begin_object();
    std::visit(
        Overloaded{
            [&](const core::StoppedEvent& stop) {
                json.key("type").string("stopped");
                json.key("pid").number(static_cast<std::int64_t>(stop.pid.value()));
                json.key("pc").string(hex(stop.pc.value()));
                json.key("reason").string(core::to_string(stop.reason));
                location_fields(json, stop.where);
                if (stop.where.source) {
                    if (auto text = session_.source_line(stop.where.source->file, stop.where.source->line)) {
                        json.key("source").string(*text);
                    }
                }
            },
            [&](const core::BreakpointHitEvent& hit) {
                json.key("type").string("breakpoint_hit");
                json.key("pid").number(static_cast<std::int64_t>(hit.pid.value()));
                json.key("id").number(static_cast<std::uint64_t>(hit.id.value()));
                json.key("pc").string(hex(hit.pc.value()));
                json.key("hit_count").number(hit.hit_count);
            },
            [&](const core::SignalReceivedEvent& received) {
                json.key("type").string("signal");
                json.key("pid").number(static_cast<std::int64_t>(received.pid.value()));
                json.key("signal").number(static_cast<std::int64_t>(received.signal.value()));
                json.key("name").string(backend::signal_name(received.signal));
            },
            [&](const core::CrashReportEvent& crash) {
                json.key("type").string("crash_report");
                json.key("pid").number(static_cast<std::int64_t>(crash.pid.value()));
                json.key("signal").number(static_cast<std::int64_t>(crash.signal.value()));
                json.key("name").string(backend::signal_name(crash.signal));
                json.key("description").string(backend::signal_description(crash.signal));
                if (crash.fault_address) {
                    json.key("fault_address").string(hex(crash.fault_address->value()));
                } else {
                    json.key("fault_address").null();
                }
                json.key("pc").string(hex(crash.where.address.value()));
                location_fields(json, crash.where);
                if (crash.where.source) {
                    if (auto text = session_.source_line(crash.where.source->file, crash.where.source->line)) {
                        json.key("source").string(*text);
                    }
                }
                json.key("registers");
                registers_object(json, crash.registers);
                json.key("backtrace");
                frames_array(json, crash.backtrace);
            },
            [&](const core::ExitedEvent& exited) {
                json.key("type").string("exited");
                json.key("pid").number(static_cast<std::int64_t>(exited.pid.value()));
                std::visit(Overloaded{
                               [&](int code) {
                                   json.key("exit_code").number(static_cast<std::int64_t>(code));
                               },
                               [&](Signal signal) {
                                   json.key("signal").number(static_cast<std::int64_t>(signal.value()));
                                   json.key("name").string(backend::signal_name(signal));
                               },
                           },
                           exited.status);
            },
        },
        event);
    json.end_object();
    json.finish();
}

void JsonRenderer::on_output(const commands::CommandOutput& output) {
    if (std::holds_alternative<commands::NoOutput>(output) ||
        std::holds_alternative<commands::QuitOutput>(output)) {
        return;
    }

    JsonWriter json{out_};
    json.begin_object();
    std::visit(
        Overloaded{
            [](const commands::NoOutput&) {},
            [](const commands::QuitOutput&) {},
            [&](const commands::RegistersOutput& registers) {
                json.key("type").string("registers");
                json.key("registers");
                registers_object(json, registers.registers);
            },
            [&](const commands::MemoryOutput& memory) {
                json.key("type").string("memory");
                json.key("address").string(hex(memory.address.value()));
                json.key("bytes").begin_array();
                for (const std::byte byte : memory.bytes) {
                    json.number(static_cast<std::uint64_t>(std::to_integer<unsigned>(byte)));
                }
                json.end_array();
            },
            [&](const commands::BreakpointSetOutput& set) {
                json.key("type").string("breakpoint_set");
                json.key("id").number(static_cast<std::uint64_t>(set.breakpoint.id.value()));
                json.key("address").string(hex(set.breakpoint.address.value()));
                if (set.breakpoint.condition) {
                    json.key("condition").string(set.breakpoint.condition->text);
                }
                location_fields(json, set.where);
            },
            [&](const commands::BreakpointListOutput& list) {
                json.key("type").string("breakpoints");
                json.key("breakpoints").begin_array();
                for (const auto& row : list.rows) {
                    json.begin_object();
                    json.key("id").number(static_cast<std::uint64_t>(row.breakpoint.id.value()));
                    json.key("address").string(hex(row.breakpoint.address.value()));
                    json.key("enabled").boolean(row.breakpoint.enabled);
                    json.key("hit_count").number(row.breakpoint.hit_count);
                    if (row.breakpoint.condition) {
                        json.key("condition").string(row.breakpoint.condition->text);
                    }
                    location_fields(json, row.where);
                    json.end_object();
                }
                json.end_array();
            },
            [&](const commands::BacktraceOutput& trace) {
                json.key("type").string("backtrace");
                json.key("frames");
                frames_array(json, trace.frames);
            },
            [&](const commands::ListOutput& list) {
                json.key("type").string("list");
                json.key("file").string(list.file);
                json.key("first_line").number(static_cast<std::uint64_t>(list.first_line));
                json.key("lines").begin_array();
                for (const std::string& line : list.lines) {
                    json.string(line);
                }
                json.end_array();
            },
            [&](const commands::InfoLineOutput& info) {
                json.key("type").string("line_info");
                json.key("address").string(hex(info.where.address.value()));
                json.key("found").boolean(info.line.has_value());
                if (info.line) {
                    json.key("file").string(info.line->location.file);
                    json.key("line").number(static_cast<std::uint64_t>(info.line->location.line));
                    json.key("start").string(hex(info.line->start.value()));
                    json.key("end").string(hex(info.line->end.value()));
                    if (info.requested_line) {
                        json.key("requested_line").number(static_cast<std::uint64_t>(*info.requested_line));
                    }
                }
            },
            [&](const commands::HelpOutput& help) {
                json.key("type").string("help");
                json.key("commands").begin_array();
                for (const auto& info : help.commands) {
                    json.begin_object();
                    json.key("name").string(info.name);
                    json.key("aliases").begin_array();
                    for (const std::string& alias : info.aliases) {
                        json.string(alias);
                    }
                    json.end_array();
                    json.key("usage").string(info.usage);
                    json.key("help").string(info.help);
                    json.end_object();
                }
                json.end_array();
            },
        },
        output);
    json.end_object();
    json.finish();
}

}  // namespace stepwise::ui
