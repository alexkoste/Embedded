#!/usr/bin/env bash
#
# Автоматизує підключення бібліотеки u8g2 до ESP-IDF проєкту (ESP32 / ESP32-S3),
# використовуючи новий I2C-драйвер driver/i2c_master.h.
#
# Що робить:
#   1. Створює main/idf_component.yml із git-залежністю на u8g2.
#   2. Створює main/u8g2_hal.h і main/u8g2_hal.c — готовий, перевірений на реальній
#      платі HAL-міст між u8g2 і driver/i2c_master.h.
#   3. Пробує підключити u8g2_hal.c і компонент u8g2 у main/CMakeLists.txt.
#   4. Запускає idf.py reconfigure, щоб одразу довантажити бібліотеку і перевірити,
#      що CMake-конфігурація коректна.
#
# Використання:
#   ./setup_u8g2.sh [шлях_до_проєкту] [опції]
#
# Опції:
#   --version X.Y.Z     версія (git tag) u8g2 (типово 2.37.1)
#   --force              перезаписати main/u8g2_hal.c/.h, якщо вже існують
#   --no-reconfigure      не запускати idf.py reconfigure наприкінці
#   -h, --help            показати цю довідку
#
# Приклад:
#   ./setup_u8g2.sh .                 # налаштувати поточний проєкт
#   ./setup_u8g2.sh ~/labs/my_oled    # налаштувати інший проєкт

set -euo pipefail

U8G2_VERSION="2.37.1"
FORCE=0
DO_RECONFIGURE=1
PROJECT_DIR="."

print_help() {
    sed -n '2,26p' "$0" | sed 's/^# \{0,1\}//'
}

while [ $# -gt 0 ]; do
    case "$1" in
        --version)
            U8G2_VERSION="$2"
            shift 2
            ;;
        --force)
            FORCE=1
            shift
            ;;
        --no-reconfigure)
            DO_RECONFIGURE=0
            shift
            ;;
        -h|--help)
            print_help
            exit 0
            ;;
        -*)
            echo "Невідома опція: $1" >&2
            exit 1
            ;;
        *)
            PROJECT_DIR="$1"
            shift
            ;;
    esac
done

log()  { printf '==> %s\n' "$1"; }
warn() { printf 'УВАГА: %s\n' "$1" >&2; }
die()  { printf 'ПОМИЛКА: %s\n' "$1" >&2; exit 1; }

MAIN_DIR="$PROJECT_DIR/main"
CMAKE="$MAIN_DIR/CMakeLists.txt"
COMPONENT_YML="$MAIN_DIR/idf_component.yml"
HAL_H="$MAIN_DIR/u8g2_hal.h"
HAL_C="$MAIN_DIR/u8g2_hal.c"

[ -f "$PROJECT_DIR/CMakeLists.txt" ] && [ -d "$MAIN_DIR" ] \
    || die "не знайдено ESP-IDF проєкт у '$PROJECT_DIR' (очікую CMakeLists.txt і папку main/)"
[ -f "$CMAKE" ] || die "не знайдено $CMAKE"

log "Проєкт: $(cd "$PROJECT_DIR" && pwd)"

if [ -z "${IDF_PATH:-}" ]; then
    warn "змінна IDF_PATH не встановлена — файли підготую, але автозбірку в кінці пропущу."
    warn "Спочатку виконай у своєму терміналі: . \$IDF_PATH/export.sh (Linux/macOS) або . export.ps1 (Windows)."
    DO_RECONFIGURE=0
fi

# --- 1. idf_component.yml -----------------------------------------------
if [ -f "$COMPONENT_YML" ] && grep -q '^\s*u8g2:' "$COMPONENT_YML" 2>/dev/null; then
    log "idf_component.yml вже містить залежність u8g2 — пропускаю"
else
    log "Створюю $COMPONENT_YML (u8g2 $U8G2_VERSION з git)"
    cat > "$COMPONENT_YML" <<EOF
dependencies:
  u8g2:
    git: https://github.com/olikraus/u8g2.git
    version: "$U8G2_VERSION"
EOF
fi

# --- 2. HAL-файли ----------------------------------------------------------
write_hal_h() {
    cat > "$HAL_H" <<'EOF'
#pragma once

#include "driver/i2c_master.h"
#include "u8g2.h"

// Прив'язує u8g2 callback-и до вже створеного I2C device handle (новий driver/i2c_master.h)
void u8g2_hal_set_i2c_device(i2c_master_dev_handle_t dev_handle);

uint8_t u8g2_esp32_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
EOF
}

