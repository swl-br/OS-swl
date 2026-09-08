# shell-rc.sh — identidade visual do terminal SWL OS (V1)
# POSIX ash (busybox). Sourced por /etc/profile e ENV.

# Evita reentrada
[ -n "${SWL_SHELL_RC:-}" ] && return 0 2>/dev/null || true
SWL_SHELL_RC=1

# Cores (ANSI)
_swl_c_cyan='\033[36m'
_swl_c_mag='\033[35m'
_swl_c_green='\033[32m'
_swl_c_dim='\033[2m'
_swl_c_bold='\033[1m'
_swl_c_reset='\033[0m'

# Prompt
PS1='SWL:~$ '

# Caminhos de arte / fetch (rootfs)
_SWL_SHARE="${SWL_SHARE:-/usr/share/swl}"
_SWL_FETCH="${SWL_FETCH:-/bin/swlfetch}"

# ---- help ---------------------------------------------------------------
swl_help() {
    printf '%b\n' "${_swl_c_cyan}${_swl_c_bold}SWL OS${_swl_c_reset} — shell de identidade (V1)"
    printf '\n'
    printf '%b\n' "${_swl_c_mag}Comandos de identidade${_swl_c_reset}"
    printf '  %-14s %s\n' 'swlfetch' 'logo Neo + infos do sistema'
    printf '  %-14s %s\n' 'swl_help' 'esta mensagem'
    printf '  %-14s %s\n' 'swl_clear' 'limpa a tela e redesenha o cabeçalho'
    printf '  %-14s %s\n' 'swl_spinner N' 'spinner de N segundos (feedback visual)'
    printf '  %-14s %s\n' 'swl_progress N' 'barra de progresso 0..N passos'
    printf '\n'
    printf '%b\n' "${_swl_c_mag}Shell${_swl_c_reset}"
    printf '  Prompt:  SWL:~$\n'
    printf '  Shell:   busybox ash (/bin/sh)\n'
    printf '  Arquivo: /usr/share/swl/shell-rc.sh\n'
    printf '\n'
}

# ---- clear com cabeçalho ------------------------------------------------
swl_clear() {
    printf '\033[2J\033[H'
    if [ -x "$_SWL_FETCH" ]; then
        "$_SWL_FETCH" 2>/dev/null || true
    elif [ -f "$_SWL_SHARE/neo-face.txt" ]; then
        cat "$_SWL_SHARE/neo-face.txt" 2>/dev/null || true
        printf '\n'
    fi
    printf '%b\n' "${_swl_c_dim}SWL OS — digite swl_help para comandos${_swl_c_reset}"
    printf '\n'
}

# ---- spinner ------------------------------------------------------------
# Uso: swl_spinner 3   → anima ~3 segundos
swl_spinner() {
    _n="${1:-2}"
    _frames='|/-\\'
    _i=0
    _end=$((_n * 8))
    while [ "$_i" -lt "$_end" ]; do
        _f=$((_i % 4))
        # busybox printf: pegar 1 char do frames
        case $_f in
            0) _ch='|' ;;
            1) _ch='/' ;;
            2) _ch='-' ;;
            3) _ch='\\' ;;
        esac
        printf '\r%b [%s] trabalhando…%b' "$_swl_c_cyan" "$_ch" "$_swl_c_reset"
        sleep 0.125 2>/dev/null || sleep 1
        _i=$((_i + 1))
        # se sleep 0.125 não existe, loop fica curto — ok
    done
    printf '\r%b [ok] pronto.        %b\n' "$_swl_c_green" "$_swl_c_reset"
}

# ---- progress bar -------------------------------------------------------
# Uso: swl_progress 10
swl_progress() {
    _total="${1:-10}"
    [ "$_total" -gt 0 ] 2>/dev/null || _total=10
    _i=0
    while [ "$_i" -le "$_total" ]; do
        _pct=$((_i * 100 / _total))
        _filled=$((_i * 20 / _total))
        _bar=''
        _j=0
        while [ "$_j" -lt 20 ]; do
            if [ "$_j" -lt "$_filled" ]; then
                _bar="$_bar#"
            else
                _bar="$_bar-"
            fi
            _j=$((_j + 1))
        done
        printf '\r%b [%s] %3d%%%b' "$_swl_c_mag" "$_bar" "$_pct" "$_swl_c_reset"
        sleep 0.05 2>/dev/null || true
        _i=$((_i + 1))
    done
    printf '\n'
}

# ---- splash na abertura (uma vez por sessão interativa) -----------------
if [ -z "${SWL_SPLASH_DONE:-}" ]; then
    # Só em shell interativo (tem stdin em tty)
    if [ -t 0 ] 2>/dev/null; then
        SWL_SPLASH_DONE=1
        export SWL_SPLASH_DONE
        if [ -x "$_SWL_FETCH" ]; then
            "$_SWL_FETCH" 2>/dev/null || true
        elif [ -f "$_SWL_SHARE/neo-face.txt" ]; then
            cat "$_SWL_SHARE/neo-face.txt" 2>/dev/null || true
            printf '\n'
        fi
        printf '%b\n' "${_swl_c_dim}bem-vindo — swl_help para a lista de comandos${_swl_c_reset}"
        printf '\n'
    fi
fi

export PS1
export ENV
