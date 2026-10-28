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

### 3. Patch e Correzioni di Bug Applicate al Codice
Tutte le correzioni sono state applicate in singoli commit Git incrementali, ciascuno firmato con `Made-by-IA: Gemini`:

1. **[`74351e8b`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/wmspec.c#L990-L1015) - `wmspec`: Correzione `sizeof(pointer)` in `updateWorkspaceNames`**:
   - `sizeof(pos)` restituiva 8 byte (dimensione del puntatore su 64-bit), limitando la copia a 7 byte e lasciando `_NET_DESKTOP_NAMES` corrotto.
   - Sostituito con copia sicura limitata a `MAX_WORKSPACENAME_WIDTH` e terminazione null esplicita.
2. **[`61e5fc9e`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/dock.c#L4745-L4870) - `dock`: Protezione da stack corruption e VLA nei Drawer e Menù**:
   - Sostituiti i buffer VLA sullo stack (`WAppIcon *aicons_to_shift[drawer->icon_count]` e `Window wins[n]`) con allocazioni dinamiche sicure su heap dimensionate su `max_icons`.
   - Aggiunto bounds check difensivo su `wins[idx]` per evitare scritture oltre i limiti dello stack.
   - In `updateWorkspaceMenu()`: spostato il null-check `!menu` prima dell'accesso a `menu->frame`.
   - In `makeWorkspaceMenu()`: aggiunto `return NULL` immediato se `wMenuCreate()` fallisce.
3. **[`2fb52eaf`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/actions.c#L198-L215) - `actions`: Controllo NULL su `focused` in `wSetFocusTo`**:
   - Se `scr->focused_window` è NULL (ad es. chiusura dell'ultima finestra del workspace), `focused->next` causava SIGSEGV. Aggiunta la guardia difensiva `if (focused) focused->next = wwin;`.
4. **[`d61746b3`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/client.c#L105-L150) - `client`: Caso default in `wClientGetGravityOffsets`**:
   - Aggiunto ramo `default` per inizializzare `*ofs_x = 0; *ofs_y = 0;` in caso di valori non standard di `win_gravity` inviati dai client.
5. **[`a474b925`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/wtext.c#L1175-L1190) - `WINGs`: Return anticipato su blocco testo NULL in `wtext.c`**:
   - Rimosso il commento/printf `...for this app will surely crash` e inserito un `return;` per evitare la dereferenziazione di `tb->sections[s]` quando `tb` è nullo.
6. **[`8e56bf09`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/wballoon.c#L375-L395) - `WINGs`: Aritmetica su puntatore NULL in `wballoon.c`**:
   - Quando `strchr` non trovava un `\n`, `ptr2` diventava NULL e `ptr = ptr2 + 1` eseguiva aritmetica su puntatore nullo.
7. **[`223810ed`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/WINGs/WINGs.h#L965-L975) - `WINGs`: Allineamento ordine parametri negli header**:
   - Sincronizzato il prototipo di `WMSetViewExpandsToParent` con la definizione reale (`leftOffs` prima di `topOffs`), e i parametri di `W_SendDnDClientMessage` in `WINGsP.h.in`.
8. **[`35006a0f`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/session.c#L438-L445) - `session`: Specificatore di formato `%u` in `sscanf`**:
   - Corretto lo specificatore di formato da `%i` a `%u` per i campi `unsigned int w, h` di `WSavedState`.
9. **[`b58f0abe`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/rootmenu.c#L510-L525) - `rootmenu`: Controllo NULL su `line` in `next_token`**:
   - Prevenuta la dereferenziazione immediata di `*line` se passato un puntatore nullo.
10. **[`a9f0042e`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/menu.c#L285-L300) - `menu`: Controlli difensivi in `wMenuEntrySetCascade`**:
    - Aggiunto controllo preventivo `if (!menu || !entry || !cascade) return;`.

11. **[`c431060f`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/WINGs/WUtil.h#L525-L530) - `WINGs`: Allineamento nomi parametri in `WMAppendBag`**:
    - Sincronizzati i nomi dei parametri tra prototipo (`WUtil.h`) e definizione (`bagtree.c`).
12. **[`306917c9`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/dock.c#L2005-L4525) - `dock`: Controlli NULL su `old_top` e rimozione appicon singola nel drawer**:
    - In `wDockRestoreState()`: verificato `old_top != NULL` prima dell'aggiornamento coordinate icona predefinita.
    - In `removeDrawerCallback()`: verificato `aicon != NULL` prima dello spostamento dell'appicon superstite verso il dock principale.
13. **[`8e562f7d`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/workspace.c#L900-L915) - `workspace`: Troncamento nomi workspace ripristinati da WMState**:
    - Utilizzato `wstrndup()` con limite `MAX_WORKSPACENAME_WIDTH` per evitare che nomi di workspace eccezionalmente lunghi nel file di configurazione superino i buffer usati da Window Maker.

---

## Stato di Verifica
- Script di compilazione e comandi `make` eseguiti con successo (exit code 0).
- Tutto il software Window Maker e le librerie compilate sono installate e aggiornate in `/opt/wmaker`.
- `build_and_install.sh`, `start.sh` e `stop.sh` operativi e validati.
