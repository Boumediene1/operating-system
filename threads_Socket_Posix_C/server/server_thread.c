/*  
 * Alexandre Deneault
 * 20044305
 * 
 * Boumediene Boukharouba
 * 20032279
 */



#define _XOPEN_SOURCE 700   /* So as to allow use of `fdopen` and `getline`.  */

#include "server_thread.h"

#include <netinet/in.h>
#include <netdb.h>

#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <time.h>
#include <semaphore.h> 

enum { NUL = '\0' };

enum {
  /* Configuration constants.  */
   max_wait_time = 30,
   server_backlog_size = 5
};

unsigned int server_socket_fd;

// Variable du journal.
// Nombre de requêtes acceptées immédiatement (ACK envoyé en réponse à REQ).
unsigned int count_accepted = 0;

// Nombre de requêtes acceptées après un délai (ACK après REQ, mais retardé).
unsigned int count_wait = 0;

// Nombre de requête erronées (ERR envoyé en réponse à REQ).
unsigned int count_invalid = 0;

// Nombre de clients qui se sont terminés correctement
// (ACK envoyé en réponse à CLO).
unsigned int count_dispatched = 0;

// Nombre total de requête (REQ) traités.
unsigned int request_processed = 0;

// Nombre de clients ayant envoyé le message CLO.
unsigned int clients_ended = 0;

// TODO: Ajouter vos structures de données partagées, ici.
int nbRessources;
int* disponibleMax;
int* disponible;
int* max;
int* alloue;
int idConnecte;
int nbConnecte;
sem_t sem;

int tempsAttente = 1;


void
st_init ()
{
  // TODO
  // Attend la connection d'un client et initialise les structures pour
  // l'algorithme du banquier.

  struct sockaddr_in addrClient;
  memset (&addrClient, 0, sizeof (addrClient));
  socklen_t len = sizeof (addrClient);
  int sClient = -1;
  int tempsMax = time(NULL) + max_wait_time;
  char msg[80];
  memset (&msg, 0, sizeof (msg));

  //Attend une première connexion
  while (sClient < 0) {
    sClient = accept(server_socket_fd, (struct sockaddr*) &addrClient,
					 &len);
    if(time(NULL) >= tempsMax) {
      perror("Délai dépassé. Serveur non initialisé\n");
      exit(-1);
    }
  }
  //Reçoit la requête.
  char rep[80];
  memset (&rep, 0, sizeof(rep));
  recv(sClient, rep, sizeof(rep), 0);
  printf("Serveur: reçu: ");
  printf(rep);
  ////Analyse la requête.
  //Commande invalide.
  if (memcmp(rep, "BEG", 3) != 0) {
    strcat(msg, "ERR Commande invalide, serveur non initialisé\n");
    send(sClient, msg, strlen(msg), 0);
    exit(-1);
  }
  if (rep[strlen(rep)-1] != '\n') {
    strcat(msg, "ERR Commande incomplète, serveur non initialisé\n");
    send(sClient, msg, strlen(msg), 0);
    exit(-1);
  }
  //Extrait les paramètres.
  int* paramEntier = separeParam(&(rep[4]));
  //Vérifie le nombre de paramètres.
  if (paramEntier == NULL || paramEntier[0] != 1) {
    strcat(msg, "ERR Nombre de paramètres erroné, serveur non initialisé\n");
    send(sClient, msg, strlen(msg), 0);
    exit(-1);
  }
  //Initialise le nombre de ressources.
  nbRessources = paramEntier[1];
  free(paramEntier);
  //Répond à la requête.
  printf("Serveur: Serveur initialisé.\n");
  strcat(msg, "ACK\n");
  send(sClient, msg, strlen(msg), 0);
  memset (&msg, 0, sizeof(msg));

  //Attend le provisionnement des ressources.
  char* rep2 = malloc((80 + 1) * sizeof(char));
  memset(rep2, 0, 81);
  int n = recv(sClient, rep2, 80, 0);
  int i = 1;
  while (n == 80 && rep2[80*i] != '\n') {
    i++;
    rep2 = realloc(rep2, (80*i + 1) * sizeof(char));
    memset(&rep2[80*(i-1)+1], 0, 80);
    n = recv(sClient, &rep2[80*(i-1)], 80, 0);
  }
  printf("Serveur: reçu: ");
  printf(rep2);
  ////Analyse le provisionnement des ressources.
  //Commande invalide.
  if (memcmp(rep2, "PRO", 3) != 0) {
    free(rep2);
    strcat(msg, "ERR Commande invalide, serveur non initialisé\n");
    send(sClient, msg, strlen(msg), 0);
    exit(-1);
  }
  if (rep2[strlen(rep2)-1] != '\n') {
    free(rep2);
    strcat(msg, "ERR Commande incomplète, serveur non initialisé\n");
    send(sClient, msg, strlen(msg), 0);
    exit(-1);
  }
  //Extrait les paramètres.
  paramEntier = separeParam(&(rep2[4]));
  //Vérifie le nombre de paramètres.
  if (paramEntier == NULL || paramEntier[0] != nbRessources) {
    free(rep2);
    strcat(msg, "ERR Nombre de paramètres erroné, serveur non initialisé\n");
    send(sClient, msg, strlen(msg), 0);
    exit(-1);
  }
  //Initialise les ressources disponibles.
  disponible = malloc(nbRessources * sizeof(int));
  disponibleMax = malloc(nbRessources * sizeof(int));
  for (int i = 0; i < nbRessources; i++) {
    disponible[i] = paramEntier[i+1];
    disponibleMax[i] = paramEntier[i+1];
  }
  free(rep2);
  free(paramEntier);
  sem_init(&sem, 0, 1);
  idConnecte = 0;
  nbConnecte = 0;
  max = malloc(nbRessources * sizeof(int));
  alloue = malloc(nbRessources * sizeof(int));
  //Répond au client.
  printf("Serveur: Ressources reçu.\n");
  strcat(msg, "ACK\n");
  send(sClient, msg, strlen(msg), 0);
  
  //Ferme la connexion.
  close(sClient);

  // END TODO
}



