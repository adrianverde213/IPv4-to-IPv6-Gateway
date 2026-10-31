#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#define SERVER_PORT 22227
#define HTTP_PORT "80"
#define BUFFER 512

int procesare_client(int sockfd_client, const char *host, const char *filename) {

    int sockfd;
    struct addrinfo server_addr, *res, *p;
    int status;
    FILE *fp = NULL; 
    char recvBuff[BUFFER];
    char cleanBuff[BUFFER];
    ssize_t numBytes, writtenBytes;


    // HTTP request sent to the server to retrieve its page
    char *http_req = "GET / HTTP/1.0\r\n\r\n";            

    printf("\n Incercare conectare la server: \"%s\" \n", host);


    // These calls are explained in more detail in the main function (MAIN)
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.ai_family = AF_INET6;    
    server_addr.ai_socktype = SOCK_STREAM;

	
    // Converts the host name string and the port into usable address structures
    // server_addr (the hints) filters the results, so we only get IPv6 addresses usable over TCP
    if ((status = getaddrinfo(host, HTTP_PORT, &server_addr, &res)) != 0) {
        fprintf(stderr, "\n Eroare getaddrinfo: %s. \n", gai_strerror(status));
        return 0;
    }

    // getaddrinfo() returned a linked list of candidate addresses; we iterate through them until
    // we find one that works
    // p starts at the head of the list (p = res) and we advance element by element (p->ai_next) while p != NULL
    for(p = res; p != NULL; p = p->ai_next) {

	// socket() creates a socket for address "p", using the family, type and protocol it describes
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
		// If the socket cannot be created, move on to the next address
		continue;
        }

	// Try to connect to the server using the created socket
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
        	// If the connection fails, close the socket to release its file descriptor
		close(sockfd);
		// Again, move on to the next address
        	continue;
        }
	
	// Reaching this point means the connection was established (connect() returned 0)
	// Leave the loop
        break; 
    }

    // If p is NULL, no usable address was found and we cannot continue
    if (p == NULL) {	
        fprintf(stderr, "\n Nu s-a putut face conexiunea la server. \n");
        
	// Free the address list to avoid a memory leak
	freeaddrinfo(res);
        return 0;
    }

    // Free the address list (res), since we no longer need it
    freeaddrinfo(res); 

    printf("\n Conectat cu succes la \"%s\". \n", host);

    // Send the request through the socket
    // If sending fails we cannot get the HTML from the site, so close both sockets
    if (send(sockfd, http_req, strlen(http_req), 0) == -1 ){
        printf("\nNu s-a putut prelua informatia de pe site.\n");
        close(sockfd_client); 
        close(sockfd);
        return 0;
    }

    printf("\n Comanda trimisa. \n");
    
    // Check that the variable holding the name of the saved HTML file is not NULL
    if (filename != NULL) {
    	
	// If it is not, open the file with that name in "wb" mode
	// If the file does not exist, it will be created
	// wb => Write/Binary: "w" is needed to write to the file, and "b" writes the bytes exactly as they are, without any translation
        fp = fopen(filename, "wb");
        

	// If the file cannot be opened/created, print an error message
        if (fp == NULL) {
            fprintf(stderr, "\n Nu s-a putut crea/deschide fisierul %s: %s. \n", filename, strerror(errno));
        } else {
	    // Otherwise report success: the file was created in the current directory
            printf("\n Fisierul \"%s\" a fost creat.\n", filename);
        }
    }    

    // The data received through recv() also contains the HTTP response header at the start of the HTML
    // It is plain text with details about the connection, content length, etc.
    // It must be removed so the retrieved page can be displayed correctly
    // The variable below is a flag that tells whether the end of the header has been found
    int headerGasit = 0;

    // The text sequence that delimits the header is "\r\n\r\n". It sits at the end of the header, right before
    // the actual HTML content, i.e. the <!DOCTYPE HTML> element
    const char *headerSecventa = "\r\n\r\n";

    // Since the sequence length is also needed to resize the data, store it in a variable
    // ("headerLungime"); strlen() returns 4 (bytes) for "\r\n\r\n"
    int headerLungime = strlen(headerSecventa);

    // The while() loop below reads the data coming from the site (HTML content)
    while ((numBytes = recv(sockfd, recvBuff, sizeof(recvBuff), 0)) > 0) {	

	// While the sequence above has not been found, check every received chunk until it is
	if (!headerGasit){

		// strstr() searches the buffer holding the received data for the delimiter sequence
		// If found, it returns a pointer to the first occurrence of "headerSecventa" defined earlier
		char *headerPozSfarsit = strstr(recvBuff, headerSecventa);

		// If the delimiter was found, remove the header with the code inside this if; otherwise continue
		if (headerPozSfarsit != NULL){

			// Since the delimiter is 4 bytes long, we can drop everything from the start of the response
			// up to and including the delimiter, removing the unwanted header from the HTML
			// removedBytes is the offset of the first byte of the actual content
			size_t removedBytes = headerPozSfarsit + headerLungime - recvBuff;
		

			// A for() loop removes the header, including the "\r\n\r\n" sequence
			// Overwrite the header by copying the useful data (from index "removedBytes") to the start of the buffer
			// Finally, subtract the header size from the total length of the processed data

			for(size_t i = removedBytes; i < numBytes; i++){
				recvBuff[i - removedBytes] = recvBuff[i];
			}
			
			// Update the buffer size
			// Set the flag from 0 to 1: the header has been found, so this if block is not entered again
			numBytes = numBytes - removedBytes;
			headerGasit = 1;
		}
	}

	// Initialize the variable that counts the bytes kept after removing carriage returns
        int cleanBytes = 0;

        // This loop removes carriage returns ('\r'), so no "^M" characters end up in the created "index.html"
        for (int i = 0; i < numBytes; i++) {
            if (recvBuff[i] != '\r') {
                cleanBuff[cleanBytes] = recvBuff[i];
                cleanBytes++;
            }
        }
	
	// Check that there is data left after filtering
        if (cleanBytes > 0) {

	    // Write to the file only if it was opened successfully
            if (fp != NULL) {

		// Write the buffer to "index.html"
                writtenBytes = fwrite(cleanBuff, 1, cleanBytes, fp);
   
		// Check that the whole buffer was written to the file
		// If the condition fails, the buffer could not be fully written, for any of several reasons
		// (e.g. not enough free disk space)
                if(writtenBytes < cleanBytes){
                    fprintf(stderr,"\n Eroare la scrierea in fisier. \n");
                }
            }

            // Check for errors when sending data to the client that sent the request
	    // Send the HTML to the client that requested the site; the page is displayed inside the ClientIPv4 application
            if (send(sockfd_client, cleanBuff, cleanBytes, 0) == -1) {
                perror("\n Eroare la trimiterea datelor catre client ");
                break;
            }
        }
    }

    // Check why the loop above ended
    // A return value of -1 means an error occurred during the while() loop
    if (numBytes == -1) {
        perror("\n Conexiunea la server s-a intrerupt ");
    }

    // Close the file only if it was opened
    if (fp != NULL) fclose(fp); 
    close(sockfd);
    return 0;
}


