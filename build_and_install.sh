#!/usr/bin/env bash
# Script per la compilazione e installazione di Window Maker in /opt/wmaker su Ubuntu 24.04
set -euo pipefail

# Colori per i messaggi
readonly C_RESET='\033[0m'
readonly C_GREEN='\033[1;32m'
readonly C_YELLOW='\033[1;33m'
readonly C_RED='\033[1;31m'
readonly C_BLUE='\033[1;34m'

log_info() {
    echo -e "${C_BLUE}==>${C_RESET} ${C_GREEN}$*${C_RESET}"
}

log_warn() {
    echo -e "${C_YELLOW}WARNING:${C_RESET} $*"
}

log_err() {
    echo -e "${C_RED}ERROR:${C_RESET} $*" >&2
}

PREFIX="/opt/wmaker"
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SRC_DIR}"
NPROCS="$(nproc)"

USE_CLANG="${USE_CLANG:-0}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --clang)
            USE_CLANG=1
            shift
            ;;
        --gcc)
            USE_CLANG=0
            shift
            ;;
        --help|-h)
            echo "Uso: $0 [--clang|--gcc]"
            echo "  --clang: Compila con Clang e LLD (clang-23 e lld-23 se disponibili)"
            echo "  --gcc:   Compila con GCC standard (default)"
            exit 0
            ;;
        *)
            log_warn "Argomento sconosciuto: $1"
            shift
            ;;
    esac
done

log_info "Verifica dei prerequisiti di compilazione per Window Maker..."

REQUIRED_PKGS=(
    build-essential
    autoconf
    automake
    libtool
    pkg-config
    libx11-dev
    libxext-dev
    libxinerama-dev
    libxrandr-dev
    libxpm-dev
    libxft-dev
    libxmu-dev
    libpango1.0-dev
    libpng-dev
    libjpeg-dev
    libgif-dev
    libtiff-dev
    libwebp-dev
    libexif-dev
    libarchive-dev
)

if [[ "${USE_CLANG}" -eq 1 ]]; then
    if ! which clang-23 &>/dev/null && ! which clang &>/dev/null; then
        REQUIRED_PKGS+=(clang-23)
    fi
    if ! which lld-23 &>/dev/null && ! which lld &>/dev/null; then
        REQUIRED_PKGS+=(lld-23)
    fi
fi

MISSING_PKGS=()
for pkg in "${REQUIRED_PKGS[@]}"; do
    if ! dpkg -s "$pkg" &>/dev/null; then
        MISSING_PKGS+=("$pkg")
    fi
done

