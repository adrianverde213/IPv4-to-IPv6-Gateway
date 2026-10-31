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


    // Mesajul de trimis catre server pentru a putea face conexiunea
    char *http_req = "GET / HTTP/1.0\r\n\r\n";            

    printf("\n Incercare conectare la server: \"%s\" \n", host);


    // Pentru o explicatie, sunt explicate in functia principala (MAIN)
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.ai_family = AF_INET6;    
    server_addr.ai_socktype = SOCK_STREAM;

	
    // Transforma string-ul/numele host-ului si portul in structuri de adresa utilizabile
    // Variabila server_addr este utilizat pentru a filtra conexiunile, adica sa ne putem conecta doar la adrese de tip IPv6, prin TCP
    if ((status = getaddrinfo(host, HTTP_PORT, &server_addr, &res)) != 0) {
        fprintf(stderr, "\n Eroare getaddrinfo: %s. \n", gai_strerror(status));
        return 0;
    }

    // Functia de inainte a returnat o lista inlantuita (linked list) de adrese pe care le poate uitliza, si itereaza prin fiecare pana cand
    // gaseste una potrivita/utilizabila
    // Variabila p ar fi inceputul listei (p = res), si iteram cat timp p are elemente (p != NULL), element cu elelement (p->ai_next)
    for(p = res; p != NULL; p = p->ai_next) {

	// Cu functia socket() cream legatura pentru adresa "p", cu specificatiile date mai sus
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
		// Daca nu putem crea socket-ul, trecem la urmatorul
		continue;
        }

	// Se incearca conexiunea la server folosind socket-ul creat
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
        	// Daca conexiunea esueaza, socket-ul este invalid, si trebuie sa-l inchidem pentru a elibera "file descriptor"-ii
		close(sockfd);
		// Iarasi, trecem la urmatoarea adresa
        	continue;
        }
	
	// Daca ajungem aici inseamna ca conexiunea a fost stabilita (functia connect() returneaza 0)
	// Iesim din bucla
        break; 
    }

    // Daca p = NULL inseamna ca nu s-a gasit vreo adresa eligiba si nu putem continua
    if (p == NULL) {	
        fprintf(stderr, "\n Nu s-a putut face conexiunea la server. \n");
        
	// Eliminam din memorie valoarea lui res pentru a nu intampina probleme legate de memorie
	freeaddrinfo(res);
        return 0;
    }

    // Golim informatiile din variabile res (adresa si informatiile despre adresa) deoarece nu le mai utilizam
    freeaddrinfo(res); 

    printf("\n Conectat cu succes la \"%s\". \n", host);

    // Trimitem request-ul prin socket
    // Daca nu putem lua informatia html de pe site, vom inchide si socket-ul creat
    if (send(sockfd, http_req, strlen(http_req), 0) == -1 ){
        printf("\nNu s-a putut prelua informatia de pe site.\n");
        close(sockfd_client); 
        close(sockfd);
        return 0;
    }

    printf("\n Comanda trimisa. \n");
    
    // Verificam daca variabila care retine numele fisierului html salvat nu este NULL
    if (filename != NULL) {
    	
	// Daca nu, vom deschide fisierul cu acelasi nume in modul wb
	// Daca fisierul in sine nu exista, acesta va fi creat
	// wb => Write/Binary. Pentru a putea scrie in fisier trebuie folosit modul "w", iar pentru a putea scrie informatiile exact cum sunt scrise in fisier
        fp = fopen(filename, "wb");
        

	// Daca nu putem deschide/crea fisierul, vom afisa o eroare prezentand faptul acesta
        if (fp == NULL) {
            fprintf(stderr, "\n Nu s-a putut crea/deschide fisierul %s: %s. \n", filename, strerror(errno));
        } else {
	    // Altfel, vom spune ca totul s-a terminat cu bine, fisierul fiind creat si salvat in acealsi directoriu
            printf("\n Fisierul \"%s\" a fost creat.\n", filename);
        }
    }    

    // La cererea de la recv() se capteaza si un mesaj de tip HTTP HEAD care se afla la inceputul codului HTML
    // Acesta este salvat ca un text simplu care arata lucruri legate de conexiune, locatie, numar de biti transmisi etc
    // Trebuie eliminate pentru a putea citi corect site-ul preluat
    // Variabila de mai jos o vom folosi ca un flag pentru gasirea/detectarea acestui mesaj
    int headerGasit = 0;

    // Secventa de text care delimiteaza header-ul este "\r\n\r\n". Acesta se afla la sfarsitul secventei header si chiar inainte de
    // partea concreta a codului html, adica elementul <!DOCTYPE HTML>
    const char *headerSecventa = "\r\n\r\n";

    // Stiind ca secventa noastra va fi utilizata si pentru a restructura marimea secventei, vom salva marimea secventei header intr-o variabila
    // "headerLungime"; Functia strlen() va return pt "\r\n\r\n" valoarea 4 (octeti)
    int headerLungime = strlen(headerSecventa);

    // Bucla while() de mai jos va fi folosita pentru citirea informatiei de pe site (informatie html)
    while ((numBytes = recv(sockfd, recvBuff, sizeof(recvBuff), 0)) > 0) {	

	// Daca inca nu a fost gasita secventa de mai sus, vom verifica de fiecare data pana cand aceasta este gasita
	if (!headerGasit){

		// Utilizand functia strstr() noi vom compara buffer-ul care contine codul html, pentru a gasi secventa cautata
		// Daca aceasta este gasita, functia va returna un pointer catre prima aparitie a secventei "headerSecventa" definita anterior 
		char *headerPozSfarsit = strstr(recvBuff, headerSecventa);

		// Daca am gasit o valoarea din buffer-ul scanat anterior, o vom elimina utilizand codul din interiorul if-ului, daca nu, continuam
		if (headerPozSfarsit != NULL){

			// Stiind ca secventa noastra cautata  este formata din 4 octeti, vom putea elimina toate informatiile de la inceputul fisierului/raspunsului
			// pana la secventa cautata, astfel eliminand partea nedorita din fisierul HTML
			// variabila removedBytes va fi offset-ul 
			size_t removedBytes = headerPozSfarsit + headerLungime - recvBuff;
		

			// Utilizam un for() pentru a elimina secventa "\r\n\r\n"
			// Suprascriem header-ul copiind datele utile (de la indexul "removedBytes") direct la inceputul buffer-ului
			// In final, scadem dimensiunea header-ului din lungimea totala a datelor procesate

			for(size_t i = removedBytes; i < numBytes; i++){
				recvBuff[i - removedBytes] = recvBuff[i];
			}
			
			// Actualizarea dimensiunii buffer-ului
			// Schimbarea flag-ului pentru secventa cautata de la 0 la 1. Acum nu vom mai reintra in aceasta bucla if
			numBytes = numBytes - removedBytes;
			headerGasit = 1;
		}
	}

	// Initializam variabila in care se va stoca numarul total de biti din fiserul, care exclude carriage return-ul
        int cleanBytes = 0;

        // Aceasta bucla elimina carriage return ('\r'), deoarece astfel nu vor mai fi prezente caracterele speicale "^M" in fisierul creat "index.html"
        for (int i = 0; i < numBytes; i++) {
            if (recvBuff[i] != '\r') {
                cleanBuff[cleanBytes] = recvBuff[i];
                cleanBytes++;
            }
        }
	
	// Verificam daca bucla contine date valide dupa filtrare
        if (cleanBytes > 0) {

	    // Scriem in fisier doar daca acesta exista sau a fost deschis
            if (fp != NULL) {

		// Scrierea din buffer in fisier-ul "index.html"
                writtenBytes = fwrite(cleanBuff, 1, cleanBytes, fp);
   
		// Verificam daca s-a putut scrie intregul buffer in fisier
		// Daca nu se indeplineste conditia, atunci inseamna ca nu s-a putut scrie intregu-l buffer in fisier, din multiple cauze
		// (nu avem destul spatiu disponibil pentru stocarea acestuia)
                if(writtenBytes < cleanBytes){
                    fprintf(stderr,"\n Eroare la scrierea in fisier. \n");
                }
            }

            // Verificam eroarea si o trimitem catre client-ul care a trimis mesajul de cerere
	    // Trimitem codul HTML al codului catre clientul care cere acces la site, aflandu-se codul site-ului in interiorul aplicatiei ClientIPv4
            if (send(sockfd_client, cleanBuff, cleanBytes, 0) == -1) {
                perror("\n Eroare la trimiterea datelor catre client ");
                break;
            }
        }
    }

    // Verificam motivul iesirii din bucla de mai sus
    // Daca valorea returnata este -1 indica faptul ca s-a intamplat o eroare pe parcursul buclei while()
    if (numBytes == -1) {
        perror("\n Conexiunea la server s-a intrerupt ");
    }

    // Daca avem fisierul deschis, doar atunci il inchidem
    if (fp != NULL) fclose(fp); 
    close(sockfd);
    return 0;
}


