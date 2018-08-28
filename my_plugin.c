#include <volva/plugins.h>

#include <stdio.h>

bool FlagTest;

CommandId exampleId;

int exampleCommand(const char **args, size_t argc) {
    printf("Hello, world!\n");
    return PLUGIN_OK;
}

int PluginInit() {
    exampleId = RegisterCommand("example", "An example command", exampleCommand);

    struct CLIFlag flag = {
        CLIFlagKind_Bool,
        .name = "test",
        .ptr.b = &FlagTest,
        .help = "A test flag"
    };
    RegisterFlag(exampleId, flag);

    return PLUGIN_OK;
}
