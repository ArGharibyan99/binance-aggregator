#!/usr/bin/env bash
# Colorized viewer for market_stats.log (or any file in the same
# StatsSerializer output format). Purely a display helper -- the log
# file itself stays plain text, no ANSI codes are ever written to it.
#
# Usage:
#   scripts/watch_stats.sh                 # show + follow market_stats.log
#   scripts/watch_stats.sh path/to/file    # show + follow a different file
#   scripts/watch_stats.sh -c path/to/file # print the whole file once, no follow

set -euo pipefail

follow=1
if [[ "${1:-}" == "-c" ]]; then
    follow=0
    shift
fi

file="${1:-market_stats.log}"

esc=$'\033'
reset="${esc}[0m"
bold_cyan="${esc}[1;36m"
yellow="${esc}[33m"
green="${esc}[32m"
red="${esc}[31m"

# sed -u: unbuffered, flushes output after every line. This matters for
# following a live file: this system's /usr/bin/awk (mawk) buffers its
# reads/writes internally in a way that neither fflush() nor `stdbuf`
# can override, so a `tail -f | awk` pipeline can silently stall for a
# long time before showing anything. sed -u does not have that problem.
colorize() {
    sed -u \
        -e "s/^timestamp=.*/${bold_cyan}&${reset}/" \
        -e "s/symbol=[A-Za-z0-9]\+/${yellow}&${reset}/" \
        -e "s/buy=[0-9]\+/${green}&${reset}/" \
        -e "s/sell=[0-9]\+/${red}&${reset}/"
}

if [[ "$follow" -eq 1 ]]; then
    tail -n +1 -f "$file" | colorize
else
    colorize < "$file"
fi
