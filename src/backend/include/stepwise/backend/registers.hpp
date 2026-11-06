#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace stepwise::backend {

struct Registers {
    std::uint64_t rax{}, rbx{}, rcx{}, rdx{}, rsi{}, rdi{}, rbp{}, rsp{};
    std::uint64_t r8{}, r9{}, r10{}, r11{}, r12{}, r13{}, r14{}, r15{};
    std::uint64_t rip{}, eflags{}, orig_rax{};
    std::uint64_t cs{}, ss{}, ds{}, es{}, fs{}, gs{}, fs_base{}, gs_base{};
};

struct RegisterField {
    std::string_view name;
    std::uint64_t Registers::*member;
};

inline constexpr auto register_fields = std::to_array<RegisterField>({
    {"rax", &Registers::rax},         {"rbx", &Registers::rbx},
    {"rcx", &Registers::rcx},         {"rdx", &Registers::rdx},
    {"rsi", &Registers::rsi},         {"rdi", &Registers::rdi},
    {"rbp", &Registers::rbp},         {"rsp", &Registers::rsp},
    {"r8", &Registers::r8},           {"r9", &Registers::r9},
    {"r10", &Registers::r10},         {"r11", &Registers::r11},
    {"r12", &Registers::r12},         {"r13", &Registers::r13},
    {"r14", &Registers::r14},         {"r15", &Registers::r15},
    {"rip", &Registers::rip},         {"eflags", &Registers::eflags},
    {"orig_rax", &Registers::orig_rax}, {"cs", &Registers::cs},
    {"ss", &Registers::ss},           {"ds", &Registers::ds},
    {"es", &Registers::es},           {"fs", &Registers::fs},
    {"gs", &Registers::gs},           {"fs_base", &Registers::fs_base},
    {"gs_base", &Registers::gs_base},
});

}  // namespace stepwise::backend
