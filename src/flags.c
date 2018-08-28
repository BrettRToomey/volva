bool FlagHelp;
bool FlagVerbose;
bool FlagVersion;
bool FlagYes;

const char *CommandName;
struct CLIFlag flags[256];
u32 flagCount;

#define GlobalCommandId (-1)

void RegisterFlag(CommandId id, struct CLIFlag flag) {
    if (flagCount >= 256) return;
    flag.commandId = id;
    flags[flagCount++] = flag;

    if (FlagVerbose) {
        printf("Registered flag: '%s'\n", flag.name);
    }
}

void RegisterGlobalFlag(struct CLIFlag flag) {
    RegisterFlag(GlobalCommandId, flag);
}

void InitBuiltinFlags() {
    RegisterGlobalFlag((struct CLIFlag){ CLIFlagKind_Bool, "help", "h", .ptr.b = &FlagHelp, .help = "Prints help information"});
    RegisterGlobalFlag((struct CLIFlag){ CLIFlagKind_Bool, "version", .ptr.b = &FlagVersion, .help = "Prints version information"});

    RegisterGlobalFlag((struct CLIFlag){ CLIFlagKind_Bool, "verbose", "v", .ptr.b = &FlagVerbose, .help = "Enable verbose output"});
    RegisterGlobalFlag((struct CLIFlag){ CLIFlagKind_Bool, "yes", "y", .ptr.b = &FlagYes, .help = "Automatic 'yes' to all prompts"});
}

struct CLIFlag *FlagForName(const char *name) {
    for (size_t i = 0; i < flagCount; i += 1) {
        if (strcmp(flags[i].name, name) == 0)
            return &flags[i];
        else if (flags[i].alias && strcmp(flags[i].alias, name) == 0)
            return &flags[i];
    }

    return NULL;
}

void parseFlags(int *pargc, const char ***pargv, b32 internalPass) {
    int argc = *pargc;
    const char **argv = *pargv;

    char buffer[0x100];

    size_t i;
    for (i = 1; i < argc; i += 1) {
        const char *arg = argv[i];
        const char *name = arg;
        if (*name == '-') {
            name++;
            if (*name == '-')
                name++;

            b32 inverse = false;
            if (strncmp("no-", name, 3) == 0) {
                inverse = true;
                name += 3;
            }

            const char *eqlIndex = index(name, '=');
            if (eqlIndex) {
                int count = eqlIndex - name;
                count = count < sizeof(buffer)-1 ? count : sizeof(buffer)-1;
                memcpy(&buffer[0], name, count);
                buffer[count] = '\0';
                name = &buffer[0];
            }

            struct CLIFlag *flag = FlagForName(name);
            if (!flag || (inverse && flag->kind != CLIFlagKind_Bool)) {
                if (!internalPass)
                    printf("Unknown flag %s\n", arg);
                continue;
            }

            switch (flag->kind) {
                case CLIFlagKind_Bool:
                    *flag->ptr.b = inverse ? false : true;
                    break;

                case CLIFlagKind_String:
                    if (eqlIndex) {
                        *flag->ptr.s = eqlIndex+1;
                    } else if (i + 1 < argc) {
                        i++;
                        *flag->ptr.s = argv[i];
                    } else {
                        printf("No value argument after -%s\n", arg);
                    }
                    break;

                case CLIFlagKind_Enum: {
                    const char *option;
                    if (i + 1 < argc) {
                        i++;
                        option = argv[i];
                    } else {
                        printf("No value argument after -%s\n", arg);
                        break;
                    }

                    b32 found = false;
                    for (size_t k = 0; k < flag->nOptions; k += 1) {
                        if (strcmp(flag->options[k], option) == 0) {
                            *flag->ptr.i = k;
                            found = true;
                            break;
                        }
                    }

                    if (!found) {
                        printf("Invalid value %s for %s. Expected (", option, arg);
                        for (size_t k = 0; i < flag->nOptions; k += 1) {
                            printf("%s", flag->options[k]);
                            printf("|");
                        }
                        printf(")\n");
                        break;
                    }
                } break;

            }
        } else {
            break;
        }
    }

    if (!internalPass) {
        *pargc = argc - i;
        *pargv = argv + i;

        if (argc - i >= 1) {
            CommandName = argv[i];
        }
    }
}

void ParseFlags(int *pargc, const char ***pargv) {
    parseFlags(pargc, pargv, false);
}

void ParseBuiltinFlags(int *pargc, const char ***pargv) {
    parseFlags(pargc, pargv, true);
}

void PrintFlags(CommandId commandId) {
    printf("Flags:\n");

    for (size_t i = 0; i < flagCount; i += 1) {
        char invokation[40];
        int k = 0;

        struct CLIFlag flag = flags[i];
        if (flag.commandId != commandId)
            continue;

        if (flag.alias) k = snprintf(invokation, sizeof(invokation), "-%s ", flag.alias);
        k += snprintf(invokation+k, sizeof(invokation) - k, "-%s", flag.name);

        switch (flag.kind) {
            case CLIFlagKind_String:
                k += snprintf(invokation+k, sizeof(invokation)-k, " <%s>", flag.argumentName);
                break;

            case CLIFlagKind_Enum:
                k += snprintf(invokation+k, sizeof(invokation) - k, " <");
                k += snprintf(invokation+k, sizeof(invokation)-k, "%s", flag.options[0]);

                for (size_t i = 1; i < flag.nOptions; i += 1) {
                    k += snprintf(invokation+k, sizeof(invokation)-k, "|%s", flag.options[i]);
                }
                k += snprintf(invokation+k, sizeof(invokation)-k, ">");
                break;

            case CLIFlagKind_Bool:
                break;
        }

        printf("  %-20s %s\n", invokation, flag.help ? flag.help : "");
    }
}

void PrintUsage(const char *programName) {
    printf("Usage: %s [flags] <command> <args>\n\n", programName);

    PrintFlags(GlobalCommandId);

    if (commands.count) {
        printf("\nCommands:\n");

        for (size_t i = 0; i < commands.count; i += 1) {
            printf("  %-20s %s\n", commands.names[i], commands.helpTexts[i]);
        }
    }
}
