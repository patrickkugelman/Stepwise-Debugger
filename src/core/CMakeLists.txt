stepwise_add_layer(core
    SOURCES breakpoint_manager.cpp condition.cpp event_bus.cpp events.cpp session.cpp session_state.cpp
    DEPS stepwise::base stepwise::backend stepwise::symbols)
