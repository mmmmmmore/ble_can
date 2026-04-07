#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"

CANDIDATE_IDF_PATHS=(
  "${IDF_PATH:-}"
  "$HOME/Gitprj/Tools/esp-idf/esp-idf"
  "$HOME/maochun/Gitprj/Tools/esp-idf/esp-idf"
)

for path in "${CANDIDATE_IDF_PATHS[@]}"; do
  if [[ -n "$path" && -f "$path/export.sh" ]]; then
    export IDF_PATH="$path"
    break
  fi
done

if [[ -z "${IDF_PATH:-}" || ! -f "$IDF_PATH/export.sh" ]]; then
  echo "ERROR: ESP-IDF not found." >&2
  echo "Please set IDF_PATH or install ESP-IDF under $HOME/Gitprj/Tools/esp-idf/esp-idf" >&2
  exit 1
fi

echo "Using IDF_PATH=$IDF_PATH"
# shellcheck disable=SC1090
source "$IDF_PATH/export.sh"

cd "$PROJECT_DIR"

if [[ ! -f sdkconfig ]]; then
  echo "Configuring target: esp32s3"
  idf.py set-target esp32s3
fi

case "${1:-env}" in
  env)
    echo "ESP-IDF environment is ready in $PROJECT_DIR"
    echo "Examples:"
    echo "  ./setup.sh build"
    echo "  ./setup.sh flash -p /dev/tty.usbmodem5B3E0301661"
    echo "  ./setup.sh monitor -p /dev/tty.usbmodem5B3E0301661"
    ;;
  build)
    shift
    idf.py build "$@"
    ;;
  flash)
    shift
    idf.py flash "$@"
    ;;
  monitor)
    shift
    idf.py monitor "$@"
    ;;
  clean)
    idf.py fullclean
    ;;
  *)
    "$@"
    ;;
esac
