#ifdef DEBUG
  #include "SEGGER_RTT.h"
  // Якщо дебаг увімкнено — логуємо на повну з кольорами
  #define LOG_INFO(fmt, ...) SEGGER_RTT_printf(0, RTT_CTRL_TEXT_BRIGHT_GREEN "[INFO] " fmt RTT_CTRL_RESET "\r\n", ##__VA_ARGS__)
  #define LOG_ERR(fmt, ...)  SEGGER_RTT_printf(0, RTT_CTRL_TEXT_BRIGHT_RED   "[ERR]  " fmt RTT_CTRL_RESET "\r\n", ##__VA_ARGS__)
  #define LOG_WARN(fmt, ...)  SEGGER_RTT_printf(0, RTT_CTRL_TEXT_BRIGHT_YELLOW   "[WARN]  " fmt RTT_CTRL_RESET "\r\n", ##__VA_ARGS__)
#else
  // Якщо це реліз — макроси просто «схлопуються» в ніщо і компілятор повністю вирізає їх із прошивки
  #define LOG_INFO(fmt, ...)
  #define LOG_ERR(fmt, ...)
  #define LOG_WARN(fmt, ...)
#endif
