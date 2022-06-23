/*
 * TP Centre de tri de colis - version �tudiant
 * 4/5/2019 - YA
 */
 
// includes Kernel
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// includes Standard 
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// includes Simulateur (redirection de la sortie printf)
#include "simulateur.h"

// D�finition du nombre d'�l�ments pour les files
#define x 10 // nombre d'�l�ments pour la File_tapis_arrivee 
#define y 10 // nombre d'�l�ments pour la File_depart_national et File_depart_international
#define z 10 // nombre d'�l�ments pour la File_tapis_relecture 

// D�finition du delai pour la t�che de relecture
#define DELAI_RELECTURE   500

// D�finition des timeout pour les files en ms
#define TIMEOUT_FILE_TAPIS_ARRIVEE   100
#define TIMEOUT_FILE_TAPIS_RELECTURE 100
#define TIMEOUT_FILE_TAPIS_DEPART		 100  // Timeout identique en national ou international

// Cr�ation des files
xQueueHandle File_tapis_arrivee;
xQueueHandle File_tapis_relecture;
xQueueHandle File_depart_national;
xQueueHandle File_depart_international;

// Cr�ation du s�maphore pour la ressource partag�e
xSemaphoreHandle Affichage;

void affiche_message(char* texte, unsigned int colis) {
	while(!xSemaphoreTake(Affichage, 10 / portTICK_RATE_MS));
	printf("%s : compteur=%d B2=%d B1=%d B0=%d\n", texte, colis>>3, (colis & (1<<2))>>2, (colis & (1<<1))>>1, (colis & (1<<0))>>0);
	xSemaphoreGive(Affichage);
}


//********************************************************
//* T�che arriv�e 
//*
//* R�p�te de fa�on cyclique une liste de colis � deposer
//*
//* B2=1 -> le colis est pass� par � tache-relecture �, sinon B2=0
//* B1=1 -> �tiquette non lisible sinon B1=0
//* B0=1 -> march� international / B0=0 -> march� international
//* 
//********************************************************
void tache_arrivee( void *pvParameters )
{	
	unsigned int liste_colis[]={ 1,  3,  1,  2,  3,  0}; // Etat des bits B2, B1 et B0 -> 0 � 3 en d�cimal car B2=0
	unsigned int liste_delai[]={ 5,100,200, 30,400, 50}; // Temps d'attente en ms pour le colis suivant
	unsigned int num_colis = 0; // Initilisation du num�ro de colis
	unsigned int colis; // Colis (�tiquette)
while(1)
	{ 
		num_colis++;
		colis = (num_colis<<3) + liste_colis[num_colis % (sizeof(liste_colis)/sizeof(unsigned int))];
		xQueueSendToBack(File_tapis_arrivee, &colis, TIMEOUT_FILE_TAPIS_ARRIVEE); // Il faudra g�rer le d�bordement et afficher un message d'erreur
		affiche_message("T�che arriv�e", colis);	// Il faudra g�rer l'affichage comme une ressource partag�e
		vTaskDelay(liste_delai[num_colis % (sizeof(liste_colis)/sizeof(unsigned int))]/portTICK_RATE_MS); // Attente entre deux colis en ms
		
  }
	vTaskDelete( NULL );
}

void tache_lecture_rapide(void* pvParameters) {
	unsigned int colis;
	while(1) {
		if(xQueueReceive(File_tapis_arrivee, &colis, TIMEOUT_FILE_TAPIS_ARRIVEE)) {
			affiche_message("T�che lecture rapide", colis);
			if(colis & (1<<1)) {
				xQueueSendToBack(File_tapis_relecture, &colis, TIMEOUT_FILE_TAPIS_RELECTURE);
			}
			else {
				if(colis & (1<<0)) {
					xQueueSendToBack(File_depart_international, &colis, TIMEOUT_FILE_TAPIS_DEPART);
				}
				else {
					xQueueSendToBack(File_depart_national, &colis, TIMEOUT_FILE_TAPIS_DEPART);
				}
			}
		}
	}
	vTaskDelete(NULL);
}

void tache_depart_national(void* pvParameters) {
	unsigned int colis;
	while(1) {
		if(xQueueReceive(File_depart_national, &colis, TIMEOUT_FILE_TAPIS_DEPART)) {
			affiche_message("T�che d�part national", colis);
		}
	}
	vTaskDelete(NULL);
}

void tache_depart_international(void* pvParameters) {
	unsigned int colis;
	while(1) {
		if(xQueueReceive(File_depart_international, &colis, TIMEOUT_FILE_TAPIS_DEPART)) {
			affiche_message("T�che d�part international", colis);
		}
	}
	vTaskDelete(NULL);
}

void tache_relecture(void* pvParameters) {
	unsigned int colis;
	while(1) {
		if(xQueueReceive(File_tapis_relecture, &colis, TIMEOUT_FILE_TAPIS_RELECTURE)) {
			affiche_message("T�che relecture", colis);
			colis |= 1<<2;
			colis &= ~(1<<1);
			vTaskDelay(50 / portTICK_RATE_MS);
			xQueueSendToFront(File_tapis_arrivee, &colis, TIMEOUT_FILE_TAPIS_ARRIVEE);
			affiche_message("T�che relecture", colis);
		}
	}
	vTaskDelete(NULL);
}

// Main()
int main(void)
{
	// ulTaskNumber[0] = tache_arrivee
	// ulTaskNumber[1] = idle task
	
	// Cr�ation des files
	File_tapis_arrivee  			= xQueueCreate(x, sizeof(unsigned int));
	File_tapis_relecture  		= xQueueCreate(z, sizeof(unsigned int));
	File_depart_national 			= xQueueCreate(y, sizeof(unsigned int));
	File_depart_international	= xQueueCreate(y, sizeof(unsigned int));

	// Cr�ation du s�maphore pour la ressource partag�e
	Affichage = xSemaphoreCreateMutex();
	
	// Cr�ation de la t�che arriv�e
	xTaskCreate(tache_arrivee,// Pointeur vers la fonction
				"Tache arrivee",					// Nom de la t�che, facilite le debug
				configMINIMAL_STACK_SIZE, // Taille de pile (mots)
				NULL, 										// Pas de param�tres pour la t�che
				1, 												// Niveau de priorit� 1 pour la t�che (0 �tant la plus faible) 
				NULL ); 									// Pas d'utilisation du task handle

	xTaskCreate(tache_lecture_rapide,
				"Tache lecture rapide",
				configMINIMAL_STACK_SIZE,
				NULL,
				1,
				NULL);

	xTaskCreate(tache_depart_national,
				"Tache depart national",
				configMINIMAL_STACK_SIZE,
				NULL,
				1,
				NULL);
				
	xTaskCreate(tache_depart_international,
				"Tache depart international",
				configMINIMAL_STACK_SIZE,
				NULL,
				1,
				NULL);
				
	xTaskCreate(tache_relecture,
				"Tache relecture",
				configMINIMAL_STACK_SIZE,
				NULL,
				1,
				NULL);
	
	// Lance le scheduler et les taches associ�es 
	vTaskStartScheduler();
	// On n'arrive jamais ici sauf en cas de probl�mes graves: pb allocation m�moire, d�bordement de pile par ex.
	while(1);
}

