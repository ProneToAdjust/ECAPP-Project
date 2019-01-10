/*
 * File:   main.c
 * Author: Ryan
 *
 * Created on October 29, 2018, 2:52 PM
 */


#pragma config OSC = XT // Oscillator Selection bits (XT oscillator)
#pragma config LVP = OFF // Single-Supply ICSP Enable bit (Single-Supply ICSP disabled)
#pragma config WDT = OFF // Watchdog Timer Enable bit (WDT disabled (control is placed on the SWDTEN bit))
//#pragma config MCLRE = OFF

#include <xc.h>
#include <stdlib.h>
#include <p18f4520.h>

#define _XTAL_FREQ 4000000
#define LCD_DATA PORTC
#define LCD_RS RE2 // RS signal for LCD
#define LCD_E RE1 // E signal for LCD
#define SEV_SEG PORTD
#define KEYPAD ((PORTA & 0b00011110) >> 1)
#define SPEAKER PORTCbits.RC2
#define MOTOR PORTCbits.RC1
#define SW1 PORTBbits.RB1

void initialisepb(void);
void Init_LCD(void); // Initialize the LCD
void interrupt ISR(void);
void W_ctr_4bit(char); // 4-bit Control word for LCD
void W_data_4bit(char); // 4-bit Text Data for LCD
void displaymsg(char MESS[]);
void displaychar(char mess);
void scrollmsg(char MESS[]);
void clearline(int lineno);
void clearlineone(void);
void clearlinetwo(void);
void display7seg(char s7);
void displaytwo7seg1cyc(char tdnum);
int passwordauth(void);

char buff[10];
int result;
unsigned char i, LCD_TEMP;
char clear[] = "                ";
char sevenseglookup[10] = {0b1000000, 0b1111001, 0b0100100, 0b0110000, 0b0011001, 0b0010010, 0b0000010, 0b1111000, 0b0000000, 0b0010000};

char keypadarray[16] = {'1', '2', '3', 'F', '4', '5', '6', 'E', '7', '8', '9', 'D', 'A', '0', 'B', 'C'};

char chararray[16] = {'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'};
char cacount = 0; // Init char array count

char password[16] = {'0', '3', '1', '0', '2', '0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'}; // Password array

void main(void) {
    initialisepb();//delete this, commit sudoku
    T2CON = 0b00000101; // Timer 2 On, postscaler = 1:1, prescaler = 1:4
    PR2 = 249; // Set PR2 = 249 for 1ms
    CCPR2L = 11111101; // CCPR1L:CCP1CON<5:4> = 500
    CCP2CON = 0b00001111; // DC1B1 & DC1B0 = 0, PWM mode
    while(1){
    }
}

void initialisepb(void){
    ADCON1 =0X0F;
    TRISA = 0b11011111;
    TRISB =0b11111111;
    TRISC =0b00001001;
    TRISD = 0;
    TRISE = 0b1000;
    
    Init_LCD();
    
    GIE=0;
    IPEN=0;
    
    INT0IF=0;
    INT0IE=1;
    INTEDG0=0;
    
    INT1IF=0;
    INT1IE=1;
    INTEDG1=0;
    
    TMR0IP=0;
    T0CON=0b10000110;
    TMR0H=0x67;
    TMR0L=0x69;
    TMR0IF=0;
    TMR0IE=1;
    
    GIE=1;
    GIEL=1;
}

void Init_LCD(){ /* LCD display initialization */
// Special Sequence a) to d) required for 4-bit interface
_delay(15); // a) 15ms LCD power-up delay
W_ctr_4bit(0x03); // b) Function Set (DB4-DB7: 8-bit interface)
_delay(5); // c) 5ms delay
W_ctr_4bit(0x02); // d) Function Set (DB4-DB7: 4-bit interface)
W_ctr_4bit(0b00101000); // Function Set - 4-bit, 2 lines, 5X7
W_ctr_4bit(0b00001100); // Display on, cursor off
W_ctr_4bit(0b00000110); // Entry mode - inc addr, no shift
W_ctr_4bit(0b00000001); // Clear display & home position
}

void interrupt ISR(void){
if (INT0IF) { // Check INT0 flag
    
    char kpinput = keypadarray[KEYPAD]; // Get keypad input
    W_ctr_4bit(0b00000010); 
    
    if(kpinput == 'E' || kpinput == 'C'){ // 
        
        if(kpinput == 'E'){ // If input is 'E' 
            int boolean = passwordauth(); // Authenticate password
            if(boolean){ // Check if password is correct
                displaymsg("pw correct");   
            }
            else{ // Password is wrong
                displaymsg("pw wrong");
            }
        }
        else
            displaymsg("cleared");
        
        for (i = 0; i<16 ; i++)
            chararray[i]= '\0'; // Clear char array
        cacount = 0; // Reset char array counter
        __delay_ms(1000);
        clearlineone();  // Clear line one
    }
    else{
        chararray[cacount] = kpinput; // Add input to char array
        displaymsg(chararray); // Update LCD display with input
        cacount++; // Increment char array counter
    }
    INT0IF = 0; // Clear interrupt flag
}
else if (INT1IF){ // Check INT1 flag
    TMR0H=0x67; // Reload timer0
    TMR0L=0x69;
    CCP2CON = 0b1111; // Enable pwm 
    INT1IF = 0; // Clear interrupt flag
}
else if (TMR0IF){ // Check TMR0IF flag
     
    if(SW1 == 0){ // Check if SW1 is up
        TMR0H=0x67; // Reload timer0
        TMR0L=0x69;
        CCP2CON = 0b1111; // Enable pwm
        }
    else{ // SW1 is down
        CCP2CON = 0b0000; // Disable pwm
        }
     
     TMR0IF = 0; // Clear interrupt flag
}
}

