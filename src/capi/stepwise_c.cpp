#include <stepwise/stepwise.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <format>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include <stepwise/backend/fake_debuggee.hpp>
#include <stepwise/backend/ptrace_debuggee.hpp>
#include <stepwise/base/overloaded.hpp>
#include <stepwise/commands/builtin_commands.hpp>
#include <stepwise/commands/command_registry.hpp>
#include <stepwise/core/session.hpp>
#include <stepwise/symbols/elf_symbol_provider.hpp>
#include <stepwise/symbols/null_symbol_provider.hpp>

namespace {

using namespace stepwise;

constexpr std::size_t max_memory_transfer = 16U * 1024U * 1024U;

stepwise_status to_status(ErrorCode code) {
    switch (code) {
        case ErrorCode::InvalidArgument:
        case ErrorCode::Protocol:
            return STEPWISE_ERR_INVALID_ARGUMENT;
        case ErrorCode::InvalidState:
            return STEPWISE_ERR_INVALID_STATE;
        case ErrorCode::NotFound:
            return STEPWISE_ERR_NOT_FOUND;
        case ErrorCode::AlreadyExists:
            return STEPWISE_ERR_ALREADY_EXISTS;
        case ErrorCode::Os:
            return STEPWISE_ERR_OS;
    }
    return STEPWISE_ERR_INTERNAL;
}

stepwise_state to_state(core::SessionState state) {
    switch (state) {
        case core::SessionState::NotStarted:
            return STEPWISE_STATE_NOT_STARTED;
        case core::SessionState::Stopped:
            return STEPWISE_STATE_STOPPED;
        case core::SessionState::Running:
            return STEPWISE_STATE_RUNNING;
        case core::SessionState::Exited:
            return STEPWISE_STATE_EXITED;
    }
    return STEPWISE_STATE_NOT_STARTED;
}

void copy_message(char* buffer, std::size_t size, std::string_view message) noexcept {
    if (buffer == nullptr || size == 0) {
        return;
    }
    const std::size_t length = std::min(message.size(), size - 1);
    std::memcpy(buffer, message.data(), length);
    buffer[length] = '\0';
}

struct StopRecord {
    stepwise_stop_kind kind{STEPWISE_STOP_NONE};
    std::int32_t pid{0};
    std::int32_t signal{0};
    std::int32_t exit_code{0};
    std::uint32_t breakpoint_id{0};
    std::uint64_t pc{0};
    std::uint64_t hit_count{0};
    std::optional<std::uint64_t> fault_address;
    std::uint32_t line{0};
    std::optional<std::string> function;
    std::optional<std::string> file;
};

}  // namespace

struct stepwise_session {
    stepwise_session(std::unique_ptr<backend::Debuggee> debuggee,
                     std::unique_ptr<symbols::SymbolProvider> symbols, backend::LaunchSpec spec)
        : debugger(std::move(debuggee), std::move(symbols), std::move(spec)),
          subscription(debugger.events().subscribe([this](const core::Event& event) { on_event(event); })) {}

    stepwise_session(const stepwise_session&) = delete;
    stepwise_session& operator=(const stepwise_session&) = delete;

    void on_event(const core::Event& event);

    core::Session debugger;
    commands::CommandRegistry registry;
    std::string error;
    StopRecord stop;
    std::vector<std::string> trace_strings;

    std::optional<std::pair<std::uint32_t, std::uint64_t>> pending_hit;
    std::optional<std::int32_t> pending_signal;
    std::optional<std::uint64_t> pending_fault;

    core::Subscription subscription;
};

