# Orthodox Reminder Pro v3

Moderan Windows desktop posetnik u C++ sa pravoslavnim kalendarom, praznicima i podsetnicima.

## Glavne funkcije

- kalendar sa izborom datuma
- prikaz velikih pravoslavnih praznika za izabrani datum
- podsetnici po datumima
- ponavljanja: jednom, dnevno, nedeljno, mesečno, godišnje
- označavanje obaveze kao završene dvoklikom u listi
- pretraga kroz sve podsetnike
- pregled predstojećih obaveza za narednih 90 dana
- godišnji pregled praznika
- izvoz dnevnog izveštaja u TXT
- izvoz godišnjeg kalendara u TXT
- izvoz predstojećih obaveza u CSV
- backup i restore podataka
- start obaveštenje sa današnjim praznicima i podsetnicima
- lokalno čuvanje podataka bez dodatne baze

## Gde se čuvaju podaci

Aplikacija čuva podatke ovde:

`%LOCALAPPDATA%\OrthodoxReminderPro\reminders_v3.txt`

Backup dugme pravi kopiju na Desktop-u:

`orthodox_reminder_backup.txt`

## Pokretanje gotovog programa

Ako već imaš `OrthodoxReminderPro.exe`, samo ga pokreneš. Nije potrebna dodatna instalacija aplikacije.

## Kako da dobiješ EXE preko GitHub-a

1. napravi novi GitHub repo
2. ubaci sadržaj ovog projekta
3. otvori tab **Actions**
4. pokreni workflow **Build Windows EXE**
5. kad build završi, preuzmi artifact **OrthodoxReminderPro-v3-windows**
6. raspakuj i pokreni `OrthodoxReminderPro.exe`

## Lokalni build

Ako želiš lokalno da kompajliraš:

```bash
cmake -S . -B build
cmake --build build --config Release
```

## Napomena

Aplikacija je pravljena kao čisti WinAPI/C++ projekat da gotov `.exe` može lako da radi na Windows-u bez dodatnih runtime zahteva van standardnog sistema i načina build-a.
