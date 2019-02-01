/*
 * File:   main.c
 * Author: Ryan
 *
 * Created on October 29, 2018, 2:52 PM
 */


#include <xc.h>

#pragma config OSC = XT // Oscillator Selection bits (XT oscillator)
#pragma config LVP = OFF // Single-Supply ICSP Enable bit (Single-Supply ICSP disabled)
#pragma config WDT = OFF // Watchdog Timer Enable bit (WDT disabled (control is placed on the SWDTEN bit))

#define _XTAL_FREQ 4000000

#define LCD_DATA PORTC
#define LCD_RS RE2
#define LCD_E RE1

#define SEVEN_SEG_DATA PORTD
#define SEVEN_SEG_SL1 RE0
#define SEVEN_SEG_SL2 RD7

#define greenLED RB3
#define yellowLED RB4
#define redLED RB5

#define KEYPAD ((PORTA & 0b00011110) >> 1)
#define SW1 RB1
#define SleepButton RB2

#define SPEAKER RC2
#define LIGHTBULB RC1

void Init_LCD();
void initializeProjectBoard();

void interrupt ISR_h();
void interrupt low_priority ISR();
int authorizeUser();
void clearLCD(int);
void sevenSegmentDisplay(int, int);
void W_ctr_4bit(char);
void W_data_4bit(char);
void displayOnLCD(char[], int);

char key;
char loop = 0;
char bulb_state = 0;
char temperature = 0;
char characterCount = 0;
char passwordCorrect = 2; //0 is not set, 1 is correct, 2 is wrong
char onKeyEntered = 0;
char keypadTimerCount = 15; //15 seconds timer for keypad
char speakerCount = 0;
char sevenSegmentSelect = 0;
char sevenSegmentCount = 0;
int speakerSoundLength = 0;
int bulbOnLength = 0;
unsigned char LCD_TEMP;

char sevenSegmentLookupTable[] = {0b01000000,
    0b01111001,
    0b00100100,
    0b00110000,
    0b00011001,
    0b00010010,
    0b00000010,
    0b01111000,
    0b00000000,
    0b00010000};
char keypadLookupTable[] = {'1', '2', '3', 'F', '4', '5', '6', 'E', '7', '8', '9', 'D', 'A', '0', 'B', 'C'};

char userInput[] = {'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'};
char password[] = {'0', '3', '1', '0', '2', '0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'};

char clearLine[] = "                ";
char correctPw[] = "Password Correct";
char wrongPw[] = "Password Wrong  ";
char clearPw[] = "Password Cleared";

void main(void) {
    initializeProjectBoard();
    while (1) {
        if (!SleepButton) {
            LIGHTBULB = 0;
            redLED = 0;
            yellowLED = 0;
            greenLED = 0;
            SEVEN_SEG_SL1 = 1;
            SEVEN_SEG_SL2 = 1;
            SLEEP();
        }
    }
}

