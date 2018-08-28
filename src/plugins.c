static char *pluginDirectory;
static b32 reloadTerminalSize = true;

void GetTermDim(int *width, int *height) {
    struct winsize size;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);

    if (width)
        *width =  size.ws_col;
    if (height)
        *height = size.ws_row;
}

const char *GetPluginDir() {
    if (!pluginDirectory) {
        pluginDirectory = malloc(1024);

        const char *home = getenv("HOME");
        if (!home) {
            printf("Unable to locate home folder\n");
            return NULL;
        }

        snprintf(pluginDirectory, 1024, "%s/.volva/plugins/", home);
    }

    return pluginDirectory;
}

const char *GetCCompiler() {
    b32 hasClang = system("which -s clang") == 0;
    return hasClang ? "clang" : "gcc";
}

b32 UserConfirmation(const char *message) {
    printf("%s\ny/[n] > ", message);

    if (FlagYes) {
        printf("y\n");
        return true;
    }

    b32 confirmation;

    int ch = getchar();
    switch (ch) {
        case 'y':
        case 'Y':
        case '1':
            confirmation = true;
            break;

        case '\n':
            // NOTE: this functionality will be configurable in the future
            confirmation = false; 
            break;

        default: 
            confirmation = false;
    }

    if (ConfigBufferedInput) {
        if (ch != '\n') {
            while (getchar() != '\n'){}
        }
    } else {
        if (ch != '\n') {
            printf("\n");
        }
    }

    return confirmation;
}

b32 UserConfirmationV(const char *fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(&buffer[0], sizeof(buffer), fmt, args);
    va_end(args);

    return UserConfirmation(&buffer[0]);
}

CommandId RegisterCommand(const char *name, const char *helpText, PluginRunFunc *func) {
    size_t index = commands.count;
    if (index >= MAX_COMMAND_COUNT) {
        printf("WARNING: Unable to register command '%s'\n", name);
        return -1;
    }

    commands.names[index] = strdup(name);
    commands.helpTexts[index] = strdup(helpText);
    commands.functions[index] = func;
    commands.count++;

    if (FlagVerbose)
        printf("Registered command: '%s' (id: %zu)\n", name, index);

    return (CommandId)index;
}

void RegisterHelper(CommandId commandId, PluginHelperFunc *helper) {
    if (commandId <= GlobalCommandId)
        return;

    commands.helpers[commandId] = helper;
}

i32 System(const char *command, const char *arg) {
    pid_t pid;
    pid_t ret;
    i32 status = 0;

    const char *args[3];
    args[0] = command;
    args[1] = arg;
    args[2] = NULL;

    pid = fork();
    if (pid != 0) {
        while ((ret = waitpid(pid, &status, 0)) == -1) {
            if (errno != EINTR) {
                printf("errno!\n");
                break;
            }
        }

        if (ret != -1 && (!WIFEXITED(status) || !(WEXITSTATUS(status)))) {
            // TODO(Brett):
        }
    } else {
        if (execvp(command, (char *const *)args) == -1) {
            exit(127);
        }
    }

    return WEXITSTATUS(status);
}

i32 SystemV(const char *command, ...) {
    pid_t pid;
    pid_t ret;
    i32 status = 0;

    const char *args[0x100];
    int argc;

    args[0] = command;

    va_list vargs;
    va_start(vargs, command);

    for (argc = 1; argc < 0x100; argc += 1) {
        const char *arg = va_arg(vargs, char *);
        if (!arg) break;
        args[argc] = arg;
    }

    va_end(vargs);

    args[argc] = NULL;

    pid = fork();
    if (pid != 0) {
        while ((ret = waitpid(pid, &status, 0)) == -1) {
            if (errno != EINTR) {
                break;
            }
        }

        if (ret != -1 && (!WIFEXITED(status) || !(WEXITSTATUS(status)))) {
            // TODO(Brett):
        }
    } else {
        if (execvp(command, (char *const *)args) == -1) {
            exit(127);
        }
    }

    return WEXITSTATUS(status);
}

i32 LoadPlugins() {
    DIR *dir;
    struct dirent *entry;
    const char *pluginDir = GetPluginDir();

    if (FlagVerbose)
        printf("Path: %s\n", pluginDir);

    if ((dir = opendir(pluginDir)) != NULL) {
        char workingDir[1024];
        getcwd(&workingDir[0], sizeof(workingDir));

        chdir(pluginDir);

        while ((entry = readdir(dir)) != NULL) {
            const char *name = entry->d_name;
            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
                continue;

            if (FlagVerbose)
                printf("  Loading: %s\n", name);

            void *plugin = dlopen(name, RTLD_NOW);
            if (!plugin) {
                continue;
            }

            PluginInitFunc *init = dlsym(plugin, "PluginInit");
            if (init) {
                b32 err = init();
                if (err) {
                    printf("Error trying to init plugin\n");
                }
            }
        }

        chdir(&workingDir[0]);
        closedir(dir);
    }

    return 0;
}
