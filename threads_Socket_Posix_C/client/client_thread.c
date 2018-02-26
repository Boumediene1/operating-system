/*  
 * Alexandre Deneault
 * 20044305
 * 
 * Boumediene Boukharouba
 * 20032279
 */


/* This `define` tells unistd to define usleep and random.  */
#define _XOPEN_SOURCE 500
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "client_thread.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <strings.h>
#include <string.h>
#include <arpa/inet.h>
#include <semaphore.h> 

int port_number = -1;
int num_clients = -1;
int num_request_per_client = -1;
int num_resources = -1;
int *provisioned_resources = NULL;

// Variable d'initialisation des threads clients.
unsigned int count = 0; 


// Variable du journal.
// Nombre de requête acceptée (ACK reçus en réponse à REQ)
unsigned int count_accepted = 0; 

// Nombre de requête en attente (WAIT reçus en réponse à REQ)
unsigned int count_on_wait = 0;

// Nombre de requête refusée (REFUSE reçus en réponse à REQ)
unsigned int count_invalid = 0;

// Nombre de client qui se sont terminés correctement (ACC reçu en réponse à END)
unsigned int count_dispatched = 0;

// Nombre total de requêtes envoyées.                          
unsigned int request_sent = 0; 



//Attente maximum pour tenter de se connecter
int attenteMax = 30;

sem_t sem;
int* alloue;
int* max;


// Vous devez modifier cette fonction pour faire l'envoie des requêtes
// Les ressources demandées par la requête doivent être choisies aléatoirement
// (sans dépasser le maximum pour le client). Elles peuvent être positives
// ou négatives.
// Assurez-vous que la dernière requête d'un client libère toute les ressources
// qu'il a jusqu'alors accumulées.
void
send_request (int client_id, int request_id, int socket_fd)
{

  // TP2 TODO

  //Imprime le numéro de la requête.
  fprintf (stdout, "Client %d envoi sa requête # %d.\n", client_id,
	   request_id);
  //Met à jour le nombre total de requête.
  sem_wait(&sem);
  request_sent += 1;
  sem_post(&sem);
  //Initialise la requête.
  char* msg = malloc((80 + 1) * sizeof(char));
  char num[10];
  int j = 1;
  memset(msg, 0, 81);
  strcat(msg, "REQ ");
  int* ressource = malloc(num_resources * sizeof(int));
  ////Determine le nombre de chaque ressource à demander.
  //Libère les ressource tenues lors de la dernière requête.
  if (request_id == num_request_per_client - 1) {
    for (int i = 0; i < num_resources; i++) {
      memset (&num, 0, sizeof (num));
      ressource[i] = -1 * alloue[(client_id * num_resources) + i];
      sprintf(num, "%d", ressource[i]);
      if (strlen(msg) + strlen(num) + 2 > 80 * j) {
        j++;
        msg = realloc(msg, (80*j + 1) * sizeof(char));
        memset(&msg[80*(j-1)], 0, 81);
      }
      strcat(strcat(msg, num), " ");
    }
  //Choisit au hasard.
  } else {
    for (int i = 0; i < num_resources; i++) {
      memset (&num, 0, sizeof (num));
      int pos = (client_id * num_resources) + i;
      ressource[i] = (random() % (max[pos] + 1)) - alloue[pos];
      sprintf(num, "%d", ressource[i]);
      if (strlen(msg) + strlen(num) + 2 > 80 * j) {
        j++;
        msg = realloc(msg, (80*j + 1) * sizeof(char));
        memset(&msg[80*(j-1)], 0, 81);
      }
      strcat(strcat(msg, num), " ");
    }
  }
  strcat(msg, "\n");
  //Envoi la requête.
  printf("Client %d: ", client_id);
  printf(msg);
  int attente = time(NULL);
  send(socket_fd, msg, strlen(msg), 0);
  free(msg);
  // Attend la réponse du serveur.
  char rep[80];
  memset (&rep, 0, sizeof (rep));
  recv(socket_fd, rep, sizeof(rep), 0);
  ////Analyse la réponse du serveur.
  //Vérifie l'attente
  bool attendu;
  if (attente + 30 > time(NULL)) {
    attendu = false;
  } else {
    attendu = true;
  }
  sem_wait(&sem);
  //Requête accepté.
  if (memcmp(rep, "ACK", 3) == 0) {
    printf ("Client %d: Requête acceptée.\n", client_id);
    //Met à jour les ressources tenues.
    for (int i = 0; i < num_resources; i++) {
      alloue[(client_id * num_resources) + i] += ressource[i];
    }
    if (!attendu) {
      count_accepted += 1;
    } else {
      count_on_wait += 1;
    }
  //Requête refusé.
  } else if (memcmp(rep, "ERR", 3) == 0) {
    printf("Client %d: ", client_id);
    printf(rep);
    count_invalid += 1;
  //Réponse invalide.
  } else {
    printf("Client %d: Réponse du serveur erronée.\n", client_id);
    count_invalid += 1;
  }
  sem_post(&sem);
  free(ressource);

  // TP2 TODO:END

}


