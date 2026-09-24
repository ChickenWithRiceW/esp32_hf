#include "esp32_hf.hpp"

bool EspHf::acceptIncomingCall(void) {
  return (esp_hf_client_answer_call() == 0); 
}

bool EspHf::rejectIncomingCall(void) {
  return (esp_hf_client_reject_call() == 0); 
}
