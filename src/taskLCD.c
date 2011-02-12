/*
 * Widget headers
 */
#include "board.h"
#include "conf_usb.h"

/*
 *  freeRTOS headers
 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/*
 * Atmel specific headers.
 */

/*
 * Widget TASK headers
 */
#include "taskLCD.h"

portBASE_TYPE xStatus;
xSemaphoreHandle mutexQueLCD;
xQueueHandle lcdCMDQUE;
//xSemaphoreHandle mutexEP_IN;
struct dataLCD lcdQUEDATA;
volatile uint8_t position;
uint8_t position_saved;


// ************* support functions *************

void ___waitForLCD(void)
{
    //  Wait: new instructions may be given to the LCD screen 50us
    //    cpu_delay_us(50, FOSC0);
    vTaskDelay( 1/portTICK_RATE_MS );
}

/**
 @brief Asserts the Enable pin for a fixed amount of time to
                 toggle out a write of new command or new data.
*/
void ___toggle_e(void)
{
    lcd_e_high();
    ___waitForLCD();
    lcd_e_low();
}

/**
 @brief   Writes a nibble to the LCD.
 @param  selectedRegister - either the command or data register
                                    that will be written to
 @param  nibble - the four bits to be written to the LCD
*/
void ___writeNibbleToLCD(uint8_t selectedRegister, uint8_t nibble)
{
    //  Pull the enable line high
    //lcd_e_high();

    //  Output the nibble to the LCD
    //  FIXME there must be a smoother method than bit banging
    gpio_clr_gpio_pin(LCD_D4);
    gpio_clr_gpio_pin(LCD_D5);
    gpio_clr_gpio_pin(LCD_D6);
    gpio_clr_gpio_pin(LCD_D7);
    if(nibble & 0x01) gpio_set_gpio_pin(LCD_D4);
    if(nibble & 0x02) gpio_set_gpio_pin(LCD_D5);
    if(nibble & 0x04) gpio_set_gpio_pin(LCD_D6);
    if(nibble & 0x08) gpio_set_gpio_pin(LCD_D7);


    //  Determine if the register select bit needs to be set
    if(selectedRegister == DATA_REGISTER)
    {
        //  If so, then set it
        lcd_rs_high();
    }
    else
    {
        //  Otherwise, clear the register select bit
        lcd_rs_low();
    }
    lcd_rw_low();

    //  Toggle the enable line to latch the nibble
    //lcd_e_low();
    lcd_e_toggle();
}

/**
 @brief   Writes an 8-bit value to the LCD screen.
 @param   selectedRegister - either the command or data register
                                    that will be written to
 @param   byte - the eight bits to be written to the LCD
*/
void ___writeByteToLCD(uint8_t selectedRegister, uint8_t byte)
{
    //  Generate the high and low part of the bytes that will be
    //  written to the LCD
    uint8_t upperNibble = byte >> 4;
    uint8_t lowerNibble = byte & 0x0f;

    //  Assuming a 4 line by 20 character display, ensure that
    //  everything goes where it is supposed to go:
    if(selectedRegister == DATA_REGISTER) 
    switch (position) {
        case  0: //___writeByteToLCD(COMMAND_REGISTER, (1 << LCD_CLR) ); vTaskDelay( 10/portTICK_RATE_MS );
                 ___writeByteToLCD(COMMAND_REGISTER, 0x80);  vTaskDelay( 2/portTICK_RATE_MS );
            break;

        case 20: ___writeByteToLCD(COMMAND_REGISTER, 0xC0);  vTaskDelay( 2/portTICK_RATE_MS );
            break;

        case 40: ___writeByteToLCD(COMMAND_REGISTER, 0x94);  vTaskDelay( 2/portTICK_RATE_MS );
            break;

        case 60: ___writeByteToLCD(COMMAND_REGISTER, 0xd4);  vTaskDelay( 2/portTICK_RATE_MS );
            break;

        case 80: ___writeByteToLCD(COMMAND_REGISTER, 0x80);   vTaskDelay( 2/portTICK_RATE_MS );
            break;

    }

    //  Wait for the LCD to become ready
    ___waitForLCD();

    //  First, write the upper four bits to the LCD
    ___writeNibbleToLCD(selectedRegister, upperNibble);

    //  Finally, write the lower four bits to the LCD
    ___writeNibbleToLCD(selectedRegister, lowerNibble);

    //  Reset the position count if it is equal to 80
    if(selectedRegister == DATA_REGISTER && ++position == 80)
		position = 0;

}

