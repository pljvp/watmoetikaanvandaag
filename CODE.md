# Weerdisplay kledingkast: code

Bijgewerkt op 3 november 2026. Menunamen van de Arduino IDE zijn in het Engels, zoals in de IDE zelf.

## Bestanden

De code bestaat uit negen bestanden in een map `weerdisplay` (geleverd als `weerdisplay.zip`). De Arduino-omgeving verwacht dat de map en het hoofdbestand dezelfde naam hebben.

| Bestand | Van wie | Wat het doet |
|---|---|---|
| `weerdisplay.ino` | code | hoofdprogramma: wifi, tijd, weer ophalen, scherm tekenen, slapen, knop, klokdata-logboek |
| `clock.h` | code | verversingsmomenten, slaaptijd met klokcorrectie, logboek van 7 dagen (alleen rekenwerk) |
| `weather.h` | code | weerlogica: dagdelen, kolomindeling, Beaufort, iconen, maanvorm, lezen van het antwoord |
| `ui.h` | code (jouw maten) | tekent het scherm, het infoscherm, alle iconen en de windpijlen |
| `net.h` | code | wifi (met onthouden kanaal), klok via internet, ophalen via HTTPS |
| `certs.h` | code | basiscertificaten voor de controle van de HTTPS-verbinding |
| `config.h` | **jij** | instellingen: locatie, verjaardagen, verversingsuren, knop, rotatie; maak het van `config.h.example` (staat niet in de repo) |
| `config.h.example` | voorbeeld | sjabloon voor `config.h` |
| `secrets.h` | **jij** | je wifi-gegevens; maak het van `secrets.h.example` en deel het nooit |
| `secrets.h.example` | voorbeeld | sjabloon voor `secrets.h` |

**Bij een update:** kopieer alles behalve `config.h` en `secrets.h` naar je schetsmap en overschrijf de bestaande. Verandert er een instelling in `config.h`, dan komen de regels die je moet toevoegen in de update-melding te staan. Sluit en open `weerdisplay.ino` daarna opnieuw, want de IDE laadt gewijzigde bestanden niet altijd opnieuw. Zoek met Ctrl+F op een nieuwe naam (bijvoorbeeld `REFRESH_H0`) om te zien of de nieuwe bestanden zijn geladen.

## Installatie op Ubuntu

1. **Arduino IDE 2** downloaden als AppImage van arduino.cc en uitvoerbaar maken (`chmod +x`, of het vinkje "uitvoeren als programma" in je bestandsbeheer). Start hem de eerste keer vanuit een terminal, dan zie je foutmeldingen:
   ```
   cd ~/Downloads
   ./arduino-ide*.AppImage
   ```
   - Melding `error loading libfuse.so.2`: installeer FUSE met `sudo apt install libfuse2` (op Ubuntu 24.04: `libfuse2t64`).
   - Melding over `chrome-sandbox`: start met `./arduino-ide*.AppImage --no-sandbox`.
2. **USB-toegang:** `sudo usermod -aG dialout $USER`, daarna uit- en weer inloggen (controleer met `groups`). Verschijnt het bord niet als poort, verwijder dan de braille-service: `sudo apt remove brltty`.
3. **ESP32-ondersteuning:** File → Preferences, bij "Additional boards manager URLs" de URL `https://espressif.github.io/arduino-esp32/package_esp32_index.json`. Installeer daarna "esp32 by Espressif Systems" via Boards Manager. Gebruikt en gecompileerd met versie 3.3.12.
4. **Bibliotheken** via Tools → Manage Libraries: GxEPD2, Adafruit GFX Library, U8g2_for_Adafruit_GFX en ArduinoJson (versie 7).
5. **Bord kiezen:** Tools → Board → esp32 → **ESP32 Dev Module**. (Ook "ESP32 Dev Board" bestaat in de lijst; die is niet getest, kies bij problemen "Dev Module".)
6. **Poort:** sluit het bord aan met een USB-datakabel en kies bij Tools → Port de poort. Bij ons: `/dev/ttyACM0`. Staat er geen poort, controleer dan met `ls /dev/ttyUSB* /dev/ttyACM*` of de poort verdwijnt als je het bord loskoppelt; zo niet, dan is het kabeltje alleen een laadkabel.
7. **Project:** pak `weerdisplay.zip` uit, kopieer `config.h.example` naar `config.h` (vul je locatie en verjaardagen in) en `secrets.h.example` naar `secrets.h` (vul je wifi in), en open `weerdisplay.ino`.

