/*************************************************************************
                           Awale -- Game (Server Main)
                             -------------------
    début                : 20/10/2025
    auteurs              : Mohammed Iich et Dame Dieng
    e-mails              : mohammed.iich@insa-lyon.fr et dame.dieng@insa-lyon.fr
    description          : - TCP listening
                           - Accepter un nouveau client
                           - Gérer les commandes des clients
*************************************************************************/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>   
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <time.h>

#include "server.h"

/* =====================================================
 *                  Authentification
 * ===================================================== */

/* Comparaison de mot de passe limitée (évite les warnings de %s). */
static int password_match(const char *a, const char *b)
{
    return strncmp(a, b, 31) == 0;
}

static int is_valid_username(const char *username)
{
    if (!username || !*username) return 0;
    if (strlen(username) > 15) return 0;
    
    for (size_t i = 0; username[i]; i++) {
        char c = username[i];
        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '_' || c == '-'))
            return 0;
    }
    return 1;
}

/* =====================================================
 *                 Supprimer un client
 * ===================================================== */
void server_remove_client(int fd)
{
    games_remove_observer_fd(fd);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].fd == fd) {

            if (g_clients[i].in_game)
                games_cancel_by_client(i, 1);

            close(fd);
            FD_CLR(fd, &g_master_set);

            g_clients[i].fd            = -1;
            g_clients[i].logged_in     = 0;
            g_clients[i].login_stage   = 0;
            g_clients[i].in_game       = 0;
            g_clients[i].ready         = 0;
            g_clients[i].private_mode  = 0;
            g_clients[i].opponent_index = -1;
            g_clients[i].player_index   = -1;
            g_clients[i].name[0]        = '\0';
            g_clients[i].pending_friend_reqs[0] = '\0';

            break;
        }
    }
}

/* =====================================================
 *            Accepter une nouvelle connexion
 * ===================================================== */
void server_handle_new_connection(int server_fd)
{
    struct sockaddr_in cli;
    socklen_t alen = sizeof(cli);

    int newfd = accept(server_fd, (struct sockaddr *)&cli, &alen);
    if (newfd < 0)
        return;

    for (int i = 0; i < MAX_CLIENTS; i++) {

        if (g_clients[i].fd == -1) {

            g_clients[i].fd            = newfd;
            g_clients[i].logged_in     = 0;
            g_clients[i].login_stage   = 0;
            g_clients[i].in_game       = 0;
            g_clients[i].ready         = 0;
            g_clients[i].private_mode  = 0;
            g_clients[i].opponent_index = -1;
            g_clients[i].player_index   = -1;
            g_clients[i].name[0]        = '\0';
            g_clients[i].pending_friend_reqs[0] = '\0';

            FD_SET(newfd, &g_master_set);
            if (newfd > g_max_fd)
                g_max_fd = newfd;

            const char *msg = "Enter your username :\n";
            send(newfd, msg, strlen(msg), 0);
            return;
        }
    }

    send(newfd, "Server full\n", 12, 0);
    close(newfd);
}

/* =====================================================
 *              Gérer le message d'un client
 * ===================================================== */
