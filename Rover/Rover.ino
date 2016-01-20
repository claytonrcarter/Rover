#include <Rover.h>

#include <avr/pgmspace.h> //to directly access PROGMEM
#include <EEPROM.h>//EEPROM RELATED
#include <SoftwareSerial.h>//CELLPHONE RELATED
#include <GSMSerial.h>
#include <Wire.h>

//pins where things should be
#define PHONE_RX 5 // prototype red
#define PHONE_TX 6 // prototype green
#define PST 11 // pin wired for the PowerSwitch Tail
#define LED 13

//compiler variables
#define LENGTH_OF_NUM 10
#define MAX_PROGMEM_STR 32

prog_char msg_1[] PROGMEM = "You are Rover's new owner! ";
prog_char msg_2[] PROGMEM = "You are now friends with Rover!"; //31+1
prog_char msg_3[] PROGMEM = "Invalid msg from ";
prog_char msg_4[] PROGMEM = "Fence is OFF.";
prog_char msg_5[] PROGMEM = "Fence is ON.";
prog_char msg_6[] PROGMEM = "Power restored. ";
prog_char msg_7[] PROGMEM = "Power failure. ";
prog_char msg_help[] PROGMEM = "Valid commands: ON, OFF, STATUS, HELP";

//prog_char cmd_on[]  = "ON";
//prog_char cmd_off[] = "OFF";

#define CHIP_SELECT 10
Rover rover;
GSMSerial phone(5,6); //5 red, 6 yellow


//flags:  0=none
//        1=sent power alarm
//        2=none
byte flags=0;
boolean fenceOn = false;
int testByte = 0;


void setup() {

  Serial.begin(9600); // baud rate?
  Serial.println("start up");
  firstConfiguration(); //will only execute first time
  //  defaultRover(); //manual reset - will "erase" Rover on start up

  Wire.begin();
  rover.start();

  pinMode(LED, OUTPUT);
  pinMode(PST, OUTPUT);

  phone.start();
  analogReference(EXTERNAL);
  
  rover.getOwner(); //just to make sure rover.num is owner's num
  for(uint8_t i=0;i<LENGTH_OF_NUM+1;i++){
    phone.origin[i] = rover.num[i];
  }
  turnFenceOn();

}

void loop(){

  rover.getOwner(); //just to make sure rover.num is owner's num

  //************************************************************
  // Check for texts
  //************************************************************
  if( phone.hasTxt() ){
    // blink the LED while we process texts
    digitalWrite(LED,HIGH);
    manageTxt();
    phone.deleteTxts();
    digitalWrite(LED,LOW);
  }

  //************************************************************
  // check status of power supply
  //************************************************************
  // if power is out and we haven't sent the warning
  //************************************************************
  if( ! phone.isCharging() && bitRead( flags, 1 ) == 0 ) {
    
    // make sure to turn the fence off when the power fails
    // save a tiny bit of juice to the PST, plus make STATUS requests accurate
    turnFenceOff( true ); // true == suppress warning    
    
    for(int i=0;i<2;i++) for(int j=0;j<8;j++) if(bitRead(EEPROM.read(100+i),j)){
      if(rover.getUserGroup(i*4+j)>=2){
        rover.getUserContact(i*4+j);
        strcpy_P(phone.buffer, msg_7);
        strcat_P(phone.buffer, msg_4);
        phone.sendTxt(rover.num, phone.buffer);
        Serial.println("Power warning sent to: ");
        Serial.println(rover.num);
      }
    }

    bitSet( flags, 1 ); //set flag
  }
  //************************************************************
  // if power in on and we haven't reset the "sent flag"
  //************************************************************
  else if ( phone.isCharging() && bitRead(flags,1) == 1 ) {
    
    // make sure to turn the fence on when the power is restored??
    turnFenceOn( true ); // true == suppress warning
    
    for(int i=0;i<2;i++) for(int j=0;j<8;j++) if(bitRead(EEPROM.read(100+i),j)){
      if(rover.getUserGroup(i*4+j)>=2){
        rover.getUserContact(i*4+j);
        strcpy_P(phone.buffer, msg_6);
        strcat_P(phone.buffer, msg_5);
        phone.sendTxt(rover.num, phone.buffer);
        Serial.println("Power restored message sent to: ");
        Serial.println(rover.num);
      }
    }
    
    bitClear( flags, 1 ); //reset flag
  }


  //************************************************************
  // now wait for 2 seconds
  //************************************************************
  delay(2000); 
}


//************************************************************
//************************************************************
void firstConfiguration(){
  if(EEPROM.read(99)==1) return;
  defaultRover();
  EEPROM.write(99,1);
}

//************************************************************
//************************************************************
void defaultRover(){
  for(uint8_t i=0;i<2;i++) EEPROM.write(100+i,0);
  rover.setPin(0,"rovr");
  rover.setPin(1,"RVR");
  // clear users?
  Serial.println("Default Rover set");
}


