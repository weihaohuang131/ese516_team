/**************************************************************************//**
* @file      CliThread.c
* @brief     File for the CLI Thread handler. Uses FREERTOS + CLI
* @author    Eduardo Garcia
* @date      2020-02-15

******************************************************************************/


/******************************************************************************
* Includes
******************************************************************************/
#include "CliThread.h"
#include "IMU\lsm6dso_reg.h"
#include "SeesawDriver/Seesaw.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/******************************************************************************
* Defines
******************************************************************************/


/******************************************************************************
* Variables
******************************************************************************/
static const int8_t * const pcWelcomeMessage =
"FreeRTOS CLI.\r\nType Help to view a list of registered commands.\r\n";

SemaphoreHandle_t cliCharReadySemaphore;		///<Semaphore to indicate that a character has been received


static const CLI_Command_Definition_t xImuGetCommand =
{
	"imu",
	"imu: Returns a value from the IMU\r\n",
	CLI_GetImuData,
	0
};


static const CLI_Command_Definition_t xResetCommand =
{
	"reset",
	"reset: Resets the device\r\n",
	CLI_ResetDevice,
	0
};

static const CLI_Command_Definition_t xNeotrellisTurnLEDCommand =
{
	"led",
	"led [keynum][R][G][B]: Sets the given LED to the given R,G,B values.\r\n",
	CLI_NeotrellisSetLed,
	4
};



//Clear screen command
const CLI_Command_Definition_t xClearScreen =
{
	CLI_COMMAND_CLEAR_SCREEN,
	CLI_HELP_CLEAR_SCREEN,
	CLI_CALLBACK_CLEAR_SCREEN,
	CLI_PARAMS_CLEAR_SCREEN
};


/******************************************************************************
* Forward Declarations
******************************************************************************/
static void FreeRTOS_read(char* character);

/******************************************************************************
* Callback Functions
******************************************************************************/


/******************************************************************************
* CLI Thread
******************************************************************************/

