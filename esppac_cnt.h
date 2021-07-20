#include "esphome.h"
#include "esppac.h"

namespace ESPPAC
{
  namespace CNT
  {
    class PanasonicACCNT : public ESPPAC::PanasonicAC
    {
      public:
        PanasonicACCNT(uart::UARTComponent *uartComponent);

        void control(const climate::ClimateCall &call) override;

        void setup() override;
        void loop() override;

      private:
        int receiveBufferIndex = 0; // Current position of the receive buffer
        byte receiveBuffer[ESPPAC::BUFFER_SIZE]; // Stores the packet currently being received

        void handle_poll();

        bool verify_packet();
        void handle_packet();
    };
  }
}
