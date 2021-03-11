/**
 * ISA - projekt
 * 
 * Dominik Nejedly (xnejed09), 2020
 * 
 * Filtrujici DNS program
 */

#include <netdb.h>
#include "charPList.h"

//navratove hodnoty
#define SUCCESS 0

#define PARAM_ERR 1
#define CS_SOCK_ERR 2
#define CS_SOCK_BIND_ERR 3
#define SR_SOCK_ERR 4
#define SR_SOCK_CONNECT_ERR 5
#define SEND_TO_CLIENT_ERR 6

#define INTERNAL_ERR 99

//pomocna makra
#define HELP 2

#define IGNORE_LINE 1

#define DNSP_INV_FORMAT 1
#define DNSP_SERVER_FAIL 2
#define DNSP_NOT_IMPLEMENTED 4
#define DNSP_REFUSED_DN 5

//masky pro bitove operace
#define QR_MASK 128
#define Z_MASK 64
#define LABEL_LENGTH_MASK 63
#define RD_MASK 1
#define RA_MASK 128
#define RCODE_MASK 240

//vychozi DNS port
#define DNS_SERVER_PORT "53"

//maximalni cislo portu
#define MAX_PORT_NUM 65535

//maximalni velikost DNS paketu
#define BUFFER_SIZE 512

//maximalni velikost domenoveho jmena
#define MAX_DN_SIZE 253

//velikost pole pro domenove jmeno
#define MAX_DN_BUFFER_SIZE 254

//deklarace funkci
int parseArgs(int argc, char *argv[], struct addrinfo **resolver, int *port, charPListT *filterFiles);
int connectToResolver(struct addrinfo *nameServer, int *srSock);
int checkPacket(char *packet, charPListT *filterFiles, int *queryLength);
int getDomainName(char *domainName, char *firstLabel);
int checkDomainName(char *domainName, charPListT *filterFiles);
int checkFileLine(char *filterDomain, FILE *fd);
int replyToClient(int csSock, int srSock, char *buffer, int msgSize, struct sockaddr_in6 *client, socklen_t clientLength, charPListT *filterFiles);
void setErrorResponse(char *packet, int errorCode);