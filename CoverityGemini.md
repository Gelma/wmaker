# Piano di Azioni Correttive dei Difetti Coverity (CoverityGemini.md)

**Data:** 2026-11-04  
**Dataset analizzato:** Report Coverity in `CoverityReport/` (88 CID univoci, 139 eventi dettagliati)  
**Autore:** Gemini  
**Branch di riferimento:** `gelma`  

---

## 1. Quadro Generale & Classificazione dei Difetti

L'analisi approfondita di tutti gli 88 file JSON presenti nella directory `CoverityReport/` evidenzia la seguente distribuzione:
- **Core Window Maker (`src/`)**: 37 CID
- **Toolkit WINGs (`WINGs/`)**: 23 CID
- **Libreria Grafica wrlib (`wrlib/`)**: 3 CID
- **Utility a riga di comando (`util/`)**: 15 CID
- **Pannello di configurazione (`WPrefs.app/`)**: 9 CID
- **Libreria legacy wmlib (`wmlib/`)**: 1 CID

### Sintesi per Categoria di Rischio

| Categoria | Descrizione Principale | Totale CID | Verdetto Ingegneristico |
|---|---|---|---|
| **Memory Safety & UAF** | Use-After-Free, buffer overrun, puntatori non inizializzati | 7 | **Massima priorità (Fix immediato obbligatorio)** |
| **Resource Leaks** | Mancata deallocazione memoria e proplist | 17 | **Priorità alta (Stabilità e tenuta a lungo termine)** |
| **Null Pointer Dereference** | Dereferenziazioni di puntatori non verificati o post-check | 20 | **Priorità alta (Prevenzione crash su casi limite/OOM)** |
| **Insecure Data & Integers** | Underflow/overflow interi, loop infiniti, bound non validati | 13 | **Priorità media (Robustezza logica e bounds checking)** |
| **Error Handling** | Return value ignorati su I/O e librerie di sistema | 10 | **Priorità media (Hardening sistemistico)** |
| **Dead Code & Maintainability** | Istruzioni irraggiungibili, assegnazioni morte, divisioni int | 9 | **Priorità bassa (Pulizia e igiene del codice)** |
| **False Positives / TOCTOU** | Falsi allarmi (`rand()` grafico, `access()` su sentinelle) | 12 | **Nessuna azione richiesta (Documentati e catalogati)** |

---

## 2. Core Window Maker (`src/`)

### CID 240602: Resource leak in `argv` (/src/dock.c)
- **CID Coverity**: `240602`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/src/dock.c:3172`
- **Verdetto**: **BUG REALE (Memory Leak in early-exit)**
- **Analisi Tecnica**: In `src/dock.c:3167-3172`, se `wtokensplit(cmdline, &argv, &argc)` produce `argc == 0`, il blocco `if (!argc)` effettua `return 0` liberando `cmdline` e `state`, ma dimentica di liberare il vettore `argv` appena allocato da `wtokensplit`.
- **Piano di Azione Correttiva**: Aggiungere `wtokenfree(argv, argc);` prima dell'istruzione `return 0;` a riga 3172.

### CID 1255978: Operands don't affect result (/src/xmodifier.c)
- **CID Coverity**: `1255978`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/xmodifier.c:210, /src/xmodifier.c:207`
- **Verdetto**: **BUG REALE (Logica macro errata)**
- **Analisi Tecnica**: Nella macro `store_modifier` in `src/xmodifier.c:155-162`, l'espressione `modifier_index == mod_bit && *(old_mod) != mod_bit` (quando invocata ad es. con `store_modifier("Alt_L", &alt_bit)`) si espande letteralmente in `modifier_index == alt_bit && alt_bit != alt_bit`. Poiché `alt_bit != alt_bit` è tautologicamente sempre falso, il controllo di avviso di collisione modificatore (`modwarn`) è codice morto che non scatterà mai.
- **Piano di Azione Correttiva**: Riscrivere la logica memorizzando il valore iniziale o controllando esplicitamente se il tasto entra in conflitto prima dell'aggiornamento: `if (modifier_index == alt_bit && old_mod_val != alt_bit)`.

### CID 1255982: Operands don't affect result (/src/xmodifier.c)
- **CID Coverity**: `1255982`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/xmodifier.c:198, /src/xmodifier.c:195`
- **Verdetto**: **BUG REALE (Logica macro errata)**
- **Analisi Tecnica**: Nella macro `store_modifier` in `src/xmodifier.c:155-162`, l'espressione `modifier_index == mod_bit && *(old_mod) != mod_bit` (quando invocata ad es. con `store_modifier("Alt_L", &alt_bit)`) si espande letteralmente in `modifier_index == alt_bit && alt_bit != alt_bit`. Poiché `alt_bit != alt_bit` è tautologicamente sempre falso, il controllo di avviso di collisione modificatore (`modwarn`) è codice morto che non scatterà mai.
- **Piano di Azione Correttiva**: Riscrivere la logica memorizzando il valore iniziale o controllando esplicitamente se il tasto entra in conflitto prima dell'aggiornamento: `if (modifier_index == alt_bit && old_mod_val != alt_bit)`.

### CID 1255987: Operands don't affect result (/src/xmodifier.c)
- **CID Coverity**: `1255987`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/xmodifier.c:201, /src/xmodifier.c:204`
- **Verdetto**: **BUG REALE (Logica macro errata)**
- **Analisi Tecnica**: Nella macro `store_modifier` in `src/xmodifier.c:155-162`, l'espressione `modifier_index == mod_bit && *(old_mod) != mod_bit` (quando invocata ad es. con `store_modifier("Alt_L", &alt_bit)`) si espande letteralmente in `modifier_index == alt_bit && alt_bit != alt_bit`. Poiché `alt_bit != alt_bit` è tautologicamente sempre falso, il controllo di avviso di collisione modificatore (`modwarn`) è codice morto che non scatterà mai.
- **Piano di Azione Correttiva**: Riscrivere la logica memorizzando il valore iniziale o controllando esplicitamente se il tasto entra in conflitto prima dell'aggiornamento: `if (modifier_index == alt_bit && old_mod_val != alt_bit)`.

### CID 1255992: Operands don't affect result (/src/xmodifier.c)
- **CID Coverity**: `1255992`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/xmodifier.c:189, /src/xmodifier.c:192`
- **Verdetto**: **BUG REALE (Logica macro errata)**
- **Analisi Tecnica**: Nella macro `store_modifier` in `src/xmodifier.c:155-162`, l'espressione `modifier_index == mod_bit && *(old_mod) != mod_bit` (quando invocata ad es. con `store_modifier("Alt_L", &alt_bit)`) si espande letteralmente in `modifier_index == alt_bit && alt_bit != alt_bit`. Poiché `alt_bit != alt_bit` è tautologicamente sempre falso, il controllo di avviso di collisione modificatore (`modwarn`) è codice morto che non scatterà mai.
- **Piano di Azione Correttiva**: Riscrivere la logica memorizzando il valore iniziale o controllando esplicitamente se il tasto entra in conflitto prima dell'aggiornamento: `if (modifier_index == alt_bit && old_mod_val != alt_bit)`.

### CID 1686033: Dereference after null check in `icon.c` (/src/icon.c)
- **CID Coverity**: `1686033`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/icon.c:663`
- **Verdetto**: **FALSO POSITIVO / GUARDIA RIDONDANTE**
- **Analisi Tecnica**: In `src/icon.c:663`, a riga 657 `if (icon && icon->owner)` induce Coverity a ritenere che `icon` possa essere NULL alle righe successive (663, 670, ecc.). `wIconUpdate` è in realtà un'API interna che richiede un'istanza valida di `icon`.
- **Piano di Azione Correttiva**: Aggiungere all'inizio della funzione `if (!icon) return;` per chiarire il contratto all'analizzatore.

### CID 1686036: Calling risky function `rand` (/src/superfluous.c)
- **CID Coverity**: `1686036`
- **Impatto / Severità**: `Low`
- **Posizione nel sorgente**: `/src/superfluous.c:90, /src/superfluous.c:84, /src/superfluous.c:89`
- **Verdetto**: **FALSO POSITIVO (Uso non crittografico)**
- **Analisi Tecnica**: Coverity segnala l'uso di `rand()` per calcolare offset di posizionamento casuale delle finestre (`placement.c`), passaggi di animazione miniature (`actions.c`) ed effetti visuali ghost (`superfluous.c`). Tali contesti non hanno alcun requisito di sicurezza crittografica.
- **Piano di Azione Correttiva**: Nessuna azione di sicurezza richiesta. Documentare il falso positivo.

### CID 1686042: Time of check time of use in `monitor.c` (/src/monitor.c)
- **CID Coverity**: `1686042`
- **Impatto / Severità**: `Low`
- **Posizione nel sorgente**: `/src/monitor.c:160`
- **Verdetto**: **FALSO POSITIVO (Polling su file sentinella di uscita)**
- **Analisi Tecnica**: In `src/monitor.c:160`, `access(exit_sentinel, F_OK)` è utilizzato in un ciclo di monitoraggio per rilevare quando il processo deve arrestarsi.
- **Piano di Azione Correttiva**: Nessuna azione richiesta, pattern di polling standard per file sentinella.

