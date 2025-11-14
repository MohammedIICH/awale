/*************************************************************************
                           Awale -- Game (Server Main)
                             -------------------
    début                : 20/10/2025
    auteurs              : Mohammed Iich et Dame Dieng
    e-mails              : mohammed.iich@insa-lyon.fr et dame.dieng@insa-lyon.fr
    description          : Quelques outils utilisés par le serveur
*************************************************************************/

#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <strings.h>
#include "server.h"

/* ================================================================
 *  Copie sécurisée de chaîne de caractères
 * ================================================================ */
void safe_strcpy(char *dst, const char *src, size_t max)
{
    if (!dst || !src || max == 0) return;

    strncpy(dst, src, max - 1);
    dst[max - 1] = '\0';
}

void copy_bounded(char *dst, size_t dstsz, const char *src)
{
    if (!dst || !src || dstsz == 0) return;

    size_t len = strlen(src);
    if (len >= dstsz) len = dstsz - 1;

    memcpy(dst, src, len);
    dst[len] = '\0';
}

void append_bounded(char *dst, size_t dstsz, const char *src)
{
    if (!dst || !src || dstsz == 0) return;

    size_t cur = strlen(dst);
    if (cur >= dstsz - 1) return;

    size_t avail = dstsz - 1 - cur;
    size_t add   = strlen(src);
    if (add > avail) add = avail;

    memcpy(dst + cur, src, add);
    dst[cur + add] = '\0';
}

size_t strlcpy_safe(char *dst, const char *src, size_t dstsz) {
    if (!dst || !src || dstsz == 0) return 0;
    size_t len = strlen(src);
    size_t n = (len >= dstsz) ? dstsz - 1 : len;
    memcpy(dst, src, n);
    dst[n] = '\0';
    return len;
}

/* ================================================================
 *  Broadcast un message à tous les clients
 * ================================================================ */
void server_broadcast(const char *msg, int except_fd)
{
    size_t len = strlen(msg);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].fd != -1 &&
            g_clients[i].logged_in &&
            g_clients[i].fd != except_fd)
        {
            send(g_clients[i].fd, msg, len, 0);
        }
    }
}

/* ================================================================
 *  Rechercher un client par son pseudo
 * ================================================================ */
int client_index_by_name(const char *name)
{
    if (!name || !*name)
        return -1;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].fd != -1 &&
            g_clients[i].logged_in &&
            ci_equal(g_clients[i].name, name))
        {
            return i;
        }
    }
    return -1;
}

/* ================================================================
 *  Vérifie si un pseudo est déjà connecté
 * ================================================================ */
int username_logged_in(const char *username)
{
    if (!username || !*username)
        return 0;

    for (int i = 0; i < MAX_CLIENTS; i++) {

        if (g_clients[i].fd != -1 &&
            g_clients[i].logged_in &&
            ci_equal(g_clients[i].name, username))
        {
            return 1;
        }
    }
    return 0;
}

/* ================================================================
 *  Vérification du mode privé d'une partie
 * ================================================================ */
int games_can_observe(Game *g, const char *observer_name)
{
    if (!g || !observer_name || !*observer_name)
        return 0;

    int ci0 = client_index_by_name(g->p0.name);
    int ci1 = client_index_by_name(g->p1.name);

    int priv0 = (ci0 >= 0) ? g_clients[ci0].private_mode : 0;
    int priv1 = (ci1 >= 0) ? g_clients[ci1].private_mode : 0;

    if (!priv0 && !priv1)
        return 1;

    int a0 = accounts_find(g->p0.name);
    int a1 = accounts_find(g->p1.name);

    if (priv0) {
        if (a0 < 0 || !accounts_is_friend(a0, observer_name))
            return 0;
    }

    if (priv1) {
        if (a1 < 0 || !accounts_is_friend(a1, observer_name))
            return 0;
    }

    return 1;
}

/* ================================================================
 *  Comparaison insensible à la casse de deux chaînes
 * ================================================================ */
int ci_equal(const char *a, const char *b)
{
    return strcasecmp(a, b) == 0;
}

void to_lowercase(char *dst, const char *src, size_t max)
{
    if (!dst || !src || max == 0) return;

    size_t i = 0;
    for (; i < max - 1 && src[i]; i++) {
        char c = src[i];
        if (c >= 'A' && c <= 'Z')
            c = c - 'A' + 'a';
        dst[i] = c;
    }
    dst[i] = '\0';
}

/* ================================================================
 *  Concatenation sécurisée de chaînes de caractères
 * ================================================================ */

size_t strlcat_safe(char *dst, const char *src, size_t dstsz)
{
    if (!dst || !src || dstsz == 0) return 0;
    size_t dlen = strlen(dst);
    size_t slen = strlen(src);

    if (dlen >= dstsz - 1) return dlen + slen;

    size_t space = dstsz - dlen - 1;
    size_t n = (slen > space) ? space : slen;

    memcpy(dst + dlen, src, n);
    dst[dlen + n] = '\0';

    return dlen + slen;
}

void safe_strcat(char *dst, const char *src, size_t dstsz)
{
    if (!dst || !src || dstsz == 0) return;
    size_t dlen = strlen(dst);
    size_t slen = strlen(src);

    if (dlen >= dstsz - 1) return;

    size_t space = dstsz - dlen - 1;
    size_t tocopy = (slen > space) ? space : slen;

    memcpy(dst + dlen, src, tocopy);
    dst[dlen + tocopy] = '\0';
}