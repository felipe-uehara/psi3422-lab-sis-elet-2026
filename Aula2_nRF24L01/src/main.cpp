#include "mbed.h"
#include "nRF24L01P.h"

// The nRF24L01+ supports transfers from 1 to 32 bytes (some boards may not handle many bytes)
#define TRANSFER_SIZE   1 

#define WAIT_TIME_MS 500 // macro for generic delay time, in milisseconds

/*** Pins definitions***/
#define MOSI    PTD2    //D11
#define MISO    PTD3    //D12
#define SCK     PTC5    
#define CS      PTD0    //D10
#define CE      PTD5    //D9
#define IRQ     PTA13   //D8

DigitalOut red(LED_RED); 
DigitalOut blue(LED_BLUE); 

nRF24L01P my_nrf24l01p(MOSI, MISO, SCK, CS, CE, IRQ); // module instance - FRDM KL25Z
// nRF24L01P my_nrf24l01p(D11, D12, D4, D10, D9, D8); // module instance (Arduino pinout - compatible to other boards)

/* Objeto Serial */
static BufferedSerial pc(USBTX, USBRX, 9600); 

int main() {

    char txData[TRANSFER_SIZE], rxData[TRANSFER_SIZE];
    int txDataCnt = 0;
    int rxDataCnt = 0;

    my_nrf24l01p.powerUp();

    // Display the (default) setup of the nRF24L01+ chip
    printf( "nRF24L01+ Frequency    : %d MHz\r\n",  my_nrf24l01p.getRfFrequency() );
    printf( "nRF24L01+ Output power : %d dBm\r\n",  my_nrf24l01p.getRfOutputPower() );
    printf( "nRF24L01+ Data Rate    : %d kbps\r\n", my_nrf24l01p.getAirDataRate() );
    printf( "nRF24L01+ TX Address   : 0x%010llX\r\n", my_nrf24l01p.getTxAddress() );
    printf( "nRF24L01+ RX Address   : 0x%010llX\r\n", my_nrf24l01p.getRxAddress() );

    printf( "Type keys to test transfers:\r\n  (transfers are grouped into %d characters)\r\n", TRANSFER_SIZE );

    my_nrf24l01p.setTransferSize( TRANSFER_SIZE );

    my_nrf24l01p.setReceiveMode();
    my_nrf24l01p.enable();

    while (true)
    {

        // If we've received anything over the host serial link...
        if ( pc.readable() ) {

            // ...add it to the transmit buffer
            // txData[txDataCnt++] = getc();
            pc.read(txData,TRANSFER_SIZE);
            txDataCnt++;

            // If the transmit buffer is full
            if ( txDataCnt >= sizeof( txData ) ) {

                // Send the transmitbuffer via the nRF24L01+
                my_nrf24l01p.write( NRF24L01P_PIPE_P0, txData, txDataCnt );

                txDataCnt = 0;
            }

            // Toggle LED1 (to help debug Host -> nRF24L01+ communication)
            red = !red;
        }

        // If we've received anything in the nRF24L01+...
        if ( my_nrf24l01p.readable() ) {

            // ...read the data into the receive buffer
            rxDataCnt = my_nrf24l01p.read( NRF24L01P_PIPE_P0, rxData, sizeof( rxData ) );

            // Display the receive buffer contents via the host serial link
            for ( int i = 0; rxDataCnt > 0; rxDataCnt--, i++ ) {
                printf("%c",rxData[i]); // putc( rxData[i] );
            }

            // Toggle LED2 (to help debug nRF24L01+ -> Host communication)
            blue = !blue;
        }

    }
}