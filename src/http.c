#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "http.h"

#define BUF_SIZE 1024
#define GET "getter"
#define HEAD "header"

/**
 * Create Client Socket by TCP
 * @param host_name - The host name e.g. www.canterbury.ac.nz
 * @param port - e.g. 80
 */
int client_socket(char *host_name, int server_port){
    //Conver int server_port into string
    char port[20];
    sprintf(port, "%d", server_port);

    struct addrinfo hints;
    struct addrinfo *server_info; //point to the result of getaddrinfo function

    //make sure the hints struct is empty
    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_INET; //use an internet address 
    hints.ai_socktype = SOCK_STREAM; //use TCP rather than UDP

    getaddrinfo(host_name, port, &hints, &server_info); //set up the server addrinfo struct that will use later

    //initialize a socket for client
    int client_sockfd = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    

    //Connect socket to server
    int rc = connect(client_sockfd, server_info->ai_addr, server_info->ai_addrlen);
    if(rc == -1){
        perror(">>Connection error with server\n");
        exit(1);
    }

    return client_sockfd;
}

/**
 * Create Http Request Packet
 * @param host_name - The host name e.g. www.canterbury.ac.nz
 * @param page - e.g. /index.html
 * @param range - Byte range e.g. 0-500. NOTE: A server may not respect this
 */
char* pack_http_request(char* host, char* page, const char* range, const char* method){
    char *http_request_packet = (char*)malloc(1024);

    memset(http_request_packet, 0, 1024);

    //Pack http request together from following

    //Separate by HEAD or GET reqeust
    if (strcmp(method, HEAD) == 0){
        strcat(http_request_packet, "HEAD /");
    }else{
        strcat(http_request_packet, "GET /");
    }

    strcat(http_request_packet, page);
    strcat(http_request_packet, " HTTP/1.0\r\n");
    strcat(http_request_packet, "Host: ");
    strcat(http_request_packet, host);
    strcat(http_request_packet, "\r\n");

    //When range has data, add "Range" into request
    if(range != NULL && strlen(range) > 0){
        strcat(http_request_packet, "Range: bytes=");
        strcat(http_request_packet, range);
        strcat(http_request_packet, "\r\n");
    }

    strcat(http_request_packet, "User-Agent: ");
    strcat(http_request_packet, "getter");
    strcat(http_request_packet, "\r\n\r\n");

    return http_request_packet;
}

/**
 * Sned Http Request Packet to Server
 * @param client_sockfd
 * @param http_request
 */
int send_http_request(int client_sockfd, char* http_request){
    int result = write(client_sockfd, http_request, strlen(http_request));
    if(result < 0){
        printf(">>Send http request error!\n");
        exit(1);
    }

    return result;
}

/**
 * Perform an HTTP 1.0 query to a given host and page and port number.
 * host is a hostname and page is a path on the remote server. The query
 * will attempt to retrievev content in the given byte range.
 * User is responsible for freeing the memory.
 * 
 * @param host - The host name e.g. www.canterbury.ac.nz
 * @param page - e.g. /index.html
 * @param range - Byte range e.g. 0-500. NOTE: A server may not respect this
 * @param port - e.g. 80
 * @return Buffer - Pointer to a buffer holding response data from query
 *                  NULL is returned on failure.
 */
Buffer* http_query(char *host, char *page, const char *range, int port) {
    // assert(0 && "not implemented yet!");

    Buffer *response;
    int read_count;
    char *http_request;

    //Step1: Setup Socket TCP connection
    int client_sockfd = client_socket(host, port);

    //Step2: send out http request
    http_request = pack_http_request(host, page, range, GET);
    send_http_request(client_sockfd, http_request);

    //Step3: get response from server
    response = (Buffer*)malloc(sizeof(Buffer));
    response->data = (char *)malloc(sizeof(char) * BUFSIZ);
    memset(response->data, 0, BUFSIZ);
    response->length = 0;

    //Define pointer to store read data
    char *new_read_data = malloc(BUFSIZ);

    read_count = 0;
    while((read_count = read(client_sockfd, new_read_data, BUFSIZ)) > 0){
        response->length = response->length + read_count;
        response->data = realloc(response->data, response->length);
        //copy data to  the end of response->data (!!response->data is the starting position for char[])
        memcpy(response->data + response->length - read_count, new_read_data, read_count);

    }

    close(client_sockfd);
    free(http_request);
    free(new_read_data);

    return response;
}


/**
 * Separate the content from the header of an http request.
 * NOTE: returned string is an offset into the response, so
 * should not be freed by the user. Do not copy the data.
 * @param response - Buffer containing the HTTP response to separate 
 *                   content from
 * @return string response or NULL on failure (buffer is not HTTP response)
 */