void *
ct_code (void *param)
{
  client_thread *ct = (client_thread *) param;

  // TP2 TODO
  // Vous devez ici faire l'initialisation des petits clients (`INI`).

  //Ouvre une connexion.
  int sServeur = ouvreConnexion();
  if (sServeur < 0) {
    printf("Le client %d n'a pas réussis à se connecter au serveur.", ct->id);
    pthread_exit (NULL);
  }

  //Envoi "INI res res ..." au serveur.
  char* msg = malloc((80 + 1) * sizeof(char));
  char num[10];
  memset (msg, 0, 81);
  int j = 1;
  strcat(msg, "INI ");
  for (int i = 0; i < num_resources; i++) {
    memset (&num, 0, sizeof (num));
    int resI = random() % (provisioned_resources[i] + 1);
    max[(ct->id * num_resources) + i] = resI;
    alloue[(ct->id * num_resources) + i] = 0;
    sprintf(num, "%d", resI);
    if (strlen(msg) + strlen(num) + 2 > 80 * j) {
      j++;
      msg = realloc(msg, (80*j + 1) * sizeof(char));
      memset(&msg[80*(j-1)], 0, 81);
    }
    strcat(strcat(msg, num), " ");
  }
  strcat(msg, "\n");
  printf("Client %d: ", ct->id);
  printf(msg);
  send(sServeur, msg, strlen(msg), 0);
  free(msg);
  // Reçoit la réponse du serveur.
  char rep[80];
  memset (&rep, 0, sizeof (rep));
  recv(sServeur, rep, sizeof(rep), 0);
  //Analyse la réponse du serveur.
  //Client initialisé.
  if (memcmp(rep, "ACK", 3) == 0) {
    printf ("Client %d initialisé.\n", ct->id);
  //Erreur d'initialisation.
  } else if (memcmp(rep, "ERR", 3) == 0) {
    printf("Client %d: ", ct->id);
    printf(rep);
    if (memcmp(rep, "ERR Connexion déjà initialisé.\n", 3) != 0) {
      pthread_exit (NULL);
    }
  //Réponse du serveur erronée.
  } else {
    perror("Réponse du serveur erronée.\n");
    pthread_exit (NULL);
  }

  // TP2 TODO:END

  for (unsigned int request_id = 0; request_id < num_request_per_client;
       request_id++)
    {

      // TP2 TODO
      // Vous devez ici coder, conjointement avec le corps de send request,
      // le protocole d'envoi de requête.

      send_request (ct->id, request_id, sServeur);

      // TP2 TODO:END

      /* Attendre un petit peu (0s-0.1s) pour simuler le calcul.  */
      usleep (random () % (100 * 1000));
      /* struct timespec delay;
       * delay.tv_nsec = random () % (100 * 1000000);
       * delay.tv_sec = 0;
       * nanosleep (&delay, NULL); */
    }

  //Envoi CLO pour fermer la connection.
  char msg2[80];
  memset (&msg2, 0, sizeof (msg2));
  strcat(msg2, "CLO\n");
  printf("Client %d: ", ct->id);
  printf(msg2);
  send(sServeur, msg2, strlen(msg2), 0);
  // Reçoit la réponse du serveur.
  memset (&rep, 0, sizeof (rep));
  recv(sServeur, rep, sizeof(rep), 0);
  ////Analyse la réponse du serveur.
  //Déconnexion accepté
  if (memcmp(rep, "ACK", 3) == 0) {
    printf ("Client %d déconnecté.\n", ct->id);
    sem_wait(&sem);
    count_dispatched += 1;
    sem_post(&sem);
  //Erreur de fermeture
  } else if (memcmp(rep, "ERR", 3) == 0) {
    printf(&(rep[4]));
  //Reéponse du serveur erronée.
  } else {
    perror("Réponse du serveur erronée.\n");
  }
  //Ferme la connexion.
  close(sServeur);

  pthread_exit (NULL);
}


