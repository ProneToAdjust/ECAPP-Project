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
void displaysevenseg(int);

char buff[10];
int result;
int delayCount = 0;
int keyPressed = 0;
int count = 0;
int tmr3Count = 0;
int tmr3toggle = 0;
int bulb_state = 0; // bulb state 0 = dim, 1 = bright
int spkrCount = 0;
int pwCorrect = 0;
int stopSpeaker = 0;
unsigned char i, LCD_TEMP;
char clear[] = "                ";
char sevenseglookup[10] = {0b1000000, 0b1111001, 0b0100100, 0b0110000, 0b0011001, 0b0010010, 0b0000010, 0b1111000, 0b0000000, 0b0010000};

char keypadarray[16] = {'1', '2', '3', 'F', '4', '5', '6', 'E', '7', '8', '9', 'D', 'A', '0', 'B', 'C'};

char chararray[16] = {'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'};
char cacount = 0; // Init char array count

char password[16] = {'0', '3', '1', '0', '2', '0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'}; // Password array

void main(void) {
    initialisepb();//delete this, commit sudoku
    
    while(1){
        
        if(delayCount > 15) { //if 15 sec passed, clear lcd reset password and stop count
            
            TMR3ON = 0;
            keyPressed = 0;
            delayCount = 0;
            
            for (i = 0; i < 16; i++)
                chararray[i] = '\0'; // Clear char array
            
            cacount = 0; // Reset char array counter
            clearlineone(); // Clear line one
            PORTEbits.RE0 = 1;
            PORTD = 0b10000000;
        }
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
    
//    TMR1IP = 0;
//    T1CON = 0b11001100;
//    TMR1H = 0xFE;
//    TMR1L = 0x0C;
//    TMR1IF = 0;
//    TMR1IE = 1;
    
    TMR3IP = 0;
    T3CON = 0b10000000;
    TMR3H = 0xEC;
    TMR3L = 0x78;
    TMR3IF = 0;
    TMR3IE = 1;
    
    T2CON = 0b00000111; // Timer 2 On, postscaler = 1:1, prescaler = 1:16
    PR2 = 62; // Set PR2 = 62 for 1ms
    TMR2IF = 0;
    TMR2IE = 1;
    TMR2IP = 0;
    
    CCPR2L = 0b00000110; // CCPR2L:CCP2CON<5:4> = 25
    CCP2CON = 0b00011111; // DC2B1 = 0, DC2B0 = 1, PWM mode 
    
    GIE=1;
    GIEL=1;
    
    PORTCbits.RC1 = 0;
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
        
        TMR3ON = 0;
        keyPressed = 0;
        PORTEbits.RE0 = 1;
        PORTD = 0b10000000;
        
        if(kpinput == 'E'){ // If input is 'E' 
            
            int boolean = passwordauth(); // Authenticate password
            TMR3ON = 1;
            if(boolean){ // Check if password is correct
                pwCorrect = 1;
                TMR3H = 0xFE;
                TMR3L = 0x0C;
                displaymsg("pw correct      ");   //16 chars to clear
            }
            
            else{ // Password is wrong
                pwCorrect = 2;
                TMR3H = 0xFE;
                TMR3L = 0x0C;
                displaymsg("pw wrong        ");  //16 chars to clear
            }
        }
        else{
            displaymsg("cleared         "); //16 chars to clear
        }
        for (i = 0; i<16 ; i++)
            chararray[i]= '\0'; // Clear char array
        
        cacount = 0; // Reset char array counter
        __delay_ms(1000);
        clearlineone();  // Clear line one
    }
    else if (cacount < 16) {
        
        if(keyPressed == 0){ //start timer on 1st key press
        
            TMR3ON = 1;
            TMR3H = 0xEC;
            TMR3L = 0x78;
            delayCount = 0;
            tmr3Count = 0;
            count = 15; // set timer to 15 sec count
            keyPressed = 1;
        }
        
        chararray[cacount] = kpinput; // Add input to char array
        displaymsg(chararray); // Update LCD display with input
        cacount++; // Increment char array counter
    }
    
    INT0IF = 0; // Clear interrupt flag
}

