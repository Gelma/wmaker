# SpecAI - Specifiche e Stato del Progetto Window Maker

## Panoramica del Progetto
- **Progetto**: Window Maker 0.96.0 (Window Manager per X Window System, stile NeXTSTEP / GNUstep).
- **Ambiente OS**: Ubuntu 24.04 LTS (x86_64, Linux).
- **Directory di installazione personalizzata**: `/opt/wmaker`.

---

## Obiettivi e Attività Eseguite

### 1. Script di compilazione e installazione (`build_and_install.sh`)
- Creato lo script eseguibile [build_and_install.sh](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/build_and_install.sh).
- **Caratteristiche dello script**:
  - Controllo automatico e installazione dei pacchetti/dipendenze necessari su Ubuntu 24.04 (`libx11-dev`, `libxext-dev`, `libxinerama-dev`, `libxrandr-dev`, `libxpm-dev`, `libxft-dev`, `libxmu-dev`, `libpango1.0-dev`, `libpng-dev`, `libjpeg-dev`, `libgif-dev`, `libtiff-dev`, `libwebp-dev`, `libexif-dev`, `libarchive-dev`, `autoconf`, `automake`, `libtool`).
  - Gestione della directory di destinazione `/opt/wmaker` (creazione e proprietà dell'utente corrente).
  - Configurazione con prefisso `/opt/wmaker` e supporto alle estensioni X (Xinerama, RandR, Shape, Shm, Pango, supporto immagini completo).
  - Compilazione parallela ottimizzata con flag `-march=native -O2 -g -Wall`.
  - Installazione completa in `/opt/wmaker`.
  - Creazione del wrapper di sessione `/opt/wmaker/bin/wmaker-session` con configurazione automatica di `PATH`, `LD_LIBRARY_PATH` e `XDG_DATA_DIRS`.
  - Registrazione della sessione X11 per il Display Manager in `/usr/share/xsessions/wmaker-opt.desktop`.

### 2. Script di controllo del ciclo di vita
- [start.sh](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/start.sh): Avvia Window Maker da `/opt/wmaker` nella sessione X11 corrente o inizializza un display `xinit` se assente.
- [stop.sh](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/stop.sh): Arresta in modo pulito i processi Window Maker inviando il segnale appropriato (`SIGTERM`).

### 3. Analisi del Codice Sorgente e Cause dei Riavvii Spontanei (Crash)
Window Maker implementa un'architettura **process-monitoring** in `src/main.c` e `src/monitor.c`:
- All'avvio, Window Maker forka un processo supervisore (`MonitorLoop`).
- Il processo figlio esegue il reale window manager (`--for-real`).
- Se il processo figlio termina a causa di un segnale fatale (`SIGSEGV`, `SIGBUS`, `SIGILL`, `SIGABRT`, `SIGFPE`), il supervisore lo cattura:
  - Se il crash avviene dopo oltre 3 secondi dall'avvio, il supervisore registra un warning (`Window Maker exited due to a crash (signal %i) and will be restarted`) e **rilancia immediatamente Window Maker**.
  - All'utente questo si manifesta esattamente come un **riavvio improvviso del window manager durante l'utilizzo**, con ricaricamento dell'interfaccia.

#### Analisi v2 (rieseguita, verificata manualmente sul codice)
Strumenti: clang scan-build 21 (29 segnalazioni su `src/`), cppcheck 2.13 su `src/`, `WINGs/`, `wrlib/`, revisione manuale.

**Correzioni rispetto all'analisi v1 (falsi positivi ritirati):**
- `dockedapp.c:168` (strlen(NULL)): NON raggiungibile, `wIconChangeImageFile()` ritorna 1 se `file == NULL`.
- RandR/`BadWindow`: `catchXError()` (startup.c:90) ignora già tutti i `BadWindow`; non causa crash.
- `xmodifier.c`: nessun uso di variabili non inizializzate; le funzioni annidate (estensione GCC) leggono `code`/`sym` solo dopo l'assegnazione.
- `wcolorpanel.c` cpColor.hsv: falso positivo (campo non usato con `set = cpRGB`).

**Bug confermati, in ordine di probabilità come causa dei crash:**
1. `dock.c` `wDrawerFillTheGap()` / `wSlideAppicons()` (righe ~4750-4800): VLA `aicons_to_shift[drawer->icon_count]` riempita contando le icone presenti in `icon_array`; se `icon_count` è disallineato (il codice stesso prevede il caso con un `wwarning`) si scrive oltre lo stack. In `wSlideAppicons` `wins[aicon->xindex - min_index]` non è controllato rispetto a `n`: xindex non contigui ⇒ scrittura fuori dallo stack. Rilevante solo se si usano i **Drawer**.
2. `wmspec.c` `updateWorkspaceNames()`: `strncpy(pos, name, sizeof(pos) - 1)` copia max 7 byte (sizeof puntatore) senza terminatore ⇒ `_NET_DESKTOP_NAMES` contiene byte non inizializzati dello stack. I nomi caricati da WMState (`workspace.c:910`) non sono troncati a 64 caratteri, quindi con nomi lunghi `pos` può superare `buf` (overflow di stack).
3. `actions.c:209` `wSetFocusTo()`: `focused->next = wwin` senza controllo; `scr->focused_window` può essere NULL (window.c:1679 dopo la chiusura dell'ultima finestra). Raggiungibile solo se `wwin` non è nella lista focus: guardia difensiva consigliata.
4. `client.c` `wClientGetGravityOffsets()`: lo `switch` su `win_gravity` (valore fornito dal client) non ha `default`; gravità invalida ⇒ `ofs_x/ofs_y` non inizializzati (posizionamento errato, nessun crash).
5. `dock.c:973` `updateWorkspaceMenu()`: `menu->frame` dereferenziato prima del controllo `if (!menu)`. `dock.c:1006-1013`: dopo `wMenuCreate` fallito si prosegue usando `menu` NULL.
6. `dock.c:2005` (`old_top` NULL se nessuna icona precedente), `dock.c:4518` (`aicon` NULL se il drawer non ha icone oltre la [0]): dereferenze NULL su percorsi rari di ripristino stato.
7. `WINGs/wtext.c:1174-1186`: `tb` dereferenziato dopo controllo `if (!tb)` (solo widget WMText, non usato da wmaker stesso).
8. `WINGs/string.c:217-220`, `WINGs/wballoon.c:388`: dereferenze dopo controlli NULL ridondanti.
9. Formati: `session.c:440` `%i` con `unsigned int *`, `geomview.c:109`, `dialog.c:1350`, `dock.c:1426`: innocui su x86_64, formalmente UB.
10. Stile/formali: `WINGs/memory.c:195` `ptr = NULL` inutile in `wfree`; ordine argomenti diverso tra dichiarazione e definizione in `WMAppendBag`, `WMSetViewExpandsToParent`, `W_SendDnDClientMessage`.

**Nota portabilità**: `xmodifier.c` usa funzioni annidate (estensione GCC): non compila con clang.

**Come diagnosticare il crash reale**: il monitor riavvia wmaker se crasha dopo >3s. Abilitare i core dump (`ulimit -c unlimited` prima di avviare la sessione, o `coredumpctl list wmaker`) e ottenere il backtrace con `gdb /opt/wmaker/bin/wmaker core` (`bt full`). La build ha già i simboli (`-g`).

---

## Stato di Verifica
- Script di compilazione testato con esito positivo: exit code 0.
- Binari generati e installati in `/opt/wmaker`:
  - `wmaker` (versione 0.96.0)
  - `WPrefs`
  - utility di supporto (`wmsetbg`, `wmagnify`, `getstyle`, `setstyle`, `wxcopy`, ecc.)
  - librerie condivise (`libWINGs.so`, `libwraster.so`, `libWUtil.so`, `libWMaker.so`)
- Script `build_and_install.sh`, `start.sh` e `stop.sh` operativi e validati.