void
st_process_requests (server_thread * st, int socket_fd)
{
  // TODO: Remplacer le contenu de cette fonction
  FILE *socket_r = fdopen (socket_fd, "r");
  bool init = false;
  char msg[80];
  int id;
  
  while (true) {
    //Reçoit la requête.
    memset (&msg, 0, sizeof (msg));
    char cmd[4] = {NUL, NUL, NUL, NUL};
    if (!fread (cmd, 3, 1, socket_r)) {
      continue;
    }
    char *args = NULL; size_t args_len = 0;
    ssize_t cnt = getline (&args, &args_len, socket_r);
    if (!args || cnt < 1 || args[cnt - 1] != '\n') {
      printf ("Serveur %d: commande incomplète: %s!\n", st->id, cmd);
      strcat(msg, "ERR Comande incomplète.\n");
      send(socket_fd, msg, strlen(msg), 0);
      continue;
    }
    
    int attente = time(NULL);  //Pour vérifier l'attente
    printf ("Serveur %d: reçu: %s%s", st->id, cmd, args);

    //////Analyse la requête

    //END
    if (memcmp(cmd, "END", 3) == 0) {
      sem_wait(&sem);
      //Retorne une erreur si des clients sont encore connectés.
      if (nbConnecte != 0) {
        strcat(msg, "ERR Clients encore connecté. Connexion fermée.\n");
        sem_post(&sem);
        send(socket_fd, msg, strlen(msg), 0);
        free(args);
        break;
      }
      //N'accepte plus de connection
      accepting_connections = false;
      printf("Serveur fermé avec succès.\n");
      sem_post(&sem);
      strcat(msg, "ACK\n");
      send(socket_fd, msg, strlen(msg), 0);
      free(args);
      break;
    }
    //fin END


    //extrait les paramètre
    int* paramEntier = separeParam(args);
    free(args);


    //CLO
    if (memcmp(cmd, "CLO", 3) == 0 && init) {
      sem_wait(&sem);
      clients_ended += 1;
      sem_post(&sem);
      bool ressLib = true;
      //Vérifie que les ressources sont libéré.
      sem_wait(&sem);
      for (int i = 0; i < nbRessources; i++) {
        max[(id*nbRessources)+i] = 0;
        if (alloue[(id*nbRessources)+i] != 0) {
          ressLib = false;
          disponible[i] += alloue[(id*nbRessources)+i];
          alloue[(id*nbRessources)+i] = 0;
        }
      }
      nbConnecte -= 1;
      sem_post(&sem);
      //Envoi les messages d'erreur
      if (paramEntier != NULL) {
        free(paramEntier);
        strcat(msg, "ERR Nombre de paramètre erroné. Connexion fermée.\n");
        send(socket_fd, msg, strlen(msg), 0);
        break;
      }
      if (!ressLib) {
        strcat(msg, "ERR Ressources non libéré. Connexion fermée.\n");
        send(socket_fd, msg, strlen(msg), 0);
        break;
      }
      //Connexion fermé avec succès.
      sem_wait(&sem);
      count_dispatched += 1;
      sem_post(&sem);
      printf("Serveur %d: Connexion fermé.\n", st->id);
      strcat(msg, "ACK\n");
      send(socket_fd, msg, strlen(msg), 0);
      break;
    }
    //fin CLO


    //Vérifie le nombre de paramètres
    if (paramEntier == NULL || paramEntier[0] != nbRessources) {
      printf("Serveur %d: Nombre de paramètres erroné.", st->id);
      strcat(msg, "ERR Nombre de paramètres erroné.\n");
      send(socket_fd, msg, strlen(msg), 0);
      continue;
    }


    //INI
    if (memcmp(cmd, "INI", 3) == 0) {
      if (init) {
        strcat(msg, "ERR Connexion déjà initialisé.\n");
        send(socket_fd, msg, strlen(msg), 0);
        free(paramEntier);
        continue;
      }
      //Vérifie les ressources max.
      sem_wait(&sem);
      for (int i = 0; i < nbRessources; i++) {
        if (disponibleMax[i] < paramEntier[i+1]) {
          strcat(msg, "ERR Ressource Max trop grande.\n");
          send(socket_fd, msg, strlen(msg), 0);
          free(paramEntier);
          sem_post(&sem);
          continue;
        }
      }
      //Met les données à jour.
      id = idConnecte;
      idConnecte += 1;
      nbConnecte += 1;
      max = realloc(max, idConnecte * nbRessources * sizeof(int));
      alloue = realloc(alloue, idConnecte * nbRessources * sizeof(int));
      for (int i = 0; i < nbRessources; i++) {
        max[(id*nbRessources)+i] = paramEntier[i+1];
        alloue[(id*nbRessources)+i] = 0;
      }
      sem_post(&sem);
      //Accepte la connexion.
      printf("Serveur %d: Connexion initialisé.\n", st->id);
      strcat(msg, "ACK\n");
      send(socket_fd, msg, strlen(msg), 0);
      init = true;
      free(paramEntier);
      continue;
    }
    //fin INI

    //Vérifie que la connexion a été initialisé
    if (!init) {
      strcat(msg, "ERR Connexion non initialisé.\n");
      send(socket_fd, msg, strlen(msg), 0);
      free(paramEntier);
      continue;
    }


    //REQ
    if (memcmp(cmd, "REQ", 3) == 0 && init) {

      sem_wait(&sem);
      request_processed += 1;

      //Verifie que la demande + les ressource alloue ne dépassent
      // pas le maximum de ressource disponible.
      bool valide = true;
      for (int i = 0; i < nbRessources; i++) {
        int pos = (id*nbRessources)+i;
        if (paramEntier[i+1] > (max[pos] - alloue[pos])
                || paramEntier[i+1] < (-1 * alloue[pos])) {
          valide = false;
        }
      }
      sem_post(&sem);
      //Requête invalide.
      if (!valide) {
        sem_wait(&sem);
        count_invalid += 1;
        sem_post(&sem);
        printf("Serveur %d: La requête dépasse le maximum permis.", st->id);
        strcat(msg, "ERR La requête dépasse le maximum permis.\n");
        send(socket_fd, msg, strlen(msg), 0);
        free(paramEntier);
        continue;
      }

      //////Algorithme du banquier.
      //bool attente = false;
      int* disp;
      bool* clients;
      int* besoin;
      bool sur;
      bool vide;
debut:
      sem_wait(&sem);
      // Vérifie si la demande est plus grande que ce qui est disponible.
      for (int i = 0; i < nbRessources; i++) {
        //Si la demande est plus grande, attente et retour au début.
        if (paramEntier[i+1] > disponible[i]) {
          //attente = true;
          sem_post(&sem);
          sleep(tempsAttente);
          goto debut;
        }
      }
      //Initialise les données pour l'algorithme.
      disp = malloc(nbRessources * sizeof(int));
      clients = malloc(idConnecte * sizeof(bool));
      besoin = malloc(idConnecte * nbRessources * sizeof(int));
      sur = true;
      vide = false;
      for (int i = 0; i < nbRessources; i++) {
        disp[i] = disponible[i] - paramEntier[i+1];
      }
      for (int i = 0; i < idConnecte; i++) {
        clients[i] = false;
      }
      for (int i = 0; i < idConnecte; i++) {
        for (int j = 0; j < nbRessources; j++) {
          if (i == id) {
            besoin[(i*nbRessources)+j] = max[(i*nbRessources)+j] 
                             - alloue[(i*nbRessources)+j] - paramEntier[j+1];
          } else {
            besoin[(i*nbRessources)+j] = max[(i*nbRessources)+j]
                                               - alloue[(i*nbRessources)+j];
          }
        }
      }

      //Exécute l'algorithme.
      while (sur && !vide) {
        sur = false;
        vide = true;
        for (int i = 0; i < idConnecte; i++) {
          if (!clients[i]) {
            bool peutFinir = true;
            for (int j = 0; j < nbRessources; j++) {
              if (besoin[(i*nbRessources)+j] > disp[j]) {
                peutFinir = false;
                vide = false;
                break;
              }
            }
            if (peutFinir) {
              clients[i] = true;
              for (int j = 0; j < nbRessources; j++) {
                if (i == id) {
                  disp[j] += alloue[(i*nbRessources)+j] + paramEntier[j+1];
                } else {
                  disp[j] += alloue[(i*nbRessources)+j];
                }
              }
              sur = true;
            }
          }
        }
      }
      //Si l'état est sur, alloue les ressources.
      if (sur) {
        for (int i = 0; i < nbRessources; i++) {
          disponible[i] = disponible[i] - paramEntier[i+1];
        }
        for (int i = 0; i < nbRessources; i++) {
          alloue[(id*nbRessources)+i] += paramEntier[i+1];
        }
        if (attente + 30 > time(NULL)) {
          count_accepted += 1;
        } else {
          count_wait += 1;
        }
      }
      sem_post(&sem);
      
      //Libère la mémoire utilisé pour l'algorithme.
      free(disp);
      free(clients);
      free(besoin);

      //Si l'état n'est pas sur, retourne au début.
      if (!sur) {
        //attente = true;
        sleep(tempsAttente);
        goto debut;
      }
      //Requête accepté
      printf("Serveur %d: Requête accepté.\n", st->id);
      strcat(msg, "ACK\n");
      send(socket_fd, msg, strlen(msg), 0);
      free(paramEntier);
      continue;

    }
    // fin REQ

    

    //Commande inconnu
    printf("Serveur %d: Commande inconnu.", st->id);
    strcat(msg, "ERR Commande inconnu.\n");
    send(socket_fd, msg, strlen(msg), 0);
    free(paramEntier);

  }

  fclose (socket_r);
  // TODO end
}


