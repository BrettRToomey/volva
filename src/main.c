#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <termios.h>

#define VERSION "0.0.0 (prerelease)"

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;

typedef int32_t i32;

typedef i32 b32;

#include "plugins.h"

#define MAX_COMMAND_COUNT 1024
struct Commands {
    const char *names[MAX_COMMAND_COUNT];
    const char *helpTexts[MAX_COMMAND_COUNT];
    PluginRunFunc *functions[MAX_COMMAND_COUNT];
    PluginHelperFunc *helpers[MAX_COMMAND_COUNT];
    size_t count;
};

static struct Commands commands;

#include "strings.c"
#include "json.c"

#include "config.c"
#include "flags.c"
#include "plugins.c"

// TODO(Brett): conditional build
#include "nodes.c"

int PluginsCommand(const char **vargs, size_t count) {
    const char *pluginDir = GetPluginDir();
    char stackBuffer[1024];

    if (!count) {
        return System("open", pluginDir);
    }

    if (strcmp(vargs[0], "build") == 0) {
        if (count == 1) {
            fprintf(stderr, "ERROR: The command `plugins build <plugin>` expects a plugin name (without a file extension)\n");
            return 1;
        }

        int count = snprintf(
            &stackBuffer[0], sizeof(stackBuffer),
            "%s.c",
            vargs[1]
        );

        const char *input = &stackBuffer[0];
        char *output = &stackBuffer[count += 2];

        snprintf(
            output, sizeof(stackBuffer)-count,
            "%s.so",
            vargs[1]
        );

        i32 status = SystemV(
            GetCCompiler(),
            "-Wl,-export_dynamic", "-Wl,-undefined,dynamic_lookup", "-fPIC", "-Werror",
            "-o", output,
            input,
            NULL
        );

        if (status) return status;

        return SystemV("cp", output, pluginDir, NULL);
    }

    int argOffset = 0;

    if (strcmp(vargs[0], "install") == 0 && count == 1) {
        argOffset++;

        if (count == 1) {
            fprintf(stderr, "ERROR: The command `plugins install <plugin>` expects a plugin file\n");
            return 1;
        }
    }

    return SystemV("cp", vargs[argOffset], pluginDir, NULL);
}

const char *skipWhitespace(const char *buffer) {
    while (*buffer == ' ' || *buffer == '\n')
        buffer++;

    return buffer;
}

const char *getSwiftPackageName(FILE *file, char *buffer, int bufferLen) {
    int count = fread(buffer, 1, bufferLen, file);
    buffer[count] = '\0';

    const char *nameStart = NULL;

    for (int i = 0; i < count; i += 1) {
        if (buffer[i] == 'n') {
            if (strncmp(buffer+i, "name", 4) == 0) {
                nameStart = buffer+i+4;
                break;
            }
        }
    }

    if (!nameStart) return NULL;

    nameStart = skipWhitespace(nameStart);
    if (*(nameStart++) != ':') return NULL;
    nameStart = skipWhitespace(nameStart);
    if (*(nameStart++) != '\"') return NULL;

    int nameLen = 0;
    while (nameStart[nameLen] != '\"')
        nameLen += 1;

    const char *name = strndup(nameStart, nameLen);
    return name;
}

int ResourceCommand(const char **args, size_t count) {
    DIR *dir = opendir(".build/checkouts");
    if (!dir) {
        printf("No .build/checkouts directory found\n");
        return PLUGIN_OK;
    }

    char dirBuffer[1024];
    char manifestBuffer[1024];
    struct dirent *entry;
    char *heapBuffer = malloc(1024*1024);

    while ((entry = readdir(dir)) != NULL) {
        snprintf(&dirBuffer[0], sizeof(dirBuffer), ".build/checkouts/%s/Resources/Views/*", entry->d_name);
        DIR *subdir = opendir(dirBuffer);
        if (!subdir) continue;
        closedir(subdir);

        snprintf(
            &manifestBuffer[0],
            sizeof(manifestBuffer),
            ".build/checkouts/%s/Package.swift",
            entry->d_name
        );

        FILE *swift = fopen(&manifestBuffer[0], "r");
        if (!swift) continue;

        const char *packageName = getSwiftPackageName(swift, heapBuffer, 1024*1024);
        fclose(swift);

        if (!packageName) continue;

        if (!UserConfirmationV("Would you like to install '%s'", packageName)) {
            printf("  Skipped\n");
            continue;
        }

        snprintf(heapBuffer, 1024, "Resources/Views/%s", packageName);
        SystemV("mkdir", "-p", heapBuffer, NULL);
        SystemV("cp", "-r", &dirBuffer[0], heapBuffer, NULL);
        SystemV("chmod", "-R", "0755", heapBuffer, NULL);
    }

    return 0;
}

void InitBuiltinCommands() {
    RegisterCommand("plugins", "Commands for creating and managing plugins", PluginsCommand);
    RegisterCommand("resource", "Copy over Vapor Resources and Views", ResourceCommand);
    RegisterNodesCommands();
}

static struct termios oldt;
static b32 changedTerminalState;

void disableBufferedInput() {
    struct termios t;
    tcgetattr( STDIN_FILENO, &oldt);
    t = oldt;

    t.c_lflag &= ~(ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    changedTerminalState = true;
}

void restoreTerminalState() {
    if (!changedTerminalState) return;
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

int main(i32 argc, const char **argv) {
    const char *programName = argv[0];

    InitBuiltinFlags();
    ParseBuiltinFlags(&argc, &argv);

    InitBuiltinCommands();

    LoadPlugins();
    ParseFlags(&argc, &argv);

    if (FlagVersion) {
        printf("%s\n", VERSION);
        return 0;
    }

    if (argc < 1) {
        PrintUsage(programName);
        exit(!FlagHelp);
    }

    i32 commandIndex = -1;
    for (size_t i = 0; i < commands.count; i += 1) {
        if (strcmp(commands.names[i], CommandName) == 0) {
            commandIndex = i;
            break;
        }
    }

    if (commandIndex == -1) {
        printf("Unknown command %s\n", CommandName);
        return 1;
    }

    if (FlagHelp) {
        printf("%s: %s\n\n", commands.names[commandIndex], commands.helpTexts[commandIndex]);
        PrintFlags(commandIndex);

        if (commands.helpers[commandIndex]) {
            printf("\n");
            return commands.helpers[commandIndex](argv+1, argc-1);
        }
        return 0;
    }

    if (commands.functions[commandIndex]) {
        if (!ConfigBufferedInput)
            disableBufferedInput();
        b32 status = commands.functions[commandIndex](argv+1, argc-1);
        restoreTerminalState();
        return status;
    }

    return 0;
}
