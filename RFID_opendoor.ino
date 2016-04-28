/*    Copyright (C) 2015 Gianmarco Garrisi
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see http://www.gnu.org/licenses/ .
 *
 *
 *
 *
 *
 *    Access control system based on the RFID reader ID-20 from Innovations:
 *    the input "DOORPHONEIN" is used as an external command to send
 *    the signal to open the door;
 *    the output "OPENPIN" is a digital output that is set HIGH for
 *    2 seconds when the door should be opened.
 *    Trusted cards are hold in EEPROM memory,
 *    that is organised as follow:
 *    the first byte memorize how many cards there are;
 *    for each card, 'CARDBYTES' bytes memorize the code and 'NAMEBYTES' bytes
 *    memorize a name for the card.
 *    This way more then 'MAXCARDS' cards can be trusted a time.
 *    You can change the values to memorize more cards, to memorize cards with
 *    longer code or to give longer names, but remember that Arduino Uno has
 *    1024 bytes of EEPROM, so ((CARDBYTES + NAMEBYTES)*MAXCARDS)+1 must be lesser then 1024.
 *    To be faster, the list of the cards is charged in SRAM at the setup in a dinamically allocated
 *    array of bytes arrays for codes and of strings (char arrays terminated with '\0') for names,
 *    and another dinamical array contains references to the ordered list of codes.
 *    So it is possible to use a binary search, wich is faster, to check if a card is allowed.
 *
 *    The "read_id" function was on http://playground.arduino.cc/Code/ID12 as the "loop" of the code
 *    RFID reader ID-12 for Arduino
 *    Based on code by BARRAGAN http://people.interaction-ivrea.it/h.barragan
 *    and code from HC Gilje - http://hcgilje.wordpress.com/resources/rfid_id12_tagreader/
 *    Modified for Arudino by djmatic
 *    Modified for ID-12 and checksum by Martijn The - http://www.martijnthe.nl/
 *
 *    Use the drawings from HC Gilje to wire up the ID-12.
 *    Remark: disconnect the rx serial wire to the ID-12 when uploading the sketch */

#include <LiquidCrystal.h>
#include <EEPROM.h>
LiquidCrystal lcd(11, 12, 4, 5, 6, 7);

#define DOORPHONEIN 8
#define OPENPIN 2
#define LCDLED 3
#define KEYPIN A0

#define CARDBYTES 5
#define NAMEBYTES 5
#define MAXCARDS  100

char alphanumspace[26+10+1];
byte ncards;
byte **cards;
char **names;
byte *cardsOrdered;

void setup()
{
    int i;
    Serial.begin(9600);                // connect to the serial port
    pinMode(DOORPHONEIN, INPUT_PULLUP);
    pinMode(OPENPIN, OUTPUT);
    pinMode(LCDLED, OUTPUT);
    lcd.begin(16, 2);
    alphanumspace[0]=' ';             // create an array contening the alphanumeric chars and the space
    for (i = 1; i<1+26; i++)
        alphanumspace[i] = 'A'+i-1;
    for (; i<26+10+1; i++)
        alphanumspace[i] = '0'+i-1-26;
    extract();                       // extract data from EEPROM to SRAM
}

void loop()
{
    if (check_id()) {                   // if a trusted card can be read, open the door
        lcd.clear();
        lcd.print("Accesso");
        lcd.display();
        digitalWrite(LCDLED, HIGH);
        lcd.setCursor(0, 1);
        lcd.print("consentito");
        opendoor();
        digitalWrite(LCDLED, LOW);
        lcd.noDisplay();
    } else if (digitalRead(DOORPHONEIN) == LOW) { // if the button to open the door is pressed on the doorphone, open the door
        opendoor();
    }
    switch (keypressed()) { // if a button of the pad is pressed, check witch button is...
      case 1:      // if it's the first, add a new card
          addCard();
        break;
      case 4:                              // If it's the fourth key start the procedure to delete a card
          deleteCard();
        break;
      default:
        break;
    }
}

