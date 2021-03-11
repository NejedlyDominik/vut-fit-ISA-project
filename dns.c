/**
 * ISA - projekt
 * 
 * Dominik Nejedly (xnejed09), 2020
 * 
 * Filtrujici DNS program
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "dns.h"

// Ridici promenna, ktera zastavi cyklus v hlavnim tele programu pri preruseni.
volatile bool cont = true;

// obsluha pro signal preruseni
void sigIntHandler()
{
    cont = false;   // ukonci nekonecny cyklus v hlavnim tele programu
}

int main(int argc, char *argv[])
{
    int state;                          // pomocna promenna pro navratove hodnoty volanych funkci
    int csSock;                         // soket pro komunikaci klienta a tohoto programu (serveru)
    int srSock;                         // soket pro komunikaci tohoto programu (serveru) a DNS resolveru
    int msgSize;
    int queryLength;                    // delka dotazu klienta od zacatku paketu po konec prvni otazky
    int port = 53;
    char buffer[BUFFER_SIZE];           // pole pro prijimany a odesilany DNS paket
    socklen_t clientLength;
    struct sockaddr_in6 client;         // struktura adresy klienta
    struct sockaddr_in6 server;         // struktura adresy tohoto programu (serveru)
    struct addrinfo *resolver = NULL;   // struntura adresy specifikovaneho DNS resolveru
    charPListT filterFiles;             // list uchovavajici jmena vsech souboru se zakazanymi domenovymi jmeny
    sigset_t noBlockedSignals;          // aktualni signalova maska
    sigset_t blockedSignals;            // signalova maska blokujici signal preruseni
    fd_set readFds;                     // deskriptory pripravene ke cteni
    struct timespec timeOut;            // struktura specifikujici casovy limit

    // nastaveni obsluhy signalu preruseni
    if(signal(SIGINT, sigIntHandler) == SIG_ERR)
    {
        fprintf(stderr, "INTERNAL_ERROR: Handler setting for interrupt signal failed.\n");
        return INTERNAL_ERR;
    }

    // pripraveni signalove masky blokujici signal preruseni
    if(sigemptyset(&blockedSignals) == -1 || sigaddset(&blockedSignals, SIGINT) == -1)
    {
        fprintf(stderr, "INTERNAL_ERROR: Mask setting for blocking of interrupt signal failed.\n");
        return INTERNAL_ERR;
    }

    init(&filterFiles);

    // nacteni a zpracovani vstupnich argumentu
    if((state = parseArgs(argc, argv, &resolver, &port, &filterFiles)) == PARAM_ERR)
    {
        fprintf(stderr, "USAGE: ./dns -s server [-p port] [-f filter_file]\n\n");
        fprintf(stderr, "For more detailed description use \"./dns -h\" or \"./dns --help\" etc.\n");
        return PARAM_ERR;
    }
    else if(state == HELP)  // Pokud byla tisknuta napoveda, program uspesne skonci.
    {
        freeaddrinfo(resolver);
        clear(&filterFiles);
        return SUCCESS;
    }
    else if(state == INTERNAL_ERR)
    {
        freeaddrinfo(resolver);
        fprintf(stderr, "INTERNAL_ERROR: Memory allocation failed.\n");
        return INTERNAL_ERR;
    }
    
    memset(&server, 0, sizeof(server));

    server.sin6_family = AF_INET6;      // nastaveni IPv6 adresovani (zpracuje i IPv4)
    server.sin6_port = htons(port);     // nastaveni portu poslechu
    server.sin6_addr = in6addr_any;     // Server komunikuje s kazdym klientem.

    // vytvoreni UDP soketu pro komunikaci klienta a tohoto serveru
    if((csSock = socket(AF_INET6, SOCK_DGRAM, 0)) == -1)
    {
        clear(&filterFiles);
        freeaddrinfo(resolver);
        fprintf(stderr, "CLIENT_SERVER_SOCKET_ERROR: Server socket for incomming queries failed to create.\n");
        return CS_SOCK_ERR;
    }

    // navazani soketu pro komunikaci klienta a tohoto serveru na port
    if(bind(csSock, (struct sockaddr *)&server, sizeof(server)) == -1)
    {
        clear(&filterFiles);
        freeaddrinfo(resolver);
        close(csSock);
        fprintf(stderr, "CLIENT_SERVER_SOCKET_BIND_ERROR: Server socket for incomming queries failed to bind.\n");
        return CS_SOCK_BIND_ERR;
    }

    // pruchod pres adresy specifikovaneho DNS resolveru a pokus se k nejake pripojit
    for(struct addrinfo *nameServer = resolver; nameServer != NULL; nameServer = nameServer->ai_next)
    {
        if((state = connectToResolver(nameServer, &srSock)) == SUCCESS)
        {
            break;
        }
    }

    freeaddrinfo(resolver);     // Tuto strukturu jiz neni nutne dale uchovavat.

    if(state != SUCCESS)
    {
        close(csSock);
        clear(&filterFiles);

        if(state == SR_SOCK_ERR)
        {
            fprintf(stderr, "SERVER_RESOLVER_SOCKET_ERROR: Server socket for communication with resolver failed to create.\n");
        }
        else
        {
            fprintf(stderr, "SERVER_RESOLVER_SOCKET_CONNECT_ERROR: Server socket for communication with resolver failed to connect.\n");
        }

        return state;
    }

    clientLength = sizeof(client);

    // nastaveni blokovani signalu preruseni
    if(sigprocmask(SIG_BLOCK, &blockedSignals, &noBlockedSignals) == -1)
    {
        close(srSock);
        close(csSock);
        clear(&filterFiles);
        fprintf(stderr, "INTERNAL_ERROR: Blocking of interrupt signal failed.\n");
        return INTERNAL_ERR;
    }

    timeOut.tv_sec = 5;
    timeOut.tv_nsec = 0;

    // cyklus rizeny promenou reagujici na signal preruseni
    while(cont) 
    {
        // nastaveni sledovaneho deskriptoru na soket pro komunikaci klienta a serveru
        FD_ZERO(&readFds);
        FD_SET(csSock, &readFds);

        // sledovani nastaveneho soketu a odblokovani signalu preruseni pouze behem teto cinnosti (prevence behove chyby)
        if(pselect(csSock + 1, &readFds, NULL, NULL, &timeOut, &noBlockedSignals) <= 0)
        {
            continue;
        }
        
        // prijmuti zpravy od klienta
        if((msgSize = recvfrom(csSock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client, &clientLength)) >= 0)
        {
            state = checkPacket(buffer, &filterFiles, &queryLength);

            if(buffer[4] != 0 || buffer[5] != 1)
            {
                queryLength = msgSize;
            }

            if(state != SUCCESS)
            {
                setErrorResponse(buffer, state);

                if(replyToClient(csSock, srSock, buffer, queryLength, &client, clientLength, &filterFiles) != SUCCESS)
                {
                    return SEND_TO_CLIENT_ERR;
                }

                if(state == DNSP_SERVER_FAIL)
                {
                    close(srSock);
                    close(csSock);
                    clear(&filterFiles);
                    return INTERNAL_ERR;
                }

                continue;
            }

            // preposlani dotazu od klienta specifikovanemu DNS resolveru
            if(send(srSock, buffer, msgSize, 0) == -1)
            {
                setErrorResponse(buffer, DNSP_SERVER_FAIL);

                if(replyToClient(csSock, srSock, buffer, queryLength, &client, clientLength, &filterFiles) != SUCCESS)
                {
                    return SEND_TO_CLIENT_ERR;
                }
            }

            // nastaveni sledovaneho desktriptoru na soket pro komunkaci serveru a DNS resolveru
            FD_ZERO(&readFds);
            FD_SET(srSock, &readFds);

            if(pselect(srSock + 1, &readFds, NULL, NULL, &timeOut, &noBlockedSignals) <= 0)
            {
                setErrorResponse(buffer, DNSP_SERVER_FAIL);

                if(replyToClient(csSock, srSock, buffer, queryLength, &client, clientLength, &filterFiles) != SUCCESS)
                {
                    return SEND_TO_CLIENT_ERR;
                }

                continue;
            }

            // prijmuti odpovedi od DNS resolveru
            if((msgSize = recv(srSock, buffer, BUFFER_SIZE, 0)) == -1)
            {
                setErrorResponse(buffer, DNSP_SERVER_FAIL);

                if(replyToClient(csSock, srSock, buffer, queryLength, &client, clientLength, &filterFiles) != SUCCESS)
                {
                    return SEND_TO_CLIENT_ERR;
                }
            }
        
            // preposlani odpovedi klientovi
            if(replyToClient(csSock, srSock, buffer, msgSize, &client, clientLength, &filterFiles) != SUCCESS)
            {
                return SEND_TO_CLIENT_ERR;
            }
        }
    }

    close(srSock);
    close(csSock);
    clear(&filterFiles);

    if(!cont)
    {
        // Pokud bylo zaznamenano preruseni, je tomuto signalu nastavena vychozi obsluha.
        if(signal(SIGINT, SIG_DFL) == SIG_ERR)
        {
            fprintf(stderr, "INTERNAL_ERROR: Default handler setting for interrupt signal failed.\n");
            return INTERNAL_ERR;
        }

        // Signal preruseni je odblokovan.
        if(sigprocmask(SIG_SETMASK, &noBlockedSignals, NULL) == -1)
        {
            fprintf(stderr, "INTERNAL_ERROR: Unblocking of interrupt signal failed.\n");
            return INTERNAL_ERR;
        }

        // Program je ukoncen prerusenim.
        if(kill(getpid(), SIGINT) == -1)
        {
            fprintf(stderr, "INTERNAL_ERROR: Interrupt signal failed to raise.\n");
            return INTERNAL_ERR;
        }
    }

    return SUCCESS;
}


/**
 * nacteni a zpracovani vstupnich argumentu
 */
