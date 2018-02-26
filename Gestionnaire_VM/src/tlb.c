
#include <stdint.h>
#include <stdio.h>

#include "tlb.h"

#include "conf.h"

struct tlb_entry
{
  unsigned int page_number;
  int frame_number;             /* Invalide si négatif.  */
  bool readonly : 1;
};

static FILE *tlb_log = NULL;
static struct tlb_entry tlb_entries[TLB_NUM_ENTRIES]; 

static unsigned int tlb_hit_count = 0;
static unsigned int tlb_miss_count = 0;
static unsigned int tlb_mod_count = 0;

/* Initialise le TLB, et indique où envoyer le log des accès.  */
void tlb_init (FILE *log)
{
  for (int i = 0; i < TLB_NUM_ENTRIES; i++)
    tlb_entries[i].frame_number = -1;
  tlb_log = log;
}

/******************** ¡ NE RIEN CHANGER CI-DESSUS !  ******************/

//Variable donnant la prochaine entree du TLB a remplacer
static int suivant = 0;

/* Recherche dans le TLB.
 * Renvoie le `frame_number`, si trouvé, ou un nombre négatif sinon.  */
static int tlb__lookup (unsigned int page_number, bool write)
{
  // TODO: COMPLÉTER CETTE FONCTION.
  //Regarde si l'entrée se trouve dans le TLB
  for (int i = 0; i < TLB_NUM_ENTRIES; i++) {
    if (tlb_entries[i].page_number == page_number) {
      //La page est marqué en lecture seulement
      if (write && tlb_entries[i].readonly) {
        break;
      }
      //Retourne le numero de page
      return tlb_entries[i].frame_number;
    }
  }
  return -1;
}

/* Ajoute dans le TLB une entrée qui associe `frame_number` à
 * `page_number`.  */
static void tlb__add_entry (unsigned int page_number,
                            unsigned int frame_number, bool readonly)
{
  // TODO: COMPLÉTER CETTE FONCTION.
  for (int i = 0; i < TLB_NUM_ENTRIES; i++) {
    //Si on ajoute une page et qu'on vient d'en evincer une autre.
    if (tlb_entries[i].frame_number == frame_number) {
      tlb_entries[i].page_number = page_number;
      tlb_entries[i].readonly = readonly;
      return;
    }
  }
  //Modifie l'entree suivante du TLB
  tlb_entries[suivant].page_number = page_number;
  tlb_entries[suivant].frame_number = frame_number;
  tlb_entries[suivant].readonly = readonly;
  suivant = (suivant + 1) % TLB_NUM_ENTRIES;
}

/******************** ¡ NE RIEN CHANGER CI-DESSOUS !  ******************/

void tlb_add_entry (unsigned int page_number,
                    unsigned int frame_number, bool readonly)
{
  tlb_mod_count++;
  tlb__add_entry (page_number, frame_number, readonly);
}

int tlb_lookup (unsigned int page_number, bool write)
{
  int fn = tlb__lookup (page_number, write);
  (*(fn < 0 ? &tlb_miss_count : &tlb_hit_count))++;
  return fn;
}

/* Imprime un sommaires des accès.  */
void tlb_clean (void)
{
  fprintf (stdout, "TLB misses   : %3u\n", tlb_miss_count);
  fprintf (stdout, "TLB hits     : %3u\n", tlb_hit_count);
  fprintf (stdout, "TLB changes  : %3u\n", tlb_mod_count);
  fprintf (stdout, "TLB miss rate: %.1f%%\n",
           100 * tlb_hit_count
           /* Ajoute 0.01 pour éviter la division par 0.  */
           / (0.01 + tlb_hit_count + tlb_miss_count));
}
