import json, glob, os

files = sorted(glob.glob("CoverityReport/cid_*.json"), key=lambda p: int(os.path.basename(p).replace("cid_", "").replace(".json", "")))

cids = {}
for fpath in files:
    cid = os.path.basename(fpath).replace("cid_", "").replace(".json", "")
    with open(fpath) as f:
        data = json.load(f)
    checker = data.get("checkerName", "")
    occs = data.get("issueOccurrences", [])
    entries = []
    for occ in occs:
        events = occ.get("events", [])
        main_ev = next((e for e in events if e.get("main")), events[-1] if events else {})
        f = main_ev.get("file", {}).get("filePathname", "")
        line = main_ev.get("lineNumber", "")
        desc = main_ev.get("eventDescription", "")
        tag = main_ev.get("eventTag", "")
        entries.append({
            "file": f,
            "line": line,
            "tag": tag,
            "desc": desc
        })
    cids[cid] = {"checker": checker, "entries": entries}

with open("CoverityReport/coverity_defects.json") as f:
    cv_data = json.load(f)

for row in cv_data.get("rows", []):
    item = {x["key"]: x["value"] for x in row}
    cid = item.get("cid")
    if cid in cids:
        cids[cid]["impact"] = item.get("displayImpact")
        cids[cid]["category"] = item.get("displayCategory")
        cids[cid]["type"] = item.get("displayType")
        cids[cid]["file"] = item.get("displayFile")