char* http_get_content(Buffer *response) {

    char* header_end = strstr(response->data, "\r\n\r\n");

    if (header_end) {
        return header_end + 4;
    }
    else {
        return response->data;
    }
}


/**
 * Splits an HTTP url into host, page. On success, calls http_query
 * to execute the query against the url. 
 * @param url - Webpage url e.g. learn.canterbury.ac.nz/profile
 * @param range - The desired byte range of data to retrieve from the page
 * @return Buffer pointer holding raw string data or NULL on failure
 */
Buffer *http_url(const char *url, const char *range) {
    char host[BUF_SIZE];
    strncpy(host, url, BUF_SIZE);

    char *page = strstr(host, "/");
    
    if (page) {
        page[0] = '\0';

        ++page;
        return http_query(host, page, range, 80);
    }
    else {

        fprintf(stderr, "could not split url into host/page %s\n", url);
        return NULL;
    }
}

/**
 * Gets the content length from response of HEAD request
 * @param response   response from HEAD request
 * @return int  content length
 */
int get_content_size_by_head(Buffer* response){
    //Find the position of Content-Length in response
    char *content_length = strstr(response->data, "Content-Length:");
    char *content_length_end = strstr(content_length, "\n");
    content_length_end[0] = '\0';

    char length[BUF_SIZE];
    int j = 0;

    //Extract content length in a char array
    for(int i = 0; content_length[i] != '\0'; ++i){
        if(content_length[i]>='0' && content_length[i]<='9'){
            length[j] = content_length[i];
            j++;
        }
    }
    length[j] = '\0';
    //Convert char array into number
    int content_size = atoi(length);

    return content_size;
}

/**
 * Calculate max_chunk_size and tasks
 * @param content_size   Size of download content
 * @param is_accept_ranges   The URL of the resource to download
 * @param threads   The number of threads to be used for the download
 * @return int  The number of downloads needed satisfying max_chunk_size
 *              to download the resource
 */
int calc_tasks(char* is_accept_ranges, int content_size, int threads){
    //Check whether server respect of range or not and calculate max_chunk_size and tasks.
    int tasks = threads; // tasks < Queue Capacity
    // int tasks = threads * 2; //tasks = Queue Capacity
    // int tasks = threads * 3; //tasks > Queue Capacity

    if (is_accept_ranges) {
        max_chunk_size = content_size / tasks;
    }
    else {
        max_chunk_size = content_size;
        tasks = 1;
    }
    return tasks;
}

/**
 * Makes a HEAD request to a given URL and gets the content length
 * Then determines max_chunk_size and number of split downloads needed
 * @param url   The URL of the resource to download
 * @param threads   The number of threads to be used for the download
 * @return int  The number of downloads needed satisfying max_chunk_size
 *              to download the resource
 */
int get_num_tasks(char *url, int threads) {
    char host[BUF_SIZE];
    char *head_http_request;
    Buffer *response;
    int read_count;

    //Separate host and page from url
    strncpy(host, url, BUF_SIZE);

    char *page = strstr(host, "/");
    
    if (page) {
        page[0] = '\0';
        ++page;
    }else {
        fprintf(stderr, "could not split url into host/page %s\n", url);
    }
    
    //Step1: Setup Socket TCP connection
    int client_sockfd = client_socket(host, 80);

    //Step2: send out http request
    head_http_request = pack_http_request(host, page, "", HEAD);
    send_http_request(client_sockfd, head_http_request);

    //Step3: get response from server
    response = (Buffer*)malloc(sizeof(Buffer));
    response->data = (char *)malloc(sizeof(char) * BUFSIZ);
    memset(response->data, 0, BUFSIZ);
    response->length = 0;

    //Define pointer to store read data
    char *new_read_data = malloc(BUFSIZ);

    read_count = 0;
    while((read_count = read(client_sockfd, new_read_data, BUFSIZ)) > 0){
        response->length = response->length + read_count;
        response->data = realloc(response->data, response->length);
        //copy data to  the end of buffer->data (!!buffer->data is the starting position for char[])
        memcpy(response->data + response->length - read_count, new_read_data, read_count);

    }

    //step4: Check whether server respect range setting
    char *is_accept_ranges = strstr(response->data, "Accept-Ranges: bytes");

    //Step5: Extract content size from HEAD response
    int content_size = get_content_size_by_head(response);
    //Base on ranges accept situation, calculate max_chunk_size and return task num
    int tasks = calc_tasks(is_accept_ranges, content_size, threads);

    close(client_sockfd);
    free(head_http_request);
    free(new_read_data);
    free(response->data);
    free(response);

    return tasks;
}


int get_max_chunk_size() {
    return max_chunk_size;
}