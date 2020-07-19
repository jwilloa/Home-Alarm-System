/******************************************************************************
Include header files.
******************************************************************************/
#include "mbed.h"
#include "TextLCD.h"

// TextLCD 
TextLCD lcd(p15, p16, p17, p18, p19, p20);//, TextLCD::LCD20x4); // rs, e, d4-d7

// Read from switches and keypad
BusOut rows(p26,p25,p24);
BusIn cols(p14,p13,p12,p11);

// Control external LEDs
SPI sw(p5, p6, p7);
DigitalOut cs(p8); 

// Serial USB
Serial pc(USBTX, USBRX); // tx, rx

DigitalOut Sounder(LED1);
DigitalOut led4(LED4);

/******************************************************************************
Declaration of functions.
******************************************************************************/
void alarmConfig(void);
void alarm_state(); 

int  read_switch(void);
int  switchleds(void);
int  readleds();
int  readled(int ledno);
void initleds();
void setleds(int ledall);
void setled(int ledno, int ledstate);

char getKey();
void setKey(void); 
int  testCode(void);

void sounder_blink();
void pattern(void);

void patternoff(void);
void Sounderoff(void);

void system_alarm(void);
void system_set(void);

void idle_count();
void switching();
/******************************************************************************
Global variables.
******************************************************************************/
enum State {unset, exits, set, entry, alarm, report} state;          
char Keytable[] = { 'F', 'E', 'D', 'C',   
                    '3', '6', '9', 'B',   
                    '2', '5', '8', '0',   
                    '1', '4', '7', 'A'    
                   };
char keyCode[] = {'1','2','3','4'};
char code[4];
int  fail = 0; // number of failed attempts
int  success;  // 0~4: number of successful matched keycodes; 5 indicates switch changing; 6 indicates time expired
int  zone;     
int  led_bits = 0;   // global LED status used for readback
int  switches;

bool time_expired ;
bool idle_flag;
volatile bool switch_flag;
int state_num;
Ticker timer;
Ticker counter;
Ticker switcher;
Timeout t_alarm, t_entry, t_exit;            

/*=============================================================================

FILE - main.cpp

DESCRIPTION
    Initiates the Finite State Machine
=============================================================================*/

int main() { 
    alarmConfig(); 

    lcd.cls();
    lcd.locate(2,0);
    lcd.printf("Alarm Lab");
    pc.printf("\n\n\r              ALARM LAB\n\r (c)Copyright University of Essex 2020 \n\r            Author: Javan Willock\n\r");
    
    setKey();  
    
    while (1){
        switchleds();
        
        alarm_state();  // FSM routine
    }
}

