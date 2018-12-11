#include <LinkedList.h>
#include <Arduino.h>

// Pins
const int KNOPPEN[] = {2, 3, 4, 5}; // Dit zijn de pinnen die verbonden zijn met de KNOPPEN
const int LEDS[] = {6, 7, 8, 9};    // Dit zijn de pinnen die verbonden zijn met de LEDS
const int BUZZER_PIN = A2;          // Buzzer pin, geeft speler feedback tijdens het spel
const int TX_PIN = A0;              // De pin waarover de data verstuurd zal worden
const int RX_PIN = A1;              // De pin waarover de data ontvangen zal worden
const int RGB_PIN[] = {10, 11, 12}; // De drie outputs verbonden met de RBG status-led

// Constants
const int WAIT_TIME = 50;          // De tijd die een controller wacht om data te beginnen lezen/sturen
const int PLAYER_WAIT_TIME = 2000; // De maximale tijd die een speler heeft om een "move" te doen (in ms)
const int DISPLAY_TIME = 300;      // De tijd waarvoor we de combinatie-eenheden aan de gebruiker tonen
const int DATA_TIME = 20;           // De tijd die men wacht bij het versturen/ontvangen van data

const int FADE_INTERVAL = 20; // Hoe snel de status-led zijn kleurenovergang effect doet

const String SYNC_STRING = "101";  // De string die gebruikt wordt om de twee controllers te "syncen"
const String LOST_STATUS = "1100"; // De string die gebruikt wordt om aan te geven dat deze speler verloren is
const String KNOP_DATA[] = {"1000", "1001", "1010", "1011"};
const int PACKET_SIZE = 4;
/*
* 1000 ==> knop 0
* 1001 ==> knop 1
* 1010 ==> knop 2
* 1011 ==> knop 3
* 1100 ==> verloren
*
*/

// Spelvariabelen
bool mijnBeurt = false; // Een van de twee spelers moet beginnen, de speler die tijdens het opstarten knop 1 ingedrukt heeft, begint
bool gameBusy = false; // Dit is "true" als de speler bezig is met het memory spel
bool lost = false;     // Dit is "true" als de speler het spel verloren heeft

bool buttonDown = false; // Heeft de gebruiker een knop ingedrukt (wordt gebruikt om dubbele detectie te voorkomen)

long inputTime = 0; // Houdt bij hoelang de speler over een "move" doet
int inputIndex = 0; // Houdt bij aan welke knop de speler zit in het memory spel

LinkedList<int> memory = LinkedList<int>(); // Houdt de huidige combinatie van knoppen bij

///
/// Stuurt de RGB status-led aan met drie kleurwaarden voor R, G en B.
/// TODO: duidelijk maken met Enum of dergelijke voor kleurnamen
///
void setColor(int redValue, int greenValue, int blueValue)
{
    analogWrite(RGB_PIN[0], redValue);
    analogWrite(RGB_PIN[2], greenValue);
    analogWrite(RGB_PIN[1], blueValue);
}

///
/// Flasht alle leds <aantal> keer
///
void flash(int aantal)
{
    for (int i = 0; i < aantal; i++) // flash de status-led drie keer om te tonen dat de speler mag proberen
    {
        setColor(0, 0, 255);     // Zet de kleur naar blauw
        delay(DISPLAY_TIME);     // Wacht
        setColor(0, 0, 0);       // Zet de status-led uit
        delay(DISPLAY_TIME / 2); // Wacht
    }
}

///
/// Geeft de waarde (1 of 0) van de knop op index 'i' weer
///
int digitalReadButton(int index)
{
    return digitalRead(KNOPPEN[index]) == HIGH ? LOW : HIGH;
}

///
/// Toont de huidige combinatie aan de speler voor hij aan zijn beurt begint.
///
void toonCombinatie()
{
    Serial.println("Toon combinatie...");
    for (int i = 0; i < memory.size(); i++) // Toon de sequentie
    {
        Serial.println(String(i));
        Serial.println("Led: " + String(LEDS[memory.get(i)]));
        digitalWrite(LEDS[memory.get(i)], HIGH);
        delay(DISPLAY_TIME);
        digitalWrite(LEDS[memory.get(i)], LOW);
        delay(DISPLAY_TIME / 2);
    }

    flash(3); // Flash de status-led drie keer
}

