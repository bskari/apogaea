#include <stdint.h>
struct RadioConfigMessage_t {
  int8_t brightness; // from 0 to 100
  int8_t sensitivity; // from 0 to 100
  int8_t speed; // from 0 to 100
  uint8_t rainbow; // =1 if switch ON and =0 if OFF, from 0 to 1
  uint8_t normalizeBands; // =1 if switch ON and =0 if OFF, from 0 to 1
  uint8_t rgbButton; // =1 if button pressed, else =0, from 0 to 1
  uint16_t rgb; // bitwise flag for the 15 LED strips that determines if that strip is RGB or not
};
