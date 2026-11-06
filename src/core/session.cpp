#include <stepwise/core/session.hpp>

#include <algorithm>
#include <array>
#include <csignal>
#include <format>
#include <utility>
#include <variant>

#include <stepwise/base/overloaded.hpp>

namespace stepwise::core {
namespace {

bool is_trap_stop(const backend::DebugEvent& event) {
    const auto* stop = std::get_if<backend::ProcessStopped>(&event);
    return stop != nullptr && stop->signal == Signal{SIGTRAP};
}

constexpr std::uint64_t max_instruction_length = 15;

constexpr std::size_t max_backtrace_frames = 256;
constexpr std::byte opcode_ret{0xC3};
constexpr std::byte opcode_push_rbp{0x55};
constexpr std::array<std::byte, 4> opcode_endbr64{std::byte{0xF3}, std::byte{0x0F}, std::byte{0x1E},
                                                  std::byte{0xFA}};

}  // namespace

Session::Session(std::unique_ptr<backend::Debuggee> debuggee,
                 std::unique_ptr<symbols::SymbolProvider> symbols, backend::LaunchSpec spec,
                 Logger logger)
    : debuggee_(std::move(debuggee)),
      symbols_(std::move(symbols)),
      spec_(std::move(spec)),
      logger_(logger) {}

Result<void> Session::require_stopped() const {
    if (state_.current() != SessionState::Stopped) {
        return Error{ErrorCode::InvalidState,
                     std::format("process is not stopped (session is {})",
                                 to_string(state_.current()))};
    }
    return {};
}

Result<void> Session::run() {
    if (!state_.can_transition_to(SessionState::Stopped)) {
        return Error{ErrorCode::InvalidState,
                     std::format("cannot run: session is {}", to_string(state_.current()))};
    }
    auto pid = debuggee_->launch(spec_);
    if (!pid) {
        return pid.error();
    }
    pid_ = pid.value();
    logger_.info(std::format("launched '{}' as process {}", spec_.program, pid.value().value()));
    pending_signal_.reset();
    breakpoints_.forget_inserted();
    if (auto moved = state_.transition_to(SessionState::Stopped); !moved) {
        return moved;
    }
    return publish_stop(StopReason::Entry);
}

Result<void> Session::cont() {
    return resume(ResumeMode::Continue);
}

Result<void> Session::step_instruction() {
    return resume(ResumeMode::SingleStep);
}

Result<void> Session::step_line() {
    if (auto stopped = require_stopped(); !stopped) {
        return stopped;
    }
    auto registers = debuggee_->read_registers();
    if (!registers) {
        return registers.error();
    }
    if (!symbols_->line_at(Address{registers.value().rip})) {
        return Error{ErrorCode::InvalidState,
                     "no line information for the current location; use stepi"};
    }
    return resume(ResumeMode::StepLine);
}

Result<backend::Registers> Session::registers() {
    if (auto stopped = require_stopped(); !stopped) {
        return stopped.error();
    }
    return debuggee_->read_registers();
}

std::optional<std::uint64_t> Session::read_word(std::uint64_t address) {
    std::uint64_t word = 0;
    if (!debuggee_->read_memory(Address{address}, std::as_writable_bytes(std::span{&word, 1}))) {
        return std::nullopt;
    }
    return word;
}

Session::FrameLayout Session::innermost_layout(Address pc, std::uint64_t sp) {
    std::byte at_pc{};
    if (debuggee_->read_memory(pc, std::span<std::byte>{&at_pc, 1}) && at_pc == opcode_ret) {
        return FrameLayout::NotYetSaved;
    }

    const auto function = symbols_->function_at(pc);
    if (!function) {
        const auto candidate = read_word(sp);
        if (candidate && symbols_->function_at(Address{*candidate - 1})) {
            return FrameLayout::NotYetSaved;
        }
        return FrameLayout::Standard;
    }
    std::array<std::byte, 5> code{};
    if (!debuggee_->read_memory(function->address, code)) {
        return FrameLayout::Standard;
    }
    std::size_t push_at = 0;
    if (std::ranges::equal(std::span{code}.first(4), opcode_endbr64)) {
        push_at = opcode_endbr64.size();
    }
    if (push_at >= code.size() || code[push_at] != opcode_push_rbp) {
        return FrameLayout::Standard;
    }
    const std::uint64_t offset = pc.value() - function->address.value();
    if (offset <= push_at) {
        return FrameLayout::NotYetSaved;
    }
    return offset == push_at + 1 ? FrameLayout::RbpPushed : FrameLayout::Standard;
}

Result<std::vector<Frame>> Session::backtrace() {
    auto registers = this->registers();
    if (!registers) {
        return registers.error();
    }
    std::vector<Frame> frames;
    std::uint64_t pc = registers.value().rip;
    std::uint64_t sp = registers.value().rsp;
    std::uint64_t fp = registers.value().rbp;
    bool innermost = true;

    while (frames.size() < max_backtrace_frames) {
        const Address lookup{innermost ? pc : pc - 1};
        CodeLocation where = describe(lookup);
        where.address = Address{pc};
        const bool reached_main = where.function && where.function->name == "main";
        frames.push_back(Frame{Address{pc}, std::move(where)});
        if (reached_main) {
            break;
        }

        std::optional<std::uint64_t> return_address;
        std::optional<std::uint64_t> caller_fp;
        switch (innermost ? innermost_layout(Address{pc}, sp) : FrameLayout::Standard) {
            case FrameLayout::NotYetSaved:
                return_address = read_word(sp);
                caller_fp = fp;
                break;
            case FrameLayout::RbpPushed:
                return_address = read_word(sp + 8);
                caller_fp = read_word(sp);
                break;
            case FrameLayout::Standard:
                if (fp == 0) {
                    break;
                }
                return_address = read_word(fp + 8);
                caller_fp = read_word(fp);
                if (caller_fp && *caller_fp != 0 && *caller_fp <= fp) {
                    caller_fp.reset();
                }
                break;
        }
        if (!return_address || !caller_fp || *return_address == 0) {
            break;
        }
        pc = *return_address;
        fp = *caller_fp;
        innermost = false;
    }
    return frames;
}

void Session::publish_crash_report(Signal signal) {
    auto registers = debuggee_->read_registers();
    if (!registers) {
        return;
    }
    std::optional<Address> fault;
    if (auto address = debuggee_->fault_address()) {
        fault = address.value();
    }
    std::vector<Frame> frames;
    if (auto trace = backtrace()) {
        frames = std::move(trace.value());
    }
    events_.publish(CrashReportEvent{*pid_, signal, fault, registers.value(),
                                     describe(Address{registers.value().rip}), std::move(frames)});
}

Result<bool> Session::condition_holds(BreakpointId id) {
    const Breakpoint* breakpoint = breakpoints_.find(id);
    if (breakpoint == nullptr || !breakpoint->condition) {
        return true;
    }
    auto registers = debuggee_->read_registers();
    if (!registers) {
        return registers.error();
    }
    return breakpoint->condition->holds(registers.value());
}

Result<Session::Resumed> Session::continue_until_stop(std::optional<Signal>& deliver,
                                                      std::optional<Address> until) {
    while (true) {
        auto resumed = continue_with_breakpoints(deliver, until);
        if (!resumed || !resumed.value().hit) {
            return resumed;
        }
        auto holds = condition_holds(*resumed.value().hit);
        if (!holds) {
            return holds.error();
        }
        if (holds.value()) {
            return resumed;
        }
        logger_.debug(std::format("breakpoint {} reached but its condition is false",
                                  resumed.value().hit->value()));
        if (resumed.value().reached_temporary) {
            Resumed without_hit = resumed.value();
            without_hit.hit.reset();
            return without_hit;
        }
    }
}

Result<std::vector<std::byte>> Session::read_memory(Address address, std::size_t length) {
    if (auto stopped = require_stopped(); !stopped) {
        return stopped.error();
    }
    std::vector<std::byte> bytes(length);
    if (auto read = debuggee_->read_memory(address, bytes); !read) {
        return read.error();
    }
    return bytes;
}

Result<void> Session::write_memory(Address address, std::span<const std::byte> data) {
    if (auto stopped = require_stopped(); !stopped) {
        return stopped;
    }
    return debuggee_->write_memory(address, data);
}

CodeLocation Session::describe(Address address) const {
    CodeLocation location{address, std::nullopt, std::nullopt};
    if (auto function = symbols_->function_at(address)) {
        location.function =
            FunctionOffset{function->name, address.value() - function->address.value()};
    }
    if (auto line = symbols_->line_at(address)) {
        location.source = line->location;
    }
    return location;
}

Result<CodeLocation> Session::resolve_location(const LocationSpec& spec, bool skip_prologue) const {
    return std::visit(
        Overloaded{
            [&](Address address) -> Result<CodeLocation> { return describe(address); },
            [&](const FunctionSpec& function) -> Result<CodeLocation> {
                auto symbol = symbols_->find_function(function.name);
                if (!symbol) {
                    return Error{ErrorCode::NotFound,
                                 std::format("function '{}' not found", function.name)};
                }
                return describe(skip_prologue ? symbols_->breakpoint_address(*symbol)
                                              : symbol->address);
            },
            [&](const FileLineSpec& where) -> Result<CodeLocation> {
                auto line = symbols_->find_line(where.file, where.line);
                if (!line) {
                    return Error{ErrorCode::NotFound,
                                 std::format("no compiled code for line {} in file '{}'", where.line,
                                             where.file)};
                }
                Address address = line->start;
                if (skip_prologue) {
                    if (auto function = symbols_->function_at(address);
                        function && function->address == address) {
                        if (line->location.line != where.line) {
                            return Error{ErrorCode::NotFound,
                                         std::format("no compiled code for line {} in file '{}'",
                                                     where.line, where.file)};
                        }
                        address = symbols_->breakpoint_address(*function);
                    }
                }
                return describe(address);
            },
        },
        spec);
}

Result<CodeLocation> Session::current_location() {
    auto registers = this->registers();
    if (!registers) {
        return registers.error();
    }
    return describe(Address{registers.value().rip});
}

Result<std::span<const std::string>> Session::source_lines(const std::string& file) {
    return sources_.lines(file);
}

std::optional<std::string> Session::source_line(const std::string& file, std::uint32_t line) {
    auto lines = sources_.lines(file);
    if (!lines || line == 0 || line > lines.value().size()) {
        return std::nullopt;
    }
    return lines.value()[line - 1];
}

Result<void> Session::resume(ResumeMode mode) {
    if (auto moved = state_.transition_to(SessionState::Running); !moved) {
        return moved;
    }

    std::optional<Signal> deliver = pending_signal_;
    logger_.debug(std::format("resuming: {}{}",
                              mode == ResumeMode::Continue ? "continue"
                              : mode == ResumeMode::StepLine ? "step line"
                                                             : "step instruction",
                              deliver ? std::format(", delivering signal {}", deliver->value()) : ""));
    Result<Resumed> outcome = [&]() -> Result<Resumed> {
        switch (mode) {
            case ResumeMode::Continue:
                return continue_until_stop(deliver, std::nullopt);
            case ResumeMode::StepLine:
                return step_to_next_line(deliver);
            case ResumeMode::SingleStep:
                break;
        }
        auto event = debuggee_->step(std::exchange(deliver, std::nullopt));
        if (!event) {
            return event.error();
        }
        return Resumed{event.value(), std::nullopt, false};
    }();

    if (!outcome) {
        pending_signal_ = deliver;
        if (auto moved = state_.transition_to(SessionState::Stopped); !moved) {
            return moved;
        }
        return outcome.error();
    }
    pending_signal_.reset();
    return handle(outcome.value(), mode);
}

Result<Session::Resumed> Session::continue_with_breakpoints(std::optional<Signal>& deliver,
                                                            std::optional<Address> until) {
    breakpoints_.set_temporary(until);
    Result<Resumed> result = [&]() -> Result<Resumed> {
        auto registers = debuggee_->read_registers();
        if (!registers) {
            return registers.error();
        }

        // sitting on a breakpoint: step the original instruction first
        if (breakpoints_.has_enabled_at(Address{registers.value().rip})) {
            auto stepped = debuggee_->step(std::exchange(deliver, std::nullopt));
            if (!stepped) {
                return stepped.error();
            }
            if (!is_trap_stop(stepped.value())) {
                return Resumed{stepped.value(), std::nullopt, false};
            }
        }

        if (auto inserted = breakpoints_.insert_all(*debuggee_); !inserted) {
            return inserted.error();
        }
        auto event = debuggee_->cont(std::exchange(deliver, std::nullopt));
        if (!event) {
            static_cast<void>(breakpoints_.remove_all(*debuggee_));
            return event.error();
        }
        return collect_stop(event.value());
    }();
    breakpoints_.set_temporary(std::nullopt);
    return result;
}

Result<std::optional<Address>> Session::call_return_address(const backend::Registers& before,
                                                            const backend::Registers& now) {
    if (now.rsp + 8 != before.rsp) {
        return std::optional<Address>{};
    }
    std::uint64_t return_address = 0;
    if (auto read = debuggee_->read_memory(Address{now.rsp},
                                           std::as_writable_bytes(std::span{&return_address, 1}));
        !read) {
        return read.error();
    }
    if (return_address <= before.rip || return_address > before.rip + max_instruction_length) {
        return std::optional<Address>{};
    }
    return std::optional<Address>{Address{return_address}};
}

Result<bool> Session::returned_to_caller(const backend::Registers& before,
                                         const backend::Registers& now) {
    if (now.rsp != before.rsp + 8) {
        return false;
    }
    std::uint64_t popped = 0;
    if (auto read = debuggee_->read_memory(Address{before.rsp},
                                           std::as_writable_bytes(std::span{&popped, 1}));
        !read) {
        return read.error();
    }
    return popped == now.rip;
}

Result<Session::Resumed> Session::step_to_next_line(std::optional<Signal>& deliver) {
    auto start = debuggee_->read_registers();
    if (!start) {
        return start.error();
    }
    const Address start_pc{start.value().rip};
    const auto start_info = symbols_->line_at(start_pc);
    if (!start_info) {
        return Error{ErrorCode::InvalidState, "no line information for the current location"};
    }
    symbols::SourceLocation current_line = start_info->location;

    backend::Registers before = start.value();
    std::optional<Address> settle_at;

    while (true) {
        auto event = debuggee_->step(std::exchange(deliver, std::nullopt));
        if (!event) {
            return event.error();
        }
        backend::DebugEvent last = event.value();
        if (!is_trap_stop(last)) {
            return Resumed{last, std::nullopt, false};
        }
        auto after = debuggee_->read_registers();
        if (!after) {
            return after.error();
        }
        backend::Registers now = after.value();
        if (logger_.enabled(LogLevel::Trace)) {
            logger_.trace(std::format("step: pc {:#x}, sp {:#x}", now.rip, now.rsp));
        }

        if (settle_at) {
            if (Address{now.rip} == *settle_at) {
                return Resumed{last, std::nullopt, false};
            }
            before = now;
            continue;
        }

        auto info = symbols_->line_at(Address{now.rip});
        if (!info) {
            auto return_address = call_return_address(before, now);
            if (!return_address) {
                return return_address.error();
            }
            if (!return_address.value()) {
                return continue_until_stop(deliver, std::nullopt);
            }
            logger_.debug(std::format("stepping over a call into code without line info, "
                                      "return address {:#x}",
                                      return_address.value()->value()));
            auto resumed = continue_until_stop(deliver, *return_address.value());
            if (!resumed) {
                return resumed;
            }
            const Resumed& stop = resumed.value();
            if (stop.hit || !stop.reached_temporary || !is_trap_stop(stop.event)) {
                return resumed;
            }
            last = stop.event;
            auto again = debuggee_->read_registers();
            if (!again) {
                return again.error();
            }
            now = again.value();
            info = symbols_->line_at(Address{now.rip});
            if (!info) {
                return continue_until_stop(deliver, std::nullopt);
            }
        }

        const Address pc{now.rip};
        const auto function = symbols_->function_at(pc);
        if (function && function->address == pc) {
            const Address body = symbols_->breakpoint_address(*function);
            logger_.debug(std::format("entered {}, running its prologue up to {:#x}", function->name,
                                      body.value()));
            if (body == pc) {
                return Resumed{last, std::nullopt, false};
            }
            settle_at = body;
        } else {
            auto returned = returned_to_caller(before, now);
            if (!returned) {
                return returned.error();
            }
            if (returned.value()) {
                // back in the caller the current line is the call's line, even if the
                // code after the call starts a new row for that line
                const auto call_site = symbols_->line_at(Address{pc.value() - 1});
                current_line = call_site ? call_site->location : info->location;
            }
            if (info->start == pc && info->is_statement && info->location != current_line) {
                return Resumed{last, std::nullopt, false};
            }
        }
        before = now;
    }
}

Result<Session::Resumed> Session::collect_stop(const backend::DebugEvent& event) {
    if (!std::holds_alternative<backend::ProcessStopped>(event)) {
        breakpoints_.forget_inserted();
        return Resumed{event, std::nullopt, false};
    }

    auto registers = debuggee_->read_registers();
    if (!registers) {
        static_cast<void>(breakpoints_.remove_all(*debuggee_));
        return registers.error();
    }

    std::optional<BreakpointId> hit;
    bool reached_temporary = false;
    if (is_trap_stop(event)) {
        const Address patched{registers.value().rip - 1};
        const auto user = breakpoints_.inserted_at(patched);
        reached_temporary = breakpoints_.temporary_inserted_at(patched);
        if (user || reached_temporary) {
            backend::Registers rewound = registers.value();
            rewound.rip = patched.value();
            if (auto written = debuggee_->write_registers(rewound); !written) {
                static_cast<void>(breakpoints_.remove_all(*debuggee_));
                return written.error();
            }
            hit = user;
        }
    }

    if (auto removed = breakpoints_.remove_all(*debuggee_); !removed) {
        return removed.error();
    }
    return Resumed{event, hit, reached_temporary};
}

Result<void> Session::handle(const Resumed& resumed, ResumeMode mode) {
    return std::visit(
        Overloaded{
            [&](const backend::ProcessStopped& stop) -> Result<void> {
                if (auto moved = state_.transition_to(SessionState::Stopped); !moved) {
                    return moved;
                }
                if (stop.signal != Signal{SIGTRAP}) {
                    pending_signal_ = stop.signal;
                    events_.publish(SignalReceivedEvent{*pid_, stop.signal});
                    if (stop.signal == Signal{SIGSEGV}) {
                        publish_crash_report(stop.signal);
                    }
                    return publish_stop(StopReason::Signal);
                }
                if (resumed.hit) {
                    const std::uint64_t count = breakpoints_.record_hit(*resumed.hit);
                    auto registers = debuggee_->read_registers();
                    if (!registers) {
                        return registers.error();
                    }
                    events_.publish(BreakpointHitEvent{*pid_, *resumed.hit,
                                                       Address{registers.value().rip}, count});
                    return publish_stop(StopReason::Breakpoint);
                }
                switch (mode) {
                    case ResumeMode::SingleStep:
                        return publish_stop(StopReason::SingleStep);
                    case ResumeMode::StepLine:
                        return publish_stop(StopReason::LineStep);
                    case ResumeMode::Continue:
                        break;
                }
                return publish_stop(StopReason::Trap);
            },
            [&](const backend::ProcessExited& exit) -> Result<void> {
                return finish(ExitedEvent{*pid_, exit.exit_code});
            },
            [&](const backend::ProcessKilled& killed) -> Result<void> {
                return finish(ExitedEvent{*pid_, killed.signal});
            },
        },
        resumed.event);
}

Result<void> Session::publish_stop(StopReason reason) {
    auto registers = debuggee_->read_registers();
    if (!registers) {
        return registers.error();
    }
    const Address pc{registers.value().rip};
    logger_.info(std::format("stopped at {:#x} ({})", pc.value(), to_string(reason)));
    events_.publish(StoppedEvent{*pid_, pc, reason, describe(pc)});
    return {};
}

Result<void> Session::finish(ExitedEvent event) {
    if (auto moved = state_.transition_to(SessionState::Exited); !moved) {
        return moved;
    }
    logger_.info("process ended");
    pid_.reset();
    pending_signal_.reset();
    breakpoints_.forget_inserted();
    events_.publish(event);
    return {};
}

}  // namespace stepwise::core
