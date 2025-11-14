/*************************************************************************
                           Awale -- Client (Utils)
                             -------------------
    début                : 14/11/2025
    description          : Fonctions utilitaires sûres pour le client :
                           - copie de chaînes sans buffer overflow
                           - initialisation sûre de buffers
                           - validation des données
*************************************************************************/

#include <string.h>
#include <strings.h>   
#include "client.h"

/* ================================================================
 *  Copie sécurisée des structures Client
 * ================================================================ */
void safe_strcpy_client(char *dst, const char *src, size_t dstsz)
{
    if (!dst || !src || dstsz == 0) return;

    strncpy(dst, src, dstsz - 1);
    dst[dstsz - 1] = '\0';
}

/* ================================================================
 *  Initialisation de chaînes
 * ================================================================ */
void safe_strinit(char *str, size_t sz)
{
    if (!str || sz == 0) return;
    str[0] = '\0';
}

/* ================================================================
 *  Comparaison de chaînes
 * ================================================================ */
int safe_strcmp(const char *a, const char *b)
{
    if (!a) a = "";
    if (!b) b = "";
    return strcmp(a, b);
}

/* ================================================================
 *  Comparaison insensible à la casse
 * ================================================================ */
int safe_strcasecmp(const char *a, const char *b)
{
    if (!a) a = "";
    if (!b) b = "";
    return strcasecmp(a, b);
}

/* ================================================================
 *  Vérifier si une chaîne est vide ou NULL
 * ================================================================ */
int is_string_empty(const char *str)
{
    return !str || str[0] == '\0';
}

/* ================================================================
 *  Vérifier si un buffer est terminé par un caractère nul
 * ================================================================ */
int is_buffer_terminated(const char *buf, size_t size)
{
    if (!buf || size == 0) return 0;
    
    for (size_t i = 0; i < size; i++) {
        if (buf[i] == '\0') return 1;
    }
    return 0;
}

/* ================================================================
 *  Valider l'index du plateau (0 à 11)
 * ================================================================ */
int is_valid_board_index(int index)
{
    return index >= 0 && index < 12;
}

/* ================================================================
 *  Valider l'index du joueur (0 ou 1)
 * ================================================================ */
int is_valid_player_index(int index)
{
    return index == 0 || index == 1;
}

/* ================================================================
 *  Valider score (non négatif)
 * ================================================================ */
int is_valid_score(int score)
{
    return score >= 0;
}