def analyze_cid(cid, info):
    ch = info.get("checker", "")
    dtype = info.get("type", "")
    f = info.get("file", "")
    impact = info.get("impact", "")
    entries = info.get("entries", [])
    loc_str = ", ".join(f"{e['file']}:{e['line']}" for e in entries) if entries else f
    first_desc = entries[0]["desc"] if entries else ""

    if cid in ["1255978", "1255982", "1255987", "1255992"]:
        return {
            "title": f"CID {cid}: Operands don't affect result ({f})",
            "verdict": "BUG REALE (Logica macro errata)",
            "impact": impact,
            "locations": loc_str,
            "analysis": f"Nella macro `store_modifier` in `src/xmodifier.c:155-162`, l'espressione `modifier_index == mod_bit && *(old_mod) != mod_bit` (quando invocata ad es. con `store_modifier(\"Alt_L\", &alt_bit)`) si espande letteralmente in `modifier_index == alt_bit && alt_bit != alt_bit`. Poiché `alt_bit != alt_bit` è tautologicamente sempre falso, il controllo di avviso di collisione modificatore (`modwarn`) è codice morto che non scatterà mai.",
            "remediation": "Riscrivere la logica memorizzando il valore iniziale o controllando esplicitamente se il tasto entra in conflitto prima dell'aggiornamento: `if (modifier_index == alt_bit && old_mod_val != alt_bit)`."
        }
    elif cid == "240602":
        return {
            "title": f"CID 240602: Resource leak in `argv` ({f})",
            "verdict": "BUG REALE (Memory Leak in early-exit)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `src/dock.c:3167-3172`, se `wtokensplit(cmdline, &argv, &argc)` produce `argc == 0`, il blocco `if (!argc)` effettua `return 0` liberando `cmdline` e `state`, ma dimentica di liberare il vettore `argv` appena allocato da `wtokensplit`.",
            "remediation": "Aggiungere `wtokenfree(argv, argc);` prima dell'istruzione `return 0;` a riga 3172."
        }
    elif cid in ["1686058", "1686112"]:
        return {
            "title": f"CID {cid}: Buffer Overrun & Unterminated String ({f})",
            "verdict": "BUG REALE CRITICO (Memory Safety / Heap & Stack Overrun)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `src/event.c:395-401`, il campo `pevent->name` letto dalla coda inotify all'interno di `buff` non ha la garanzia di essere terminato da un byte nullo `\\0`. Il successivo ciclo con `strcmp(fname, allowed[i])` può correre oltre la fine dell'array `buff` (1344 byte) causando overrun in lettura e potenziale crash o comportamento indefinito.",
            "remediation": "Garantire esplicitamente la presenza del terminatore forzando `buff[i + sizeof(struct inotify_event) + pevent->len - 1] = '\\0'` prima di referenziare `fname`, oppure confrontare usando `strncmp` limitato a `pevent->len` verificando che `pevent->len == strlen(allowed[i])`."
        }
    elif cid == "1686080":
        return {
            "title": f"CID 1686080: Dereference null pointer in `aicon->icon` ({f})",
            "verdict": "BUG REALE (Possibile crash per dereferenziazione NULL)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `src/dock.c:3600` e `3655`, `aicon->icon` viene dereferenziato (`aicon->icon->selected`, `aicon->icon->owner`). Sebbene la funzione sia chiamata per icone della dock, in casi di icone in transizione, drawer o icone surrogate `aicon->icon` può essere NULL (a riga 3555 il codice contiene infatti un controllo esplicito difensivo).",
            "remediation": "Proteggere gli accessi con guardia: `entry->flags.indicator_on = (aicon->icon ? aicon->icon->selected : 0);` e `if (aicon->icon && aicon->icon->owner) ...`."
        }
    elif cid == "1686081":
        return {
            "title": f"CID 1686081: Copy-paste error flag in `WCHANGE_STATE` ({f})",
            "verdict": "FALSO POSITIVO COVERITY (Transizione Modale Intenzionale)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `src/dock.c:221` e `src/appicon.c:574`, Coverity nota `WCHANGE_STATE(WSTATE_MODAL)` seguito decine di righe dopo da `WCHANGE_STATE(WSTATE_NORMAL)` (riga 273/609) e deduce che `WSTATE_NORMAL` sia il valore originale clonato per errore. In realtà si tratta di un pattern modale canonico: il window manager entra nello stato modale per visualizzare un popup di conferma e, una volta chiuso, ripristina lo stato normale.",
            "remediation": "Nessuna modifica necessaria. Aggiungere un commento esplicativo per documentare che la transizione è voluta."
        }
    elif cid == "1686083":
        return {
            "title": f"CID 1686083: Dereference null return value in `dock.c` ({f})",
            "verdict": "DIFETTO REALE (Mancata validazione dizionario proplist)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `src/dock.c:1675`, `WMGetFromPLDictionary(old_state, tmp)` viene passato direttamente a funzioni proplist senza verificare se restituisce NULL (possibile su file di stato corrotti o creati da versioni precedenti).",
            "remediation": "Assegnare il risultato a variabile temporanea e verificare `if (prop) ...` prima dell'uso."
        }
    elif cid == "1686105":
        return {
            "title": f"CID 1686105: Dereference null return value in `dock.c` ({f})",
            "verdict": "DIFETTO REALE (Mancata validazione dizionario e stringhe proplist)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `src/dock.c:4954` e `5004`, `value` e `dock_state` sono restituiti da chiamate `WMGetFromPLDictionary` e dereferenziati senza verificare che non siano NULL.",
            "remediation": "Aggiungere verifiche difensive `if (value && WMIsPLString(value))` e `if (dock_state && WMIsPLDictionary(dock_state))`."
        }
    elif cid == "1686047":
        return {
            "title": f"CID 1686047: Structurally dead code / unreachable loop increment ({f})",
            "verdict": "DIFETTO REALE (Codice morto strutturale)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `src/stacking.c:493`, il ciclo `for (above = WMBagFirst(...); above != NULL; above = WMBagNext(...))` contiene un corpo che esce sempre tramite `return` o `break` al primo passaggio, rendendo l'espressione di incremento `above = WMBagNext(...)` totalmente irraggiungibile.",
            "remediation": "Sostituire il costrutto `for` con una singola query o riordinare la logica del ciclo per riflettere l'intento reale."
        }
    elif cid == "1686048":
        return {
            "title": f"CID 1686048: Resource leak in `shortcut` modifier parsing ({f})",
            "verdict": "BUG REALE (Memory leak in errore di parsing)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `src/misc.c:908-911`, quando `wXModifierFromKey(text)` fallisce restituendo un valore negativo, la funzione dealloca `buffer` ed esce con `return wstrdup(\"bug\")`, dimenticando di deallocare `tmp` (allocato a riga 899 con `wstrdup(shortcut)`).",
            "remediation": "Aggiungere `wfree(tmp);` prima del return di errore a riga 910."
        }
    elif cid == "1686075":
        return {
            "title": f"CID 1686075: Resource leak in `fixLeaderProperties` ({f})",
            "verdict": "BUG REALE (Memory leak sovrascrittura puntatore)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `src/window.c:1004-1009`, `PropGetWMClass(wwin->main_window, &class, &instance)` alloca memoria per `class`. Se `class[0] == '\\0'`, la variabile viene riassegnata con `class = strdup(...)` senza prima liberare la stringa vuota allocata da `PropGetWMClass`.",
            "remediation": "Inserire `if (class) free(class);` prima di riassegnare `class = strdup(...)`."
        }
    elif cid == "1686104":
        return {
            "title": f"CID 1686104: Resource leak in `wScreenInit` failure path ({f})",
            "verdict": "BUG REALE (Memory leak in path di abort)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `src/screen.c:746-751`, in caso di fallimento della creazione di `scr->wmscreen`, il salto all'etichetta `abort_no_context` libera vari campi e infine `scr`, ma omette di deallocare `scr->randr_state` allocato in precedenza.",
            "remediation": "Aggiungere `if (scr->randr_state) free(scr->randr_state);` prima di `wfree(scr)` all'etichetta di abort."
        }
    elif cid == "1686092":
        return {
            "title": f"CID 1686092: Logically dead code in event handler ({f})",
            "verdict": "PULIZIA CODICE (Istruzione morta)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `src/event.c:1975`, l'istruzione `WMFreeArray(scr->shortcutWindows)` si trova collocata dopo un'uscita incondizionata o in un blocco irraggiungibile.",
            "remediation": "Rimuovere l'istruzione morta o collocarla nella routine di shutdown corretta."
        }
    elif cid == "1686033":
        return {
            "title": f"CID 1686033: Dereference after null check in `icon.c` ({f})",
            "verdict": "FALSO POSITIVO / GUARDIA RIDONDANTE",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `src/icon.c:663`, a riga 657 `if (icon && icon->owner)` induce Coverity a ritenere che `icon` possa essere NULL alle righe successive (663, 670, ecc.). `wIconUpdate` è in realtà un'API interna che richiede un'istanza valida di `icon`.",
            "remediation": "Aggiungere all'inizio della funzione `if (!icon) return;` per chiarire il contratto all'analizzatore."
        }
    elif cid == "1686054":
        return {
            "title": f"CID 1686054: Null dereference in switchpanel ({f})",
            "verdict": "DIFETTO REALE (Dereferenziazione puntatore nullo)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `src/switchpanel.c:188`, `icon` (puntatore a `WIcon`) può risultare NULL per client che non possiedono un'icona associata, e viene passato a `WMSetFrameRelief(icon, ...)` provocando un potenziale crash.",
            "remediation": "Aggiungere il controllo difensivo `if (icon) WMSetFrameRelief(icon, ...);`."
        }
    elif cid == "1686091":
        return {
            "title": f"CID 1686091: Null dereference in `wMenuUnmap` ({f})",
            "verdict": "DIFETTO REALE (Dereferenziazione sottomenu)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `src/menu.c:1226`, viene passato un sottomenu a `wMenuUnmap` senza verificare se esso o la sua struttura interna sia effettivamente mappata e istanziata.",
            "remediation": "Aggiungere `if (menu && menu->flags.mapped) wMenuUnmap(menu);`."
        }
    elif cid == "1686096":
        return {
            "title": f"CID 1686096: Null dereference in `winspector.c` ({f})",
            "verdict": "DIFETTO REALE (Dereferenziazione pannello nullo)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `src/winspector.c:1207, 1210`, `panel->appFrm` viene dereferenziato all'interno di `changePage` quando l'ispettore viene aperto per finestre che non appartengono ad alcuna applicazione di gruppo.",
            "remediation": "Aggiungere verifica di nullità `if (panel->appFrm) ...` prima di accedere ai suoi sotto-elementi."
        }
    elif cid == "1686078":
        return {
            "title": f"CID 1686078: Null return dereference in `texture.c` ({f})",
            "verdict": "DIFETTO REALE (Mancato controllo ritorno RCreateImage)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `src/texture.c:365`, `image` creato da `RCreateImage` o `wTextureRenderImage` può essere NULL in scenari OOM e viene passato a `RClearImage(image, ...)` causando SIGSEGV.",
            "remediation": "Aggiungere `if (!image) return NULL;` prima di operare sul buffer immagine."
        }
    elif cid == "1686079":
        return {
            "title": f"CID 1686079: Null return dereference in `defaults.c` ({f})",
            "verdict": "DIFETTO REALE (Mancato controllo ritorno RCreateImage)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `src/defaults.c:3602`, `img` può risultare NULL e viene passato direttamente a `RCopyArea`.",
            "remediation": "Verificare `if (!img) return;` prima della copia."
        }
    elif cid == "1686084":
        return {
            "title": f"CID 1686084: Null return dereference in `workspace.c` ({f})",
            "verdict": "DIFETTO REALE (Mancata validazione stringa proplist)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `src/workspace.c:950`, un elemento estratto dall'array proplist dei nomi workspace può essere NULL o non-stringa se la configurazione è corrotta, provocando crash in `WMGetFromPLString(pstr)`.",
            "remediation": "Verificare `if (!pstr || !WMIsPLString(pstr)) continue;` nel loop di parsing."
        }
    elif cid == "1686107":
        return {
            "title": f"CID 1686107: Reverse Inull in `session.c` ({f})",
            "verdict": "INCONSISTENZA DI CODIFICA (Controllo post-dereferenziazione)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `src/session.c:276`, `command` viene dereferenziato incondizionatamente decine di righe prima e poi controllato con `if (command != NULL)`. Se potesse essere nullo, il programma sarebbe già crashato.",
            "remediation": "Anticipare il controllo `if (!command) return;` in cima alla funzione di caricamento comandi sessione."
        }
    elif cid == "1686060":
        return {
            "title": f"CID 1686060: Reverse Inull in `wsmap.c` ({f})",
            "verdict": "INCONSISTENZA DI CODIFICA (Controllo post-dereferenziazione)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `src/wsmap.c:116`, `wsmap->win` viene dereferenziato prima del controllo di nullità.",
            "remediation": "Rimuovere il controllo ridondante a valle o anticiparlo ad inizio procedura."
        }
    elif cid in ["1686061", "1686076", "1686062", "1686088", "1686093", "1686099"]:
        return {
            "title": f"CID {cid}: Unchecked library/system return ({f})",
            "verdict": "HARDENING DI SISTEMA (Return value ignorato)",
            "impact": impact,
            "locations": loc_str,
            "analysis": f"Chiamate di sistema o di libreria (`fcntl` in startup.c, `poll` in misc.c, `XGrabPointer` in wsmap.c, `rename`/`mkdir` in telemetry.c) eseguite senza verifica del valore di ritorno.",
            "remediation": "Aggiungere controllo esplicito con fallback/log d'errore o cast a `(void)` per le operazioni intrinsecamente best-effort."
        }
    elif cid in ["1686036", "1686085", "1686108"]:
        return {
            "title": f"CID {cid}: Calling risky function `rand` ({f})",
            "verdict": "FALSO POSITIVO (Uso non crittografico)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "Coverity segnala l'uso di `rand()` per calcolare offset di posizionamento casuale delle finestre (`placement.c`), passaggi di animazione miniature (`actions.c`) ed effetti visuali ghost (`superfluous.c`). Tali contesti non hanno alcun requisito di sicurezza crittografica.",
            "remediation": "Nessuna azione di sicurezza richiesta. Documentare il falso positivo."
        }
    elif cid == "1686042":
        return {
            "title": f"CID 1686042: Time of check time of use in `monitor.c` ({f})",
            "verdict": "FALSO POSITIVO (Polling su file sentinella di uscita)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `src/monitor.c:160`, `access(exit_sentinel, F_OK)` è utilizzato in un ciclo di monitoraggio per rilevare quando il processo deve arrestarsi.",
            "remediation": "Nessuna azione richiesta, pattern di polling standard per file sentinella."
        }
    elif cid in ["1686063", "1686095"]:
        return {
            "title": f"CID {cid}: Unused value assignment ({f})",
            "verdict": "PULIZIA CODICE (Assegnazione morta)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "Assegnazioni `dict = NULL` in `defaults.c` e `lpath = NULL` in `rootmenu.c` che vengono immediatamente sovrascritte prima di essere lette.",
            "remediation": "Eliminare l'assegnazione iniziale ridondante."
        }
    # WINGs
    elif cid == "1686055":
        return {
            "title": f"CID 1686055: Use-After-Free in Red-Black Tree ({f})",
            "verdict": "BUG REALE CRITICO (Use After Free in WMBag / RB-Tree)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `WINGs/bagtree.c:459-464`: `ptr = rbTreeDelete(self, ptr); ... wfree(ptr); wassertrv(self->count == 0 || self->root->index >= 0, 1);`. Quando il nodo rimosso è la radice precedente, l'asserzione dereferenzia `self->root->index` che potrebbe puntare alla memoria appena rilasciata da `wfree(ptr)` se `rbTreeDelete` non ha aggiornato consistentemente `self->root` prima del free.",
            "remediation": "Eseguire la verifica e l'eventuale asserzione prima di chiamare `wfree(ptr)`, oppure garantire che `self->root` punti al nodo sentinella `self->nil` prima dell'accesso."
        }
    elif cid == "1686072":
        return {
            "title": f"CID 1686072: Use-After-Free in Text Widget ({f})",
            "verdict": "BUG REALE CRITICO (Use After Free in WMText)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `WINGs/wtext.c:1874-1878`: `WMDestroyTextBlock(WMRemoveTextBlock(tPtr))` distrugge e dealloca il TextBlock corrente `tb`. A riga 1878, la clausola `if (sibling)` continua a dereferenziare campi di `tb` (`tb->used`, `tb->text`), operando su memoria già liberata.",
            "remediation": "Memorizzare i valori necessari in variabili automatiche prima di distruggere il blocco, e impostare `tb = NULL;` subito dopo la deallocazione."
        }
    elif cid in ["1686031", "1686038"]:
        return {
            "title": f"CID {cid}: Overflowed integer argument in `wsplitview.c` ({f})",
            "verdict": "DIFETTO DI VALIDAZIONE (Possibile underflow nei calcoli di splitview)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `WINGs/wsplitview.c:363, 484`, la differenza tra la dimensione della vista e `getTotalSize(...)` può diventare negativa se la finestra viene ridimensionata al di sotto dei vincoli minimi, provocando un underflow quando convertita a unsigned o passata a funzioni di dimensionamento.",
            "remediation": "Applicare una clausola di clamp esplicita: `if (delta < 0) delta = 0;` prima di propagare la nuova dimensione."
        }
    elif cid in ["1686057", "1686059", "1686082"]:
        return {
            "title": f"CID {cid}: Overflowed / negative integer in `wtextfield.c` ({f})",
            "verdict": "DIFETTO DI BOUNDS (Possibile indice negativo in TextField)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `WINGs/wtextfield.c:199, 426, 1247`, le coordinate di visualizzazione `tPtr->viewPosition` e `tPtr->cursorPosition` possono assumere valori negativi a seguito di scorrimento veloce o backspace UTF-8 ripetuto senza verifica di zero inferiore.",
            "remediation": "Applicare un vincolo esplicito `if (tPtr->viewPosition < 0) tPtr->viewPosition = 0;` prima dell'invocazione di `WMWidthOfString` o `paintTextField`."
        }
    elif cid == "1686071":
        return {
            "title": f"CID 1686071: Overflowed integer argument in `error.c` ({f})",
            "verdict": "DIFETTO DI BOUNDS (Potenziale underflow in strncat)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `WINGs/error.c:98, 100`, il terzo argomento di `strncat(buf, ..., linemax - 1 - strlen(buf))` può andare in underflow (diventando un enorme `size_t` positivo) se la lunghezza corrente di `buf` supera `linemax - 1`.",
            "remediation": "Calcolare la lunghezza residua in modo sicuro: `size_t cur = strlen(buf); size_t rem = (cur + 1 < linemax) ? (linemax - 1 - cur) : 0; if (rem > 0) strncat(..., rem);`."
        }
    elif cid == "1686050":
        return {
            "title": f"CID 1686050: Overflowed constant in `dragsource.c` ({f})",
            "verdict": "DIFETTO DI UNDERFLOW (Unsigned underflow in ciclo)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `WINGs/dragsource.c:736`, `for (i = nchildren - 1; i >= 0; i--)` dove `nchildren` è di tipo unsigned e può valere 0. `0U - 1` produce `4294967295U`, che assegnato a `int i` causa overflow / loop anomalo.",
            "remediation": "Condizionare il loop: `if (nchildren > 0) { for (int i = (int)nchildren - 1; i >= 0; i--) { ... } }`."
        }
    elif cid == "1686065":
        return {
            "title": f"CID 1686065: Overflowed constant in `wcolorpanel.c` ({f})",
            "verdict": "BUG REALE (Loop infinito per overflow di tipo)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `WINGs/wcolorpanel.c:3554`, una variabile contatore `unsigned char i` utilizzata in `for (i = 0; i <= 255; i++)` non potrà mai superare 255, provocando un overflow ciclico a 0 e un loop infinito.",
            "remediation": "Dichiarare il contatore come `int i` o terminare con `i < 256`."
        }
    elif cid == "1686053":
        return {
            "title": f"CID 1686053: Dereference null return value in `wcolorpanel.c` ({f})",
            "verdict": "DIFETTO REALE (Dereferenziazione spettro nullo)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `WINGs/wcolorpanel.c:2808`, la variabile `spectrum` viene dereferenziata senza verificare se la modalità attiva del pannello colori ha istanziato la relativa vista spettro.",
            "remediation": "Aggiungere la guardia `if (spectrum) ...`."
        }
    elif cid in ["1686067", "1686106"]:
        return {
            "title": f"CID {cid}: Explicit null dereferenced in `wscrollview.c` ({f})",
            "verdict": "DIFETTO REALE (Dereferenziazione scroller non presenti)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `WINGs/wscrollview.c:421, 454`, all'interno di `reorganizeInterior`, i membri `sPtr->hScroller` o `sPtr->vScroller` vengono passati a funzioni di riconfigurazione senza verificare se sono abilitati o nulli.",
            "remediation": "Aggiungere guardie `if (sPtr->hScroller)` e `if (sPtr->vScroller)` prima di accedere alle geometrie dello scroller."
        }
    elif cid in ["1686032", "1686070"]:
        return {
            "title": f"CID {cid}: Null check inconsistencies in `wtext.c` ({f})",
            "verdict": "DIFETTO REALE (Dereferenziazione TextBlock nullo)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `WINGs/wtext.c:1047-1053` e `1194`, `tb` viene dereferenziato prima o dopo controlli di nullità parziali, segnalando che il blocco di testo corrente può essere NULL se il widget è vuoto.",
            "remediation": "Uniformare i controlli difensivi proteggendo sistematicamente gli accessi a `tb` quando il widget non contiene testo."
        }
    elif cid in ["1686041", "1686069", "1686074", "1686097", "1686102"]:
        return {
            "title": f"CID {cid}: Time of check time of use in WINGs ({f})",
            "verdict": "FALSO POSITIVO (Verifica file locale standard)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "Chiamate `stat` o `access` prima di scrivere file proplist o palette colori nella home dell'utente. In un contesto desktop a singolo utente non costituiscono minaccia di escalation di privilegi.",
            "remediation": "Nessuna azione urgente richiesta; passare a `open(..., O_CREAT | O_EXCL)` per le nuove implementazioni."
        }
    elif cid == "1686111":
        return {
            "title": f"CID 1686111: Logically dead code in `dragsource.c` ({f})",
            "verdict": "PULIZIA CODICE (Codice morto)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `WINGs/dragsource.c:375`, un'assegnazione di versione protocollo non può mai essere raggiunta a causa di un controllo di versione restrittivo all'inizio della funzione.",
            "remediation": "Rimuovere il ramo logico non raggiungibile."
        }
    elif cid == "1686035":
        return {
            "title": f"CID 1686035: Unchecked library return in `wcolorpanel.c` ({f})",
            "verdict": "HARDENING (Return value di remove ignorato)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `WINGs/wcolorpanel.c:3122`, `remove(toPath)` viene invocato senza verificare se la cancellazione del file temporaneo ha avuto successo.",
            "remediation": "Verificare il valore di ritorno ed emettere un warning in caso di errore di cancellazione."
        }
    elif cid == "1686064":
        return {
            "title": f"CID 1686064: Reverse Inull in `wfilepanel.c` ({f})",
            "verdict": "INCONSISTENZA DI CODIFICA (Controllo post-dereferenziazione)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `WINGs/wfilepanel.c:652`, `file` viene dereferenziato a monte e poi controllato con `if (file)` a valle.",
            "remediation": "Anticipare il controllo `if (!file) return;` a monte."
        }
    # wrlib
    elif cid in ["1686030", "1686077"]:
        return {
            "title": f"CID {cid}: Ignoring number of bytes read ({f})",
            "verdict": "DIFETTO REALE (Mancata validazione completezza lettura fread)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `wrlib/load_webp.c:82` e `load_ppm.c:183`, `fread` viene verificato solo come valore booleano (`if (!fread(...))`). Se il file viene troncato a metà dell'header, la lettura parziale viene accettata procedendo con dati parziali o non inizializzati.",
            "remediation": "Confrontare esplicitamente il ritorno di `fread` con il numero esatto di elementi richiesti: `if (fread(buf, 1, count, f) != count) ...`."
        }
    elif cid == "1686110":
        return {
            "title": f"CID 1686110: Untrusted loop bound in `load_ppm.c` ({f})",
            "verdict": "DIFETTO DI VALIDAZIONE (Loop bound non validato)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `wrlib/load_ppm.c:126`, un valore letto direttamente dal flusso dati PPM viene impiegato come limite superiore di un ciclo di scansione senza una previa verifica rispetto alla dimensione massima ammessa.",
            "remediation": "Aggiungere un controllo sul valore massimo ammesso prima di avviare il ciclo."
        }
    # util
    elif cid == "1686037":
        return {
            "title": f"CID 1686037: Use-After-Free in `wmgenmenu.c` ({f})",
            "verdict": "BUG REALE CRITICO (Use After Free in iterazione loop)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `util/wmgenmenu.c:413, 459`, alla fine di ogni iterazione del ciclo `wtokenfree(argv, argc)` dealloca `argv`. All'inizio dell'iterazione successiva, se `list[i][1]` è vuoto o se `wtokensplit` non riassegna correttamente i puntatori, `argv[0]` fa riferimento alla memoria liberata al giro precedente.",
            "remediation": "Inizializzare `argv = NULL; argc = 0;` ad ogni iterazione del ciclo all'interno del blocco `while`."
        }
    elif cid == "1686103":
        return {
            "title": f"CID 1686103: Uninitialized pointer read in `wmiv.c` ({f})",
            "verdict": "BUG REALE (Free di puntatore non inizializzato)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `util/wmiv.c:1549-1568`, se `scandir` fallisce restituendo errore o 0 voci, la variabile `dir` resta non inizializzata. A riga 1568 `free(dir)` tenta di liberare un puntatore con valore casuale sullo stack, causando crash immediato.",
            "remediation": "Inizializzare `struct dirent **dir = NULL;` alla dichiarazione della funzione `connect_dir`."
        }
    elif cid in ["1686034", "1686039", "1686043", "1686051", "1686052", "1686056", "1686094", "1686100"]:
        return {
            "title": f"CID {cid}: Resource leak in utility CLI ({f})",
            "verdict": "PULIZIA MEMORIA (Memory Leak in tool generatore menu/config)",
            "impact": impact,
            "locations": loc_str,
            "analysis": f"In `{f}`, stringhe temporanee o oggetti proplist allocati con `WMCreatePLString` o `wtokensplit` non vengono rilasciati prima dell'uscita dalla funzione o al termine dell'elaborazione della voce.",
            "remediation": "Aggiungere le corrispondenti chiamate di deallocazione `WMReleasePropList` o `wfree` nei punti di uscita."
        }
    elif cid == "1686045":
        return {
            "title": f"CID 1686045: Null dereference in `util/xdnd.c` ({f})",
            "verdict": "DIFETTO REALE (Dereferenziazione puntatore nullo)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `util/xdnd.c:933`, `typelist` nullo viene passato a `array_length` che ne dereferenzia i puntatori interni senza controllo preliminare.",
            "remediation": "Aggiungere verifica di validità `if (typelist) ...` prima di calcolarne la lunghezza."
        }
    elif cid in ["1686049", "1686086"]:
        return {
            "title": f"CID {cid}: Deadcode in utility ({f})",
            "verdict": "PULIZIA CODICE (Istruzione non raggiungibile)",
            "impact": impact,
            "locations": loc_str,
            "analysis": f"Istruzioni di `break` ridondanti posizionate immediatamente dopo `return` o `exit` in `{f}`.",
            "remediation": "Rimuovere le istruzioni non raggiungibili."
        }
    elif cid in ["1686044", "1686089"]:
        return {
            "title": f"CID {cid}: TOCTOU in utility ({f})",
            "verdict": "FALSO POSITIVO (Controllo parametri CLI)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "Uso di `stat` per validare l'esistenza di file passati come argomenti da riga di comando prima dell'elaborazione.",
            "remediation": "Nessuna azione richiesta."
        }
    # WPrefs.app
    elif cid == "1686046":
        return {
            "title": f"CID 1686046: Negative array index read ({f})",
            "verdict": "BUG REALE (Accesso array fuori limite con indice -1)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `WPrefs.app/WindowHandling.c:263, 275` e `Icons.c:461`, la chiamata `WMGetPopUpButtonSelectedItem(...)` restituisce `-1` se nessun elemento è selezionato. Tale valore negativo viene utilizzato direttamente come indice di array (`window_placements[-1]`), provocando lettura di memoria non valida e crash dell'interfaccia WPrefs.",
            "remediation": "Assegnare l'indice a variabile locale e validare `int sel = WMGetPopUpButtonSelectedItem(...); if (sel >= 0) { ... }`."
        }
    elif cid in ["1686068", "1686098", "1686101", "1686109"]:
        return {
            "title": f"CID {cid}: Resource leak in WPrefs ({f})",
            "verdict": "PULIZIA MEMORIA (Memory Leak in pannelli WPrefs)",
            "impact": impact,
            "locations": loc_str,
            "analysis": f"In `{f}`, oggetti `timage`, stringhe `str` e proplist allocati durante l'anteprima delle texture e la gestione scorciatoie non vengono rilasciati alla chiusura o sovrascrittura del pannello.",
            "remediation": "Inserire le chiamate di rilascio `RReleaseImage`, `wfree` e `WMReleasePropList` appropriate."
        }
    elif cid == "1686040":
        return {
            "title": f"CID 1686040: Null return dereference in `Appearance.c` ({f})",
            "verdict": "DIFETTO REALE (Dereferenziazione RImage nullo)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `WPrefs.app/Appearance.c:591`, `image` creato per l'anteprima può essere NULL se la memoria è esaurita e viene passato a `RClearImage`.",
            "remediation": "Aggiungere `if (image) RClearImage(image, ...);`."
        }
    elif cid == "1686087":
        return {
            "title": f"CID 1686087: Unintended integer division in `Appearance.c` ({f})",
            "verdict": "DIFETTO DI CALCOLO (Troncamento da divisione intera)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `WPrefs.app/Appearance.c:2015`, la formula divide due costanti intere `2736 / 154` convertendo il risultato a floating point solo a posteriori, perdendo la parte decimale del rapporto.",
            "remediation": "Utilizzare costanti floating point `2736.0 / 154.0` per preservare la precisione della proporzione."
        }
    elif cid == "1686090":
        return {
            "title": f"CID 1686090: Unchecked return value in `TexturePanel.c` ({f})",
            "verdict": "HARDENING (Return value di XParseColor ignorato)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `WPrefs.app/TexturePanel.c:916`, `XParseColor` viene chiamato senza verificare se il nome colore inserito dall'utente è valido.",
            "remediation": "Verificare il ritorno di `XParseColor`: se fallisce, ripristinare il colore di fallback nero o emettere messaggio di avviso."
        }
    elif cid == "1686073":
        return {
            "title": f"CID 1686073: TOCTOU in `Appearance.c` ({f})",
            "verdict": "FALSO POSITIVO (Verifica anteprima file)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `WPrefs.app/Appearance.c:1850, 1860`, `access` viene usato per verificare se l'anteprima del tema esiste prima di caricarla nell'interfaccia utente.",
            "remediation": "Nessuna azione richiesta."
        }
    # wmlib
    elif cid == "1686066":
        return {
            "title": f"CID 1686066: Resource leak in `wmlib/menu.c` ({f})",
            "verdict": "BUG REALE (Memory leak in distruzione menu)",
            "impact": impact,
            "locations": loc_str,
            "analysis": "In `wmlib/menu.c:57`, la struttura `menu` viene deallocata senza liberare preventivamente il puntatore membro `menu->entryline2` allocato durante la costruzione delle voci.",
            "remediation": "Invocare `if (menu->entryline2) free(menu->entryline2);` prima della deallocazione della struttura `menu`."
        }
    else:
        return {
            "title": f"CID {cid}: {dtype} ({f})",
            "verdict": "ANALISI GENERICA",
            "impact": impact,
            "locations": loc_str,
            "analysis": f"Segnalazione Coverity checker {ch}: {first_desc}",
            "remediation": "Verificare il codice sorgente e applicare patch difensiva."
        }

