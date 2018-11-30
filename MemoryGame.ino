#include <Arduino.h>
#include <LinkedList.h>

// Pins
const int KNOPPEN[] = {2, 3, 4, 5}; // Dit zijn de pinnen die verbonden zijn met de KNOPPEN
const int LEDS[] = {6, 7, 8, 9};    // Dit zijn de pinnen die verbonden zijn met de LEDS
const int BUZZER_PIN = 10;          // Buzzer pin, geeft speler feedback tijdens het spel
const int DATA_PIN = 8;             // De pin waarover de data verstuurd zal worden

// Constants
const int SLEEP_TIME = 50;         // De tijd die een controller wacht om input te lezen
const int WAIT_TIME = 50;          // De tijd die een controller wacht om data te beginnen lezen/sturen
const int PLAYER_WAIT_TIME = 2000; // De maximale tijd die een speler heeft om een "move" te doen (in ms)

// Spelvariabelen
bool mijnBeurt = false; // Een van de twee spelers moet beginnen, de speler die tijdens het opstarten knop 1 ingedrukt heeft, begint
bool gameBusy = false;  // Dit is "true" als de speler bezig is met het memory spel
bool lost = false;      // Dit is "true" als de speler het spel verloren heeft

long inputTime = 0; // Houdt bij hoelang de speler over een "move" doet
int inputIndex = 0; // Houdt bij aan welke knop de speler zit in het memory spel

LinkedList<int>
    memory = new LinkedList<int>(); // Houdt de huidige combinatie van knoppen bij

void setup() // Loopt alleen bij start
{
    // Initialiseer de KNOPPEN en LEDS
    for (int i = 0; i < 4; i++)
    {
        pinMode(KNOPPEN[i], INPUT);
        if
            digitalRead(KNOPPEN[i]) == HIGH
            {
                mijnBeurt = true;
            }
    }

    for (int i = 0; i < 4; i++)
    {
        pinMode(LEDS[i], OUTPUT);
    }

    beep(50);
}

void loop()
{
    // Loopt telkens opnieuw

    if (mijnBeurt)            // Speel het spel en stuur het resultaat over de POF
    {                         // Als de speler de combinatie juist heeft, dat sturen we de nieuwe knop door
                              // Als de speler een fout maakt sturen we door dat hij/zij verloren is
        beep(50);             // Maak een geluid zodat de speler weet dat hij/zij aan de beurt is
        gameBusy = true;      // Het spel begint
        inputTime = millis(); // Stokeer de huidige tijd, zodat we weten wanneer de speler te traag is
        toonCombinatie();     // Toon de combinatie tot nu toe en geef de speler signaal dat hij mag proberen
        while
            gameBusy
            {
                status = checkMemory(); // Checkt of de speler het juist heeft of niet
                if
                    status == -1 lost = true; // De speler gaf de foute combinatie in
                else if
                    status == 1 // De speler gaf de juist combinatie in
                    {
                        inputIndex++;         // Update de huidige inputIndex
                        inputTime = millis(); // Reset de input timer

                        if
                            inputIndex == memory.size() // Als de speler de volledige sequentie doorlopen heeft, wint hij
                            {
                                beep(50); // Toon de speler dat hij gewonnen is met een geluidssignaal
                                flash(3);
                                int knop = addCombinatie(); // Laat de speler een nieuwe knop toevoegen aan de combinatie (en return deze nieuwe knop)
                                toonWacht();                // Toon de speler dat het de beurt van de andere speler is en wacht
                                mijnBeurt = false;
                                inputIndex = 0;       // Reset de index
                                sendCombinatie(knop); // verstuur de nieuwe knop over de data lijn
                                break;
                            }
                    }

                if (lost or millis() - inputTime > PLAYER_WAIT_TIME)
                {
                    gameBusy = false;
                    lost = true;
                    break;
                }
            }

        if
            lost
            {
                spelerVerliest() // Vertel beide spelers dat deze persoon verloren is
                                 // beëindig het spel en toon score eventueel
            }
    }
    else
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
checkMemory()
{
    int knop = -1;              // De knop die op dit moment ingedrukt wordt
                                // -1 ==> geen knop ingedrukt
    for (int i = 0; i < 4; i++) // Check welke knop(pen) ingedrukt is (zijn)
    {
        if
            digitalRead(KNOPPEN[i]) == HIGH
            {
                if (knop != -1) // Er zijn meerdere knoppen ingedrukt
                    return -1;
                knop = i;
            }
    }

    if
        knop == -1 return 0;
    if
        knop == memory.get(inputIndex) return 1;
    else
        return -1;
}

///
/// Voegt een nieuwe knop toe aan de combinatie
/// Als de speler meerdere knoppen tegelijk indrukt geeft het een foutsignaal en probeert opnieuw
///
addCombinatie()
{
    bool done = false; // Is de nieuwe knop toegevoegd?
    while
        !done
        {
            int knop = -1;              // De knop die op dit moment ingedrukt wordt
                                        // -1 ==> geen knop ingedrukt
            for (int i = 0; i < 4; i++) // Check welke knop(pen) ingedrukt is (zijn)
            {
                if
                    digitalRead(KNOPPEN[i]) == HIGH
                    {
                        if (knop != -1) // Er zijn meerdere knoppen ingedrukt
                        {
                            beep(200);  // Geef foutsignaal (audio)
                            flash(1);   // Geef foutsignaal (visueel)
                            delay(200); // Wait a bit
                            flash(1);   // Geef foutsignaal (visueel) opnieuw
                            knop = -1;  // reset de knop
                            break;
                        }
                        knop = i;
                    }
            }
            if
                knop != -1
                {
                    combinatie.add(knop);
                    done = true;
                    return knop
                }
        }
}

///
/// Toont de huidige combinatie aan de speler voor hij aan zijn beurt begint.
///
toonCombinatie()
{
    for (int i = 0; i < memory.size(); i++) // toon de sequentie
    {
        digitalWrite(LEDS[memory.get(i)], HIGH);
        delay(DISPLAY_TIME);
        digitalWrite(LEDS[memory.get(i)], LOW);
        delay(DISPLAY_TIME / 2);
    }

    flash(3); // Flash alle leds drie keer
}

///
/// Gives a beep for <duration> ms
///
beep(int duration)
{
    analogWrite(BUZZER_PIN, 2);
    delay(duration);
    analogWrite(BUZZER_PIN, 0);
}

///
/// Toont iets aan de speler zodat hij weet dat het niet zijn beurt is
/// TODO: WORK IN PROGRESS
/// Misschien een animatie van de leds die ronddraaien?
///
toonWacht()
{
    // TODO: WIP
}

///
/// Flasht alle leds <aantal> keer
///
flash(int aantal)
{
    for (i = 0; i < aantal : i++) // flash LED0 drie keer om te tonen dat de speler mag proberen
    {
        digitalWrite(LEDS[0], HIGH);
        delay(200);
        digitalWrite(LEDS[0], LOW);
        delay(100);
    }
}