void freeDB()                               //free the memory space allocated by the last call to 'extract()'
{
    for (byte i = 0; i < ncards; i++) {
        free(cards[i]);
        free(names[i]);
    }
    free(cards);
    free(names);
}
void extract()                               //allocate the arrays and extract data from the EEPROM to SRAM
{
    byte h, i, j;
    ncards = EEPROM.read(0);

    cards = (byte **)malloc(ncards*sizeof(*cards));
    names = (char **)malloc(ncards*sizeof(*names));

    cardsOrdered = (byte *)malloc(ncards*sizeof(*cardsOrdered));


    for (i = 0; i<ncards; i++) {
        cards[i] = (byte *)malloc(CARDBYTES*sizeof(*cards[i]));
        names[i] = (char *)malloc(1+NAMEBYTES*sizeof(*names[i]));
    }

    for (i = 0; i < ncards; i++) {
        for (j = 0; j < CARDBYTES; j++) {
            cards[i][j] = EEPROM.read(1+j+i*(CARDBYTES+NAMEBYTES));
        }

        for (j = 0; j < NAMEBYTES; j++) {
            names[i][j] = EEPROM.read(i*(CARDBYTES+NAMEBYTES)+1+CARDBYTES+j);
        }
        names[i][j] = '\0';

        cardsOrdered[i] = i;
    }


    for (h=1; h < ncards/9; h = 3*h+1);
    while (h>0) {
        for (i = h; i < ncards; i++) {
            j = i;
            byte tmp = cardsOrdered[i];
            while (j>= h && codecmp(cards[cardsOrdered[i]], cards[tmp]) > 0) {
                cardsOrdered[j] = cardsOrdered[j-h];
                j-=h;
            }
            cardsOrdered[j] = tmp;
        }
        h /= 3;
    }
}
void opendoor()                     // open the door
{
    digitalWrite(OPENPIN, HIGH);
    delay(2000);
    digitalWrite(OPENPIN, LOW);
}
int codecmp(byte code1[], byte code2[])     // return 0 if the arrays of bytes are equal, or the difference between the first different characters
{
    for (byte i=0; i<CARDBYTES; i++) {
        if (code1[i] != code2[i]) return code1[i]-code2[i];
    }
    return 0;
}
boolean read_id (byte code[])                  // returns a String contening the bytes of the code
{
    byte i = 0;
    byte val = 0;
    byte checksum = 0;
    byte bytesread = 0;
    byte tempbyte = 0;

    if(Serial.available()) {
        if((val = Serial.read()) == 2) {                  // check for header
            bytesread = 0;
            while (bytesread < (CARDBYTES*2)+2) {                        // read cardbytes*2 exadecimal digit code + 2 digit checksum
                if(Serial.available()) {
                    val = Serial.read();
                    if((val == 0x0D)||(val == 0x0A)||(val == 0x03)||(val == 0x02)) { // if header or stop bytes before the 10 digit reading
                        break;                                    // stop reading
                    }

                    // Do Ascii/Hex conversion:
                    if ((val >= '0') && (val <= '9')) val = val - '0';
                    else if ((val >= 'A') && (val <= 'F')) val = 10 + val - 'A';

                    // Every two hex-digits, add byte to code:
                    if (bytesread & 1 == 1) {
                        // make some space for this hex-digit by
                        // shifting the previous hex-digit with 4 bits to the left:
                        code[bytesread >> 1] = (val | (tempbyte << 4));

                        if (bytesread >> 1 != 5) {                // If we're at the checksum byte,
                            checksum ^= code[bytesread >> 1];       // Calculate the checksum... (XOR)
                        };
                    } else {
                        tempbyte = val;                           // Store the first hex digit first...
                    };

                    bytesread++;                                // ready to read next digit
                }
            }

            if (bytesread == (CARDBYTES*2)+2 && code[CARDBYTES] == checksum)                          // if 10 digit read is complete
                return true;                                                                            // and the checksum is ok, return true...
        }
    }
    return false;                                                        // ... else return false
}
boolean check_id ()                                  // return true if the code belongs to a trusted card, else returns false
{
    byte ID[CARDBYTES];
    int res;
    int a = 0, b = ncards-1, c;
    if (read_id(ID) == 0) return false;
    while (a<=b) {
        c = (a+b)/2;
        if ((res = codecmp(ID, cards[cardsOrdered[c]])) == 0) return true;
        if (res < 0) b = c-1;
        else a = c+1;
    }
    return false;
}
String BtoX(byte unconv[], int len)                   //return a string converted from bytes to esadecimal
{
    char conv[2*len+1];
    char tmp;

    for (int i = 0; i<len; i++) {
        conv [2*i] = ( (tmp = unconv[i]>>4)>9 ? 'A'+tmp-9 : '0' + tmp);
        conv[2*i+1] = ( (tmp = unconv[i]-(tmp<<4))>9 ? 'A'+tmp-9: '0'+tmp);
    }
    conv[2*len+1] = '\0';
    return String(conv);
}
byte keypressed()         //returns witch button was pressed
{

    int keyVal = analogRead(KEYPIN);

    if (keyVal > 1020 && keyVal <= 1023)
        return 1;
    else if (keyVal > 990 && keyVal < 1010)
        return 2;
    else if(keyVal > 505 && keyVal < 515)
        return 3;
    else if(keyVal > 5 && keyVal < 15)
        return 4;
    else
        return 0;
}
void addCard()
{
    byte stop = 0;
    int i, j;
    if (ncards == MAXCARDS) {
        lcd.clear();
        lcd.print("Memoria piena");
        lcd.display();
        digitalWrite(LCDLED, HIGH);
        lcd.setCursor(0,1);
        lcd.print("Eliminare alcune");
        delay(500);
        lcd.clear();
        lcd.print("Eliminare alcune");
        lcd.setCursor(0,1);
        lcd.print("schede e riprovare");
        stop = 1;
        delay(2000);
        digitalWrite (LCDLED, LOW);
        lcd.noDisplay();
    }
    byte newID[CARDBYTES];
    lcd.clear();
    lcd.print("Aggiungi scheda:");
    lcd.display();
    digitalWrite(LCDLED, HIGH);
    lcd.setCursor(0, 1);
    lcd.print("Avvicinare RFID");
    for (i=0; i<1000 && !stop; i++) {   //wait 5 seconds to read the new card
        if (read_id(newID)) break;
        if(keypressed()==4) {           //the 4th key cancel the operation
            lcd.clear();
            lcd.print("Operazione");
            lcd.setCursor(0, 1);
            lcd.print("Annullata");
            delay(1000);
            stop = 1;
            digitalWrite(LCDLED, LOW);
            lcd.noDisplay();
        }
        delay(5);
    }
    if (i == 1000 && !stop) {     //if no card is read within 5 seconds the operation is cancelled
        lcd.clear();
        lcd.print("Nessuna scheda");
        lcd.setCursor(0, 1);
        lcd.print("rilevata");
        delay(1000);
        stop = 1;
        digitalWrite(LCDLED, LOW);
        lcd.noDisplay();
    } else if (!stop) {          //if the operation wasn't cancelled
        {
            //check if the card was known yet
            int a = 0, b = ncards-1, c, res;
            while (a<=b) {
                c = (a+b)/2;
                if ((res = codecmp(newID, cards[cardsOrdered[c]])) == 0) {
                    lcd.clear();
                    lcd.print("Questa carta e'");
                    lcd.setCursor(0, 1);
                    lcd.print("gia' conosciuta");
                    delay(1000);
                    digitalWrite(LCDLED, LOW);
                    lcd.noDisplay();
                    stop = 1;
                    break;
                }
                if (res < 0) b = c-1;
                else a = c+1;
            }
        }
        if (stop) return;
        // else the user must insert a name to remember the card, in case it has to be delated
        lcd.clear();
        lcd.print("Inserisci ora");
        lcd.setCursor(0, 1);
        lcd.print("un nome di");
        delay(500);
        lcd.clear();
        lcd.print("un nome di");
        lcd.setCursor(0, 1);
        lcd.print("5 caratteri");
        delay(500);
        lcd.clear();
        lcd.print("5 caratteri");
        lcd.setCursor(0, 1);
        lcd.print("per identificarla");
        delay(1000);
        lcd.clear();
        lcd.print("Scheda: ");
        lcd.print(BtoX(newID, CARDBYTES));
        char name[NAMEBYTES+1] = "";
        int c;
        for (i=0; i<NAMEBYTES && !stop; i++) { //to insert the name, the user has to select each character from the array.
            c=0;
            lcd.setCursor(i,1);
            lcd.blink();
            for (j=0; j<1000 && !stop; j++) {  //there are 30 seconds to intert the name, the "timer" restarts whenever a key is pressed.
                lcd.setCursor(i,1);
                lcd.print(alphanumspace[c]);
                switch (keypressed()) {
                case 1:
                    name[i] = alphanumspace[c];
                    j = 1000;
                    delay(100);
                    break;
                case 2:
                    (c==0) ? (c = 36) : c--;
                    j = 0;
                    delay(100);
                    break;
                case 3:
                    (c==36) ? (c = 0) : c++;
                    j = 0;
                    delay(100);
                    break;
                case 4:
                    lcd.clear();
                    lcd.print("Operazione");
                    lcd.setCursor(0, 1);
                    lcd.print("Annullata");
                    delay(1000);
                    stop = 1;
                    digitalWrite(LCDLED, LOW);
                    lcd.noDisplay();
                    break;
                default:
                    delay(30);
                    break;
                }
            }
        }
        lcd.noBlink();
        if (!stop) {                                          //Now the new card and the related name are saved in the EEPROM and the database is recharged
            EEPROM.write(0, ncards+1);
            for (i = 0; i<CARDBYTES; i++)
                EEPROM.write( ncards*(CARDBYTES+NAMEBYTES)+1+i, newID[i]);

            for (i = 0; i<NAMEBYTES; i++)
                EEPROM.write( ncards*(CARDBYTES+NAMEBYTES)+1+CARDBYTES+i, name[i]);

            freeDB();
            extract();
            stop = 1;
            lcd.clear();
            lcd.print("Tessera");
            lcd.setCursor(0,1);
            lcd.print("aggiunta");
            delay(1000);
            digitalWrite(LCDLED, LOW);
            lcd.noDisplay();
        }
    }
}
void deleteCard()
{
    int i, j;
    char stop = 0;
    lcd.clear();
    lcd.print("Elimina Scheda");
    lcd.display();
    digitalWrite(LCDLED, HIGH);
    if (ncards == 0){
      delay(1000);
        lcd.clear();
        lcd.print("Nessuna scheda");
        lcd.display();
        digitalWrite(LCDLED, HIGH);
        lcd.setCursor(0,1);
        lcd.print("memorizzata!");
        stop = 1;
        delay(2000);
        digitalWrite (LCDLED, LOW);
        lcd.noDisplay();
        return;
    }
    lcd.setCursor(0,1);
    lcd.print("Seleziona la scheda");
    delay(1000);
    int c = 0;
    for (i = 0; i < 1000 && !stop; i++) {   //there are 30 seconds to select the card to eliminate
        lcd.clear();
        lcd.print("Scheda: ");
        lcd.print(BtoX(cards[c], CARDBYTES));
        lcd.setCursor(0,1);
        lcd.print("Nome: ");
        lcd.print(names[c]);
        switch (keypressed()) {
        case 1:                              //in case of selection is required a confirm
            lcd.clear();
            lcd.print("Premi di nuovo");
            lcd.setCursor(0,1);
            lcd.print("per confermare");
            delay(1000);
            lcd.clear();
            lcd.print("Scheda: ");
            lcd.print(BtoX(cards[c], CARDBYTES));
            lcd.setCursor(0,1);
            lcd.print("Nome: ");
            lcd.print(names[c]);
            for(j=0; j<1000 && !stop; j++) {
                switch (keypressed()) {
                case 1:                    //in case of confirm the card is eliminated and the database recharged
                    int n;
                    stop = 1;
                    EEPROM.write(0, ncards-1);
                    for(int k = c; k < ncards; k++) {
                        for(n=0; n<CARDBYTES; n++)
                            EEPROM.write((k-1)*(CARDBYTES+NAMEBYTES)+1+n, cards[k][n]);
                        for(n=0; n<NAMEBYTES; n++)
                            EEPROM.write((k-1)*(CARDBYTES+NAMEBYTES)+1+CARDBYTES+n, names[k][n]);
                    }
                    freeDB();
                    extract();
                    lcd.clear();
                    lcd.print("Scheda");
                    lcd.setCursor(0,1);
                    lcd.print("eliminata");
                    delay(1000);
                    digitalWrite(LCDLED, LOW);
                    lcd.noDisplay();
                    break;
                case 4:
                    j = 1000;
                    break;
                default:
                    delay(30);
                    break;
                }
                if (j==1000) {
                    lcd.clear();
                    lcd.print("Operazione");
                    lcd.setCursor(0, 1);
                    lcd.print("Annullata");
                    delay(1000);
                    stop = 1;
                    digitalWrite(LCDLED, LOW);
                    lcd.noDisplay();
                }
            }
            break;
        case 2:
            (c==0) ? (c=ncards) : c--;
            i = 0;
            delay(100);
            break;
        case 3:
            (c==ncards) ? (c = 0) : c++;
            i = 0;
            delay(100);
            break;
        case 4:
            lcd.clear();
            lcd.print("Operazione");
            lcd.setCursor(0, 1);
            lcd.print("Annullata");
            delay(1000);
            stop = 1;
            digitalWrite(LCDLED, LOW);
            lcd.noDisplay();
            break;
        default:
            delay(30);
            break;
        }
    }
}
