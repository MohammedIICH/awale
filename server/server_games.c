/*************************************************************************
                           Awale -- Game (Server Games)
                             -------------------
    début                : 20/10/2025
    auteurs              : Mohammed Iich et Dame Dieng
    e-mails              : mohammed.iich@insa-lyon.fr et dame.dieng@insa-lyon.fr
    description          : Gérer une session du jeu :
                           start, moves, observers, cancellation.
*************************************************************************/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>     
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>

#include "server.h"

/* =====================================================
 *          Envoyer l’état du plateau 
 * ===================================================== */
void games_send_board(Game *g)
{
    char msg[256];
    snprintf(msg, sizeof(msg),
             "BOARD %d %d %d %d %d %d %d %d %d %d %d %d | Scores: %d-%d | Next: %d\n",
             g->board[0], g->board[1], g->board[2], g->board[3],
             g->board[4], g->board[5], g->board[6], g->board[7],
             g->board[8], g->board[9], g->board[10], g->board[11],
             g->p0.score, g->p1.score, g->to_move);

    size_t len = strlen(msg);

    if (g->player_fd0 > 0)
        send(g->player_fd0, msg, len, 0);

    if (g->player_fd1 > 0)
        send(g->player_fd1, msg, len, 0);

    for (int z = 0; z < g->observer_count; z++) {
        int fd = g->observers[z];
        if (fd > 0)
            send(fd, msg, len, 0);
    }
}

/* =====================================================
 *              Chercher jeu par joueur
 * ===================================================== */
int games_find_by_player_name(const char *name)
{
    if (!name || !*name)
        return -1;

    for (int k = 0; k < MAX_GAMES; k++) {

        if (!g_games[k].active)
            continue;

        if (strcasecmp(g_games[k].p0.name, name) == 0 ||
            strcasecmp(g_games[k].p1.name, name) == 0)
        {
            return k;
        }
    }

    return -1;
}

/* =====================================================
 *                Fin du jeu et broadcast
 * ===================================================== */
static void games_end(Game *g)
{
    char endmsg[128];

    snprintf(endmsg, sizeof(endmsg),
             "GAME_END %d %d\n", g->p0.score, g->p1.score);

    size_t len = strlen(endmsg);

    if (g->player_fd0 > 0)
        send(g->player_fd0, endmsg, len, 0);

    if (g->player_fd1 > 0)
        send(g->player_fd1, endmsg, len, 0);

    for (int z = 0; z < g->observer_count; z++) {
        int fd = g->observers[z];
        if (fd > 0)
            send(fd, endmsg, len, 0);
    }

    /* Append to game log */
    FILE *f = fopen(g->filename, "a");
    if (f) {
        fprintf(f, "GAME_END %s: %d   %s: %d\n",
                g->p0.name, g->p0.score,
                g->p1.name, g->p1.score);
        fclose(f);
    }

    g->active = 0;
}

/* =====================================================
 *                    Lancer une partie
 * ===================================================== */
int games_start(int client_a, int client_b)
{
    int g_idx = -1;

    for (int i = 0; i < MAX_GAMES; i++) {
        if (!g_games[i].active) {
            g_idx = i;
            break;
        }
    }
    if (g_idx < 0)
        return -1;

    Game *g = &g_games[g_idx];
    memset(g, 0, sizeof(*g));

    g->active         = 1;
    g->observer_count = 0;

    initGame(g->board);
    resetScores(&g->p0, &g->p1);

    /* Définir un username */
    copy_bounded(g->p0.name, sizeof(g->p0.name), g_clients[client_a].name);
    copy_bounded(g->p1.name, sizeof(g->p1.name), g_clients[client_b].name);

    g->p0.number = 0;
    g->p1.number = 1;

    g->to_move   = rand() % 2;

    g->player_fd0 = g_clients[client_a].fd;
    g->player_fd1 = g_clients[client_b].fd;

    /* Bind des clients*/
    g_clients[client_a].in_game        = 1;
    g_clients[client_b].in_game        = 1;
    g_clients[client_a].opponent_index = client_b;
    g_clients[client_b].opponent_index = client_a;
    g_clients[client_a].player_index   = 0;
    g_clients[client_b].player_index   = 1;
    g_clients[client_a].ready          = 0;
    g_clients[client_b].ready          = 0;

    /* Créer un file log du jeu */
    time_t t = time(NULL);
    char ts[32];
    size_t ts_len = strftime(ts, sizeof(ts), "%Y-%m-%d_%H-%M-%S", localtime(&t));
    
    if (ts_len == 0) {
        snprintf(ts, sizeof(ts), "game_%ld", (long)t);
    }

    char tmp[160];
    snprintf(tmp, sizeof(tmp),
             "saved_games/%s_vs_%s_%s.txt",
             g->p0.name, g->p1.name, ts);
    copy_bounded(g->filename, sizeof(g->filename), tmp);

    FILE *f = fopen(g->filename, "w");
    if (f) {
        fprintf(f, "GAME_START %s vs %s\n", g->p0.name, g->p1.name);
        fclose(f);
    }

    /* Notifier les joueurs */
    char msg[128];
    snprintf(msg, sizeof(msg),
             "GAME_START %s vs %s\n", g->p0.name, g->p1.name);

    send(g->player_fd0, msg, strlen(msg), 0);
    send(g->player_fd1, msg, strlen(msg), 0);

    return g_idx;
}

