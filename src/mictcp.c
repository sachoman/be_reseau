#include <mictcp.h>
#include <api/mictcp_core.h>

#define nb_envois_max 10


typedef struct liste_sock_addr{
    mic_tcp_sock sock_local;
    mic_tcp_sock_addr addr_distante;
    int pe_a;
    struct liste_sock_addr * suivant;
}liste_sock_addr;

liste_sock_addr * liste_socket_addresses = NULL;
liste_sock_addr * pointeur_courant = NULL;

void fd_to_pointeur(int fd,liste_sock_addr ** pointeur_liste_socket){
    if  ((*pointeur_liste_socket) == NULL){
        pointeur_courant = NULL;
    }
    else{
        if ((((*pointeur_liste_socket)->sock_local).fd) == fd){
            pointeur_courant = (*pointeur_liste_socket);
        }
        else{
            fd_to_pointeur(fd, &((*pointeur_liste_socket)->suivant));
        }
    }
}

void parcours_liste(liste_sock_addr * pointeur_liste_socket){
    if  (pointeur_liste_socket->suivant ==NULL){
        printf("fin liste\n");
    }
    else{
            printf("num socket: %d\n",(pointeur_liste_socket->sock_local).fd);
            parcours_liste(pointeur_liste_socket->suivant);
        }
}

int add_sock_list(int num, liste_sock_addr ** pointeur_liste_socket){
    if ((*pointeur_liste_socket) == NULL){
        //printf("if %d\n",*pointeur_liste_socket);
        printf("num %d\n ",num);
        (*pointeur_liste_socket) = malloc(sizeof(liste_sock_addr));
        //printf("attribution %d\n",*pointeur_liste_socket);
        ((*pointeur_liste_socket)->sock_local).fd = num;
        ((*pointeur_liste_socket)->sock_local).state = CLOSED;
        (*pointeur_liste_socket)->suivant = NULL;
        return num;
    }
    else{
        //printf("else %d\n",*pointeur_liste_socket);
        add_sock_list(num+1, &((*pointeur_liste_socket)->suivant));
    }
    return -1;
}

/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
int mic_tcp_socket(start_mode sm)
{
   int result = -1;
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   result = initialize_components(sm); /* Appel obligatoire */
   if (result ==-1){
       return -1;
   }
   set_loss_rate(0);
   result = add_sock_list(0,&liste_socket_addresses);
   return result;
}

/*
 * Permet d’attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d’échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr)
{
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   fd_to_pointeur(socket,&liste_socket_addresses);
   if (pointeur_courant == NULL){
       printf("erreur bind\n");
       printf("SOCKET CHERCHE  %d\n", socket);
       parcours_liste(liste_socket_addresses);
       return -1;
   }
   (pointeur_courant->sock_local).addr = addr;
   return 0;
}

/*
 * Met le socket en état d'acceptation de connexions
 * Retourne 0 si succès, -1 si erreur
 */

int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    fd_to_pointeur(socket,&liste_socket_addresses);
    if (pointeur_courant == NULL){
       printf("erreur connexion\n");
       return -1;
    }
    (pointeur_courant->addr_distante) = *addr;
    int j = 0;
    mic_tcp_pdu synack;
    j = IP_recv(&synack, addr, 4);
    if ((j== -1) || ((synack.header).syn != 1)) {
        printf("connexion non détectée\n");
        exit(1);
    }
    int i=-1;
    synack.header.syn = 1;
    synack.header.ack = 1;
    synack.header.source_port = (pointeur_courant->sock_local).addr.port;
    synack.header.dest_port = (pointeur_courant->addr_distante).port;
    mic_tcp_pdu ack;
    int nb_envois = 0;
    while (i==-1){
        if (nb_envois > nb_envois_max){
            printf("erreur trop d'envois avec échec \n");
            exit(1);
        }
        IP_send(synack,*addr);
                nb_envois ++;
        i = IP_recv(&ack, addr, 4);
        if ((ack.header).ack ==0){
            i = -1;
        }
    }   
    (pointeur_courant -> pe_a)=0;
    (pointeur_courant -> sock_local).state = ESTABLISHED;
    return 0;
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */

int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    fd_to_pointeur(socket,&liste_socket_addresses);
    if (pointeur_courant == NULL){
       printf("erreur connexion\n");
       return -1;
    }
    (pointeur_courant->addr_distante) = addr;
    int i=-1;
    mic_tcp_header header_syn;
    header_syn.syn = 1;
    header_syn.source_port = (pointeur_courant->sock_local).addr.port;
    header_syn.dest_port = (pointeur_courant->addr_distante).port;
    mic_tcp_pdu syn;
    syn.header=header_syn;
    mic_tcp_pdu synack;
    int nb_envois = 0;
    while (i==-1){
        if (nb_envois > nb_envois_max){
            printf("erreur trop d'envois avec échec \n");
            exit(1);
        }
        IP_send(syn,addr);
        nb_envois ++;
        i = IP_recv(&synack, &addr, 4);
        if (i!=-1){
            if ((synack.header.ack != 1) || (synack.header.syn !=1)){
                i = -1;
            }
        }
    }
    syn.header.ack = 1;
    IP_send(syn,addr);
    (pointeur_courant -> pe_a)=0;
    (pointeur_courant -> sock_local).state = ESTABLISHED;
    return 0;
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send (int mic_sock, char* mesg, int mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    /* create pdu*/
    mic_tcp_pdu pdu;
    mic_tcp_header header;
    fd_to_pointeur(mic_sock,&liste_socket_addresses);
    if (pointeur_courant == NULL){
       printf("erreur send\n");
       return -1;
    }
    header.source_port = (pointeur_courant->addr_distante).port; /* numéro de port source */
    /*on a les infos du connect

    connect -> dest_port;
    connect -> addr_dest;

    seq_num; numéro de séquence 
    ack_num; numéro d'acquittement 
    syn; flag SYN (valeur 1 si activé et 0 si non) 
    ack; flag ACK (valeur 1 si activé et 0 si non) 
    fin; flag FIN (valeur 1 si activé et 0 si non) 
    */
    /**/
    mic_tcp_payload payload;
    payload.data = mesg;
    payload.size= mesg_size;
    /**/
    pdu.payload = payload;
    pdu.header = header;
    /**/
    int nb_envoye;
    if ((nb_envoye = IP_send(pdu, (pointeur_courant->addr_distante)))==-1){
        printf("erreur envoi\n");
        exit(1);
    }
    return nb_envoye;
}

/*
 * Permet à l’application réceptrice de réclamer la récupération d’une donnée
 * stockée dans les buffers de réception du socket
 * Retourne le nombre d’octets lu ou bien -1 en cas d’erreur
 * NB : cette fonction fait appel à la fonction app_buffer_get()
 */
int mic_tcp_recv (int socket, char* mesg, int max_mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    mic_tcp_payload payload;
    payload.data = mesg;
    payload.size = max_mesg_size;
    int taille_recue;
    taille_recue = app_buffer_get(payload);
    return taille_recue;
}

/*
 * Permet de réclamer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close (int socket)
{
    printf("[MIC-TCP] Appel de la fonction :  "); printf(__FUNCTION__); printf("\n");
    return 0;
}

/*
 * Traitement d’un PDU MIC-TCP reçu (mise à jour des numéros de séquence
 * et d'acquittement, etc.) puis insère les données utiles du PDU dans
 * le buffer de réception du socket. Cette fonction utilise la fonction
 * app_buffer_put().
 */
void process_received_PDU(mic_tcp_pdu pdu, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    app_buffer_put(pdu.payload);
}
