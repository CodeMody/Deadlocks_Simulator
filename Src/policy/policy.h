/*=====================================================================
*  policy.h
 *  ---------------------------------------------------------------
 *  Öffentliches Interface für alle Deadlock-Behandlungsstrategien.
 *====================================================================*/

#ifndef POLICY_H
#define POLICY_H

#include "../core/types.h"

/* -----------------------------------------------------------------
   Konkretes Policy-Objekt vollständige Definition
   ----------------------------------------------------------------- */
typedef struct Policy {
    const char        *name;        /* menschenlesbarer Name der Policy     */
    policy_request_f   on_request;  /* wird bei jedem Request-Event aufgerufen */
    policy_tick_f      on_tick;     /* optionale periodische Wartungsfunktion */
    policy_cleanup_f   cleanup;     /* gibt interne Daten der Policy frei     */
    void              *private;     /* policy-spezifischer Kontext (opaque)   */
} Policy;

/* -----------------------------------------------------------------
   Factory-Funktionen jede Policy erhält die Größe des Systems,
   auf dem sie arbeiten soll.
   ----------------------------------------------------------------- */
Policy *detect_policy_create(uint32_t n_procs,
                             uint32_t n_classes);   /* Graph-Erkennung */

Policy *banker_policy_create(uint32_t n_procs,
                             uint32_t n_res);       /* Banker (mehrere Ressourcen) */

Policy *holdwait_policy_create(uint32_t n_procs,
                               uint32_t n_res);     /* Hold-and-Wait-Eliminierung */

#endif /* POLICY_H */