// 
// Vous devez changer le contenu de cette fonction afin de régler le
// problème de synchronisation de la terminaison.
// Le client doit attendre que le serveur termine le traitement de chacune
// de ses requêtes avant de terminer l'exécution. 
//
void
ct_wait_server (int num_clients, client_thread *client_threads)
{

  // TP2 TODO

  //Attend que les clients terminent leurs requêtes.
  for(int i = 0; i < num_clients; i++) {
    void* valeur;
    pthread_join(client_threads[i].pt_tid, &valeur);
  }
  //Libère les ressources
  free(max);
  free(alloue);
  sem_destroy(&sem);
  ////Envoi le END
  //Ouvre une connexion.
  int sServeur = ouvreConnexion();
  if (sServeur < 0) {
    exit(-1);
  }
  //Envoi "END" au serveur.
  char msg[80];
  memset (&msg, 0, sizeof (msg));
  strcat(msg, "END\n");
  printf("Client: ");
  printf(msg);
  send(sServeur, msg, strlen(msg), 0);
  // Reçoit la réponse du serveur.
  char rep[80];
  memset (&rep, 0, sizeof (rep));
  recv(sServeur, rep, sizeof(rep), 0);
  ////Analyse la réponse du serveur.
  //Fermeture accepté.
  if (memcmp(rep, "ACK", 3) == 0) {
    printf ("Client: Le serveur s'est fermé avec succès.\n");
  //Erreur à la fermeture.
  } else if (memcmp(rep, "ERR", 3) == 0) {
    printf("Client: ");
    printf(rep);
    exit(-1);
  //Réponse du serveur erroné.
  } else {
    perror("Réponse du serveur erronée.\n");
    exit(-1);
  }

  //sleep (4);

  // TP2 TODO:END

}


void
ct_init (client_thread * ct)
{
  ct->id = count++;
}

void
ct_create_and_start (client_thread * ct)
{
  pthread_attr_init (&(ct->pt_attr));
  pthread_create (&(ct->pt_tid), &(ct->pt_attr), &ct_code, ct);
  //pthread_detach (ct->pt_tid);
}

//
// Affiche les données recueillies lors de l'exécution du
// serveur.
// La branche else ne doit PAS être modifiée.
//
void
st_print_results (FILE * fd, bool verbose)
{
  if (fd == NULL)
    fd = stdout;
  if (verbose)
    {
      fprintf (fd, "\n---- Résultat du client ----\n");
      fprintf (fd, "Requêtes acceptées: %d\n", count_accepted);
      fprintf (fd, "Requêtes : %d\n", count_on_wait);
      fprintf (fd, "Requêtes invalides: %d\n", count_invalid);
      fprintf (fd, "Clients : %d\n", count_dispatched);
      fprintf (fd, "Requêtes envoyées: %d\n", request_sent);
    }
  else
    {
      fprintf (fd, "%d %d %d %d %d\n", count_accepted, count_on_wait,
	       count_invalid, count_dispatched, request_sent);
    }
}