void interrupt ISR_high() {
    if (INT0IF) {
        key = keypadLookupTable[KEYPAD];
        if (key == 'E' || key == 'C') {
            TMR3ON = 0;
            SEVEN_SEG_SL1 = 1;
            SEVEN_SEG_SL2 = 1;
            onKeyEntered = 0;
            if (key == 'E') {
                passwordCorrect = authorizeUser();
                if (passwordCorrect == 2) {
                    displayOnLCD(wrongPw, 1);
                } else if (passwordCorrect == 1) {
                    displayOnLCD(correctPw, 1);
                }
                TMR3H = 0xFE;
                TMR3L = 0x0C;
                TMR3ON = 1;
            } else {
                passwordCorrect = 2;
                displayOnLCD(clearPw, 1);
                __delay_ms(1000);
                clearLCD(1);
            }
            for (loop = 0; loop < 16; loop++)
                userInput[loop] = '\0';
            characterCount = 0;
        } else if (characterCount < 16) {
            if (!onKeyEntered) {
                W_ctr_4bit(0b10000000);
                TMR3ON = 1;
                TMR3H = 0xEC;
                TMR3L = 0x78;
                onKeyEntered = 1;
                keypadTimerCount = 15;
                passwordCorrect = 0;
            }
            userInput[characterCount] = key;
            W_data_4bit(key);
            characterCount++;
        }
        INT0IF = 0;
    } else if (INT1IF) {
        if (bulb_state == 1) {

            TMR0H = 0x67; //Reload Timer0
            TMR0L = 0x69;

        } else if (bulb_state == 0) {

            TMR0H = 0x67; //Reload Timer0
            TMR0L = 0x69;

            for (int x = 0; x < 24; x++) { //brighten bulb
                CCPR2L++;
                _delay(10000);
            }

            bulb_state = 1;
        }
        INT1IF = 0; // Clear interrupt flag
    } else if (TMR0IF) {
        if (SW1 == 0) { // Check if SW1 is up
            if (bulb_state == 1) {

                TMR0H = 0x67; // Reload timer0
                TMR0L = 0x69;

            } else if (bulb_state == 0) {

                TMR0H = 0x67; // Reload timer0
                TMR0L = 0x69;

            }
        } else { // SW1 is down
            if (bulb_state == 1) {

                TMR0H = 0x67; // Reload timer0
                TMR0L = 0x69;

                for (int x = 0; x < 24; x++) { //dim bulb
                    CCPR2L--;
                    _delay(10000);
                }

                bulb_state = 0;
            } else if (bulb_state == 0) {

                TMR0H = 0x67; // Reload timer0
                TMR0L = 0x69;

            }
        }
        TMR0IF = 0;
    } else if (TMR3IF) {
        if (!passwordCorrect && onKeyEntered) {
            sevenSegmentSelect = ~sevenSegmentSelect;
            sevenSegmentDisplay(keypadTimerCount, sevenSegmentSelect);
            sevenSegmentCount++;
            if (!keypadTimerCount) {
                onKeyEntered = 0;
                passwordCorrect = 2;
                for (loop = 0; loop < characterCount; loop++)
                    userInput[loop] = '\0';
                characterCount = 0;
                clearLCD(1);
                SEVEN_SEG_SL1 = 1;
                SEVEN_SEG_SL2 = 1;
                TMR3ON = 0;
            }
            if (sevenSegmentCount == 200) {
                keypadTimerCount--;
                sevenSegmentCount = 0;
            }
            TMR3H = 0xEC;
            TMR3L = 0x78;
        } else {
            speakerSoundLength++;
            if (passwordCorrect == 1) {
                SPEAKER = ~SPEAKER;
            } else {
                speakerCount++;
                if (speakerCount == 4) {
                    speakerCount = 0;
                    SPEAKER = ~SPEAKER;
                }
            }
            if (speakerSoundLength == 1000) {
                TMR3ON = 0;
                speakerSoundLength = 0;
                speakerCount = 0;
                clearLCD(1);
            }
            TMR3H = 0xFE;
            TMR3L = 0x0C;
        }
        TMR3IF = 0;
    }
}

void interrupt low_priority ISR() {
    if (TMR2IF) {
        temperature = 0;
        ADCON0bits.GO = 1;
        while (ADCON0bits.DONE);
        temperature = ADRESH >> 4;

        if (temperature < 2 || temperature > 14) {
            SPEAKER = ~SPEAKER;
            redLED = 1;
            yellowLED = 0;
            greenLED = 0;
        } else {
            if (temperature > 10) {
                redLED = 1;
                yellowLED = 0;
                greenLED = 0;
            } else if (temperature > 5) {
                greenLED = 1;
                yellowLED = 0;
                redLED = 0;
            } else if (temperature > 3) {
                yellowLED = 1;
                redLED = 0;
                greenLED = 0;
            }
        }
        TMR2IF = 0;
    }
}

void Init_LCD() {
    _delay(15);
    W_ctr_4bit(0x03);
    _delay(5);
    W_ctr_4bit(0x02);
    W_ctr_4bit(0b00101000);
    W_ctr_4bit(0b00001100);
    W_ctr_4bit(0b00000110);
    W_ctr_4bit(0b00000001);
}