///
/// Voegt een nieuwe knop toe aan de combinatie
/// Als de speler meerdere knoppen tegelijk indrukt geeft het een foutsignaal en probeert opnieuw
///
int addCombinatie()
{
    bool done = false; // Is de nieuwe knop toegevoegd?
    while (!done)      // Zolang de nieuwe knop niet succesvol toegevoegd is, herhaal:
    {
        Serial.println("Run add");
        int knop = -1;              // De knop die op dit moment ingedrukt wordt
                                    // -1 ==> geen knop ingedrukt
        for (int i = 0; i < 4; i++) // Check welke knop(pen) ingedrukt is (zijn)
        {
            if (digitalReadButton(i) == HIGH)
            {
                Serial.println("De nieuwe knop is: " + String(i));
                if (knop != -1) // Er zijn meerdere knoppen ingedrukt
                {
                    flash(1);   // Geef foutsignaal
                    delay(200); // Wait a bit
                    flash(1);   // Geef foutsignaal opnieuw
                    knop = -1;  // reset de knop
                    break;
                }
                knop = i;
            }
        }
        if (knop != -1)
        {
            memory.add(knop);
            done = true;
            return knop;
        }
    }
}

///
/// Checkt of de speler een knop ingedrukt
/// Als er meerdere knoppen ingedrukt worden ==> speler verliest
/// Als er maar één knop ingedrukt wordt:
///     - Als dit de foute knop is ==> speler verliest
///     - Als dit de correcte knop is ==> speler gaat door naar de volgende knop
/// -1 ==> speler is verloren
/// 1 ==> speler is correcte
/// 0 ==> speler heeft niets ingedrukt
///
int checkMemory()
{
    int knop = -1;              // De knop die op dit moment ingedrukt wordt
                                // -1 ==> geen knop ingedrukt
    for (int i = 0; i < 4; i++) // Check welke knop(pen) ingedrukt is (zijn)
    {
        if (digitalReadButton(i) == HIGH)
        {
            if (knop != -1) // Er zijn meerdere knoppen ingedrukt
            {
                Serial.println("Meerdere knoppen ingedrukt!");
                return -1;
            }
            knop = i;
        }
    }

    if (knop == -1)
        return 0;

    Serial.println("Knop ingedrukt: " + String(knop));
    buttonDown = true;
    if (knop == memory.get(inputIndex))
        return 1;
    else
        return -1;
}

///
/// Wordt opgeroepen wanneer deze speler het spel verliest
///
void LoseGame()
{
    Serial.println("Je hebt het spel verloren!");
    // Reset de variabelen
    gameBusy = false;
    lost = true;
    setColor(255, 0, 0);
    mijnBeurt = false;

    // Verstuur het verlies over de datalijn
    sendData(LOST_STATUS);

    while (true) // Flikker de rode led
    {
        setColor(255, 0, 0);
        delay(2 * DISPLAY_TIME);
        setColor(0, 0, 0);
        delay(DISPLAY_TIME);
        // playLostTune(); // Speel een verliezingstoontje
    }
}

///
/// Vermits de RX ge-inverteerd wordt, moeten we deze waarde terug omkeren
///
int getRX()
{
    return digitalRead(RX_PIN) == HIGH ? 0 : 1;
}

///
/// Leest de synchronisatie-sequentie en checkt of deze juist is
///
bool readSyncSequence()
{
  for (int i = 0; i < SYNC_STRING.length(); i++)
    {
        String rx = String(!getRX());            // Het tegenovergestelde van wat we ontvangen
        if (String(SYNC_STRING.charAt(i)) == rx) // Als we het foute ontvangen, is de synchronisatie mislukt
        {
            Serial.println("synchronisatie mislukt op (" + String(i) + "), we proberen opnieuw...");
            Serial.println("!ontvangen: " + rx);
            return false;
        }
        Serial.println("Bit juist ontvangen!");
        delay(DATA_TIME);
    }
    return true;
}

///
/// Maakt de controller klaar om data te lezen
///
void synchroniseControllersToRead()
{
    bool inSync = false;

    while (!inSync) // Herhaal dit zolang we niet in sync zijn
    {
        Serial.println("Begin de synchronisatie.");

        while (getRX() == HIGH) // Wacht tot de pin terug laag is om te syncen
        {
            Serial.println("Wachten tot laag...");
            delay(1);
        }

        delay(WAIT_TIME);       // Als er HOOG gestuurd wordt, wacht dat tot het terug laag is, en wacht daarna een vaste tijd
        if (readSyncSequence()) // Dit checkt of de data die aangekomen is, de "Sync sequentie" is, indien ja, zijn we in sync
        {
            Serial.println("Sync gelukt!");
            digitalWrite(TX_PIN, HIGH); // Antwoord OK, de data is in sync
            delay(2 * WAIT_TIME);       // Wacht een vaste tijd
            digitalWrite(TX_PIN, LOW);  // Reset de TX pin
            inSync = true;              // We zijn in sync, klaar om te ontvangen
            return;
        }
        Serial.println("Sync mislukt");

        delay(WAIT_TIME);

        while (getRX() == LOW) // Wacht tot de pin hoog gestuurd wordt
        {
            delay(WAIT_TIME / 2);
        }
        Serial.println("Retry");
    }
}