else if (INT1IF){ // Check INT1 flag
    
//    TMR0H=0x67; // Reload timer0
//    TMR0L=0x69;
    
    //CCPR2L = 0b00011111; // CCPR2L:CCP2CON<5:4> = 125
    //CCP2CON = 0b00011111; // DC2B1 = 0, DC2B0 = 1, PWM mode
    
    if(bulb_state == 1){
            
            TMR0H=0x67; // Reload timer0
            TMR0L=0x69;
        }
        
    else if(bulb_state == 0){
            
            TMR0H=0x67; // Reload timer0
            TMR0L=0x69;
            
            for(int x = 0;x < 24;x++){//brighten bulb
                
               CCPR2L++;
               _delay(10000);
            }
            
            bulb_state = 1;
            
        }
    
    INT1IF = 0; // Clear interrupt flag
}

else if (TMR0IF){ // Check TMR0IF flag
     
    if(SW1 == 0){ // Check if SW1 is up
        
        //TMR0H=0x67; // Reload timer0
        //TMR0L=0x69;
        
        if(bulb_state == 1){
            
            TMR0H=0x67; // Reload timer0
            TMR0L=0x69;
            }
        
        else if(bulb_state == 0){
            
            TMR0H=0x67; // Reload timer0
            TMR0L=0x69;
            
//            for(int x = 0;x < 24;x++){//dim bulb
//                
//               CCPR2L--;
//               _delay(1000);
//               }
//            
//            bulb_state = 0;
            
            }
        
        }
    
    else{         // SW1 is down
        
        //CCPR2L = 0b00000110; // CCPR2L:CCP2CON<5:4> = 25
        //CCP2CON = 0b00011111; // DC2B1 = 0, DC2B0 = 1, PWM mode 
        
        if(bulb_state == 1){
            
            TMR0H=0x67; // Reload timer0
            TMR0L=0x69;
            
            for(int x = 0;x < 24;x++){//dim bulb
                
               CCPR2L--;
               _delay(10000);
               }
            
            bulb_state = 0;
        }
        
        else if(bulb_state == 0){
            
            TMR0H=0x67; // Reload timer0
            TMR0L=0x69;
        }
        
        }
     
     TMR0IF = 0; // Clear interrupt flag
}

else if (TMR3IF) {
    if(pwCorrect) {
        if(pwCorrect == 1) {
            SPEAKER = ~SPEAKER;
        } else {
            spkrCount++;
            if(spkrCount == 4) {
                spkrCount = 0;
                SPEAKER = ~SPEAKER;
            }
        }
        stopSpeaker++;
        if(stopSpeaker == 1000) {
            TMR3ON = 0;
            stopSpeaker = 0;
            pwCorrect = 0;
        }
        TMR3H = 0xFE;
        TMR3L = 0x0C;
    } else {
        tmr3toggle = !tmr3toggle; //toggle between displaying num on 7seg

        if(keyPressed) {
            displaysevenseg(tmr3toggle); //display num on 7seg
        }
        tmr3Count++;

        if(tmr3Count == 200) { //reduce count by 1 after 200 tmr3 overflow interrupts
            delayCount++;
            tmr3Count = 0;
            count = 15 - delayCount;
        }

        TMR3H = 0xEC;
        TMR3L = 0x78;
    }
    TMR3IF = 0;
}

}

void W_ctr_4bit(char x){
/* Write control word in term of 4-bit at a time to LCD */
LCD_RS = 0; // Logic ?0?
LCD_TEMP = x; // Store control word
//LCD_TEMP >> 4 // send upper nibble of control word
LCD_E = 1; // Logic ?1?
LCD_DATA = LCD_TEMP  & 0b11110000;
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

void displaysevenseg(int toggle) {
    if(toggle == 1) { //display num on 1st 7seg
        if((count/10) > 0) {
            PORTEbits.RE0 = 1;
            PORTD = sevenseglookup[count/10] & 0b11111111;
        }
    } else { //display num on 2nd 7seg
        PORTEbits.RE0 = 0;
        PORTD = sevenseglookup[count%10] | 0b10000000;
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