## Instellen

Alle instellingen staan in `config.h` en `secrets.h`.

| Instelling | Bestand | Waarde |
|---|---|---|
| Wifi | `secrets.h` | `WIFI_SSID` en `WIFI_PASS` tussen dubbele aanhalingstekens (met een `\` voor een `"` of `\` in je wachtwoord); alleen 2,4 GHz, een gastnetwerk wordt aangeraden |
| Locatie | `config.h` | `LATITUDE` en `LONGITUDE` in graden met een punt, op twee decimalen (Leiden: 52.16 en 4.49) |
| Verversing | `config.h` | `REFRESH_H0` tot `REFRESH_H3`: de uren, oplopend. Standaard 6, 11, 17, 23 |
| Infoscherm | `config.h` | `INFO_BUTTON_PIN` (39) en `INFO_SECONDS` (10) |
| Beeld draaien | `config.h` | `SCREEN_ROTATION` 1 of 3 |

Het scherm staat vast op DEPG0213BN (`GxEPD2_213_BN`) met de pinnen CS 5, DC 17, RST 16 en BUSY 4. Die pinnen zijn gecontroleerd in het LilyGO-schema.

### Uiterlijk aanpassen (ui.h)

| Wat | Waar in `ui.h` | Standaard |
|---|---|---|
| Grootte van het icoon | `UI_ICON` | 28 (de iconen zijn voor 28 pixels ontworpen, dat is het scherpst) |
| Hoogte van de neerslagregel (druppel, mm en %) | `UI_BASE_RAIN` (hoger getal is lager op het scherm) | 83 |
| Hoogte van de windregel | `UI_WIND_Y` | 99 |
| Ruimte tussen getal en eenheid (mm, %) | in `drawNumUnit` de `+ 5` | 5 |
| Ruimte tussen mm en het percentage | in de kolomtekening `x + 7` bij de neerslagregel | 7 |
| Ruimte voor het graden-teken | in `drawTemp` de `- 6` en `x + w + 3` | 1 pixel |

Bij "12 mm 100 %" (10 mm of meer en 100% kans) loopt de regel tot tegen de volgende kolom; dat is een bekende beperking van de smalle kolommen.

## Controleren of HTTPS werkt

Het bord vertrouwt alleen de certificaten in `certs.h`. Controleer eenmalig op je computer welke uitgever bovenaan de keten van Open-Meteo staat:

```
openssl s_client -showcerts -connect api.open-meteo.com:443 -servername api.open-meteo.com </dev/null 2>/dev/null | grep -E "^ *(s|i):"
```

De laatste regel met `i:` noemt de uitgever. Staat die naam (bijvoorbeeld ISRG Root X1) in de commentaarregels van `certs.h`, dan is het goed. Anders toont het scherm geen weer en moet dat basiscertificaat aan `certs.h` worden toegevoegd. Het scherm toont nu weer, dus het huidige certificaat werkt.

## Hoe de code werkt

### De cyclus

1. **Wakker worden.** Van een timer (verversing) of van de knop (infoscherm).
2. **Timer:** wifi aan, klok gelijkzetten via internet, klok meten en loggen, kijken of er een nieuw verversingsmoment is, weer ophalen (2 dagen), scherm tekenen, wifi uit, slapen tot 5 minuten voor het volgende verversingsmoment.
3. **Knop:** batterij meten, wifi aan en signaal meten, infoscherm tekenen, wachten op de knop (of 10 seconden), weer terugzetten, slapen.
4. **Fout:** het oude beeld blijft staan, een mislukte poging wordt onthouden (voor het infoscherm) en na 15 minuten (wifi of klok) of 30 minuten (weer) volgt een nieuwe poging.

Tussen de verversingen staat het bord in diepe slaap en gebruikt alleen de klok stroom. Het scherm houdt het beeld vast zonder stroom (en gaat in slaapstand, `hibernate`).

### Kolomindeling per verversing

| Verversing | Kolom 1 | Kolom 2 | Kolom 3 |
|---|---|---|---|
| 06:00 | ochtend | middag | avond |
| 11:00 | middag | avond | ochtend (morgen) |
| 17:00 | avond | ochtend (morgen) | middag (morgen) |
| 23:00 | ochtend (morgen) | middag (morgen) | avond (morgen) |

De indeling hangt af van het uur van de verversing (`layoutFor` in `weather.h`). Het bord wordt 5 minuten vóór het verversingsmoment wakker en ververst dan meteen; die 5 minuten worden bij de berekening meegenomen. De maanstand staat altijd in de avondkolom, ook als die op kolom 1 of 2 staat; om 23:00 is dat de avond van morgen.

### Berekening van een dagdeel

| Waarde | Berekening |
|---|---|
| Temperatuur | gemiddelde van de uurwaarden, afgerond op hele graden |
| Neerslag (mm) | som van de uren, afgerond op hele mm |
| Regenkans (%) | hoogste uurwaarde |
| Windkracht | gemiddelde snelheid in km/u, omgezet naar Beaufort (KNMI-tabel) |
| Windrichting | vectorgemiddelde, pijl wijst waarheen de wind waait |
| Icoon | het zwaarste weer van dat dagdeel (onweer > sneeuw > regen > mist > bewolkt > half bewolkt > helder); helder is zon of maan naargelang dag of nacht |
| Niveau regen of sneeuw | 1 tot 3 uit de grootste uurwaarde: tot 2,5 mm licht, tot 7,5 mm matig, daarboven zwaar |
| Maanstand | 8 standen uit de `moon_phase` van Open-Meteo; zonder antwoord een berekening op datum |

### Weerdata (Open-Meteo)

Eén aanvraag per verversing, over HTTPS, met alleen de velden die nodig zijn: `temperature_2m`, `precipitation_probability`, `precipitation`, `wind_speed_10m`, `wind_direction_10m`, `weather_code`, `is_day` (per uur) en `moon_phase` (per dag), voor 2 dagen, windsnelheid in km/u en de tijdzone Europe/Amsterdam.

### Klokcorrectie en slaap

De klok van het bord loopt tijdens de slaap af (ongeveer 5% volgens de gevonden bronnen; op jouw bord nog niet gemeten). Daarom:

- Bij elke synchronisatie vergelijkt het bord hoeveel echte tijd een slaap duurde met hoeveel slaap het vroeg. Die verhouding (de **ratio**) wordt onthouden en gemiddeld met de vorige meting. Alleen slaaptijden vanaf 30 minuten en waarden tussen 0,8 en 1,25 tellen mee.
- De volgende slaaptijd wordt gedeeld door die ratio, zodat het bord op tijd wakker wordt.
- **Voordat er een meting is** slaapt het bord eerst 90% van de tijd en meet daarna. De eerste verversing na de eerste nacht kan daardoor een paar minuten verschuiven.
- Wakker worden gebeurt 5 minuten vóór het verversingsmoment (`CLOCK_EARLY_S` in `clock.h`). Binnen dat venster ververst het bord meteen.
- Er is geen tussenstap meer: het bord synchroniseert alleen bij een verversing (4 wifi-sessies per dag).

Na de knop wordt de slaaptijd berekend op de klok van het bord zelf (niet nieuw gesynchroniseerd), dus die slaap draagt niet bij aan de meting.

### Klokdata-logboek

Bij elke synchronisatie komt er een regel in het flashgeheugen (dus ook na stroomuitval). Er blijft 7 dagen bewaard: vandaag plus de 6 dagen ervoor. Zodra dag 8 begint, verdwijnt dag 1. Er passen maximaal 64 regels in.

Lezen: sluit het bord met USB aan en open Tools → Serial Monitor op 115200 (dit herstart het bord meestal), of druk op de resetknop. Het logboek verschijnt na enkele seconden. Een regel bevat:

| Kolom | Betekenis |
|---|---|
| datum, tijd | het moment van de synchronisatie |
| gevraagd | de slaap die het bord vroeg, in seconden |
| echt | de echt verstreken tijd, in seconden |
| ratio | echt gedeeld door gevraagd (1,0000 is een klok die precies loopt) |
| klok_vooraf | seconden dat de klok van het bord afweek vlak voor het gelijkzetten (+ is te vroeg) |

### Infoscherm

Toont huidige tijd, laatste verversing (bij een mislukte poging staat er "mislukt" achter), volgende verversing, de batterij en het wifi-signaal. De batterij komt uit de spanning op GPIO 35 (de helft van de echte spanning): 3,3 V is 0%, 4,2 V is 100%, afgerond op 10%. Dat is een grove schatting. Het wifi-signaal wordt live gemeten: het bord verbindt eerst en dat duurt meestal 5 tot 10 seconden, bij een mislukte verbinding maximaal ongeveer 20 seconden. De staafjes: 4 boven -55 dBm, 3 boven -65, 2 boven -75, 1 boven -85, anders geen. De tijd op het scherm komt van de klok van het bord; staat die nog niet goed, dan staat er `--:--`.

### Wifi snel verbinden

Het bord onthoudt het kanaal en het adres van je router (in de slaap). De volgende keer verbindt het daarmee direct, zonder te zoeken. Lukt dat niet binnen 8 seconden, dan zoekt het gewoon opnieuw.

## Testen en beperkingen

De rekenlogica is op een computer getest, de ESP32-onderdelen niet. Met `testomgeving.zip` (zie `LEESMIJ.txt`) draait de tekencode op een gewone computer en schrijft een beeld van 250 x 122 pixels.

| Onderdeel | Status |
|---|---|
| Schermindeling, iconen, lettertypen | getest op een computer met dezelfde tekencode en lettertypen, als beeld van 250 x 122 pixels |
| Weerlogica | getest: Beaufort-grenzen, maanvorm, iconen, niveaus, lezen van een voorbeeldantwoord (ook zonder maanstand en met lege waarden) |
| Kolomindeling per uur | getest op een computer, ook rond middernacht en bij de overgang naar wintertijd |
| Verversingsmomenten, klokcorrectie, logboek (7 dagen) | getest op een computer |
| Compileren voor de ESP32 | gelukt voor de eerste versies (ongeveer 85% van het programmageheugen); de nieuwste wijzigingen (klokcorrectie, logboek) nog te bevestigen |
| Eerste versie op het bord | werkt: wifi, HTTPS, scherm, weer |
| Infoscherm met knop, batterij- en wifimeting | nog niet bevestigd op het bord |
| Wakker worden met de timer en klokcorrectie | nog niet bevestigd; wacht op de eerste dagen met het logboek |
| Verversing om 06:00, 11:00, 17:00 en 23:00 | nog niet bevestigd |
| Echt antwoord van Open-Meteo | werkt (scherm toont weer); de testbestanden zijn zelf gemaakt volgens de documentatie |
| Batterijduur | niet gemeten |

Aannames die nog moeten blijken: de systeemtijd van het bord blijft behouden tijdens diepe slaap (anders staat in het logboek `klok_vooraf` op 0 of onzin), en de knop (GPIO 39) wekt het bord goed uit slaap.

## Problemen oplossen

| Probleem | Wat te doen |
|---|---|
| `libfuse.so.2` ontbreekt bij het starten | `sudo apt install libfuse2` (of `libfuse2t64`) |
| Arduino IDE start niet (sandbox) | start met `--no-sandbox` |
| Geen poort in Tools → Port | USB-datakabel gebruiken, `brltty` verwijderen, `dialout`-groep en opnieuw inloggen |
| Bord toont niets of oud beeld | batterij of USB aansluiten; controleer de schuifschakelaar; kijk in de seriële monitor naar het logboek |
| Compileerfout over `REFRESH_H0`, `REFRESH_H1` of `INFO_BUTTON_PIN` | `config.h` is nog niet vervangen door de nieuwste |
| Upload blijft op "Connecting......" hangen | druk niet blind op knoppen; het bord start vanzelf in uploadmodus. Probeer een andere kabel of poort |
| Geen nieuw weer | controleer het certificaat (opdracht hierboven), de wifi (2,4 GHz) en het infoscherm voor "geen verbinding" of "mislukt" |
| Bij een foutmelding of raar beeld | noteer de melding en bij welke stap het mislukt |
