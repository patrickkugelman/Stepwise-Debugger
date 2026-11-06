#pragma once

namespace stepwise {

template <class... Fs>
struct Overloaded : Fs... {
    using Fs::operator()...;
};

}  // namespace stepwise