### CID 1686047: Structurally dead code / unreachable loop increment (/src/stacking.c)
- **CID Coverity**: `1686047`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/stacking.c:493`
- **Verdetto**: **DIFETTO REALE (Codice morto strutturale)**
- **Analisi Tecnica**: In `src/stacking.c:493`, il ciclo `for (above = WMBagFirst(...); above != NULL; above = WMBagNext(...))` contiene un corpo che esce sempre tramite `return` o `break` al primo passaggio, rendendo l'espressione di incremento `above = WMBagNext(...)` totalmente irraggiungibile.
- **Piano di Azione Correttiva**: Sostituire il costrutto `for` con una singola query o riordinare la logica del ciclo per riflettere l'intento reale.

### CID 1686048: Resource leak in `shortcut` modifier parsing (/src/misc.c)
- **CID Coverity**: `1686048`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/src/misc.c:910`
- **Verdetto**: **BUG REALE (Memory leak in errore di parsing)**
- **Analisi Tecnica**: In `src/misc.c:908-911`, quando `wXModifierFromKey(text)` fallisce restituendo un valore negativo, la funzione dealloca `buffer` ed esce con `return wstrdup("bug")`, dimenticando di deallocare `tmp` (allocato a riga 899 con `wstrdup(shortcut)`).
- **Piano di Azione Correttiva**: Aggiungere `wfree(tmp);` prima del return di errore a riga 910.

### CID 1686054: Null dereference in switchpanel (/src/switchpanel.c)
- **CID Coverity**: `1686054`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/switchpanel.c:188`
- **Verdetto**: **DIFETTO REALE (Dereferenziazione puntatore nullo)**
- **Analisi Tecnica**: In `src/switchpanel.c:188`, `icon` (puntatore a `WIcon`) può risultare NULL per client che non possiedono un'icona associata, e viene passato a `WMSetFrameRelief(icon, ...)` provocando un potenziale crash.
- **Piano di Azione Correttiva**: Aggiungere il controllo difensivo `if (icon) WMSetFrameRelief(icon, ...);`.

### CID 1686058: Buffer Overrun & Unterminated String (/src/event.c)
- **CID Coverity**: `1686058`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/src/event.c:401`
- **Verdetto**: **BUG REALE CRITICO (Memory Safety / Heap & Stack Overrun)**
- **Analisi Tecnica**: In `src/event.c:395-401`, il campo `pevent->name` letto dalla coda inotify all'interno di `buff` non ha la garanzia di essere terminato da un byte nullo `\0`. Il successivo ciclo con `strcmp(fname, allowed[i])` può correre oltre la fine dell'array `buff` (1344 byte) causando overrun in lettura e potenziale crash o comportamento indefinito.
- **Piano di Azione Correttiva**: Garantire esplicitamente la presenza del terminatore forzando `buff[i + sizeof(struct inotify_event) + pevent->len - 1] = '\0'` prima di referenziare `fname`, oppure confrontare usando `strncmp` limitato a `pevent->len` verificando che `pevent->len == strlen(allowed[i])`.

### CID 1686060: Reverse Inull in `wsmap.c` (/src/wsmap.c)
- **CID Coverity**: `1686060`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/wsmap.c:116`
- **Verdetto**: **INCONSISTENZA DI CODIFICA (Controllo post-dereferenziazione)**
- **Analisi Tecnica**: In `src/wsmap.c:116`, `wsmap->win` viene dereferenziato prima del controllo di nullità.
- **Piano di Azione Correttiva**: Rimuovere il controllo ridondante a valle o anticiparlo ad inizio procedura.

### CID 1686061: Unchecked library/system return (/src/startup.c)
- **CID Coverity**: `1686061`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/startup.c:832`
- **Verdetto**: **HARDENING DI SISTEMA (Return value ignorato)**
- **Analisi Tecnica**: Chiamate di sistema o di libreria (`fcntl` in startup.c, `poll` in misc.c, `XGrabPointer` in wsmap.c, `rename`/`mkdir` in telemetry.c) eseguite senza verifica del valore di ritorno.
- **Piano di Azione Correttiva**: Aggiungere controllo esplicito con fallback/log d'errore o cast a `(void)` per le operazioni intrinsecamente best-effort.

### CID 1686062: Unchecked library/system return (/src/misc.c)
- **CID Coverity**: `1686062`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/misc.c:474`
- **Verdetto**: **HARDENING DI SISTEMA (Return value ignorato)**
- **Analisi Tecnica**: Chiamate di sistema o di libreria (`fcntl` in startup.c, `poll` in misc.c, `XGrabPointer` in wsmap.c, `rename`/`mkdir` in telemetry.c) eseguite senza verifica del valore di ritorno.
- **Piano di Azione Correttiva**: Aggiungere controllo esplicito con fallback/log d'errore o cast a `(void)` per le operazioni intrinsecamente best-effort.

### CID 1686063: Unused value assignment (/src/defaults.c)
- **CID Coverity**: `1686063`
- **Impatto / Severità**: `Low`
- **Posizione nel sorgente**: `/src/defaults.c:1157, /src/defaults.c:1118`
- **Verdetto**: **PULIZIA CODICE (Assegnazione morta)**
- **Analisi Tecnica**: Assegnazioni `dict = NULL` in `defaults.c` e `lpath = NULL` in `rootmenu.c` che vengono immediatamente sovrascritte prima di essere lette.
- **Piano di Azione Correttiva**: Eliminare l'assegnazione iniziale ridondante.

### CID 1686075: Resource leak in `fixLeaderProperties` (/src/window.c)
- **CID Coverity**: `1686075`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/src/window.c:1007`
- **Verdetto**: **BUG REALE (Memory leak sovrascrittura puntatore)**
- **Analisi Tecnica**: In `src/window.c:1004-1009`, `PropGetWMClass(wwin->main_window, &class, &instance)` alloca memoria per `class`. Se `class[0] == '\0'`, la variabile viene riassegnata con `class = strdup(...)` senza prima liberare la stringa vuota allocata da `PropGetWMClass`.
- **Piano di Azione Correttiva**: Inserire `if (class) free(class);` prima di riassegnare `class = strdup(...)`.

### CID 1686076: Unchecked library/system return (/src/startup.c)
- **CID Coverity**: `1686076`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/startup.c:560, /src/startup.c:562, /src/startup.c:563, /src/startup.c:561`
- **Verdetto**: **HARDENING DI SISTEMA (Return value ignorato)**
- **Analisi Tecnica**: Chiamate di sistema o di libreria (`fcntl` in startup.c, `poll` in misc.c, `XGrabPointer` in wsmap.c, `rename`/`mkdir` in telemetry.c) eseguite senza verifica del valore di ritorno.
- **Piano di Azione Correttiva**: Aggiungere controllo esplicito con fallback/log d'errore o cast a `(void)` per le operazioni intrinsecamente best-effort.

### CID 1686078: Null return dereference in `texture.c` (/src/texture.c)
- **CID Coverity**: `1686078`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/texture.c:365`
- **Verdetto**: **DIFETTO REALE (Mancato controllo ritorno RCreateImage)**
- **Analisi Tecnica**: In `src/texture.c:365`, `image` creato da `RCreateImage` o `wTextureRenderImage` può essere NULL in scenari OOM e viene passato a `RClearImage(image, ...)` causando SIGSEGV.
- **Piano di Azione Correttiva**: Aggiungere `if (!image) return NULL;` prima di operare sul buffer immagine.

### CID 1686079: Null return dereference in `defaults.c` (/src/defaults.c)
- **CID Coverity**: `1686079`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/defaults.c:3602`
- **Verdetto**: **DIFETTO REALE (Mancato controllo ritorno RCreateImage)**
- **Analisi Tecnica**: In `src/defaults.c:3602`, `img` può risultare NULL e viene passato direttamente a `RCopyArea`.
- **Piano di Azione Correttiva**: Verificare `if (!img) return;` prima della copia.

### CID 1686080: Dereference null pointer in `aicon->icon` (/src/dock.c)
- **CID Coverity**: `1686080`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/dock.c:3600, /src/dock.c:3655`
- **Verdetto**: **BUG REALE (Possibile crash per dereferenziazione NULL)**
- **Analisi Tecnica**: In `src/dock.c:3600` e `3655`, `aicon->icon` viene dereferenziato (`aicon->icon->selected`, `aicon->icon->owner`). Sebbene la funzione sia chiamata per icone della dock, in casi di icone in transizione, drawer o icone surrogate `aicon->icon` può essere NULL (a riga 3555 il codice contiene infatti un controllo esplicito difensivo).
- **Piano di Azione Correttiva**: Proteggere gli accessi con guardia: `entry->flags.indicator_on = (aicon->icon ? aicon->icon->selected : 0);` e `if (aicon->icon && aicon->icon->owner) ...`.

