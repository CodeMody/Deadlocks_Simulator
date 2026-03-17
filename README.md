# Deadlock-Simulator

## Projektbeschreibung

Der Deadlock-Simulator ist eine interaktive Anwendung zur Visualisierung und zum Vergleich klassischer Deadlock-Behandlungsstrategien aus der Betriebssystemtheorie. Ziel des Projekts ist es, die theoretischen Konzepte aus der Vorlesung „Betriebssysteme" erfahrbar zu machen: Man kann live beobachten, wie drei verschiedene Algorithmen mit konkurrierenden Prozessen umgehen, die gemeinsam Ressourcen anfordern und freigeben.

Das Programm simuliert ein Ressourcensystem mit **3 Prozessen** und **2 Ressourcenklassen** (z. B. Drucker und Festplatten). Alle Prozesse durchlaufen zyklisch einen Lebenszyklus: Sie fordern Ressourcen an (IDLE → RUNNING), arbeiten eine Zeit lang, geben die Ressourcen frei (RUNNING → DONE) und starten dann neu. Die Simulation läuft in logischen **Ticks** ab und ist vollständig deterministisch konfigurierbar.

### Die drei Deadlock-Strategien

| Policy | Ansatz | Vorteil | Nachteil |
|---|---|---|---|
| **Bankier-Algorithmus** | Führt vor jeder Vergabe eine Sicherheitsprüfung durch; genehmigt nur bei nachgewiesener Sicherheit | Präzise, keine unnötigen Ablehnungen | Rechenaufwand O(n² · m) pro Anfrage |
| **Graph-Erkennung** | Baut den Wait-For-Graphen auf und prüft mit DFS auf Zyklen | Erkennt zirkuläre Abhängigkeiten direkt | Aufbau und DFS ebenfalls O(n²) |
| **Hold-and-Wait-Eliminierung** | Lehnt Anfragen von Prozessen ab, die bereits Ressourcen halten | Sehr einfach, kein Graphaufbau nötig | Kann zu Starvation führen |

Der **Verschiebungs-Zähler** (POSTPONED) zeigt an, wie viele Anfragen bisher von der aktiven Policy abgelehnt und in die Zukunft verschoben wurden — ein direktes Maß für die „Strenge" der jeweiligen Strategie.

---

## Bedienungsanleitung

### Voraussetzungen

Das Programm benötigt folgende Abhängigkeiten:

- **C-Compiler**: GCC ≥ 9 oder Clang ≥ 10 mit C11-Unterstützung
- **CMake**: Version ≥ 3.15
- **GTK 3**: Entwicklungspaket `gtk+-3.0`
- **pkg-config**: Zum automatischen Auffinden der GTK-Bibliotheken

#### Abhängigkeiten installieren (Ubuntu / Debian)

```bash
sudo apt update
sudo apt install build-essential cmake libgtk-3-dev pkg-config
sudo apt upgrade
```

#### Abhängigkeiten installieren (Fedora / RHEL)

```bash
sudo dnf install gcc cmake gtk3-devel pkg-config
```

#### Abhängigkeiten installieren (macOS mit Homebrew)

```bash
brew install cmake gtk+3 pkg-config
```
#### Abhängigkeiten installieren (Windows)
https://www.msys2.org/
---

### Kompilierung

1. **Repository-Verzeichnis öffnen:**

```bash
cd Deadlocks_Simulator
```

2. **Build-Verzeichnis erstellen und CMake konfigurieren:**

```bash
mkdir -p build
cd build
cmake .
```

3. **Kompilieren:**

```bash
make
```

Das fertige Programm liegt anschließend unter `build/Deadlocks_Simulator`.

#### Alternativ: Direkt mit dem vorhandenen cmake-build-debug

Falls CLion verwendet wird, kann das Projekt direkt über die IDE gebaut werden. Das `cmake-build-debug`-Verzeichnis ist bereits vorkonfiguriert.

---

### Programm starten

