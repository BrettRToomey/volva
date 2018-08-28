#include "net.c"

static const char *envAppName;
static const char *envName = "staging";
static bool flagAllEnvironments;

static CommandId envId;

static const struct CLIFlag appFlag = {
    CLIFlagKind_String,
    .name = "app",
    .argumentName = "name",
    .ptr.s = &envAppName,
    .help = "The target application"
};

static const struct CLIFlag envFlag = {
    CLIFlagKind_String,
    .name = "env",
    .alias = "e",
    .argumentName = "name",
    .ptr.s = &envName,
    .help = "The target application"
};

static const struct CLIFlag allFlag = {
    CLIFlagKind_Bool,
    .name = "all",
    .alias = "a",
    .ptr.b = &flagAllEnvironments,
    .help = "Set value(s) on all environments"
};

static void dumpConfig(const char *app, const char *env, struct KeyValue *configs, u32 count) {
    printf("app: %s\n", app);
    printf("env: %s\n", env);
    printf("\n");

    i32 valueWidth = 48;
    i32 termWidth;
    GetTermDim(&termWidth, NULL);

    if (termWidth > 80) {
        // TODO(Brett): dyamic width dump
        // valueWidth += (termWidth - 80);
    }

    if (count) {
        printf("┌───────────────────────────┬──────────────────────────────────────────────────┐\n");

        for (size_t i = 0; i < count; i += 1) {
            struct KeyValue config = configs[i];
            printf("│ %-25.25s │ %-*.*s │\n", config.key, valueWidth, valueWidth, config.value);
            if (strlen(config.value) > valueWidth) {
                printf("│                           │ %-*.*s │\n", valueWidth, valueWidth, config.value+48);
            }

            if (i +1 != count)
                printf("├───────────────────────────┼──────────────────────────────────────────────────┤\n");
        }

        printf("└───────────────────────────┴──────────────────────────────────────────────────┘\n");
    } else {
        printf("Environment does not have any configurations set\n");
    }
}

static i32 getEnv() {
    enum NetError err;

    u32 count;
    struct KeyValue *configs;

    err = GetVaporCloudConfig(envAppName, envName, &configs, &count);
    if (err != NetError_None) {
        return err;
    }

    dumpConfig(envAppName, envName, configs, count);
    
    return PLUGIN_OK;
}

static i32 setEnv(const char **args, size_t count) {
    if (!count) {
        printf("ERROR: expected a list of key-value pairs\n");
        return PLUGIN_SHOW_HELP;
    }

    struct KeyValue *configs = malloc(count * sizeof(struct KeyValue));
    u32 configCount = 0;

    for (size_t i = 0; i < count; i += 1) {
        const char *arg = args[i];
        const char *eqlIndex = index(arg, '=') ?: index(arg, ':');

        const char *key, *value;
        key = value = NULL;

        if (eqlIndex) {
            value = eqlIndex+1;

            int keyLen = eqlIndex-arg;

            char *buffer = malloc(keyLen+1);
            memcpy(buffer, arg, keyLen);
            buffer[keyLen] = '\0';

            key = buffer;
        } else {
            printf("TODO. Skipping arg\n");
            continue;
        }

        struct KeyValue *config = &configs[configCount++];
        config->key =   key;
        config->value = value;
    }

    dumpConfig(envAppName, envName, configs, configCount);

    if (!UserConfirmation("Is the above correct?")) {
        printf("Aborted\n");
        return PLUGIN_OK;
    }

    SetVaporCloudConfig(envAppName, envName, &configs, &configCount);

    dumpConfig(envAppName, envName, configs, configCount);

    return PLUGIN_OK;
}

i32 envCommand(const char **args, size_t count) {
    if (!envAppName) {
        fprintf(stderr, "ERROR: Please provide an app name with -%s <name>\n", appFlag.name);
        return PLUGIN_SHOW_HELP;
    }

    if (!count) {
        return getEnv();
    }

    if (strcmp(args[0], "set") == 0) {
        args++;
        count--;
    }

    return setEnv(args, count);
}

void RegisterNodesCommands() {
    envId = RegisterCommand("env", "Commands for creating and managing VCloud env. variables", envCommand);

    RegisterFlag(envId, appFlag);
    RegisterFlag(envId, allFlag);
    RegisterFlag(envId, envFlag);
}