# Generate complete Markdown
doc = []
doc.append("# Piano di Azioni Correttive dei Difetti Coverity (CoverityGemini.md)\n")
doc.append("**Data:** 2026-11-04  ")
doc.append("**Dataset analizzato:** Report Coverity in `CoverityReport/` (88 CID univoci, 139 eventi dettagliati)  ")
doc.append("**Autore:** Gemini  ")
doc.append("**Branch di riferimento:** `gelma`  \n")
doc.append("---\n")
doc.append("## 1. Quadro Generale & Classificazione dei Difetti\n")
doc.append("L'analisi approfondita di tutti gli 88 file JSON presenti nella directory `CoverityReport/` evidenzia la seguente distribuzione:")
doc.append("- **Core Window Maker (`src/`)**: 37 CID")
doc.append("- **Toolkit WINGs (`WINGs/`)**: 23 CID")
doc.append("- **Libreria Grafica wrlib (`wrlib/`)**: 3 CID")
doc.append("- **Utility a riga di comando (`util/`)**: 15 CID")
doc.append("- **Pannello di configurazione (`WPrefs.app/`)**: 9 CID")
doc.append("- **Libreria legacy wmlib (`wmlib/`)**: 1 CID\n")
doc.append("### Sintesi per Categoria di Rischio\n")
doc.append("| Categoria | Descrizione Principale | Totale CID | Verdetto Ingegneristico |")
doc.append("|---|---|---|---|")
doc.append("| **Memory Safety & UAF** | Use-After-Free, buffer overrun, puntatori non inizializzati | 7 | **Massima priorità (Fix immediato obbligatorio)** |")
doc.append("| **Resource Leaks** | Mancata deallocazione memoria e proplist | 17 | **Priorità alta (Stabilità e tenuta a lungo termine)** |")
doc.append("| **Null Pointer Dereference** | Dereferenziazioni di puntatori non verificati o post-check | 20 | **Priorità alta (Prevenzione crash su casi limite/OOM)** |")
doc.append("| **Insecure Data & Integers** | Underflow/overflow interi, loop infiniti, bound non validati | 13 | **Priorità media (Robustezza logica e bounds checking)** |")
doc.append("| **Error Handling** | Return value ignorati su I/O e librerie di sistema | 10 | **Priorità media (Hardening sistemistico)** |")
doc.append("| **Dead Code & Maintainability** | Istruzioni irraggiungibili, assegnazioni morte, divisioni int | 9 | **Priorità bassa (Pulizia e igiene del codice)** |")
doc.append("| **False Positives / TOCTOU** | Falsi allarmi (`rand()` grafico, `access()` su sentinelle) | 12 | **Nessuna azione richiesta (Documentati e catalogati)** |\n")
doc.append("---\n")