int parseArgs(int argc, char *argv[], struct addrinfo **resolver, int *port, charPListT *filterFiles)
{
    struct addrinfo hints;              // struktura specifikujici kriteria vyberu adresy DNS resolveru
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;        // nastaveni vracene adresove rodiny (v tomto pripade IPv4 i IPv6)
    hints.ai_socktype = SOCK_DGRAM;     // preferovany typ soketu
    char *argPtr;                       // ukazatel a argument
    char *endPtr;                       // pomocny ukazatel

    // pruchod pres vsechny agrumenty
    for(int i = 1; i < argc; i++)
    {
        argPtr = argv[i];

        // Pokud prepinac zacina na "-h" nebo je roven "--help", je tisknuta napoveda.
        if(strncmp(argPtr, "-h", 2) == 0 || strcmp(argPtr, "--help") == 0)
        {
            printf("DESCRIPTION: DNS program that filters queries of type A targeting to domains in filter files and their subdomains.\n");
            printf("USAGE: ./dns -s server [-p port] -f filter_file\n");
            printf("OPTIONS:\n");
            printf("\t-s server     \tIP address or domain name of DNS server (resolver), where the query should be send.\n");
            printf("\t-p port       \tPort number, where the queries will be expected by the program. Default port number is 53.\n");
            printf("\t-f filter_file\tFile containing undesirable domains. IT is possible to specify more than one filter file.\n\n");
            printf("The order of the parameters is arbitrary.\n");
            return HELP;
        }
        else if(strncmp(argPtr, "-s", 2) == 0)
        {
            // Ukazatel na argument je nastaven tak, aby ukazoval na retezec ke zpracovani.
            if(strlen(argPtr) > 2)
            {
                // Pokud je argument delsi nez prepinac, je ukazatel posunut tak, aby ukazoval za nej.
                argPtr += 2;
            }
            else    // Jinak se posune na dalsi argument.
            {
                if(++i >= argc)
                {
                    fprintf(stderr, "PARAM_ERROR: Missing IP address or domain name of DNS server after option \"-s\".\n");
                    return PARAM_ERR;
                }

                argPtr = argv[i];
            }

            // ziskani adres specifikovaneho resolveru
            if(getaddrinfo(argPtr, DNS_SERVER_PORT, &hints, resolver) != 0)
            {
                fprintf(stderr, "PARAM_ERROR: Invalid IP address or domain name of DNS server: %s\n", argPtr);
                return PARAM_ERR;
            }
        }
        else if(strncmp(argPtr, "-p", 2) == 0)
        {
            if(strlen(argPtr) > 2)
            {
                argPtr += 2;
            }
            else
            {
                if(++i >= argc)
                {
                    fprintf(stderr, "PARAM_ERROR: Missing port number after option \"-p\".\n");
                    return PARAM_ERR;
                }

                argPtr = argv[i];
            }
            
            // ziskani cisla portu z argumentu
            *port = strtol(argPtr, &endPtr, 10);

            // kontrola validity zadane hodnoty portu
            if((strcmp(endPtr, "") != 0) || *port < 1 || *port > MAX_PORT_NUM)
            {
                fprintf(stderr, "PARAM_ERROR: Invalid port number: %s\n", argPtr);
                return PARAM_ERR;
            }
        }
        else if(strncmp(argPtr, "-f", 2) == 0)
        {
            if(strlen(argPtr) > 2)
            {
                argPtr += 2;
            }
            else
            {
                if(++i >= argc)
                {
                    fprintf(stderr, "PARAM_ERROR: Missing name of filter file after option \"-f\".\n");
                    return PARAM_ERR;
                }

                argPtr = argv[i];
            }

            // pridani zaznamu do seznamu jmen filtrovacich souboru
            if(push(filterFiles, argPtr) == -1)
            {
                clear(filterFiles);
                return INTERNAL_ERR;
            }
        }
        else if(strncmp(argPtr, "-", 1) == 0)   // neznamy prepinac
        {
            fprintf(stderr, "PARAM_ERROR: Invalid option: %s\n", argPtr);
            return PARAM_ERR;
        }
    }

    // Neni specifikovany DNS resolver.
    if(*resolver == NULL)
    {
        fprintf(stderr, "PARAM_ERROR: Missing IP address or domain name of DNS server.\n");
        return PARAM_ERR;
    }

    if(isEmpty(filterFiles))
    {
        fprintf(stderr, "PARAM_ERROR: Missing filter file.\n");
        return PARAM_ERR;
    }

    return SUCCESS;
}