void stepwise_session::on_event(const core::Event& event) {
    std::visit(
        Overloaded{
            [&](const core::BreakpointHitEvent& hit) {
                pending_hit = std::pair{hit.id.value(), hit.hit_count};
            },
            [&](const core::SignalReceivedEvent& received) { pending_signal = received.signal.value(); },
            [&](const core::CrashReportEvent& crash) {
                if (crash.fault_address) {
                    pending_fault = crash.fault_address->value();
                }
            },
            [&](const core::StoppedEvent& stopped) {
                StopRecord next;
                next.pid = stopped.pid.value();
                next.pc = stopped.pc.value();
                switch (stopped.reason) {
                    case core::StopReason::Entry:
                        next.kind = STEPWISE_STOP_ENTRY;
                        break;
                    case core::StopReason::SingleStep:
                        next.kind = STEPWISE_STOP_SINGLE_STEP;
                        break;
                    case core::StopReason::LineStep:
                        next.kind = STEPWISE_STOP_LINE_STEP;
                        break;
                    case core::StopReason::Breakpoint:
                        next.kind = STEPWISE_STOP_BREAKPOINT;
                        break;
                    case core::StopReason::Trap:
                        next.kind = STEPWISE_STOP_TRAP;
                        break;
                    case core::StopReason::Signal:
                        next.kind = STEPWISE_STOP_SIGNAL;
                        break;
                }
                if (pending_hit) {
                    next.breakpoint_id = pending_hit->first;
                    next.hit_count = pending_hit->second;
                }
                next.signal = pending_signal.value_or(0);
                next.fault_address = pending_fault;
                if (stopped.where.function) {
                    next.function = stopped.where.function->name;
                }
                if (stopped.where.source) {
                    next.file = stopped.where.source->file;
                    next.line = stopped.where.source->line;
                }
                stop = std::move(next);
                pending_hit.reset();
                pending_signal.reset();
                pending_fault.reset();
            },
            [&](const core::ExitedEvent& exited) {
                StopRecord next;
                next.pid = exited.pid.value();
                if (const int* code = std::get_if<int>(&exited.status)) {
                    next.kind = STEPWISE_STOP_EXITED;
                    next.exit_code = *code;
                } else {
                    next.kind = STEPWISE_STOP_KILLED;
                    next.signal = std::get<Signal>(exited.status).value();
                }
                stop = std::move(next);
                pending_hit.reset();
                pending_signal.reset();
                pending_fault.reset();
            },
        },
        event);
}

namespace {

void set_error(stepwise_session& session, std::string_view message) noexcept {
    try {
        session.error.assign(message);
    } catch (...) {
        session.error.clear();
    }
}

stepwise_status fail(stepwise_session& session, const Error& error) noexcept {
    set_error(session, error.message());
    return to_status(error.code());
}

stepwise_status fail(stepwise_session& session, stepwise_status status, std::string_view message) noexcept {
    set_error(session, message);
    return status;
}

template <class Body>
stepwise_status guarded(stepwise_session* session, Body&& body) noexcept {
    if (session == nullptr) {
        return STEPWISE_ERR_INVALID_ARGUMENT;
    }
    try {
        return body(*session);
    } catch (const std::exception& exception) {
        set_error(*session, exception.what());
    } catch (...) {
        set_error(*session, "unknown internal error");
    }
    return STEPWISE_ERR_INTERNAL;
}

stepwise_status from_result(stepwise_session& session, const Result<void>& result) noexcept {
    return result ? STEPWISE_OK : fail(session, result.error());
}

stepwise_status create(std::unique_ptr<backend::Debuggee> debuggee,
                       std::unique_ptr<symbols::SymbolProvider> symbols, backend::LaunchSpec spec,
                       stepwise_session** out_session, char* error, std::size_t error_size) noexcept {
    try {
        auto session = std::make_unique<stepwise_session>(std::move(debuggee), std::move(symbols),
                                                          std::move(spec));
        if (auto registered = commands::register_builtin_commands(session->registry); !registered) {
            copy_message(error, error_size, registered.error().message());
            return STEPWISE_ERR_INTERNAL;
        }
        *out_session = session.release();
        return STEPWISE_OK;
    } catch (const std::bad_alloc&) {
        copy_message(error, error_size, "out of memory");
    } catch (...) {
        copy_message(error, error_size, "unknown internal error");
    }
    return STEPWISE_ERR_INTERNAL;
}

Result<commands::CommandOutput> execute(stepwise_session& session, const std::string& line) {
    auto command = session.registry.parse_line(line);
    if (!command) {
        return command.error();
    }
    commands::CommandContext context{session.debugger, session.registry};
    return command.value()->execute(context);
}

bool has_blank(const char* text) {
    for (; *text != '\0'; ++text) {
        if (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r') {
            return true;
        }
    }
    return false;
}

const char* c_str_or_null(const std::optional<std::string>& text) {
    return text ? text->c_str() : nullptr;
}

}  // namespace

