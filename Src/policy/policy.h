/*=====================================================================
 *  policy.h
 *  ---------------------------------------------------------------
 *  Public interface for all dead‑lock handling policies.
 *====================================================================*/

#ifndef POLICY_H
#define POLICY_H

#include "../core/types.h"

/* -----------------------------------------------------------------
   The concrete Policy object – full definition (only here!).
   ----------------------------------------------------------------- */
typedef struct Policy {
    const char        *name;        /**< human‑readable name                */
    policy_request_f   on_request;  /**< called for every request event     */
    policy_tick_f      on_tick;     /**< optional periodic housekeeping      */
    policy_cleanup_f   cleanup;     /**< free internal data                  */
    void              *private;     /**< policy‑specific context (opaque)    */
} Policy;

/* -----------------------------------------------------------------
   Factory functions – each policy now receives the size of the
   system it will work on.
   ----------------------------------------------------------------- */
Policy *detect_policy_create(uint32_t n_procs,
                             uint32_t n_classes);   /* graph detection */

Policy *banker_policy_create(uint32_t n_procs,
                             uint32_t n_res);       /* Banker (multi)   */

Policy *holdwait_policy_create(uint32_t n_procs,
                               uint32_t n_res);     /* Hold‑and‑Wait    */

#endif /* POLICY_H */