void
st_signal ()
{
  // TODO: Remplacer le contenu de cette fonction

  //Libère la mémoire
  free(disponibleMax);
  free(disponible);
  free(max);
  free(alloue);
  sem_destroy(&sem);


  // TODO end
}


void *
st_code (void *param)
{
  server_thread *st = (server_thread *) param;

  struct sockaddr_in thread_addr;
  socklen_t socket_len = sizeof (thread_addr);
  int thread_socket_fd = -1;
  int end_time = time (NULL) + max_wait_time;

  // Boucle jusqu'à ce que `accept` reçoive la première connection.
  while (thread_socket_fd < 0)
    {
      thread_socket_fd =
	accept (server_socket_fd, (struct sockaddr *) &thread_addr,
		&socket_len);
   
      if (time (NULL) >= end_time)
	{
	  break;
	}
    }

  // Boucle de traitement des requêtes.
  while (accepting_connections)
    {
      if (time (NULL) >= end_time)
	{
	  fprintf (stderr, "Time out on thread %d.\n", st->id);
	  pthread_exit (NULL);
	}
      if (thread_socket_fd > 0)
	{
	  st_process_requests (st, thread_socket_fd);
	  close (thread_socket_fd);
          end_time = time (NULL) + max_wait_time;
	}
      thread_socket_fd =
	accept (server_socket_fd, (struct sockaddr *) &thread_addr,
		&socket_len);
    }
  return NULL;
}


