#include <utility>
#include <vector>

#include "argument_parsing.hpp"
#include "command_groups.hpp"

namespace stepwise::commands {
namespace {

constexpr std::size_t default_read_length = 16;
constexpr std::size_t max_read_length = 4096;

class MemCommand final : public Command {
public:
    static CommandInfo info() {
        return {"mem", {"x"}, "mem read <address> [length] | mem write <address> <hexbyte>...",
                "Dump up to 4096 bytes (default 16), or overwrite bytes given in hex.",
                {{ArgumentKind::Word, {"read", "write"}}}};
    }

    static Result<CommandPtr> parse(CommandArgs args) {
        if (args.size() < 2) {
            return usage_error(info().usage);
        }
        auto address = parse_address(args[1]);
        if (!address) {
            return address.error();
        }

        auto command = std::make_unique<MemCommand>();
        command->address_ = address.value();

        if (args[0] == "read" && args.size() <= 3) {
            command->write_ = false;
            if (args.size() == 3) {
                auto length = parse_unsigned(args[2]);
                if (!length) {
                    return length.error();
                }
                if (length.value() == 0 || length.value() > max_read_length) {
                    return Error{ErrorCode::InvalidArgument,
                                 "length must be between 1 and 4096"};
                }
                command->length_ = static_cast<std::size_t>(length.value());
            }
            return command;
        }
        if (args[0] == "write" && args.size() >= 3) {
            command->write_ = true;
            for (std::string_view token : args.subspan(2)) {
                auto byte = parse_hex_byte(token);
                if (!byte) {
                    return byte.error();
                }
                command->data_.push_back(byte.value());
            }
            return command;
        }
        return usage_error(info().usage);
    }

    Result<CommandOutput> execute(CommandContext& context) override {
        if (write_) {
            return no_output(context.session.write_memory(address_, data_));
        }
        auto bytes = context.session.read_memory(address_, length_);
        if (!bytes) {
            return bytes.error();
        }
        return MemoryOutput{address_, std::move(bytes.value())};
    }

private:
    Address address_{0};
    bool write_{false};
    std::size_t length_{default_read_length};
    std::vector<std::byte> data_;
};

}  // namespace

Result<void> register_memory_commands(CommandRegistry& registry) {
    return registry.add<MemCommand>();
}

}  // namespace stepwise::commands
