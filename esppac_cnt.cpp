#include "esppac_cnt.h"
#include "esppac_commands_cnt.h"
// TODO: Switch to CNT commands

namespace ESPPAC
{
  namespace CNT
  {
    PanasonicACCNT::PanasonicACCNT(uart::UARTComponent *uartComponent) : ESPPAC::PanasonicAC(uartComponent) {}

    void PanasonicACCNT::setup()
    {
      PanasonicAC::setup();
    }

    void PanasonicACCNT::loop()
    {
      PanasonicAC::read_data();

      //handle_resend(); // Handle packets that need to be resent

      //handle_poll(); // Handle sending poll packets
    }

    /*
    * ESPHome control request
    */

    void PanasonicACCNT::control(const climate::ClimateCall &call)
    {
    }

    /*
    * Loop handling
    */

    void PanasonicACCNT::handle_poll()
    {
    }

    /*
    * Packet handling
    */

    bool PanasonicACCNT::verify_packet()
    {
    }

    void PanasonicACCNT::handle_packet()
    {

      //this->publish_state();
    }
  }
}
