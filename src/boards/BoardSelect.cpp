#include "boards/BoardSelect.h"
#include "core/Board.h"

#if defined(BOARD_ESP32S3_35)
#include "core/boards/Esp32S3Board.h"
static core::Esp32S3Board s_board;
#elif defined(BOARD_ESP32P4_43)
#include "core/boards/Esp32P4Board.h"
static core::Esp32P4Board s_board;
#else
// Default: ESP32-C6 Touch 1.47"
#include "core/boards/Esp32C6Board.h"
static core::Esp32C6Board s_board;
#endif

core::Board& getBoard() {
    return s_board;
}
