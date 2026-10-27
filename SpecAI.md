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

#### Principali criticità e bug identificati nel codice:
1. **Crash in `src/xmodifier.c` (`x_reset_modifier_mapping`)**:
   - Nella commit recente `b4bd6d0c`, l'assegnazione è stata mutata in `sym = W_KeycodeToKeysym(display, code, column);`.
   - In determinate configurazioni della mappa tastiera o layout multilivello, `sym` non veniva inizializzato prima della chiamata o risultava `NoSymbol`, ma soprattutto le sub-routine inline (`modwarn`, `modbarf`, `store_modifier`) usavano variabili esterne `code` e `sym` prima che fossero valorizzate per il blocco logico, rischiando decisioni errate e branch su valori spazzatura.
2. **Buffer Overflow in `src/wmspec.c` (`updateWorkspaceNames`)**:
   - `char buf[MAX_WORKSPACES * (MAX_WORKSPACENAME_WIDTH + 1)], *pos;`
   - `strncpy(pos, scr->workspaces[i]->name, sizeof(pos) - 1);`
   - **Bug formale grave**: `sizeof(pos)` restituisce la dimensione del puntatore (8 byte su 64-bit), invece dello spazio rimanente nel buffer o della lunghezza del nome. Inoltre `strncpy` non garantisce il terminatore null se il nome supera 7 caratteri, e l'avanzamento `pos += (curr_size + 1)` assume ciecamente che tutto il nome sia stato copiato, provocando disallineamenti di memoria e potenziale sovrascrittura di stack quando si manipolano workspace o NetWM aggiorna i nomi.
3. **Potenziale dereferenziazione NULL in `src/dockedapp.c` (`panelBtnCallback`)**:
   - Alla riga 168: se `text` è vuoto o NULL (`text = NULL;`), la chiamata `wIconChangeImageFile(..., text)` ritorna false, portando all'esecuzione di `strlen(text)` che va in crash immediato (SIGSEGV).
4. **Crash in `WINGs/wtext.c`**:
   - Alla riga 1185/1186: presenza di blocco commentato `if (!tb) printf("...for this app will surely crash :-)\n");` seguito da dereferenziazione incondizionata `tb->sections[s]` senza null-check difensivo durante il calcolo dei confini di scrolling/testo.
5. **Gestione eventi RandR / Debounce (`src/randr.c`)**:
   - Il supporto RandR (specie con monitor esterni hotplug o cambio risoluzione) usa un timer di debounce (`randr_debounce_timer`). Se durante un rimescolamento di uscite video (`wRandR_BringAllWindowsInside`, `wDockSwap`) una finestra ha già perso il riferimento al root window o è in stato zombie, si verificano errori `BadWindow` non gestiti su Xlib che portano all'uscita o abort se X synchronous o se un crash handler non è configurato per ignorare `BadWindow`.
6. **Formati `printf` / `sscanf` non corrispondenti**:
   - In `src/session.c`, `src/dialog.c`, `src/dock.c` e `src/geomview.c` sono presenti disallineamenti tra specificatori di formato (`%hu`, `%u`, `%i`) e tipi `unsigned int`/`int *`/`signed int *` che possono causare corruzione dello stack su architetture a 64 bit quando combinati con dati esterni del session file.

---

## Stato di Verifica
- Script di compilazione testato con esito positivo: exit code 0.
- Binari generati e installati in `/opt/wmaker`:
  - `wmaker` (versione 0.96.0)
  - `WPrefs`
  - utility di supporto (`wmsetbg`, `wmagnify`, `getstyle`, `setstyle`, `wxcopy`, ecc.)
  - librerie condivise (`libWINGs.so`, `libwraster.so`, `libWUtil.so`, `libWMaker.so`)
- Script `build_and_install.sh`, `start.sh` e `stop.sh` operativi e validati.
