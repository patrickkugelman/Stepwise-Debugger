#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <stepwise/backend/debuggee.hpp>
#include <stepwise/base/log.hpp>
#include <stepwise/base/result.hpp>
#include <stepwise/base/types.hpp>
#include <stepwise/core/breakpoint_manager.hpp>
#include <stepwise/core/event_bus.hpp>
#include <stepwise/core/location.hpp>
#include <stepwise/core/session_state.hpp>
#include <stepwise/symbols/source_cache.hpp>
#include <stepwise/symbols/symbol_provider.hpp>

namespace stepwise::core {

class Session {
public:
    Session(std::unique_ptr<backend::Debuggee> debuggee,
            std::unique_ptr<symbols::SymbolProvider> symbols, backend::LaunchSpec spec,
            Logger logger = {});

    [[nodiscard]] SessionState state() const noexcept { return state_.current(); }
    [[nodiscard]] EventBus& events() noexcept { return events_; }
    [[nodiscard]] BreakpointManager& breakpoints() noexcept { return breakpoints_; }
    [[nodiscard]] const BreakpointManager& breakpoints() const noexcept { return breakpoints_; }
    [[nodiscard]] const symbols::SymbolProvider& symbols() const noexcept { return *symbols_; }

    Result<void> run();
    Result<void> cont();
    Result<void> step_instruction();
    Result<void> step_line();
    Result<backend::Registers> registers();

    Result<std::vector<Frame>> backtrace();

    Result<std::vector<std::byte>> read_memory(Address address, std::size_t length);
    Result<void> write_memory(Address address, std::span<const std::byte> data);

    [[nodiscard]] CodeLocation describe(Address address) const;
    Result<CodeLocation> resolve_location(const LocationSpec& spec, bool skip_prologue) const;
    Result<CodeLocation> current_location();

    Result<std::span<const std::string>> source_lines(const std::string& file);
    std::optional<std::string> source_line(const std::string& file, std::uint32_t line);

private:
    enum class ResumeMode { Continue, SingleStep, StepLine };

    struct Resumed {
        backend::DebugEvent event;
        std::optional<BreakpointId> hit;
        bool reached_temporary;
    };

    enum class FrameLayout {
        Standard,
        NotYetSaved,
        RbpPushed,
    };

    Result<void> require_stopped() const;
    Result<void> resume(ResumeMode mode);
    Result<Resumed> continue_until_stop(std::optional<Signal>& deliver, std::optional<Address> until);
    Result<bool> condition_holds(BreakpointId id);
    FrameLayout innermost_layout(Address pc, std::uint64_t sp);
    std::optional<std::uint64_t> read_word(std::uint64_t address);
    void publish_crash_report(Signal signal);
    Result<Resumed> continue_with_breakpoints(std::optional<Signal>& deliver,
                                              std::optional<Address> until);
    Result<Resumed> step_to_next_line(std::optional<Signal>& deliver);
    Result<std::optional<Address>> call_return_address(const backend::Registers& before,
                                                       const backend::Registers& now);
    Result<bool> returned_to_caller(const backend::Registers& before, const backend::Registers& now);
    Result<Resumed> collect_stop(const backend::DebugEvent& event);
    Result<void> handle(const Resumed& resumed, ResumeMode mode);
    Result<void> publish_stop(StopReason reason);
    Result<void> finish(ExitedEvent event);

    std::unique_ptr<backend::Debuggee> debuggee_;
    std::unique_ptr<symbols::SymbolProvider> symbols_;
    backend::LaunchSpec spec_;
    Logger logger_;
    SessionStateMachine state_;
    EventBus events_;
    BreakpointManager breakpoints_;
    symbols::SourceCache sources_;
    std::optional<Pid> pid_;
    std::optional<Signal> pending_signal_;
};

}  // namespace stepwise::core