subsystems_list = [
    ("Core Window Maker (`src/`)", ["/src/"]),
    ("Toolkit WINGs (`WINGs/`)", ["/WINGs/"]),
    ("Libreria Grafica wrlib (`wrlib/`)", ["/wrlib/"]),
    ("Utility e Script (`util/`)", ["/util/"]),
    ("Pannello Preferenze (`WPrefs.app/`)", ["/WPrefs.app/"]),
    ("Libreria di Supporto (`wmlib/`)", ["/wmlib/"])
]

for sub_name, prefixes in subsystems_list:
    doc.append(f"## 2. {sub_name}\n")
    matching_cids = []
    for cid, info in cids.items():
        f = info.get("file", "")
        if any(f.startswith(p) for p in prefixes):
            matching_cids.append((cid, info))
    
    matching_cids.sort(key=lambda x: int(x[0]))
    for cid, info in matching_cids:
        res = analyze_cid(cid, info)
        doc.append(f"### {res['title']}")
        doc.append(f"- **CID Coverity**: `{cid}`")
        doc.append(f"- **Impatto / Severità**: `{res['impact']}`")
        doc.append(f"- **Posizione nel sorgente**: `{res['locations']}`")
        doc.append(f"- **Verdetto**: **{res['verdict']}**")
        doc.append(f"- **Analisi Tecnica**: {res['analysis']}")
        doc.append(f"- **Piano di Azione Correttiva**: {res['remediation']}\n")