void vCommandConsoleTask( void *pvParameters )
{
//REGISTER COMMANDS HERE

FreeRTOS_CLIRegisterCommand( &xImuGetCommand );
FreeRTOS_CLIRegisterCommand( &xClearScreen );
FreeRTOS_CLIRegisterCommand( &xResetCommand );
FreeRTOS_CLIRegisterCommand( &xNeotrellisTurnLEDCommand );

uint8_t cRxedChar[2], cInputIndex = 0;
BaseType_t xMoreDataToFollow;
/* The input and output buffers are declared static to keep them off the stack. */
static int8_t pcOutputString[ MAX_OUTPUT_LENGTH_CLI  ], pcInputString[ MAX_INPUT_LENGTH_CLI ];
static char pcLastCommand[ MAX_INPUT_LENGTH_CLI ];
static bool isEscapeCode = false;
static char pcEscapeCodes [4];
static uint8_t pcEscapeCodePos = 0;

    /* This code assumes the peripheral being used as the console has already
    been opened and configured, and is passed into the task as the task
    parameter.  Cast the task parameter to the correct type. */

    /* Send a welcome message to the user knows they are connected. */
    SerialConsoleWriteString( pcWelcomeMessage);

	//Any semaphores/mutexes/etc you needed to be initialized, you can do them here
	cliCharReadySemaphore = xSemaphoreCreateBinary();
	if(cliCharReadySemaphore == NULL)
	{
		LogMessage(LOG_ERROR_LVL, "Could not allocate semaphore\r\n");
		vTaskSuspend( NULL );
	}


    for( ;; )
    {

	FreeRTOS_read(&cRxedChar[0]);

	if( cRxedChar[0] == '\n' || cRxedChar[0] == '\r'  )
        {
            /* A newline character was received, so the input command string is
            complete and can be processed.  Transmit a line separator, just to
            make the output easier to read. */
            SerialConsoleWriteString("\r\n");
			//Copy for last command
			isEscapeCode = false; pcEscapeCodePos = 0;
			strncpy(pcLastCommand, pcInputString, MAX_INPUT_LENGTH_CLI-1);
			pcLastCommand[MAX_INPUT_LENGTH_CLI-1] = 0;	//Ensure null termination

            /* The command interpreter is called repeatedly until it returns
            pdFALSE.  See the "Implementing a command" documentation for an
            explanation of why this is. */
            do
            {
                /* Send the command string to the command interpreter.  Any
                output generated by the command interpreter will be placed in the
                pcOutputString buffer. */
                xMoreDataToFollow = FreeRTOS_CLIProcessCommand
                              (
                                  pcInputString,   /* The command string.*/
                                  pcOutputString,  /* The output buffer. */
                                  MAX_OUTPUT_LENGTH_CLI/* The size of the output buffer. */
                              );

                /* Write the output generated by the command interpreter to the
                console. */
				//Ensure it is null terminated
				pcOutputString[MAX_OUTPUT_LENGTH_CLI - 1] = 0;
                SerialConsoleWriteString(pcOutputString);

            } while( xMoreDataToFollow != pdFALSE );

            /* All the strings generated by the input command have been sent.
            Processing of the command is complete.  Clear the input string ready
            to receive the next command. */
            cInputIndex = 0;
            memset( pcInputString, 0x00, MAX_INPUT_LENGTH_CLI );
			memset( pcOutputString, 0, MAX_OUTPUT_LENGTH_CLI);
        }
        else
        {
		            /* The if() clause performs the processing after a newline character
            is received.  This else clause performs the processing if any other
            character is received. */
		
			if (true == isEscapeCode) {

				if(pcEscapeCodePos < CLI_PC_ESCAPE_CODE_SIZE) {
					pcEscapeCodes[pcEscapeCodePos++] = cRxedChar[0];
				}
				else {
					isEscapeCode = false; pcEscapeCodePos = 0;
				}
			
				if (pcEscapeCodePos >= CLI_PC_MIN_ESCAPE_CODE_SIZE) {
				
					// UP ARROW SHOW LAST COMMAND
					if(strcasecmp(pcEscapeCodes, "oa"))	{
                            /// Delete current line and add prompt (">")
                            sprintf(pcInputString, "%c[2K\r>", 27);
				            SerialConsoleWriteString((char*)pcInputString);
                            /// Clear input buffer
                            cInputIndex = 0;
                            memset( pcInputString, 0x00, MAX_INPUT_LENGTH_CLI );
                        /// Send last command
						strncpy(pcInputString, pcLastCommand, MAX_INPUT_LENGTH_CLI - 1); 	
                        cInputIndex = (strlen(pcInputString) < MAX_INPUT_LENGTH_CLI - 1) ? strlen(pcLastCommand) : MAX_INPUT_LENGTH_CLI - 1;
						SerialConsoleWriteString(pcInputString);
					}
				
					isEscapeCode = false; pcEscapeCodePos = 0;
				}			
			}
            /* The if() clause performs the processing after a newline character
            is received.  This else clause performs the processing if any other
            character is received. */

            else if( cRxedChar[0] == '\r' )
            {
                /* Ignore carriage returns. */
            }
            else if( cRxedChar[0] == ASCII_BACKSPACE || cRxedChar[0] == ASCII_DELETE )
            {
				char erase[4] = {0x08, 0x20, 0x08, 0x00};
				SerialConsoleWriteString(erase);
                /* Backspace was pressed.  Erase the last character in the input
                buffer - if there are any. */
                if( cInputIndex > 0 )
                {
                    cInputIndex--;
                    pcInputString[ cInputIndex ] = 0;
                }
            }
			// ESC
			else if( cRxedChar[0] == ASCII_ESC) {
				isEscapeCode = true; //Next characters will be code arguments
				pcEscapeCodePos = 0;
			}
            else
            {
                /* A character was entered.  It was not a new line, backspace
                or carriage return, so it is accepted as part of the input and
                placed into the input buffer.  When a n is entered the complete
                string will be passed to the command interpreter. */
                if( cInputIndex < MAX_INPUT_LENGTH_CLI )
                {
                    pcInputString[ cInputIndex ] = cRxedChar[0];
                    cInputIndex++;
                }

					//Order Echo
					cRxedChar[1] = 0;
					SerialConsoleWriteString(&cRxedChar[0]);
            }
        }
    }
}



