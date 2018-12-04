#include <LinkedList.h>
#include <Arduino.h>

// Pins
const int KNOPPEN[] = {2, 3, 4, 5}; // Dit zijn de pinnen die verbonden zijn met de KNOPPEN
const int LEDS[] = {6, 7, 8, 9};    // Dit zijn de pinnen die verbonden zijn met de LEDS
const int BUZZER_PIN = 10;          // Buzzer pin, geeft speler feedback tijdens het spel
const int DATA_PIN = 8;             // De pin waarover de data verstuurd zal worden
const int RGB_PIN[] = {10, 11, 12}; // De drie outputs verbonden met de RBG status-led

// Constants
const int SLEEP_TIME = 50;         // De tijd die een controller wacht om input te lezen
const int WAIT_TIME = 50;          // De tijd die een controller wacht om data te beginnen lezen/sturen
const int PLAYER_WAIT_TIME = 2000; // De maximale tijd die een speler heeft om een "move" te doen (in ms)
const int DISPLAY_TIME = 300;      // De tijd waarvoor we de combinatie-eenheden aan de gebruiker tonen

const String SYNC_STRING = "101";   // De string die gebruikt wordt om de twee controllers te "syncen"
const String LOST_STATUS = "11101"; // De string die gebruikt wordt om aan te geven dat deze speler verloren is

// Spelvariabelen
bool mijnBeurt = true; // Een van de twee spelers moet beginnen, de speler die tijdens het opstarten knop 1 ingedrukt heeft, begint
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
/// Toont de huidige combinatie aan de speler voor hij aan zijn beurt begint.
///
void toonCombinatie()
{
    Serial.println("Toon combinatie...");
    for (int i = 0; i < memory.size(); i++) // Toon de sequentie
    {
        digitalWrite(LEDS[memory.get(i)], HIGH);
        delay(DISPLAY_TIME);
        digitalWrite(LEDS[memory.get(i)], LOW);
        delay(DISPLAY_TIME / 2);
    }

    flash(3); // Flash de status-led drie keer
}

///
/// Toont iets aan de speler zodat hij weet dat het niet zijn beurt is
/// TODO: WORK IN PROGRESS
/// Misschien een animatie van de leds die ronddraaien?
///
void toonWacht()
{
}

void setup() // Loopt alleen bij start
{
    memory.add(0); // Initialiseer het spel met een beginwaarde (Dit wordt uiteindelijk de waarde van de ingedrukte startknop)

    // Initialiseer de KNOPPEN en LEDS
    for (int i = 0; i < 4; i++)
    {
        pinMode(KNOPPEN[i], INPUT);
        if (digitalRead(KNOPPEN[i]) == HIGH)
        {
            mijnBeurt = true;
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

    // Begin de seriële comunicatie
    Serial.begin(9600);
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
            if (digitalRead(KNOPPEN[i]) == HIGH)
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
        if (digitalRead(KNOPPEN[i]) == HIGH)
        {
            if (knop != -1) // Er zijn meerdere knoppen ingedrukt
                return -1;
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
    Serial.println("Je bent verloren!");
    // Reset de variabelen
    gameBusy = false;
    lost = true;
    setColor(255, 0, 0);
    mijnBeurt = false;

    // Verstuur het verlies over de datalijn
    // SendLostStatus();
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
            /*Serial.println("Status: " + String(digitalRead(KNOPPEN[inputIndex - 1])));
            Serial.println("i: " + String(inputIndex));
            Serial.println("Button: " + String(KNOPPEN[memory.get(inputIndex - 1)]));*/
            if (buttonDown and inputIndex != 0) // Als er een knop ingedrukt is en het is niet de eerste input van een sequentie (om dubbele detectie te voorkomen)
            {
                if (digitalRead(KNOPPEN[memory.get(inputIndex - 1)]) == LOW) // Als de vorige correcte knop niet meer ingedrukt is, kunnen we verdergaan
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
                    //toonWacht();                // Toon de speler dat het de beurt van de andere speler is en wacht
                    //mijnBeurt = false;
                    inputIndex = 0; // Reset de index
                    //sendCombinatie(knop); // verstuur de nieuwe knop over de data lijn
                    break;
                }
            }

            //Serial.println("Huidige tijd: " + String(millis() - inputTime));
            if (lost or (millis() - inputTime > PLAYER_WAIT_TIME))
            {
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
    /*else
    {
        inSync = false;

        while
            !inSync
            { // Herhaal dit zolang we niet in sync zijn
                while
                    digitalRead(DATA_PIN) == LOW // Wacht tot de pin hoog gestuurd wordt
                    {
                        delay(SLEEP_TIME);
                    }

                while
                    digitalRead(DATA_PIN) == HIGH // Wacht tot de pin terug laag is om te syncen
                    {
                        delay(1);
                    }

                delay(WAIT_TIME); // Als er HOOG gestuurd wordt, wacht dat tot het terug laag is, en wacht daarna een vaste tijd
                if
                    readSyncSequence(DATA_PIN) // Dit checkt of de data die aangekomen is, de "Sync sequentie" is, indien ja, zijn we in sync
                    {
                        respondOK(DATA_PIN); // Antwoord OK, de data is in sync
                        inSync = true;
                    }
                else
                {
                    respondNOTOK(DATA_PIN); // Antwoord NOT OK en probeer opnieuw te syncen
                }
            }

        // We zijn in sync en klaar om de nieuwe memory-eenheid te ontvangen
        delay(WAIT_TIME);
        int mem = readMemorySequence(DATA_PIN); // lees de nieuwe memory-eenheid
        if
            mem == -1 // we hebben de bit niet juist doorgekregen
            {
                respondNOTOK(DATA_PIN); // Antwoord NOT OK en probeer opnieuw te syncen
                // TODO: Opnieuw syncen
            }
        else
        {
            memory.add(mem); // voeg deze toe aan de gekende memory volgorde
        }
        mijnBeurt = true;
    }*/
}