### CID 1686081: Copy-paste error flag in `WCHANGE_STATE` (/src/dock.c)
- **CID Coverity**: `1686081`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/dock.c:221, /src/appicon.c:574`
- **Verdetto**: **FALSO POSITIVO COVERITY (Transizione Modale Intenzionale)**
- **Analisi Tecnica**: In `src/dock.c:221` e `src/appicon.c:574`, Coverity nota `WCHANGE_STATE(WSTATE_MODAL)` seguito decine di righe dopo da `WCHANGE_STATE(WSTATE_NORMAL)` (riga 273/609) e deduce che `WSTATE_NORMAL` sia il valore originale clonato per errore. In realtà si tratta di un pattern modale canonico: il window manager entra nello stato modale per visualizzare un popup di conferma e, una volta chiuso, ripristina lo stato normale.
- **Piano di Azione Correttiva**: Nessuna modifica necessaria. Aggiungere un commento esplicativo per documentare che la transizione è voluta.

### CID 1686083: Dereference null return value in `dock.c` (/src/dock.c)
- **CID Coverity**: `1686083`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/dock.c:1675`
- **Verdetto**: **DIFETTO REALE (Mancata validazione dizionario proplist)**
- **Analisi Tecnica**: In `src/dock.c:1675`, `WMGetFromPLDictionary(old_state, tmp)` viene passato direttamente a funzioni proplist senza verificare se restituisce NULL (possibile su file di stato corrotti o creati da versioni precedenti).
- **Piano di Azione Correttiva**: Assegnare il risultato a variabile temporanea e verificare `if (prop) ...` prima dell'uso.

### CID 1686084: Null return dereference in `workspace.c` (/src/workspace.c)
- **CID Coverity**: `1686084`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/workspace.c:950`
- **Verdetto**: **DIFETTO REALE (Mancata validazione stringa proplist)**
- **Analisi Tecnica**: In `src/workspace.c:950`, un elemento estratto dall'array proplist dei nomi workspace può essere NULL o non-stringa se la configurazione è corrotta, provocando crash in `WMGetFromPLString(pstr)`.
- **Piano di Azione Correttiva**: Verificare `if (!pstr || !WMIsPLString(pstr)) continue;` nel loop di parsing.

### CID 1686085: Calling risky function `rand` (/src/placement.c)
- **CID Coverity**: `1686085`
- **Impatto / Severità**: `Low`
- **Posizione nel sorgente**: `/src/placement.c:522, /src/placement.c:523`
- **Verdetto**: **FALSO POSITIVO (Uso non crittografico)**
- **Analisi Tecnica**: Coverity segnala l'uso di `rand()` per calcolare offset di posizionamento casuale delle finestre (`placement.c`), passaggi di animazione miniature (`actions.c`) ed effetti visuali ghost (`superfluous.c`). Tali contesti non hanno alcun requisito di sicurezza crittografica.
- **Piano di Azione Correttiva**: Nessuna azione di sicurezza richiesta. Documentare il falso positivo.

### CID 1686088: Unchecked library/system return (/src/wsmap.c)
- **CID Coverity**: `1686088`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/wsmap.c:487`
- **Verdetto**: **HARDENING DI SISTEMA (Return value ignorato)**
- **Analisi Tecnica**: Chiamate di sistema o di libreria (`fcntl` in startup.c, `poll` in misc.c, `XGrabPointer` in wsmap.c, `rename`/`mkdir` in telemetry.c) eseguite senza verifica del valore di ritorno.
- **Piano di Azione Correttiva**: Aggiungere controllo esplicito con fallback/log d'errore o cast a `(void)` per le operazioni intrinsecamente best-effort.

### CID 1686091: Null dereference in `wMenuUnmap` (/src/menu.c)
- **CID Coverity**: `1686091`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/menu.c:1226`
- **Verdetto**: **DIFETTO REALE (Dereferenziazione sottomenu)**
- **Analisi Tecnica**: In `src/menu.c:1226`, viene passato un sottomenu a `wMenuUnmap` senza verificare se esso o la sua struttura interna sia effettivamente mappata e istanziata.
- **Piano di Azione Correttiva**: Aggiungere `if (menu && menu->flags.mapped) wMenuUnmap(menu);`.

### CID 1686092: Logically dead code in event handler (/src/event.c)
- **CID Coverity**: `1686092`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/event.c:1975`
- **Verdetto**: **PULIZIA CODICE (Istruzione morta)**
- **Analisi Tecnica**: In `src/event.c:1975`, l'istruzione `WMFreeArray(scr->shortcutWindows)` si trova collocata dopo un'uscita incondizionata o in un blocco irraggiungibile.
- **Piano di Azione Correttiva**: Rimuovere l'istruzione morta o collocarla nella routine di shutdown corretta.

### CID 1686093: Unchecked library/system return (/src/telemetry.c)
- **CID Coverity**: `1686093`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/telemetry.c:276`
- **Verdetto**: **HARDENING DI SISTEMA (Return value ignorato)**
- **Analisi Tecnica**: Chiamate di sistema o di libreria (`fcntl` in startup.c, `poll` in misc.c, `XGrabPointer` in wsmap.c, `rename`/`mkdir` in telemetry.c) eseguite senza verifica del valore di ritorno.
- **Piano di Azione Correttiva**: Aggiungere controllo esplicito con fallback/log d'errore o cast a `(void)` per le operazioni intrinsecamente best-effort.

### CID 1686095: Unused value assignment (/src/rootmenu.c)
- **CID Coverity**: `1686095`
- **Impatto / Severità**: `Low`
- **Posizione nel sorgente**: `/src/rootmenu.c:675`
- **Verdetto**: **PULIZIA CODICE (Assegnazione morta)**
- **Analisi Tecnica**: Assegnazioni `dict = NULL` in `defaults.c` e `lpath = NULL` in `rootmenu.c` che vengono immediatamente sovrascritte prima di essere lette.
- **Piano di Azione Correttiva**: Eliminare l'assegnazione iniziale ridondante.

### CID 1686096: Null dereference in `winspector.c` (/src/winspector.c)
- **CID Coverity**: `1686096`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/winspector.c:1210, /src/winspector.c:1207`
- **Verdetto**: **DIFETTO REALE (Dereferenziazione pannello nullo)**
- **Analisi Tecnica**: In `src/winspector.c:1207, 1210`, `panel->appFrm` viene dereferenziato all'interno di `changePage` quando l'ispettore viene aperto per finestre che non appartengono ad alcuna applicazione di gruppo.
- **Piano di Azione Correttiva**: Aggiungere verifica di nullità `if (panel->appFrm) ...` prima di accedere ai suoi sotto-elementi.

### CID 1686099: Unchecked library/system return (/src/telemetry.c)
- **CID Coverity**: `1686099`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/telemetry.c:297`
- **Verdetto**: **HARDENING DI SISTEMA (Return value ignorato)**
- **Analisi Tecnica**: Chiamate di sistema o di libreria (`fcntl` in startup.c, `poll` in misc.c, `XGrabPointer` in wsmap.c, `rename`/`mkdir` in telemetry.c) eseguite senza verifica del valore di ritorno.
- **Piano di Azione Correttiva**: Aggiungere controllo esplicito con fallback/log d'errore o cast a `(void)` per le operazioni intrinsecamente best-effort.

### CID 1686104: Resource leak in `wScreenInit` failure path (/src/screen.c)
- **CID Coverity**: `1686104`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/src/screen.c:751`
- **Verdetto**: **BUG REALE (Memory leak in path di abort)**
- **Analisi Tecnica**: In `src/screen.c:746-751`, in caso di fallimento della creazione di `scr->wmscreen`, il salto all'etichetta `abort_no_context` libera vari campi e infine `scr`, ma omette di deallocare `scr->randr_state` allocato in precedenza.
- **Piano di Azione Correttiva**: Aggiungere `if (scr->randr_state) free(scr->randr_state);` prima di `wfree(scr)` all'etichetta di abort.

### CID 1686105: Dereference null return value in `dock.c` (/src/dock.c)
- **CID Coverity**: `1686105`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/dock.c:5004, /src/dock.c:4954`
- **Verdetto**: **DIFETTO REALE (Mancata validazione dizionario e stringhe proplist)**
- **Analisi Tecnica**: In `src/dock.c:4954` e `5004`, `value` e `dock_state` sono restituiti da chiamate `WMGetFromPLDictionary` e dereferenziati senza verificare che non siano NULL.
- **Piano di Azione Correttiva**: Aggiungere verifiche difensive `if (value && WMIsPLString(value))` e `if (dock_state && WMIsPLDictionary(dock_state))`.

### CID 1686107: Reverse Inull in `session.c` (/src/session.c)
- **CID Coverity**: `1686107`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/src/session.c:276`
- **Verdetto**: **INCONSISTENZA DI CODIFICA (Controllo post-dereferenziazione)**
- **Analisi Tecnica**: In `src/session.c:276`, `command` viene dereferenziato incondizionatamente decine di righe prima e poi controllato con `if (command != NULL)`. Se potesse essere nullo, il programma sarebbe già crashato.
- **Piano di Azione Correttiva**: Anticipare il controllo `if (!command) return;` in cima alla funzione di caricamento comandi sessione.

