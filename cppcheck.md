# Static Analysis Report (cppcheck)

Date: 2026-11-04  
Tool: `cppcheck 2.23 dev`  
Command line:
```bash
cppcheck -DHAVE_CONFIG_H -I. -I./src -I./wrlib -I./WINGs --enable=warning,performance,portability,style --inline-suppr -q src/ wrlib/ WINGs/ test/
```

---

## Executive Summary

- **Sezione A (`wrlib/`)**: **0 errori, 0 warning**. La libreria grafica è completamente pulita dopo i recenti commit di rimozione dei visual obsoleti e di introduzione dell'aritmetica protetta con `<stdckdint.h>`.
- **Sezione B (Bug critici - Null Pointer Dereference)**: Due crash certi per dereferenziazione NULL corretti con patch mirate in `src/defaults.c` e `WINGs/widgets.c`.
- **Sezione C (Riscontri minori e anomalie logiche)**: Tre anomalie identificate e corrette con relative patch in `test/wm_stress.c` (allocazione non verificata), `src/window.c` (operatore bitwise su booleani) e `WINGs/wmisc.c` (indicizzazione array antecedente al controllo dei limiti).
- **Sezione D (Falsi allarmi)**: Tracciamento dei falsi allarmi generati dalle macro WINGs opache e dai cast convenzionali X11.

---

## Sezione A — Moduli Puliti (`wrlib/`)

L'intero albero di `wrlib/` compilato con le macro di progetto e C23:
- Nessun errore di allocazione, nessun buffer overflow.
- Tutte le chiamate `ckd_mul()` e `ckd_add()` introdotte in `wrlib/raster.c` risultano perfettamente tipizzate e sicure.
- La rimozione dei visual 8-bit in `wrlib/convert.c` ha ripulito oltre 570 righe di potenziale codice fragile senza lasciare dangling references o memorie orfane.

---

## Sezione B — Bug Critici (Null Pointer Dereference & Crash)

### B.1 `src/defaults.c`: Null Pointer Dereference via `wScreenUpdateRootEventMask(scr)`

#### Descrizione
In `src/defaults.c:1359-1368` all'interno di `wReadDefaults(WScreen *scr, WMPropList *dict)`, il codice gestisce esplicitamente il caso `scr == NULL` (quando le impostazioni vengono ricaricate globalmente o durante l'avvio prima della selezione dello schermo):

```c
if (scr && !scr->flags.key_grabs_suspended) {
    wScreenSetKeyGrabs(scr);
} else if (!scr) {
    int s_idx;
    for (s_idx = 0; s_idx < w_global.screen_count; s_idx++) {
        WScreen *s = wScreenWithNumber(s_idx);
        if (s && !s->flags.key_grabs_suspended)
            wScreenSetKeyGrabs(s);
    }
}
```

Tuttavia, alla riga 1371 viene chiamata incondizionatamente:
```c
wScreenUpdateRootEventMask(scr);
```
In `src/screen.c:87`, `wScreenUpdateRootEventMask(WScreen *scr)` dereferenzia direttamente `scr->root_win`:
```c
void wScreenUpdateRootEventMask(WScreen *scr)
{
    long event_mask = EVENT_MASK;

    if (!(wPreferences.scrollable_menus || wPreferences.hot_corners))
        event_mask &= ~PointerMotionMask;

    XSelectInput(dpy, scr->root_win, event_mask);
}
```
Se `scr == NULL`, la chiamata genera un crash immediato con Segmentation Fault (`SIGSEGV`).

#### Patch Proposta
```diff
--- a/src/defaults.c
+++ b/src/defaults.c
@@ -1368,7 +1368,14 @@ void wReadDefaults(WScreen *scr, WMPropList *dict)
 		}
 	}
 
-	wScreenUpdateRootEventMask(scr);
+	if (scr) {
+		wScreenUpdateRootEventMask(scr);
+	} else {
+		for (int s_idx = 0; s_idx < w_global.screen_count; s_idx++) {
+			WScreen *s = wScreenWithNumber(s_idx);
+			if (s)
+				wScreenUpdateRootEventMask(s);
+		}
+	}
 }
 
 void wReadKeybindings(WScreen *scr, WMPropList *dict)
```

---

### B.2 `WINGs/widgets.c`: Crash immediato in `WMDestroyScreen(WMScreen *scr)` su `scr == NULL`

#### Descrizione
In `WINGs/widgets.c:1154-1174`, `WMDestroyScreen` definisce e inizializza un array locale sullo stack `pixmaps[]` accedendo ai membri di `scr`:

```c
void WMDestroyScreen(WMScreen *scr)
{
	int i;
	W_Pixmap *pixmaps[] = {
		scr->checkButtonImageOn, scr->checkButtonImageOff,
		scr->radioButtonImageOn, scr->radioButtonImageOff,
		...
	};

	if (!scr)
		return;
```

Poiché l'array `pixmaps[]` viene inizializzato all'ingresso della funzione, se `scr` è `NULL` l'accesso ai suoi campi avviene durante la costruzione dello stack frame, provocando un crash istantaneo prima che il controllo `if (!scr) return;` possa essere eseguito. La guardia `if (!scr) return;` risulta attualmente codice morto irraggiungibile.

