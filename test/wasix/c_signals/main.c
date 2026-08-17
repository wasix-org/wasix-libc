#include <signal.h>
#include <stdio.h>
#include <string.h>

static volatile sig_atomic_t handled;

static void siginfo_handler(int sig, siginfo_t *info, void *context)
{
    if (sig == SIGUSR1 && info != NULL && info->si_signo == SIGUSR1 &&
        context == NULL) {
        handled++;
    }
}

static int install_siginfo_handler(int flags)
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = siginfo_handler;
    action.sa_flags = SA_SIGINFO | flags;
    sigemptyset(&action.sa_mask);
    return sigaction(SIGUSR1, &action, NULL);
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;

    if (strcmp(argv[1], "siginfo") == 0) {
        if (install_siginfo_handler(0) != 0) return 3;
        if (raise(SIGUSR1) != 0) return 4;
        return handled == 1 ? 0 : 5;
    }

    if (strcmp(argv[1], "reset") == 0) {
        if (install_siginfo_handler(SA_RESETHAND) != 0) return 6;
        if (raise(SIGUSR1) != 0) return 7;
        if (handled != 1) return 8;
        raise(SIGUSR1);
        return 9;
    }

    if (strcmp(argv[1], "terminate") == 0) {
        struct sigaction action;
        memset(&action, 0, sizeof(action));
        action.sa_handler = SIG_DFL;
        sigemptyset(&action.sa_mask);
        if (sigaction(SIGTERM, &action, NULL) != 0) return 10;
        raise(SIGTERM);
        return 12;
    }

    return 11;
}