### CID 1686108: Calling risky function `rand` (/src/actions.c)
- **CID Coverity**: `1686108`
- **Impatto / Severità**: `Low`
- **Posizione nel sorgente**: `/src/actions.c:1355`
- **Verdetto**: **FALSO POSITIVO (Uso non crittografico)**
- **Analisi Tecnica**: Coverity segnala l'uso di `rand()` per calcolare offset di posizionamento casuale delle finestre (`placement.c`), passaggi di animazione miniature (`actions.c`) ed effetti visuali ghost (`superfluous.c`). Tali contesti non hanno alcun requisito di sicurezza crittografica.
- **Piano di Azione Correttiva**: Nessuna azione di sicurezza richiesta. Documentare il falso positivo.

### CID 1686112: Buffer Overrun & Unterminated String (/src/event.c)
- **CID Coverity**: `1686112`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/src/event.c:401, /src/event.c:401`
- **Verdetto**: **BUG REALE CRITICO (Memory Safety / Heap & Stack Overrun)**
- **Analisi Tecnica**: In `src/event.c:395-401`, il campo `pevent->name` letto dalla coda inotify all'interno di `buff` non ha la garanzia di essere terminato da un byte nullo `\0`. Il successivo ciclo con `strcmp(fname, allowed[i])` può correre oltre la fine dell'array `buff` (1344 byte) causando overrun in lettura e potenziale crash o comportamento indefinito.
- **Piano di Azione Correttiva**: Garantire esplicitamente la presenza del terminatore forzando `buff[i + sizeof(struct inotify_event) + pevent->len - 1] = '\0'` prima di referenziare `fname`, oppure confrontare usando `strncmp` limitato a `pevent->len` verificando che `pevent->len == strlen(allowed[i])`.

## 2. Toolkit WINGs (`WINGs/`)

### CID 1686031: Overflowed integer argument in `wsplitview.c` (/WINGs/wsplitview.c)
- **CID Coverity**: `1686031`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/WINGs/wsplitview.c:484`
- **Verdetto**: **DIFETTO DI VALIDAZIONE (Possibile underflow nei calcoli di splitview)**
- **Analisi Tecnica**: In `WINGs/wsplitview.c:363, 484`, la differenza tra la dimensione della vista e `getTotalSize(...)` può diventare negativa se la finestra viene ridimensionata al di sotto dei vincoli minimi, provocando un underflow quando convertita a unsigned o passata a funzioni di dimensionamento.
- **Piano di Azione Correttiva**: Applicare una clausola di clamp esplicita: `if (delta < 0) delta = 0;` prima di propagare la nuova dimensione.

### CID 1686032: Null check inconsistencies in `wtext.c` (/WINGs/wtext.c)
- **CID Coverity**: `1686032`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/WINGs/wtext.c:1194`
- **Verdetto**: **DIFETTO REALE (Dereferenziazione TextBlock nullo)**
- **Analisi Tecnica**: In `WINGs/wtext.c:1047-1053` e `1194`, `tb` viene dereferenziato prima o dopo controlli di nullità parziali, segnalando che il blocco di testo corrente può essere NULL se il widget è vuoto.
- **Piano di Azione Correttiva**: Uniformare i controlli difensivi proteggendo sistematicamente gli accessi a `tb` quando il widget non contiene testo.

### CID 1686035: Unchecked library return in `wcolorpanel.c` (/WINGs/wcolorpanel.c)
- **CID Coverity**: `1686035`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/WINGs/wcolorpanel.c:3122`
- **Verdetto**: **HARDENING (Return value di remove ignorato)**
- **Analisi Tecnica**: In `WINGs/wcolorpanel.c:3122`, `remove(toPath)` viene invocato senza verificare se la cancellazione del file temporaneo ha avuto successo.
- **Piano di Azione Correttiva**: Verificare il valore di ritorno ed emettere un warning in caso di errore di cancellazione.

### CID 1686038: Overflowed integer argument in `wsplitview.c` (/WINGs/wsplitview.c)
- **CID Coverity**: `1686038`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/WINGs/wsplitview.c:363`
- **Verdetto**: **DIFETTO DI VALIDAZIONE (Possibile underflow nei calcoli di splitview)**
- **Analisi Tecnica**: In `WINGs/wsplitview.c:363, 484`, la differenza tra la dimensione della vista e `getTotalSize(...)` può diventare negativa se la finestra viene ridimensionata al di sotto dei vincoli minimi, provocando un underflow quando convertita a unsigned o passata a funzioni di dimensionamento.
- **Piano di Azione Correttiva**: Applicare una clausola di clamp esplicita: `if (delta < 0) delta = 0;` prima di propagare la nuova dimensione.

### CID 1686041: Time of check time of use in WINGs (/WINGs/wcolorpanel.c)
- **CID Coverity**: `1686041`
- **Impatto / Severità**: `Low`
- **Posizione nel sorgente**: `/WINGs/wcolorpanel.c:3028`
- **Verdetto**: **FALSO POSITIVO (Verifica file locale standard)**
- **Analisi Tecnica**: Chiamate `stat` o `access` prima di scrivere file proplist o palette colori nella home dell'utente. In un contesto desktop a singolo utente non costituiscono minaccia di escalation di privilegi.
- **Piano di Azione Correttiva**: Nessuna azione urgente richiesta; passare a `open(..., O_CREAT | O_EXCL)` per le nuove implementazioni.

### CID 1686050: Overflowed constant in `dragsource.c` (/WINGs/dragsource.c)
- **CID Coverity**: `1686050`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/WINGs/dragsource.c:736`
- **Verdetto**: **DIFETTO DI UNDERFLOW (Unsigned underflow in ciclo)**
- **Analisi Tecnica**: In `WINGs/dragsource.c:736`, `for (i = nchildren - 1; i >= 0; i--)` dove `nchildren` è di tipo unsigned e può valere 0. `0U - 1` produce `4294967295U`, che assegnato a `int i` causa overflow / loop anomalo.
- **Piano di Azione Correttiva**: Condizionare il loop: `if (nchildren > 0) { for (int i = (int)nchildren - 1; i >= 0; i--) { ... } }`.

### CID 1686053: Dereference null return value in `wcolorpanel.c` (/WINGs/wcolorpanel.c)
- **CID Coverity**: `1686053`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/WINGs/wcolorpanel.c:2808`
- **Verdetto**: **DIFETTO REALE (Dereferenziazione spettro nullo)**
- **Analisi Tecnica**: In `WINGs/wcolorpanel.c:2808`, la variabile `spectrum` viene dereferenziata senza verificare se la modalità attiva del pannello colori ha istanziato la relativa vista spettro.
- **Piano di Azione Correttiva**: Aggiungere la guardia `if (spectrum) ...`.

### CID 1686055: Use-After-Free in Red-Black Tree (/WINGs/bagtree.c)
- **CID Coverity**: `1686055`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/WINGs/bagtree.c:464`
- **Verdetto**: **BUG REALE CRITICO (Use After Free in WMBag / RB-Tree)**
- **Analisi Tecnica**: In `WINGs/bagtree.c:459-464`: `ptr = rbTreeDelete(self, ptr); ... wfree(ptr); wassertrv(self->count == 0 || self->root->index >= 0, 1);`. Quando il nodo rimosso è la radice precedente, l'asserzione dereferenzia `self->root->index` che potrebbe puntare alla memoria appena rilasciata da `wfree(ptr)` se `rbTreeDelete` non ha aggiornato consistentemente `self->root` prima del free.
- **Piano di Azione Correttiva**: Eseguire la verifica e l'eventuale asserzione prima di chiamare `wfree(ptr)`, oppure garantire che `self->root` punti al nodo sentinella `self->nil` prima dell'accesso.

