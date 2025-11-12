#ifndef JEU_H
#define JEU_H

#include <stdio.h>

// Structure représentant un joueur
typedef struct {
    int numero;         // 0 ou 1
    char pseudo[16];    // max de 15 caractères par pseudo
    int score;          
} Joueur;

// Initialise le plateau de jeu avec 4 graines par case
void initialiserJeu(int plateau[]);

// Réinitialise les scores des deux joueurs
void reinitialiserScores(Joueur *joueur0, Joueur *joueur1);

// Détecte une situation de famine
// Retourne -1 si aucune famine, sinon le numéro du joueur affamé
int detecterFamine(int plateau[]);

// Vérifie si la partie est terminée (score ou plateau vide)
// Retourne 1 si fin de partie, sinon 0
int terminerPartie(int plateau[], int *score0, int *score1);

// Gère la capture de graines après un coup et met à jour les scores et le plateau
void captureGraines(int plateau[], int numero, Joueur *joueur0, Joueur *joueur1, int derniereCase);

// Joue un coup pour le joueur donné et met à jour le plateau après le coup
// Codes de retour :
// 0 : Coup valide, joué avec succès
// 1 : Numéro de joueur invalide (doit être 0 ou 1)
// 2 : Indice de case hors bornes (doit être entre 0 et 11)
// 3 : Joueur 0 joue une case du camp adverse (indice > 5)
// 4 : Joueur 1 joue une case du camp adverse (indice ≤ 5)
// 5 : Case vide, impossible de jouer
// 6 : Coup interdit car il ne nourrit pas l’adversaire en famine
int jouerCoup(int plateau[], int numero, Joueur *joueur0, Joueur *joueur1, int indice);

// Affiche le plateau et les scores
void afficherPlateau(int plateau[], Joueur joueur0, Joueur joueur1);

#endif
