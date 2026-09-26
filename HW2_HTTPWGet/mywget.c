#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>


#include<sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

//#include <WinSock2.h>
//#include <ws2tcpip.h>
#include <unistd.h>

#include <stdint.h>
#include "arraylist.h"

#define BUFSIZE 1024

struct myargs {
    char* url;
    char domain[BUFSIZE]; // Domain of URL
    char path[BUFSIZE]; // Path of URL
    char* port; // 80 by default
    char* target;
    struct timeval timeout; // 10 (second) by default
};

/**
 * @brief Separate a URL out into the domain part and the path part
 * 
 * @param url Pointer to a URL string
 * @param domain Pointer to a domain string to which to write result, 
 *               assumed to be BUFSIZE large
 * @param path Pointer to path string to which to write result,
 *               assumed to be BUFSIZE large.  If there is no path, put
 *               a "/"
 */
void parseURL(char* url, char* domain, char* path) {
    char* httpStr = "http://";
    int len = strlen(url);
    memset(domain, '\0', BUFSIZE);
    memset(path, '\0', BUFSIZE);
    if (strncmp(url, httpStr, strlen(httpStr)) == 0) {
        // Skip over any http:// at the front
        url += strlen(httpStr);
    }
    int idxSep = 0;
    while (idxSep < len && url[idxSep] != '/') {
        idxSep++;
    }

    if (idxSep+1 > BUFSIZE) {
        fprintf(stderr, "ERROR: Domain part of URL exceeds %i bytes", BUFSIZE);
        exit(0);
    }
    strncpy(domain, url, idxSep);

    if (len-idxSep+1 > BUFSIZE) {
        fprintf(stderr, "ERROR: Path part of URL exceeds %i bytes", BUFSIZE);
        exit(0);
    }
    if (idxSep == len) {
        // No path specified; default to "/"
        path[0] = '/';
    }
    else {
        strncpy(path, url+idxSep, len-idxSep+1);
    }
}

/**
 * @brief Parse command line arguments for the HTTP client
 */
struct myargs parseArgs(int argc, char** argv) {
    struct myargs ret;
    // Step 1: Setup default values
    ret.url = "";
    ret.port = "80";
    ret.timeout.tv_sec = 10;
    ret.timeout.tv_usec = 0;
    ret.target = "";

    // Step 2: Parse user specified values
    // Advance to the next element
    char* programName = argv[0];
    argv++; 
    argc--; 
    while (argc > 0) {
        if((*argv)[0] == '-') {
            if (strcmp(*argv, "--help") == 0) {
                printf("Usage: %s --url <url of file>", programName);
                printf(" --target <target filename to save>");
                printf(" [--port <port number>] [--timeout <timeout>]\n");
                exit(0);
            }
            else if (strcmp(*argv, "--url") == 0) {
                argv++; argc--;
                if (argc > 0) {
                    ret.url = *argv;
                    parseURL(*argv, ret.domain, ret.path);
                }
                else {
                    fprintf(stderr, "Error: Expecting field after --url\n");
                    exit(0);
                }
            }
            else if (strcmp(*argv, "--port") == 0) {
                argv++; argc--;
                if (argc > 0) {
                    ret.port = *argv;
                }
                else {
                    fprintf(stderr, "Error: Expecting field after --port\n");
                    exit(0);
                }
            }
            else if (strcmp(*argv, "--target") == 0) {
                argv++; argc--;
                if (argc > 0) {
                    ret.target = *argv;
                }
                else {
                    fprintf(stderr, "Error: Expecting field after --path\n");
                    exit(0);
                }
            }
            else if (strcmp(*argv, "--timeout") == 0) {
                argv++; argc--;
                if (argc > 0) {
                    ret.timeout.tv_sec = atol(*argv);
                }
                else {
                    fprintf(stderr, "Error: Expecting field after --timeout\n");
                    exit(0);
                }
            }

        }
        else {
            fprintf(stderr, "Warning: Unrecognized field %s\n", *argv);
        }
        argv++; argc--;
    }

    // Step 3: Check for required values
    if (strcmp(ret.url, "") == 0) {
        fprintf(stderr, "Error: Require a --url to be specified\n");
        exit(0);
    }
    if (strcmp(ret.target, "") == 0) {
        fprintf(stderr, "Error: Require a --target to be specified\n");
        exit(0);
    }

    return ret;
}

int findSocket(struct myargs args);

int findMessageBodyIndex(struct ArrayListBuf b, ssize_t bytesTotal);