void ___lcd_puthex(U8 c)
{
	U8 ch, cl;
	ch = c >> 4;
	cl = c & 0x0f;
	if (ch < 10) ___writeByteToLCD(DATA_REGISTER, (ch+'0'));
	else ___writeByteToLCD(DATA_REGISTER,(ch+'0'+7));
	if (cl < 10) ___writeByteToLCD(DATA_REGISTER,(cl+'0'));
	else ___writeByteToLCD(DATA_REGISTER,(cl+'0'+7));
}


// **************  TASK DRIVER  ****************
static void vtaskLCD( void * pcParameters ) {

    portBASE_TYPE xStatus;
    struct dataLCD lcdQUEDATA;
    

    // Create a queue capable of holding 50 LCD data structures.
    lcdCMDQUE = xQueueCreate( 50, sizeof ( struct dataLCD ) );
    
    for( ;; )
    {
        xStatus = xQueueReceive( lcdCMDQUE, &lcdQUEDATA, portMAX_DELAY );
        if( xStatus == pdPASS )
        {
            switch ( lcdQUEDATA.CMD ) {

#if defined(LCD_ENABLED)
                case lcdINIT:   position = 0;
                                //  First, delay50us for at least 15ms after power on
                                vTaskDelay( 15/portTICK_RATE_MS );

                                //  Set for 4-bit LCD mode
                                ___writeNibbleToLCD( COMMAND_REGISTER, 0x03 );  vTaskDelay( 5/portTICK_RATE_MS );
                                ___writeNibbleToLCD( COMMAND_REGISTER, 0x03 );  vTaskDelay( 5/portTICK_RATE_MS );
                                ___writeNibbleToLCD( COMMAND_REGISTER, 0x03 );  vTaskDelay( 5/portTICK_RATE_MS );
                                ___writeNibbleToLCD( COMMAND_REGISTER, 0x02 );  vTaskDelay( 5/portTICK_RATE_MS );

                                //  Function set
                                ___writeByteToLCD(COMMAND_REGISTER, (1 << LCD_FUNCTION) | 
                                                                    (1 << LCD_FUNCTION_2LINES));
                                vTaskDelay( 12/portTICK_RATE_MS );
                                //  Turn display off
                                ___writeByteToLCD(COMMAND_REGISTER, (1 << LCD_ON));
                                vTaskDelay( 12/portTICK_RATE_MS );
                                //  Clear LCD and return home
                                ___writeByteToLCD(COMMAND_REGISTER, (1 << LCD_CLR));
                                vTaskDelay( 12/portTICK_RATE_MS );
                                //  Turn on display, no blink no cursor
                                ___writeByteToLCD(COMMAND_REGISTER, ( LCD_DISP_ON));
                                vTaskDelay( 12/portTICK_RATE_MS );
                        break;

                case lcdHOME:  ___writeByteToLCD(COMMAND_REGISTER, (1 << LCD_HOME) );
                                position = 0;
                        break;

                case lcdCLEAR:  ___writeByteToLCD(COMMAND_REGISTER, (1 << LCD_CLR) );
                                position = 0;
                        break;
                         
                case lcdWRITE:
                        break;
                        
                case lcdGOTO:   position = (lcdQUEDATA.data.scrnPOS.row * 20 ) + lcdQUEDATA.data.scrnPOS.col;
                                //  Send the correct commands to the command register of the LCD
                                if(position < 20)
                                    ___writeByteToLCD(COMMAND_REGISTER, 0x80 | position);
                                else if(position >= 20 && position < 40)
                                    ___writeByteToLCD(COMMAND_REGISTER, 0x80 | (position % 20 + 0x40));
                                else if(position >= 40 && position < 60)
                                    ___writeByteToLCD(COMMAND_REGISTER, 0x80 | (position % 40 + 0x14));
                                else if(position >= 60 && position < 80)
                                    ___writeByteToLCD(COMMAND_REGISTER, 0x80 | (position % 60 + 0x54));
                        break;
                         
                case lcdPUTC:
								// Display a single char at the current cursor position
                                ___writeByteToLCD( DATA_REGISTER, (uint8_t) lcdQUEDATA.data.aChar );
                        break;

                case lcdPUTH:	// Display a single hexadecimal value at current cursor position
								___lcd_puthex((U8) lcdQUEDATA.data.aChar);
						break;

                case lcdPUTS:   // Write a null terminated string to LCD at current cursor position
                                while (*lcdQUEDATA.data.aString)
                                    ___writeByteToLCD(DATA_REGISTER, *lcdQUEDATA.data.aString++);
                        break;
                         
                case lcdSET:    ___writeByteToLCD(COMMAND_REGISTER, lcdQUEDATA.data.rawBYTE );
                        break;
                        
                case lcdCRLF: // CR received.  Move cursor down to next line
						if (position < 20) position = 20;
						else if (position < 40) position = 40;
						else if (position < 60) position = 60;
						else position = 0;

                        if(position < 20)
                             ___writeByteToLCD(COMMAND_REGISTER, 0x80 | position);
                         else if(position >= 20 && position < 40)
                             ___writeByteToLCD(COMMAND_REGISTER, 0x80 | (position % 20 + 0x40));
                         else if(position >= 41 && position < 60)
                             ___writeByteToLCD(COMMAND_REGISTER, 0x80 | (position % 40 + 0x14));
                         else if(position >= 61 && position < 80)
                             ___writeByteToLCD(COMMAND_REGISTER, 0x80 | (position % 60 + 0x54));
                        break;

                case lcdPOSW:	// save the present cursor position
						position_saved = position;
						break;

                case lcdPOSR:  // read back the present cursor position
					    position = position_saved;
						if(position < 20)
								 ___writeByteToLCD(COMMAND_REGISTER, 0x80 | position);
						 else if(position >= 20 && position < 40)
							 ___writeByteToLCD(COMMAND_REGISTER, 0x80 | (position % 20 + 0x40));
						 else if(position >= 41 && position < 60)
							 ___writeByteToLCD(COMMAND_REGISTER, 0x80 | (position % 40 + 0x14));
						 else if(position >= 61 && position < 80)
							 ___writeByteToLCD(COMMAND_REGISTER, 0x80 | (position % 60 + 0x54));

					    break;
#endif

                default:
                    break;
                
            }

        }
        else  vTaskDelay( 50/portTICK_RATE_MS );
    }
}