//************************************************************
//************************************************************
void manageTxt(){
  
  //the next line is to make sure we've actually received a text
  //checks the integrity of phone.origin  
  if(phone.content[1]=='\0') return;
  for(int i=0;i<LENGTH_OF_NUM;i++) if(phone.origin[i]=='\0') return;
  Serial.print("New text from: ");
  Serial.println(phone.origin);
  Serial.println(phone.content);

  //************************************************************
  // if it starts with # then we're doing something important 
  // setting new owner or friend
  //************************************************************
  if(phone.content[0]=='#'){ 
    char pin[5]; 
    for(int i=0;i<5;i++) pin[i]=phone.content[i+1];

    //************************************************************
    // master pin supplied
    //************************************************************
    if(rover.isPin(0,pin)) {
      if( true || strncmp(rover.num, phone.origin, LENGTH_OF_NUM)!=0){ //make them owner if not already so
        if(rover.findUser(phone.origin)!=255) rover.removeUser(rover.findUser(phone.origin)); //remove them if they are normal user
        rover.setOwner(phone.origin);
        // send "new owner" message
        strcpy_P(phone.buffer, msg_1);
//        phone.sendTxt(phone.origin, phone.buffer);
        // send "help" message
        strcat_P( phone.buffer, msg_help );
        phone.sendTxt(phone.origin, phone.buffer);
        
        sendFenceStatus();
        
        Serial.print("New owner: ");
        Serial.println(phone.origin);
      }

    }

    //************************************************************
    // secondary pin supplied
    //************************************************************
    else if(rover.isPin(1,pin) && rover.findUser(phone.origin)!=9){  //if they've given sub pin, make them user (unless they're owner!)
      if(rover.findUser(phone.origin)!=255) rover.removeUser(rover.findUser(phone.origin)); //delete them if they already exist
      strcpy_P(phone.buffer, msg_2);
      rover.addUser(atoi(phone.content[6]),phone.origin);
      phone.sendTxt(phone.origin, phone.buffer);
    }

    else { //maybe someone is guessing pin?
      strangerDanger(); //tell owner
    }
  }

  //************************************************************
  // text doesn't start with #, so we're just doing regular work
  //************************************************************
  else{

    // make sure that we know the person sending the command
    if ( rover.findUser(phone.origin) != 255 ) {

      // using strncmp to ignore any trailing punc or whitespace
      if ( strncmp( phone.content, "Off", 3 ) == 0 ||
           strncmp( phone.content, "off", 3 ) == 0 ||
           strncmp( phone.content, "OFF", 3 ) == 0 ) {

        turnFenceOff();

      }
      else if ( strncmp( phone.content, "On", 2 ) == 0 ||
                strncmp( phone.content, "on", 2 ) == 0 ||
                strncmp( phone.content, "ON", 2 ) == 0 ) {

        turnFenceOn();

      }
      else if ( strncmp( phone.content, "Status", 6 ) == 0 ||
                strncmp( phone.content, "status", 6 ) == 0 ||
                strncmp( phone.content, "STATUS", 6 ) == 0 ) {

        sendFenceStatus();

      }
      else if ( strncmp( phone.content, "Help", 4 ) == 0 ||
                strncmp( phone.content, "help", 4 ) == 0 ||
                strncmp( phone.content, "HELP", 4 ) == 0 ) {
                  
        strcpy_P(phone.buffer, msg_help);
        phone.sendTxt(phone.origin, phone.buffer);

      }
    }
    
    else {
      strangerDanger();
    }

  }
}


// no args == not quiet
void turnFenceOn() { turnFenceOn( false ); }
void turnFenceOn( boolean quiet ) {
  
  digitalWrite( PST, HIGH );
  fenceOn = true;

  // don't send the update if we're supposed to be quiet
  if ( ! quiet ) {
    sendFenceStatus();

    Serial.print("Fence turned ON by ");
    Serial.println(phone.origin); 
  }
}

void turnFenceOff() { turnFenceOff( false ); }
void turnFenceOff( boolean quiet ) {
  digitalWrite( PST, LOW );
  fenceOn = false;

  // don't send the update if we're supposed to be quiet
  if ( ! quiet ) {
    sendFenceStatus();

    Serial.print("Fence turned OFF by ");
    Serial.println(phone.origin);
  }
}

void sendFenceStatus() {
  
  if ( fenceOn ) {
    
    strcpy_P(phone.buffer, msg_5);
    phone.sendTxt(phone.origin, phone.buffer);
    Serial.println("Fence is ON.");
    
  }
  else {
    
    strcpy_P(phone.buffer, msg_4);
    phone.sendTxt(phone.origin, phone.buffer);          
    Serial.println("Fence is OFF.");
    
  }
  
}

void strangerDanger(){
  strcpy_P(phone.buffer, msg_3);  
  phone.openTxt(rover.num);
  phone.inTxt(phone.buffer);
  phone.inTxt(phone.origin);
  phone.closeTxt();
}


uint8_t atoi(char c) {
  return c-48;
}

char itoa(uint8_t c) {
  return c+48;
}