///
/// Maakt de controller klaar om data te versturen
///
void synchroniseControllersToWrite()
{
    bool inSync = false;

    while (!inSync) // Herhaal dit zolang we niet in sync zijn
    {
        Serial.println("Begin de synchronisatie...");
        digitalWrite(TX_PIN, HIGH); // Kondig aan de we gaan versturen
        delay(WAIT_TIME);
        digitalWrite(TX_PIN, LOW); // Reset de TX pin

        delay(WAIT_TIME); // Wacht een vaste tijd

        for (int i = 0; i < SYNC_STRING.length(); i++)
        {
            String tx = String(SYNC_STRING.charAt(i)); // De bit die verstuurd moet worden
            Serial.println("Verstuurde bit " + String(i) + "= " + tx);

            if (tx == "1") // "Verstuur" de bit
            {
              digitalWrite(TX_PIN, HIGH);
            }
            else
            {
              digitalWrite(TX_PIN, LOW);
            }
            
            delay(DATA_TIME);                // Wacht tot we de volgende bit versturen
        }
        
        delay(WAIT_TIME); // Wacht de helft van de wachttijd zodat we de RX zeker ontvangen
        if (getRX() == HIGH)
        {
            while (getRX() == HIGH)
            {
              delay(1);
            }
            delay(10);
            //delay(WAIT_TIME); // Wacht de andere helft van de tijd
            inSync = true;
            return;
        }
        else
        {
            delay(2 * WAIT_TIME);
        }
    }
}

///
/// Deze functie synchroniseert de controllers en ontvangt data
/// TODO: return enum or list index, ...
///
String receiveData()
{
    // Eerst synchroniseren we de controllers
    synchroniseControllersToRead();

    Serial.println("Klaar om te ontvangen...");
    int i = 0;
    delay(25);
    /*while (i < 20)
    {
      i++;
      Serial.println(String(getRX()));
      delay(DATA_TIME);
    }*/
    
    String data = "";
    for (int i = 0; i < PACKET_SIZE; i++) // We lezen de boodschap, bit voor bit
    {
        String rx = String(getRX());
        data += rx;
        Serial.println("Ontvangen bit " + String(i) + " = " + rx);
        delay(DATA_TIME);
    }
    Serial.println("Boodschap ontvangen: " + data);

    return data;
}

///
/// Deze functie synchroniseert de controllers en verstuurt data
///
void sendData(String data)
{
    // Eerst synchroniseren we de controllers
    synchroniseControllersToWrite();

    Serial.println("Klaar om te versturen...");
    for (int i = 0; i < PACKET_SIZE; i++) // We versturen de boodschap, bit voor bit
    {
        if (String(data.charAt(i)) == "1")
        {
          digitalWrite(TX_PIN, HIGH);
        }
        else
        {
          digitalWrite(TX_PIN, LOW);
        }
        Serial.println("Verstuurde bit " + String(i) + "= " + data.charAt(i));
        delay(DATA_TIME);
        digitalWrite(TX_PIN, LOW);
    }
    Serial.println("Boodschap verstuurd: " + data);
}

void setup() // Loopt alleen bij start
{
    // Begin de seriële comunicatie
    Serial.begin(9600);
    Serial.println("Begin seriele communicatie...");
    
    memory.add(0); // Initialiseer het spel met een beginwaarde (Dit wordt uiteindelijk de waarde van de ingedrukte startknop)

    // Initialiseer de KNOPPEN en LEDS
    for (int i = 0; i < 4; i++)
    {
        pinMode(KNOPPEN[i], INPUT_PULLUP);
        if (digitalReadButton(i) == HIGH)
        {
            mijnBeurt = true;
            break;
            Serial.println("Starting...");
        }
    }

    for (int i = 0; i < 4; i++)
    {
        pinMode(LEDS[i], OUTPUT);
    }

    // Initialiseer de status-led outputs
    pinMode(RGB_PIN[0], OUTPUT);
    pinMode(RGB_PIN[1], OUTPUT);
    pinMode(RGB_PIN[2], OUTPUT);

    // Initialiseer de TX/RX pinnen
    pinMode(TX_PIN, OUTPUT);
    pinMode(RX_PIN, INPUT);
}