/**************************************************************************//**
* @fn			void FreeRTOS_read(char* character)
* @brief		This function block the thread unless we received a character
* @details		This function blocks until UartSemaphoreHandle is released to continue reading characters in CLI
* @note
*****************************************************************************/
static void FreeRTOS_read(char* character)
{


//We check if there are more characters in the buffer that arrived since the last time
//This function returns -1 if the buffer is empty, other value otherwise
int ret = SerialConsoleReadCharacter(character);


if(ret == -1)
{
	//there are no more characters - block the thread until we receive a semaphore indicating reception of at least 1 character
	xSemaphoreTake(cliCharReadySemaphore, portMAX_DELAY);

	//If we are here it means there are characters in the buffer - we re-read from the buffer to get the newly acquired character
	SerialConsoleReadCharacter(character);
}


}


/**************************************************************************//**
* @fn			void CliCharReadySemaphoreGiveFromISR(void)
* @brief		Give cliCharReadySemaphore binary semaphore from an ISR
* @details		
* @note
*****************************************************************************/
void CliCharReadySemaphoreGiveFromISR(void)
{
	static BaseType_t xHigherPriorityTaskWoken;
	xHigherPriorityTaskWoken = pdFALSE;
	xSemaphoreGiveFromISR( cliCharReadySemaphore, &xHigherPriorityTaskWoken );
	portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

/******************************************************************************
* CLI Functions - Define here
******************************************************************************/
//Example CLI Command. Reads from the IMU and returns data.
BaseType_t CLI_GetImuData( int8_t *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString )
{
static int16_t  data_raw_acceleration[3];
static int16_t  data_raw_angular_rate;
static float acceleration_mg[3];
uint8_t reg;
stmdev_ctx_t *dev_ctx = GetImuStruct();


/* Read output only if new xl value is available */
lsm6dso_xl_flag_data_ready_get(GetImuStruct(), &reg);

if(reg){
	//original used raw_angular_rate, which is wrong. changed to data_raw_acceleration
	memset(data_raw_acceleration, 0x00, 3 * sizeof(int16_t));
      //lsm6dso_acceleration_raw_get(&dev_ctx, data_raw_acceleration);
      lsm6dso_acceleration_raw_get(GetImuStruct(), data_raw_acceleration);
      
	  acceleration_mg[0] =
      lsm6dso_from_fs2_to_mg(data_raw_acceleration[0]);
      acceleration_mg[1] =
      lsm6dso_from_fs2_to_mg(data_raw_acceleration[1]);
      acceleration_mg[2] =
      lsm6dso_from_fs2_to_mg(data_raw_acceleration[2]);

	snprintf(pcWriteBuffer,xWriteBufferLen, "Acceleration [mg]:X %d\tY %d\tZ %d\r\n",
	(int)acceleration_mg[0], (int)acceleration_mg[1], (int)acceleration_mg[2]);
}else
{
	snprintf(pcWriteBuffer,xWriteBufferLen, "No data ready! \r\n");
}
return pdFALSE;
}


//THIS COMMAND USES vt100 TERMINAL COMMANDS TO CLEAR THE SCREEN ON A TERMINAL PROGRAM LIKE TERA TERM
//SEE http://www.csie.ntu.edu.tw/~r92094/c++/VT100.html for more info
//CLI SPECIFIC COMMANDS
static char bufCli[CLI_MSG_LEN];
BaseType_t xCliClearTerminalScreen( char *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString )
{
	char clearScreen = ASCII_ESC;
	snprintf(bufCli, CLI_MSG_LEN - 1, "%c[2J", clearScreen);
	snprintf(pcWriteBuffer, xWriteBufferLen, bufCli);
	return pdFALSE;
}


//Example CLI Command. Resets system.
BaseType_t CLI_ResetDevice( int8_t *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString )
{
	for (int i = 0; i<=15; i++) {
		SeesawSetLed(i, 0, 0, 0);
		SeesawOrderLedUpdate();
	}
	system_reset();
	//remove all on LEDs
	
	return pdFALSE;
}

/**************************************************************************//**
BaseType_t CLI_NeotrellisSetLed( int8_t *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString )
* @brief	CLI command to turn on a given LED to a given R,G,B, value
* @param[out] *pcWriteBuffer. Buffer we can use to write the CLI command response to! See other CLI examples on how we use this to write back!
* @param[in] xWriteBufferLen. How much we can write into the buffer
* @param[in] *pcCommandString. Buffer that contains the complete input. You will find the additional arguments, if needed. Please see 
https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_CLI/FreeRTOS_Plus_CLI_Implementing_A_Command.html#Example_Of_Using_FreeRTOS_CLIGetParameter
Example 3
                				
* @return		Returns pdFALSE if the CLI command finished.
* @note         Please see https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_CLI/FreeRTOS_Plus_CLI_Accessing_Command_Line_Parameters.html
				for more information on how to use the FreeRTOS CLI.

*****************************************************************************/
char stringCheckbuffer[64]; //command message holder
char checkerprint[64]; //error message holder
//init variable to hold our parameters
int ledID;
int Rval;
int Gval;
int Bval;
//int array to hold the ints from the command
int arr[10];
BaseType_t CLI_NeotrellisSetLed( int8_t *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString )
{
	//snprintf(pcWriteBuffer,xWriteBufferLen, "Students to fill out!");
	//SerialConsoleWriteString(pcWriteBuffer);
	//add string to check buffer to get commands
	snprintf(stringCheckbuffer, xWriteBufferLen, "%s\r\n", pcCommandString);
	SerialConsoleWriteString(stringCheckbuffer); //to display it

	char *p = stringCheckbuffer;
	int i=0;
	//loop through pcCommandString in stringCheckbuffer to check for negatives first
	for (int j=0; j<30; j++) {
		//snprintf(checkerprint,64, "current char is %s\r\n", stringCheckbuffer[j]);
		//SerialConsoleWriteString(checkerprint);
		if (stringCheckbuffer[j] == '-') {
			SerialConsoleWriteString("Cannot have negative values!\r\n");
			return pdFALSE;
		}
	}
	//loop through char p to get all ints
	while (*p) {
		if (isdigit(*p)) {
			//extract int from string
			int val = strtol(p, &p, 10);
			arr[i] = val;
			i++;
			//snprintf(checkerprint,64, "current p pointer #: %i\r\n", p);
			//SerialConsoleWriteString(checkerprint);
			} 
		else {
			p++;
			//snprintf(checkerprint,64, "current p pointer #: %i\r\n", p);
			//SerialConsoleWriteString(checkerprint);
		}
	}
	//save extracted ints into respective vars
	ledID = arr[0];
	Rval = arr[1];
	Gval = arr[2];
	Bval = arr[3];
	//snprintf(checkerprint,64, "the saved param is: %i %i %i %i\r\n", ledID, Rval, Gval, Bval);
	//SerialConsoleWriteString(checkerprint);
	//do checker to see if any of the ints are out of bounds
	if (ledID > 15) {
		snprintf(pcWriteBuffer,xWriteBufferLen, "LED ID (int) out of bounds! Need to be 0 to 15. \r\n");
		SerialConsoleWriteString(pcWriteBuffer);
		//return pdFALSE;
	}
	else if (Rval > 255) {
		snprintf(pcWriteBuffer,xWriteBufferLen, "R value (int) out of bounds! Need to be 0 to 255. \r\n");
		SerialConsoleWriteString(pcWriteBuffer);
		//return pdFALSE;
	}
	else if (Gval > 255) {
		snprintf(pcWriteBuffer,xWriteBufferLen, "G value (int) out of bounds! Need to be 0 to 255. \r\n");
		SerialConsoleWriteString(pcWriteBuffer);
		//return pdFALSE;
	}
	else if (Bval > 255) {
		snprintf(pcWriteBuffer,xWriteBufferLen, "B value (int) out of bounds! Need to be 0 to 255. \r\n");
		SerialConsoleWriteString(pcWriteBuffer);
		//return pdFALSE;
	}
	//if not out of bounds, set the LED accordingly
	else {
		SeesawSetLed(ledID, Rval, Gval, Bval);
		SeesawOrderLedUpdate();
		//return pdFALSE;
	}


	return pdFALSE;
}
