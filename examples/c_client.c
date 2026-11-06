#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stepwise/stepwise.h>

#define MAX_STOPS 100000u

static void die(stepwise_session *session, const char *what, stepwise_status status) {
    fprintf(stderr, "error: %s: %s (%s)\n", what, stepwise_last_error(session),
            stepwise_status_string(status));
    stepwise_close(session);
    exit(1);
}

#define CHECK(session, call)                      \
    do {                                          \
        stepwise_status status_ = (call);         \
        if (status_ != STEPWISE_OK) {             \
            die((session), #call, status_);       \
        }                                         \
    } while (0)

static void print_location(const stepwise_stop_info *info) {
    if (info->function != NULL) {
        printf("%s", info->function);
    } else {
        printf("??");
    }
    if (info->file != NULL) {
        printf(" (%s:%u)", info->file, info->line);
    }
}

static void print_backtrace(stepwise_session *session) {
    size_t count = 0;
    CHECK(session, stepwise_backtrace(session, NULL, 0, &count));
    if (count == 0) {
        return;
    }
    stepwise_frame *frames = calloc(count, sizeof *frames);
    if (frames == NULL) {
        die(session, "calloc", STEPWISE_ERR_INTERNAL);
    }
    size_t filled = count;
    stepwise_status status = stepwise_backtrace(session, frames, count, &filled);
    if (status != STEPWISE_OK) {
        free(frames);
        die(session, "stepwise_backtrace", status);
    }
    for (size_t i = 0; i < filled; ++i) {
        printf("    #%zu 0x%016" PRIx64 " %s", i, frames[i].pc,
               frames[i].function != NULL ? frames[i].function : "??");
        if (frames[i].file != NULL) {
            printf(" (%s:%u)", frames[i].file, frames[i].line);
        }
        printf("\n");
    }
    free(frames);
}

static void print_registers_and_code(stepwise_session *session, const stepwise_stop_info *info) {
    uint64_t rax = 0;
    uint64_t rdi = 0;
    CHECK(session, stepwise_read_register(session, "rax", &rax));
    CHECK(session, stepwise_read_register(session, "rdi", &rdi));
    printf("    rax=%" PRIu64 " rdi=%" PRIu64 "\n", rax, rdi);

    unsigned char code[4] = {0};
    if (stepwise_read_memory(session, info->pc, code, sizeof code) == STEPWISE_OK) {
        printf("    code at pc: %02x %02x %02x %02x\n", code[0], code[1], code[2], code[3]);
    }
}

static stepwise_stop_info fetch_stop_info(stepwise_session *session) {
    stepwise_stop_info info;
    memset(&info, 0, sizeof info);
    info.struct_size = sizeof info;
    CHECK(session, stepwise_get_stop_info(session, &info));
    return info;
}

static int report_stop(stepwise_session *session, const stepwise_stop_info *info, unsigned *breakpoint_stops) {
    switch (info->kind) {
    case STEPWISE_STOP_EXITED:
        printf("program exited with code %d\n", info->exit_code);
        return 0;
    case STEPWISE_STOP_KILLED:
        printf("program killed by signal %d\n", info->signal);
        return 0;
    case STEPWISE_STOP_SIGNAL:
        printf("signal %d at ", info->signal);
        print_location(info);
        if (info->has_fault_address) {
            printf(", fault address 0x%" PRIx64, info->fault_address);
        }
        printf("\n");
        print_backtrace(session);
        return 1;
    case STEPWISE_STOP_BREAKPOINT:
        ++*breakpoint_stops;
        printf("stop %u: breakpoint %u, hit %" PRIu64 " at ", *breakpoint_stops, info->breakpoint_id,
               info->hit_count);
        print_location(info);
        printf("\n");
        print_registers_and_code(session, info);
        print_backtrace(session);
        return 1;
    case STEPWISE_STOP_LINE_STEP:
        printf("    stepped to ");
        print_location(info);
        printf("\n");
        return 1;
    default:
        printf("stopped (kind %d) at 0x%016" PRIx64 "\n", (int)info->kind, info->pc);
        return 1;
    }
}

int main(int argc, char **argv) {
    if ((stepwise_api_version() >> 16) != STEPWISE_API_VERSION_MAJOR) {
        fprintf(stderr, "error: this client was built for API %d, the library is %u\n",
                STEPWISE_API_VERSION_MAJOR, stepwise_api_version() >> 16);
        return 1;
    }

    stepwise_session *session = NULL;
    const char *location = NULL;
    const char *condition = NULL;
    if (argc >= 2 && strcmp(argv[1], "--demo") == 0) {
        stepwise_status status = stepwise_open_demo(&session);
        if (status != STEPWISE_OK) {
            fprintf(stderr, "error: cannot open the demo: %s\n", stepwise_status_string(status));
            return 1;
        }
        location = argc >= 3 ? argv[2] : "0x401004";
        condition = argc >= 4 ? argv[3] : NULL;
    } else if (argc >= 3) {
        char message[256];
        stepwise_status status = stepwise_open(argv[1], NULL, &session, message, sizeof message);
        if (status != STEPWISE_OK) {
            fprintf(stderr, "error: cannot open '%s': %s (%s)\n", argv[1], message,
                    stepwise_status_string(status));
            return 1;
        }
        location = argv[2];
        condition = argc >= 4 ? argv[3] : NULL;
    } else {
        fprintf(stderr, "usage: %s --demo [location [condition]]\n       %s <program> <location> [condition]\n",
                argv[0], argv[0]);
        return 2;
    }

    uint32_t breakpoint = 0;
    CHECK(session, stepwise_set_breakpoint(session, location, condition, &breakpoint));
    printf("breakpoint %u at %s%s%s\n", breakpoint, location, condition != NULL ? " if " : "",
           condition != NULL ? condition : "");

    CHECK(session, stepwise_run(session));

    unsigned stops = 0;
    int stepped_once = 0;
    for (unsigned guard = 0; guard < MAX_STOPS; ++guard) {
        CHECK(session, stepwise_continue(session));
        stepwise_stop_info info = fetch_stop_info(session);
        if (!report_stop(session, &info, &stops)) {
            break;
        }

        if (info.kind == STEPWISE_STOP_BREAKPOINT && !stepped_once) {
            stepped_once = 1;
            if (stepwise_step_line(session) != STEPWISE_OK) {
                printf("    (cannot step by line here: %s)\n", stepwise_last_error(session));
                continue;
            }
            stepwise_stop_info next = fetch_stop_info(session);
            if (!report_stop(session, &next, &stops)) {
                break;
            }
        }
    }

    printf("%u breakpoint stop(s); session state %d\n", stops, (int)stepwise_get_state(session));
    stepwise_close(session);
    return 0;
}