/**********************************************************************************
Finite State Machine for Alarm
**********************************************************************************/
void alarm_state(){
    
    switch(state){
 /*--------------------------------------------------------------------------------
    UN_SET STATE (STATE 1)
---------------------------------------------------------------------------------*/ 
    case unset:
        counter.detach();
        switcher.detach();
        switch_flag = false; 
        idle_flag = false;       
        state_num = 1; 
        patternoff();
        
        setled(8,3);  // turn unset led on
        
        pc.printf("\n\r Unset mode\n\r");
        lcd.cls();
        lcd.printf("Unset mode");

        success = testCode();
        
        if (success == 4) {
            fail = 0;  // failure counter reset
            state = exits;    
        }
        else if (fail == 2) {
            pc.printf("\n\r Attempt : %d", fail + 1);   
            state = alarm;
            zone = 5;    // zone identifier(5 represents an invalid code is entered three times)
            fail = 0;      //failure counter reset   
        }
        else{
            fail++;
            state = unset;
            pc.printf("\n\r Attempt : %d", fail);    
        }
        break;
/*--------------------------------------------------------------------------------
    EXIT STATE (STATE 2)
---------------------------------------------------------------------------------*/  
    case exits: 
        counter.detach();
        switcher.detach();
        switch_flag = false; 
        idle_flag = false;
        state_num = 2;
         
        setled(7,3);   // turn exits led on   
        
        pc.printf("\n\r Exit mode\n\r");
        lcd.cls();
        lcd.printf("Exit mode"); 
       
        pattern();     // Sounder switches on and then off at 250ms intervals                         
        
        // you should program here!  
        counter.attach(&idle_count, 4);
        switcher.attach(&switching, 0.1);

        success = testCode();

        if (success == 4) {
            fail = 0;  // failure counter reset
            counter.detach();
            state = unset;    
        }
        else if (fail == 2) {
            pc.printf("\n\r Attempt %d", fail+1); 
            state = alarm;
            zone = 5;      // zone identifier(5 represents an invalid code is entered three times)
            fail = 0;      //failure counter reset   
        }

        else if (idle_flag == true){
            counter.detach();
            switcher.detach();
            state = set;
        }  
        else if (switch_flag == true){
            counter.detach();
            zone = 2;
            state = alarm;
        } 
        
        else{
            fail++;
            state = exits;
            pc.printf("\n\r Attempt %d", fail);  
        }
        break;
/*--------------------------------------------------------------------------------
    SET STATE (STATE 3) 
---------------------------------------------------------------------------------*/ 
    case set: 
        counter.detach();
        switcher.detach();
        switch_flag = false; 
        idle_flag = false;
        
        state_num = 3;
        
        patternoff();
        setled(6,3);   // turn set led on
        
         // you should program here!     

        if (switches ==1){
            zone = 5;
            state = entry;
        }
        else if (switches == 2){
            zone = 2;
            state = alarm;
        }
         
        break;       
        
/*--------------------------------------------------------------------------------
    ENTRY STATE (STATE 4) 
---------------------------------------------------------------------------------*/ 
    case entry:
        setled(5,3);   // turn entry led on
        pattern();     // Sounder switches on and then off at 250ms intervals
        timer.detach();
        state_num = 4;
        pc.printf("\n\r Entry mode\n\r");
        lcd.cls();
        lcd.printf("Entry mode"); 
        
        pattern();     // Sounder switches on and then off at 250ms intervals   
        counter.attach(&idle_count, 5);
        switcher.attach(&switching, 0.1);
        
        // you should program here!  
        success = testCode();         
        if (success == 4) {
            state = unset;    
        }
        else if (idle_flag == true){
            counter.detach();
            idle_flag = false;
            state = alarm;
        }  
        else if (switch_flag == true){
            switcher.detach();
            switch_flag = false;
            state = alarm;
        }
        else{
            state = entry;  
        }           
        break;
/*--------------------------------------------------------------------------------
    ALARM STATE (STATE 5)
---------------------------------------------------------------------------------*/ 
    case alarm:
        counter.detach();
        switcher.detach();
        switch_flag = false; 
        //idle_flag = false;
        patternoff();
        state_num = 5;

        setled(4,3);   // turn entry led on
        if (idle_flag == false){
            Sounder = 1;   //Sounder enabled all the time during alarm state
        }
        
        pc.printf("\n\r Alarm mode\n\r");
        lcd.cls();
        lcd.printf("Alarm mode");         
        
        counter.attach(&idle_count, 3);

        // you should program here!  
        
        success = testCode();
        if (success == 4) {
            state = report;    
        }
        else if(idle_flag == true){
            pc.printf("SOUNDER OFF");
            Sounder = 0;
            counter.detach();
            state = alarm; 
        }
        else{
            state = alarm;  
        }
        break;     
/*--------------------------------------------------------------------------------
    REPORT (STATE 6)
---------------------------------------------------------------------------------*/         
    case report:
        setled(3,3);   // turn entry led on
        patternoff();
        state_num = 6;
        
        pc.printf("\n\r Report mode\n\r");
        pc.printf("\r Zone numbers: %i\n\r", zone);
        lcd.cls();
        lcd.printf("Zone numbers:%i",zone);    
        lcd.locate(0,1);
        lcd.printf("C key to clear");  
        
        while (1){
            char b = getKey();  
            if (b == 'C'){
                state = unset;
                break;
            }            
        }     
        break;
    } // end switch(state)

} // end alarm_state()


/**********************************************************************************
Configurations
**********************************************************************************/
void alarmConfig() 
{
    cs=0;
    sw.format(16,0); 
    sw.frequency(1000000);
    
    state = unset;   // Initial state
    
    led_bits  = 0x0000;  // turn off all the external leds
    sw.write(led_bits); 
    cs = 1;
    cs = 0;
}

/**********************************************************************************
External switches 
**********************************************************************************/
int read_switch()
{
    rows = 4;
    switches = cols;
    rows = 5;
    switches = switches*16 + cols;
    
    return switches;
}

int switchleds()
{
    int switches = read_switch();
        
    for(int i=0;i<=7;i++){
        if ((switches & 0x0001<<i)!=0){                         // 1, then turn on 
            led_bits  = led_bits | (0x0003 << i*2); }
        else {                                                  // 0, then turn off
            led_bits  = led_bits & ((0x0003 << i*2) ^ 0xffff); }
    }
    sw.write(led_bits); 
    cs = 1;
    cs = 0;

    return switches;
}

/**********************************************************************************
External LED functionality
**********************************************************************************/
void initleds() {
    cs = 0;                                        // latch must start low
    sw.format(16,0);                               // SPI 16 bit data, low state, high going clock
    sw.frequency(1000000);                         // 1MHz clock rate
}

void setleds(int ledall) {
    led_bits = ledall;                              // update global LED status
    sw.write((led_bits & 0x03ff) | ((led_bits & 0xa800) >> 1) | ((led_bits & 0x5400) << 1));
    cs = 1;                                        // latch pulse start 
    cs = 0;                                        // latch pulse end
}

void setled(int ledno, int ledstate) {
    ledno = 9 - ledno;
    ledno = ((ledno - 1) & 0x0007) + 1;             // limit led number
    ledno = (8 - ledno) * 2;                        // offset of led state in 'led_bits'
    ledstate = ledstate & 0x0003;                   // limit led state
    ledstate = ledstate << ledno;
    int statemask = ((0x0003 << ledno) ^ 0xffff);   // mask used to clear led state
    led_bits = ((led_bits & statemask) | ledstate); // clear and set led state
    setleds(led_bits);
}

