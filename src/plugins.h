#ifndef VOL_PLUGINS_H
#define VOL_PLUGINS_H

#include <stddef.h>
#include<stdbool.h>

extern bool FlagHelp;
extern bool FlagVerbose;
extern bool FlagVersion;
extern bool FlagYes;

extern const char *CommandName;

enum CLIFlagKind {
    CLIFlagKind_Bool,
    CLIFlagKind_String,
    CLIFlagKind_Enum,
};

struct CLIFlag {
    enum CLIFlagKind kind;
    const char *name;
    const char *alias;
    const char **options;
    const char *argumentName;
    const char *help;
    int nOptions;
    int commandId;
    union {
        int *i;
        bool *b;
        const char **s;
    } ptr;
};

#define PLUGIN_OK 0
#define PLUGIN_SHOW_HELP 1

#define VOLV_API extern __attribute__((weak))
typedef int PluginInitFunc();
typedef int PluginRunFunc(const char **args, size_t count);
typedef int PluginHelperFunc(const char **args, size_t count);

typedef int CommandId;

VOLV_API void VolLog(const char *msg);
VOLV_API CommandId RegisterCommand(const char *name, const char *helpText, PluginRunFunc *func);
VOLV_API void RegisterHelper(CommandId commandId, PluginHelperFunc *helper);

VOLV_API void RegisterFlag(CommandId id, struct CLIFlag flag);
VOLV_API void PrintFlags(CommandId commandId);

VOLV_API const char *GetCCompiler();
VOLV_API b32 UserConfirmation(const char *message);
#endif