int main(){
    int sockfd_listen; // Main (listening) socket
    int sockfd_client; // Per-client (connected) socket

    struct sockaddr_in server_addr;
    struct sockaddr_storage client_addr_info; 

    // socklen_t variable that stores the size of the received address structure
    socklen_t client_addr_size;
   

    // Host we connect to for testing the code.
    // The mini-project assignment required using "www.speedtest6.com", but it was not working.
    // So we looked for a site that has an IPv6 address and also accepts connections on port 80 (HTTP)
    char *host = "ipv6.test-ipv6.com"; 

    // The page content is saved in a file named "index.html"
    char *filename = "index.html"; 
        

    // Client counter, used for display purposes
    int nr_client_curent = 0;


    // Create the listening socket (IPv4, TCP) with socket() and store its descriptor
    sockfd_listen = socket(AF_INET, SOCK_STREAM, 0);

    // Check whether the socket was created successfully
    // On failure, socket() returns -1
    if(sockfd_listen == -1){
        perror("\n Nu s-a putut crea socket-ul principal ");
        return 0;
    }

    printf("\nSocket creat cu succes!\n");


    // Socket options
        // SO_REUSEADDR matters when restarting the server
        // When the server stops, the port stays blocked in the TIME_WAIT state for a short time
        // This option allows restarting the server immediately on the same port without errors

    int da = 1; // SO_REUSEADDR expects a pointer to the value, so we cannot pass "1" directly to setsockopt()

    if (setsockopt(sockfd_listen, SOL_SOCKET, SO_REUSEADDR, &da, sizeof(da)) == -1) {
        perror("\nEroare la setarea optiunilor socket-ului ");
        close(sockfd_listen);
        return 0;
    }
    
    // Zero the structure using memset
    memset(&server_addr, 0, sizeof(server_addr));

    // Use IPv4
    server_addr.sin_family = AF_INET;

    // INADDR_ANY tells the server to listen on all network interfaces
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // htons() => Host TO Network Short
    // Converts the port number from the host byte order (little endian) to the standard network byte order (big endian)
    server_addr.sin_port = htons(SERVER_PORT);

    // The bind() function
        // Associates the socket with the IP address and port configured above
       // Cast to (struct sockaddr *) because bind() is a generic function that accepts any address type, not only sockaddr_in

    if (bind(sockfd_listen, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1){
        perror("\nEroare la asocierea portului ");
        close(sockfd_listen);
        return 0;
    }

    printf("\nPortul dvs este: %d.\n", SERVER_PORT);

    // The listen() function
        // Marks the socket as passive (it waits for incoming connections)

    if (listen(sockfd_listen, 10) == -1){
        perror("\nEroare in timpul ascultarii pe port ");
        close(sockfd_listen);
        return 0;
    }

    printf("\nSe asteapta conexiuni la server.....\n");

    // MAIN loop: accept clients forever
    while(1){ 

        // Store the size of client_addr_info in a separate variable for readability
        client_addr_size = sizeof(client_addr_info);

        // accept() is a blocking function: the while() loop pauses until
	// a connection arrives on the socket
        sockfd_client = accept(sockfd_listen, (struct sockaddr *)&client_addr_info, &client_addr_size);
    
        if (sockfd_client == -1) {
            perror("\nEroare la acceptarea conexiunii client ");
            continue;
        }
        
        // Increment the client counter
        nr_client_curent++;
        
        printf("\nS-a conectat clientul cu Nr. %d. Se creeaza procesul secundar...\n", nr_client_curent);

        // The fork() function
        // fork() clones the current process (1 process => 2 processes)
        // Called n times in a row, fork() can produce 2^n concurrent processes
        // It returns 0 in the child process and the PID of the child (an int) in the parent process

        if (fork() == 0) {

            // Child process section
            // This code runs independently and in parallel with the parent
            // The child does not need to accept other clients, so it closes the listening socket

            close(sockfd_listen);
            
	    // Buffer for the message sent by ClientIPv4; recv() reads at most BUFFER - 1 bytes,
	    // leaving room for the null terminator, which allows string comparison
            char buff[BUFFER];
            ssize_t numBytes;
        
            while(1){
            
                numBytes  = recv(sockfd_client, buff, BUFFER - 1, 0);
            
		// A return value of -1 means recv() failed
                if (numBytes == -1){
                    printf("\nAplicatia clientului cu Nr. \"%d\" a fost inchisa. \n", nr_client_curent);
		    break;
                }
		// A return value of 0 means the client disconnected
                else if (numBytes == 0){
                    printf("\nClientul cu Nr. \"%d\" s-a deconectat. \n", nr_client_curent);
                    break;
                }
                else{
            
		    // Add the null terminator to close the received message, since the client application does not add it automatically
                    buff[numBytes] = '\0';

                    printf("\nClientul cu Nr. \"%d\" a trimis comanda: \"%s\" . \n", nr_client_curent, buff);

		  
		    // Command "07#" is used for testing the code
		    // If it is received, run the processing function; otherwise send the message "Comanda neimplementata" (command not implemented)
                    if (strcmp(buff, "07#") == 0){
                        procesare_client(sockfd_client, host, filename);	
                    }
                    else{
			// For readability, store the message and pass its length to send() via strlen(), so we do not have to count its characters by hand
			const char *mesajNeimplementat = "\nComanda neimplementata.\n";

                        if(send(sockfd_client, mesajNeimplementat, strlen(mesajNeimplementat) ,0) == -1){
                            perror("\n Eroare la trimiterea mesajului catre client. \n");
                        }
                    }
                }
            }

            // The child closes its client socket and terminates
            close(sockfd_client);
            exit(0); 
        } else {
            // Parent process section
 
            // The parent does not talk to the client directly; the child process handles the connection
            // It must close its own copy of the client socket descriptor, otherwise the connection
            // will not close properly and descriptors would leak

            close(sockfd_client); 
        }
    }

    // If the loop is ever exited because of an error, close the listening socket

    close(sockfd_listen);
    return 0;
}
