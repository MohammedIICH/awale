/*************************************************************************
                           Awale -- Game (Server Accounts)
                             -------------------
    début                : 20/10/2025
    auteurs              : Mohammed Iich et Dame Dieng
    e-mails              : mohammed.iich@insa-lyon.fr et dame.dieng@insa-lyon.fr
    description          : Gestion des comptes utilisateurs :
                           chargement, sauvegarde, amis, bio.
*************************************************************************/

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include "server.h"

/* ================================================================
 *  Variables globales
 * ================================================================ */

Client  g_clients[MAX_CLIENTS];
Game    g_games[MAX_GAMES];
Account g_accounts[MAX_ACCOUNTS];
int     g_account_count = 0;

fd_set  g_master_set;
int     g_max_fd = -1;

/* ================================================================
 *  Encodage/Décodage bio
 * ================================================================ */

static void bio_encode(const char *in, char *out, size_t outsz) {
    size_t k = 0;
    for (size_t i = 0; in[i] && k + 1 < outsz; i++) {
        char c = in[i];
        if (c == '\n') c = '|';
        if (c == ':')  c = ' ';
        out[k++] = c;
    }
    out[k] = '\0';
}

static void bio_decode(const char *in, char *out, size_t outsz) {
    size_t k = 0;
    for (size_t i = 0; in[i] && k + 1 < outsz; i++) {
        out[k++] = (in[i] == '|') ? '\n' : in[i];
    }
    out[k] = '\0';
}

/* ================================================================
 *  Charger les comptes
 * ================================================================ */

int accounts_load(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[1024];
    int n = 0;

    while (n < MAX_ACCOUNTS && fgets(line, sizeof(line), f)) {

        line[strcspn(line, "\r\n")] = 0;
        if (!*line) continue;

        char *p1 = strchr(line, ':');
        if (!p1) continue;
        *p1++ = '\0';

        char *p2 = strchr(p1, ':');
        if (!p2) continue;
        *p2++ = '\0';

        char *p3 = strchr(p2, ':');

        char *username = line;
        char *pw = p1;
        char *bio_enc;
        char *friends;

        if (p3) {
            *p3++ = '\0';
            bio_enc = p2;
            friends = p3;
        } else {
            bio_enc = p2;
            friends = (char *)"";
        }

        strlcpy_safe(g_accounts[n].username, username,
                     sizeof(g_accounts[n].username));

        strlcpy_safe(g_accounts[n].password, pw,
                     sizeof(g_accounts[n].password));

        bio_decode(bio_enc, g_accounts[n].bio,
                   sizeof(g_accounts[n].bio));

        strlcpy_safe(g_accounts[n].friends, friends,
                     sizeof(g_accounts[n].friends));

        n++;
    }

    fclose(f);
    g_account_count = n;
    return n;
}

/* ================================================================
 *  Persister les comptes
 * ================================================================ */

void accounts_save(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return;

    for (int i = 0; i < g_account_count; i++) {

        char encoded[512];
        bio_encode(g_accounts[i].bio, encoded, sizeof(encoded));

        fprintf(f, "%s:%s:%s:%s\n",
                g_accounts[i].username,
                g_accounts[i].password,
                encoded,
                g_accounts[i].friends);
    }

    fclose(f);
}

/* ================================================================
 *  Recherche compte par nom d'utilisateur
 * ================================================================ */

int accounts_find(const char *username) {
    for (int i = 0; i < g_account_count; i++) {
        if (ci_equal(g_accounts[i].username, username))
            return i;
    }
    return -1;
}

/* ================================================================
 *  Amities
 * ================================================================ */

int accounts_is_friend(int a, const char *username)
{
    if (a < 0 || a >= g_account_count) return 0;
    if (!username || !*username) return 0;
    if (!g_accounts[a].friends[0]) return 0;

    char tmp[256];
    strlcpy_safe(tmp, g_accounts[a].friends, sizeof(tmp));

    char *tok = strtok(tmp, ",");
    while (tok) {
        if (ci_equal(tok, username))
            return 1;
        tok = strtok(NULL, ",");
    }
    return 0;
}

void accounts_set_bio(int a, const char *bio)
{
    if (a < 0 || a >= g_account_count) return;
    strlcpy_safe(g_accounts[a].bio, bio, sizeof(g_accounts[a].bio));
}

const char *accounts_get_bio(int a)
{
    if (a < 0 || a >= g_account_count) return "";
    return g_accounts[a].bio;
}

void accounts_add_friend(int a, const char *friend_name,
                         int *ok, int *full)
{
    if (ok) *ok = 0;
    if (full) *full = 0;

    if (a < 0 || a >= g_account_count) return;
    if (!friend_name || !*friend_name) return;

    if (accounts_is_friend(a, friend_name)) return;

    size_t cur = strlen(g_accounts[a].friends);
    size_t needed = strlen(friend_name) + (cur > 0 ? 1 : 0);

    if (cur + needed + 1 >= sizeof(g_accounts[a].friends)) {
        if (full) *full = 1;
        return;
    }

    if (cur > 0)
        strlcat_safe(g_accounts[a].friends, ",",
                     sizeof(g_accounts[a].friends));

    strlcat_safe(g_accounts[a].friends, friend_name,
                 sizeof(g_accounts[a].friends));

    if (ok) *ok = 1;
}

int accounts_remove_friend(int a, const char *friend_name)
{
    if (a < 0 || a >= g_account_count) return 0;

    char orig[256];
    strlcpy_safe(orig, g_accounts[a].friends, sizeof(orig));

    char tmp[256] = "";
    int removed = 0;
    int first = 1;

    char *tok = strtok(orig, ",");
    while (tok) {
        if (ci_equal(tok, friend_name)) {
            removed = 1;
        } else {
            if (!first) strlcat_safe(tmp, ",", sizeof(tmp));
            strlcat_safe(tmp, tok, sizeof(tmp));
            first = 0;
        }
        tok = strtok(NULL, ",");
    }

    strlcpy_safe(g_accounts[a].friends, tmp,
                 sizeof(g_accounts[a].friends));

    return removed;
}