void loop()
{
    // Loopt telkens opnieuw
    if (mijnBeurt)            // Speel het spel en stuur het resultaat over de POF
    {                         // Als de speler de combinatie juist heeft, dat sturen we de nieuwe knop door
                              // Als de speler een fout maakt sturen we door dat hij/zij verloren is
                              // beep(50);             // Maak een geluid zodat de speler weet dat hij/zij aan de beurt is
        gameBusy = true;      // Het spel begint
        toonCombinatie();     // Toon de combinatie tot nu toe en geef de speler signaal dat hij mag proberen
        inputTime = millis(); // Stokeer de huidige tijd, zodat we weten wanneer de speler te traag is
        while (gameBusy)
        {

            setColor(0, 255, 0); // Zet de status-led naar groen om te tonen dat de speler mag invoeren
            /*Serial.println("Status: " + String(digitalReadButton(inputIndex - 1)));
            Serial.println("i: " + String(inputIndex));
            Serial.println("Button: " + String(KNOPPEN[memory.get(inputIndex - 1)]));*/
            if (buttonDown && inputIndex != 0) // Als er een knop ingedrukt is en het is niet de eerste input van een sequentie (om dubbele detectie te voorkomen)
            {
                if (digitalReadButton(memory.get(inputIndex - 1)) == LOW) // Als de vorige correcte knop niet meer ingedrukt is, kunnen we verdergaan
                {
                    buttonDown = false;  // De knop is niet meer ingedrukt
                    setColor(255, 0, 0); // Zet de status-led op rood en wacht even (om dubbele detectie te voorkomen)
                    delay(DISPLAY_TIME); // Wachten
                    continue;
                }

                continue;
            }

            int stat = checkMemory(); // Checkt of de speler het juist heeft of niet
            //Serial.println("Status2: " + String(stat));
            if (stat == -1)
                lost = true;    // De speler gaf de foute combinatie in
            else if (stat == 1) // De speler gaf de juist combinatie in
            {
                inputIndex++;         // Update de huidige inputIndex
                inputTime = millis(); // Reset de input timer
                //Serial.println("Index: " + String(inputIndex));

                if (inputIndex == memory.size()) // Als de speler de volledige sequentie doorlopen heeft, wint hij
                {
                    Serial.println("Correct!");
                    flash(3);                   // Toon de speler dat hij gewonnen is door de staus-led te flikkeren
                    setColor(255, 0, 0);        // Zet de led op rood en laat de speler even wachten
                    delay(DISPLAY_TIME);        // Wachten
                    setColor(244, 66, 223);     // Zet de led op wit/magenta om te tonen dat de speler een nieuwe knop mag invoeren
                    int knop = addCombinatie(); // Laat de speler een nieuwe knop toevoegen aan de combinatie (en return deze nieuwe knop)
                    mijnBeurt = false;
                    inputIndex = 0;            // Reset de index
                    sendData(KNOP_DATA[knop]); // verstuur de nieuwe knop over de data lijn
                    break;
                }
            }

            //Serial.println("Huidige tijd: " + String(millis() - inputTime));
            if (lost || (millis() - inputTime > PLAYER_WAIT_TIME))
            {
                if (!lost)
                  Serial.println("You waited to long.");
                LoseGame();
                break;
            }
        }

        if (lost)
        {
            // beëindig het spel en toon score eventueel
            LoseGame();
        }
    }
    else // Het is niet mijn beurt
    {
        while (getRX() == LOW) // Wacht tot de pin hoog gestuurd wordt
        {
            setColor(12, 255, 100);
            Serial.println("Waiting...");
            delay(WAIT_TIME / 2);
        }
        setColor(0, 0, 0);

        Serial.println("Ready to receive!");
        String data = receiveData();
        if (data == LOST_STATUS) // De andere speler is verloren
        {
            Serial.println("Received LOST STATUS");
            while (true) // Flikker de groene led
            {
                setColor(0, 255, 0);
                delay(2 * DISPLAY_TIME);
                setColor(0, 0, 0);
                delay(DISPLAY_TIME);
                // playWinTune(); // Speel een overwinningstoontje
            }
        }

        int knop = -1;
        for (int i = 0; i < 4; i++) // Check welke knop doorgestuurd is
        {
            if (data == KNOP_DATA[i])
            {
                knop = i;
                break;
            }
        }
        if (knop == -1)
        {
          Serial.println("Foute waarde ontvangen.");
        }
        Serial.println("Added knop: " + String(knop));
        memory.add(knop); // Reset de variabelen
        mijnBeurt = true;
        inputIndex = 0;
    }
}