int readled(int ledno) {
    ledno = 9 - ledno;
    ledno = ((ledno - 1) & 0x0007) + 1;             // limit led number
    ledno = (8 - ledno) * 2;                        // offset of led state in 'led_bits'
    int ledstate = led_bits;
    ledstate = ledstate >> ledno;                   // shift selected led state into ls 2 bits
    return (ledstate & 0x0003);                     // mask out and return led state
}

int readleds() {
    return led_bits;                                // return LED status
}

/**********************************************************************************
Keypad functionality
**********************************************************************************/
char getKey()
{
    int i,j;
    char ch=' ';
    
    for (i = 0; i <= 3; i++) {
        rows = i; 
        for (j = 0; j <= 3; j++) {           
            if (((cols ^ 0x00FF)  & (0x0001<<j)) != 0) {
                ch = Keytable[(i * 4) + j];
            }            
        }        
    }
    wait(0.2); //debouncing
    return ch;
}

int testCode(void)
{
    int a, y;
    char b;
    
    lcd.locate(0, 1);   
    lcd.printf("Enter Code: ____");
    y = 0; 
    for(a = 0; a < 4; a++) {
        
        if (idle_flag == true){break;}// when exit idle flag raised, it will break for loop
        else if (switch_flag == true){break;}// when in exit activation of sensor flag raised will break for loop
        
        b = getKey();     
        switch(b) {
            case ' ':
                a--; 
                break;
            case 'C':   // use 'C' to delete input
                if (a > 0){
                    a = a-2;
                    lcd.locate(13+a, 1);    
                    lcd.putc('_'); 
                    if ( code[a+1] == keyCode[a+1]){
                        y--;    
                    }                    
                }
                else if (a == 0){
                    a--;
                }
                break;              
            default:
                code[a] = b; 
                lcd.locate(12+a, 1);    
                lcd.putc('*');
                if(code[a] == keyCode[a]) {
                    y++; 
                }
        }
    }
    // from entry mode : if idle is high display alarm mode
    if (idle_flag == true and state_num == 4){
        pc.printf("\n\r Alarm mode\n\r");       
        } 
        
    // from exit mode : if idle is high display set mode            
    else if (idle_flag == true and state_num == 2){
        pc.printf("\n\r Set mode\n\r");       
        lcd.cls();
        lcd.printf("Set mode");  } 
    
    else if (idle_flag == true and state_num == 5){
        Sounder = 0;
        pc.printf("\n\r Alarm mode\n\r"); 
        lcd.cls();
        lcd.printf("Alarm mode"); 
        lcd.locate(0, 1);   
        lcd.printf("Enter Code: ____");          
        }
    
    else if (switch_flag == true){
        pc.printf("\n\r Alarm mode\n\r"); 
        lcd.cls();
        lcd.printf("Alarm mode"); 
        lcd.locate(0, 1);   
        lcd.printf("Enter Code: ____"); }        
                
    else {
        pc.printf("\n\r B toggled\n\r"); 
        lcd.cls();
        lcd.locate(0,1);
        lcd.printf("Press B to set"); }
    
    while (1){
        b = getKey();  
        if (b == 'B'){
            break;}
        
        else if (idle_flag == true){break;}//flags used to break loop
        else if (switch_flag == true){break;}

    }
    
    return(y); 
} 

void setKey(void) 
{
    int a;
    char b;
    
    lcd.locate(0, 1);   
    lcd.printf("Init  Code: ____");
    for(a = 0; a < 4; a++) {
        b = getKey(); 

        switch(b){
            case ' ':
                a--; 
                break;
            case 'C':
                if (a > 0){
                    a = a-2;
                    lcd.locate(13+a, 1);    
                    lcd.putc('_');                   
                }
                else if (a == 0){
                    a--;
                }
                break; 
            default:
                lcd.locate(12+a, 1);    
                lcd.putc(b);
                keyCode[a] = b; 
                
        }
    }
    wait(1);
} 

/**********************************************************************************
Simulation of alarm internal sounder
**********************************************************************************/
void sounder_blink()
{
    Sounder = !Sounder;  
    wait(0.1); 
}

void pattern(void)
{  
    timer.attach(&sounder_blink, 0.25); 
}

 // Function interrupt to check for new user input


void idle_count(){
    //pc.printf("Idle_flag: %d\n", idle_flag);
    idle_flag = true;
    //pc.printf("Idle_flag: %d\n", idle_flag);
    
}

void switching(){
    int sw = read_switch();
    if (sw == 2){switch_flag = true;}
    //pc.printf("\r\n FLAG : %d\n\r", switch_flag);
}

void patternoff(void)
{
    timer.detach();  // turn the Sounder off 
    Sounder = 0;  
}

void Sounderoff(void)
{
    Sounder = 0;    
}
/**********************************************************************************
Set system state
**********************************************************************************/
void system_alarm(void)
{
    state = alarm; 
    time_expired = true;    
}

void system_set(void)
{
    state = set; 
    time_expired = true;   
}