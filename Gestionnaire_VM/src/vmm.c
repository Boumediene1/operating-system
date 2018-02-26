#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "conf.h"
#include "common.h"
#include "vmm.h"
#include "tlb.h"
#include "pt.h"
#include "pm.h"

static unsigned int read_count = 0;
static unsigned int write_count = 0;
static FILE* vmm_log;

//Pour choisir les frames a evincer
struct entreeMemoire {
  unsigned int pageNb;
  bool accede;
};
struct entreeMemoire memoire[NUM_FRAMES];
static int suivant = 0;

void vmm_init (FILE *log)
{
  // Initialise le fichier de journal.
  vmm_log = log;
  for (int i = 0; i < NUM_FRAMES; i++) {
    memoire[i].pageNb = -1;
  }
}


// NE PAS MODIFIER CETTE FONCTION
static void vmm_log_command (FILE *out, const char *command,
                             unsigned int laddress, /* Logical address. */
		             unsigned int page,
                             unsigned int frame,
                             unsigned int offset,
                             unsigned int paddress, /* Physical address.  */
		             char c) /* Caractère lu ou écrit.  */
{
  if (out)
    fprintf (out, "%s[%c]@%05d: p=%d, o=%d, f=%d pa=%d\n", command, c, laddress,
	     page, offset, frame, paddress);
}

//Va chercher la page et la charge dans la memoire
//Retourne le numero de frame
int pageFault(unsigned int pageNb, bool write) {
  //Choisit la frame a remplacer
  while (memoire[suivant].accede == true) {
    memoire[suivant].accede = false;
    suivant = (suivant + 1) % NUM_FRAMES;
  }
  int frame = suivant;
  int ancient = memoire[frame].pageNb;
  //Si la page a ete modifiee
  if (ancient != -1 && !pt_readonly_p(ancient)) {
    pm_backup_page(frame, ancient);
    pt_set_readonly(ancient, true);
  }
  //Sors l'ancienne page de la table
  if (!(ancient == -1)) {
    pt_unset_entry(ancient);
  }
  //Charge la nouvelle page
  pm_download_page(pageNb, frame);
  //Ajoute les donnees dans la table des pages
  pt_set_entry(pageNb, frame);
  pt_set_readonly(pageNb, write);
  //Ajoute les donnees dans le TLB
  tlb_add_entry(pageNb, frame, pt_readonly_p(pageNb));
  //Prepare le prochain evincement
  memoire[frame].pageNb = pageNb;
  suivant = (suivant + 1) % NUM_FRAMES;
  return frame;
}

/* Effectue une lecture à l'adresse logique `laddress`.  */
char vmm_read (unsigned int laddress)
{
  char c = '!';
  read_count++;
  /* ¡ TODO: COMPLÉTER ! */
  //Conversion de l'adresse logique en nb de page et offset
  unsigned int pageNb = laddress >> 8;
  unsigned int offset = laddress & 0xff;
  //Cherche dans le TLB
  int frame = tlb_lookup(pageNb, false);
  //Cherche dans la table des pages
  if (frame < 0) {
    frame = pt_lookup(pageNb);
    if (!(frame < 0)) {
      tlb_add_entry(pageNb, frame, pt_readonly_p(pageNb));
    }
  }
  //Page fault
  if(frame < 0) {
    frame = pageFault(pageNb, false);
  }
  //Conversion du nb de frame et offset en adresse physique
  unsigned int pAdresse = (frame * PAGE_FRAME_SIZE) + offset;
  //Lecture du caractere
  c = pm_read(pAdresse);
  //POur l'evincement
  memoire[frame].accede = true;
  // TODO: Fournir les arguments manquants.
  vmm_log_command (stdout, "READING", laddress, pageNb, frame, 
                                                offset, pAdresse, c);
  return c;
}

/* Effectue une écriture à l'adresse logique `laddress`.  */
void vmm_write (unsigned int laddress, char c)
{
  write_count++;
  /* ¡ TODO: COMPLÉTER ! */
   //Conversion de l'adresse logique en nb de page et offset
  unsigned int pageNb = laddress >> 8;
  unsigned int offset = laddress & 0xff;
  //Cherche dans le TLB
  int frame = tlb_lookup(pageNb, false);
  //Cherche dans la table des pages
  if (frame < 0) {
    frame = pt_lookup(pageNb);
    if (!(frame < 0)) {
      tlb_add_entry(pageNb, frame, pt_readonly_p(pageNb));
    }
  }
  //Page fault
  if(frame < 0) {
    frame = pageFault(pageNb, false);
  }
  //Conversion du nb de frame et offset en adresse physique
  unsigned int pAdresse = (frame * PAGE_FRAME_SIZE) + offset;
  //Ecriture du caractere
  pm_write(pAdresse, c);
  //Pour l'evincement
  memoire[frame].accede = true;
  // TODO: Fournir les arguments manquants.
  vmm_log_command (stdout, "WRITING", laddress, pageNb, frame, 
                                                offset, pAdresse, c);
}


// NE PAS MODIFIER CETTE FONCTION
void vmm_clean (void)
{
  fprintf (stdout, "VM reads : %4u\n", read_count);
  fprintf (stdout, "VM writes: %4u\n", write_count);
}