//
//Initialise le serveur.
//
void initServeur(){
  
  //Ouvre la connexion.
  int sServeur = ouvreConnexion();
  if (sServeur < 0) {
    exit(-1);
  }
  //Envoi "BEG nbRes" au serveur.
  char num[10], msg[80];
  memset (&msg, 0, sizeof (msg));
  sprintf(num, "%d", num_resources);
  strcat(strcat(strcat(msg, "BEG "), num), "\n");
  printf("Client: ");
  printf(msg);
  send(sServeur, msg, strlen(msg), 0);
  // Reçoit la réponse du serveur.
  char rep[80];
  memset (&rep, 0, sizeof (rep));
  recv(sServeur, rep, sizeof(rep), 0);
  ////Analyse la réponse du serveur.
  //Serveur initialisé.
  if (memcmp(rep, "ACK", 3) == 0) {
    printf ("Client: Serveur initialisé.\n");
  //Erreur d'initialisation.
  } else if (memcmp(rep, "ERR", 3) == 0) {
    printf("Client: ");
    printf(rep);
    exit(-1);
  //Réponse du serveur invalide.
  } else {
    perror("Réponse du serveur erronée.\n");
    exit(-1);
  }

  //Provisionne les ressources au serveur.
  char* msg2 = malloc((80 + 1) * sizeof(char));
  memset(msg2, 0, 81);
  int j = 1;
  strcat(msg2, "PRO ");
  for (int i = 0; i < num_resources; i++) {
    memset (&num, 0, sizeof (num));
    sprintf(num, "%d", provisioned_resources[i]);
    if (strlen(msg2) + strlen(num) + 2 > 80 * j) {
      j++;
      msg2 = realloc(msg2, (80*j + 1) * sizeof(char));
      memset(&msg2[80*(j-1)], 0, 81);
    }
    strcat(strcat(msg2, num), " ");
  }
  strcat(msg2, "\n");
  printf("Client: ");
  printf(msg2);
  send(sServeur, msg2, strlen(msg2), 0);
  free(msg2);
  //Attend la réponse du serveur.
  memset (&rep, 0, sizeof (rep));
  recv(sServeur, rep, sizeof(rep), 0);

  ////Analyse la réponse du serveur.
  //Ressources accepté.
  if (memcmp(rep, "ACK", 3) == 0) {
    printf ("Client: Ressource provisionnées.\n");
  //Erreur de provisionnement.
  } else if (memcmp(rep, "ERR", 3) == 0) {
    printf("Client: ");
    printf(rep);
    exit(-1);
  //Réponse du serveur invalide.
  } else {
    perror("Réponse du serveur erronée.\n");
    exit(-1);
  }
  //Demande la mémoire pour les structures.
  alloue = malloc(num_clients * num_resources * sizeof(int));
  max = malloc(num_clients * num_resources * sizeof(int));
  sem_init(&sem, 0, 1);
  // Ferme la connexion.
  close(sServeur);

}



//
//Ouvre une connexion.
//Retourne le descripteur ou -1 s'il y a eu une erreur.
//
int ouvreConnexion() {

  //Ouvre le socket.
  int sServeur = socket (AF_INET, SOCK_STREAM, 0);
  if (sServeur < 0) {
    perror ("Erreur d'ouverture.");
    return(-1);
  }
  //Remplis la structure contenant l'addresse.
  struct addrinfo info;
  struct addrinfo* addr;
  memset (&info, 0, sizeof (info));
  info.ai_family = AF_INET;
  info.ai_socktype = SOCK_STREAM;
  info.ai_flags = AI_PASSIVE;
  char port[10];
  sprintf(port, "%d", port_number);

  if(getaddrinfo(NULL, port, &info, &addr) < 0) {
    perror("Erreur de recherche d'addresse.");
    return(-1);
  }
  //Tente de se connecter au serveur.
  int tempsMax = time (NULL) + attenteMax;
  while (connect(sServeur, addr->ai_addr, addr->ai_addrlen) < 0) {
    if (tempsMax < time(NULL)) {
      perror("Erreur de connexion.");
     return(-1);
    }
  }

  freeaddrinfo(addr);
  return sServeur;

}