void initializeProjectBoard() {
    OSCCON = 0;
    ADCON0 = 0b00000001;
    ADCON1 = 0X0E;
    ADCON2 = 0b00010001;
    TRISA = 0b11011111;
    TRISB = 0b11000111;
    TRISC = 0b00001001;
    TRISD = 0;
    TRISE = 0b1000;

    Init_LCD();

    GIE = 0; //Disable all interrupts
    IPEN = 1; //Disable interrupt priority

    //Initialize Interrupt 0
    INT0IF = 0;
    INT0IE = 1;
    INTEDG0 = 0;

    //Initialize Interrupt 1
    INT1IP = 1;
    INT1IF = 0;
    INT1IE = 1;
    INTEDG1 = 0;

    //Initialize Timer 0 Overflow Interrupt
    TMR0IP = 1;
    T0CON = 0b10000110;
    TMR0H = 0x67;
    TMR0L = 0x69;
    TMR0IF = 0;
    TMR0IE = 1;

    //Initialize Timer 2 For PWM
    T2CON = 0b00000111; // Timer 2 On, postscaler = 1:1, prescaler = 1:16
    PR2 = 999; // Set PR2 = 62 for 1ms
    TMR2IF = 0;
    TMR2IE = 1;
    TMR2IP = 0;

    //Timer 2 PWM settings
    CCPR2L = 0b00001100; // CCPR2L:CCP2CON<5:4> = 50
    CCP2CON = 0b00101100; // DC2B1 = 1, DC2B0 = 0, PWM mode 

    //Initialize Timer 3 Overflow Interrupt
    TMR3IP = 1;
    T3CON = 0b10000000;
    TMR3H = 0xEC;
    TMR3L = 0x78;
    TMR3IF = 0;
    TMR3IE = 1;

    //Enable all interrupts
    GIE = 1;
    GIEL = 1;

    //Turn off light bulb
    PORTCbits.RC1 = 0;
}

void W_ctr_4bit(char x) {
    LCD_RS = 0;
    LCD_TEMP = x;
    LCD_E = 1;
    LCD_DATA = LCD_TEMP & 0b11110000;
    _delay(1000);
    LCD_E = 0;
    _delay(1000);
    LCD_TEMP = x;
    LCD_TEMP <<= 4;
    LCD_E = 1;
    LCD_DATA = LCD_TEMP & 0b11110000;
    _delay(1000);
    LCD_E = 0;
    _delay(1000);
}

void W_data_4bit(char x) {
    LCD_RS = 1;
    LCD_TEMP = x;
    LCD_E = 1;
    LCD_DATA = LCD_TEMP & 0b11110000;
    _delay(1000);
    LCD_E = 0;
    _delay(1000);
    LCD_TEMP = x;
    LCD_TEMP <<= 4;
    LCD_E = 1;
    LCD_DATA = LCD_TEMP & 0b11110000;
    _delay(1000);
    LCD_E = 0;
    _delay(1000);
}

void clearLCD(int lineNumber) {
    if (!lineNumber) {
        W_ctr_4bit(0b10000000);
        for (loop = 0; clearLine[loop] != 0; loop++)
            W_data_4bit(clearLine[loop]);
        W_ctr_4bit(0b11000000);
        for (loop = 0; clearLine[loop] != 0; loop++)
            W_data_4bit(clearLine[loop]);
    } else {
        lineNumber--;
        W_ctr_4bit(0b10000000 | lineNumber << 6);
        for (loop = 0; clearLine[loop] != 0; loop++)
            W_data_4bit(clearLine[loop]);
    }
}

void sevenSegmentDisplay(int displayNumber, int selectPin) {
    if (!selectPin) {
        if ((displayNumber / 10) > 0) {
            SEVEN_SEG_SL1 = 1;
            SEVEN_SEG_DATA = sevenSegmentLookupTable[displayNumber / 10] & 0b01111111;
        }
    } else {
        SEVEN_SEG_SL1 = 0;
        SEVEN_SEG_DATA = sevenSegmentLookupTable[displayNumber % 10] | 0b10000000;
    }
}

int authorizeUser() {
    for (loop = 0; loop < 16; loop++) {
        if (userInput[loop] != password[loop]) {
            return 2;
            break;
        } else if (loop == 15)
            return 1;
    }
}

void displayOnLCD(char message[], int lineNumber) {
    lineNumber--;
    W_ctr_4bit(0b10000000 | lineNumber << 6);
    for (loop = 0; message[loop] != 0; loop++)
        W_data_4bit(message[loop]);
}