### CID 1686057: Overflowed / negative integer in `wtextfield.c` (/WINGs/wtextfield.c)
- **CID Coverity**: `1686057`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/WINGs/wtextfield.c:199, /WINGs/wtextfield.c:199`
- **Verdetto**: **DIFETTO DI BOUNDS (Possibile indice negativo in TextField)**
- **Analisi Tecnica**: In `WINGs/wtextfield.c:199, 426, 1247`, le coordinate di visualizzazione `tPtr->viewPosition` e `tPtr->cursorPosition` possono assumere valori negativi a seguito di scorrimento veloce o backspace UTF-8 ripetuto senza verifica di zero inferiore.
- **Piano di Azione Correttiva**: Applicare un vincolo esplicito `if (tPtr->viewPosition < 0) tPtr->viewPosition = 0;` prima dell'invocazione di `WMWidthOfString` o `paintTextField`.

### CID 1686059: Overflowed / negative integer in `wtextfield.c` (/WINGs/wtextfield.c)
- **CID Coverity**: `1686059`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/WINGs/wtextfield.c:1247, /WINGs/wtextfield.c:1247`
- **Verdetto**: **DIFETTO DI BOUNDS (Possibile indice negativo in TextField)**
- **Analisi Tecnica**: In `WINGs/wtextfield.c:199, 426, 1247`, le coordinate di visualizzazione `tPtr->viewPosition` e `tPtr->cursorPosition` possono assumere valori negativi a seguito di scorrimento veloce o backspace UTF-8 ripetuto senza verifica di zero inferiore.
- **Piano di Azione Correttiva**: Applicare un vincolo esplicito `if (tPtr->viewPosition < 0) tPtr->viewPosition = 0;` prima dell'invocazione di `WMWidthOfString` o `paintTextField`.

### CID 1686064: Reverse Inull in `wfilepanel.c` (/WINGs/wfilepanel.c)
- **CID Coverity**: `1686064`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/WINGs/wfilepanel.c:652`
- **Verdetto**: **INCONSISTENZA DI CODIFICA (Controllo post-dereferenziazione)**
- **Analisi Tecnica**: In `WINGs/wfilepanel.c:652`, `file` viene dereferenziato a monte e poi controllato con `if (file)` a valle.
- **Piano di Azione Correttiva**: Anticipare il controllo `if (!file) return;` a monte.

### CID 1686065: Overflowed constant in `wcolorpanel.c` (/WINGs/wcolorpanel.c)
- **CID Coverity**: `1686065`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/WINGs/wcolorpanel.c:3554`
- **Verdetto**: **BUG REALE (Loop infinito per overflow di tipo)**
- **Analisi Tecnica**: In `WINGs/wcolorpanel.c:3554`, una variabile contatore `unsigned char i` utilizzata in `for (i = 0; i <= 255; i++)` non potrà mai superare 255, provocando un overflow ciclico a 0 e un loop infinito.
- **Piano di Azione Correttiva**: Dichiarare il contatore come `int i` o terminare con `i < 256`.

### CID 1686067: Explicit null dereferenced in `wscrollview.c` (/WINGs/wscrollview.c)
- **CID Coverity**: `1686067`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/WINGs/wscrollview.c:421`
- **Verdetto**: **DIFETTO REALE (Dereferenziazione scroller non presenti)**
- **Analisi Tecnica**: In `WINGs/wscrollview.c:421, 454`, all'interno di `reorganizeInterior`, i membri `sPtr->hScroller` o `sPtr->vScroller` vengono passati a funzioni di riconfigurazione senza verificare se sono abilitati o nulli.
- **Piano di Azione Correttiva**: Aggiungere guardie `if (sPtr->hScroller)` e `if (sPtr->vScroller)` prima di accedere alle geometrie dello scroller.

### CID 1686069: Time of check time of use in WINGs (/WINGs/wfilepanel.c)
- **CID Coverity**: `1686069`
- **Impatto / Severità**: `Low`
- **Posizione nel sorgente**: `/WINGs/wfilepanel.c:705`
- **Verdetto**: **FALSO POSITIVO (Verifica file locale standard)**
- **Analisi Tecnica**: Chiamate `stat` o `access` prima di scrivere file proplist o palette colori nella home dell'utente. In un contesto desktop a singolo utente non costituiscono minaccia di escalation di privilegi.
- **Piano di Azione Correttiva**: Nessuna azione urgente richiesta; passare a `open(..., O_CREAT | O_EXCL)` per le nuove implementazioni.

### CID 1686070: Null check inconsistencies in `wtext.c` (/WINGs/wtext.c)
- **CID Coverity**: `1686070`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/WINGs/wtext.c:1047, /WINGs/wtext.c:1048, /WINGs/wtext.c:1053`
- **Verdetto**: **DIFETTO REALE (Dereferenziazione TextBlock nullo)**
- **Analisi Tecnica**: In `WINGs/wtext.c:1047-1053` e `1194`, `tb` viene dereferenziato prima o dopo controlli di nullità parziali, segnalando che il blocco di testo corrente può essere NULL se il widget è vuoto.
- **Piano di Azione Correttiva**: Uniformare i controlli difensivi proteggendo sistematicamente gli accessi a `tb` quando il widget non contiene testo.

### CID 1686071: Overflowed integer argument in `error.c` (/WINGs/error.c)
- **CID Coverity**: `1686071`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/WINGs/error.c:98, /WINGs/error.c:100`
- **Verdetto**: **DIFETTO DI BOUNDS (Potenziale underflow in strncat)**
- **Analisi Tecnica**: In `WINGs/error.c:98, 100`, il terzo argomento di `strncat(buf, ..., linemax - 1 - strlen(buf))` può andare in underflow (diventando un enorme `size_t` positivo) se la lunghezza corrente di `buf` supera `linemax - 1`.
- **Piano di Azione Correttiva**: Calcolare la lunghezza residua in modo sicuro: `size_t cur = strlen(buf); size_t rem = (cur + 1 < linemax) ? (linemax - 1 - cur) : 0; if (rem > 0) strncat(..., rem);`.

### CID 1686072: Use-After-Free in Text Widget (/WINGs/wtext.c)
- **CID Coverity**: `1686072`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/WINGs/wtext.c:1878, /WINGs/wtext.c:1878`
- **Verdetto**: **BUG REALE CRITICO (Use After Free in WMText)**
- **Analisi Tecnica**: In `WINGs/wtext.c:1874-1878`: `WMDestroyTextBlock(WMRemoveTextBlock(tPtr))` distrugge e dealloca il TextBlock corrente `tb`. A riga 1878, la clausola `if (sibling)` continua a dereferenziare campi di `tb` (`tb->used`, `tb->text`), operando su memoria già liberata.
- **Piano di Azione Correttiva**: Memorizzare i valori necessari in variabili automatiche prima di distruggere il blocco, e impostare `tb = NULL;` subito dopo la deallocazione.

### CID 1686074: Time of check time of use in WINGs (/WINGs/wcolorpanel.c)
- **CID Coverity**: `1686074`
- **Impatto / Severità**: `Low`
- **Posizione nel sorgente**: `/WINGs/wcolorpanel.c:3114`
- **Verdetto**: **FALSO POSITIVO (Verifica file locale standard)**
- **Analisi Tecnica**: Chiamate `stat` o `access` prima di scrivere file proplist o palette colori nella home dell'utente. In un contesto desktop a singolo utente non costituiscono minaccia di escalation di privilegi.
- **Piano di Azione Correttiva**: Nessuna azione urgente richiesta; passare a `open(..., O_CREAT | O_EXCL)` per le nuove implementazioni.

### CID 1686082: Overflowed / negative integer in `wtextfield.c` (/WINGs/wtextfield.c)
- **CID Coverity**: `1686082`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/WINGs/wtextfield.c:426`
- **Verdetto**: **DIFETTO DI BOUNDS (Possibile indice negativo in TextField)**
- **Analisi Tecnica**: In `WINGs/wtextfield.c:199, 426, 1247`, le coordinate di visualizzazione `tPtr->viewPosition` e `tPtr->cursorPosition` possono assumere valori negativi a seguito di scorrimento veloce o backspace UTF-8 ripetuto senza verifica di zero inferiore.
- **Piano di Azione Correttiva**: Applicare un vincolo esplicito `if (tPtr->viewPosition < 0) tPtr->viewPosition = 0;` prima dell'invocazione di `WMWidthOfString` o `paintTextField`.

### CID 1686097: Time of check time of use in WINGs (/WINGs/proplist.c)
- **CID Coverity**: `1686097`
- **Impatto / Severità**: `Low`
- **Posizione nel sorgente**: `/WINGs/proplist.c:1788`
- **Verdetto**: **FALSO POSITIVO (Verifica file locale standard)**
- **Analisi Tecnica**: Chiamate `stat` o `access` prima di scrivere file proplist o palette colori nella home dell'utente. In un contesto desktop a singolo utente non costituiscono minaccia di escalation di privilegi.
- **Piano di Azione Correttiva**: Nessuna azione urgente richiesta; passare a `open(..., O_CREAT | O_EXCL)` per le nuove implementazioni.

### CID 1686102: Time of check time of use in WINGs (/WINGs/proplist.c)
- **CID Coverity**: `1686102`
- **Impatto / Severità**: `Low`
- **Posizione nel sorgente**: `/WINGs/proplist.c:1865`
- **Verdetto**: **FALSO POSITIVO (Verifica file locale standard)**
- **Analisi Tecnica**: Chiamate `stat` o `access` prima di scrivere file proplist o palette colori nella home dell'utente. In un contesto desktop a singolo utente non costituiscono minaccia di escalation di privilegi.
- **Piano di Azione Correttiva**: Nessuna azione urgente richiesta; passare a `open(..., O_CREAT | O_EXCL)` per le nuove implementazioni.

### CID 1686106: Explicit null dereferenced in `wscrollview.c` (/WINGs/wscrollview.c)
- **CID Coverity**: `1686106`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/WINGs/wscrollview.c:454`
- **Verdetto**: **DIFETTO REALE (Dereferenziazione scroller non presenti)**
- **Analisi Tecnica**: In `WINGs/wscrollview.c:421, 454`, all'interno di `reorganizeInterior`, i membri `sPtr->hScroller` o `sPtr->vScroller` vengono passati a funzioni di riconfigurazione senza verificare se sono abilitati o nulli.
- **Piano di Azione Correttiva**: Aggiungere guardie `if (sPtr->hScroller)` e `if (sPtr->vScroller)` prima di accedere alle geometrie dello scroller.

