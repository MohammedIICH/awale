/*************************************************************************
                           Awale -- Game
                             -------------------
    début                : 20/10/2025
    auteurs              : Mohammed Iich et Dame Dieng
    e-mails              : mohammed.iich@insa-lyon.fr et dame.dieng@insa-lyon.fr
    description          : Implémentation complète du moteur du jeu Awalé
*************************************************************************/

#include "game.h"

/* Initialise toutes les cases du plateau à 4 graines */
void initGame(int board[]) {
    for (int i = 0; i < 12; i++)
        board[i] = 4;
}

/* Remet les scores des joueurs à zéro */
void resetScores(Player *p0, Player *p1) {
    p0->score = 0;
    p1->score = 0;
}

/*
 * Détermine si un joueur n'a plus de graines.
 * Retour :
 *  -1 : personne n'est affamé
 *   0 : joueur 0 affamé
 *   1 : joueur 1 affamé
 */
int detectStarvation(int board[]) {
    int seedsP0 = 0, seedsP1 = 0;

    for (int i = 0; i <= 5; i++)  seedsP0 += board[i];
    for (int i = 6; i <= 11; i++) seedsP1 += board[i];

    if (seedsP0 == 0) return 0;
    if (seedsP1 == 0) return 1;
    return -1;
}

/*
 * Détermine si la partie doit se terminer en fonction :
 *  - du score atteint
 *  - de la famine
 *  - du nombre total de graines
 */
int isGameOver(int board[], int *score0, int *score1) {

    /* Victoire automatique si un joueur dépasse 24 points */
    if (*score0 > 24 || *score1 > 24)
        return 1;

    /* Si toutes les graines ont été capturées */
    if ((*score0 + *score1) >= 48)
        return 1;

    /* Test de famine */
    int starving = detectStarvation(board);
    int seedsP0 = 0, seedsP1 = 0;

    for (int i = 0; i <= 5; i++)  seedsP0 += board[i];
    for (int i = 6; i <= 11; i++) seedsP1 += board[i];

    if (starving != -1) {
        int emptyPlayer = (seedsP0 == 0) ? 0 : 1;
        int opponent    = 1 - emptyPlayer;

        /* Vérifie si l'adversaire peut nourrir le joueur affamé */
        int canFeed = 0;

        if (opponent == 0) {
            for (int i = 0; i <= 5; i++)
                if (board[i] > (5 - i)) { canFeed = 1; break; }
        } else {
            for (int i = 6; i <= 11; i++)
                if (board[i] > (i - 5)) { canFeed = 1; break; }
        }

        /* Si impossible de nourrir → les graines restantes vont à l’adversaire */
        if (!canFeed) {
            int remaining = seedsP0 + seedsP1;
            if (emptyPlayer == 0) *score1 += remaining;
            else                  *score0 += remaining;

            for (int i = 0; i < 12; i++) board[i] = 0;
            return 1;
        }
    }

    /* Si trop peu de graines restent pour continuer (≤ 3) */
    int total = 0;
    for (int i = 0; i < 12; i++) total += board[i];

    if (total <= 3) {
        for (int i = 0; i <= 5; i++)  *score0 += board[i];
        for (int i = 6; i <= 11; i++) *score1 += board[i];
        for (int i = 0; i < 12; i++) board[i] = 0;
        return 1;
    }

    return 0;
}

/*
 * Applique les règles de capture :
 * le joueur capture les cases de l’adversaire contenant
 * exactement 2 ou 3 graines, en remontant la ligne adverse.
 */
void captureSeeds(int board[], int playerNumber,
                  Player *p0, Player *p1,
                  int lastPit) {

    int direction = (playerNumber == 0) ? -1 : 1;
    int pos = lastPit;
    int totalCaptured = 0;
    int capturedPits[12];
    int captureCount = 0;

    /* Parcourt les cases adverses contiguës valides pour capture */
    while (pos >= 0 && pos <= 11 &&
           ((playerNumber == 0 && pos >= 6) ||
            (playerNumber == 1 && pos <= 5)) &&
           (board[pos] == 2 || board[pos] == 3)) {

        totalCaptured += board[pos];
        capturedPits[captureCount++] = pos;
        pos += direction;
    }

    /* Vérifie si ces captures affameraient l’adversaire */
    int remainingSeeds = 0;

    if (playerNumber == 0) {
        for (int j = 6; j <= 11; j++) {
            int seeds = board[j];
            for (int k = 0; k < captureCount; k++)
                if (capturedPits[k] == j) seeds = 0;
            remainingSeeds += seeds;
        }
    } else {
        for (int j = 0; j <= 5; j++) {
            int seeds = board[j];
            for (int k = 0; k < captureCount; k++)
                if (capturedPits[k] == j) seeds = 0;
            remainingSeeds += seeds;
        }
    }

    /* Si l’adversaire garde au moins une graine → captures valides */
    if (remainingSeeds > 0) {
        for (int k = 0; k < captureCount; k++)
            board[capturedPits[k]] = 0;

        if (playerNumber == 0)
            p0->score += totalCaptured;
        else
            p1->score += totalCaptured;
    }
}

/*
 * Joue un coup selon les règles :
 * - semailles en sens antihoraire
 * - saut de la case d'origine
 * - capture éventuelle
 */
int playMove(int board[], int playerNumber,
             Player *p0, Player *p1,
             int pit) {

    /* Vérifications de base */
    if (playerNumber != 0 && playerNumber != 1) return 1;
    if (pit < 0 || pit > 11) return 2;
    if (playerNumber == 0 && pit > 5)  return 3;
    if (playerNumber == 1 && pit <= 5) return 4;
    if (board[pit] == 0) return 5;

    int seeds = board[pit];

    /* Règle : si l'adversaire est affamé, le coup doit nourrir */
    int starving = detectStarvation(board);

    if (starving != -1) {
        if (playerNumber == 0 && seeds <= (5 - pit))  return 6;
        if (playerNumber == 1 && seeds <= (11 - pit)) return 6;
    }

    /* Semailles */
    board[pit] = 0;
    int pos = pit;

    while (seeds > 0) {
        pos = (pos + 1) % 12;
        if (pos == pit) continue;
        board[pos]++;
        seeds--;
    }

    /* Tentative de capture */
    captureSeeds(board, playerNumber, p0, p1, pos);

    return 0;
}

/* Affichage simple ASCII du plateau en 2 lignes */
void printBoard(int board[], Player p0, Player p1) {
    printf("%-15s : ", p1.name);
    for (int i = 11; i >= 6; i--)
        printf(" %2d ", board[i]);
    printf("| Score : %d\n\n", p1.score);

    printf("%-15s : ", p0.name);
    for (int i = 0; i <= 5; i++)
        printf(" %2d ", board[i]);
    printf("| Score : %d\n\n", p0.score);
}
