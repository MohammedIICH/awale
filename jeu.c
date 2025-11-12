#include "jeu.h"

// Initialise le plateau avec 4 graines par case
void initialiserJeu(int plateau[]) {
    for (int i = 0; i < 12; i++) {
        plateau[i] = 4;
    }
}

// Remet les scores à zéro
void reinitialiserScores(Joueur *joueur0, Joueur *joueur1) {
    joueur0->score = 0;
    joueur1->score = 0;
}

// Détecte une famine (0 : joueur 0 affamé, 1 : joueur 1 affamé, -1 : aucune famine)
int detecterFamine(int plateau[]) {
    int grainesJ0 = 0, grainesJ1 = 0;

    for (int i = 0; i <= 5; i++) grainesJ0 += plateau[i];
    for (int i = 6; i <= 11; i++) grainesJ1 += plateau[i];

    if (grainesJ0 == 0 && grainesJ1 == 0) return -1;  // plateau vide
    if (grainesJ0 == 0) return 0;
    if (grainesJ1 == 0) return 1;

    return -1;
}

// Vérifie si la partie doit se terminer
int terminerPartie(int plateau[], int *score0, int *score1) {
    // Un joueur atteint ou dépasse 25 points
    if (*score0 > 24 || *score1 > 24)
        return 1;

    // 48 graines ont été capturées
    if ((*score0 + *score1) >= 48)
        return 1;

    // Cas de famine
    int grainesJ0 = 0, grainesJ1 = 0;
    for (int i = 0; i <= 5; i++) grainesJ0 += plateau[i];
    for (int i = 6; i <= 11; i++) grainesJ1 += plateau[i];

    int etat = detecterFamine(plateau);
    if (etat != -1) {
        int joueurVide = (grainesJ0 == 0) ? 0 : 1;
        int joueurAdverse = (joueurVide == 0) ? 1 : 0;

        // Vérifie si l’adversaire peut nourrir
        int peutNourrir = 0;
        if (joueurAdverse == 0) {
            for (int i = 0; i <= 5; i++) {
                if (plateau[i] > (5 - i)) {
                    peutNourrir = 1;
                    break;
                }
            }
        } else {
            for (int i = 6; i <= 11; i++) {
                if (plateau[i] > (i - 5)) {
                    peutNourrir = 1;
                    break;
                }
            }
        }

        // Aucun coup possible pour nourrir → fin de partie
        if (!peutNourrir) {
            int grainesRestantes = grainesJ0 + grainesJ1;

            if (joueurVide == 0)
                *score1 += grainesRestantes;
            else
                *score0 += grainesRestantes;

            for (int i = 0; i < 12; i++) plateau[i] = 0;
            return 1;
        }
    }

    // Fin par indétermination (trop peu de graines restantes)
    int totalGraines = 0;
    for (int i = 0; i < 12; i++) totalGraines += plateau[i];

    if (totalGraines <= 3) {
        for (int i = 0; i <= 5; i++) *score0 += plateau[i];
        for (int i = 6; i <= 11; i++) *score1 += plateau[i];
        for (int i = 0; i < 12; i++) plateau[i] = 0;
        return 1;
    }

    return 0; // partie continue
}

// Capture des graines selon la dernière case jouée
void captureGraines(int plateau[], int numero, Joueur *joueur0, Joueur *joueur1, int derniereCase) {
    int increment = (numero == 0) ? -1 : 1;
    int i = derniereCase, captures = 0, nbCaptures = 0;
    int indicesCaptures[12];

    // Identifie les cases capturables (2 ou 3 graines dans le camp adverse)
    while (i >= 0 && i <= 11 &&
           ((numero == 0 && i >= 6) || (numero == 1 && i <= 5)) &&
           (plateau[i] == 2 || plateau[i] == 3)) {
        captures += plateau[i];
        indicesCaptures[nbCaptures++] = i;
        i += increment;
    }

    // Vérifie qu’il reste des graines dans le camp adverse après capture
    int grainesRestantes = 0;
    for (int j = (numero == 0 ? 6 : 0); j <= (numero == 0 ? 11 : 5); j++) {
        int valeur = plateau[j];
        for (int k = 0; k < nbCaptures; k++)
            if (indicesCaptures[k] == j) valeur = 0;
        grainesRestantes += valeur;
    }

    // Applique la capture si elle n’affame pas l’adversaire
    if (grainesRestantes > 0) {
        for (int k = 0; k < nbCaptures; k++)
            plateau[indicesCaptures[k]] = 0;

        if (numero == 0) joueur0->score += captures;
        else joueur1->score += captures;
    }
}

// Joue un coup simple pour le joueur donné
int jouerCoup(int plateau[], int numero, Joueur *joueur0, Joueur *joueur1, int indice) {
    if (numero != 0 && numero != 1) return 1;
    if (indice < 0 || indice > 11) return 2;
    if (numero == 0 && indice > 5) return 3;
    if (numero == 1 && indice <= 5) return 4;
    if (plateau[indice] == 0) return 5;

    int graines = plateau[indice];

    // Si l’adversaire est en famine, le coup doit le nourrir
    int etatFamine = detecterFamine(plateau);
    if (etatFamine != -1) {
        if (numero == 0 && graines <= (5 - indice)) return 6;
        if (numero == 1 && graines <= (11 - indice)) return 6;
    }

    // Distribution des graines (règle des 12)
    plateau[indice] = 0;
    int pos = indice;
    while (graines > 0) {
        pos = (pos + 1) % 12;
        if (pos == indice) continue;
        plateau[pos]++;
        graines--;
    }

    // Capture éventuelle
    captureGraines(plateau, numero, joueur0, joueur1, pos);
    return 0;
}

// Affiche le plateau et les scores
void afficherPlateau(int plateau[], Joueur joueur0, Joueur joueur1) {
    printf("%-15s : ", joueur1.pseudo);
    for (int i = 11; i >= 6; i--)
        printf(" %2d ", plateau[i]);
    printf("| Score : %d\n\n", joueur1.score);

    printf("%-15s : ", joueur0.pseudo);
    for (int i = 0; i <= 5; i++)
        printf(" %2d ", plateau[i]);
    printf("| Score : %d\n\n", joueur0.score);
}