write_hal_c() {
    cat > "$HAL_C" <<'EOF'
#include "u8g2_hal.h"

#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "U8G2_HAL";
static i2c_master_dev_handle_t s_dev_handle;

void u8g2_hal_set_i2c_device(i2c_master_dev_handle_t dev_handle)
{
    s_dev_handle = dev_handle;
}

uint8_t u8g2_esp32_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    // u8g2 надсилає I2C-транзакцію шматками по 32 байти (SEND) між START/END_TRANSFER
    static uint8_t buffer[32];
    static uint8_t buf_idx;

    switch (msg) {
        case U8X8_MSG_BYTE_START_TRANSFER:
            buf_idx = 0;
            break;

        case U8X8_MSG_BYTE_SEND: {
            const uint8_t *data = (const uint8_t *)arg_ptr;
            for (uint8_t i = 0; i < arg_int && buf_idx < sizeof(buffer); i++) {
                buffer[buf_idx++] = data[i];
            }
            break;
        }

        case U8X8_MSG_BYTE_END_TRANSFER: {
            esp_err_t err = i2c_master_transmit(s_dev_handle, buffer, buf_idx, 100);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "i2c_master_transmit failed: %s", esp_err_to_name(err));
            }
            break;
        }

        case U8X8_MSG_BYTE_INIT:
        default:
            break;
    }
    return 1;
}

uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch (msg) {
        case U8X8_MSG_DELAY_MILLI:
            vTaskDelay(pdMS_TO_TICKS(arg_int));
            break;

        case U8X8_MSG_DELAY_10MICRO:
            esp_rom_delay_us(10);
            break;

        case U8X8_MSG_DELAY_100NANO:
            esp_rom_delay_us(1);
            break;

        case U8X8_MSG_GPIO_AND_DELAY_INIT:
        case U8X8_MSG_GPIO_RESET:
        default:
            // апаратний I2C, окремі GPIO для reset/clock/data не використовуються
            break;
    }
    return 1;
}
EOF
}

if [ -f "$HAL_H" ] && [ "$FORCE" -ne 1 ]; then
    log "$HAL_H вже існує — пропускаю (--force, щоб перезаписати)"
else
    log "Створюю $HAL_H"
    write_hal_h
fi

if [ -f "$HAL_C" ] && [ "$FORCE" -ne 1 ]; then
    log "$HAL_C вже існує — пропускаю (--force, щоб перезаписати)"
else
    log "Створюю $HAL_C"
    write_hal_c
fi

# --- 3. CMakeLists.txt ---------------------------------------------------
if grep -q 'u8g2_hal\.c' "$CMAKE" && grep -qE '(^|[^_a-zA-Z0-9])u8g2([^_a-zA-Z0-9]|$)' "$CMAKE"; then
    log "$CMAKE вже підключає u8g2_hal.c і компонент u8g2 — пропускаю"
else
    cp "$CMAKE" "$CMAKE.bak"
    PATCHED_SRCS=0
    PATCHED_REQUIRES=0

    # SRCS: квотований варіант "main.c" (типовий idf.py create-project) або звичайний main.c
    if grep -q '"main\.c"' "$CMAKE" && ! grep -q 'u8g2_hal\.c' "$CMAKE"; then
        sed -i 's/"main\.c"/"main.c" "u8g2_hal.c"/' "$CMAKE"
        PATCHED_SRCS=1
    elif grep -qE '(^|[[:space:]])main\.c([[:space:]]|$)' "$CMAKE" && ! grep -q 'u8g2_hal\.c' "$CMAKE"; then
        sed -i 's/\bmain\.c\b/main.c u8g2_hal.c/' "$CMAKE"
        PATCHED_SRCS=1
    fi

    # REQUIRES: якщо блок REQUIRES вже є — додаємо туди рядок "u8g2"
    # (порожній REQUIRES у стандартному шаблоні йде з коментарем, напр. "REQUIRES  # optional, ...")
    if grep -qE '^\s*REQUIRES\s*(#.*)?$' "$CMAKE"; then
        sed -i -E '/^\s*REQUIRES\s*(#.*)?$/a\    u8g2' "$CMAKE"
        PATCHED_REQUIRES=1
    elif grep -qE '^\s*REQUIRES[[:space:]]+[a-zA-Z]' "$CMAKE"; then
        sed -i -E '0,/^\s*REQUIRES[[:space:]]+/s//&u8g2 /' "$CMAKE"
        PATCHED_REQUIRES=1
    fi

    if [ "$PATCHED_SRCS" -eq 1 ] && [ "$PATCHED_REQUIRES" -eq 1 ]; then
        log "Оновив $CMAKE (резервна копія: $CMAKE.bak)"
    else
        mv "$CMAKE.bak" "$CMAKE"
        warn "не вдалося автоматично розпізнати формат $CMAKE — онови вручну:"
        cat >&2 <<'EOF'

    idf_component_register(
        SRCS main.c u8g2_hal.c
        INCLUDE_DIRS "."
        REQUIRES driver esp_timer log freertos u8g2
    )

EOF
    fi
fi

# --- 4. Перевірка збірки ---------------------------------------------------
if [ "$DO_RECONFIGURE" -eq 1 ] && command -v idf.py >/dev/null 2>&1; then
    log "Довантажую u8g2 і перевіряю CMake-конфігурацію (idf.py reconfigure)"
    (cd "$PROJECT_DIR" && idf.py reconfigure)
    log "Готово! Бібліотека u8g2 підключена. Далі: idf.py build && idf.py -p COMx/ttyUSBx flash monitor"
else
    log "Готово! Файли підготовлені. Далі виконай вручну:"
    echo "    idf.py set-target esp32s3   # або свій target"
    echo "    idf.py reconfigure          # довантажить u8g2 з git"
    echo "    idf.py build"
fi