//
// Ouvre un socket pour le serveur.
//
void
st_open_socket (int port_number)
{
  server_socket_fd = socket (AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (server_socket_fd < 0)
    perror ("ERROR opening socket");

  struct sockaddr_in serv_addr;
  memset (&serv_addr, 0, sizeof (serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons (port_number);

  if (bind
      (server_socket_fd, (struct sockaddr *) &serv_addr,
       sizeof (serv_addr)) < 0)
    perror ("ERROR on binding");

  listen (server_socket_fd, server_backlog_size);
}


//
// Affiche les données recueillies lors de l'exécution du
// serveur.
// La branche else ne doit PAS être modifiée.
//
void
st_print_results (FILE * fd, bool verbose)
{
  if (fd == NULL) fd = stdout;
  if (verbose)
    {
      fprintf (fd, "\n---- Résultat du serveur ----\n");
      fprintf (fd, "Requêtes acceptées: %d\n", count_accepted);
      fprintf (fd, "Requêtes : %d\n", count_wait);
      fprintf (fd, "Requêtes invalides: %d\n", count_invalid);
      fprintf (fd, "Clients : %d\n", count_dispatched);
      fprintf (fd, "Requêtes traitées: %d\n", request_processed);
    }
  else
    {
      fprintf (fd, "%d %d %d %d %d\n", count_accepted, count_wait,
	       count_invalid, count_dispatched, request_processed);
    }
}


//Retourne un pointeur vers un tableau d'entier représentant les paramètres.
//Le premier élément du tableau est le nombre de paramètres.
int* separeParam(char* param) {
  int i = 0;
  int* paramEntier = malloc(1 * sizeof(int));
  bool test = false;

  char* c = strtok(param, " \n");
  while (c != NULL) {
    if (strlen(c) != 0) {
      i++;
      paramEntier = realloc(paramEntier, (i+1) * sizeof(int));
      paramEntier[i] = atoi(c);
      test = true;
    }
    c = strtok(NULL, " \n");
  }

  if (test) {
    paramEntier[0] = i;
    return paramEntier;
  } else {
    free(paramEntier);
    return NULL;
  }
  

}
