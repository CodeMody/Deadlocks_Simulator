/**
 * @file state.h
 * @brief Öffentliches Interface für die Verwaltung des Systemzustands.
 *
 * Der Systemzustand (@c SystemState) modelliert das Ressourcensystem nach
 * dem Bankier-Modell mit Allokations- und Bedarfsmatrizen. Dieses Modul
 * stellt Funktionen zum Erstellen, Befüllen, Prüfen und Freigeben des
 * Zustands bereit.
 */

#ifndef STATE_H
#define STATE_H

#include "../core/types.h"

/**
 * @brief Erstellt einen neuen Systemzustand mit gegebenen Ressourcenklassen.
 *
 * Allokiert und initialisiert die Vektoren für Gesamtinstanzen (@c instances)
 * und verfügbare Instanzen (@c available). Die Prozess-Matrizen werden erst
 * durch @c state_allocate_process_matrices() initialisiert.
 *
 * @param n_classes  Anzahl der Ressourcenklassen.
 * @param instances  Array der Gesamtinstanzen je Klasse (Länge @p n_classes).
 * @return           Zeiger auf den neu erstellten @c SystemState.
 */
SystemState *state_create(uint32_t n_classes,
                          const uint32_t *instances);

/**
 * @brief Gibt den gesamten Systemzustand und seinen Speicher frei.
 *
 * Gibt die Instanz- und Verfügbarkeitsvektoren sowie die Allokations-
 * und Bedarfsmatrizen frei. Ein NULL-Zeiger wird sicher ignoriert.
 *
 * @param st  Zeiger auf den freizugebenden @c SystemState.
 */
void state_destroy(SystemState *st);

/**
 * @brief Allokiert die Prozess-Matrizen für einen gegebenen Systemzustand.
 *
 * Muss nach @c state_create() aufgerufen werden, sobald die Anzahl der
 * Prozesse bekannt ist. Legt die Allokationsmatrix @c C und die
 * Bedarfsmatrix @c R mit Nullen initialisiert an.
 *
 * @param st       Zeiger auf den Systemzustand.
 * @param n_procs  Anzahl der simulierten Prozesse.
 */
void state_allocate_process_matrices(SystemState *st,
                                     uint32_t n_procs);

/**
 * @brief Setzt einen Eintrag in der Allokationsmatrix.
 *
 * Ungültige Indizes (pid oder class_id außerhalb des gültigen Bereichs)
 * werden ohne Fehlermeldung ignoriert.
 *
 * @param st        Zeiger auf den Systemzustand.
 * @param pid       Prozess-ID (Zeilenindex in @c allocation).
 * @param class_id  Ressourcenklasse (Spaltenindex in @c allocation).
 * @param amount    Anzahl der als gehalten einzutragenden Instanzen.
 */
void state_set_allocation(SystemState *st,
                          uint32_t pid,
                          uint32_t class_id,
                          uint32_t amount);

/**
 * @brief Setzt einen Eintrag in der Bedarfsmatrix.
 *
 * Ungültige Indizes werden ohne Fehlermeldung ignoriert.
 *
 * @param st        Zeiger auf den Systemzustand.
 * @param pid       Prozess-ID (Zeilenindex in @c request).
 * @param class_id  Ressourcenklasse (Spaltenindex in @c request).
 * @param amount    Anzahl der noch benötigten Instanzen.
 */
void state_set_request(SystemState *st,
                       uint32_t pid,
                       uint32_t class_id,
                       uint32_t amount);

/**
 * @brief Prüft mit dem Bankier-Algorithmus, ob der aktuelle Zustand sicher ist.
 *
 * Simuliert eine sequenzielle Abarbeitung aller Prozesse: Ein Prozess gilt
 * als abgeschlossen, wenn sein Bedarf (@c request) vollständig durch den
 * aktuellen Arbeitsvektor (@c work) gedeckt werden kann. Nach Abschluss
 * eines Prozesses werden seine Ressourcen zum Arbeitsvektor addiert.
 * Können alle Prozesse abgeschlossen werden, ist der Zustand sicher.
 *
 * @param st  Zeiger auf den zu prüfenden Systemzustand (const).
 * @return    @c true, wenn ein sicherer Ablaufplan existiert; sonst @c false.
 */
bool state_is_safe(const SystemState *st);

/**
 * @brief Gibt eine formatierte Übersicht des Systemzustands auf @c stdout aus.
 *
 * Zeigt Gesamtressourcen, verfügbare Ressourcen, die Allokationsmatrix C
 * und die Bedarfsmatrix R zeilenweise an.
 *
 * @param st  Zeiger auf den auszugebenden Systemzustand (const).
 */
void state_print(const SystemState *st);

#endif /* STATE_H */