/**
 * vytvoreni soketu a jeho pripojeni ke specifikovanemu DNS resolveru
 */
int connectToResolver(struct addrinfo *nameServer, int *srSock)
{
    // vytvoreni UDP soketu pro komunikaci tohoto serveru a DNS resolveru
    if((*srSock = socket(nameServer->ai_family, SOCK_DGRAM, 0)) == -1)
    {
        return SR_SOCK_ERR;
    }

    // nastaveni spojovane UDP schranky pro komunikaci tohoto serveru a DNS resolveru
    if(connect(*srSock, nameServer->ai_addr, nameServer->ai_addrlen) == -1)
    {
        close(*srSock);
        return SR_SOCK_CONNECT_ERR;
    }

    return SUCCESS;
}


/**
 * kontrola struktury prijate zpravy od klienta a dotazovaneho domenoveho jmena
 */
int checkPacket(char *packet, charPListT *filterFiles, int *queryLength)
{
    char domainName[MAX_DN_BUFFER_SIZE];                    // pole pro uchovani domenoveho jmena z prijateho paketu
    int i = getDomainName(domainName, &packet[12]) + 12;    // promenna uchovavajici index smerujici za domenove jmeno v paketu

    *queryLength = i + 4;   // delka dotazu klienta od zacatku paketu po konec prvni otazky

    // Kontrola QR a Z bitu ve FLAGS v hlavicce paketu, tedy zdali se jedna o dotaz a Z bit neni nastaveny na 1.
    if((packet[2] & QR_MASK) != 0 || (packet[3] & Z_MASK) != 0)
    {
        return DNSP_INV_FORMAT;
    }
    
    // kontrola poctu otazek v paketu (podporovana pouze 1)
    if(packet[4] != 0 || packet[5] != 1)
    {
        return DNSP_NOT_IMPLEMENTED;
    }

    // kontrola typu dotazu (podporovany pouze typ A)
    if(packet[i++] != 0 || packet[i] != 1)
    {
        return DNSP_NOT_IMPLEMENTED;
    }

    // Zjisti, zdali domenove jmeno neni filtrovane.
    return checkDomainName(domainName, filterFiles);
}