### CID 1686111: Logically dead code in `dragsource.c` (/WINGs/dragsource.c)
- **CID Coverity**: `1686111`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/WINGs/dragsource.c:375`
- **Verdetto**: **PULIZIA CODICE (Codice morto)**
- **Analisi Tecnica**: In `WINGs/dragsource.c:375`, un'assegnazione di versione protocollo non può mai essere raggiunta a causa di un controllo di versione restrittivo all'inizio della funzione.
- **Piano di Azione Correttiva**: Rimuovere il ramo logico non raggiungibile.

## 2. Libreria Grafica wrlib (`wrlib/`)

### CID 1686030: Ignoring number of bytes read (/wrlib/load_webp.c)
- **CID Coverity**: `1686030`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/wrlib/load_webp.c:82`
- **Verdetto**: **DIFETTO REALE (Mancata validazione completezza lettura fread)**
- **Analisi Tecnica**: In `wrlib/load_webp.c:82` e `load_ppm.c:183`, `fread` viene verificato solo come valore booleano (`if (!fread(...))`). Se il file viene troncato a metà dell'header, la lettura parziale viene accettata procedendo con dati parziali o non inizializzati.
- **Piano di Azione Correttiva**: Confrontare esplicitamente il ritorno di `fread` con il numero esatto di elementi richiesti: `if (fread(buf, 1, count, f) != count) ...`.

### CID 1686077: Ignoring number of bytes read (/wrlib/load_ppm.c)
- **CID Coverity**: `1686077`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/wrlib/load_ppm.c:183`
- **Verdetto**: **DIFETTO REALE (Mancata validazione completezza lettura fread)**
- **Analisi Tecnica**: In `wrlib/load_webp.c:82` e `load_ppm.c:183`, `fread` viene verificato solo come valore booleano (`if (!fread(...))`). Se il file viene troncato a metà dell'header, la lettura parziale viene accettata procedendo con dati parziali o non inizializzati.
- **Piano di Azione Correttiva**: Confrontare esplicitamente il ritorno di `fread` con il numero esatto di elementi richiesti: `if (fread(buf, 1, count, f) != count) ...`.

### CID 1686110: Untrusted loop bound in `load_ppm.c` (/wrlib/load_ppm.c)
- **CID Coverity**: `1686110`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/wrlib/load_ppm.c:126`
- **Verdetto**: **DIFETTO DI VALIDAZIONE (Loop bound non validato)**
- **Analisi Tecnica**: In `wrlib/load_ppm.c:126`, un valore letto direttamente dal flusso dati PPM viene impiegato come limite superiore di un ciclo di scansione senza una previa verifica rispetto alla dimensione massima ammessa.
- **Piano di Azione Correttiva**: Aggiungere un controllo sul valore massimo ammesso prima di avviare il ciclo.

## 2. Utility e Script (`util/`)

### CID 1686034: Resource leak in utility CLI (/util/wmmenugen_parse_wmconfig.c)
- **CID Coverity**: `1686034`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/util/wmmenugen_parse_wmconfig.c:142`
- **Verdetto**: **PULIZIA MEMORIA (Memory Leak in tool generatore menu/config)**
- **Analisi Tecnica**: In `/util/wmmenugen_parse_wmconfig.c`, stringhe temporanee o oggetti proplist allocati con `WMCreatePLString` o `wtokensplit` non vengono rilasciati prima dell'uscita dalla funzione o al termine dell'elaborazione della voce.
- **Piano di Azione Correttiva**: Aggiungere le corrispondenti chiamate di deallocazione `WMReleasePropList` o `wfree` nei punti di uscita.

### CID 1686037: Use-After-Free in `wmgenmenu.c` (/util/wmgenmenu.c)
- **CID Coverity**: `1686037`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/util/wmgenmenu.c:413`
- **Verdetto**: **BUG REALE CRITICO (Use After Free in iterazione loop)**
- **Analisi Tecnica**: In `util/wmgenmenu.c:413, 459`, alla fine di ogni iterazione del ciclo `wtokenfree(argv, argc)` dealloca `argv`. All'inizio dell'iterazione successiva, se `list[i][1]` è vuoto o se `wtokensplit` non riassegna correttamente i puntatori, `argv[0]` fa riferimento alla memoria liberata al giro precedente.
- **Piano di Azione Correttiva**: Inizializzare `argv = NULL; argc = 0;` ad ogni iterazione del ciclo all'interno del blocco `while`.

### CID 1686039: Resource leak in utility CLI (/util/wmmenugen_parse_xdg.c)
- **CID Coverity**: `1686039`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/util/wmmenugen_parse_xdg.c:196, /util/wmmenugen_parse_xdg.c:196`
- **Verdetto**: **PULIZIA MEMORIA (Memory Leak in tool generatore menu/config)**
- **Analisi Tecnica**: In `/util/wmmenugen_parse_xdg.c`, stringhe temporanee o oggetti proplist allocati con `WMCreatePLString` o `wtokensplit` non vengono rilasciati prima dell'uscita dalla funzione o al termine dell'elaborazione della voce.
- **Piano di Azione Correttiva**: Aggiungere le corrispondenti chiamate di deallocazione `WMReleasePropList` o `wfree` nei punti di uscita.

### CID 1686043: Resource leak in utility CLI (/util/wmmenugen_parse_wmconfig.c)
- **CID Coverity**: `1686043`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/util/wmmenugen_parse_wmconfig.c:142`
- **Verdetto**: **PULIZIA MEMORIA (Memory Leak in tool generatore menu/config)**
- **Analisi Tecnica**: In `/util/wmmenugen_parse_wmconfig.c`, stringhe temporanee o oggetti proplist allocati con `WMCreatePLString` o `wtokensplit` non vengono rilasciati prima dell'uscita dalla funzione o al termine dell'elaborazione della voce.
- **Piano di Azione Correttiva**: Aggiungere le corrispondenti chiamate di deallocazione `WMReleasePropList` o `wfree` nei punti di uscita.

### CID 1686044: TOCTOU in utility (/util/wmmenugen.c)
- **CID Coverity**: `1686044`
- **Impatto / Severità**: `Low`
- **Posizione nel sorgente**: `/util/wmmenugen.c:160`
- **Verdetto**: **FALSO POSITIVO (Controllo parametri CLI)**
- **Analisi Tecnica**: Uso di `stat` per validare l'esistenza di file passati come argomenti da riga di comando prima dell'elaborazione.
- **Piano di Azione Correttiva**: Nessuna azione richiesta.

### CID 1686045: Null dereference in `util/xdnd.c` (/util/xdnd.c)
- **CID Coverity**: `1686045`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/util/xdnd.c:933`
- **Verdetto**: **DIFETTO REALE (Dereferenziazione puntatore nullo)**
- **Analisi Tecnica**: In `util/xdnd.c:933`, `typelist` nullo viene passato a `array_length` che ne dereferenzia i puntatori interni senza controllo preliminare.
- **Piano di Azione Correttiva**: Aggiungere verifica di validità `if (typelist) ...` prima di calcolarne la lunghezza.

### CID 1686049: Deadcode in utility (/util/wmsetbg.c)
- **CID Coverity**: `1686049`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/util/wmsetbg.c:565`
- **Verdetto**: **PULIZIA CODICE (Istruzione non raggiungibile)**
- **Analisi Tecnica**: Istruzioni di `break` ridondanti posizionate immediatamente dopo `return` o `exit` in `/util/wmsetbg.c`.
- **Piano di Azione Correttiva**: Rimuovere le istruzioni non raggiungibili.

