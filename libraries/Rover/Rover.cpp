/*
Author: Louis Thiery

COPYRIGHT
This work is released under Attribution-ShareAlike 3.0 Unported (CC BY-SA 3.0)
For details, visit: http://creativecommons.org/licenses/by-sa/3.0/

DESCRIPTION
This is a library written to help manage the EEPROM interactions with the Arduino's AVR chip
Handles booleans, ints, and strings. Strings were chosen for phone numbers since they can be of any relevant length.

All numbers are authorized to text for updates. Any number can text for update if option is chosen.

Use constructor to create class variables, run start() in setup to get variables from EEPROM, 
Use any of the get functions during the loop to get data from the class variables (not EEPROM) 
Any of the set methods will rewrite the EEPROM memory and refresh the class variable
*/

#include "Arduino.h"
#include "EEPROM.h"
#include "Rover.h"

#define FLAG_0 0 //first byte of EEPROM contains a set of 8 booleans
//on FLAG_0

#define PIN1 10 //4 char string plus #
#define PIN2 15
//there's room from 23-100
//110+16*13 is where the end of user info is

//4 bytes serving as flags to tell if there is a number somewhere
#define UFLAG 100 //will take up 2 spots
#define LENGTH_OF_USER 14 
//location of number strings, appended by their usergroup
//9=owner,0-2 levels of alarms
#define NUMBERS_LOC 110 //takes X+1 bytes, X numbers in char form appended with # 

//CONSTRUCTOR
Rover::Rover(){
  char num[LENGTH_OF_NUM];
  byte _uFlag[2];
}

void Rover::start(void){
  for(int i=0;i<2;i++) _uFlag[i]=EEPROM.read(UFLAG+i);
  getOwner();
}
//PUBLIC METHODS

//USER SYSTEM RELATED
void Rover::setOwner(const char c[]){
  EEPROM.write(NUMBERS_LOC,9);
  _writeStr(NUMBERS_LOC+1,c,LENGTH_OF_NUM);
  bitSet(_uFlag[0],0); //update bit in prog mem
  EEPROM.write(UFLAG,_uFlag[0]); //update byte in EEPROM
  getOwner();
}

void Rover::getOwner(void){
  if (bitRead(EEPROM.read(UFLAG),0)) _getNum(NUMBERS_LOC+1);//owner will always be first user
  else for(uint8_t i=0;i<LENGTH_OF_NUM;i++) num[i]='0';
}


uint8_t Rover::getUserGroup(uint8_t user){
  return EEPROM.read(_userLoc(user));
}

void Rover::getUserContact(uint8_t user){
  _getNum(_userLoc(user)+1);
}

void Rover::clearUsers(){
  //clear all user flags in progmem
  for(int i=0;i<2;i++){
    _uFlag[i]=0;
    if(i==0) bitSet(_uFlag[i],0);//keep owner
  }
  //write this to EEPROM;
  for(int i=0;i<2;i++){
    EEPROM.write(UFLAG+i,_uFlag[i]);
  }
}


void Rover::addUser(uint8_t ug,const char c[]){
  //look for first open spot
  for(int i=0;i<2;i++) for(int j=0;j<8;j++) if(!bitRead(_uFlag[i],j)){
	EEPROM.write(_userLoc(j+i*8),ug); //write UG to first spot
	_writeStr(_userLoc(j+i*8)+1,c,LENGTH_OF_NUM+1); //follow 11 spots hold number
	bitSet(_uFlag[i],j); //update bit in prog mem
	EEPROM.write(UFLAG+i,_uFlag[i]); //update byte in EEPROM
	return;
      }
  Serial.println("No more room for users!");
}

void Rover::removeUser(uint8_t user){
  if(user==0) return; //don't remove owner or non-user
  bitClear(_uFlag[user/8],user%8);
  EEPROM.write(UFLAG+user/8,_uFlag[user/8]);
}

void Rover::removeUser(const char c[]){
  uint8_t userNum = findUser(c);
  if(userNum==255) return;
  else removeUser(userNum);
}

uint8_t Rover::findUser(const char c[]){
  for(int i=0;i<2;i++) for(int j=0;j<8;j++) if(bitRead(_uFlag[i],j)){
	getUserContact(i*8+j);
	if(strncmp(c,num,LENGTH_OF_NUM)==0) return i*8+j;
      } 
  return 255;
}  

void Rover::setPin(bool two,const char c[]){
  if(two) _writeStr(PIN2,c,4);
  else _writeStr(PIN1,c,4);
}

bool Rover::isPin(bool two,const char c[]){
  char pin[4];
  for(int i=0;i<4;i++){
    if(two) pin[i]=EEPROM.read(PIN2+i);
    else pin[i]=EEPROM.read(PIN1+i);
  }
  return (strncmp(pin,c,4)==0);
}


//PRIVATE METHODS
void Rover::_setBool(int bit, bool IF){
  byte temp = EEPROM.read(FLAG_0);
  if(IF){bitSet(temp,bit);}
  else{bitClear(temp,bit);}
  EEPROM.write(FLAG_0, temp);
}
uint8_t Rover::_readInt(int location){
  return EEPROM.read(location);
}

void Rover::_writeInt(int location, uint8_t num){
  EEPROM.write(location, num);
}

void Rover::_writeStr(int location,const char c[], uint8_t length){
  for(uint8_t i=0;i<length;i++){
    EEPROM.write(location+i, c[i]);
  }
}


void Rover::_getStr(int location, int length){
  for(uint8_t i=0;i<length;i++){
    num[i]=EEPROM.read(location+i);
  }
}

void Rover::_getNum(int location){
  _getStr(location,LENGTH_OF_NUM);
}

int Rover::_userLoc(uint8_t user){
  return NUMBERS_LOC+user*13;
}