/**
 * Ziska domenove jmeno z paketu dotazu.
 */
int getDomainName(char *domainName, char *firstLabel)
{
    int i;                                          // aktualni delka domenoveho jmena
    int j = 0;                                      // pomocna promenna pro prepis retezce znacky do pole domenoveho jmena
    int length = *firstLabel & LABEL_LENGTH_MASK;   // delka retezce znacky

    firstLabel++;   // posun na prvni znak retezce znacky

    // zisk a uprava dotazovaneho domenoveho jmena z paketu
    for(i = 0; i < MAX_DN_SIZE; i++)
    {
        // prepis retezce ze znacky
        if(j < length)
        {
            domainName[i] = firstLabel[i];
            j++;
        }
        else
        {
            // zisk delky retezce nasledujici znacky 
            length = firstLabel[i] & LABEL_LENGTH_MASK;

            // Pokud je nulova, je ziskano cele domenove jmeno.
            if(length == 0)
            {
                break;
            }

            // Jinak vlozi tecku a resetuje promenou j.
            domainName[i] = '.';
            j = 0;
        }
    }

    domainName[i] = 0;  // Vlozi ukoncujici znak retezce.

    return i + 2;   // Vrati pocet bytu domenoveho jmena v paketu (tedy soucet velikosti vsech znacek).
}


