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
14. **[`588e572d`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/util/wxpaste.c#L80-L150) - `wxpaste`: Correzione del loop di eventi e variabile non inizializzata**:
    - In `fetchSelection()` il ciclo d'attesa dell'evento `SelectionNotify` era erroneamente scritto come `while (0)`, saltando completamente la ricezione e lasciando `res` non inizializzata.
    - In `main()` la variabile di lunghezza `l` passata a `write(STDOUT_FILENO, buf, l)` non veniva impostata da `fetchSelection()`, causando la scrittura di byte casuali su stdout.
    - Corretto il ciclo in `while (1)`, inizializzata `res = 0`, restituita la lunghezza effettiva della proprietà in `l` e inizializzata `l = 0`.
15. **[`4d8a5118`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/wtext.c#L1170-L1185) - `WINGs`: Controllo NULL preventivo su `tb` in `wtext.c`**:
    - Nel salto a `_doneH:` all'interno di `findClosestPositionTo()`, `tb->graphic` veniva dereferenziato prima del controllo `if (!tb) return;` situato sotto `_doNothing:`.
    - Spostato il controllo di validità `if (!tb) return;` all'inizio del blocco `_doneH:`, prevenendo dereferenziazioni su puntatore nullo nei widget di testo.
16. **[`45de612a`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/Extras/wtableview.c#L870-L895) - `WINGs`: Inizializzazione della struttura `range` in `columnsInRect`**:
    - Se `totalColumns == 0` o nessuna colonna intersecava il rettangolo, `range.position` e `range.count` restavano non inizializzati, causando accessi out-of-bounds in `drawRow()`.
    - Inizializzati `range.position = 0; range.count = 0;` con uscita rapida se nessuna colonna è presente o trovata.
17. **[`d0053942`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/appmenu.c#L130-L140) - `appmenu`: Limite di ampiezza in `sscanf` contro stack overflow**:
    - Nel parsing del menu applicazione X11 in `parseMenu()`, `%s` senza limite di ampiezza leggeva una stringa da finestra client dentro il buffer su stack `char rtext[300]`.
    - Aggiunto il limite `%299s` per scongiurare stack buffer overflow in caso di payload malevoli o corrotti inviati dai client X.
18. **[`c4172978`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/xdnd.c#L80-L86), [`b22c8da1`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/util/wmiv.c#L1595-L1605), [`26370998`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/util/setstyle.c#L225-L235), [`cfe35312`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WPrefs.app/Appearance.c#L1315-L1330) - Correzione tipi `%x`/`%X` in `sscanf`/`fscanf`**:
    - Risolti i type mismatch dove venivano passati puntatori a variabili intere con segno (`int *`) a specificatori esadecimali `%x`/`%2X` che richiedono rigorosamente `unsigned int *`.
19. **[`ae59afa4`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/wcolorpanel.c#L1890-L3490) - `WINGs`: Inizializzazione struct `CPColor`, fix formato e base length nel Color Panel**:
    - Inizializzate con `{ 0 }` tutte le istanze locali di `CPColor` sullo stack nelle callback per evitare valori spazzatura in `cpColor.hsv` quando viene impostato solo `cpColor.rgb`.
    - Corretto l'overflow potenziale di buffer in `hsbInit()` ingrandendo `tmp` a 16 byte con `snprintf()` e specificatore `%d` al posto di `%hu`.
    - In `generateNewFilename()` verificata la presenza di spazio prima di `{` per evitare indici negativi e buffer underflow in `strncpy()` su nomi colore specifici.
20. **[`bb0727d2`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/dialog.c#L1157-L1160) - Allineamento specificatori di formato `printf`/`snprintf` e dimensioni buffer**:
    - `src/dialog.c`: Corretto `%hu` in `%u` per l'anno corrente in `COPYRIGHT_TEXT`.
    - `src/dock.c`: Utilizzato `%d` per `current_workspace` e ampliato il buffer `ws_number` a 32 byte per eliminare i warning di troncamento.
    - `src/geomview.c`: Sostituito `%+5i` con `%5u` per le dimensioni di larghezza e altezza senza segno.
    - `src/session.c`: Sostituito `%ix%i` con `%ux%u` per memorizzare width ed height senza segno.
    - `WINGs/wruler.c`: Ingrandito il buffer `c` a 16 byte e corretto `%hu` in `%d` con `strlen(c)` dinamico.
21. **[`b4499511`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/client.c#L310-L320) - `client`: Guardie NULL in `wClientCheckProperty`**:
    - Aggiunto controllo preventivo `if (!wwin || !event) return;` per evitare dereferenziazione immediata di `wwin->flags` quando viene gestito un evento PropertyNotify su finestre non associate o non valide.
22. **[`c7dfaef1`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/event.c#L732-L1590) - `event`: Verifica puntatore `main_window_desc` negli handler di eventi**:
    - In `handleDestroyNotify`, `handlePropertyNotify` e nel binding `WKBD_HIDE`, verificata la non-nullità di `wapp->main_window_desc` prima di accedere a `screen_ptr` o a `WFLAGP`, prevenendo crash di Window Maker quando un'applicazione X11 si chiude in modo anomalo.
23. **[`1b03ad80`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/actions.c#L2014-L2018) - `actions`: Controllo NULL in `wUnhideApplication`**:
    - Aggiunta la guardia `if (!wapp || !wapp->main_window_desc) return;` per evitare la dereferenziazione nulla di `wapp->main_window_desc->screen_ptr`.
24. **[`d8f5f4cd`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/winmenu.c#L638) - `winmenu`: Guardia NULL su `main_window_desc` nel menu finestra**:
    - In `updateMenu()`, verificata l'esistenza di `wapp->main_window_desc` prima di valutare la macro `WFLAGP(wapp->main_window_desc, no_appicon)`, evitando crash durante l'apertura del menù comandi finestra.
25. **[`6d46cd4b`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/window.c#L1329) - `window`: Controllo NULL su `main_window_desc` in `wManageWindow`**:
    - Verificato `app->main_window_desc != NULL` prima dell'accesso all'attributo `start_hidden`.
26. **[`8dcb6567`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/wrlib/wraster.h.in#L65) - `wrlib`: Macro `__wrlib_nonnull` variadica nel template `wraster.h.in`**:
    - Nel template di fallback, la macro `#define __wrlib_nonnull(argidx)` accettava un solo argomento invece di `(...)`, causando l'abort dell'espansione macro e dei parser di analisi statica. Corretta in macro variadica `__wrlib_nonnull(...)`.
27. **[`99a11f75`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/util/xdnd.c#L525-L545) - `xdnd`: Correzione aritmetica puntatori out-of-bounds e controlli allocazione heap**:
    - In `util/xdnd.c:526`, `*descriptions` è di tipo `char **`. L'istruzione `memcpy(*descriptions + l, data, dcount)` sommava l'offset moltiplicato per `sizeof(char *)`, provocando grave corruzione dello heap e stack smash. Corretto con casting a byte `(char *)*descriptions + l`.
    - Aggiunti controlli di fallimento allocazione su `*descriptions` e `*types`.
28. **[`2873c117`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/util/wmmenugen_parse_wmconfig.c#L105) - `wmmenugen`: Guardia NULL su `label` in `parse_wmconfig_file`**:
    - Se una linea di configurazione non forniva label (`label == NULL`), la chiamata `strcmp(lastlabel, label)` causava crash immediato di `wmmenugen`. Aggiunta la condizione di guardia `if (label)`.
29. **[`14f5d37e`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/session.c#L188-L195) - `session`: Eliminazione leak di memoria e possibile crash NULL su classe finestra**:
    - La riassegnazione `class = strdup(wwin->wm_class)` perdeva il riferimento al puntatore allocato da XGetClassHint e non controllava `wwin->wm_class != NULL`. Sostituito con un puntatore temporaneo `const char *c` sicuro.
30. **[`9dc319ea`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/superfluous.c#L145-L155) - `superfluous`: Controllo del valore di ritorno di `malloc` in `wAppBounce`**:
    - Verificata la corretta allocazione di memoria per il buffer `points` prima del ciclo di calcolo dell'animazione.
31. **[`83ebd261`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/window.c#L1840) - `window`: Pulizia controllo ridondante di `focus_mode` in `wUnmanageWindow`**:
    - Rimosso controllo annidato duplicato `focus_mode == WKF_CLICK` già garantito dalla condizione esterna.
32. **[`5625724f`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/main.c#L365) - `main`: Utilizzo di `wmalloc` per `wVisualID`**:
    - Sostituita la chiamata non controllata a `malloc()` con la funzione sicura `wmalloc()`.
33. **[`0a48ac89`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/xdnd.c#L445) - `xdnd`: Controllo fallimento allocazione in `wXDNDGetTypeList`**:
    - Verificata l'allocazione dell'array `typelist` per evitare dereferenziazione NULL in condizioni OOM.
34. **[`22f090e0`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/dialog.c#L1158) - `dialog`: Allineamento specificatore di formato `%d` per `current_year`**:
    - Sostituito lo specificatore unsigned `%u` con `%d` per intero con segno in `COPYRIGHT_TEXT`.
35. **[`58f2a261`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/menuparser_macros.c#L205) - `WINGs`: Specificatore `%u` per conteggio senza segno in `menuparser_macros`**:
    - Corretto specificatore `%d` in `%u` per variabile unsigned int.
36. **[`757e51a2`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/wrlib/scale.c#L740-L750) - `wrlib`: Controllo allocazioni di memoria in `RSmoothScaleImage`**:
    - Verificata la corretta allocazione delle linee di raster `wlines[0]` e `wlines[1]` per prevenire crash su immagini scalate in esaurimento memoria.
37. **[`f3fd4576`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/Tests/wtest.c#L360) - `WINGs/Tests`: Chiusura file descriptor e conversione encoding a UTF-8**:
    - Inserita la chiusura `fclose(f)` nel ciclo di lettura dei file di test e convertito il carattere accentato Latin-1 in UTF-8 valido.
38. **[`42bdb767`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/wrlib/gradient.c#L47) - `wrlib`: Controllo NULL su array `colors` in `RRenderMultiGradient`**:
    - Aggiunta guardia `if (!colors) return NULL;` prima dell'accesso ciclico agli stop del gradiente.
39. **[`f2a3be62`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/string.c#L209) - `WINGs`: Semplificazione controlli null pointer in `wstrconcat`**:
    - Ottimizzata la sequenza di guardie sui puntatori alle stringhe concatenate.
40. **[`4b7b4e79`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/wtext.c#L355-L2085) - `WINGs`: Incapsulamento `printf` di debug sotto macro `DEBUG` e pulizia logica**:
    - Racchiuse le istruzioni di logging diagnostico di basso livello in `#ifdef DEBUG` e corretto specificatore unsigned `%u` per `tb->first`.
    - Rimosse condizioni ridondanti su `tb->graphic` e semplificata l'assegnazione di allineamento del testo.
41. **[`84cca8a3`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/memory.c#L195) - `WINGs`: Rimozione assegnazione morta su `ptr` in `wfree`**:
    - Rimosso `ptr = NULL;` al termine di `wfree(void *ptr)` in quanto parametro passato per valore, privo di effetti esterni e segnalato da Cppcheck come `uselessAssignmentPtrArg`.
42. **[`053afe12`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/wtext.c#L1660) - `WINGs`: Specificatore `%u` per `nitems` in `wtext.c`**:
    - Corretto lo specificatore di formato da `%d` a `%u` per la variabile `unsigned int nitems` nella `printf` diagnostica.
43. **[`ec434c8b`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/wrlib/tests/testgrad.c#L109-L120) - `wrlib/tests`: Verifica valore di ritorno di `malloc` nei test dei gradienti**:
    - Verificata l'allocazione dell'array `colors` e delle singole strutture `colors[i]` in `testgrad.c` prima della dereferenziazione.
44. **[`199419bd`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/WINGs/WUtil.h#L48-L208) - `WINGs`: Protezione macro di asserzione con `do-while` e parentesi in `WMScale`**:
    - Racchiuse le macro di asserzione `wassertr` e `wassertrv` in blocchi `do { ... } while (0)` per evitare il bug del dangling-else se usate in strutture `if-else`.
    - Inserite le parentesi `(value)` in `(double)(value)` nelle macro `WMScaleX` e `WMScaleY` per prevenire errori di precedenza operatore quando viene passata un'espressione aritmetica complessa.
45. **[`d54c888c`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/WindowMaker.h#L261-L274) - `wmaker`: Incapsulamento `WCHANGE_STATE` e `SIG_WCHANGE_STATE` in `do-while`**:
    - Sostituiti i blocchi di istruzioni semplici `{ ... }` con costrutti sicuri `do { ... } while (0)`.
46. **[`bff27388`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/xdnd.h#L33-L42) - `xdnd`: Parentesi su parametro indice in `XDND_ENTER_TYPE` e `do-while` per `XDND_STATUS_RECT_SET`**:
    - In `src/xdnd.h` e `util/xdnd.h`, corretto `2 + i` in `2 + (i)` ed esteso l'incapsulamento in `do { ... } while (0)` per `XDND_STATUS_RECT_SET`.
47. **[`819e62e8`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/window.h#L83) - `window`: Parentesizzazione espressione con virgola in `WSETUFLAG`**:
    - Racchiusa tra parentesi tonde l'espressione `((wwin)->user_flags.FLAG = (VAL), ...)` per evitare problemi di precedenza con operatori ternari o assegnazioni.
48. **[`84e8f018`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/stacking.h#L32) - `stacking`: Rimozione prototipo obsoleto `CommitStackingForFrame`**:
    - Eliminata la dichiarazione superflua e non implementata, sostituita storicamente da `CommitStackingForWindow`.
49. **[`fdf528c5`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/workspace.h#L40) - `workspace`: Rimozione prototipo orfano `wWorkspaceMenuEdit`**:
    - Rimosso prototipo non presente in alcuna unità di compilazione.
50. **[`64d03409`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/texture.h#L168-L177) - `texture`: Rimozione prototipi non implementati e sicurezza `FREE_PIXMAP`**:
    - Rimossi i prototipi non implementati `wTexturePaint`, `wTextureRender` e `wTexturePaintTitlebar`.
    - Racchiusa la macro `FREE_PIXMAP` in blocco protetto `do { ... } while (0)`.
51. **[`e907a7c3`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/defaults.h#L39) - `defaults`: Rimozione prototipo morto `wSaveDefaults`**:
    - Eliminata la dichiarazione non implementata.
52. **[`6a09948f`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/xutil.h#L25-L28) - `xutil`: Rimozione prototipi orfani `RequestSelection` e `GetSelection`**:
    - Rimosse le dichiarazioni residue risalenti al vecchio codice di selezione X11.
53. **[`ec224d35`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/keybind.h#L195) - `keybind`: Rimozione prototipo morto `wKeyboardInitialize`**:
    - Eliminato prototipo non definito in `src/misc.c` né nel resto del progetto.
54. **[`e09bbed4`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/WINGs/WINGs/WINGsP.h.in#L332) - `WINGs`: Rimozione prototipo obsoleto `W_FreeDragOperationItem`**:
    - Rimosso il prototipo privo di implementazione nel modulo drag-and-drop.
55. **[`510eb8c4`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/menu.c#L553-L573) - `menu`: Cancellazione timer `jump_back` alla distruzione del menu (Fix Segfault `wMenuMove` offset `0x4a903`)**:
    - Analizzato il crash kernel (`segfault at 51 ip 000055555559e903 ... in wmaker[4a903]`), localizzato in `wMenuMove` (`src/menu.c:2126`) durante la dereferenziazione di `menu->frame->core->window` su una struttura `WMenu` già liberata (use-after-free).
    - `wMenuScroll()` registrava un timer di 400ms (`MENU_JUMP_BACK_DELAY`, `callback_leaving`) passando `menu->jump_back`, ma `wMenuDestroy()` cancellava solo `menu->timer` dimenticando `menu->jump_back`. Se il menu veniva distrutto prima dello scadere dei 400ms, `callback_leaving` invocava `wMenuMove(dl->menu, ...)` sul menu deallocato.
    - Corretto `wMenuDestroy` per cancellare e liberare `menu->jump_back`, e corretto `wMenuScroll` quando `jump_back` viene azzerato cliccando sulla barra del titolo.
56. **[`e999a4bd`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/winmenu.c#L849-L879) - `winmenu`: Reset di `scr->workspace_submenu` e pulizia `rtext` in `DestroyWindowMenu`**:
    - `DestroyWindowMenu()` distruggeva ricorsivamente `scr->window_menu` (incluso il sottomenu `scr->workspace_submenu`) senza azzerare `scr->workspace_submenu = NULL` e senza liberare preventivamente le stringhe `rtext` delle scorciatoie nel sottomenu opzioni (prevenendo un double-free con `setModifierKeyLabels`/`setModifierShortKeyLabels`).
57. **[`108f91eb`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/rootmenu.c#L1623-L1687) - `rootmenu`: Protezione contro la distruzione di `scr->root_menu` mentre è attivo/mappato e ricostruzione di `wKeyTreeRoot`**:
    - Impedita in `wRootMenuReparse()` la distruzione di `scr->root_menu` (es. su evento `inotify` di `wDefaultsCheckDomains`) mentre il menu o un suo sottomenu/brother è mappato o dentro `menuMouseDown` (`flags.inside_handler`), evitando il use-after-free in `menuMouseDown` -> `wMenuMove(parentMenu(desc->parent), ...)`.
    - Aggiunta la chiamata a `wKeyTreeRebuild(scr)` quando cambiano le scorciatoie del root menu in `wRootMenuReparse()`, `OpenRootMenu()` ed `executeButtonAction()`, evitando puntatori pendenti `WMenu *` / `WMenuEntry *` nell'albero `wKeyTreeRoot`.
58. **[`55a742a3`](file:///home/gelma/dev/prg/intelligenti/wmaker/wmaker_git/src/menu.c#L604-L2189) - `menu`: Hardening di `wMenuMove`, `wMenuDestroy` e `menuMouseDown` contro puntatori NULL o pendenti**:
    - Aggiunte guardie su `menu->frame`, `menu->frame->core`, limiti di `selected_entry` e `menu->cascades[i] != NULL` in `wMenuMove`, `selectEntry`, `closeBrotherCascadesOf`, `parentMenu`, `raiseMenus`, `keyboardMenu`, `saveMenuRecurs` e `restoreMenuRecurs`.
    - In `wMenuDestroy(menu, False)`, scollegato `menu->cascades[i]->parent = NULL` per evitare puntatori `parent` pendenti nei sottomenu non distrutti.
    - In `menuMouseDown()`, evitata la sovrascrittura di `desc` all'apertura del window menu tramite tasto destro su `scr->switch_menu`, garantendo l'azzeramento di `flags.inside_handler`.

---

## Stato di Verifica
- Script di compilazione e comandi `make` eseguiti con successo (exit code 0).
- Tutto il software Window Maker e le librerie compilate sono installate e aggiornate in `/opt/wmaker`.
- `build_and_install.sh`, `start.sh` e `stop.sh` operativi e validati.