doc.append("---\n")
doc.append("## 3. Piano di Implementazione a Step Incrementali\n")
doc.append("Per evitare regressioni e garantire commit piccoli, atomici e verificati conformemente alle direttive di progetto, gli interventi saranno eseguiti nella seguente sequenza ordinata:\n")
doc.append("### Step 1: Memory Safety & Crash Prevention nei componenti Core (`src/` e `WINGs/`)")
doc.append("1. **`src/event.c` (CID 1686058, 1686112)**: terminazione esplicita stringa inotify `fname` e prevenzione buffer overrun.")
doc.append("2. **`WINGs/bagtree.c` (CID 1686055)**: correzione Use-After-Free su `self->root` in `WMEraseFromBag`.")
doc.append("3. **`WINGs/wtext.c` (CID 1686072)**: correzione Use-After-Free su TextBlock `tb` eliminato.")
doc.append("4. **`src/dock.c` (CID 240602)**: correzione memory leak di `argv` nel ramo di early-exit `!argc`.")
doc.append("5. **`src/dock.c` (CID 1686080, 1686083, 1686105)**: guardie difensive per `aicon->icon` e validazione dizionari proplist.")
doc.append("6. **`src/screen.c` (CID 1686104)**: deallocazione di `randr_state` nel percorso di abort `wScreenInit`.")
doc.append("7. **`src/misc.c` (CID 1686048)**: deallocazione di `tmp` in caso di errore nel parser scorciatoie.")
doc.append("8. **`src/window.c` (CID 1686075)**: deallocazione della stringa precedente prima della sovrascrittura di `class`.\n")