### CID 1686051: Resource leak in utility CLI (/util/wmmenugen_parse_wmconfig.c)
- **CID Coverity**: `1686051`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/util/wmmenugen_parse_wmconfig.c:120, /util/wmmenugen_parse_wmconfig.c:124, /util/wmmenugen_parse_wmconfig.c:142, /util/wmmenugen_parse_wmconfig.c:122, /util/wmmenugen_parse_wmconfig.c:126`
- **Verdetto**: **PULIZIA MEMORIA (Memory Leak in tool generatore menu/config)**
- **Analisi Tecnica**: In `/util/wmmenugen_parse_wmconfig.c`, stringhe temporanee o oggetti proplist allocati con `WMCreatePLString` o `wtokensplit` non vengono rilasciati prima dell'uscita dalla funzione o al termine dell'elaborazione della voce.
- **Piano di Azione Correttiva**: Aggiungere le corrispondenti chiamate di deallocazione `WMReleasePropList` o `wfree` nei punti di uscita.

### CID 1686052: Resource leak in utility CLI (/util/wmmenugen_parse_xdg.c)
- **CID Coverity**: `1686052`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/util/wmmenugen_parse_xdg.c:196`
- **Verdetto**: **PULIZIA MEMORIA (Memory Leak in tool generatore menu/config)**
- **Analisi Tecnica**: In `/util/wmmenugen_parse_xdg.c`, stringhe temporanee o oggetti proplist allocati con `WMCreatePLString` o `wtokensplit` non vengono rilasciati prima dell'uscita dalla funzione o al termine dell'elaborazione della voce.
- **Piano di Azione Correttiva**: Aggiungere le corrispondenti chiamate di deallocazione `WMReleasePropList` o `wfree` nei punti di uscita.

### CID 1686056: Resource leak in utility CLI (/util/wmmenugen_parse_wmconfig.c)
- **CID Coverity**: `1686056`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/util/wmmenugen_parse_wmconfig.c:142`
- **Verdetto**: **PULIZIA MEMORIA (Memory Leak in tool generatore menu/config)**
- **Analisi Tecnica**: In `/util/wmmenugen_parse_wmconfig.c`, stringhe temporanee o oggetti proplist allocati con `WMCreatePLString` o `wtokensplit` non vengono rilasciati prima dell'uscita dalla funzione o al termine dell'elaborazione della voce.
- **Piano di Azione Correttiva**: Aggiungere le corrispondenti chiamate di deallocazione `WMReleasePropList` o `wfree` nei punti di uscita.

### CID 1686086: Deadcode in utility (/util/setstyle.c)
- **CID Coverity**: `1686086`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/util/setstyle.c:409`
- **Verdetto**: **PULIZIA CODICE (Istruzione non raggiungibile)**
- **Analisi Tecnica**: Istruzioni di `break` ridondanti posizionate immediatamente dopo `return` o `exit` in `/util/setstyle.c`.
- **Piano di Azione Correttiva**: Rimuovere le istruzioni non raggiungibili.

### CID 1686089: TOCTOU in utility (/util/setstyle.c)
- **CID Coverity**: `1686089`
- **Impatto / Severità**: `Low`
- **Posizione nel sorgente**: `/util/setstyle.c:440`
- **Verdetto**: **FALSO POSITIVO (Controllo parametri CLI)**
- **Analisi Tecnica**: Uso di `stat` per validare l'esistenza di file passati come argomenti da riga di comando prima dell'elaborazione.
- **Piano di Azione Correttiva**: Nessuna azione richiesta.

### CID 1686094: Resource leak in utility CLI (/util/wmgenmenu.c)
- **CID Coverity**: `1686094`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/util/wmgenmenu.c:488, /util/wmgenmenu.c:488`
- **Verdetto**: **PULIZIA MEMORIA (Memory Leak in tool generatore menu/config)**
- **Analisi Tecnica**: In `/util/wmgenmenu.c`, stringhe temporanee o oggetti proplist allocati con `WMCreatePLString` o `wtokensplit` non vengono rilasciati prima dell'uscita dalla funzione o al termine dell'elaborazione della voce.
- **Piano di Azione Correttiva**: Aggiungere le corrispondenti chiamate di deallocazione `WMReleasePropList` o `wfree` nei punti di uscita.

### CID 1686100: Resource leak in utility CLI (/util/wmgenmenu.c)
- **CID Coverity**: `1686100`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/util/wmgenmenu.c:419, /util/wmgenmenu.c:419, /util/wmgenmenu.c:444, /util/wmgenmenu.c:444`
- **Verdetto**: **PULIZIA MEMORIA (Memory Leak in tool generatore menu/config)**
- **Analisi Tecnica**: In `/util/wmgenmenu.c`, stringhe temporanee o oggetti proplist allocati con `WMCreatePLString` o `wtokensplit` non vengono rilasciati prima dell'uscita dalla funzione o al termine dell'elaborazione della voce.
- **Piano di Azione Correttiva**: Aggiungere le corrispondenti chiamate di deallocazione `WMReleasePropList` o `wfree` nei punti di uscita.

### CID 1686103: Uninitialized pointer read in `wmiv.c` (/util/wmiv.c)
- **CID Coverity**: `1686103`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/util/wmiv.c:1549`
- **Verdetto**: **BUG REALE (Free di puntatore non inizializzato)**
- **Analisi Tecnica**: In `util/wmiv.c:1549-1568`, se `scandir` fallisce restituendo errore o 0 voci, la variabile `dir` resta non inizializzata. A riga 1568 `free(dir)` tenta di liberare un puntatore con valore casuale sullo stack, causando crash immediato.
- **Piano di Azione Correttiva**: Inizializzare `struct dirent **dir = NULL;` alla dichiarazione della funzione `connect_dir`.

## 2. Pannello Preferenze (`WPrefs.app/`)

### CID 1686040: Null return dereference in `Appearance.c` (/WPrefs.app/Appearance.c)
- **CID Coverity**: `1686040`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/WPrefs.app/Appearance.c:591`
- **Verdetto**: **DIFETTO REALE (Dereferenziazione RImage nullo)**
- **Analisi Tecnica**: In `WPrefs.app/Appearance.c:591`, `image` creato per l'anteprima può essere NULL se la memoria è esaurita e viene passato a `RClearImage`.
- **Piano di Azione Correttiva**: Aggiungere `if (image) RClearImage(image, ...);`.

### CID 1686046: Negative array index read (/WPrefs.app/WindowHandling.c)
- **CID Coverity**: `1686046`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/WPrefs.app/WindowHandling.c:275, /WPrefs.app/Icons.c:461, /WPrefs.app/WindowHandling.c:263`
- **Verdetto**: **BUG REALE (Accesso array fuori limite con indice -1)**
- **Analisi Tecnica**: In `WPrefs.app/WindowHandling.c:263, 275` e `Icons.c:461`, la chiamata `WMGetPopUpButtonSelectedItem(...)` restituisce `-1` se nessun elemento è selezionato. Tale valore negativo viene utilizzato direttamente come indice di array (`window_placements[-1]`), provocando lettura di memoria non valida e crash dell'interfaccia WPrefs.
- **Piano di Azione Correttiva**: Assegnare l'indice a variabile locale e validare `int sel = WMGetPopUpButtonSelectedItem(...); if (sel >= 0) { ... }`.

### CID 1686068: Resource leak in WPrefs (/WPrefs.app/HotCornerShortcuts.c)
- **CID Coverity**: `1686068`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/WPrefs.app/HotCornerShortcuts.c:157, /WPrefs.app/HotCornerShortcuts.c:162, /WPrefs.app/HotCornerShortcuts.c:155`
- **Verdetto**: **PULIZIA MEMORIA (Memory Leak in pannelli WPrefs)**
- **Analisi Tecnica**: In `/WPrefs.app/HotCornerShortcuts.c`, oggetti `timage`, stringhe `str` e proplist allocati durante l'anteprima delle texture e la gestione scorciatoie non vengono rilasciati alla chiusura o sovrascrittura del pannello.
- **Piano di Azione Correttiva**: Inserire le chiamate di rilascio `RReleaseImage`, `wfree` e `WMReleasePropList` appropriate.

### CID 1686073: TOCTOU in `Appearance.c` (/WPrefs.app/Appearance.c)
- **CID Coverity**: `1686073`
- **Impatto / Severità**: `Low`
- **Posizione nel sorgente**: `/WPrefs.app/Appearance.c:1860, /WPrefs.app/Appearance.c:1850`
- **Verdetto**: **FALSO POSITIVO (Verifica anteprima file)**
- **Analisi Tecnica**: In `WPrefs.app/Appearance.c:1850, 1860`, `access` viene usato per verificare se l'anteprima del tema esiste prima di caricarla nell'interfaccia utente.
- **Piano di Azione Correttiva**: Nessuna azione richiesta.