void vStartTaskLCD( unsigned portBASE_TYPE uxPriority )
{
	mutexQueLCD = xSemaphoreCreateMutex();

	xStatus = xTaskCreate( vtaskLCD, 
                         ( signed char * ) "LCDpanel", 
                           configMINIMAL_STACK_SIZE, 
                           NULL, 
                           uxPriority, 
                         ( xTaskHandle * ) NULL );

}

// ************* queue entry functions *************

void lcd_q_init(void)
{
	xSemaphoreTake( mutexQueLCD, portMAX_DELAY );
	lcdQUEDATA.CMD=lcdINIT;
	xStatus = xQueueSendToBack( lcdCMDQUE, &lcdQUEDATA, portMAX_DELAY );
    xSemaphoreGive( mutexQueLCD );
}

void lcd_q_clear(void)
{
	xSemaphoreTake( mutexQueLCD, portMAX_DELAY );
	lcdQUEDATA.CMD=lcdCLEAR;
	xStatus = xQueueSendToBack( lcdCMDQUE, &lcdQUEDATA, portMAX_DELAY );
    xSemaphoreGive( mutexQueLCD );
}

void lcd_q_crlf(void)
{
	xSemaphoreTake( mutexQueLCD, portMAX_DELAY );
	lcdQUEDATA.CMD=lcdCRLF;
	xStatus = xQueueSendToBack( lcdCMDQUE, &lcdQUEDATA, portMAX_DELAY );
    xSemaphoreGive( mutexQueLCD );
}

void lcd_q_goto(uint8_t row, uint8_t col)
{
	xSemaphoreTake( mutexQueLCD, portMAX_DELAY );
	lcdQUEDATA.CMD=lcdGOTO;
	lcdQUEDATA.data.scrnPOS.row = row;
	lcdQUEDATA.data.scrnPOS.col = col;
	xStatus = xQueueSendToBack( lcdCMDQUE, &lcdQUEDATA, portMAX_DELAY );
    xSemaphoreGive( mutexQueLCD );
}

void lcd_q_puth(uint8_t hex)
{
	xSemaphoreTake( mutexQueLCD, portMAX_DELAY );
    lcdQUEDATA.CMD=lcdPUTH;
    lcdQUEDATA.data.aChar=hex;
    xStatus = xQueueSendToBack( lcdCMDQUE, &lcdQUEDATA, portMAX_DELAY );
    xSemaphoreGive( mutexQueLCD );
}

void lcd_q_print(char *string)
{
	xSemaphoreTake( mutexQueLCD, portMAX_DELAY );
    lcdQUEDATA.CMD=lcdPUTS;
    lcdQUEDATA.data.aString = (uint8_t*) string;
	xStatus = xQueueSendToBack( lcdCMDQUE, &lcdQUEDATA, portMAX_DELAY );
    xSemaphoreGive( mutexQueLCD );
}

void lcd_q_set(uint8_t cmd)
{
	xSemaphoreTake( mutexQueLCD, portMAX_DELAY );
    lcdQUEDATA.CMD=lcdSET;
    lcdQUEDATA.data.rawBYTE = cmd;
	xStatus = xQueueSendToBack( lcdCMDQUE, &lcdQUEDATA, portMAX_DELAY );
    xSemaphoreGive( mutexQueLCD );
}