//make | ./mywget --url www.ctralie.com/ctralie_cv.pdf --port 80 --target out.pdf
int main(int argc, char** argv) {
    struct myargs args = parseArgs(argc, argv);
    
    //My fault for choosing windows.
    /*
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) 
    {
        fprintf(stderr, "WSAStartup failed\n");
        exit(1);
    }
    */
    
    //finding a working socket
    int sock = findSocket(args);

    //testing sockfd again, just in case.
    if (sock < 0) 
    {
        exit(-1);
    }
    printf("Connected successfully!\n");

    // ... send()/recv() logic goes here ...

    //Constructing GET request
    char request[1024];
    int n =snprintf(request, sizeof(request),
        "GET %s HTTP/1.0\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "\r\n",
        args.path, args.domain);
    if (n < 0 || n >= (int)sizeof(request)) 
    {
       fprintf(stderr, "Request too long or encoding error\n");
       exit(2);
    }
    //Testing message
    printf("Message: \n%s", request);

    // Send the HTTP request over the socket
    if (send(sock, request, strlen(request), 0) == -1) 
    {
        perror("Send failed");
        closesocket(sock);
        exit(EXIT_FAILURE);
    }

    // Receive and print the response chunks until the server closes the connection
    //recv(sockfd, buf, size, flags);
    struct ArrayListBuf buffedList;
    ArrayListBuf_init(&buffedList);
    char tempBuffer[buffedList.capacity];
    ssize_t bytesCurrent;
    ssize_t bytesTotal = 0;

    while ((bytesCurrent = recv(sock, tempBuffer, sizeof(tempBuffer), 0)) > 0) 
    {
        ArrayListBuf_push(&buffedList, tempBuffer, (int)bytesCurrent);
        bytesTotal += bytesCurrent;
    }
    if (bytesCurrent == -1) 
    {
        perror("read failed");
    }

    //add a check for OK 200 and other status codes
    //char current = '0';
    int i = 0;
    int j = 0;
    char statusCode[1028] = "";
    while(buffedList.buff[i] != ' '){++i;}
    ++i;
    while(buffedList.buff[i] != ' ')
    {
        statusCode[j] = buffedList.buff[i];
        ++i;
        ++j;
    }
    //Code get!
    int statusCodeInt = atoi(statusCode);

    //Checking status code
    if(statusCodeInt == 200)
    {
        //Move index until we find "\r\n\r\n"
        int bodyStart = findMessageBodyIndex(buffedList, bytesTotal);
        int bodyLen = bytesTotal - bodyStart;

        //1 char is 8 bits, 1 byte. 
        FILE *fptr;
        // Open a file in writing mode (WRITING BIT MODE)
        fptr = fopen(args.target, "wb");
        if (!fptr) 
        {
            perror("fopen failed");
            exit(3);
        }
        fwrite(buffedList.buff + bodyStart, 1, bodyLen, fptr);
        fclose(fptr);
    }
    else
    {
        //printf("Server failure: %d\n", statusCodeInt);
        fprintf(stderr, "Server failure: %d\n", statusCodeInt);
        ArrayListBuf_free(&buffedList);
        closesocket(sock);
        exit(4);
    }

    //Always close/free your memory before you head out.
    ArrayListBuf_free(&buffedList);
    closesocket(sock);
    return 0;
}

int findSocket(struct myargs args)
{
    //creating addrinfo struct including 
    // hints: Criteria for valid IP address
    // res: a list of addresses
    // p: a pointer we use to iterate through res
    struct addrinfo hints, *res, *p;
    int sockfd;
    
    //Allocating memory for hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    int status = getaddrinfo(args.domain, args.port, &hints, &res);
    
    // Tests for getaddrinfo error
    if (status != 0) 
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return -1;
    }

    // Try each result until one connects successfully
    for (p = res; p != NULL; p = p->ai_next)
    {
        
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        //Testing if socket is valid
        if (sockfd == -1) 
        {
            perror("socket failed");
            continue;
        }

        //testing how our socket works in connect
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) 
        {
            perror("connect failed");
            closesocket(sockfd);
            continue;
        }
        //Success
        break;
    }

    //This means there are no valid ports or sockets to connect to our desired domain (at the time of request)
    if (p == NULL) 
    {
        fprintf(stderr, "Failed to connect to %s:%s\n", args.domain, args.port);
        freeaddrinfo(res);
        exit(-1);
    }

    //releasing memory for linked list
    freeaddrinfo(res);
    return sockfd;
}

int findMessageBodyIndex(struct ArrayListBuf b, ssize_t bytesTotal)
    {
        int i = 0;
        while (i + 3 < bytesTotal &&
            !(b.buff[i]   == '\r' && b.buff[i+1] == '\n' &&
            b.buff[i+2] == '\r' && b.buff[i+3] == '\n')) 
            {i++;}
        if (i + 3 >= bytesTotal) 
        {
            fprintf(stderr, "Could not find end of HTTP headers\n");
            exit(1);
        }
        //skip the messsage
        return i+4;
    }