void server_handle_client_message(int fd)
{
    char buf[BUF_SIZE];
    int n = recv(fd, buf, sizeof(buf) - 1, 0);

    if (n <= 0) {
        server_remove_client(fd);
        return;
    }

    buf[n] = '\0';
    buf[strcspn(buf, "\r\n")] = '\0';

    /* Find client index */
    int i = -1;
    for (int k = 0; k < MAX_CLIENTS; k++) {
        if (g_clients[k].fd == fd) {
            i = k;
            break;
        }
    }
    if (i == -1) return;

    /* =====================================================
     *                          LOGIN
     * ===================================================== */
    if (!g_clients[i].logged_in)
    {
        /* ---------- Étape 0 : USERNAME ---------- */
        if (g_clients[i].login_stage == 0)
        {
            if (buf[0] == '\0') {
                const char *m = "Enter your username :\n";
                send(fd, m, strlen(m), 0);
                return;
            }

            /* Valider le format du username */
            if (!is_valid_username(buf)) {
                const char *msg = 
                    "ERROR : Invalid username (alphanumeric, - and _ only, max 15 chars)\n";
                send(fd, msg, strlen(msg), 0);
                const char *prompt = "Enter your username :\n";
                send(fd, prompt, strlen(prompt), 0);
                return;
            }

            /* Vérifier si le username existe déjà */
            if (username_logged_in(buf)) {
                char msg[128];
                snprintf(msg, sizeof(msg),
                         "ERROR : User %s is already logged in !\n", buf);
                send(fd, msg, strlen(msg), 0);
                const char *prompt = "Enter your username :\n";
                send(fd, prompt, strlen(prompt), 0);
                return;
            }

            /* On mets le username en minuscule */
            char normalized[16];
            to_lowercase(normalized, buf, sizeof(normalized));
            copy_bounded(g_clients[i].name,
                         sizeof(g_clients[i].name),
                         normalized);

            int acc = accounts_find(g_clients[i].name);

            if (acc >= 0) {
                char msg[128];
                snprintf(msg, sizeof(msg),
                         "Nice to meet you again, %s !\nEnter your password :\n",
                         g_clients[i].name);
                send(fd, msg, strlen(msg), 0);
            } else {
                char msg[128];
                snprintf(msg, sizeof(msg),
                         "Welcome, %s !\nPlease set your password :\n",
                         g_clients[i].name);
                send(fd, msg, strlen(msg), 0);
            }

            g_clients[i].login_stage = 1;
            return;
        }

        /* ---------- Étape 1 : Mot de passe ---------- */
        int acc = accounts_find(g_clients[i].name);

        if (acc >= 0)
        {
            if (password_match(g_accounts[acc].password, buf))
            {
                if (username_logged_in(g_clients[i].name)) {
                    const char *msg =
                        "ERROR : Already logged in on another session !\n";
                    send(fd, msg, strlen(msg), 0);

                    g_clients[i].login_stage = 0;
                    g_clients[i].name[0] = '\0';

                    const char *prompt = "Enter your username :\n";
                    send(fd, prompt, strlen(prompt), 0);
                    return;
                }

                g_clients[i].logged_in   = 1;
                g_clients[i].login_stage = 2;

                const char *ok = "Logged in successfully !\n";
                send(fd, ok, strlen(ok), 0);
            }
            else {
                const char *msg =
                    "ERROR : Wrong password\nEnter your username again :\n";
                send(fd, msg, strlen(msg), 0);

                g_clients[i].login_stage = 0;
                g_clients[i].name[0] = '\0';
            }

            return;
        }
        else
        {
            if (g_account_count >= MAX_ACCOUNTS) {
                const char *msg = "ERROR : Account storage full !\n";
                send(fd, msg, strlen(msg), 0);
                g_clients[i].login_stage = 0;
                g_clients[i].name[0] = '\0';
                return;
            }

            /* Création d'un nouveau compte */
            copy_bounded(g_accounts[g_account_count].username,
                         sizeof(g_accounts[g_account_count].username),
                         g_clients[i].name);

            copy_bounded(g_accounts[g_account_count].password,
                         sizeof(g_accounts[g_account_count].password),
                         buf);

            g_accounts[g_account_count].bio[0]     = '\0';
            g_accounts[g_account_count].friends[0] = '\0';

            g_account_count++;
            accounts_save(USERS_FILE);

            g_clients[i].logged_in   = 1;
            g_clients[i].login_stage = 2;

            const char *ok = "New account created and logged in !\n";
            send(fd, ok, strlen(ok), 0);
            return;
        }
    }

    /* =====================================================
     *              Commandes du menu principal
     * ===================================================== */

    /* ---- HELP ---- */
    if (strcasecmp(buf, "HELP") == 0) {
        const char *m =
            "Commands: LIST, GAMES, CHALLENGE, ACCEPT, REFUSE, MOVE, "
            "CANCEL_GAME, OBSERVE, OUT_OBSERVER, SAY, MESSAGE, "
            "BIO, SHOWBIO, MY_FRIENDS, FRIEND, ACCEPT_FRIEND, DECLINE_FRIEND, UNFRIEND, PRIVATE, QUIT.\n";
        send(fd, m, strlen(m), 0);
        return;
    }

    /* ---- LIST ---- */
    if (strcmp(buf, "LIST") == 0) {

        char msg[512];
        msg[0] = '\0';
        append_bounded(msg, sizeof(msg), "ONLINE:");

        for (int j = 0; j < MAX_CLIENTS; j++) {

            if (g_clients[j].fd != -1 &&
                g_clients[j].logged_in &&
                j != i &&
                !g_clients[j].in_game)
            {
                append_bounded(msg, sizeof(msg), " ");
                append_bounded(msg, sizeof(msg), g_clients[j].name);
            }
        }

        if (strcmp(msg, "ONLINE:") == 0)
            append_bounded(msg, sizeof(msg), " (no other players online)");

        append_bounded(msg, sizeof(msg), "\n");
        send(fd, msg, strlen(msg), 0);
        return;
    }

    /* ---- GAMES ---- */
    if (strcmp(buf, "GAMES") == 0)
    {
        char msg[512];
        msg[0] = '\0';
        append_bounded(msg, sizeof(msg), "ONGOING GAMES:\n");
        int count = 0;

        for (int g = 0; g < MAX_GAMES; g++) {
            if (!g_games[g].active)
                continue;

            char line[128];
            snprintf(line, sizeof(line),
                     "  ID %d: %s vs %s\n",
                     g, g_games[g].p0.name, g_games[g].p1.name);

            append_bounded(msg, sizeof(msg), line);
            count++;
        }

        if (!count)
            append_bounded(msg, sizeof(msg), "  (no active games)\n");

        send(fd, msg, strlen(msg), 0);
        return;
    }

    /* ---- OBSERVE <id> ---- */
    if (strncmp(buf, "OBSERVE ", 8) == 0)
    {
        if (g_clients[i].in_game) {
            const char *msg = "ERROR : You cannot observe while in a game !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        int id;
        if (sscanf(buf + 8, "%d", &id) != 1) {
            const char *msg = "ERROR : Usage: OBSERVE <id> !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        if (id < 0 || id >= MAX_GAMES || !g_games[id].active) {
            const char *msg = "ERROR : Invalid game ID !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        Game *g = &g_games[id];

        if (!games_can_observe(g, g_clients[i].name)) {
            const char *msg =
                "ERROR : Game is private. You are not allowed to observe !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        if (!games_add_observer(g, fd)) {
            const char *msg = "ERROR : Too many observers !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        char ok[128];
        snprintf(ok, sizeof(ok),
                 "Now observing game %d: %s vs %s\n",
                 id, g->p0.name, g->p1.name);
        send(fd, ok, strlen(ok), 0);

        games_send_board(g);
        return;
    }

    /* ---- OUT_OBSERVER ---- */
    if (strcmp(buf, "OUT_OBSERVER") == 0)
    {
        games_remove_observer_fd(fd);
        const char *msg = "Left observation mode. Back to menu.\n";
        send(fd, msg, strlen(msg), 0);
        return;
    }

    /* ---- PRIVATE ON|OFF ---- */
    if (strncmp(buf, "PRIVATE ", 8) == 0)
    {
        char mode[8] = {0};
        sscanf(buf + 8, "%7s", mode);

        if (strcasecmp(mode, "ON") == 0) {
            g_clients[i].private_mode = 1;
            const char *msg =
                "Private mode ON: only your friends may observe your games.\n";
            send(fd, msg, strlen(msg), 0);
        } else {
            g_clients[i].private_mode = 0;
            const char *msg =
                "Private mode OFF: everyone may observe your games.\n";
            send(fd, msg, strlen(msg), 0);
        }
        return;
    }

    /* =====================================================
     *                        BIO
     * ===================================================== */
    if (strncmp(buf, "BIO ", 4) == 0)
    {
        int acc = accounts_find(g_clients[i].name);
        if (acc < 0) {
            const char *msg = "ERROR : Account not found !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        const char *src = buf + 4;
        while (*src == ' ' || *src == '\t')
            src++;

        if (*src == '\0') {
            const char *msg = "ERROR : Bio cannot be empty !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        char cleaned[512];
        size_t k = 0;
        int lines = 0;

        while (*src && k + 1 < sizeof(cleaned)) {
            char c = *src++;

            if (c == '\r') continue;
            if (c == '\t') c = ' ';
            if (c == '|')  c = '/';
            if (c == '\\' && *src == 'n') {
                c = '\n';
                src++;
            }
            if (c == '\n') {
                if (++lines >= 10)
                    c = ' ';
            }
            cleaned[k++] = c;
        }
        cleaned[k] = '\0';

        accounts_set_bio(acc, cleaned);
        accounts_save(USERS_FILE);

        const char *ok = "Bio updated\n";
        send(fd, ok, strlen(ok), 0);
        return;
    }

    /* ---- SHOWBIO <user> ---- */
    if (strncmp(buf, "SHOWBIO ", 8) == 0)
    {
        char target[16];
        if (sscanf(buf + 8, "%15s", target) != 1) {
            const char *msg = "ERROR : Usage: SHOWBIO <user> !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        int acc = accounts_find(target);
        if (acc < 0) {
            const char *msg = "ERROR : User not found !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        const char *bio = accounts_get_bio(acc);

        char msg[700];
        snprintf(msg, sizeof(msg),
                 "\n--- BIO of %s ---\n%s\n-----------------\n",
                 g_accounts[acc].username,
                 (bio && bio[0]) ? bio : "(no bio)");
        send(fd, msg, strlen(msg), 0);
        return;
    }

    /* ---- MY_FRIENDS ---- */
    if (strcmp(buf, "MY_FRIENDS") == 0)
    {
        int me = accounts_find(g_clients[i].name);
        if (me < 0) {
            const char *msg = "ERROR : Account not found !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        /* Vérifier si le joueur a une liste d'amis */
        if (!g_accounts[me].friends[0]) {
            const char *msg = "MY_FRIENDS:\n  (no friends yet)\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        char msg[512];
        msg[0] = '\0';
        append_bounded(msg, sizeof(msg), "MY_FRIENDS:");

        char friends_copy[256];
        strlcpy_safe(friends_copy, g_accounts[me].friends, sizeof(friends_copy));

        char *tok = strtok(friends_copy, ",");
        int count = 0;
        while (tok && count < 50) {  /* Safety limit */
            append_bounded(msg, sizeof(msg), " ");
            append_bounded(msg, sizeof(msg), tok);
            tok = strtok(NULL, ",");
            count++;
        }

        append_bounded(msg, sizeof(msg), "\n");
        send(fd, msg, strlen(msg), 0);
        return;
    }

    /* ---- FRIEND <user> ---- */
    if (strncmp(buf, "FRIEND ", 7) == 0)
    {
        char target[16];
        if (sscanf(buf + 7, "%15s", target) != 1) {
            const char *msg = "ERROR : Usage: FRIEND <user> !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        if (ci_equal(target, g_clients[i].name)) {
            const char *msg = "ERROR : You cannot friend yourself !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        int me  = accounts_find(g_clients[i].name);
        int you = accounts_find(target);

        if (me < 0 || you < 0) {
            const char *msg = "ERROR : Unknown user !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        if (accounts_is_friend(me, target)) {
            const char *msg = "Already friends\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        /* Trouver le joueur à qui envoyer la demande */
        int target_idx = client_index_by_name(target);
        if (target_idx < 0) {
            const char *msg = "Friend request sent (user offline)\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        char req_msg[64];
        snprintf(req_msg, sizeof(req_msg), "FRIEND_REQUEST %s\n", g_clients[i].name);
        
        /* On ajoute au demandes en cours */
        size_t cur_len = strlen(g_clients[target_idx].pending_friend_reqs);
        if (cur_len > 0 && cur_len + strlen(g_clients[i].name) + 2 < sizeof(g_clients[target_idx].pending_friend_reqs)) {
            strcat(g_clients[target_idx].pending_friend_reqs, ",");
            strcat(g_clients[target_idx].pending_friend_reqs, g_clients[i].name);
        } else if (cur_len == 0 && strlen(g_clients[i].name) < sizeof(g_clients[target_idx].pending_friend_reqs)) {
            strcpy(g_clients[target_idx].pending_friend_reqs, g_clients[i].name);
        }

        send(g_clients[target_idx].fd, req_msg, strlen(req_msg), 0);

        const char *ok = "Friend request sent\n";
        send(fd, ok, strlen(ok), 0);
        return;
    }

    /* ---- UNFRIEND <user> ---- */
    if (strncmp(buf, "UNFRIEND ", 9) == 0)
    {
        char target[16];
        if (sscanf(buf + 9, "%15s", target) != 1) {
            const char *msg = "ERROR : Usage: UNFRIEND <user> !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        if (ci_equal(target, g_clients[i].name)) {
            const char *msg = "ERROR : You cannot unfriend yourself !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        int me = accounts_find(g_clients[i].name);
        int them = accounts_find(target);

        if (me < 0) {
            const char *msg = "ERROR : Account not found !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }
        
        int removed_me = accounts_remove_friend(me, target);
        
        if (them >= 0) {
            accounts_remove_friend(them, g_clients[i].name);
        }

        accounts_save(USERS_FILE);

        if (removed_me) {
            const char *msg = "Friend removed !\n";
            send(fd, msg, strlen(msg), 0);
        } else {
            const char *msg = "No such friend\n";
            send(fd, msg, strlen(msg), 0);
        }
        return;
    }

    /* ---- ACCEPT_FRIEND <user> ---- */
    if (strncmp(buf, "ACCEPT_FRIEND ", 14) == 0)
    {
        char requester[16];
        if (sscanf(buf + 14, "%15s", requester) != 1) {
            const char *msg = "ERROR : Usage: ACCEPT_FRIEND <user> !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        int me = accounts_find(g_clients[i].name);
        int them = accounts_find(requester);

        if (me < 0 || them < 0) {
            const char *msg = "ERROR : Unknown user !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        int ok1 = 0, full1 = 0;
        accounts_add_friend(me, requester, &ok1, &full1);

        int ok2 = 0, full2 = 0;
        accounts_add_friend(them, g_clients[i].name, &ok2, &full2);

        if (full1 || full2) {
            const char *msg = "ERROR : Friends list full !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        accounts_save(USERS_FILE);

        char reqs[256];
        strcpy(reqs, g_clients[i].pending_friend_reqs);
        g_clients[i].pending_friend_reqs[0] = '\0';

        char *tok = strtok(reqs, ",");
        while (tok) {
            if (strcasecmp(tok, requester) != 0) {
                if (g_clients[i].pending_friend_reqs[0]) {
                    strcat(g_clients[i].pending_friend_reqs, ",");
                }
                strcat(g_clients[i].pending_friend_reqs, tok);
            }
            tok = strtok(NULL, ",");
        }

        const char *msg = "Friend request accepted !\n";
        send(fd, msg, strlen(msg), 0);

        int requester_idx = client_index_by_name(requester);
        if (requester_idx >= 0) {
            char notify[64];
            snprintf(notify, sizeof(notify), "FRIEND_ACCEPTED %s\n", g_clients[i].name);
            send(g_clients[requester_idx].fd, notify, strlen(notify), 0);
        }

        return;
    }

    /* ---- DECLINE_FRIEND <user> ---- */
    if (strncmp(buf, "DECLINE_FRIEND ", 15) == 0)
    {
        char requester[16];
        if (sscanf(buf + 15, "%15s", requester) != 1) {
            const char *msg = "ERROR : Usage: DECLINE_FRIEND <user> !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        char reqs[256];
        strcpy(reqs, g_clients[i].pending_friend_reqs);
        g_clients[i].pending_friend_reqs[0] = '\0';

        char *tok = strtok(reqs, ",");
        while (tok) {
            if (strcasecmp(tok, requester) != 0) {
                if (g_clients[i].pending_friend_reqs[0]) {
                    strcat(g_clients[i].pending_friend_reqs, ",");
                }
                strcat(g_clients[i].pending_friend_reqs, tok);
            }
            tok = strtok(NULL, ",");
        }

        const char *msg = "Friend request declined\n";
        send(fd, msg, strlen(msg), 0);

        int requester_idx = client_index_by_name(requester);
        if (requester_idx >= 0) {
            char notify[64];
            snprintf(notify, sizeof(notify), "FRIEND_DECLINED %s\n", g_clients[i].name);
            send(g_clients[requester_idx].fd, notify, strlen(notify), 0);
        }

        return;
    }

    /* ---- CHALLENGE <user> ---- */
    if (strncmp(buf, "CHALLENGE ", 10) == 0)
    {
        if (g_clients[i].in_game) {
            const char *msg = "ERROR : You cannot challenge while in a game !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        char target[16];
        if (sscanf(buf + 10, "%15s", target) != 1) {
            const char *msg = "ERROR : Usage: CHALLENGE <user> !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        if (ci_equal(target, g_clients[i].name)) {
            const char *msg = "ERROR : You cannot challenge yourself !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        int idx = client_index_by_name(target);
        if (idx < 0) {
            const char *msg = "ERROR : No such user !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        if (g_clients[idx].in_game) {
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "ERROR : %s is already in a game !\n", target);
            send(fd, msg, strlen(msg), 0);
            return;
        }

        if (g_clients[i].in_game) {
            const char *msg = "ERROR : You are already in a game !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        char msg[64];
        snprintf(msg, sizeof(msg),
                 "CHALLENGE_FROM %s\n", g_clients[i].name);
        send(g_clients[idx].fd, msg, strlen(msg), 0);

        const char *ok = "Challenge sent\n";
        send(fd, ok, strlen(ok), 0);
        return;
    }

    /* ---- REFUSE <user> ---- */
    if (strncmp(buf, "REFUSE ", 7) == 0)
    {
        if (g_clients[i].in_game) {
            const char *msg = "ERROR : You cannot refuse while in a game !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        char target[16];
        if (sscanf(buf + 7, "%15s", target) != 1) {
            const char *msg = "ERROR : Usage: REFUSE <user> !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        int idx = client_index_by_name(target);
        if (idx < 0) {
            const char *msg = "ERROR : No such user !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        char msg[64];
        snprintf(msg, sizeof(msg),
                 "REFUSED_BY %s\n", g_clients[i].name);
        send(g_clients[idx].fd, msg, strlen(msg), 0);

        const char *ok = "Challenge refused\n";
        send(fd, ok, strlen(ok), 0);
        return;
    }

    /* ---- ACCEPT <user> ---- */
    if (strncmp(buf, "ACCEPT ", 7) == 0)
    {
        if (g_clients[i].in_game) {
            const char *msg = "ERROR : You cannot accept while in a game !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        char target[16];
        if (sscanf(buf + 7, "%15s", target) != 1) {
            const char *msg = "ERROR : Usage: ACCEPT <user> !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        int idx = client_index_by_name(target);
        if (idx < 0) {
            const char *msg = "ERROR : No such user !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        if (g_clients[i].in_game) {
            const char *msg = "ERROR : You are already in a game !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        if (g_clients[idx].in_game) {
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "ERROR : %s is already in a game !\n", target);
            send(fd, msg, strlen(msg), 0);
            return;
        }

        games_start(i, idx);
        return;
    }

    /* ---- READY ---- */
    if (strcmp(buf, "READY") == 0)
    {
        if (!g_clients[i].in_game) {
            const char *msg = "ERROR : Not in game !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        g_clients[i].ready = 1;
        int opp = g_clients[i].opponent_index;

        if (opp >= 0 && g_clients[opp].ready) {
            int g_idx = games_find_by_player_name(g_clients[i].name);
            if (g_idx >= 0)
                games_send_board(&g_games[g_idx]);
        }
        return;
    }

    /* ---- MOVE <pit> ---- */
    if (strncmp(buf, "MOVE ", 5) == 0)
    {
        int pit;
        if (sscanf(buf + 5, "%d", &pit) != 1) {
            const char *msg = "ERROR : Usage: MOVE <0-11> !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        games_process_move(i, pit);
        return;
    }

    /* ---- CANCEL_GAME ---- */
    if (strcmp(buf, "CANCEL_GAME") == 0)
    {
        if (!g_clients[i].in_game) {
            const char *msg = "ERROR : You are not in a game !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        games_cancel_by_client(i, 1);

        const char *ok = "Game canceled. Back to menu.\n";
        send(fd, ok, strlen(ok), 0);
        return;
    }

    /* ---- MESSAGE <user> <message> ---- */
    if (strncmp(buf, "MESSAGE ", 8) == 0)
    {
        char target[16];
        char body[480];
        memset(body, 0, sizeof(body));

        if (sscanf(buf + 8, "%15s %479[^\n]", target, body) < 2) {
            const char *msg =
                "ERROR : Usage: MESSAGE <user> <message> !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        if (ci_equal(target, g_clients[i].name)) {
            const char *msg = "ERROR : You cannot message yourself !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        body[479] = '\0';

        int idx = client_index_by_name(target);
        if (idx < 0) {
            const char *msg = "ERROR : User not found !\n";
            send(fd, msg, strlen(msg), 0);
            return;
        }

        size_t body_len = strlen(body);
        if (body_len > 440) body_len = 440;

        char pm[BUF_SIZE];
        snprintf(pm, sizeof(pm),
                 "PM from %s: %.*s\n",
                 g_clients[i].name, (int)body_len, body);

        send(g_clients[idx].fd, pm, strlen(pm), 0);

        const char *ok = "Message sent\n";
        send(fd, ok, strlen(ok), 0);
        return;
    }

    if (strncmp(buf, "SAY ", 4) == 0)
    {
        const char *text = buf + 4;
        size_t msg_len = strlen(text);
        if (msg_len > 450) msg_len = 450;

        char msg[BUF_SIZE];
        snprintf(msg, sizeof(msg),
                 "CHAT %s: %.*s\n",
                 g_clients[i].name, (int)msg_len, text);

        server_broadcast(msg, fd);
        return;
    }

    /* ---- QUIT ---- */
    if (strcmp(buf, "QUIT") == 0)
    {
        server_remove_client(fd);
        return;
    }

    /* ---- UNKNOWN ---- */
    {
        const char *msg = "ERROR : Unknown command !\n";
        send(fd, msg, strlen(msg), 0);
    }
}

/* =====================================================
 *                    Boucle principale
 * ===================================================== */
void server_run(int port)
{
    mkdir("users", 0777);
    mkdir("saved_games", 0777);

    g_account_count = accounts_load(USERS_FILE);

    srand((unsigned int)time(NULL));

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    FD_ZERO(&g_master_set);
    FD_SET(server_fd, &g_master_set);
    g_max_fd = server_fd;

    for (int i = 0; i < MAX_CLIENTS; i++)
        g_clients[i].fd = -1;
    for (int g = 0; g < MAX_GAMES; g++)
        g_games[g].active = 0;

    printf("Awale server listening on port %d...\n", port);

    while (1)
    {
        fd_set read_fds = g_master_set;

        if (select(g_max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        for (int i = 0; i <= g_max_fd; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == server_fd)
                    server_handle_new_connection(server_fd);
                else
                    server_handle_client_message(i);
            }
        }
    }

    close(server_fd);
}

int main(int argc, char *argv[])
{
    int port = DEFAULT_PORT;

    if (argc > 1) {
        int parsed_port = atoi(argv[1]);
        if (parsed_port > 0 && parsed_port < 65536) {
            port = parsed_port;
        } else {
            fprintf(stderr, "ERROR : Invalid port number. Must be between 1 and 65535.\n");
            fprintf(stderr, "Usage: %s [port]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    server_run(port);
    return 0;
}