#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdio.h>
#include "boolean.h"

#define MARK '\0'
#define NMax 250
#define BLANK ' '
/* Press enter -> ditandai -> ganti */
#define ENTER '\n'

typedef struct {
    char TabKata[NMax];
    int Length;
} Kata;

/* Ini didefinisikan di source code keyboard.c */
extern char CC;
extern boolean EOP;
extern boolean EndKata;
extern Kata CKata;
extern char *commands[];
extern int num_commands;

void START();
/* Mesin siap dioperasikan. Pita disiapkan untuk dibaca.
   Karakter pertama yang ada pada pita dipilih sebagai CC.
   EOP akan menyala (true) jika CC adalah MARK.
   Jika EOP menyala, maka CC tidak terdefinisi. */

void ADV();
/* Karakter berikutnya dipilih sebagai CC.
   EOP akan menyala (true) jika CC adalah MARK.
   Jika EOP menyala, maka CC tidak terdefinisi. */

void IgnoreBlank();
/* Mengabaikan satu atau beberapa BLANK dan ENTER di pita. */

void STARTKATA();
/* I.S. : CC sembarang 
   F.S. : EndKata = true, dan CC = MARK; 
          atau EndKata = false, CKata adalah kata yang sudah diakuisisi,
          CC karakter pertama sesudah karakter terakhir kata */

void ADVKATA();
/* I.S. : CC adalah karakter pertama kata yang akan diakuisisi 
   F.S. : CKata adalah kata terakhir yang sudah diakuisisi, 
          CC adalah karakter pertama sesudah karakter terakhir kata
          Jika CC == MARK, EndKata = true. */

void STASHKATA();
/* Menghapus sisa karakter pada baris yang sedang dibaca hingga MARK. 
   I.S. : CC sembarang
   F.S. : EOP menyala (true), CC = MARK. */

void getLineWithAutofill(char *buffer, int max_len, const char *current_tab, const char *current_website, int execution_count);
/* Membaca satu baris input dengan dukungan autofill menggunakan tombol Tab. */

void SalinKata();
/* Mengakuisisi kata, menyimpan dalam CKata.
   I.S. : CC adalah karakter pertama dari kata
   F.S. : CKata berisi kata yang sudah diakuisisi; 
          CC = BLANK atau CC = MARK atau CC = ENTER; 
          CKata.Length adalah panjang kata yang diakuisisi. */

boolean isKataEqual(Kata K1, char* s);
/* Membandingkan apakah K1 sama dengan string s */

boolean KataIs(char *s);
/* Membandingkan apakah CKata sama dengan string s */

#endif // KEYBOARD_H