#### Patch Proposta
```diff
--- a/WINGs/widgets.c
+++ b/WINGs/widgets.c
@@ -1154,6 +1154,9 @@ void W_RedisplayWidget(WMWidget *w)
 void WMDestroyScreen(WMScreen *scr)
 {
 	int i;
+	if (!scr)
+		return;
+
 	W_Pixmap *pixmaps[] = {
 		scr->checkButtonImageOn, scr->checkButtonImageOff,
 		scr->radioButtonImageOn, scr->radioButtonImageOff,
@@ -1169,9 +1172,6 @@ void WMDestroyScreen(WMScreen *scr)
 		scr->applicationIconPixmap
 	};
 
-	if (!scr)
-		return;
-
 	if (scr->sharedOpenPanel) {
 		WMFreeFilePanel(scr->sharedOpenPanel);
 		scr->sharedOpenPanel = NULL;
```

---

## Sezione C — Riscontri Minori e Anomalie Logiche

### C.1 `test/wm_stress.c`: Mancato controllo sul valore di ritorno di `calloc()`

#### Descrizione
In `test/wm_stress.c:363-366`, `malloc` e `calloc` vengono invocati per allocare i descrittori dei thread worker senza verificare che il puntatore restituito sia valido prima dell'indicizzazione in memoria. In caso di esaurimento memoria (OOM), si verifica una dereferenziazione di puntatore nullo.

#### Patch Proposta
```diff
--- a/test/wm_stress.c
+++ b/test/wm_stress.c
@@ -362,6 +362,11 @@ int main(int argc, char **argv)
 
 	pthread_t *threads = malloc(sizeof(pthread_t) * num_threads);
 	WorkerArg *args = calloc(num_threads, sizeof(WorkerArg));
+	if (!threads || !args) {
+		fprintf(stderr, "Fatal: failed to allocate memory for worker threads\n");
+		free(threads);
+		free(args);
+		return 1;
+	}
 
 	for (int i = 0; i < num_threads; i++) {
 		args[i].thread_id = i;
```

---

### C.2 `src/window.c`: Operatore bitwise `&` errato su condizioni logiche booleane

#### Descrizione
In `src/window.c:3305` (`handleTitlebarButton`):
```c
if (!WFLAGP(wwin, no_shadeable) & !wPreferences.double_click_fullscreen) {
    /* shade window */
    if (wwin->flags.shaded)
        wUnshadeWindow(wwin);
    else
        wShadeWindow(wwin);
}
```
L'operatore `&` è un bitwise AND, usato erroneamente al posto del logical AND `&&`. Oltre a impedire il cortocircuito logico (short-circuiting), genera avvisi di portabilità e stile del compilatore.

#### Patch Proposta
```diff
--- a/src/window.c
+++ b/src/window.c
@@ -3302,7 +3302,7 @@ static void handleTitlebarButton(WCoreWindow *sender, void *data, XEvent *event)
 
 	if (event->xbutton.button == Button1 ) {
 		if (event->xbutton.state == 0 ) {
-			if (!WFLAGP(wwin, no_shadeable) & !wPreferences.double_click_fullscreen) {
+			if (!WFLAGP(wwin, no_shadeable) && !wPreferences.double_click_fullscreen) {
 				/* shade window */
 				if (wwin->flags.shaded)
 					wUnshadeWindow(wwin);
```

---

### C.3 `WINGs/wmisc.c`: Indicizzazione array prima della verifica dei limiti (`arrayIndexThenCheck`)

#### Descrizione
In `WINGs/wmisc.c:138` (`W_GetTextHeight` / helper di wrapping):
```c
if (!isspace(text[i]) && word1 > 0) {
    i = word1;
} else if (isspace(text[i]) && i < beforecrlf) {
    /* keep space on current row, so new row has next word in column 1 */
    i++;
}
```
Nella clausola `else if`, `isspace(text[i])` viene valutato **prima** di verificare `i < beforecrlf`. Se `i` ha raggiunto o superato il limite `beforecrlf`, l'accesso `text[i]` rischia di leggere oltre il buffer valido. Invertendo l'ordine dei controlli si sfrutta la valutazione a corto circuito garantendo la totale sicurezza dell'accesso in memoria.

#### Patch Proposta
```diff
--- a/WINGs/wmisc.c
+++ b/WINGs/wmisc.c
@@ -135,7 +135,7 @@ static int fitWidth(WMFont * font, const char *text, int width, int beforecrlf)
 	/* keep words complete if possible */
 	if (!isspace(text[i]) && word1 > 0) {
 		i = word1;
-	} else if (isspace(text[i]) && i < beforecrlf) {
+	} else if (i < beforecrlf && isspace(text[i])) {
 		/* keep space on current row, so new row has next word in column 1 */
 		i++;
 	}
```

---

## Sezione D — Analisi dei Falsi Allarmi

1. **`WINGs/selection.c` e `WINGs/wfontpanel.c` (`uninitvar`)**:
   `cppcheck` segnala come non inizializzate le variabili usate dentro `WM_ITERATE_ARRAY(array, var, i)`. La macro espande in:
   ```c
   for (var = WMArrayFirst(array, &(i)); (i) != WANotFound; var = WMArrayNext(array, &(i)))
   ```
   L'assegnazione iniziale `var = WMArrayFirst(...)` avviene regolarmente prima della valutazione della condizione e del corpo del ciclo. Si tratta di un limite dell'analizzatore sulle macro opache.

2. **`src/defaults.c` (`portability: intToPointerCast`)**:
   I cast di costanti numeriche `(void *)WM_CLIP` e `(void *)M_QUICK` come clientData di callback rappresentano un idioma X11 standard e storico, innocuo sulle architetture supportate.