/**
 * Zjisti, zdali ziskane domenove jmeno napatří do nějaké z filtrovaných domén uvedených ve filtrovacím souboru.
 */
int checkDomainName(char *domainName, charPListT *filterFiles)
{
    char filterDomain[MAX_DN_BUFFER_SIZE];  // domenove jmeno ziskane z filtrovaciho souboru
    char *domainSubStr;                     // pomocny ukazatel na podretezec domenoveho jmena
    FILE *fd;                               // deskriptor pro otevreni filtrovaciho souboru a nasledne cteni z nej

    // pruchod pres vsechny filtrovaci soubory
    for(charPNodeT *file = filterFiles->head; file != NULL; file = file->next)
    {
        fd = fopen(file->str, "r");

        if(fd == NULL)
        {
            fprintf(stderr, "INTERNAL_ERROR: file \"%s\" failed to open.\n", file->str);
            return DNSP_SERVER_FAIL;
        }

        // cteni radku z filtrovaciho souboru
        while(fgets(filterDomain, MAX_DN_BUFFER_SIZE, fd) != NULL)
        {
            // kotrola nacteneho radku
            if(checkFileLine(filterDomain, fd) == IGNORE_LINE)
            {
                continue;
            }

            // Domenove jmeno ziskane z filtrovaciho souboru je podretezcem domenoveho jmena z paketu.
            while((domainSubStr = strstr(domainName, filterDomain)) != NULL)
            {
                // Pokud je tento podretezec roven domenovemu jmenu z filtrovaciho souboru, tak je domenove jmeno v dotazu filtrovane.
                if(domainSubStr != NULL && strcmp(domainSubStr, filterDomain) == 0)
                {
                    fclose(fd);
                    return DNSP_REFUSED_DN;
                }

                // Pokud byla nalezena shoda, ale nejednalo se o filtrovanou domenu, posune se ukazatel v kontrolovanem domenovem jmene prave za nalezeny podretezec.
                domainName = domainSubStr + strlen(filterDomain);
            }
        }

        fclose(fd);
    }


    return SUCCESS;
}


