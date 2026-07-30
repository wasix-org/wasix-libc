#ifndef SIGNAL_H
#define SIGNAL_H

#include "../../include/signal.h"

hidden int __sigaction(int, const struct sigaction *, struct sigaction *);

hidden void __block_all_sigs(void *);
hidden void __block_app_sigs(void *);
hidden void __restore_sigs(void *);

hidden void __get_handler_set(sigset_t *);

/* Dispatches signals that are pending on the calling thread and no longer
 * blocked. WASIX delivers signals only at syscall boundaries, so whoever
 * unblocks a signal has to flush it: nothing will interrupt us to do it. */
hidden void __sig_deliver_pending(void);

/* (Re-)registers the guest signal callback with the host. Needed after fork:
 * the host's registration is per instance and does not survive it. */
hidden void __sig_register_callback(void);

#endif
