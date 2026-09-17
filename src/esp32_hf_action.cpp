#include "esp32_hf.h"

bool EspHf::acceptIncomingCall(void) {
  return (esp_hf_client_answer_call() == 0); 
}