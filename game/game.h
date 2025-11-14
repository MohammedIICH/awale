/*************************************************************************
                           Awale -- Game
                             -------------------
    début                : 20/10/2025
    auteurs              : Mohammed Iich et Dame Dieng
    e-mails              : mohammed.iich@insa-lyon.fr et dame.dieng@insa-lyon.fr
    description          : Définition des structures et prototypes du
                           moteur de jeu Awalé sans aucun affichage réseau
*************************************************************************/

#ifndef GAME_H
#define GAME_H

#include <stdio.h>

/*
 * Représente un joueur d'Awalé.
 * number : 0 ou 1
 * name   : pseudonyme limité à 15 caractères
 * score  : graines capturées
 */
typedef struct {
    int number;
    char name[16];
    int score;
} Player;

/* Initialise le plateau avec 4 graines dans chaque case */
void initGame(int board[]);

/* Remet les scores des deux joueurs à zéro */
void resetScores(Player *p0, Player *p1);

/*
 * Détecte une famine :
 *  - retourne 0 si le joueur 0 n’a plus de graines
 *  - retourne 1 si le joueur 1 n’a plus de graines
 *  - retourne -1 si aucun joueur n’est affamé
 */
int detectStarvation(int board[]);

/*
 * Détermine si la partie doit se terminer,
 * en tenant compte :
 *  - du total des points
 *  - d’un cas de famine
 *  - du nombre de graines restantes
 */
int isGameOver(int board[], int *score0, int *score1);

/*
 * Applique la règle de capture après qu’un coup a été joué.
 * Les captures se font en remontant les cases adverses contenant 2 ou 3 graines.
 */
void captureSeeds(int board[], int playerNumber,
                  Player *p0, Player *p1,
                  int lastPit);

/*
 * Joue un coup :
 *  - playerNumber = 0 ou 1
 *  - pit : index de case joué
 *
 * Codes de retour :
 *  0 = coup valide
 *  1 = numéro de joueur invalide
 *  2 = case hors limites
 *  3 = joueur 0 tente une case de l'adversaire
 *  4 = joueur 1 tente une case de l'adversaire
 *  5 = case vide
 *  6 = ne nourrit pas un adversaire affamé (coup illégal)
 */
int playMove(int board[], int playerNumber,
             Player *p0, Player *p1,
             int pit);

/* Affiche le plateau en deux lignes (utile pour le débogage local) */
void printBoard(int board[], Player p0, Player p1);

#endif
