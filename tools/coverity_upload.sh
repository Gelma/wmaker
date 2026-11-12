#!/bin/bash
# Build Coverity (cov-build) del repo in una copia pulita sotto /tmp e upload
# dello scan su Coverity Scan, previa conferma interattiva.
set -euo pipefail

readonly REPO_SRC="/home/gelma/dev/prg/intelligenti/wmaker/gelma_github_repo"
readonly COV_BUILD="/opt/dev/cov-analysis-linux64-2024.12.1/bin/cov-build"
readonly CREDENTIALS_FILE="${HOME}/.coverity_credentials"
readonly UPLOAD_URL="https://scan.coverity.com/builds"

log() { printf '==> %s\n' "$*"; }
die() { printf 'ERRORE: %s\n' "$*" >&2; exit 1; }

preflight() {
    local tool
    [[ -d "$REPO_SRC/.git" ]] || die "repository non trovato: $REPO_SRC"
    [[ -x "$COV_BUILD" ]] || die "cov-build non trovato: $COV_BUILD"
    for tool in git make tar curl; do
        command -v "$tool" >/dev/null 2>&1 || die "strumento mancante: $tool"
    done
}

load_credentials() {
    if [[ ! -f "$CREDENTIALS_FILE" ]]; then
        (
            umask 077
            cat > "$CREDENTIALS_FILE" <<'EOF'
# Parametri Coverity Scan usati da tools/coverity_upload.sh
COVERITY_PROJECT="XXXX"
COVERITY_EMAIL="XXXX"
COVERITY_TOKEN="XXXXX"
EOF
        )
        log "Creato $CREDENTIALS_FILE (parametri Coverity)"
    fi
    # shellcheck source=/dev/null
    source "$CREDENTIALS_FILE"
    [[ -n "${COVERITY_PROJECT:-}" ]] || die "COVERITY_PROJECT mancante in $CREDENTIALS_FILE"
    [[ -n "${COVERITY_EMAIL:-}" ]] || die "COVERITY_EMAIL mancante in $CREDENTIALS_FILE"
    [[ -n "${COVERITY_TOKEN:-}" ]] || die "COVERITY_TOKEN mancante in $CREDENTIALS_FILE"
}

main() {
    local workdir tarball version description reply project_enc response_file http_code

    preflight
    load_credentials

    workdir=$(mktemp -d /tmp/.wmaker-coverity.XXXXXXXX)
    log "Directory di lavoro: $workdir"

    log "Copia del repository in corso..."
    cp -af "$REPO_SRC/." "$workdir/"

    cd "$workdir"
    [[ -d .git && "$PWD" != "$REPO_SRC" ]] || die "la copia in $workdir non è un repository git"

    log "git clean -xdf"
    git clean -xdf

    log "./autogen.sh"
    ./autogen.sh

    log "./configure"
    ./configure

    log "cov-build --dir cov-int make -j 4"
    "$COV_BUILD" --dir cov-int make -j 4

    tarball="$workdir/wmaker.lzma"
    log "tar cpvaf wmaker.lzma cov-int"
    tar cpvaf "$tarball" cov-int

    version=$(date '+%y.%m.%d.%H%M')
    description="Branch $(git rev-parse --abbrev-ref HEAD), commit $(git rev-parse --short HEAD), $(date '+%Y-%m-%d %H:%M:%S')"

    log "Archivio pronto: $tarball ($(du -h "$tarball" | cut -f1))"
    log "Progetto: $COVERITY_PROJECT | Email: $COVERITY_EMAIL | Versione: $version"

    printf "Procedo con l'upload su Coverity Scan? [y/N] "
    if ! read -r reply; then
        reply=""
    fi
    if [[ ! "$reply" =~ ^[Yy]([Ee][Ss])?$ ]]; then
        log "Upload annullato. Archivio conservato: $tarball"
        exit 0
    fi

    project_enc=${COVERITY_PROJECT// /%20}
    response_file="$workdir/upload_response.txt"
    log "Upload in corso..."
    http_code=$(curl -sS -o "$response_file" -w '%{http_code}' --connect-timeout 30 \
         --form "token=$COVERITY_TOKEN" \
         --form "email=$COVERITY_EMAIL" \
         --form "file=@$tarball" \
         --form "version=$version" \
         --form "description=$description" \
         "${UPLOAD_URL}?project=${project_enc}") || true
    if [[ "$http_code" == "200" || "$http_code" == "201" ]]; then
        cat "$response_file"
        printf '\n'
        log "Upload completato."
    else
        printf 'Risposta HTTP %s: ' "$http_code"
        cat "$response_file"
        printf '\n'
        if [[ "$http_code" == "401" ]]; then
            die "token rifiutato: per l'upload serve il project token della pagina 'Submit a build' del progetto su scan.coverity.com, non la authentication key dell'API REST; aggiorna COVERITY_TOKEN in $CREDENTIALS_FILE"
        fi
        die "upload fallito (HTTP $http_code)"
    fi
}

trap 'printf "ERRORE: comando fallito alla riga %s (directory di lavoro: %s)\n" "$LINENO" "${workdir:-non creata}" >&2' ERR

main "$@"