void W_ctr_4bit(char x){
/* Write control word in term of 4-bit at a time to LCD */
LCD_RS = 0; // Logic ?0?
LCD_TEMP = x; // Store control word
//LCD_TEMP >> 4 // send upper nibble of control word
LCD_E = 1; // Logic ?1?
LCD_DATA = LCD_TEMP;
_delay(1000); // 1ms delay
LCD_E = 0; // Logic ?0?
_delay(1000); // 1ms delay
LCD_TEMP= x; // Store control word
LCD_TEMP<<= 4; // Send lower nibble of control word
LCD_E = 1; // Logic ?1?
LCD_DATA = LCD_TEMP & 0b11110000;
_delay(1000); // 1ms delay
LCD_E = 0; // Logic 0?
_delay(1000); // 1ms delay
}

void W_data_4bit(char x){
/* Write text data in term of 4-bit at a time to LCD */
LCD_RS = 1; // Logic ?1?
LCD_TEMP = x; // Store text data
//LCD_TEMP >>= 4; // Send upper nibble of text data
LCD_E = 1; // Logic ?1?
LCD_DATA = LCD_TEMP & 0b11110000;
_delay(1000); // 1ms delay
LCD_E = 0; // Logic ?0?
_delay(1000); // 1ms delay
LCD_TEMP = x; // Store text data
LCD_TEMP <<= 4; // Send lower nibble of text data
LCD_E = 1; // Logic ?1?
LCD_DATA = LCD_TEMP & 0b11110000;
_delay(1000); // 1ms delay
LCD_E = 0; // Logic ?0?
_delay(1000); // 1ms delay
}

void delayms(unsigned int ms) {
    for (unsigned int msloop = 0; msloop < ms; msloop++) {
        _delay(1000);
    }
}

void delaysec(int sec) {
    for (int secloop = 0; secloop < sec; secloop++) {
        for (int d = 0; d < 1000; d++)_delay(1000);
    }
}

void displaymsg(char MESS[]) {
    
    W_ctr_4bit(0b00000110);// Entry mode - inc addr, no shift
    for (i = 0; MESS[i] ; i++) // Output message to LCD
    W_data_4bit(MESS[i]); // Write individual character to LCD
    W_data_4bit(' ');
}

void scrollmsg(char MESS[])
{   
    W_ctr_4bit(0b00000001);//clear screen   
    W_ctr_4bit(0b00000010);//return cursor to home
    W_ctr_4bit(0b00000111);//entry mode set
        for (unsigned char msgcount = 0; MESS[msgcount] != 0; msgcount++) {
        W_data_4bit(MESS[msgcount]);
    }
    for(int i=0;MESS[i];i++){
        W_ctr_4bit(0b00011000);//scroll left
        delayms(150);
    }
}

void displaychar(char mess) {
    W_ctr_4bit(0b00000110);//entry mode set
    W_data_4bit(mess);
}

void clearline(int lineno){//clear line function with selectable lines(might be slower than bottom two functions)
    if(lineno == 1){
        W_ctr_4bit(0b00000010);//return cursor to home
    }
    else if(lineno == 2){
        W_ctr_4bit(0xc0); // Set cursor to line 2
    }
    for (i=0; clear[i]; i++)
        W_data_4bit(clear[i]);
}

void clearlineone(void){//function to clear line 1
    W_ctr_4bit(0b00000010);//return cursor to home
    for (i=0; clear[i]; i++)
        W_data_4bit(clear[i]);//clears line using clear char array
}

void clearlinetwo(void){//function to clear line 2
    W_ctr_4bit(0xc0);//set cursor to line 2
    for (i=0; clear[i]; i++)
        W_data_4bit(clear[i]);//clears line using clear char array
}

void display7seg(char s7) {
    SEV_SEG = sevenseglookup[s7];
}

void displaytwo7seg1cyc(char tdnum){
    PORTDbits.RD7 = 0;
    PORTEbits.RE0 = 1;
    display7seg(tdnum%10);
    PORTDbits.RD7 = 1;
    PORTEbits.RE0 = 0;
    display7seg((tdnum-tdnum%10)/10);
}

int passwordauth(){
    
    int boolean = 3;
    
    for (int i = 0; i < 16; i++) {
        if ((chararray[i] == password[i]) && (chararray[i] != '\0')) {
            continue;
        } else if ((chararray[i] == password[i]) && (chararray[i] == '\0')) {
            boolean = 1;
            break; //correct password break
        } else {
            boolean = 0;
            break; //wrong password break
        }
    }
    
    return boolean;
}