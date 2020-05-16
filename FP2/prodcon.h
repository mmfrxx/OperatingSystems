#ifndef PRODCON

#define PRODCON

#define QLEN            5
#define BUFSIZE         2048
#define MAX_CLIENTS     512
#define MAX_PROD    	480
#define MAX_CON     	480
#define SLOW_CLIENT     3
#define REJECT_TIME     2
#define BYTE_ERROR	"ERROR: bytes"
#define SUCCESS		"SUCCESS: bytes"
#define REJECT		"ERROR: REJECTED"
#define MAX_LETTERS     1000000

// function prototypes
int connectsock( char *host, char *service, char *protocol );
int passivesock( char *service, char *protocol, int qlen, int *rport );

// This item struct will work for all versions
typedef struct item_t
{
	int	size;
	int		psd;
    char		*letters;
} ITEM;




#endif