if [[ ${#MISSING_PKGS[@]} -gt 0 ]]; then
    log_warn "I seguenti pacchetti di sviluppo mancano sul sistema:"
    echo "  ${MISSING_PKGS[*]}"
    log_info "Installazione automatica pacchetti (richiede sudo)..."
    sudo apt-get update
    sudo apt-get install -y "${MISSING_PKGS[@]}"
fi

log_info "Verifica directory di installazione: ${PREFIX}"
if [[ ! -d "${PREFIX}" ]]; then
    log_info "Creazione della directory ${PREFIX}..."
    sudo mkdir -p "${PREFIX}"
    sudo chown "${USER}:${USER}" "${PREFIX}"
elif [[ ! -w "${PREFIX}" ]]; then
    log_info "Impostazione permessi utente per ${PREFIX}..."
    sudo chown "${USER}:${USER}" "${PREFIX}"
fi

cd "${BUILD_DIR}"

if [[ ! -f "configure" ]]; then
    log_info "Generazione script di configurazione con autogen.sh..."
    ./autogen.sh
fi

CONF_TOOLCHAIN_ARGS=()
if [[ "${USE_CLANG}" -eq 1 ]]; then
    CLANG_BIN="$(which clang-23 2>/dev/null || which clang 2>/dev/null || true)"
    LLD_BIN="$(which lld-23 2>/dev/null || which lld 2>/dev/null || true)"
    if [[ -z "${CLANG_BIN}" ]]; then
        log_err "Compilatore Clang non trovato."
        exit 1
    fi
    log_info "Selezione toolchain LLVM/Clang: ${CLANG_BIN}"
    CONF_TOOLCHAIN_ARGS+=(
        "CC=${CLANG_BIN}"
        "CFLAGS=-march=native -O2 -g -Wall -Wno-unknown-warning-option"
    )
    if [[ -n "${LLD_BIN}" ]]; then
        log_info "Selezione linker rapido LLD: ${LLD_BIN}"
        CONF_TOOLCHAIN_ARGS+=("LDFLAGS=-fuse-ld=lld")
    fi
else
    log_info "Selezione toolchain GNU GCC standard..."
    CONF_TOOLCHAIN_ARGS+=(
        "CFLAGS=-march=native -O2 -g -Wall"
    )
fi

log_info "Configurazione sorgenti per prefisso ${PREFIX}..."
./configure \
    --prefix="${PREFIX}" \
    --sysconfdir="${PREFIX}/etc" \
    --with-pkgconfdir="${PREFIX}/etc/WindowMaker" \
    --enable-debug \
    --enable-modelock \
    --enable-xinerama \
    --enable-randr \
    --enable-usermenu \
	--enable-sni \
    "${CONF_TOOLCHAIN_ARGS[@]}"

log_info "Compilazione con ${NPROCS} processi paralleli..."
make -j"${NPROCS}"

log_info "Installazione in ${PREFIX}..."
make install

# Creazione script wrapper per esecuzione con percorsi corretti (LD_LIBRARY_PATH, PATH)
mkdir -p "${PREFIX}/bin"
cat <<'EOF' > "${PREFIX}/bin/wmaker-session"
#!/usr/bin/env bash
set -euo pipefail

WM_PREFIX="/opt/wmaker"
export PATH="${WM_PREFIX}/bin:${PATH}"
export LD_LIBRARY_PATH="${WM_PREFIX}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
export XDG_DATA_DIRS="${WM_PREFIX}/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"

exec "${WM_PREFIX}/bin/wmaker" "$@"
EOF
chmod +x "${PREFIX}/bin/wmaker-session"

# Creazione file desktop di sessione X11 per Display Manager (GDM/LightDM/SDDM)
if [[ -d "/usr/share/xsessions" && -w "/usr/share/xsessions" ]]; then
    cat <<EOF > /usr/share/xsessions/wmaker-opt.desktop
[Desktop Entry]
Name=Window Maker (/opt)
Comment=This session logs you into Window Maker installed in /opt/wmaker
Exec=/opt/wmaker/bin/wmaker-session
TryExec=/opt/wmaker/bin/wmaker-session
Icon=/opt/wmaker/share/WindowMaker/Icons/wmicons/GNUstep.xpm
Type=Application
EOF
    log_info "Sessione X11 installata in /usr/share/xsessions/wmaker-opt.desktop"
elif sudo -n true 2>/dev/null; then
    sudo bash -c 'cat <<EOF > /usr/share/xsessions/wmaker-opt.desktop
[Desktop Entry]
Name=Window Maker (/opt)
Comment=This session logs you into Window Maker installed in /opt/wmaker
Exec=/opt/wmaker/bin/wmaker-session
TryExec=/opt/wmaker/bin/wmaker-session
Icon=/opt/wmaker/share/WindowMaker/Icons/wmicons/GNUstep.xpm
Type=Application
EOF'
    log_info "Sessione X11 registrata in /usr/share/xsessions/wmaker-opt.desktop"
fi

log_info "=== Compilazione e installazione completate con successo in ${PREFIX} ==="
log_info "Binari disponibili in: ${PREFIX}/bin"
log_info "Per eseguire Window Maker: ${PREFIX}/bin/wmaker-session"