doc.append("### Step 2: Integrità Logica & Bounds Checking (`src/`, `WINGs/`, `wrlib/`)")
doc.append("1. **`src/xmodifier.c` (CID 1255978, 1255982, 1255987, 1255992)**: correzione logica macro `store_modifier` eliminando la tautologia `alt_bit != alt_bit`.")
doc.append("2. **`wrlib/load_webp.c` & `load_ppm.c` (CID 1686030, 1686077, 1686110)**: validazione rigorosa dei byte letti da `fread` e verifica limite loop in PPM.")
doc.append("3. **`WINGs/wcolorpanel.c` (CID 1686065)**: correzione del contatore unsigned char nel loop a 255 (prevenzione loop infinito).")
doc.append("4. **`WINGs/dragsource.c` (CID 1686050)**: prevenzione underflow `nchildren - 1` su array vuoto.")
doc.append("5. **`WINGs/wsplitview.c` (CID 1686031, 1686038)** e **`WINGs/wtextfield.c` (CID 1686057, 1686059, 1686082)**: clamp difensivo su dimensioni e posizioni cursore.\n")

doc.append("### Step 3: Hardening e Memory Safety nelle Utility esterne (`util/`)")
doc.append("1. **`util/wmgenmenu.c` (CID 1686037)**: reinizializzazione di `argv = NULL; argc = 0;` a ogni iterazione del loop (eliminazione Use-After-Free).")
doc.append("2. **`util/wmiv.c` (CID 1686103)**: inizializzazione a NULL del puntatore `dir` prima di `scandir` (eliminazione free non valida).")
doc.append("3. **`util/xdnd.c` (CID 1686045)**: guardia difensiva per `typelist` nullo.")
doc.append("4. **`util/wmgenmenu.c` & `wmmenugen_parse_*.c` (CID 1686034, 1686039, 1686043, 1686051, 1686052, 1686056, 1686094, 1686100)**: rilascio proplist e stringhe temporanee orfane.\n")