```bash
./build/Deadlocks_Simulator
```

Das Fenster öffnet sich. Die Simulation startet **noch nicht** — erst nach Auswahl einer Policy.

---

### Bedienung der Oberfläche

Die Benutzeroberfläche ist in vier Bereiche gegliedert:

```
┌──────────────────────────────────────────────────────────┐
│  DEADLOCK SIMULATOR          BETRIEB SYSTEME             │  ← Header
├──────────────────────────────────────────────────────────┤
│ ⬡ GRAPH DETECT  ⬡ BANKER'S ALGO  ⬡ HOLD & WAIT  ▶ STEP │  ← Buttons
├──────────────────────────────────────────────────────────┤
│  STRATIGIE  —   ·   VERSCHOBEN  0   ·   TICK  0          │  ← Statusleiste
├──────────────────────────────────┬───────────────────────┤
│                                  │  LEGENDE              │
│  === Takt 5 ==================   │  Gehaltene Ressourcen │
│  Verfügbar je Klasse : 1 2       │  bedarf noch benötigt │
│  P0  zugeteilt: 1 0   bedarf: 0 2│  Verfügbar freeie Ins.│
│  P1  zugeteilt: 0 1   bedarf: 2 0│  Verschoben block. anf│
│  P2  zugeteilt: 0 0   bedarf: 1 1│  TAKT Simulationsuhr  │
│                                  │                       │
│                                  │  Wähle einen Policy   │
│                                  │  zum starten.         │
└──────────────────────────────────┴───────────────────────┘
```

#### Schaltflächen

| Schaltfläche | Funktion |
|---|---|
| **⬡ GRAPH DETECT** | Startet die Simulation mit der Wait-For-Graph-Zykluserkennung. |
| **⬡ BANKER'S ALGO** | Startet die Simulation mit dem Bankier-Algorithmus. |
| **⬡ HOLD & WAIT** | Startet die Simulation mit der Hold-and-Wait-Eliminierung. |
| **▶ STEP** | Führt einen einzelnen Simulationsschritt manuell aus. |

#### Statusleiste

- **POLICY**: Zeigt den Namen der aktuell aktiven Strategie.
- **POSTPONED**: Anzahl der bisher abgelehnten und verschobenen Ressourcenanfragen — je höher, desto restriktiver ist die Policy.
- **TICK**: Aktueller logischer Zeitschritt der Simulation.

#### Hauptanzeige (TextView)

Zeigt in Echtzeit (automatisch jede Sekunde):
- `Available per class`: Derzeit frei verfügbare Ressourcen je Klasse.
- `Px  alloc: ...`: Ressourcen, die Prozess x aktuell hält.
- `Px  need: ...`:  Ressourcen, die Prozess x noch benötigt.

#### Strategien vergleichen

1. Starte mit **GRAPH DETECT** und beobachte den POSTPONED-Zähler.
2. Klicke auf **BANKER'S ALGO** — die Simulation startet neu. Vergleiche den Anstieg des Zählers.
3. Wechsle zu **HOLD & WAIT** — diese Policy ist oft die restriktivste und erzeugt den höchsten Zähler.
4. Nutze **▶ STEP**, um die Simulation Schritt für Schritt nachzuvollziehen.

---

## Codestruktur (Doxygen)

```
Deadlocks_Simulator/
├── CMakeLists.txt              # Build-Konfiguration (CMake)
├── README.md                   # Diese Dokumentation
└── Src/
    ├── main.c                  # Einstiegspunkt: GTK-Init, Fenster erstellen
    ├── core/                   # Simulationskern
    │   ├── types.h             # Gemeinsame Typdefinitionen (Event, SystemState, Funktionszeiger)
    │   ├── event.h / event.c   # Min-Heap-basierte zeitgesteuerte Event-Queue
    │   ├── state.h / state.c   # Systemzustand (Matrizen, Bankier-Sicherheitsprüfung)
    │   └── scheduler.h / scheduler.c  # Ereignisschleife und Ressourcenvergabe
    ├── policy/                 # Deadlock-Strategien (Strategy-Pattern)
    │   ├── policy.h            # Gemeinsames Policy-Interface
    │   ├── banker_policy.c     # Bankier-Algorithmus
    │   ├── detect_policy.c     # Wait-For-Graph-Zykluserkennung
    │   └── holdwait_policy.c   # Hold-and-Wait-Eliminierung
    └── gui/                    # Grafische Benutzeroberfläche
        ├── gui.h               # Öffentliches GUI-Interface
        └── gui.c               # GTK-3-Implementierung
```