/**
 * kontrola nacteneho radku ze souboru
 */
int checkFileLine(char *filterDomain, FILE *fd)
{
    char c;                     // pomocna promenna
    char *notSpace;             // ukazatel za bile znaky
    unsigned int eolPos;        // pozice znaku konce radku
    unsigned int spacesNum;     // pocet bilych znaku

    // zisk pozice znaku konce radku
    eolPos = strcspn(filterDomain, "\n\r");

    // preskoceni prazdneho radku
    if(eolPos == 0)
    {
        return IGNORE_LINE;
    }

    // zisk poctu bilych znaku na zacatku radku
    spacesNum = strspn(filterDomain, " \t");

    // Radek je preskocen, pokud jsou na nem jen bile znaky, nebo pokud je prvnim nebilym znakem '#'.
    if(eolPos == spacesNum || filterDomain[spacesNum] == '#')
    {
        return IGNORE_LINE;
    }

    // Bile znaky na zacatku radku jsou odstraneny.
    if(spacesNum != 0)
    {
        notSpace = &filterDomain[spacesNum];
        memmove(filterDomain, notSpace, strlen(notSpace) + 1);
    }

    // zisk pozice konce domenoveho jmena
    eolPos = strcspn(filterDomain, " \r\n");
    
    // Nebyl nacten znak konce radku nebo bily znak za domenovym jmenem v souboru.
    if(eolPos == strlen(filterDomain))
    {
        // Nacita znaky ze souboru, dokud nenarazi na bily znak, nebo znak konce radku.
        while((c = fgetc(fd)) != EOF && c != '\n' && c != '\r' && c != ' ' && c != '\t' && eolPos < MAX_DN_SIZE)
        {
            // Dokud nebyla prekrocena maximalni delka domenoveho jmena, prida znak na konec pole filtrovane domeny.
            filterDomain[eolPos] = c;
            eolPos++;
        }

        // Nacita nadbytecne znaky na radku, dokud nenarazi na znak konce radku, nebo znak konce souboru.
        while(c != EOF && c != '\n' && c != '\r')
        {
            c = fgetc(fd);
        }
    }

    // Vlozi ukoncujici znak retezce.
    filterDomain[eolPos] = 0;

    return SUCCESS;
}


/**
 * Odpovi klientovi.
 */
int replyToClient(int csSock, int srSock, char *buffer, int msgSize, struct sockaddr_in6 *client, socklen_t clientLength, charPListT *filterFiles)
{
    // Pokud se odpoved nepodarilo odeslat, tak uzavre otevrene sokety, uvolni alokovanou pamet a vrati chybovy navratovy kod.
    if(sendto(csSock, buffer, msgSize, 0, (struct sockaddr *)client, clientLength) != msgSize)
    {
        close(srSock);
        close(csSock);
        clear(filterFiles);
        fprintf(stderr, "SEND_TO_CLIENT_ERROR: Replying to client failed.\n");
        return SEND_TO_CLIENT_ERR;
    }

    return SUCCESS;
}


/**
 * Nastavi chybovou odpoved.
 */
void setErrorResponse(char *packet, int errorCode)
{
    packet[2] |= QR_MASK;   // Nastavi QR bit ve FLAGS na 1, tedy na odpoved.

    // Pokud je v dotazu ve FLAGS nastaven RD bit.
    if(packet[2] & RD_MASK)
    {
        packet[3] |= RA_MASK;   // Nastavi RA vit ve FLAGS na 1.
    }

    packet[3] &= RCODE_MASK;    // Vynuluje RCODE ve FLAGS.
    packet[3] |= errorCode;     // Nastavi RCODE ve FLAGS, tedy chybovy kod.

    if(packet[4] == 0 && packet[5] == 1)
    {
        memset(packet + 6, 0, 6);   // Vynuluje urcite hodnoty v hlavicce paketu, pokud paket obsahuje jednu otazku.
    }
}