doc.append("### Step 4: Robustezza e Correzioni in WPrefs e wmlib (`WPrefs.app/`, `wmlib/`)")
doc.append("1. **`WPrefs.app/WindowHandling.c` & `Icons.c` (CID 1686046)**: validazione preventiva `sel >= 0` prima di indicizzare gli array delle preferenze (prevenzione lettura a indice `-1`).")
doc.append("2. **`WPrefs.app/Appearance.c` (CID 1686040, 1686087)**: guardia `if (image)` su `RClearImage` e conversione divisione in floating point `2736.0 / 154.0`.")
doc.append("3. **`WPrefs.app/TexturePanel.c`, `Appearance.c`, `HotCornerShortcuts.c` (CID 1686068, 1686098, 1686101, 1686109)**: deallocazione proplist e immagini d'anteprima orfane.")
doc.append("4. **`wmlib/menu.c` (CID 1686066)**: deallocazione di `entryline2` prima della liberazione della struttura menu.\n")

doc.append("### Step 5: Verifica, Baseline & Regression Testing")
doc.append("1. Ricompilazione pulita con GCC 14 e Clang 23 (`make -j$(nproc)`).")
doc.append("2. Esecuzione suite di test completa (`make exit-test`, `test/wrlib_blend_test`).")
doc.append("3. Esecuzione pipeline di analisi statica unificata `tools/static-analysis.sh check`.")
doc.append("4. Aggiornamento progressivo di `SpecAI.md` ad ogni step completato.\n")

with open("CoverityGemini.md", "w") as f_out:
    f_out.write("\n".join(doc) + "\n")

print(f"CoverityGemini.md successfully generated ({len(doc)} lines).")