### Modul-Beschreibungen

#### `core/types.h` — Gemeinsame Typdefinitionen

Zentrale Typdatei, die alle Module verbindet. Definiert:

- **`EventType`**: Aufzählung der möglichen Ereignisarten (`EV_REQUEST`, `EV_RELEASE`, `EV_CHECKPOINT`, `EV_TERMINATE`).
- **`Event`**: Struktur eines Simulationsereignisses mit Zeitstempel, Typ, Prozess-ID, Ressourcenklasse, Menge und Retry-Zähler.
- **`SystemState`**: Kernstruktur des Ressourcenmodells mit den Matrizen E (Gesamtinstanzen), A (verfügbar), C (Allokation) und R (Bedarf).
- **Funktionszeiger** `policy_request_f`, `policy_tick_f`, `policy_cleanup_f`: Definieren das austauschbare Policy-Interface.

#### `core/event.h` / `core/event.c` — Ereignis-Heap

Implementiert eine Min-Heap-basierte Prioritätswarteschlange für zeitgesteuerte Ereignisse. Ereignisse werden aufsteigend nach Zeitstempel sortiert, sodass der Scheduler stets das zeitlich früheste Ereignis zuerst verarbeitet.

- **Datenstruktur**: Binärer Min-Heap mit 1-basierter Indizierung. Kapazität verdoppelt sich automatisch bei Bedarf.
- **Kernoperationen**: `event_push()` (Einfügen mit Sift-Up), `event_pop()` (Entfernen mit Sift-Down), `event_peek()` (Lesen ohne Entfernen).
- **Zeitkomplexität**: O(log n) für Push und Pop.

#### `core/state.h` / `core/state.c` — Systemzustand

Verwaltet den zentralen Zustand des Ressourcensystems nach dem Bankier-Modell:

- **`state_create()`**: Allokiert den Zustand mit gegebenen Ressourcenklassen.
- **`state_allocate_process_matrices()`**: Legt die Allokations- und Bedarfsmatrizen an (nach Bekanntwerden der Prozessanzahl).
- **`state_is_safe()`**: Kernfunktion — implementiert den Bankier-Algorithmus. Prüft mit einem Arbeitsvektor iterativ, ob alle Prozesse in einer bestimmten Reihenfolge abgeschlossen werden können.
- **`state_set_allocation()` / `state_set_request()`**: Setzen einzelne Matrizeneinträge mit Bereichsprüfung.

#### `core/scheduler.h` / `core/scheduler.c` — Ereignis-Scheduler

Das Herzstück der Simulation. Verarbeitet Ereignisse chronologisch aus dem Heap und aktualisiert den Systemzustand:

- **Opaque-Pointer-Pattern**: Die interne `struct Scheduler` ist nur in `scheduler.c` sichtbar — vollständiges Information Hiding.
- **`handle_request()`**: Interne Funktion, die eine Ressourcenanforderung verarbeitet. Delegiert an die Policy; bei Ablehnung wird das Ereignis mit erhöhtem Retry-Zähler erneut eingeplant (max. 100 Versuche).
- **`scheduler_run_until()`**: Hauptschleife — verarbeitet alle Ereignisse im Zeitfenster [now, until_time].
- **Event-Typen**: `EV_REQUEST` (Policy-Entscheidung), `EV_RELEASE` (direkte Freigabe mit Über-Freigabe-Schutz), `EV_TERMINATE` (vollständige Ressourcenfreigabe und Bedarfsreset).