extern "C" {

STEPWISE_API uint32_t stepwise_api_version(void) {
    return (static_cast<uint32_t>(STEPWISE_API_VERSION_MAJOR) << 16) | STEPWISE_API_VERSION_MINOR;
}

STEPWISE_API const char* stepwise_status_string(stepwise_status status) {
    switch (status) {
        case STEPWISE_OK:
            return "success";
        case STEPWISE_ERR_INVALID_ARGUMENT:
            return "invalid argument";
        case STEPWISE_ERR_INVALID_STATE:
            return "not allowed in the current state";
        case STEPWISE_ERR_NOT_FOUND:
            return "not found";
        case STEPWISE_ERR_ALREADY_EXISTS:
            return "already exists";
        case STEPWISE_ERR_OS:
            return "operating system error";
        case STEPWISE_ERR_INTERNAL:
            return "internal error";
        default:
            return "unknown status";
    }
}

STEPWISE_API stepwise_status stepwise_open(const char* program, const char* const* args,
                                           stepwise_session** out_session, char* error, size_t error_size) {
    copy_message(error, error_size, "");
    if (out_session == nullptr) {
        copy_message(error, error_size, "out_session is NULL");
        return STEPWISE_ERR_INVALID_ARGUMENT;
    }
    *out_session = nullptr;
    if (program == nullptr || *program == '\0') {
        copy_message(error, error_size, "program is NULL or empty");
        return STEPWISE_ERR_INVALID_ARGUMENT;
    }

    try {
        backend::LaunchSpec spec;
        spec.program = program;
        for (; args != nullptr && *args != nullptr; ++args) {
            spec.args.emplace_back(*args);
        }

        std::unique_ptr<symbols::SymbolProvider> symbols;
        if (auto loaded = symbols::ElfSymbolProvider::load(spec.program)) {
            symbols = std::move(loaded.value());
        } else {
            symbols = std::make_unique<symbols::NullSymbolProvider>();
        }
        return create(std::make_unique<backend::PtraceDebuggee>(), std::move(symbols), std::move(spec),
                      out_session, error, error_size);
    } catch (const std::bad_alloc&) {
        copy_message(error, error_size, "out of memory");
    } catch (...) {
        copy_message(error, error_size, "unknown internal error");
    }
    return STEPWISE_ERR_INTERNAL;
}

STEPWISE_API stepwise_status stepwise_open_demo(stepwise_session** out_session) {
    if (out_session == nullptr) {
        return STEPWISE_ERR_INVALID_ARGUMENT;
    }
    *out_session = nullptr;
    backend::LaunchSpec spec;
    spec.program = "demo";
    try {
        return create(std::make_unique<backend::FakeDebuggee>(backend::demo_fake_program()),
                      std::make_unique<symbols::NullSymbolProvider>(), std::move(spec), out_session,
                      nullptr, 0);
    } catch (...) {
        return STEPWISE_ERR_INTERNAL;
    }
}

STEPWISE_API void stepwise_close(stepwise_session* session) {
    const std::unique_ptr<stepwise_session> owner{session};
}

STEPWISE_API const char* stepwise_last_error(const stepwise_session* session) {
    return session == nullptr ? "" : session->error.c_str();
}

STEPWISE_API stepwise_state stepwise_get_state(const stepwise_session* session) {
    return session == nullptr ? STEPWISE_STATE_NOT_STARTED : to_state(session->debugger.state());
}

STEPWISE_API stepwise_status stepwise_run(stepwise_session* session) {
    return guarded(session, [](stepwise_session& s) { return from_result(s, s.debugger.run()); });
}

STEPWISE_API stepwise_status stepwise_continue(stepwise_session* session) {
    return guarded(session, [](stepwise_session& s) { return from_result(s, s.debugger.cont()); });
}

STEPWISE_API stepwise_status stepwise_step_instruction(stepwise_session* session) {
    return guarded(session, [](stepwise_session& s) { return from_result(s, s.debugger.step_instruction()); });
}

STEPWISE_API stepwise_status stepwise_step_line(stepwise_session* session) {
    return guarded(session, [](stepwise_session& s) { return from_result(s, s.debugger.step_line()); });
}

STEPWISE_API stepwise_status stepwise_get_stop_info(stepwise_session* session, stepwise_stop_info* out) {
    return guarded(session, [out](stepwise_session& s) -> stepwise_status {
        if (out == nullptr) {
            return fail(s, STEPWISE_ERR_INVALID_ARGUMENT, "out is NULL");
        }
        if (out->struct_size < sizeof(stepwise_stop_info)) {
            return fail(s, STEPWISE_ERR_INVALID_ARGUMENT,
                        "out->struct_size must be set to sizeof(stepwise_stop_info)");
        }
        const StopRecord& stop = s.stop;
        stepwise_stop_info info{};
        info.struct_size = out->struct_size;
        info.kind = stop.kind;
        info.pid = stop.pid;
        info.signal = stop.signal;
        info.exit_code = stop.exit_code;
        info.breakpoint_id = stop.breakpoint_id;
        info.pc = stop.pc;
        info.hit_count = stop.hit_count;
        info.fault_address = stop.fault_address.value_or(0);
        info.has_fault_address = stop.fault_address.has_value() ? 1 : 0;
        info.line = stop.line;
        info.function = c_str_or_null(stop.function);
        info.file = c_str_or_null(stop.file);
        std::memcpy(out, &info, sizeof info);
        return STEPWISE_OK;
    });
}

STEPWISE_API stepwise_status stepwise_set_breakpoint(stepwise_session* session, const char* location,
                                                     const char* condition, uint32_t* out_id) {
    return guarded(session, [=](stepwise_session& s) -> stepwise_status {
        if (location == nullptr || *location == '\0' || has_blank(location) || out_id == nullptr) {
            return fail(s, STEPWISE_ERR_INVALID_ARGUMENT,
                        "location must be a non-empty word without spaces, and out_id must not be NULL");
        }
        std::string line = std::format("break {}", location);
        if (condition != nullptr && *condition != '\0') {
            line += std::format(" if {}", condition);
        }
        auto output = execute(s, line);
        if (!output) {
            return fail(s, output.error());
        }
        const auto* set = std::get_if<commands::BreakpointSetOutput>(&output.value());
        if (set == nullptr) {
            return fail(s, STEPWISE_ERR_INTERNAL, "the break command returned an unexpected result");
        }
        *out_id = set->breakpoint.id.value();
        return STEPWISE_OK;
    });
}

STEPWISE_API stepwise_status stepwise_delete_breakpoint(stepwise_session* session, uint32_t id) {
    return guarded(session, [id](stepwise_session& s) {
        return from_result(s, s.debugger.breakpoints().remove(BreakpointId{id}));
    });
}

STEPWISE_API stepwise_status stepwise_read_register(stepwise_session* session, const char* name,
                                                    uint64_t* out_value) {
    return guarded(session, [name, out_value](stepwise_session& s) -> stepwise_status {
        if (name == nullptr || out_value == nullptr) {
            return fail(s, STEPWISE_ERR_INVALID_ARGUMENT, "name and out_value must not be NULL");
        }
        const std::string_view wanted{name};
        const auto field = std::ranges::find_if(backend::register_fields,
                                                [wanted](const backend::RegisterField& f) { return f.name == wanted; });
        if (field == backend::register_fields.end()) {
            return fail(s, STEPWISE_ERR_NOT_FOUND, std::format("unknown register '{}'", wanted));
        }
        auto registers = s.debugger.registers();
        if (!registers) {
            return fail(s, registers.error());
        }
        *out_value = registers.value().*field->member;
        return STEPWISE_OK;
    });
}

STEPWISE_API stepwise_status stepwise_read_memory(stepwise_session* session, uint64_t address, void* buffer,
                                                  size_t length) {
    return guarded(session, [=](stepwise_session& s) -> stepwise_status {
        if ((buffer == nullptr && length != 0) || length > max_memory_transfer) {
            return fail(s, STEPWISE_ERR_INVALID_ARGUMENT,
                        "buffer must not be NULL and length must be at most 16 MiB");
        }
        auto bytes = s.debugger.read_memory(Address{address}, length);
        if (!bytes) {
            return fail(s, bytes.error());
        }
        if (length != 0) {
            std::memcpy(buffer, bytes.value().data(), length);
        }
        return STEPWISE_OK;
    });
}

STEPWISE_API stepwise_status stepwise_write_memory(stepwise_session* session, uint64_t address,
                                                   const void* data, size_t length) {
    return guarded(session, [=](stepwise_session& s) -> stepwise_status {
        if ((data == nullptr && length != 0) || length > max_memory_transfer) {
            return fail(s, STEPWISE_ERR_INVALID_ARGUMENT,
                        "data must not be NULL and length must be at most 16 MiB");
        }
        const std::span<const std::byte> bytes{static_cast<const std::byte*>(data), length};
        return from_result(s, s.debugger.write_memory(Address{address}, bytes));
    });
}

STEPWISE_API stepwise_status stepwise_backtrace(stepwise_session* session, stepwise_frame* frames,
                                                size_t capacity, size_t* out_count) {
    return guarded(session, [=](stepwise_session& s) -> stepwise_status {
        if (out_count == nullptr || (frames == nullptr && capacity != 0)) {
            return fail(s, STEPWISE_ERR_INVALID_ARGUMENT,
                        "out_count must not be NULL, and frames not NULL unless capacity is 0");
        }
        auto trace = s.debugger.backtrace();
        if (!trace) {
            return fail(s, trace.error());
        }
        const auto& found = trace.value();
        *out_count = found.size();

        const std::size_t filled = std::min(capacity, found.size());
        s.trace_strings.clear();
        s.trace_strings.reserve(filled * 2);
        for (std::size_t i = 0; i < filled; ++i) {
            s.trace_strings.push_back(found[i].where.function ? found[i].where.function->name : std::string{});
            s.trace_strings.push_back(found[i].where.source ? found[i].where.source->file : std::string{});
        }
        for (std::size_t i = 0; i < filled; ++i) {
            const core::CodeLocation& where = found[i].where;
            frames[i] = stepwise_frame{found[i].pc.value(),
                                       where.function ? s.trace_strings[2 * i].c_str() : nullptr,
                                       where.source ? s.trace_strings[2 * i + 1].c_str() : nullptr,
                                       where.source ? where.source->line : 0U, 0U};
        }
        return STEPWISE_OK;
    });
}

}  // extern "C"