### CID 1686087: Unintended integer division in `Appearance.c` (/WPrefs.app/Appearance.c)
- **CID Coverity**: `1686087`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/WPrefs.app/Appearance.c:2015`
- **Verdetto**: **DIFETTO DI CALCOLO (Troncamento da divisione intera)**
- **Analisi Tecnica**: In `WPrefs.app/Appearance.c:2015`, la formula divide due costanti intere `2736 / 154` convertendo il risultato a floating point solo a posteriori, perdendo la parte decimale del rapporto.
- **Piano di Azione Correttiva**: Utilizzare costanti floating point `2736.0 / 154.0` per preservare la precisione della proporzione.

### CID 1686090: Unchecked return value in `TexturePanel.c` (/WPrefs.app/TexturePanel.c)
- **CID Coverity**: `1686090`
- **Impatto / Severità**: `Medium`
- **Posizione nel sorgente**: `/WPrefs.app/TexturePanel.c:916`
- **Verdetto**: **HARDENING (Return value di XParseColor ignorato)**
- **Analisi Tecnica**: In `WPrefs.app/TexturePanel.c:916`, `XParseColor` viene chiamato senza verificare se il nome colore inserito dall'utente è valido.
- **Piano di Azione Correttiva**: Verificare il ritorno di `XParseColor`: se fallisce, ripristinare il colore di fallback nero o emettere messaggio di avviso.

### CID 1686098: Resource leak in WPrefs (/WPrefs.app/Appearance.c)
- **CID Coverity**: `1686098`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/WPrefs.app/Appearance.c:2361`
- **Verdetto**: **PULIZIA MEMORIA (Memory Leak in pannelli WPrefs)**
- **Analisi Tecnica**: In `/WPrefs.app/Appearance.c`, oggetti `timage`, stringhe `str` e proplist allocati durante l'anteprima delle texture e la gestione scorciatoie non vengono rilasciati alla chiusura o sovrascrittura del pannello.
- **Piano di Azione Correttiva**: Inserire le chiamate di rilascio `RReleaseImage`, `wfree` e `WMReleasePropList` appropriate.

### CID 1686101: Resource leak in WPrefs (/WPrefs.app/Appearance.c)
- **CID Coverity**: `1686101`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/WPrefs.app/Appearance.c:760, /WPrefs.app/Appearance.c:740`
- **Verdetto**: **PULIZIA MEMORIA (Memory Leak in pannelli WPrefs)**
- **Analisi Tecnica**: In `/WPrefs.app/Appearance.c`, oggetti `timage`, stringhe `str` e proplist allocati durante l'anteprima delle texture e la gestione scorciatoie non vengono rilasciati alla chiusura o sovrascrittura del pannello.
- **Piano di Azione Correttiva**: Inserire le chiamate di rilascio `RReleaseImage`, `wfree` e `WMReleasePropList` appropriate.

### CID 1686109: Resource leak in WPrefs (/WPrefs.app/TexturePanel.c)
- **CID Coverity**: `1686109`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/WPrefs.app/TexturePanel.c:1029, /WPrefs.app/TexturePanel.c:1041, /WPrefs.app/TexturePanel.c:1062, /WPrefs.app/TexturePanel.c:1062, /WPrefs.app/TexturePanel.c:1033, /WPrefs.app/TexturePanel.c:1062, /WPrefs.app/TexturePanel.c:1033, /WPrefs.app/TexturePanel.c:1062, /WPrefs.app/TexturePanel.c:1041, /WPrefs.app/TexturePanel.c:1037, /WPrefs.app/TexturePanel.c:1045, /WPrefs.app/TexturePanel.c:1037, /WPrefs.app/TexturePanel.c:1018, /WPrefs.app/TexturePanel.c:1045, /WPrefs.app/TexturePanel.c:1029`
- **Verdetto**: **PULIZIA MEMORIA (Memory Leak in pannelli WPrefs)**
- **Analisi Tecnica**: In `/WPrefs.app/TexturePanel.c`, oggetti `timage`, stringhe `str` e proplist allocati durante l'anteprima delle texture e la gestione scorciatoie non vengono rilasciati alla chiusura o sovrascrittura del pannello.
- **Piano di Azione Correttiva**: Inserire le chiamate di rilascio `RReleaseImage`, `wfree` e `WMReleasePropList` appropriate.

## 2. Libreria di Supporto (`wmlib/`)

### CID 1686066: Resource leak in `wmlib/menu.c` (/wmlib/menu.c)
- **CID Coverity**: `1686066`
- **Impatto / Severità**: `High`
- **Posizione nel sorgente**: `/wmlib/menu.c:57`
- **Verdetto**: **BUG REALE (Memory leak in distruzione menu)**
- **Analisi Tecnica**: In `wmlib/menu.c:57`, la struttura `menu` viene deallocata senza liberare preventivamente il puntatore membro `menu->entryline2` allocato durante la costruzione delle voci.
- **Piano di Azione Correttiva**: Invocare `if (menu->entryline2) free(menu->entryline2);` prima della deallocazione della struttura `menu`.

---

## 3. Piano di Implementazione a Step Incrementali

Per evitare regressioni e garantire commit piccoli, atomici e verificati conformemente alle direttive di progetto, gli interventi saranno eseguiti nella seguente sequenza ordinata:

### Step 1: Memory Safety & Crash Prevention nei componenti Core (`src/` e `WINGs/`)
1. **`src/event.c` (CID 1686058, 1686112)**: terminazione esplicita stringa inotify `fname` e prevenzione buffer overrun.
2. **`WINGs/bagtree.c` (CID 1686055)**: correzione Use-After-Free su `self->root` in `WMEraseFromBag`.
3. **`WINGs/wtext.c` (CID 1686072)**: correzione Use-After-Free su TextBlock `tb` eliminato.
4. **`src/dock.c` (CID 240602)**: correzione memory leak di `argv` nel ramo di early-exit `!argc`.
5. **`src/dock.c` (CID 1686080, 1686083, 1686105)**: guardie difensive per `aicon->icon` e validazione dizionari proplist.
6. **`src/screen.c` (CID 1686104)**: deallocazione di `randr_state` nel percorso di abort `wScreenInit`.
7. **`src/misc.c` (CID 1686048)**: deallocazione di `tmp` in caso di errore nel parser scorciatoie.
8. **`src/window.c` (CID 1686075)**: deallocazione della stringa precedente prima della sovrascrittura di `class`.

### Step 2: Integrità Logica & Bounds Checking (`src/`, `WINGs/`, `wrlib/`)
1. **`src/xmodifier.c` (CID 1255978, 1255982, 1255987, 1255992)**: correzione logica macro `store_modifier` eliminando la tautologia `alt_bit != alt_bit`.
2. **`wrlib/load_webp.c` & `load_ppm.c` (CID 1686030, 1686077, 1686110)**: validazione rigorosa dei byte letti da `fread` e verifica limite loop in PPM.
3. **`WINGs/wcolorpanel.c` (CID 1686065)**: correzione del contatore unsigned char nel loop a 255 (prevenzione loop infinito).
4. **`WINGs/dragsource.c` (CID 1686050)**: prevenzione underflow `nchildren - 1` su array vuoto.
5. **`WINGs/wsplitview.c` (CID 1686031, 1686038)** e **`WINGs/wtextfield.c` (CID 1686057, 1686059, 1686082)**: clamp difensivo su dimensioni e posizioni cursore.

### Step 3: Hardening e Memory Safety nelle Utility esterne (`util/`)
1. **`util/wmgenmenu.c` (CID 1686037)**: reinizializzazione di `argv = NULL; argc = 0;` a ogni iterazione del loop (eliminazione Use-After-Free).
2. **`util/wmiv.c` (CID 1686103)**: inizializzazione a NULL del puntatore `dir` prima di `scandir` (eliminazione free non valida).
3. **`util/xdnd.c` (CID 1686045)**: guardia difensiva per `typelist` nullo.
4. **`util/wmgenmenu.c` & `wmmenugen_parse_*.c` (CID 1686034, 1686039, 1686043, 1686051, 1686052, 1686056, 1686094, 1686100)**: rilascio proplist e stringhe temporanee orfane.

### Step 4: Robustezza e Correzioni in WPrefs e wmlib (`WPrefs.app/`, `wmlib/`)
1. **`WPrefs.app/WindowHandling.c` & `Icons.c` (CID 1686046)**: validazione preventiva `sel >= 0` prima di indicizzare gli array delle preferenze (prevenzione lettura a indice `-1`).
2. **`WPrefs.app/Appearance.c` (CID 1686040, 1686087)**: guardia `if (image)` su `RClearImage` e conversione divisione in floating point `2736.0 / 154.0`.
3. **`WPrefs.app/TexturePanel.c`, `Appearance.c`, `HotCornerShortcuts.c` (CID 1686068, 1686098, 1686101, 1686109)**: deallocazione proplist e immagini d'anteprima orfane.
4. **`wmlib/menu.c` (CID 1686066)**: deallocazione di `entryline2` prima della liberazione della struttura menu.

### Step 5: Verifica, Baseline & Regression Testing
1. Ricompilazione pulita con GCC 14 e Clang 23 (`make -j$(nproc)`).
2. Esecuzione suite di test completa (`make exit-test`, `test/wrlib_blend_test`).
3. Esecuzione pipeline di analisi statica unificata `tools/static-analysis.sh check`.
4. Aggiornamento progressivo di `SpecAI.md` ad ogni step completato.