#### `policy/policy.h` — Policy-Interface

Definiert das gemeinsame Interface aller Deadlock-Strategien nach dem **Strategy-Pattern**:

```c
typedef struct Policy {
    const char       *name;       // Anzeigename
    policy_request_f  on_request; // Entscheidungs-Callback
    policy_tick_f     on_tick;    // Optionaler Wartungs-Callback
    policy_cleanup_f  cleanup;    // Speicherfreigabe
    void             *private;    // Policy-spezifischer Kontext
} Policy;
```

Alle drei Policies teilen dieselbe Struktur; der Scheduler muss nicht wissen, welche konkrete Strategie aktiv ist.

#### `policy/banker_policy.c` — Bankier-Algorithmus

Implementiert die klassische Deadlock-Vermeidung nach Dijkstra:

1. Simuliert die Ressourcenvergabe temporär in den Matrizen.
2. Ruft `state_is_safe()` auf.
3. Macht die Simulation rückgängig.
4. Genehmigt nur bei sicherem Zustand.

Der private Kontext `BankerCtx` zählt Ablehnungen (`deadlocks`).

#### `policy/detect_policy.c` — Wait-For-Graph-Zykluserkennung

Erkennt potenzielle Deadlocks anhand von Zyklen im Wait-For-Graphen:

1. Baut eine n×n Adjazenzmatrix auf: Kante A→B, wenn A eine Ressource braucht, die B hält.
2. Führt eine DFS mit „on-stack"-Bit zur Zykluserkennung durch.
3. Genehmigt die Anfrage nur, wenn kein Zyklus entsteht.

#### `policy/holdwait_policy.c` — Hold-and-Wait-Eliminierung

Einfachste der drei Strategien: Unterbindet die Hold-and-Wait-Bedingung durch eine einfache Prüfung vor jeder Ressourcenvergabe:

- Hat der anfragende Prozess bereits Ressourcen irgendeiner Klasse? → Ablehnen.
- Sind nicht genügend Ressourcen verfügbar? → Ablehnen.
- Sonst: Genehmigen.

#### `gui/gui.h` / `gui/gui.c` — GTK-3-Frontend

Implementiert die vollständige grafische Oberfläche mit GTK 3:

- **`gui_create_window()`**: Erstellt alle Widgets, lädt das CSS-Theme und startet den 1-Sekunden-Timer.
- **`gui_set_policy()`**: Orchestriert den Policy-Wechsel — gibt alle alten Ressourcen frei und startet die Simulation neu.
- **`gui_step()`**: Ein Simulationsschritt — erzeugt Prozessereignisse, lässt den Scheduler sie verarbeiten und aktualisiert die Anzeige.
- **Prozess-Simulator** (`ProcSim`, `generate_proc_events()`): Modelliert 3 Prozesse mit Zustandsautomat IDLE→RUNNING→DONE und erzeugt daraus die Ereignissequenz.
- **Demo-Szenario** (`build_demo_state()`): Festes Szenario mit 2 Ressourcenklassen (2 Drucker, 3 Festplatten) und 3 Prozessen mit definierten Maximalbedarfswerten.

### Abhängigkeitsgraph der Module

```
main.c
  └── gui/gui.c
        ├── core/scheduler.c
        │     ├── core/event.c
        │     └── core/state.c
        │           └── core/types.h
        └── policy/
              ├── banker_policy.c
              ├── detect_policy.c
              └── holdwait_policy.c
                    └── policy.h → types.h
```

Alle Module hängen letztlich von `core/types.h` ab, das die gemeinsamen Datenstrukturen definiert. `gui.c` ist der einzige Ort, an dem alle Module zusammengeführt werden.

---

*Dokumentation generiert mit Doxygen-Kommentaren im Quellcode.*