int main(){
    int sockfd_listen; // Socket-ul principal
    int sockfd_client; // Socket-ul temporar

    struct sockaddr_in server_addr;
    struct sockaddr_storage client_addr_info; 

    // Variabila de tip socklen_t pentru a memora marimea structurii de adresa primita
    socklen_t client_addr_size;
   

    // Link-ul la care ne conectam pentru testarea codului. 
    // In cerinta din indrumatorul de miniproiect, aveam de folosit site-ul "www.speedtest6.com", dar acesta nu functiona.
    // Astfel am cautat un site care prezinta o adresa IPv6 si poate accepta si conexiuni prin portul 80 "HTTP"
    char *host = "ipv6.test-ipv6.com"; 

    // Salvam informatiile de pe site-ul accesat in fisierul numit "index.html"
    char *filename = "index.html"; 
        

    // Contorul de clienti, utilizat pentru afisaj
    int nr_client_curent = 0;


    // Se stocheaza informatiile din socket-ul creat, care sunt preluate utilizand functia socket()
    sockfd_listen = socket(AF_INET, SOCK_STREAM, 0);

    // Se testeaza conexiunea catre site utilizand socket-ul creat
    // In cazul in care nu se poate face legatura, functia va returna valoarea -1
    if(sockfd_listen == -1){
        perror("\n Nu s-a putut crea socket-ul principal ");
        return 0;
    }

    printf("\nSocket creat cu succes!\n");


    // Optiuni pentru socket
        // SO_REUSEADDR este important pentru restartarea serverului
        // Cand serverul se opreste, portul ramane blocat in starea TIME_WAIT pentru scurt timp
        // Aceasta optiune permite repornirea imediata a serverului pe acelasi port fara erori

    int da = 1; // Pentru SO_REUSEADDR, acesta cerand o adresa, nu putem pune valorea "1" direct ca si parametru pentru functia setsockopt()

    if (setsockopt(sockfd_listen, SOL_SOCKET, SO_REUSEADDR, &da, sizeof(da)) == -1) {
        perror("\nEroare la setarea optiunilor socket-ului ");
        close(sockfd_listen);
        return 0;
    }
    
    // Curatam memorie utilizand memset
    memset(&server_addr, 0, sizeof(server_addr));

    // Utilizam IPv4
    server_addr.sin_family = AF_INET;

    // INADDR_ANY îi spune serverului sa asculte pe toate interfetele de retea
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // htons() => Host TO Network Short 
    // Converteste numarul portului din formatul procesorului (little endian) în formatul standard al retelei (big endian)
    server_addr.sin_port = htons(SERVER_PORT);

    // Functia "bind()"
        // Asociem socket-ul creat cu adresa IP si portul configurate mai sus
       // Facem cast la (struct sockaddr *) deoarece bind() este o functie generica ce accepta orice tip de adresa, nu doar sockaddr_in

    if (bind(sockfd_listen, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1){
        perror("\nEroare la asocierea portului ");
        close(sockfd_listen);
        return 0;
    }

    printf("\nPortul dvs este: %d.\n", SERVER_PORT);

    // Functia "listen()"
        // Marcam socket-ul ca fiind pasiv (asteapta conexiunile)

    if (listen(sockfd_listen, 10) == -1){
        perror("\nEroare in timpul ascultarii pe port ");
        close(sockfd_listen);
        return 0;
    }

    printf("\nSe asteapta conexiuni la server.....\n");

    // MAIN
    while(1){ 

        // Salvam valoarea marimii variabile client_addr_info intr-o variabila separata pt lizibilitate
        client_addr_size = sizeof(client_addr_info);

        // Functia accept() este o functie blocanta, adica face ca bucla while() sa se opresaca pana 
	// in momentul in care functia accept() primeste o conexiune pe socket
        sockfd_client = accept(sockfd_listen, (struct sockaddr *)&client_addr_info, &client_addr_size);
    
        if (sockfd_client == -1) {
            perror("\nEroare la acceptarea conexiunii client ");
            continue;
        }
        
        // Incrementam contorul clientilor
        nr_client_curent++;
        
        printf("\nS-a conectat clientul cu Nr. %d. Se creeaza procesul secundar...\n", nr_client_curent);

        // Functia "fork()"
        // fork() cloneaza procesul curent ( Un proces => 2 procese) 
        // Functia fork() poate crea 2^n fire concurente, n fiind numarul de apelari
        // Returneaza 0 in procesul "copil" si ID-ul copilului (o valoarea int) in procesul "parinte"

        if (fork() == 0) {

            // Zona proces "copil"
            // Acest cod ruleaza independent si paralel cu parintele    
            // Copilul nu are nevoie sa asculte dupa alti clienti, asa ca inchide socket-ul principal

            close(sockfd_listen);
            
	    // Adaugam terminatorul pentru a putea face comparatie de siruri de caractere
	    // Mesajul transmis de catre ClientIPv4 va fi salvat in buff
            char buff[BUFFER];
            ssize_t numBytes;
        
            while(1){
            
                numBytes  = recv(sockfd_client, buff, BUFFER - 1, 0);
            
		// Daca avem ca valoare de return -1, inseamna ca avem o eroare la fct recv()
                if (numBytes == -1){
                    printf("\nAplicatia clientului cu Nr. \"%d\" a fost inchisa. \n", nr_client_curent);
		    break;
                }
		// Daca avem 0, inseamna ca clientul s-a deconectat
                else if (numBytes == 0){
                    printf("\nClientul cu Nr. \"%d\" s-a deconectat. \n", nr_client_curent);
                    break;
                }
                else{
            
		    // Utilizam terminatorul pentru a putea inchide mesajul trimis, fiindca comanda din aplicatie nu-l pune automat
                    buff[numBytes] = '\0';

                    printf("\nClientul cu Nr. \"%d\" a trimis comanda: \"%s\" . \n", nr_client_curent, buff);

		  
		    // Utilizam comanda "07#" pentru testarea codului 
		    // Daca se trimite aceasta, vom rula functia de procesare, altfel, vom trimite mesajul "Comanda neimplementata"
                    if (strcmp(buff, "07#") == 0){
                        procesare_client(sockfd_client, host, filename);	
                    }
                    else{
			// Pentru a avea un cod mai lizibil, vom salva mesajul si vom trimite lungimea lui catre functia send, pentru a nu trebui sa numaram caracter cu caracter mesajul scris
			const char *mesajNeimplementat = "\nComanda neimplementata.\n";

                        if(send(sockfd_client, mesajNeimplementat, strlen(mesajNeimplementat) ,0) == -1){
                            perror("\n Eroare la trimiterea mesajului catre client. \n");
                        }
                    }
                }
            }

            // Procesul copil isi inchide socket-ul clientului si isi termina executia
            close(sockfd_client);
            exit(0); 
        } else {
            // Zona proces "parinte"
 
            // Procesul parinte nu este intr-o conexiune directa cu clientul, ci procesul copil va face conexiunea
            // Trebuie sa inchida descriptorul de socket al clientului in propriul proces, altfel conexiunea 
            // nu se va inchide corect, si ar aparea un leak de memorie

            close(sockfd_client); 
        }
    }

    // In cazul in care iesim din bucla loop din cauza unei erori, vom opri socket-ul principal

    close(sockfd_listen);
    return 0;
}
