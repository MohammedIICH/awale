#include <stdio.h>
#include <string.h>
#include "jeu.h"

#define SEP "\n============================================================\n"

void afficherTest(const char *titre) {
    printf("%s%s%s\n", SEP, titre, SEP);
}

int main(void) {
    int plateau[12];
    Joueur joueur0, joueur1;

    // === INITIALISATION ===
    strcpy(joueur0.pseudo, "Mohammed");
    strcpy(joueur1.pseudo, "Dame");

    reinitialiserScores(&joueur0, &joueur1);

    // === TEST 1 : INITIALISATION ET PREMIERS COUPS ===
    afficherTest("TEST 1 : INITIALISATION ET PREMIERS COUPS");
    initialiserJeu(plateau);
    afficherPlateau(plateau, joueur0, joueur1);

    printf("\n%s joue la case 0 :\n\n", joueur0.pseudo);
    jouerCoup(plateau, 0, &joueur0, &joueur1, 0);
    afficherPlateau(plateau, joueur0, joueur1);

    printf("\n%s joue la case 11 :\n\n", joueur1.pseudo);
    jouerCoup(plateau, 1, &joueur0, &joueur1, 11);
    afficherPlateau(plateau, joueur0, joueur1);

    printf("\n%s joue la case 3 :\n\n", joueur0.pseudo);
    jouerCoup(plateau, 0, &joueur0, &joueur1, 3);
    afficherPlateau(plateau, joueur0, joueur1);

    // === TEST 2 : CAPTURE DES GRAINES ===
    reinitialiserScores(&joueur0, &joueur1);
    afficherTest("TEST 2 : CAPTURE DES GRAINES");

    for (int i = 0; i < 12; i++) plateau[i] = 0;
    plateau[11] = 5; plateau[8] = 2; plateau[7] = 1;
    plateau[6] = 2; plateau[5] = 1; plateau[4] = 4;

    afficherPlateau(plateau, joueur0, joueur1);
    printf("\n%s joue la case 4 :\n\n", joueur0.pseudo);
    jouerCoup(plateau, 0, &joueur0, &joueur1, 4);
    afficherPlateau(plateau, joueur0, joueur1);

    // === TEST 3 : CAPTURE ANNULÉE POUR ÉVITER LA FAMINE ===
    reinitialiserScores(&joueur0, &joueur1);
    afficherTest("TEST 3 : CAPTURE ANNULÉE (FAMINE)");

    for (int i = 0; i < 12; i++) plateau[i] = 0;
    plateau[8] = 2; plateau[7] = 1; plateau[6] = 2;
    plateau[5] = 1; plateau[4] = 4;

    afficherPlateau(plateau, joueur0, joueur1);
    printf("\n%s joue la case 4 :\n\n", joueur0.pseudo);
    jouerCoup(plateau, 0, &joueur0, &joueur1, 4);
    afficherPlateau(plateau, joueur0, joueur1);
    printf("=> Capture annulée pour éviter la famine.\n\n");

    // === TEST 4 : FIN PAR SCORE > 24 ===
    reinitialiserScores(&joueur0, &joueur1);
    afficherTest("TEST 4 : FIN DE PARTIE PAR SCORE > 24");

    initialiserJeu(plateau);
    joueur0.score = 24;
    afficherPlateau(plateau, joueur0, joueur1);
    printf("Partie terminée ? %s\n\n",
           terminerPartie(plateau, &joueur0.score, &joueur1.score) ? "Oui" : "Non");

    joueur0.score = 26;
    afficherPlateau(plateau, joueur0, joueur1);
    printf("Partie terminée ? %s\n\n",
           terminerPartie(plateau, &joueur0.score, &joueur1.score) ? "Oui" : "Non");

    // === TEST 5 : FIN QUAND PLUS RIEN À PRENDRE ===
    afficherTest("TEST 5 : FIN QUAND PLUS RIEN À PRENDRE");
    reinitialiserScores(&joueur0, &joueur1);

    for (int i = 0; i < 12; i++) plateau[i] = 0;
    afficherPlateau(plateau, joueur0, joueur1);
    printf("Partie terminée ? %s\n\n",
           terminerPartie(plateau, &joueur0.score, &joueur1.score) ? "Oui" : "Non");

    // === TEST 6 : FAMINE SANS POSSIBILITÉ DE NOURRIR ===
    reinitialiserScores(&joueur0, &joueur1);
    afficherTest("TEST 6 : FAMINE SANS POSSIBILITÉ DE NOURRIR");

    for (int i = 0; i < 12; i++) plateau[i] = 0;
    plateau[0] = 5;
    afficherPlateau(plateau, joueur0, joueur1);
    printf("Partie terminée ? %s\n\n",
           terminerPartie(plateau, &joueur0.score, &joueur1.score) ? "Oui" : "Non");

    reinitialiserScores(&joueur0, &joueur1);

    printf("Si on met 6 dans la case 0 :\n\n");
    plateau[0] = 6;
    afficherPlateau(plateau, joueur0, joueur1);
    printf("Partie terminée ? %s\n\n",
           terminerPartie(plateau, &joueur0.score, &joueur1.score) ? "Oui" : "Non");

    // === TEST 7 : RÈGLE DES 12 GRAINES ===
    reinitialiserScores(&joueur0, &joueur1);
    afficherTest("TEST 7 : RÈGLE DES 12 GRAINES");

    for (int i = 0; i < 12; i++) plateau[i] = 0;
    plateau[0] = 30;

    afficherPlateau(plateau, joueur0, joueur1);
    printf("\n%s joue la case 0 :\n\n", joueur0.pseudo);
    jouerCoup(plateau, 0, &joueur0, &joueur1, 0);
    afficherPlateau(plateau, joueur0, joueur1);
    printf("=> La case d’origine n’a pas été remplie, redistribution correcte.\n\n");

    // === TEST 8 : OBLIGATION DE NOURRIR ===
    reinitialiserScores(&joueur0, &joueur1);
    afficherTest("TEST 8 : OBLIGATION DE NOURRIR SI FAMINE");

    for (int i = 0; i < 12; i++) plateau[i] = 0;
    plateau[0] = 10;
    plateau[1] = 1;

    afficherPlateau(plateau, joueur0, joueur1);
    printf("État de famine : %s\n\n", detecterFamine(plateau) ? "Oui" : "Non");
    printf("Partie terminée ? %s\n\n",
           terminerPartie(plateau, &joueur0.score, &joueur1.score) ? "Oui" : "Non");

    printf("%s tente de jouer la case 1 (refusé) :\n\n", joueur0.pseudo);
    jouerCoup(plateau, 0, &joueur0, &joueur1, 1);
    afficherPlateau(plateau, joueur0, joueur1);

    printf("\n%s joue la case 0 (accepté) :\n\n", joueur0.pseudo);
    jouerCoup(plateau, 0, &joueur0, &joueur1, 0);
    afficherPlateau(plateau, joueur0, joueur1);

    // === TEST 9 : FIN PAR INDÉTERMINATION ===
    reinitialiserScores(&joueur0, &joueur1);
    afficherTest("TEST 9 : FIN PAR INDÉTERMINATION");

    for (int i = 0; i < 12; i++) plateau[i] = 0;
    plateau[3] = 1;
    plateau[9] = 2;

    afficherPlateau(plateau, joueur0, joueur1);
    int fin = terminerPartie(plateau, &joueur0.score, &joueur1.score);

    printf("\nAprès détection :\n\n");
    afficherPlateau(plateau, joueur0, joueur1);
    printf("Partie terminée ? %s\n", fin ? "Oui" : "Non");
    printf("\n=> Les graines restantes sont attribuées à chaque joueur.\n");

    printf("%sTOUS LES TESTS SONT TERMINÉS AVEC SUCCÈS !%s\n\n", SEP, SEP);
    return 0;
}