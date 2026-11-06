/* stepwise C API, version 1.
 * - functions return a stepwise_status (STEPWISE_OK is 0); stepwise_last_error() has the message
 * - strings you get back stay valid until the next call that resumes the program
 * - not thread safe: use a session only from the thread that opened it (ptrace)
 * - set stepwise_stop_info.struct_size to sizeof(stepwise_stop_info)
 */
#ifndef STEPWISE_STEPWISE_H
#define STEPWISE_STEPWISE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STEPWISE_API_VERSION_MAJOR 1
#define STEPWISE_API_VERSION_MINOR 0

#define STEPWISE_API __attribute__((visibility("default")))

typedef struct stepwise_session stepwise_session;

typedef int32_t stepwise_status;
enum {
    STEPWISE_OK = 0,
    STEPWISE_ERR_INVALID_ARGUMENT = 1,
    STEPWISE_ERR_INVALID_STATE = 2,
    STEPWISE_ERR_NOT_FOUND = 3,
    STEPWISE_ERR_ALREADY_EXISTS = 4,
    STEPWISE_ERR_OS = 5,
    STEPWISE_ERR_INTERNAL = 6
};

typedef int32_t stepwise_state;
enum {
    STEPWISE_STATE_NOT_STARTED = 0,
    STEPWISE_STATE_STOPPED = 1,
    STEPWISE_STATE_RUNNING = 2,
    STEPWISE_STATE_EXITED = 3
};

typedef int32_t stepwise_stop_kind;
enum {
    STEPWISE_STOP_NONE = 0,
    STEPWISE_STOP_ENTRY = 1,
    STEPWISE_STOP_SINGLE_STEP = 2,
    STEPWISE_STOP_LINE_STEP = 3,
    STEPWISE_STOP_BREAKPOINT = 4,
    STEPWISE_STOP_TRAP = 5,
    STEPWISE_STOP_SIGNAL = 6,
    STEPWISE_STOP_EXITED = 7,
    STEPWISE_STOP_KILLED = 8
};

typedef struct stepwise_stop_info {
    uint32_t struct_size;
    stepwise_stop_kind kind;
    int32_t pid;
    int32_t signal;
    int32_t exit_code;
    uint32_t breakpoint_id;
    uint64_t pc;
    uint64_t hit_count;
    uint64_t fault_address;
    int32_t has_fault_address;
    uint32_t line;
    const char* function;
    const char* file;
} stepwise_stop_info;

typedef struct stepwise_frame {
    uint64_t pc;
    const char* function;
    const char* file;
    uint32_t line;
    uint32_t reserved;
} stepwise_frame;

STEPWISE_API uint32_t stepwise_api_version(void);

STEPWISE_API const char* stepwise_status_string(stepwise_status status);

STEPWISE_API stepwise_status stepwise_open(const char* program, const char* const* args,
                                           stepwise_session** out_session, char* error,
                                           size_t error_size);

STEPWISE_API stepwise_status stepwise_open_demo(stepwise_session** out_session);

STEPWISE_API void stepwise_close(stepwise_session* session);

STEPWISE_API const char* stepwise_last_error(const stepwise_session* session);

STEPWISE_API stepwise_state stepwise_get_state(const stepwise_session* session);

STEPWISE_API stepwise_status stepwise_run(stepwise_session* session);
STEPWISE_API stepwise_status stepwise_continue(stepwise_session* session);
STEPWISE_API stepwise_status stepwise_step_instruction(stepwise_session* session);
STEPWISE_API stepwise_status stepwise_step_line(stepwise_session* session);

STEPWISE_API stepwise_status stepwise_get_stop_info(stepwise_session* session, stepwise_stop_info* out);

STEPWISE_API stepwise_status stepwise_set_breakpoint(stepwise_session* session, const char* location,
                                                     const char* condition, uint32_t* out_id);
STEPWISE_API stepwise_status stepwise_delete_breakpoint(stepwise_session* session, uint32_t id);

STEPWISE_API stepwise_status stepwise_read_register(stepwise_session* session, const char* name,
                                                    uint64_t* out_value);

STEPWISE_API stepwise_status stepwise_read_memory(stepwise_session* session, uint64_t address,
                                                  void* buffer, size_t length);
STEPWISE_API stepwise_status stepwise_write_memory(stepwise_session* session, uint64_t address,
                                                   const void* data, size_t length);

STEPWISE_API stepwise_status stepwise_backtrace(stepwise_session* session, stepwise_frame* frames,
                                                size_t capacity, size_t* out_count);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
