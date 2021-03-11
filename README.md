# ISA projekt - Filtrující DNS resolver

### Dominik Nejedlý (xnejed09)

### Popis

Program filtruje DNS dotazy typu A, které směřují na domény, které se nacházejí v dodaných souborech, 
a jejich poddomény. Na nežádoucí dotazy, případně při selhání zpracování dotazu, program pošle klientovi 
zprávu s odpovídajícím chybovým kódem. Ostatní dotazy jsou dále přeposílány specifikovanému DNS resolveru, 
jehož odpovědi jsou navráceny původním tazatelům.

### Překlad

Vytvoření spustitelného programu obstarává přítomný `Makefile`.

Příkazy:

- `make` - vytvoří spustitelný soubor a všechny objektové soubory
- `make clean` - smaže spustitelný soubor a všechny objektové soubory

### Spuštění

Program se spouští pomocí příkazu:

	./dns -s server [-p port] -f filter_file

Případně je možné použít i tento zápis:

	./dns -sserver [-pport] -ffilter_file

Oba způsoby se dají také kombinovat.

Vstupní parametry:

- `-s server` - DNS server, na který jsou přeposílány dotazy k rezoluci (povinný)
- `-p port` - Číslo portu určeného pro příjem příchozích dotazů (volitelný, výchozí hodnota je 53)
- `-f filter_file` - Soubor s nežádoucími doménami (povinný, je možné filtrovat i přes několik souborů)

Pro výpis nápovědy je možné použít parametr `-h` nebo `--help`.

### Seznam odevzdaných souborů

- `README`
- `manual.pdf`
- `Makefile`
- `dns.c`
- `dns.h`
- `charRList.c`
- `charPList.h`