/* =====================================================
 *                     Jouer un coup
 * ===================================================== */
void games_process_move(int client_index, int pit)
{
    if (!g_clients[client_index].in_game) {
        send(g_clients[client_index].fd, "ERROR : Not in game\n", 21, 0);
        return;
    }

    int g_idx = games_find_by_player_name(g_clients[client_index].name);

    if (g_idx < 0) {
        send(g_clients[client_index].fd, "ERROR : Internal game not found\n", 30, 0);
        return;
    }

    Game *g = &g_games[g_idx];

    /* Non respect du tour */
    if (g_clients[client_index].player_index != g->to_move) {
        send(g_clients[client_index].fd, "ERROR : Not your turn\n", 23, 0);
        return;
    }

    /* Jouer un coup */
    int rc = playMove(g->board,
                      g_clients[client_index].player_index,
                      &g->p0, &g->p1,
                      pit);

    if (rc != 0) {
        send(g_clients[client_index].fd, "ERROR : Illegal move\n", 22, 0);
        return;
    }

    /* Commande MOVE */
    FILE *f = fopen(g->filename, "a");
    if (f) {
        fprintf(f, "%s MOVE %d\n",
                g_clients[client_index].name, pit);
        fclose(f);
    }

    g->to_move ^= 1;

    games_send_board(g);

    if (isGameOver(g->board, &g->p0.score, &g->p1.score))
        games_end(g);
}

/* =====================================================
 *                  Mode observateur
 * ===================================================== */
int games_add_observer(Game *g, int fd)
{
    if (g->observer_count >= MAX_CLIENTS)
        return 0;

    g->observers[g->observer_count++] = fd;
    return 1;
}

void games_remove_observer_fd(int fd)
{
    for (int gi = 0; gi < MAX_GAMES; gi++) {

        if (!g_games[gi].active)
            continue;

        Game *g = &g_games[gi];

        for (int z = 0; z < g->observer_count; z++) {

            if (g->observers[z] == fd) {
                if (z < g->observer_count - 1) {
                    g->observers[z] = g->observers[g->observer_count - 1];
                }
                g->observer_count--;
                return;
            }
        }
    }
}

/* =====================================================
 *                   Annuler une partie
 * ===================================================== */
void games_cancel_by_client(int client_index, int notify)
{
    if (!g_clients[client_index].in_game)
        return;

    int g_idx = games_find_by_player_name(g_clients[client_index].name);

    if (g_idx < 0)
        return;

    Game *g = &g_games[g_idx];

    int opp = g_clients[client_index].opponent_index;

    /* On notifie l'adversaire */
    if (notify && opp >= 0 && g_clients[opp].fd > 0)
    {
        char msg[128];
        
        snprintf(msg, sizeof(msg),
                 "GAME_CANCELED %s\n",
                 g_clients[client_index].name);

        if (send(g_clients[opp].fd, msg, strlen(msg), 0) > 0)
        {
            g_clients[opp].in_game        = 0;
            g_clients[opp].ready          = 0;
            g_clients[opp].opponent_index = -1;
        }
    }

    /* Notifier tous les observateur que le jeu est fini */
    char obs_msg[64];
    snprintf(obs_msg, sizeof(obs_msg),
             "GAME_CANCELED %s\n",
             g_clients[client_index].name);

    for (int z = 0; z < g->observer_count; z++) {
        int fd = g->observers[z];
        if (fd > 0)
            send(fd, obs_msg, strlen(obs_msg), 0);
    }

    g_clients[client_index].in_game        = 0;
    g_clients[client_index].ready          = 0;
    g_clients[client_index].opponent_index = -1;

    FILE *f = fopen(g->filename, "a");
    if (f) {
        fprintf(f, "GAME_CANCELED by %s\n", g_clients[client_index].name);
        fclose(f);
    }

    g->active = 0;
}